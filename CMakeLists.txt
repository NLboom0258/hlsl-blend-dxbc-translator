cmake_minimum_required(VERSION 3.15)
project(hlsl_blend_dxbc_translator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

add_executable(hlsl_blend_dxbc_translator ${SOURCES} ${HEADERS})

if(MSVC)
    target_compile_definitions(hlsl_blend_dxbc_translator PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
