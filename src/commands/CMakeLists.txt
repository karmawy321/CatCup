add_library(commands STATIC
    src/UndoStack.cpp
    src/ClipCommands.cpp
    src/TransitionCommands.cpp
    src/EffectCommands.cpp
    src/SmartCommands.cpp
)

target_include_directories(commands PUBLIC include)
target_link_libraries(commands PUBLIC core ai)

if(MSVC)
    target_compile_options(commands PRIVATE /W4 /WX)
endif()
