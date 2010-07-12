// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.

#include "gen-cpp/BucketStoreMapping.h"
#include <protocol/TBinaryProtocol.h>
#include <server/TSimpleServer.h>
#include <transport/TServerSocket.h>
#include <transport/TBufferTransports.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace boost;
using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;

using namespace  ::scribe::thrift;

class BucketStoreMappingHandler : virtual public BucketStoreMappingIf {
 public:
  BucketStoreMappingHandler(const string& file) : file_(file) {
  }

  void getMapping(std::map<int32_t, HostPort> & _return, const std::string& category) {
    // get current time
    char timeBuf[1024];
    time_t now = time(NULL);
    ctime_r(&now, timeBuf);
    // chop the last "\n";
    timeBuf[strlen(timeBuf) - 1] = 0;

    // open the file "file", it should be in the form of
    // i1,host,port
    // i2
    // where n is number of bucket and i1, i2 are bucket number
    ifstream ifs(file_.c_str());
    if (ifs.good()) {
      string line;
      // read in one line
      while (!ifs.eof()) {
        getline(ifs, line);
        if (line.empty()) {
          continue;
        }
        vector<string> parts;
        boost::split(parts, line, boost::is_any_of(","));
        if (parts.size() < 3) {
          cerr << "ignorig line: " << line << endl;
          continue;
        }
        int32_t bid = atoi(parts[0].c_str());
        HostPort hp;
        hp.host = parts[1];
        hp.port = atoi(parts[2].c_str());
        _return[bid] = hp;
        cout << "[" << timeBuf << "] bucket " << bid << " => "
             << hp.host << ":" << hp.port << endl;
      }
    } else {
      cerr << "Can't read configure file: " << file_ << endl;
    }
  }

 protected:
  string file_;
};

void help() {
  cout << "Usage: -f bucket_map_file [-p port]" << endl;
}

int main(int argc, char **argv) {
  int port = 9090;
  string file;

  int c;
  while ((c = getopt(argc, argv, "p:f:")) != -1) {
    switch (c) {
    case 'p':
      port = atoi(optarg);
      break;
    case 'f':
      file = optarg;
      break;
    default:
      abort();
    }
  }
  shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());
  shared_ptr<BucketStoreMappingHandler> handler(new BucketStoreMappingHandler(file));
  shared_ptr<BucketStoreMappingProcessor> processor(new BucketStoreMappingProcessor(handler));
  shared_ptr<TServerSocket> serverSocket(new TServerSocket(port));
  //shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
  shared_ptr<TTransportFactory> transportFactory(new TFramedTransportFactory());

  TSimpleServer server(processor, serverSocket, transportFactory, protocolFactory);
  server.serve();
  return 0;
}

