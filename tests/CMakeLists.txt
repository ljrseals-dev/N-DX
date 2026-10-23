cmake_minimum_required(VERSION 3.15)
project(ndx_dsp_test CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ndx_dsp_test dsp_test.cpp)
target_include_directories(ndx_dsp_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
if(MSVC)
    target_compile_definitions(ndx_dsp_test PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

enable_testing()
add_test(NAME ndx_dsp COMMAND ndx_dsp_test)
