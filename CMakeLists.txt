cmake_minimum_required(VERSION 3.24)

project(
  livekit-server-cpp
  VERSION 0.1.0
  DESCRIPTION "C++20 SDK for LiveKit server APIs and access tokens"
  LANGUAGES CXX
)

include(FetchContent)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(LIVEKIT_SERVER_BUILD_TESTS "Build deterministic unit tests" ${PROJECT_IS_TOP_LEVEL})
option(LIVEKIT_SERVER_BUILD_INTEGRATION_TESTS "Build tests requiring a LiveKit server" OFF)
option(LIVEKIT_SERVER_BUILD_EXAMPLES "Build examples" ON)
option(LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER
  "Expose generated LiveKit protobuf headers through a compatibility target" ON)
set(LIVEKIT_PROTOCOL_ROOT "" CACHE PATH "Path to a livekit/protocol source tree")

find_package(Protobuf CONFIG REQUIRED)

if(LIVEKIT_PROTOCOL_ROOT)
  get_filename_component(LIVEKIT_PROTOCOL_ROOT "${LIVEKIT_PROTOCOL_ROOT}" ABSOLUTE)
  set(LIVEKIT_PROTOCOL_PROTO_DIR "${LIVEKIT_PROTOCOL_ROOT}/protobufs")
else()
  FetchContent_Declare(
    livekit_protocol
    URL https://github.com/livekit/protocol/archive/refs/tags/v1.50.4.tar.gz
    URL_HASH SHA256=a2aba0546975f51badd7f91f3cebb7dcc94df6801b9d359af6ee52b14705733b
  )
  FetchContent_MakeAvailable(livekit_protocol)
  set(LIVEKIT_PROTOCOL_PROTO_DIR "${livekit_protocol_SOURCE_DIR}/protobufs")
endif()

if(NOT EXISTS "${LIVEKIT_PROTOCOL_PROTO_DIR}/livekit_room.proto")
  message(FATAL_ERROR "LIVEKIT_PROTOCOL_ROOT does not contain protobufs/livekit_room.proto")
endif()

set(LIVEKIT_SERVER_PROTO_FILES
  "${LIVEKIT_PROTOCOL_PROTO_DIR}/livekit_agent.proto"
  "${LIVEKIT_PROTOCOL_PROTO_DIR}/livekit_agent_dispatch.proto"
  "${LIVEKIT_PROTOCOL_PROTO_DIR}/livekit_connector.proto"
  "${LIVEKIT_PROTOCOL_PROTO_DIR}/livekit_connector_twilio.proto"
  "${LIVEKIT_PROTOCOL_PROTO_DIR}/livekit_connector_whatsapp.proto"
  "${LIVEKIT_PROTOCOL_PROTO_DIR}/livekit_egress.proto"
  "${LIVEKIT_PROTOCOL_PROTO_DIR}/livekit_ingress.proto"
  "${LIVEKIT_PROTOCOL_PROTO_DIR}/livekit_metrics.proto"
  "${LIVEKIT_PROTOCOL_PROTO_DIR}/livekit_models.proto"
  "${LIVEKIT_PROTOCOL_PROTO_DIR}/livekit_room.proto"
  "${LIVEKIT_PROTOCOL_PROTO_DIR}/livekit_rtc.proto"
  "${LIVEKIT_PROTOCOL_PROTO_DIR}/livekit_sip.proto"
  "${LIVEKIT_PROTOCOL_PROTO_DIR}/livekit_webhook.proto"
  "${LIVEKIT_PROTOCOL_PROTO_DIR}/logger/options.proto"
)

