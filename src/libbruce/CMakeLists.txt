project(libbruce VERSION 0.1 LANGUAGES CXX)

#----------------------------------------------------------------------

enable_language(ASM)
#set(can_use_assembler TRUE)

add_library(bruce 
    src/bruce.cpp
    src/be/mem.cpp
    src/be/disk.cpp
    src/mutation.cpp
    src/memslice.cpp
    src/mempool.cpp
    src/serializing.cpp
    src/tree_impl.cpp
    src/edit_tree.cpp
    src/edit_tree_impl.cpp
    src/query_tree.cpp
    src/query_tree_impl.cpp
    src/query_iterator.cpp
    src/query_iterator_impl.cpp
    src/nodes.cpp
    src/types.cpp
    src/sha1-fast-64.S
    src/util/blockcache.cpp)

target_include_directories(bruce PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

install(TARGETS bruce
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
export(TARGETS bruce FILE LibBruceConfig.cmake)
target_link_libraries(bruce boost)

#----------------------------------------------------------------------

add_executable(testbruce
    test/testserializing.cpp
    test/testedit_tree.cpp
    test/testedit_tree_impl.cpp
    test/testhelpers.cpp
    test/testnodes.cpp
    test/testquery_tree.cpp
    test/testquery_iterator.cpp
    test/testtypes.cpp
    test/testbruce.cpp)

target_include_directories(testbruce
    PRIVATE src  # Testing private headers
    )

target_link_libraries(testbruce
    bruce)

add_test(testbruce testbruce)

include(CMakeOptions.txt)
