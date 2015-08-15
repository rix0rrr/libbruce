cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(bruce VERSION 0.1 LANGUAGES CXX)

# Global include for tests
include_directories(libs/catch/include)

find_package(Boost REQUIRED)

enable_testing()
add_subdirectory(libbruce)
add_subdirectory(bruce_demo)
