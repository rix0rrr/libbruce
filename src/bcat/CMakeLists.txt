project(bcat VERSION 0.1 LANGUAGES CXX)

add_executable(bcat
        bcat.cpp)

target_link_libraries(bcat
        boost-iostreams
        bruce)
