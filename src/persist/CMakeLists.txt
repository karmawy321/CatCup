add_library(persist STATIC
    src/Json.cpp
    src/Migrations.cpp
    src/AtomicFile.cpp
    src/ProjectSerializer.cpp
)

target_include_directories(persist PUBLIC include)
target_link_libraries(persist PUBLIC core)

if(MSVC)
    target_compile_options(persist PRIVATE /W4 /WX)
endif()
