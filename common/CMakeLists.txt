configure_file(
  build_info.cpp.in
  build_info.cpp
)

add_library(${COMMON_LIB} STATIC
  ${CMAKE_CURRENT_BINARY_DIR}/build_info.cpp
  enum_map.cpp
  plist_info.mm
)

# enum_map.cpp includes the protobuf headers, which protoc writes into the
# source tree while the build runs. Unlike the driver and tool libraries this
# one does not link ${RPC_LIB}, so nothing ordered it after the generation step
# and a parallel build could compile it against the previous run's headers.
add_dependencies(${COMMON_LIB} ${RPC_LIB})
