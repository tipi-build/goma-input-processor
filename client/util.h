// Copyright 2010 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVTOOLS_GOMA_CLIENT_UTIL_H_
#define DEVTOOLS_GOMA_CLIENT_UTIL_H_

#ifndef _WIN32
# include <unistd.h>
#else
# include <direct.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include "config_win.h"
#endif

#include "absl/strings/ascii.h"
#include "absl/types/optional.h"
#include "google/protobuf/repeated_field.h"
#include "google/protobuf/stubs/port.h"

namespace devtools_goma {

// Options to be used with ReadCommandOutput to specify which command output
// will be returned.
enum CommandOutputOption {
  MERGE_STDOUT_STDERR,
  STDOUT_ONLY,
};

typedef std::string (*ReadCommandOutputFunc)(
    const std::string& prog,
    const std::vector<std::string>& argv,
    const std::vector<std::string>& env,
    const std::string& cwd,
    CommandOutputOption option,
    int32_t* status);

// Installs new ReadCommandOutput function.
// ReadCommandOutput function should be installed before calling it.
void InstallReadCommandOutputFunc(ReadCommandOutputFunc func);

// Calls current ReadCommandOutput function.
// If exit status of the command is not zero and |status| == NULL,
// then fatal error.
// Note: You MUST call InstallReadCommandOuptutFunc beforehand.
std::string ReadCommandOutput(const std::string& prog,
                              const std::vector<std::string>& argv,
                              const std::vector<std::string>& env,
                              const std::string& cwd,
                              CommandOutputOption option,
                              int32_t* status);

// Platform independent getenv.
// Note: in chromium/win, gomacc can only get environments that was
// extracted by build/toolchain/win/setup_toolchain.py.
absl::optional<std::string> GetEnv(const std::string& name);

// Platform independent setenv.
void SetEnv(const std::string& name, const std::string& value);

// Gets iterator to the environment variable entry.
template <typename Iter>
Iter GetEnvIterFromEnvIter(Iter env_begin,
                           Iter env_end,
                           const std::string& name,
                           bool ignore_case) {
  std::string key = name + "=";
  if (ignore_case)
    absl::AsciiStrToLower(&key);

  for (Iter i = env_begin; i != env_end; ++i) {
    std::string token = i->substr(0, key.length());
    if (ignore_case)
      absl::AsciiStrToLower(&token);
    if (token == key) {
      return i;
    }
  }
  return env_end;
}

// Gets an environment variable between |envs_begin| and |envs_end|.
// Do not care |name| case if |ignore_case| is true.
template <typename Iter>
std::string GetEnvFromEnvIter(Iter env_begin,
                              Iter env_end,
                              const std::string& name,
                              bool ignore_case) {
  Iter found = GetEnvIterFromEnvIter(env_begin, env_end, name, ignore_case);
  if (found == env_end)
    return "";
  return found->substr(name.length() + 1);  // Also cuts off "=".
}

// Gets an environment variable between |envs_begin| and |envs_end|.
// It automatically ignores case according to the platform.
template <typename Iter>
std::string GetEnvFromEnvIter(Iter env_begin,
                              Iter env_end,
                              const std::string& name) {
#ifdef _WIN32
  return GetEnvFromEnvIter(env_begin, env_end, name, true);
#else
  return GetEnvFromEnvIter(env_begin, env_end, name, false);
#endif
}

// Replace an environment variable |name| value to |to_replace|
// between |envs_begin| and |envs_end|.
// It automatically ignores case according to the platform.
template <typename Iter>
bool ReplaceEnvInEnvIter(Iter env_begin,
                         Iter env_end,
                         const std::string& name,
                         const std::string& to_replace) {
#ifdef _WIN32
  Iter found = GetEnvIterFromEnvIter(env_begin, env_end, name, true);
#else
  Iter found = GetEnvIterFromEnvIter(env_begin, env_end, name, false);
#endif
  if (found != env_end) {
    found->replace(name.size() + 1, found->size() - (name.size() + 1),
                   to_replace);
    return true;
  }
  return false;
}

// Platform independent getpid function.
pid_t Getpid();

// Wrapper for chdir(). VS2015 warns using chdir().
// Returns true if succeeded.
inline bool Chdir(const char* path) {
#ifndef _WIN32
  return chdir(path) == 0;
#else
  return _chdir(path) == 0;
#endif
}

// Returns sum of all integers in |input|, which is a repeated field of int32s.
int64_t SumRepeatedInt32(
    const google::protobuf::RepeatedField<google::protobuf::int32>& input);

}  // namespace devtools_goma

// Convert absl::StrSplit result to std::vector<string>
// Because of clang-cl.exe bug, we cannot write
// std::vector<string> vs = absl::StrSplit(...);
// This function provides an wrapper to avoid this bug.
// After clang-cl.exe or absl has been fixed, this function be removed.
// See b/73514249 for more detail.
template <typename SplitResult>
std::vector<std::string> ToVector(SplitResult split_result) {
  std::vector<std::string> result;
  for (auto&& s : split_result) {
    result.push_back(std::string(s));
  }
  return result;
}

// Used for printing absl::optional<T>. T must be compatible with the <<
// operator to pass it to a stringstream.
template <typename T>
std::string OptionalToString(const absl::optional<T>& opt_value) {
  std::stringstream ss;
  if (opt_value.has_value()) {
    ss << *opt_value;
  } else {
    ss << "(none)";
  }
  return ss.str();
}

#endif  // DEVTOOLS_GOMA_CLIENT_UTIL_H_
