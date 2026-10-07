cmake_minimum_required(VERSION 3.24)

project(livekit-server-protobuf-consumer LANGUAGES CXX)

find_package(LiveKitServer CONFIG REQUIRED)

if(NOT TARGET LiveKitServer::protobuf_adapter)
  message(FATAL_ERROR "The installed package does not provide the protobuf adapter")
endif()

add_executable(livekit_server_protobuf_consumer main.cpp)
target_compile_features(livekit_server_protobuf_consumer PRIVATE cxx_std_20)
target_link_libraries(livekit_server_protobuf_consumer PRIVATE LiveKitServer::protobuf_adapter)
