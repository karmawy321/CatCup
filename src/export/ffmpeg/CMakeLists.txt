add_library(export_ffmpeg STATIC
    src/Mp4Exporter.cpp
)

target_include_directories(export_ffmpeg PUBLIC include)
target_link_libraries(export_ffmpeg
    PUBLIC
        core
        render
        media
        media_ffmpeg
    PRIVATE
        FFmpeg::avcodec
        FFmpeg::avformat
        FFmpeg::avutil
        FFmpeg::swresample
        FFmpeg::avfilter
)

if(MSVC)
    target_compile_options(export_ffmpeg PRIVATE /W4 /WX)
    target_compile_options(export_ffmpeg PRIVATE /experimental:external /external:W0)
    set_target_properties(export_ffmpeg PROPERTIES VS_GLOBAL_ExternalIncludePath "$(FFMPEG_ROOT)/include")
endif()
