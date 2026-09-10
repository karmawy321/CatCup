add_library(core STATIC
    src/Rational.cpp
    src/Ids.cpp
    src/Model.cpp
    src/ChangeBus.cpp
)

target_include_directories(core PUBLIC include)

# Strict warnings for first-party code.
if(MSVC)
    target_compile_options(core PRIVATE /W4 /WX)
endif()
