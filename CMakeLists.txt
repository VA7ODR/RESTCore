cmake_minimum_required(VERSION 3.16)
project(untitled5)

set(CMAKE_CXX_STANDARD 20)

# Prefer static Boost libraries
set(Boost_USE_STATIC_LIBS ON)
# Do not force static runtime; keep system default to avoid link issues
# set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

add_subdirectory(json)

add_executable(untitled5
    main.cpp
    HTTPClient.cpp
    HTTPServerHost.cpp
)

# Link Boost, pthreads, and OpenSSL
if(TARGET Boost::system)
    target_link_libraries(untitled5 PRIVATE Boost::system Boost::thread Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
else()
    # Fallback for older CMake/Boost without imported targets
    target_include_directories(untitled5 PRIVATE ${Boost_INCLUDE_DIRS})
    target_link_libraries(untitled5 PRIVATE ${Boost_LIBRARIES} Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
endif()


# Enable CTest integration
include(CTest)

# --- HTTP smoke tests using Boost.Test (single-header) ---
add_executable(http_smoke_test
    tests/http_smoke_test.cpp
    HTTPClient.cpp
    HTTPServerHost.cpp
)

# Ensure the tests can include project headers
target_include_directories(http_smoke_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# Link Boost, pthreads, and OpenSSL similar to main target
if(TARGET Boost::system)
    target_link_libraries(http_smoke_test PRIVATE Boost::system Boost::thread Threads::Threads OpenSSL::SSL OpenSSL::Crypto
        ajson adata ojson odata json_document tinyxml)
else()
    target_include_directories(http_smoke_test PRIVATE ${Boost_INCLUDE_DIRS})
    target_link_libraries(http_smoke_test PRIVATE ${Boost_LIBRARIES} Threads::Threads OpenSSL::SSL OpenSSL::Crypto
        ajson adata ojson odata json_document tinyxml)
endif()

# Register with CTest
add_test(NAME http_smoke_test COMMAND http_smoke_test)
