project(bkv VERSION 0.1 LANGUAGES CXX)

option(PERFTEST "Use disk and don't delete pages")

add_executable(bkv
        src/bkv.cpp)

target_link_libraries(bkv bruce)
if(TARGET awsbruce)
    target_link_libraries(bkv awsbruce)
endif()

if(PERFTEST)
    target_compile_definitions(bkv PRIVATE -DPERFTEST)
endif()

set_property(TARGET bkv PROPERTY CXX_STANDARD 11)
set_target_properties(bkv PROPERTIES CXX_EXTENSIONS OFF)
