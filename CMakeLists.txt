cmake_minimum_required(VERSION 3.16)
project(untitled5)

set(CMAKE_CXX_STANDARD 20)

# Prefer static Boost libraries
set(Boost_USE_STATIC_LIBS ON)
# Do not force static runtime; keep system default to avoid link issues
# set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)

add_executable(untitled5 main.cpp)

# Link Boost and pthreads
if(TARGET Boost::system)
    target_link_libraries(untitled5 PRIVATE Boost::system Boost::thread Threads::Threads)
else()
    # Fallback for older CMake/Boost without imported targets
    target_include_directories(untitled5 PRIVATE ${Boost_INCLUDE_DIRS})
    target_link_libraries(untitled5 PRIVATE ${Boost_LIBRARIES} Threads::Threads)
endif()
