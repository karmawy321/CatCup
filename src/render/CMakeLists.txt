add_library(render STATIC
    src/Evaluator.cpp
)

target_include_directories(render PUBLIC include)
target_link_libraries(render PUBLIC core)

if(MSVC)
    target_compile_options(render PRIVATE /W4 /WX)
endif()
