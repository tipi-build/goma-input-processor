// Copyright 2012 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "file_stat.h"

#include <sys/stat.h>
#include <sstream>

#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#else
#include "filetime_win.h"
#endif

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "counterz.h"
#include "glog/logging.h"
#include "scoped_fd.h"

namespace devtools_goma {

const off_t FileStat::kInvalidFileSize = -1;

FileStat::FileStat(const std::string& filename)
    : size(kInvalidFileSize), is_directory(false), taken_at(absl::Now()) {
  GOMA_COUNTERZ("FileStat");
#ifndef _WIN32
  struct stat stat_buf;
  if (stat(filename.c_str(), &stat_buf) == 0) {
    InitFromStat(stat_buf);
  }
#else
  WIN32_FILE_ATTRIBUTE_DATA fileinfo;
  if (GetFileAttributesExA(filename.c_str(), GetFileExInfoStandard,
                           &fileinfo) == 0) {
    DWORD err = GetLastError();
    if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
      return;
    }
    LOG_SYSRESULT(err);
    LOG(ERROR) << "Failed to get file attribute of " << filename;
    return;
  }
  if ((fileinfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
    // fast path.  not symlink.
    if (fileinfo.nFileSizeHigh != 0) {
      LOG(ERROR) << "Goma won't handle a file whose size is larger than "
                 << "4 GB: " << filename;
      return;
    }
    if (fileinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      is_directory = true;
    }
    size = static_cast<off_t>(fileinfo.nFileSizeLow);
    mtime = ConvertFiletimeToAbslTime(fileinfo.ftLastWriteTime);
    return;
  }
  // file is a symbolic link.
  // don't use GetFileAttributesExA here.
  // GetFileAttributesExA acts like lstat, not stat.
  ScopedFd h(CreateFileA(filename.c_str(), 0,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                         OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
  if (!h.valid()) {
    LOG_SYSRESULT(GetLastError());
    LOG(ERROR) << "Failed to open " << filename << " for file stat";
    return;
  }
  LARGE_INTEGER size_data;
  if (GetFileSizeEx(h.handle(), &size_data) == 0) {
    LOG_SYSRESULT(GetLastError());
    LOG(ERROR) << "Failed to get file size of " << filename;
    return;
  }
  if (size_data.u.HighPart != 0) {
    LOG(ERROR) << "Goma won't handle a file whose size is larger than 4 GB: "
               << filename;
    return;
  }
  FILE_BASIC_INFO finfo;
  if (GetFileInformationByHandleEx(h.handle(), FileBasicInfo, &finfo,
                                   sizeof finfo) == 0) {
    LOG_SYSRESULT(GetLastError());
    LOG(ERROR) << "Failed to get file information of " << filename;
    return;
  }
  if (finfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    is_directory = true;
  }
  size = static_cast<off_t>(size_data.u.LowPart);
  FILETIME last_write_time;
  last_write_time.dwLowDateTime = finfo.LastWriteTime.u.LowPart;
  last_write_time.dwHighDateTime = finfo.LastWriteTime.u.HighPart;
  mtime = ConvertFiletimeToAbslTime(last_write_time);
#endif
}

#ifndef _WIN32
void FileStat::InitFromStat(const struct stat& stat_buf) {
#ifdef __MACH__
  mtime = absl::TimeFromTimespec(stat_buf.st_mtimespec);
#else
  mtime = absl::TimeFromTimespec(stat_buf.st_mtim);
#endif

  size = stat_buf.st_size;
  is_directory = S_ISDIR(stat_buf.st_mode);
}
#endif

bool FileStat::IsValid() const {
  return size != kInvalidFileSize && mtime.has_value();
}

bool FileStat::CanBeNewerThan(const FileStat& old) const {
  return old.CanBeStale() || *this != old;
}

std::string FileStat::DebugString() const {
  std::stringstream ss;
  ss << "{";
  ss << " mtime=";
  if (mtime) {
    ss << *mtime;
  } else {
    ss << "not set";
  }
  ss << " size=" << size;
  ss << " is_directory=" << is_directory;
  ss << "}";
  return ss.str();
}

bool FileStat::CanBeStale() const {
  DCHECK(mtime.has_value());

  // If mtime + 1 >= taken_at, the file might be updated within
  // the same second. We need to re-check the file for this case, too.
  // The plus one is for VMs, where mtime can delay 1 second or Apple's HFS.
  // TODO: make time resolution configurable.
  return *mtime + absl::Seconds(1) >= taken_at;
}

std::ostream& operator<<(std::ostream& os, const FileStat& stat) {
  return os << stat.DebugString();
}

}  // namespace devtools_goma
