// Copyright 2015 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "goma_init.h"

#include <iostream>

#include "autolock_timer.h"
#include "compiler_proxy_info.h"
#include "env_flags.h"
#include "glog/logging.h"
#include "mypath.h"
#include "ioutil.h"

GOMA_DECLARE_string(SERVER_HOST);
GOMA_DECLARE_int32(SERVER_PORT);


namespace devtools_goma {

void Init(int argc, char* argv[], const char* envp[]) {
  CheckFlagNames(envp);
  AutoConfigureFlags(envp);

  // Display version string and exit if --version is specified.
  if (argc == 2 && strcmp(argv[1], "--version") == 0) {
    std::cout << "GOMA version " << kBuiltRevisionString << std::endl;
    exit(0);
  }
  if (argc == 2 && strcmp(argv[1], "--build-info") == 0) {
    std::cout << kUserAgentString << std::endl;
    exit(0);
  }
  if (argc == 2 && strcmp(argv[1], "--print-server-host") == 0) {
    std::cout << FLAGS_SERVER_HOST << ':' << FLAGS_SERVER_PORT << std::endl;
    exit(0);
  }
#ifndef NO_AUTOLOCK_STAT
  if (FLAGS_ENABLE_CONTENTIONZ)
    g_auto_lock_stats = new AutoLockStats;
#endif

  const std::string username = GetUsernameNoEnv();
  if (username != GetUsernameEnv()) {
    LOG(ERROR) << "username mismatch: " << username
               << " env:" << GetUsernameEnv();
  }

  FLAGS_TMP_DIR = GetGomaTmpDir();
  CheckTempDirectory(FLAGS_TMP_DIR);
}

void InitLogging(const char* argv0) {
  google::InitGoogleLogging(argv0);
#ifndef _WIN32
  google::InstallFailureSignalHandler();
#endif
  LOG(INFO) << "public goma client";

  LOG(INFO) << "goma built date " << kBuiltTimeString;
  LOG(INFO) << "goma built revision " << kBuiltRevisionString;
  LOG(INFO) << "goma built gn args " << kBuiltGNArgsString;
#ifndef NDEBUG
  LOG(ERROR) << "WARNING: DEBUG BINARY -- Performance may suffer";
#endif
#ifdef ADDRESS_SANITIZER
  LOG(ERROR) << "WARNING: ASAN BINARY -- Performance may suffer";
#endif
#ifdef THREAD_SANITIZER
  LOG(ERROR) << "WARNING: TSAN BINARY -- Performance may suffer";
#endif
#ifdef MEMORY_SANITIZER
  LOG(ERROR) << "WARNING: MSAN BINARY -- Performance may suffer";
#endif
  {
    std::ostringstream ss;
    DumpEnvFlag(&ss);
    LOG(INFO) << "goma flags:" << ss.str();
  }
  FlushLogFiles();
}

}  // namespace devtools_goma
