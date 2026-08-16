include(ExternalProject)

include(ProcessorCount)
ProcessorCount(NUM_CPU)

set(LIST_SEPARATOR "!")
string(REPLACE ";" "${LIST_SEPARATOR}"
  OSX_ARCHITECTURES_LISTSEP "${CMAKE_OSX_ARCHITECTURES}")
string(REPLACE ";" ","
  OSX_ARCHITECTURES_COMMA "${CMAKE_OSX_ARCHITECTURES}")

if("$ENV{CI}" STREQUAL "1" OR "$ENV{CI}" STREQUAL "true")
  set(ENABLE_LOGS NO)
else()
  set(ENABLE_LOGS YES)
endif()

set(TOOL_LIST
  scons
  pkg-config
  autoconf
  automake
  libtool
)
foreach(TOOL IN LISTS TOOL_LIST)
  find_program(${TOOL}_EXE ${TOOL})
  if(NOT ${TOOL}_EXE)
    string(REPLACE ";" " " TOOL_LIST "${TOOL_LIST}")
    message(FATAL_ERROR
      "\nMissing required build tool: '${TOOL}'\nTry running:\nbrew install ${TOOL_LIST}\n")
  endif()
endforeach()

# ExternalProject sub-builds start a fresh CMake, which does not inherit the
# toolchain from this one. Without forwarding it they configure with the host
# compiler while still being handed CMAKE_OSX_ARCHITECTURES, which fails
# outright when cross-compiling and silently produces host binaries when it
# does not. Expands to nothing for a native macOS build.
if(CMAKE_TOOLCHAIN_FILE)
  set(DEP_TOOLCHAIN_ARG -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()

# Several dependencies still declare cmake_minimum_required below 3.5, which
# CMake 4.x refuses outright: "Compatibility with CMake < 3.5 has been removed
# from CMake". libASPL v3.1.1 is the first to hit it. Nothing to do with
# cross-compiling — a native build with CMake 4 fails the same way.
# roc-toolkit already passes this to its own sub-builds.
set(DEP_POLICY_ARG -DCMAKE_POLICY_VERSION_MINIMUM=3.5)

# A cross toolchain sets CMAKE_FIND_ROOT_PATH_MODE_{LIBRARY,INCLUDE,PACKAGE} to
# ONLY, so find_package and friends look only inside the SDK root and cannot
# see the dependencies we just cross-built into 3rdparty/. Adding those
# prefixes to the find root is the standard way to make cross-built deps
# discoverable — without it gRPC fails with "Could NOT find OpenSSL" despite
# being handed OPENSSL_ROOT_DIR, and the main build would then fail the same
# way looking for libASPL and gRPC.
if(CMAKE_TOOLCHAIN_FILE)
  foreach(DEP roc aspl boringssl zlib absl grpc fmt spdlog cli11 googletest)
    list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/${DEP})
  endforeach()
endif()

# gRPC's vendored zlib is broken on macOS however it is built, natively
# included: zutil.h keys off TARGET_OS_MAC, concludes it is targeting classic
# Mac OS, and defines "fdopen(fd,mode) NULL", after which Apple's own <stdio.h>
# fails to parse. Point gRPC at the modern zlib built below instead.
set(DEP_GRPC_PROVIDER_ARGS
  -DgRPC_ZLIB_PROVIDER=package
  -DZLIB_USE_STATIC_LIBS=ON)

# gRPCConfig.cmake calls find_package(ZLIB) when the provider is "package", so
# the main pass has to resolve zlib to the same copy gRPC was built against
# rather than to whichever one the SDK happens to offer.
set(ZLIB_ROOT ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/zlib)

# zlib's CMake builds libz.a and libz.dylib both, and FindZLIB prefers the
# shared one. That is fatal here rather than merely untidy: the result is a HAL
# bundle with an @rpath dependency on a dylib in the build tree, which
# coreaudiod's sandboxed helper cannot read (errno=13) and which is not inside
# the bundle to begin with, so the plug-in fails to dlopen. Everything else this
# project links is static for the same reason.
set(ZLIB_USE_STATIC_LIBS ON)

