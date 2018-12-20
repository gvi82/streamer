cmake_minimum_required(VERSION 2.8)


set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(client)

find_package(Boost COMPONENTS filesystem system thread REQUIRED)
link_directories(${Boost_LIBRARY_DIRS})

set(source_list src/client_app.cpp
                src/client.cpp
                src/sender.cpp)

add_library(clientl ${source_list})

target_link_libraries(clientl PRIVATE common)

target_link_libraries(clientl PRIVATE ${Boost_LIBRARIES})
target_include_directories(clientl PRIVATE  ${Boost_INCLUDE_DIR} src ..)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG  REQUIRED  libavformat libavcodec libavutil libswscale)

target_link_libraries(clientl PRIVATE ${FFMPEG_LDFLAGS})

add_executable(client src/client.cpp)

target_link_libraries(client PRIVATE clientl boost_program_options)

add_custom_command(
    TARGET client PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/tests/test_video.mp4 ${CMAKE_CURRENT_BINARY_DIR}/test_video.mp4
    )
