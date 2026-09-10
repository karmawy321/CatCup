add_library(media STATIC
    src/StubProber.cpp
)

target_include_directories(media PUBLIC include)
target_link_libraries(media PUBLIC core)

if(MSVC)
    target_compile_options(media PRIVATE /W4 /WX)
endif()

# Real FFmpeg backend lives in src/media/ffmpeg/ and is gated by ENABLE_FFMPEG.
if(ENABLE_FFMPEG)
    add_subdirectory(ffmpeg)
endif()