# gRPC 1.63 predates clang making missing-template-arg-list-after-template-kw an
# error by default, so basic_seq.h stops compiling under any sufficiently recent
# clang -- the Command Line Tools' clang 21 as much as the newer host clang
# osxcross uses. Put it back to a warning.
set(DEP_GRPC_CXXFLAGS_ARG
  -DCMAKE_CXX_FLAGS=-Wno-missing-template-arg-list-after-template-kw)

# Cross-only: gRPC's vendored abseil predates the "SHELL:" fix for -Xarch flag
# pairing. A native build should stay as close to upstream as possible.
if(CMAKE_TOOLCHAIN_FILE)
  list(APPEND DEP_GRPC_PROVIDER_ARGS
    -DgRPC_ABSL_PROVIDER=package)
endif()

# roc-toolkit builds under scons rather than CMake, so it needs telling
# separately. Its SConstruct exposes --host for exactly this; without it scons
# picks the native compiler and quietly emits a host-architecture libroc.a
# that links nowhere useful.
set(SCONS_HOST "" CACHE STRING
  "Target triple passed to roc-toolkit's scons --host (empty = native build)")
if(SCONS_HOST)
  set(SCONS_HOST_ARG --host=${SCONS_HOST})
endif()

# Which third-party libraries roc builds for itself, and optionally at which
# versions ("all" or e.g. "all,libuv:1.51.0"). roc's default libuv is 1.35.0,
# from 2020, which cannot compile against a modern macOS SDK: it defines strict
# _POSIX_C_SOURCE, so the SDK headers hide the BSD types and socket constants
# it then goes on to use — u_int, IP_TTL, IP_MULTICAST_TTL and friends all come
# back undeclared.
set(ROC_3RDPARTY "all" CACHE STRING
  "Value passed to roc-toolkit's scons --build-3rdparty")

# Roc
set(ROC_SOURCE "https://github.com/ArviFoxy/roc-toolkit.git"
  CACHE STRING "roc-toolkit git repository (URL or local path)")
set(ROC_TAG "multiroom"
  CACHE STRING "roc-toolkit git tag/branch/commit")

set(SCONS_CMD
  scons -j ${NUM_CPU}
    -C ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/roc/src/roc_lib
    --prefix=${CMAKE_CURRENT_BINARY_DIR}/3rdparty/roc
    --enable-static
    --disable-shared
    --disable-tools
    --disable-sox
    --disable-openssl
    --build-3rdparty=${ROC_3RDPARTY}
    --compiler-launcher=${CMAKE_CXX_COMPILER_LAUNCHER}
    ${SCONS_HOST_ARG}
    --macos-platform=${CMAKE_OSX_DEPLOYMENT_TARGET}
    --macos-arch=${OSX_ARCHITECTURES_COMMA}
)
# Local fork rather than upstream: the server's receivers run this tree, and
# upstream's "master" is a moving target that can change the build underneath
# us. Pinned to a commit for that second reason as much as the first — the
# fork's own changes (Prometheus metrics, latency tuner) are receiver-side, so
# a sender built from upstream would very likely interoperate anyway.
#
# ROC_SOURCE / ROC_TAG are overridable so a CI or clean-room build can still
# point at a URL instead of this machine's filesystem.
ExternalProject_Add(roc_lib
  GIT_REPOSITORY "${ROC_SOURCE}"
  GIT_TAG "${ROC_TAG}"
  GIT_SHALLOW OFF
  GIT_PROGRESS ON
  # The one dependency that is not pinned to a tag. ROC_TAG defaults to a branch
  # that is actively developed, so leaving the clone disconnected means a build
  # silently keeps whatever was fetched the first time and no later commit ever
  # arrives. Every other dependency here is a fixed tag or commit and stays
  # disconnected, where re-fetching would only cost time. The price is that a
  # build now needs the network to reach the roc remote; pass
  # -DROC_TAG=<sha> to pin it if that is a problem.
  UPDATE_DISCONNECTED OFF
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/roc
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ${SCONS_CMD}
  INSTALL_COMMAND ${SCONS_CMD} install
  LOG_DOWNLOAD ${ENABLE_LOGS}
  LOG_CONFIGURE ${ENABLE_LOGS}
  LOG_BUILD ${ENABLE_LOGS}
  LOG_INSTALL ${ENABLE_LOGS}
)
include_directories(SYSTEM
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/roc/include
)
link_directories(
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/roc/lib
)

