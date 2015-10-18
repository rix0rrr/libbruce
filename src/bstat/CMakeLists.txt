project(bstat VERSION 0.1 LANGUAGES CXX)

add_executable(bstat
        bstat.cpp
        visitor.cpp
        stats.cpp)

target_link_libraries(bstat
        boost-iostreams
        bruce)
