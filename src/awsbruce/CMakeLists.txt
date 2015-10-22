# DynamoDB and S3 backends for libbruce
project(awsbruce VERSION 0.1 LANGUAGES CXX)

#----------------------------------------------------------------------

add_library(awsbruce 
    src/s3be.cpp)

target_include_directories(awsbruce PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(awsbruce
    aws-cpp-sdk-s3
    boost-iostreams
    libcrypto
    bruce)

# We need C++11 because of the AWS cpp SDK
set_property(TARGET awsbruce PROPERTY CXX_STANDARD 11)
set_target_properties(awsbruce PROPERTIES CXX_EXTENSIONS OFF)

install(TARGETS awsbruce
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
export(TARGETS awsbruce FILE AwsBruceConfig.cmake)

#----------------------------------------------------------------------

#add_executable(testawsbruce
    #)

#target_include_directories(testbruce
    #PRIVATE src  # Testing private headers
    #)

#target_link_libraries(testbruce
    #bruce)

#add_test(testbruce testbruce)

#----------------------------------------------------------------------