# libASPL
ExternalProject_Add(aspl_lib
  GIT_REPOSITORY "https://github.com/gavv/libASPL.git"
  GIT_TAG "v3.1.1"
  GIT_SHALLOW OFF
  GIT_PROGRESS ON
  UPDATE_DISCONNECTED ON
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/aspl
  LIST_SEPARATOR ${LIST_SEPARATOR}
  CMAKE_ARGS
    -DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}
    ${DEP_TOOLCHAIN_ARG}
    ${DEP_POLICY_ARG}
    -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
    -DCMAKE_OSX_ARCHITECTURES=${OSX_ARCHITECTURES_LISTSEP}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
  BUILD_COMMAND
    ${CMAKE_COMMAND} --build . -- -j ${NUM_CPU}
  LOG_DOWNLOAD ${ENABLE_LOGS}
  LOG_CONFIGURE ${ENABLE_LOGS}
  LOG_BUILD ${ENABLE_LOGS}
  LOG_INSTALL ${ENABLE_LOGS}
)
include_directories(SYSTEM
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/aspl/include
)
list(PREPEND CMAKE_PREFIX_PATH
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/aspl/lib/cmake
)

# BoringSSL
ExternalProject_Add(boringssl_lib
  GIT_REPOSITORY "https://github.com/google/boringssl.git"
  GIT_TAG "2db0eb3f96a5756298dcd7f9319e56a98585bd10"
  GIT_SHALLOW OFF
  GIT_PROGRESS ON
  UPDATE_DISCONNECTED ON
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/boringssl
  LIST_SEPARATOR ${LIST_SEPARATOR}
  CMAKE_ARGS
    -DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}
    ${DEP_TOOLCHAIN_ARG}
    ${DEP_POLICY_ARG}
    -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
    -DCMAKE_OSX_ARCHITECTURES=${OSX_ARCHITECTURES_LISTSEP}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DOPENSSL_NO_ASM=1
  BUILD_COMMAND
    ${CMAKE_COMMAND} --build . -- -j ${NUM_CPU}
  LOG_DOWNLOAD ${ENABLE_LOGS}
  LOG_CONFIGURE ${ENABLE_LOGS}
  LOG_BUILD ${ENABLE_LOGS}
  LOG_INSTALL ${ENABLE_LOGS}
)
include_directories(SYSTEM
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/boringssl/include
)
list(PREPEND CMAKE_PREFIX_PATH
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/boringssl/lib/cmake
)

# zlib
# gRPC vendors a very old zlib whose zutil.h keys off TARGET_OS_MAC and decides
# it is building for classic Mac OS: it sets OS_CODE 7 and defines
# "fdopen(fd,mode) NULL", which then makes Apple's own <stdio.h> fail to parse.
# The same copy also passes -msse4.1 on arm64. Building a modern zlib ourselves
# and pointing gRPC at it with gRPC_ZLIB_PROVIDER=package avoids both.
ExternalProject_Add(zlib_lib
  GIT_REPOSITORY "https://github.com/madler/zlib.git"
  GIT_TAG "v1.3.1"
  GIT_SHALLOW ON
  GIT_PROGRESS ON
  UPDATE_DISCONNECTED ON
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/zlib
  LIST_SEPARATOR ${LIST_SEPARATOR}
  CMAKE_ARGS
    -DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}
    ${DEP_TOOLCHAIN_ARG}
    ${DEP_POLICY_ARG}
    -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
    -DCMAKE_OSX_ARCHITECTURES=${OSX_ARCHITECTURES_LISTSEP}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DZLIB_BUILD_EXAMPLES=OFF
  BUILD_COMMAND
    ${CMAKE_COMMAND} --build . -- -j ${NUM_CPU}
  # zlib installs the shared library unconditionally; deleting it afterwards
  # leaves nothing for a stray find_package to pick up.
  INSTALL_COMMAND
    ${CMAKE_COMMAND} --build . --target install
  COMMAND
    sh -c "rm -f <INSTALL_DIR>/lib/libz*.dylib"
  LOG_DOWNLOAD ${ENABLE_LOGS}
  LOG_CONFIGURE ${ENABLE_LOGS}
  LOG_BUILD ${ENABLE_LOGS}
  LOG_INSTALL ${ENABLE_LOGS}
)
include_directories(SYSTEM
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/zlib/include
)
list(PREPEND CMAKE_PREFIX_PATH
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/zlib/lib/cmake
)

