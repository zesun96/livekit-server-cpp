cmake_minimum_required(VERSION 3.24)

project(livekit-server-consumer LANGUAGES CXX)

find_package(LiveKitServer CONFIG REQUIRED)

add_executable(livekit_server_consumer main.cpp)
target_compile_features(livekit_server_consumer PRIVATE cxx_std_20)
target_link_libraries(livekit_server_consumer PRIVATE LiveKitServer::livekitserver)
