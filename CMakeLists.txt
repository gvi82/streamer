cmake_minimum_required(VERSION 3.1)

project(streamer)

#[[
include(CheckCXXCompilerFlag)

function(enable_cxx_compiler_flag_if_supported flag)
    string(FIND "${CMAKE_CXX_FLAGS}" "${flag}" flag_already_set)
    if(flag_already_set EQUAL -1)
        check_cxx_compiler_flag("${flag}" flag_supported)
        if(flag_supported)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}" PARENT_SCOPE)
        endif()
        unset(flag_supported CACHE)
    endif()
endfunction()

enable_cxx_compiler_flag_if_supported("-Wall")
enable_cxx_compiler_flag_if_supported("-Wextra")
enable_cxx_compiler_flag_if_supported("-pedantic")

enable_cxx_compiler_flag_if_supported("-Wcast-align")
enable_cxx_compiler_flag_if_supported("-Wcast-qual")
enable_cxx_compiler_flag_if_supported("-Wconversion")
enable_cxx_compiler_flag_if_supported("-Wctor-dtor-privacy")
enable_cxx_compiler_flag_if_supported("-Wenum-compare")
enable_cxx_compiler_flag_if_supported("-Wfloat-equal")
enable_cxx_compiler_flag_if_supported("-Wnon-virtual-dtor")
enable_cxx_compiler_flag_if_supported("-Wold-style-cast")
enable_cxx_compiler_flag_if_supported("-Woverloaded-virtual")
enable_cxx_compiler_flag_if_supported("-Wredundant-decls")
enable_cxx_compiler_flag_if_supported("-Wsign-conversion")
enable_cxx_compiler_flag_if_supported("-Wsign-promo")
]]

add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(tests)
