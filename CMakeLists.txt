cmake_minimum_required(VERSION 3.16)
project(RESTCore LANGUAGES CXX)

# Prefer static Boost libraries
set(Boost_USE_STATIC_LIBS ON)
# Do not force static runtime; keep system default to avoid link issues
# set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)


# Core library target holding reusable HTTP client/server code
add_library(RESTCore
    src/Client.cpp
    src/Server.cpp
)

# Public headers live under include/; expose to dependents
target_include_directories(RESTCore
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# Require C++20 for consumers without setting a global standard
target_compile_features(RESTCore PUBLIC cxx_std_20)

# Reasonable warnings (non-fatal)
if(MSVC)
    target_compile_options(RESTCore PRIVATE /W4 /permissive-)
else()
    target_compile_options(RESTCore PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Link Boost, pthreads, and OpenSSL at the library level so executables inherit it
if(TARGET Boost::system)
    target_link_libraries(RESTCore
        PUBLIC
            Boost::system Boost::thread Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
else()
    # Fallback for older CMake/Boost without imported targets
    target_include_directories(RESTCore PUBLIC ${Boost_INCLUDE_DIRS})
    target_link_libraries(RESTCore PUBLIC ${Boost_LIBRARIES} Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
endif()


# Enable CTest integration
# We only need enable_testing() for add_test; avoid legacy dashboard targets.
enable_testing()

# --- HTTP smoke tests using Boost.Test (single-header) ---
add_executable(http_smoke_test
    tests/http_smoke_test.cpp
)

target_link_libraries(http_smoke_test PRIVATE RESTCore)

# Register with CTest
add_test(NAME http_smoke_test COMMAND http_smoke_test --catch_system_errors=no)

# --- Doxygen docs target (optional) ---
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_SOURCE_DIR}/Doxyfile)
    if(EXISTS ${DOXYGEN_IN})
        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_IN}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM)
    endif()
endif()


# --- OpenAPI code generator tool (MVP scaffold) ---
add_executable(openapi_codegen
    tools/openapi_codegen.cpp
)
# Require C++20 for the tool as well
set_property(TARGET openapi_codegen PROPERTY CXX_STANDARD 20)
set_property(TARGET openapi_codegen PROPERTY CXX_STANDARD_REQUIRED ON)

# --- Optional: OpenAPI Echo example apps (server/client) ---
option(RESTCORE_BUILD_OPENAPI_ECHO_EXAMPLE "Build the OpenAPI 'echo/motd' example server and client" OFF)
if(RESTCORE_BUILD_OPENAPI_ECHO_EXAMPLE)
    add_subdirectory(examples/openapi_echo)
endif()

# --- Optional: Generated OpenAPI JSON example (uses openapi_codegen) ---
option(RESTCORE_BUILD_OPENAPI_GENERATED_EXAMPLE "Build the generated OpenAPI JSON example (runs openapi_codegen)" OFF)
if(RESTCORE_BUILD_OPENAPI_GENERATED_EXAMPLE)
    add_subdirectory(examples/openapi_generated)
endif()

# --- Tests for the OpenAPI code generator ---
# This CTest runs the generator on a tiny sample spec and validates the output files.
add_test(NAME openapi_codegen_test
    COMMAND /bin/bash ${CMAKE_SOURCE_DIR}/tests/openapi_codegen_test.sh
            $<TARGET_FILE:openapi_codegen>
            ${CMAKE_SOURCE_DIR}/tests/data/petstore.json
            ${CMAKE_BINARY_DIR}/generated_openapi_test)