# abseil
# Cross-compiling only. Natively, gRPC builds and installs its own vendored
# abseil (gRPC_ABSL_PROVIDER defaults to "module"), and injecting a second,
# newer copy ahead of it on the include path is an ODR trap rather than a
# convenience: abseil puts its version in an inline namespace, so roc-vad's
# sources compile against absl::lts_20240722 while gRPC's libraries export
# absl::lts_20240116, and every absl symbol comes up undefined at link time.
# The version skew below is deliberate and only safe when it applies to gRPC
# too, which is exactly when gRPC_ABSL_PROVIDER=package is passed.
if(CMAKE_TOOLCHAIN_FILE)
  # gRPC 1.63 vendors abseil 20240116.0, whose CMake emits
  # "-Xarch_x86_64 -maes -Xarch_x86_64 -msse4.1" for randen_hwaes. CMake
  # de-duplicates repeated compile options, collapsing that to
  # "-Xarch_x86_64 -maes -msse4.1" — and since -Xarch_ guards only the argument
  # that follows it, -msse4.1 leaks onto the arm64 compile and clang rejects it.
  # Abseil fixed this in 20240722 ("Fixup absl_random compile breakage in Apple
  # ARM64 targets") by using CMake's SHELL: prefix, which keeps the pair together
  # and exempt from de-duplication. 20240722.2 is the next LTS after the version
  # gRPC expects, so it is the smallest jump that carries the fix.
  ExternalProject_Add(absl_lib
    GIT_REPOSITORY "https://github.com/abseil/abseil-cpp.git"
    GIT_TAG "20240722.2"
    GIT_SHALLOW ON
    GIT_PROGRESS ON
    UPDATE_DISCONNECTED ON
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/absl
    LIST_SEPARATOR ${LIST_SEPARATOR}
    CMAKE_ARGS
      -DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}
      ${DEP_TOOLCHAIN_ARG}
      ${DEP_POLICY_ARG}
      -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
      -DCMAKE_OSX_ARCHITECTURES=${OSX_ARCHITECTURES_LISTSEP}
      -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
      -DCMAKE_CXX_STANDARD=17
      -DABSL_PROPAGATE_CXX_STD=ON
      -DABSL_ENABLE_INSTALL=ON
      -DBUILD_TESTING=OFF
    BUILD_COMMAND
      ${CMAKE_COMMAND} --build . -- -j ${NUM_CPU}
    LOG_DOWNLOAD ${ENABLE_LOGS}
    LOG_CONFIGURE ${ENABLE_LOGS}
    LOG_BUILD ${ENABLE_LOGS}
    LOG_INSTALL ${ENABLE_LOGS}
  )
  include_directories(SYSTEM
    ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/absl/include
  )
  list(PREPEND CMAKE_PREFIX_PATH
    ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/absl/lib/cmake
  )
endif()

