add_library(effects STATIC
    src/EffectRegistry.cpp
)

target_include_directories(effects PUBLIC include)
target_link_libraries(effects PUBLIC core)

if(MSVC)
    target_compile_options(effects PRIVATE /W4 /WX)
endif()
