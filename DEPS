# Copyright 2019 Google LLC.

vars = {
     "chromium_git": "https://chromium.googlesource.com",
     "clang_revision": "657c88b9c012b3f8036f891f2c819626e74f31c9",
     "gn_version": "git_revision:dfcbc6fed0a8352696f92d67ccad54048ad182b3",
     "mingw_version": "version:2@11.2.0-6",
     "checkout_mingw": False,
}

deps = {
     # protobuf > 3.15.6
     # TODO: use released proto including
     # https://github.com/protocolbuffers/protobuf/blob/ee4f2492ea4e7ff120f68a792af870ee30435aa5/src/google/protobuf/io/zero_copy_stream.h#L122
     "client/third_party/protobuf/protobuf":
     "https://github.com/google/protobuf.git@6aa539bf0195f188ff86efe6fb8bfa2b676cdd46",

     # google-glog v0.5.0
     "client/third_party/glog":
     "https://github.com/google/glog.git@8f9ccfe770add9e4c64e9b25c102658e3c763b73",

     # googletest 1.8.1
     "client/third_party/gtest":
     Var('chromium_git') + '/external/github.com/google/googletest.git' + '@' +
         '2fe3bd994b3189899d93f1d5a881e725e046fdc2',

     # zlib
     "client/third_party/zlib":
     "https://chromium.googlesource.com/chromium/src/third_party/zlib@a6d209ab932df0f1c9d5b7dc67cfa74e8a3272c0",

     # xz v5.2.0
     "client/third_party/xz":
     "https://goma.googlesource.com/xz.git@fbafe6dd0892b04fdef601580f2c5b0e3745655b",

     # jsoncpp
     "client/third_party/jsoncpp/source":
     Var("chromium_git") + '/external/github.com/open-source-parsers/jsoncpp.git@9059f5cad030ba11d37818847443a53918c327b1', # 1.9.4

     # chrome's tools/clang
     "client/tools/clang":
     Var("chromium_git") + "/chromium/src/tools/clang.git@" +
         Var("clang_revision"),

     # chrome's deps/third_party/boringssl
     "client/third_party/boringssl/src":
     "https://boringssl.googlesource.com/boringssl@d24a38200fef19150eef00cad35b138936c08767",

     # google-breakpad
     "client/third_party/breakpad/breakpad":
     Var("chromium_git") + "/breakpad/breakpad.git@" +
         "7685201906b55d93be6151fa4681e8b24d6c9c74",

     # lss
     "client/third_party/lss":
     Var("chromium_git") + "/linux-syscall-support.git@" +
         "a89bf7903f3169e6bc7b8efc10a73a7571de21cf",

     # nasm
     "client/third_party/nasm":
     Var("chromium_git") + "/chromium/deps/nasm.git@" +
         "e9be5fd6d723a435ca2da162f9e0ffcb688747c1",

     # chromium's buildtools containing libc++, libc++abi, clang_format and gn.
     "client/buildtools":
     Var("chromium_git") + "/chromium/src/buildtools@" +
         "37dc929ecb351687006a61744b116cda601753d7",

     # libFuzzer
     "client/third_party/libFuzzer/src":
     Var("chromium_git") +
         "/chromium/llvm-project/compiler-rt/lib/fuzzer.git@" +
         "debe7d2d1982e540fbd6bd78604bf001753f9e74",

     # libprotobuf-mutator
     "client/third_party/libprotobuf-mutator/src":
     Var("chromium_git") +
         "/external/github.com/google/libprotobuf-mutator.git@" +
         "439e81f8f4847ec6e2bf11b3aa634a5d8485633d",

     # abseil
     "client/third_party/abseil/src":
     "https://github.com/abseil/abseil-cpp.git@0064d9db90d32d35e9f9d70e2df4ddf8d0ab1257",

     # google benchmark v1.4.1
     "client/third_party/benchmark/src":
     "https://github.com/google/benchmark.git@0d98dba29d66e93259db7daa53a9327df767a415",

     # Jinja2 template engine v3.1.2
     "client/third_party/jinja2":
     "https://github.com/pallets/jinja.git@b08cd4bc64bb980df86ed2876978ae5735572280",

     # Markupsafe module v1.0
     "client/third_party/markupsafe":
     "https://github.com/pallets/markupsafe.git@d2a40c41dd1930345628ea9412d97e159f828157",

     # depot_tools
     'client/third_party/depot_tools':
     Var('chromium_git') + '/chromium/tools/depot_tools.git',

     # gflags 2.2.1
     "client/third_party/gflags/src":
     "https://github.com/gflags/gflags.git@46f73f88b18aee341538c0dfc22b1710a6abedef",

     # libyaml dist-0.2.2
     "client/third_party/libyaml/src":
     "https://github.com/yaml/libyaml.git@d407f6b1cccbf83ee182144f39689babcb220bd6",

     # chromium's build.
     "client/third_party/chromium_build":
     "https://chromium.googlesource.com/chromium/src/build/@39eb47bf26d31b548638157fafe945f691db9ae8",

     'client/tools/clang/dsymutil': {
       'packages': [
         {
           'package': 'chromium/llvm-build-tools/dsymutil',
           'version': 'M56jPzDv1620Rnm__jTMYS62Zi8rxHVq7yw0qeBFEgkC',
         }
       ],
       'condition': 'checkout_mac or checkout_ios',
       'dep_type': 'cipd',
     },

     # Go toolchain.
     'client/third_party/go': {
         'packages': [
             {
                 'package': 'infra/3pp/tools/go/${{platform}}',
                 'version': 'version:2@1.21.6',
             },
         ],
         'dep_type': 'cipd',
     },

     # libc++
     'client/buildtools/third_party/libc++/trunk':
     Var('chromium_git') + '/external/github.com/llvm/llvm-project/libcxx.git' +
         '@' + '8fa87946779682841e21e2da977eccfb6cb3bded',

     # libc++abi
     'client/buildtools/third_party/libc++abi/trunk':
     Var('chromium_git') + '/external/github.com/llvm/llvm-project/libcxxabi.git' +
         '@' + '6918862bfc2bff22b45058fac22b1596c49982fb',

     # clang-format helper scripts, used by `git cl format`.
     'client/buildtools/clang_format/script':
     Var('chromium_git') +
     '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git' +
         '@' + '99803d74e35962f63a775f29477882afd4d57d94',

     # GN
    'client/buildtools/linux64': {
      'packages': [
        {
          'package': 'gn/gn/linux-amd64',
          'version': Var('gn_version'),
        }
      ],
      'dep_type': 'cipd',
      'condition': 'host_os == "linux"',
    },
    'client/buildtools/mac': {
      'packages': [
        {
          'package': 'gn/gn/mac-${{arch}}',
          'version': Var('gn_version'),
        }
      ],
      'dep_type': 'cipd',
      'condition': 'host_os == "mac"',
    },
    'client/buildtools/win': {
      'packages': [
        {
          'package': 'gn/gn/windows-amd64',
          'version': Var('gn_version'),
        }
      ],
      'dep_type': 'cipd',
      'condition': 'host_os == "win"',
    },
    'client/third_party/mingw': {
      'packages': [
        {
          'package': 'infra/3pp/build_support/mingw/windows-amd64',
          'version': Var('mingw_version'),
        }
      ],
      'dep_type': 'cipd',
      'condition': 'host_os == "win" and checkout_mingw',
    },
    'client/third_party/ninja': {
      'packages': [
        # https://chrome-infra-packages.appspot.com/p/infra/3pp/tools/ninja
        {
          'package': 'infra/3pp/tools/ninja/${{platform}}',
          'version': 'version:2@1.11.1.chromium.6',
        }
      ],
      'dep_type': 'cipd',
    },
}

