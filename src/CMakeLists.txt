
add_library(arclib
        ast.c
        binding.c
        filter.c
        guide.c
        image.c
        io.c
        mtwister.c
        nnet.cpp
        task.c
        transform.c
)
target_link_libraries(arclib "${TORCH_LIBRARIES}")
target_link_libraries(arclib "${TORCH_LIBRARIES}")
target_link_libraries(arclib ${CJSON_LIBRARIES})
set_property(TARGET arclib PROPERTY CXX_STANDARD 20)

add_executable(arc main.c)
set_property(TARGET arc PROPERTY CXX_STANDARD 20)

target_link_libraries(arc arclib)

target_include_directories(arc PRIVATE ${CJSON_INCLUDE_DIRS})
