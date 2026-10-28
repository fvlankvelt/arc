
add_executable(test test.c)
set_property(TARGET test PROPERTY CXX_STANDARD 20)

target_link_libraries(test "${TORCH_LIBRARIES}")
target_link_libraries(test ${CJSON_LIBRARIES})
target_link_libraries(test arclib)
target_sources(test PRIVATE
        test_ast.c
        test_filter.c
        test_graph.c
        test_io.c
        test_mem.c
        test_transform.c
)

target_include_directories(test PRIVATE ../src)
target_include_directories(test PRIVATE ${CJSON_INCLUDE_DIRS})

