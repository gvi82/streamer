cmake_minimum_required(VERSION 2.6)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Link runTests with what we want to test and the GTest and pthread library
add_executable(runTests tests.cpp)
target_include_directories(runTests PRIVATE  ..)
target_link_libraries(runTests ${GTEST_LIBRARIES} clientl)

add_custom_command(
    TARGET runTests PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/tests/test_video.mp4 ${CMAKE_CURRENT_BINARY_DIR}/test_video.mp4
    )