# gRPC
ExternalProject_Add(grpc_lib
  GIT_REPOSITORY "https://github.com/grpc/grpc.git"
  GIT_TAG "v1.63.0"
  GIT_SHALLOW ON
  GIT_PROGRESS ON
  UPDATE_DISCONNECTED ON
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/grpc
  LIST_SEPARATOR ${LIST_SEPARATOR}
  CMAKE_ARGS
    -DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}
    ${DEP_TOOLCHAIN_ARG}
    ${DEP_POLICY_ARG}
    -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
    -DCMAKE_OSX_ARCHITECTURES=${OSX_ARCHITECTURES_LISTSEP}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DBUILD_TESTING=OFF
    -DgRPC_BUILD_TESTS=OFF
    -DgRPC_BUILD_CODEGEN=ON
    -DgRPC_BUILD_CSHARP_EXT=OFF
    -DgRPC_BUILD_GRPC_CPP_PLUGIN=ON
    -DgRPC_BUILD_GRPC_CSHARP_PLUGIN=OFF
    -DgRPC_BUILD_GRPC_NODE_PLUGIN=OFF
    -DgRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN=OFF
    -DgRPC_BUILD_GRPC_PHP_PLUGIN=OFF
    -DgRPC_BUILD_GRPC_PYTHON_PLUGIN=OFF
    -DgRPC_BUILD_GRPC_RUBY_PLUGIN=OFF
    -DgRPC_SSL_PROVIDER=package
    ${DEP_GRPC_PROVIDER_ARGS}
    ${DEP_GRPC_CXXFLAGS_ARG}
    -DZLIB_ROOT=${CMAKE_CURRENT_BINARY_DIR}/3rdparty/zlib
    -DOPENSSL_ROOT_DIR=${CMAKE_CURRENT_BINARY_DIR}/3rdparty/boringssl
    -DOPENSSL_USE_STATIC_LIBS=ON
    # cross toolchains restrict find_* to the SDK root; BoringSSL lives outside
    # it, so OPENSSL_ROOT_DIR alone is not enough to make FindOpenSSL see it
    -DCMAKE_FIND_ROOT_PATH=${CMAKE_CURRENT_BINARY_DIR}/3rdparty/boringssl${LIST_SEPARATOR}${CMAKE_CURRENT_BINARY_DIR}/3rdparty/zlib${LIST_SEPARATOR}${CMAKE_CURRENT_BINARY_DIR}/3rdparty/absl
  BUILD_COMMAND
    ${CMAKE_COMMAND} --build . -- -j ${NUM_CPU}
  LOG_DOWNLOAD ${ENABLE_LOGS}
  LOG_CONFIGURE ${ENABLE_LOGS}
  LOG_BUILD ${ENABLE_LOGS}
  LOG_INSTALL ${ENABLE_LOGS}
)
include_directories(SYSTEM
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/grpc/include
)
list(PREPEND CMAKE_PREFIX_PATH
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/grpc/lib/cmake
)
get_filename_component(GRPC_BIN_DIR
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/grpc/bin
  ABSOLUTE
)

# fmt
ExternalProject_Add(fmt_lib
  GIT_REPOSITORY "https://github.com/fmtlib/fmt.git"
  GIT_TAG "10.2.1"
  GIT_SHALLOW ON
  GIT_PROGRESS ON
  UPDATE_DISCONNECTED ON
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/fmt
  LIST_SEPARATOR ${LIST_SEPARATOR}
  CMAKE_ARGS
    -DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}
    ${DEP_TOOLCHAIN_ARG}
    ${DEP_POLICY_ARG}
    -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
    -DCMAKE_OSX_ARCHITECTURES=${OSX_ARCHITECTURES_LISTSEP}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DBUILD_TESTING=OFF
    -DFMT_DOC=OFF
    -DFMT_INSTALL=ON
    -DFMT_TEST=OFF
  BUILD_COMMAND
    ${CMAKE_COMMAND} --build . -- -j ${NUM_CPU}
  LOG_DOWNLOAD ${ENABLE_LOGS}
  LOG_CONFIGURE ${ENABLE_LOGS}
  LOG_BUILD ${ENABLE_LOGS}
  LOG_INSTALL ${ENABLE_LOGS}
)
include_directories(SYSTEM
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/fmt/include
)
list(PREPEND CMAKE_PREFIX_PATH
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/fmt/lib/cmake
)

# spdlog
ExternalProject_Add(spdlog_lib
  GIT_REPOSITORY "https://github.com/gabime/spdlog.git"
  GIT_TAG "v1.14.1"
  GIT_SHALLOW ON
  GIT_PROGRESS ON
  UPDATE_DISCONNECTED ON
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/spdlog
  LIST_SEPARATOR ${LIST_SEPARATOR}
  CMAKE_ARGS
    -DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}
    ${DEP_TOOLCHAIN_ARG}
    ${DEP_POLICY_ARG}
    -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
    -DCMAKE_OSX_ARCHITECTURES=${OSX_ARCHITECTURES_LISTSEP}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DCMAKE_PREFIX_PATH=${CMAKE_CURRENT_BINARY_DIR}/3rdparty/fmt/lib/cmake
    -DSPDLOG_FMT_EXTERNAL=ON
  BUILD_COMMAND
    ${CMAKE_COMMAND} --build . -- -j ${NUM_CPU}
  LOG_DOWNLOAD ${ENABLE_LOGS}
  LOG_CONFIGURE ${ENABLE_LOGS}
  LOG_BUILD ${ENABLE_LOGS}
  LOG_INSTALL ${ENABLE_LOGS}
)
include_directories(SYSTEM
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/spdlog/include
)
list(PREPEND CMAKE_PREFIX_PATH
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/spdlog/lib/cmake
)
add_definitions(
  -DSPDLOG_COMPILED_LIB
  -DSPDLOG_FMT_EXTERNAL
)

