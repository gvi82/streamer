cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 17)

project(common)

add_library(common common.cpp
                   appimpl.cpp)

find_package(Boost COMPONENTS REQUIRED)
target_include_directories(common PRIVATE ${Boost_INCLUDE_DIRS})

execute_process(
  COMMAND git log -1 --format=%h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_definitions(-DAPP_VERSION="${GIT_COMMIT_HASH}")

target_link_libraries(common PRIVATE ${Boost_LIBRARIES} dl)
