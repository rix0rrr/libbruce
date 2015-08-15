project(libbruce VERSION 0.1 LANGUAGES CXX)

#----------------------------------------------------------------------

add_library(bruce
    src/bruce.cpp
    src/be/mem.cpp
    src/tree.cpp
    src/mutation.cpp
    src/memory.cpp
    src/operations.cpp
    src/serializing.cpp)

target_include_directories(bruce
    INTERFACE include
    PUBLIC include ${Boost_INCLUDE_DIR}
    PRIVATE src)

install(TARGETS bruce
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

#----------------------------------------------------------------------

add_executable(testbruce
    test/testserializing.cpp
    test/testbruce.cpp)
target_include_directories(testbruce
    PRIVATE src  # Testing private headers
    )

target_link_libraries(testbruce
    bruce)

add_test(testbruce testbruce)
