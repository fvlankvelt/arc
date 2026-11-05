
add_library(arga
        arga/arga_task.c
        arga/binding.c
        arga/filter.c
        arga/guide.c
        arga/image.c
        arga/transform.c
)
add_library(util
        util/io.c
        util/mtwister.c
        util/nnet.cpp
        util/task.c
)
add_library(pola
        pola/ast.cpp
)
target_link_libraries(util ${CJSON_LIBRARIES} ${TORCH_LIBRARIES})
set_property(TARGET util PROPERTY CXX_STANDARD 20)
set_property(TARGET pola PROPERTY CXX_STANDARD 20)

add_executable(arc arga/main.c)
set_property(TARGET arc PROPERTY CXX_STANDARD 20)

target_link_libraries(arc arga util)

target_include_directories(arc PRIVATE ${CJSON_INCLUDE_DIRS})
