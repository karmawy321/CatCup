# Pinned FFmpeg (BtbN win64 shared, FFmpeg 8.1). See third_party/ffmpeg/PINNED.md.
# Only src/media/ffmpeg/ and src/export/ffmpeg/ may link these targets.

set(FFMPEG_ROOT
    "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/ffmpeg-n8.1-latest-win64-gpl-shared-8.1"
    CACHE PATH "Pinned FFmpeg root (include/, lib/, bin/)")

if(NOT EXISTS "${FFMPEG_ROOT}/include/libavcodec/avcodec.h")
    message(FATAL_ERROR
        "FFmpeg not found at ${FFMPEG_ROOT}. "
        "Follow third_party/ffmpeg/PINNED.md to fetch and verify the pinned build.")
endif()

# Imported DLL names carry the pinned major versions — deliberate, so an
# accidental FFmpeg swap fails at configure time instead of at runtime.
set(_ffmpeg_dll_suffixes
    avcodec:avcodec-62
    avformat:avformat-62
    avutil:avutil-60
    swscale:swscale-9
    swresample:swresample-6
    avfilter:avfilter-11)

foreach(_pair IN LISTS _ffmpeg_dll_suffixes)
    string(REPLACE ":" ";" _parts "${_pair}")
    list(GET _parts 0 _lib)
    list(GET _parts 1 _dll)
    # SHARED IMPORTED: MSVC links IMPORTED_IMPLIB (.lib); IMPORTED_LOCATION
    # (.dll) is runtime-only. Never feed a DLL to the linker (LNK1107).
    add_library(FFmpeg::${_lib} SHARED IMPORTED GLOBAL)
    set_target_properties(FFmpeg::${_lib} PROPERTIES
        IMPORTED_IMPLIB "${FFMPEG_ROOT}/lib/${_lib}.lib"
        IMPORTED_LOCATION "${FFMPEG_ROOT}/bin/${_dll}.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_ROOT}/include")
    if(NOT EXISTS "${FFMPEG_ROOT}/lib/${_lib}.lib")
        message(FATAL_ERROR "FFmpeg import lib missing: ${FFMPEG_ROOT}/lib/${_lib}.lib")
    endif()
endforeach()

set(FFMPEG_BIN_DIR "${FFMPEG_ROOT}/bin" CACHE PATH "Pinned FFmpeg DLL directory")

# Copy the FFmpeg runtime next to a target so dev runs and tests "just work".
# Packaging (windeployqt + explicit DLL list) is a separate step in packaging/.
function(ffmpeg_copy_runtime_dlls target)
    foreach(_dll avcodec-62 avformat-62 avutil-60 swscale-9 swresample-6 avfilter-11)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FFMPEG_BIN_DIR}/${_dll}.dll"
                "$<TARGET_FILE_DIR:${target}>/${_dll}.dll"
            COMMENT "Staging FFmpeg runtime ${_dll}.dll for ${target}")
    endforeach()
endfunction()
