add_library(ai STATIC
    src/JobSystem.cpp
)

target_include_directories(ai PUBLIC include)
target_link_libraries(ai PUBLIC core)

if(MSVC)
    target_compile_options(ai PRIVATE /W4 /WX)
endif()
