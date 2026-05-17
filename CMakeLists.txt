cmake_minimum_required(VERSION 3.20)
project(opensamp_core CXX)

set(CMAKE_CXX_STANDARD 17)

# SLikeNet (форк RakNet)
include(FetchContent)
FetchContent_Declare(
    slikenet
    GIT_REPOSITORY https://github.com/SLikeSoft/SLikeNet.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(slikenet)

add_library(opensamp_core STATIC
    src/net/client.cpp
    src/samp/packets.cpp
)

target_include_directories(opensamp_core PUBLIC
    include/
    ${slikenet_SOURCE_DIR}/Source/include
)

target_link_libraries(opensamp_core PUBLIC SLikeNetLibStatic)
