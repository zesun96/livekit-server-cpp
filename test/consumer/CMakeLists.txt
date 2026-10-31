cmake_minimum_required(VERSION 3.24)

project(livekit-server-consumer LANGUAGES C CXX)

find_package(LiveKitServer CONFIG REQUIRED)

add_executable(livekit_server_consumer main.cpp)
target_compile_features(livekit_server_consumer PRIVATE cxx_std_20)
target_link_libraries(livekit_server_consumer PRIVATE LiveKitServer::livekitserver)

add_executable(livekit_server_c_consumer c_api.c)
set_target_properties(livekit_server_c_consumer PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(livekit_server_c_consumer PRIVATE LiveKitServer::livekitserver)
