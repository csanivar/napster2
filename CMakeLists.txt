cmake_minimum_required(VERSION 3.3)
project(napster2)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gcc")

set(SOURCE_FILES main.c)
add_executable(napster2 ${SOURCE_FILES})