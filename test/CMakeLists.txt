
add_executable(test test.c)
set_property(TARGET test PROPERTY CXX_STANDARD 20)

target_link_libraries(test ${CJSON_LIBRARIES} ${TORCH_LIBRARIES})
target_link_libraries(test arga util pola)
target_sources(test PRIVATE
        pola/test_ast.c
        arga/test_filter.c
        arga/test_graph.c
        util/test_io.c
        util/test_mem.c
        arga/test_transform.c
        pola/test_interpreter.cpp
)

target_include_directories(test PRIVATE ../src)
target_include_directories(test PRIVATE ${CJSON_INCLUDE_DIRS})

