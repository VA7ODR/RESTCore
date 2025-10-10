cmake_minimum_required(VERSION 3.16)

# Advanced example that uses the openapi_codegen tool to generate JSON-backed
# model facades, and demonstrates a server and a client using RESTCore.
#
# API name: GeneratedExample (fixed via --name to keep include paths stable)
# Generated headers are placed under the binary dir.

set(API_NAME GeneratedExample)
set(GEN_OUT_DIR ${CMAKE_BINARY_DIR}/examples/openapi_generated/generated)
set(GEN_INCLUDE_DIR ${GEN_OUT_DIR}/include)
set(GEN_NS_DIR ${GEN_INCLUDE_DIR}/RESTCore_${API_NAME})

# Generate headers at build time
add_custom_command(
    OUTPUT 
        ${GEN_NS_DIR}/json_backend.hpp
        ${GEN_NS_DIR}/Client.hpp
        ${GEN_NS_DIR}/Server.hpp
    COMMAND $<TARGET_FILE:openapi_codegen>
            --output ${GEN_OUT_DIR}
            --input ${CMAKE_CURRENT_SOURCE_DIR}/openapi.yaml
            --name ${API_NAME}
    DEPENDS openapi_codegen ${CMAKE_CURRENT_SOURCE_DIR}/openapi.yaml
    COMMENT "Running openapi_codegen for ${API_NAME}"
    VERBATIM)

add_custom_target(openapi_generated_codegen
    DEPENDS 
        ${GEN_NS_DIR}/json_backend.hpp
        ${GEN_NS_DIR}/Client.hpp
        ${GEN_NS_DIR}/Server.hpp)

# Server executable
add_executable(openapi_generated_server
    openapi_generated_server.cpp)
set_property(TARGET openapi_generated_server PROPERTY CXX_STANDARD 20)
set_property(TARGET openapi_generated_server PROPERTY CXX_STANDARD_REQUIRED ON)
add_dependencies(openapi_generated_server openapi_generated_codegen)

target_include_directories(openapi_generated_server PRIVATE
    ${GEN_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party) # provides minimal nlohmann/json.hpp

target_link_libraries(openapi_generated_server PRIVATE RESTCore)

# Client executable
add_executable(openapi_generated_client
    openapi_generated_client.cpp)
set_property(TARGET openapi_generated_client PROPERTY CXX_STANDARD 20)
set_property(TARGET openapi_generated_client PROPERTY CXX_STANDARD_REQUIRED ON)
add_dependencies(openapi_generated_client openapi_generated_codegen)

target_include_directories(openapi_generated_client PRIVATE
    ${GEN_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

target_link_libraries(openapi_generated_client PRIVATE RESTCore)

# Make the OpenAPI spec visible
add_custom_target(openapi_generated_spec ALL
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/openapi.yaml)
