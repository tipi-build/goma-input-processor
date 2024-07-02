// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_compiler_info_builder_helper.h"

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "file_dir.h"
#include "glog/logging.h"
#include "path.h"
#include "path_resolver.h"

#ifdef _WIN32
#include "posix_helper_win.h"
#endif

namespace devtools_goma {

namespace {

// Given a clang binary in |clang_dir|, add paths of its library file
// dependencies to |resource_paths|
void CollectClangDependentLibs(absl::string_view clang_dir,
                               absl::string_view cwd,
                               std::vector<std::string>* resource_paths) {
  // Also, collect all dependent libraries by ldd.
  // Currently, instead using ldd, just list the necessary files.
  // TODO: Really use ldd to collect necessary libraries.
#ifdef __linux__
  // this part is used only for PNaClClang.
  // NaClClang uses generic ELF loader in
  // GCCCompilerInfoBuilder::SetTypeSpecificCompilerInfo
  const std::string lib_dir = file::JoinPath(clang_dir, "..", "lib");

  // pnacl_newlib
  resource_paths->push_back(file::JoinPath(lib_dir, "libLLVM-3.7svn.so"));
  std::string libcxxso = file::JoinPath(lib_dir, "libc++.so.1");
  std::string abs_libcxxso = file::JoinPathRespectAbsolute(cwd, libcxxso);
  // new nacl toolchain link c++ stdlib statically, so there is no libc++.so.1.
  // http://b/156639786
  if (access(abs_libcxxso.c_str(), R_OK) == 0) {
    resource_paths->push_back(std::move(libcxxso));
  }

  // PNaClClang doesn't exist in saigo_newlib
#elif defined(__MACH__)
  const std::string lib_dir = file::JoinPath(clang_dir, "..", "lib");

  // pnacl_newlib
  std::string libllvm37dylib = file::JoinPath(lib_dir, "libLLVM-3.7svn.dylib");
  std::string abs_libllvm37dylib =
      file::JoinPathRespectAbsolute(cwd, libllvm37dylib);
  if (access(abs_libllvm37dylib.c_str(), R_OK) == 0) {
    resource_paths->push_back(std::move(libllvm37dylib));
    std::string libcxxdylib = file::JoinPath(lib_dir, "libc++.1.dylib");
    std::string abs_libcxxdylib =
        file::JoinPathRespectAbsolute(cwd, libcxxdylib);
    // new nacl toolchain link c++ stdlib statically, so there is no
    // libc++.1.dylib. http://b/156639786
    if (access(abs_libcxxdylib.c_str(), R_OK) == 0) {
      resource_paths->push_back(std::move(libcxxdylib));
    }
  }

  // saigo_newlib
  std::string libllvmdylib = file::JoinPath(lib_dir, "libLLVM.dylib");
  std::string abs_libllvmdylib =
      file::JoinPathRespectAbsolute(cwd, libllvmdylib);
  if (access(abs_libllvmdylib.c_str(), R_OK) == 0) {
    resource_paths->push_back(std::move(libllvmdylib));
    resource_paths->push_back(file::JoinPath(lib_dir, "libclang-cpp.dylib"));
  }
#elif defined(_WIN32)

  // pnacl_newlib
  std::string libllvm37dll = file::JoinPath(clang_dir, "LLVM-3.7svn.dll");
  std::string abs_libllvm37dll =
      file::JoinPathRespectAbsolute(cwd, libllvm37dll);
  if (access(abs_libllvm37dll.c_str(), R_OK) == 0) {
    resource_paths->push_back(std::move(libllvm37dll));
    resource_paths->push_back(file::JoinPath(clang_dir, "libstdc++-6.dll"));
    resource_paths->push_back(file::JoinPath(clang_dir, "libgcc_s_sjlj-1.dll"));
    resource_paths->push_back(file::JoinPath(clang_dir, "libwinpthread-1.dll"));
  }

  // saigo_newlib depends no *.dll in saigo_newlib.
#else
#error "unsupported platform"
#endif
}

}  // namespace

#ifdef _WIN32
// static
std::string NaClCompilerInfoBuilderHelper::GetNaClToolchainRoot(
    const std::string& normal_nacl_gcc_path) {
  return PathResolver::ResolvePath(
      file::JoinPath(file::Dirname(normal_nacl_gcc_path), ".."));
}
#endif

// static
void NaClCompilerInfoBuilderHelper::CollectPNaClClangResources(
    const std::string& local_compiler_path,
    const std::string& cwd,
    std::vector<std::string>* resource_paths) {
  // If compiler is pnacl, gather all pydir/*.py (don't gather other files.)

  absl::string_view local_compiler_dir = file::Dirname(local_compiler_path);
  std::vector<DirEntry> entries;
  std::string pydir(file::JoinPath(local_compiler_dir, "pydir"));
  std::string abs_pydir = file::JoinPathRespectAbsolute(cwd, pydir);
  if (ListDirectory(abs_pydir, &entries)) {
    for (const auto& entry : entries) {
      if (!entry.is_dir && absl::EndsWith(entry.name, ".py")) {
        resource_paths->push_back(file::JoinPath(pydir, entry.name));
      }
    }
  }

  // REV is used for --version.
  resource_paths->push_back(file::JoinPath(local_compiler_dir, "..", "REV"));

  resource_paths->push_back(file::JoinPath(local_compiler_dir, "driver.conf"));

#ifdef __linux__
  // subprograms? pnacl-clang needs this, but pnacl-clang++ not? not sure the
  // exact condition.
  resource_paths->push_back(file::JoinPath(local_compiler_dir, "pnacl-llc"));
#elif defined(__MACH__)
  // TODO: Get corresponding Mac paths. For now, let it fall back
  // to local compile.
#elif defined(_WIN32)
  resource_paths->push_back(file::JoinPath(local_compiler_dir, "clang.exe"));
#else
#error "unsupported platform"
#endif

  CollectClangDependentLibs(local_compiler_dir, cwd, resource_paths);
}

// static
void NaClCompilerInfoBuilderHelper::CollectNaClGccResources(
    const std::string& local_compiler_path,
    const std::string& cwd,
    std::vector<std::string>* resource_paths) {
  absl::string_view local_dir = file::Dirname(local_compiler_path);

  const std::string libexec_dir = file::JoinPath(local_dir, "..", "libexec");
  const std::string libexec_gcc_dir =
      file::JoinPath(libexec_dir, "gcc", "x86_64-nacl", "4.4.3");

  // this is subprogram?
  // Note this verbose path is actually used in nacl-gcc.
  const std::string nacl_bin_dir = file::JoinPath(
      local_dir, "..", "lib", "gcc", "x86_64-nacl", "4.4.3", "..",
      "..", "..", "..", "x86_64-nacl", "bin");

#ifdef __linux__
  resource_paths->push_back(file::JoinPath(libexec_gcc_dir, "cc1"));
  resource_paths->push_back(file::JoinPath(libexec_gcc_dir, "cc1plus"));
  resource_paths->push_back(file::JoinPath(nacl_bin_dir, "as"));
#elif defined(__MACH__)
  // TODO: Get corresponding Mac paths.
#elif defined(_WIN32)
  resource_paths->push_back(file::JoinPath(libexec_dir, "cygiconv-2.dll"));
  resource_paths->push_back(file::JoinPath(libexec_dir, "cygintl-8.dll"));
  resource_paths->push_back(file::JoinPath(libexec_dir, "cygwin1.dll"));
  resource_paths->push_back(file::JoinPath(libexec_dir, "x86_64-nacl-as.exe"));

  absl::string_view compiler_name = file::Basename(local_compiler_path);
  resource_paths->push_back(file::JoinPath(libexec_dir, compiler_name));

  resource_paths->push_back(file::JoinPath(libexec_gcc_dir, "cc1.exe"));
  resource_paths->push_back(file::JoinPath(libexec_gcc_dir, "cc1plus.exe"));
  resource_paths->push_back(file::JoinPath(libexec_gcc_dir, "cyggcc_s-1.dll"));
  resource_paths->push_back(file::JoinPath(libexec_gcc_dir, "cygiconv-2.dll"));
  resource_paths->push_back(file::JoinPath(libexec_gcc_dir, "cygintl-8.dll"));
  resource_paths->push_back(file::JoinPath(libexec_gcc_dir, "cygstdc++-6.dll"));
  resource_paths->push_back(file::JoinPath(libexec_gcc_dir, "cygwin1.dll"));

  resource_paths->push_back(file::JoinPath(nacl_bin_dir, "as.exe"));
#else
#error "unsupported platform"
#endif
}

// static
void NaClCompilerInfoBuilderHelper::CollectNaClClangResources(
    const std::string& local_compiler_path,
    const std::string& cwd,
    std::vector<std::string>* resource_paths) {
#ifdef __linux__
  // linux is handled by ELF loader later
  // in GCCCompilerInfoBuilder::SetTypeSpecificCompilerInfo,
  // so no need to handle special for x86_64-nacl-{clang,clang++} here.
#else
  absl::string_view local_dir = file::Dirname(local_compiler_path);
  CollectClangDependentLibs(local_dir, cwd, resource_paths);
#endif
}

}  // namespace devtools_goma
