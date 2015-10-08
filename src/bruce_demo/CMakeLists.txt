add_executable(bruce_demo
        src/bruce_demo.cpp)

target_link_libraries(bruce_demo
        bruce awsbruce)

set_property(TARGET bruce_demo PROPERTY CXX_STANDARD 11)
