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
include(CTest)

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
