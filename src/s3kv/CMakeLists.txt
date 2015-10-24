project(s3kv VERSION 0.1 LANGUAGES CXX)

option(PERFTEST "Use disk and don't delete pages")

add_executable(s3kv
        src/s3kv.cpp)

target_link_libraries(s3kv
        bruce
        awsbruce)

if(PERFTEST)
    target_compile_definitions(s3kv PRIVATE -DPERFTEST)
endif()

set_property(TARGET s3kv PROPERTY CXX_STANDARD 11)
set_target_properties(s3kv PROPERTIES CXX_EXTENSIONS OFF)