hooks = [
     # Download to make Linux Goma client linked with an old libc.
     {
       'name': 'sysroot_x64',
       'pattern': '.',
       'condition': 'checkout_linux',
       'action': [
         'python3',
         ('client/third_party/chromium_build/linux/sysroot_scripts/'
          'install-sysroot.py'),
          '--arch=x64',
       ],
     },

     # Update the Windows toolchain if necessary. Must run before 'clang' below.
     {
       'name': 'win_toolchain',
       'pattern': '.',
       'action': [
         'python3',
         'client/third_party/chromium_build/vs_toolchain.py',
         'update',
       ],
     },
     {
       "name": "clang",
       "pattern": ".",
       "action": ["python3", "client/tools/clang/scripts/update.py"],
     },

     {
       # Update LASTCHANGE.
       'name': 'lastchange',
       'pattern': '.',
       'action': ['python3',
                  'client/third_party/chromium_build/util/lastchange.py',
                  '-o', 'client/build/util/LASTCHANGE'],
     },

     # Pull clang-format binaries using checked-in hashes.
     {
         'name': 'clang_format_win',
         'pattern': '.',
         'condition': 'host_os == "win"',
         'action': [ 'download_from_google_storage',
                     '--no_resume',
                     '--no_auth',
                     '--bucket', 'chromium-clang-format',
                     '-s', 'client/buildtools/win/clang-format.exe.sha1',
         ],
     },
     {
         'name': 'clang_format_mac',
         'pattern': '.',
         'condition': 'host_os == "mac"',
         'action': [ 'download_from_google_storage',
                     '--no_resume',
                     '--no_auth',
                     '--bucket', 'chromium-clang-format',
                     '-s', 'client/buildtools/mac/clang-format.sha1',
         ],
     },
     {
         'name': 'clang_format_linux',
         'pattern': '.',
         'condition': 'host_os == "linux"',
         'action': [ 'download_from_google_storage',
                     '--no_resume',
                     '--no_auth',
                     '--bucket', 'chromium-clang-format',
                     '-s', 'client/buildtools/linux64/clang-format.sha1',
         ],
     },
     # Update the Mac toolchain if necessary.
     {
       'name': 'mac_toolchain',
       'pattern': '.',
       'condition': 'checkout_ios or checkout_mac',
       'action': ['python3',
                  'client/third_party/chromium_build/mac_toolchain.py'],
     },

     # Ensure that the DEPS'd "depot_tools" has its self-update capability
     # disabled.
     {
       'name': 'disable_depot_tools_selfupdate',
       'pattern': '.',
       'action': [
         'python3',
         'client/third_party/depot_tools/update_depot_tools_toggle.py',
         '--disable',
       ],
     },
]