# CLI111
ExternalProject_Add(cli11_lib
  GIT_REPOSITORY "https://github.com/CLIUtils/CLI11.git"
  GIT_TAG "v2.4.1"
  GIT_SHALLOW ON
  GIT_PROGRESS ON
  UPDATE_DISCONNECTED ON
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/cli11
  LIST_SEPARATOR ${LIST_SEPARATOR}
  CMAKE_ARGS
    -DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}
    ${DEP_TOOLCHAIN_ARG}
    ${DEP_POLICY_ARG}
    -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
    -DCMAKE_OSX_ARCHITECTURES=${OSX_ARCHITECTURES_LISTSEP}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DBUILD_TESTING=OFF
    -DCLI11_BUILD_TESTS=OFF
    -DCLI11_BUILD_EXAMPLES=OFF
    -DCLI11_SINGLE_FILE=ON
  BUILD_COMMAND
    ${CMAKE_COMMAND} --build . -- -j ${NUM_CPU}
  LOG_DOWNLOAD ${ENABLE_LOGS}
  LOG_CONFIGURE ${ENABLE_LOGS}
  LOG_BUILD ${ENABLE_LOGS}
  LOG_INSTALL ${ENABLE_LOGS}
)
include_directories(SYSTEM
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/cli11/include
)

# GoogleTest
ExternalProject_Add(googletest_lib
  GIT_REPOSITORY "https://github.com/google/googletest.git"
  GIT_TAG "v1.14.0"
  GIT_SHALLOW ON
  GIT_PROGRESS ON
  UPDATE_DISCONNECTED ON
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/googletest
  LIST_SEPARATOR ${LIST_SEPARATOR}
  CMAKE_ARGS
    -DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}
    ${DEP_TOOLCHAIN_ARG}
    ${DEP_POLICY_ARG}
    -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
    -DCMAKE_OSX_ARCHITECTURES=${OSX_ARCHITECTURES_LISTSEP}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DBUILD_TESTING=OFF
  BUILD_COMMAND
    ${CMAKE_COMMAND} --build . -- -j ${NUM_CPU}
  LOG_DOWNLOAD ${ENABLE_LOGS}
  LOG_CONFIGURE ${ENABLE_LOGS}
  LOG_BUILD ${ENABLE_LOGS}
  LOG_INSTALL ${ENABLE_LOGS}
)
include_directories(SYSTEM
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/googletest/include
)
list(PREPEND CMAKE_PREFIX_PATH
  ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/googletest/lib/cmake
)

# abseil is only built here when cross-compiling; see the provider args above.
if(CMAKE_TOOLCHAIN_FILE)
  set(DEP_ABSL absl_lib)
endif()

# List of all third-party dependencies.
# Order matters: the loop below makes each entry depend on every entry before
# it, so anything gRPC consumes has to be listed ahead of grpc_lib.
set(ALL_DEPENDENCIES
  roc_lib
  aspl_lib
  boringssl_lib
  zlib_lib
  ${DEP_ABSL}
  grpc_lib
  fmt_lib
  spdlog_lib
  cli11_lib
  googletest_lib
)

# Serialize dependencies
# (each one depends on previous in list)
set(REV_DEPENDENCIES ${ALL_DEPENDENCIES})
list(REVERSE REV_DEPENDENCIES)
set(OTHER_DEPENDENCIES ${ALL_DEPENDENCIES})
foreach(DEPENDENCY IN LISTS REV_DEPENDENCIES)
  list(REMOVE_ITEM OTHER_DEPENDENCIES ${DEPENDENCY})
  if(OTHER_DEPENDENCIES)
    add_dependencies(${DEPENDENCY}
      ${OTHER_DEPENDENCIES}
    )
  endif()
endforeach()

# After building dependencies, touch commit-file
add_custom_command(
  COMMENT "Commit bootstrap"
  DEPENDS ${ALL_DEPENDENCIES}
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/bootstrap.commit
  COMMAND ${CMAKE_COMMAND} -E touch
    ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/bootstrap.commit
)
add_custom_target(commit_bootstrap ALL
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/bootstrap.commit
)
