cmake_minimum_required(VERSION 3.13)

include(cmake/options.cmake)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(roc_vad CXX)

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(COMMON_LIB "rocvad_common")
set(RPC_LIB "rocvad_rpc")
set(DRIVER_LIB "rocvad_driver")

set(TOOL_EXE "roc-vad")
set(TOOL_LIB "rocvad_tool")
set(TEST_EXE "rocvad_test")

set(DRIVER_BUNDLE_NAME "roc_vad" CACHE STRING "")
set(DRIVER_BUNDLE_ID "org.rocstreaming.rocvad" CACHE STRING "")
set(DRIVER_UUID "FE352D6E-B65E-4496-BD3D-ECBBB6750E8B" CACHE STRING "")
set(DRIVER_SOCKET "127.0.0.1:9712" CACHE STRING "")
set(DRIVER_MANUFACTURER "Roc Streaming" CACHE STRING "")
set(DRIVER_COPYRIGHT "Copyright (c) Roc Streaming authors" CACHE STRING "")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

add_compile_options(
  # enable warnings
  "-Wall"
  "-Wextra"

  # disable warnings
  "-Wno-unused-but-set-variable"
  "-Wno-unused-parameter"

  # enable assertions (even in release build)
  "-UNDEBUG"
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(
    "-ggdb"
    "-funwind-tables"
    "-fno-omit-frame-pointer"
  )
endif()

include_directories(
  common
  rpc
)

include(cmake/dependencies.cmake)

if(NOT BOOTSTRAP)
  enable_testing()

  add_subdirectory(common)
  add_subdirectory(rpc)
  add_subdirectory(driver)
  add_subdirectory(tool)
  add_subdirectory(test)

  # Everything we build. The intermediate static libraries belong here as much
  # as the final artifacts: they include headers from the third-party tree and
  # from protoc, and a parallel build will start compiling them before either
  # exists unless told not to. ${DRIVER_BUNDLE_NAME}, not ${DRIVER_BUNDLE} —
  # the latter is not set anywhere, so it silently expanded to nothing.
  set(OUR_TARGETS
    ${COMMON_LIB}
    ${RPC_LIB}
    ${DRIVER_LIB}
    ${DRIVER_BUNDLE_NAME}
    ${TOOL_LIB}
    ${TOOL_EXE}
    ${TEST_EXE}
  )

  foreach(TARGET IN ITEMS ${OUR_TARGETS})
    add_dependencies(${TARGET}
      ${ALL_DEPENDENCIES}
    )
  endforeach()

  # ${RPC_LIB} is the target that runs protoc, and the headers it generates go
  # into the source tree. Only the final executables link it, so the static
  # libraries have to be told to wait: otherwise they compile against the
  # previous run's headers, and every field the proto just gained comes back as
  # "no member named ...".
  foreach(TARGET IN ITEMS ${COMMON_LIB} ${DRIVER_LIB} ${TOOL_LIB})
    add_dependencies(${TARGET} ${RPC_LIB})
  endforeach()

  # roc is linked by bare name through link_directories, so CMake has no
  # file-level dependency on libroc.a and would not relink when roc-toolkit is
  # rebuilt. Header changes happen to force a recompile, but a change confined to
  # roc's .cpp files does not, and the binaries then keep the previous library
  # without any sign of it. LINK_DEPENDS adds the missing dependency and leaves
  # the link line alone.
  foreach(TARGET IN ITEMS ${DRIVER_BUNDLE_NAME} ${TOOL_EXE} ${TEST_EXE})
    set_property(TARGET ${TARGET} APPEND PROPERTY
      LINK_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/roc/lib/libroc.a)
  endforeach()

  add_custom_command(
    COMMENT "Copying compile_commands.json to project root"
    DEPENDS
      ${OUR_TARGETS}
    OUTPUT
      ${PROJECT_SOURCE_DIR}/compile_commands.json
    COMMAND ${CMAKE_COMMAND} -E copy
      ${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json
      ${PROJECT_SOURCE_DIR}/compile_commands.json
    )
  add_custom_target(compile_commands ALL
    DEPENDS ${PROJECT_SOURCE_DIR}/compile_commands.json
  )
endif()
