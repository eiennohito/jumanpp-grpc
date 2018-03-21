get_target_property(JPP_PROTOBUF_DIRS jpp_jumandic PROTOBUF_DIRS)
set(PROTOBUF_IMPORT_DIRS ${JPP_PROTOBUF_DIRS})
PROTOBUF_GENERATE_CPP(jpp_pb_srcs jpp_pb_hdrs jumandic-svc.proto)
PROTOBUF_GENERATE_GRPC_CPP(jpp_grpc_srcs jpp_grpc_hdrs jumandic-svc.proto)

list( APPEND jpp_grpc_srcs
  analyzer_cache.cc
  analyzer_cache.h
  launcher.cc
  stream_call.h interfaces.h unary_call.cc unary_call.h service_env.cc service_env.h calls_impl.cc calls_impl.h)

add_executable(jumanpp-jumandic-grpc ${jpp_grpc_srcs} ${jpp_grpc_hdrs} ${jpp_pb_srcs} ${jpp_pb_hdrs})
target_include_directories(jumanpp-jumandic-grpc PRIVATE ${GRPC_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(jumanpp-jumandic-grpc ${GRPC_LIBRARIES} jpp_jumandic)