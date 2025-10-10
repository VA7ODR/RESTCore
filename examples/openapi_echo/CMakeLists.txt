cmake_minimum_required(VERSION 3.16)

# This subdirectory builds two tiny example apps demonstrating a trivial
# OpenAPI-defined service using the RESTCore library:
# - openapi_echo_server: serves GET /motd and POST /echo (text/plain)
# - openapi_echo_client: CLI to call the above endpoints

# Server
add_executable(openapi_echo_server
    openapi_echo_server.cpp
)
set_property(TARGET openapi_echo_server PROPERTY CXX_STANDARD 20)
set_property(TARGET openapi_echo_server PROPERTY CXX_STANDARD_REQUIRED ON)

target_link_libraries(openapi_echo_server PRIVATE RESTCore)

# Client
add_executable(openapi_echo_client
    openapi_echo_client.cpp
)
set_property(TARGET openapi_echo_client PROPERTY CXX_STANDARD 20)
set_property(TARGET openapi_echo_client PROPERTY CXX_STANDARD_REQUIRED ON)

target_link_libraries(openapi_echo_client PRIVATE RESTCore)

# Make the OpenAPI spec visible in IDEs and available for packaging if desired
add_custom_target(openapi_echo_spec ALL
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/openapi.yaml)
