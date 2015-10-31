project(bstat VERSION 0.1 LANGUAGES CXX)

add_executable(bstat
        bstat.cpp
        stats.cpp
        visitor.cpp
        table.cpp
        )

target_link_libraries(bstat
        boost-iostreams
        bruce)

if(TARGET awsbruce)
    target_link_libraries(bstat awsbruce)
endif()
