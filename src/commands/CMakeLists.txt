add_library(commands STATIC
    src/UndoStack.cpp
    src/ClipCommands.cpp
)

target_include_directories(commands PUBLIC include)
target_link_libraries(commands PUBLIC core)

if(MSVC)
    target_compile_options(commands PRIVATE /W4 /WX)
endif()
