//  Copyright (c) 2007-2008 Facebook
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
// See accompanying file LICENSE or visit the Scribe site at:
// http://developers.facebook.com/scribe/
//
// @author Bobby Johnson
// @author Jason Sobel
// @author Avinash Lakshman

#include "Common.h"
#include "StdFile.h"

#define UINT_SIZE 4

static const unsigned kInitialBufferSize = (64 * 1024);
static const unsigned kLargeBufferSize   = (16 * kInitialBufferSize);

using namespace boost;

namespace scribe {

StdFile::StdFile(const string& name, bool frame)
  : FileInterface(name, frame), inputBuffer_(NULL), bufferSize_(0) {
}

StdFile::~StdFile() {
  if (inputBuffer_) {
    delete[] inputBuffer_;
    inputBuffer_ = NULL;
  }
}

bool StdFile::exists() {
  return filesystem::exists(filename_);
}

bool StdFile::openRead() {
  return open(fstream::in);
}

bool StdFile::openWrite() {
  // open file for write in append mode
  ios_base::openmode mode = fstream::out | fstream::app;
  return open(mode);
}

bool StdFile::openTruncate() {
  // open an existing file for write and truncate its contents
  ios_base::openmode mode = fstream::out | fstream::app | fstream::trunc;
  return open(mode);
}

bool StdFile::open(ios_base::openmode mode) {

  if (file_.is_open()) {
    return false;
  }

  file_.open(filename_.c_str(), mode);

  return file_.good();
}

bool StdFile::isOpen() {
  return file_.is_open();
}

void StdFile::close() {
  if (file_.is_open()) {
    file_.close();
  }
}

string StdFile::getFrame(unsigned dataLength) {

  if (framed_) {
    char buf[UINT_SIZE];
    serializeUInt(dataLength, buf);
    return string(buf, UINT_SIZE);

  } else {
    return string();
  }
}

bool StdFile::write(const string& data) {

  if (!file_.is_open()) {
    return false;
  }

  file_ << data;
  if (file_.fail()) {
    LOG_OPER("Failed to write to file %s state=0x%x",
             filename_.c_str(), file_.rdstate());
    return false;
  }
  return true;
}

void StdFile::flush() {
  if (file_.is_open()) {
    file_.flush();
  }
}

/*
 * read the next frame in the file that is currently open. returns the
 * body of the frame in _return.
 *
 * returns a negative number if it
 * encounters any problem when reading from the file. The negative
 * number is the number of bytes in the file that will not be read
 * becuase of this problem (most likely corruption of file).
 *
 * returns 0 on end of file or when it encounters a frame of size 0
 *
 * On success it returns the number of bytes in the frame's body
 *
 * This function assumes that the file it is reading is framed.
 */
long
StdFile::readNext(string* item) {
  long size;

#define CALC_LOSS() do {                    \
  int offset = file_.tellg();                \
  if (offset != -1) {                       \
    size = -(fileSize() - offset);          \
  } else {                                  \
    size = -fileSize();                     \
  }                                         \
  if (size > 0) {                           \
    /* loss size can't be positive          \
     * choose a arbitrary but reasonable
     * value for loss
     */                                     \
    size = -(1000 * 1000 * 1000);           \
  }                                         \
  /* loss size can be 0 */                  \
}  while (0)

  if (!inputBuffer_) {
    bufferSize_ = kInitialBufferSize;
    inputBuffer_ = (char *) malloc(bufferSize_);
    if (inputBuffer_ == NULL) {
      CALC_LOSS();
      LOG_OPER("WARNING: nomem Data Loss loss %ld bytes in %s", size,
          filename_.c_str());
     return (size);
    }
  }

  file_.read(inputBuffer_, UINT_SIZE);
  if (!file_.good() || (size = unserializeUInt(inputBuffer_)) == 0) {
    /* end of file */
    return (0);
  }
  // check if most signiifcant bit set - should never be set
  if (size >= INT_MAX) {
    /* Definitely corrupted. Stop reading any further */
    CALC_LOSS();
    LOG_OPER("WARNING: Corruption Data Loss %ld bytes in %s", size,
        filename_.c_str());
    return (size);
  }

  if (size > bufferSize_) {
    bufferSize_ = ((size + kInitialBufferSize - 1) / kInitialBufferSize) *
        kInitialBufferSize;
    free(inputBuffer_);
    inputBuffer_ = (char *) malloc(bufferSize_);
    if (bufferSize_ > kLargeBufferSize) {
      LOG_OPER("WARNING: allocating large buffer Corruption? %d", bufferSize_);
    }
  }
  if (inputBuffer_ == NULL) {
    CALC_LOSS();
    LOG_OPER("WARNING: nomem Corruption? Data Loss %ld bytes in %s", size,
        filename_.c_str());
    return (size);
  }
  file_.read(inputBuffer_, size);
  if (file_.good()) {
    item->assign(inputBuffer_, size);
  } else {
    CALC_LOSS();
    LOG_OPER("WARNING: Data Loss %ld bytes in %s", size, filename_.c_str());
  }
  if (bufferSize_ > kLargeBufferSize) {
    free(inputBuffer_);
    inputBuffer_ = NULL;
  }
  return (size);
#undef CALC_LOSS
}

unsigned long StdFile::fileSize() {
  unsigned long size = 0;
  try {
    size = filesystem::file_size(filename_.c_str());
  } catch(const exception& e) {
    LOG_OPER("Failed to get size for file <%s> error <%s>",
             filename_.c_str(), e.what());
    size = 0;
  }
  return size;
}

void StdFile::listImpl(const string& path, vector<string>* files) {
  try {
    if (filesystem::exists(path)) {
      filesystem::directory_iterator dirIt(path), endIt;

      for ( ; dirIt != endIt; ++ dirIt) {
        files->push_back(dirIt->filename());
      }
    }
  } catch (const exception& e) {
    LOG_OPER("exception <%s> listing files in <%s>",
             e.what(), path.c_str());
  }
}

void StdFile::deleteFile() {
  filesystem::remove(filename_);
}

bool StdFile::createDirectory(const string& path) {
  try {
    filesystem::create_directories(path);
  } catch(const exception& e) {
    LOG_OPER("Exception < %s > in StdFile::createDirectory for path %s ",
      e.what(),path.c_str());
    return false;
  }

  return true;
}

bool StdFile::createSymlink(const string& oldPath, const string& newPath) {
  if (symlink(oldPath.c_str(), newPath.c_str()) == 0) {
    return true;
  }

  return false;
}

} //! namespace scribe
