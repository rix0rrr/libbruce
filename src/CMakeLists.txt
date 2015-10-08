cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(bruce VERSION 0.1 LANGUAGES CXX)

# Global include for tests
include_directories(libs/catch/include)

#----------------------------------------------------------------------

find_package(Boost REQUIRED iostreams)

add_library(boost INTERFACE IMPORTED)
set_property(TARGET boost PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})

add_library(boost-iostreams SHARED IMPORTED)
set_property(TARGET boost-iostreams PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})
set_property(TARGET boost-iostreams PROPERTY IMPORTED_LOCATION ${Boost_IOSTREAMS_LIBRARY})

#----------------------------------------------------------------------

find_package(AwsCppSdk)
if (AwsCppSdk_FOUND)
    # The targets exported by the CMake file don't contain headers yet
    set_property(TARGET aws-cpp-sdk-core PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${AwsCppSdk_DIR}/../aws-cpp-sdk-core/include)
    set_property(TARGET aws-cpp-sdk-s3 PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${AwsCppSdk_DIR}/../aws-cpp-sdk-s3/include)
endif()

#----------------------------------------------------------------------

enable_testing()
add_subdirectory(libbruce)
if (AwsCppSdk_FOUND)
    add_subdirectory(awsbruce)
endif()
add_subdirectory(bruce_demo)
