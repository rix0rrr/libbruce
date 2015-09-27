project(libbruce VERSION 0.1 LANGUAGES CXX)

#----------------------------------------------------------------------

find_package(zlib)

#----------------------------------------------------------------------

add_library(bruce
    src/bruce.cpp
    src/be/mem.cpp
    src/mutation.cpp
    src/memory.cpp
    src/serializing.cpp
    src/tree_impl.cpp
    src/edit_tree.cpp
    src/edit_tree_impl.cpp
    src/query_tree.cpp
    src/query_tree_impl.cpp
    src/query_iterator.cpp
    src/query_iterator_impl.cpp
    src/nodes.cpp)

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
    test/testedit_tree.cpp
    test/testedit_tree_impl.cpp
    test/testhelpers.cpp
    test/testnodes.cpp
    test/testquery_tree.cpp
    test/testquery_iterator.cpp
    test/testbruce.cpp)

target_include_directories(testbruce
    PRIVATE src  # Testing private headers
    )

target_link_libraries(testbruce
    bruce)

add_test(testbruce testbruce)

include(CMakeOptions.txt)