set(LIVEKIT_SERVER_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${LIVEKIT_SERVER_GENERATED_DIR}")

add_library(livekit_server_proto OBJECT ${LIVEKIT_SERVER_PROTO_FILES})
set_target_properties(livekit_server_proto PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(livekit_server_proto PUBLIC protobuf::libprotobuf)
target_include_directories(livekit_server_proto PUBLIC "${LIVEKIT_SERVER_GENERATED_DIR}")
protobuf_generate(
  TARGET livekit_server_proto
  LANGUAGE cpp
  PROTOS ${LIVEKIT_SERVER_PROTO_FILES}
  APPEND_PATH
  IMPORT_DIRS "${LIVEKIT_PROTOCOL_PROTO_DIR}"
  PROTOC_OUT_DIR "${LIVEKIT_SERVER_GENERATED_DIR}"
)

add_library(
  livekitserver
  src/access_token.cpp
  src/agent_dispatch_client.cpp
  src/connector_client.cpp
  src/egress_client.cpp
  src/error.cpp
  src/http_transport.cpp
  src/ingress_client.cpp
  src/livekit_api.cpp
  src/room_service_client.cpp
  src/sip_client.cpp
  src/webhook_receiver.cpp
  src/detail/client_context.cpp
  src/detail/crypto.cpp
  $<TARGET_OBJECTS:livekit_server_proto>
)
add_library(LiveKitServer::livekitserver ALIAS livekitserver)
target_compile_features(livekitserver PUBLIC cxx_std_20)
target_include_directories(
  livekitserver
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    "${LIVEKIT_SERVER_GENERATED_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(livekitserver PRIVATE protobuf::libprotobuf)
target_compile_definitions(livekitserver PRIVATE LIVEKIT_SERVER_BUILDING_LIBRARY=1)

if(LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER)
  add_library(livekitserver_protobuf_adapter INTERFACE)
  add_library(LiveKitServer::protobuf_adapter ALIAS livekitserver_protobuf_adapter)
  set_target_properties(livekitserver_protobuf_adapter PROPERTIES EXPORT_NAME protobuf_adapter)
  target_include_directories(livekitserver_protobuf_adapter INTERFACE
    $<BUILD_INTERFACE:${LIVEKIT_SERVER_GENERATED_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/livekit/server/protobuf>)
  target_compile_definitions(livekitserver_protobuf_adapter INTERFACE
    LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER_API=1)
  target_link_libraries(livekitserver_protobuf_adapter INTERFACE
    LiveKitServer::livekitserver protobuf::libprotobuf)
endif()

if(WIN32)
  target_compile_definitions(livekitserver PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(livekitserver PRIVATE bcrypt winhttp)
endif()

set_target_properties(
  livekitserver
  PROPERTIES
    OUTPUT_NAME livekitserver
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(MSVC)
  target_compile_options(livekitserver PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(livekitserver PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(LIVEKIT_SERVER_BUILD_TESTS OR LIVEKIT_SERVER_BUILD_INTEGRATION_TESTS)
  enable_testing()
  include(cmake/Testing.cmake)
endif()

if(LIVEKIT_SERVER_BUILD_TESTS)
  file(GLOB_RECURSE LIVEKIT_SERVER_PUBLIC_HEADERS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/include"
    CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/livekit/server/*.h")
  set(LIVEKIT_SERVER_PUBLIC_HEADER_TEST_SOURCES test/unit/public_headers_test.cpp)
  foreach(public_header IN LISTS LIVEKIT_SERVER_PUBLIC_HEADERS)
    string(MAKE_C_IDENTIFIER "${public_header}" public_header_id)
    set(public_header_test_source
      "${CMAKE_CURRENT_BINARY_DIR}/public_header_tests/${public_header_id}.cpp")
    file(GENERATE OUTPUT "${public_header_test_source}"
      CONTENT "#include <${public_header}>\n")
    list(APPEND LIVEKIT_SERVER_PUBLIC_HEADER_TEST_SOURCES "${public_header_test_source}")
  endforeach()
  add_library(livekit_server_public_headers_test OBJECT
    ${LIVEKIT_SERVER_PUBLIC_HEADER_TEST_SOURCES})
  target_compile_features(livekit_server_public_headers_test PRIVATE cxx_std_20)
  target_include_directories(livekit_server_public_headers_test
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

  if(LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER)
    add_library(livekit_server_protobuf_adapter_headers_test OBJECT
      test/unit/protobuf_adapter_headers_test.cpp)
    target_compile_features(livekit_server_protobuf_adapter_headers_test PRIVATE cxx_std_20)
    target_link_libraries(livekit_server_protobuf_adapter_headers_test
      PRIVATE LiveKitServer::protobuf_adapter)
  endif()

  add_executable(livekit_server_unit_tests test/unit/server_sdk_test.cpp)
  target_include_directories(livekit_server_unit_tests PRIVATE "${LIVEKIT_SERVER_GENERATED_DIR}")
  target_compile_definitions(livekit_server_unit_tests
    PRIVATE LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER_API=1)
  target_link_libraries(
    livekit_server_unit_tests
    PRIVATE LiveKitServer::livekitserver GTest::gtest_main protobuf::libprotobuf
  )
  gtest_discover_tests(
    livekit_server_unit_tests
    TEST_PREFIX "unit."
    PROPERTIES LABELS "unit" TIMEOUT 10
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(livekit_server_webhook_tests test/unit/webhook_receiver_test.cpp)
  target_include_directories(livekit_server_webhook_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src" "${LIVEKIT_SERVER_GENERATED_DIR}")
  target_link_libraries(
    livekit_server_webhook_tests
    PRIVATE LiveKitServer::livekitserver GTest::gtest_main protobuf::libprotobuf
  )
  gtest_discover_tests(
    livekit_server_webhook_tests
    TEST_PREFIX "unit."
    PROPERTIES LABELS "unit" TIMEOUT 10
    DISCOVERY_MODE PRE_TEST
  )
endif()

if(LIVEKIT_SERVER_BUILD_INTEGRATION_TESTS)
  add_executable(livekit_server_integration_tests test/integration/room_service_test.cpp)
  target_include_directories(livekit_server_integration_tests
    PRIVATE "${LIVEKIT_SERVER_GENERATED_DIR}")
  target_compile_definitions(livekit_server_integration_tests
    PRIVATE LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER_API=1)
  target_link_libraries(
    livekit_server_integration_tests
    PRIVATE LiveKitServer::livekitserver GTest::gtest_main protobuf::libprotobuf
  )
  gtest_discover_tests(
    livekit_server_integration_tests
    TEST_PREFIX "integration."
    PROPERTIES LABELS "integration" TIMEOUT 30
    DISCOVERY_MODE PRE_TEST
  )

  if(WIN32)
    add_executable(livekit_server_webhook_integration_tests
      test/integration/webhook_callback_test.cpp)
    target_include_directories(livekit_server_webhook_integration_tests
      PRIVATE "${LIVEKIT_SERVER_GENERATED_DIR}")
    target_compile_definitions(livekit_server_webhook_integration_tests
      PRIVATE LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER_API=1)
    target_link_libraries(livekit_server_webhook_integration_tests
      PRIVATE LiveKitServer::livekitserver GTest::gtest_main protobuf::libprotobuf ws2_32)
    gtest_discover_tests(
      livekit_server_webhook_integration_tests
      TEST_PREFIX "integration."
      PROPERTIES LABELS "integration" TIMEOUT 30
      DISCOVERY_MODE PRE_TEST
    )
  endif()
endif()

if(LIVEKIT_SERVER_BUILD_EXAMPLES)
  add_executable(livekit_server_room_example examples/room_service.cpp)
  target_link_libraries(livekit_server_room_example PRIVATE LiveKitServer::livekitserver)
  add_executable(livekit_server_webhook_example examples/webhook_handler.cpp)
  target_link_libraries(livekit_server_webhook_example PRIVATE LiveKitServer::livekitserver)
endif()

install(
  TARGETS livekitserver
  EXPORT LiveKitServerTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
if(LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER)
  install(TARGETS livekitserver_protobuf_adapter EXPORT LiveKitServerTargets)
endif()
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
if(LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER)
  install(
    DIRECTORY "${LIVEKIT_SERVER_GENERATED_DIR}/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/livekit/server/protobuf"
    FILES_MATCHING PATTERN "*.pb.h"
  )
endif()
install(
  EXPORT LiveKitServerTargets
  FILE LiveKitServerTargets.cmake
  NAMESPACE LiveKitServer::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LiveKitServer"
)

configure_package_config_file(
  cmake/LiveKitServerConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/LiveKitServerConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LiveKitServer"
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/LiveKitServerConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/LiveKitServerConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/LiveKitServerConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LiveKitServer"
)
