cmake_minimum_required(VERSION 2.8)


set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(server)

#include_directories(../common)

find_package(Boost COMPONENTS filesystem system thread REQUIRED)
link_directories(${Boost_LIBRARY_DIRS})

set(source_list src/server_app.cpp
                src/server.cpp
                src/stream_svc.cpp
                src/receiver.cpp)

add_library(serverl ${source_list})

target_link_libraries(serverl PRIVATE common)

target_link_libraries(serverl PRIVATE ${Boost_LIBRARIES})
target_include_directories(serverl PRIVATE  ${Boost_INCLUDE_DIR} src ..)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG  REQUIRED  libavformat libavcodec libavutil libswscale)

target_link_libraries(serverl PRIVATE ${FFMPEG_LDFLAGS})

add_executable(server src/server.cpp)

target_link_libraries(server PRIVATE serverl)
