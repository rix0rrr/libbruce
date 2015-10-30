project(libbruce VERSION 0.1 LANGUAGES CXX)

#----------------------------------------------------------------------

enable_language(ASM)
#set(can_use_assembler TRUE)

add_library(bruce 
    src/be/disk.cpp
    src/be/mem.cpp
    src/bruce.cpp
    src/internal_node.cpp
    src/leaf_node.cpp
    src/mempool.cpp
    src/memslice.cpp
    src/mutation.cpp
    src/nodes.cpp
    src/overflow_node.cpp
    src/query_iterator.cpp
    src/query_iterator_impl.cpp
    src/serializing.cpp
    src/sha1-fast-64.S
    src/tree.cpp
    src/tree_impl.cpp
    src/types.cpp
    src/util/blockcache.cpp
    )

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
target_compile_options(bruce PRIVATE -march=native)

#----------------------------------------------------------------------

add_executable(testbruce
    test/test_editing.cpp
    test/test_iterator.cpp
    test/test_querying.cpp
    test/test_queueing.cpp
    test/testbruce.cpp
    test/testhelpers.cpp
    test/testleaf_node.cpp
    test/testnodes.cpp
    test/testserializing.cpp
    test/testtypes.cpp
    )

target_include_directories(testbruce
    PRIVATE src  # Testing private headers
    )

target_link_libraries(testbruce
    bruce)

add_test(testbruce testbruce)

include(CMakeOptions.txt)
