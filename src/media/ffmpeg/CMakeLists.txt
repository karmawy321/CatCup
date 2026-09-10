add_library(media_ffmpeg STATIC
    src/FfmpegProber.cpp
    src/FfmpegTime.cpp
    src/VideoDecoder.cpp
    src/AudioDecoder.cpp
    src/FfmpegThumbnailer.cpp
)

target_include_directories(media_ffmpeg PUBLIC include)
target_link_libraries(media_ffmpeg
    PUBLIC
        core
        media
    PRIVATE
        FFmpeg::avcodec
        FFmpeg::avformat
        FFmpeg::avutil
        FFmpeg::swscale
        FFmpeg::swresample
)

if(MSVC)
    target_compile_options(media_ffmpeg PRIVATE /W4 /WX)
    # FFmpeg's C headers are warning-noisy under /W4; keep our code strict
    # but don't fail the build on third-party header noise.
    target_compile_options(media_ffmpeg PRIVATE /experimental:external /external:W0)
    set_target_properties(media_ffmpeg PROPERTIES VS_GLOBAL_ExternalIncludePath "$(FFMPEG_ROOT)/include")
endif()
