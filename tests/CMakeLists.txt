cmake_minimum_required(VERSION 3.28)

function(editor_test name)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE support)
    target_link_libraries(${name} PRIVATE core persist commands media render export effects ai)
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /WX)
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

editor_test(test_time test_time.cpp)
editor_test(test_project test_project.cpp)
editor_test(test_serialize test_serialize.cpp)
editor_test(test_commands test_commands.cpp)
editor_test(test_evaluator test_evaluator.cpp)
editor_test(test_effects test_effects.cpp)
editor_test(test_export_job test_export_job.cpp)
editor_test(test_jobs test_jobs.cpp)

# Headless acceptance: build demo project, save, reopen, mock-export.
add_test(NAME acceptance_demo
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -Command
        "$exe = '$<TARGET_FILE:editor>'; & $exe --create-demo acceptance-demo.json; if ($LASTEXITCODE -ne 0) { exit 1 }; & $exe --check acceptance-demo.json"
)
set_tests_properties(acceptance_demo PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Stage 1 media/export tests (need the pinned FFmpeg + real fixture).
if(ENABLE_FFMPEG)
    foreach(s1test test_media_ffmpeg test_mp4_export)
        editor_test(${s1test} ${s1test}.cpp)
        target_link_libraries(${s1test} PRIVATE media_ffmpeg export_ffmpeg)
        target_compile_definitions(${s1test} PRIVATE
            FIXTURE_MP4="${CMAKE_SOURCE_DIR}/research/fixtures/inspection-test.mp4"
            TMP_DIR="${CMAKE_CURRENT_BINARY_DIR}")
        ffmpeg_copy_runtime_dlls(${s1test})
        # Reuse one mp4 across the export cases (title/plain/cancel share it).
        set_tests_properties(${s1test} PROPERTIES FIXTURES_REQUIRED s1tmp)
    endforeach()
    # Real end-to-end: headless CLI exports the demo project to real MP4.
    # The fixture travels as an absolute path: the demo json lives in the
    # build dir, so a repo-relative asset reference would mis-resolve there
    # (relative references resolve against the .json location — by design).
    add_test(NAME acceptance_mp4
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -Command
            "$exe = '$<TARGET_FILE:editor>'; $fix = '${CMAKE_SOURCE_DIR}/research/fixtures/inspection-test.mp4'; & $exe --create-demo acceptance-mp4.json $fix; if ($LASTEXITCODE -ne 0) { exit 1 }; & $exe --export-mp4 acceptance-mp4.json acceptance-mp4.mp4; if ($LASTEXITCODE -ne 0) { exit 1 }; if (-not (Test-Path acceptance-mp4.mp4)) { exit 1 }"
    )
    set_tests_properties(acceptance_mp4 PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endif()

# Stage 1 gate (BUILD-PLAN: "30-second edit, reopen, export the same frames,
# timing, title, and sound"). Heavier than the 8 s fixture tests above, so it
# generates its own 30 s source in the build dir and checks the real export
# with ffprobe. Title-burn correctness itself is covered frame-exactly by
# test_mp4_export; this gate proves duration/timing/sound survive the full
# save -> reopen -> export loop at S1 scale.
if(ENABLE_FFMPEG)
    add_test(NAME acceptance_s1_30s
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -Command
            "$ff = '${FFMPEG_BIN_DIR}/ffmpeg.exe'; $fp = '${FFMPEG_BIN_DIR}/ffprobe.exe'; $exe = '$<TARGET_FILE:editor>'; & $ff -hide_banner -loglevel error -y -f lavfi -i testsrc2=duration=30:size=1280x720:rate=30 -f lavfi -i sine=frequency=440:duration=30:sample_rate=48000 -c:v mpeg4 -q:v 3 -c:a aac -ac 2 -shortest s1gate-30s.mp4; if ($LASTEXITCODE -ne 0) { exit 1 }; & $exe --create-s1 s1gate-30s.mp4 s1gate.json; if ($LASTEXITCODE -ne 0) { exit 1 }; & $exe --check s1gate.json; if ($LASTEXITCODE -ne 0) { exit 1 }; & $exe --export-mp4 s1gate.json s1gate-out.mp4; if ($LASTEXITCODE -ne 0) { exit 1 }; $info = & $fp -hide_banner -v error -show_entries format=duration -show_entries stream=codec_name -of default=noprint_wrappers=1 s1gate-out.mp4; if ($LASTEXITCODE -ne 0) { exit 1 }; if (-not ($info -match 'codec_name=h264')) { Write-Output $info; exit 1 }; if (-not ($info -match 'codec_name=aac')) { Write-Output $info; exit 1 }; $dur = [double](($info | Select-String 'duration=([\\d.]+)').Matches[0].Groups[1].Value); if ($dur -lt 29.5 -or $dur -gt 30.5) { Write-Output ('duration=' + $dur); exit 1 }; Write-Output ('S1 gate OK: 30 s edit, reopen, h264+aac export, duration=' + $dur)"
    )
    set_tests_properties(acceptance_s1_30s PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        TIMEOUT 900)
endif()

# Stage 1 shell test: Main.qml must load warning-free (offscreen).
# Kept strict on purpose — the smoke harness prints every QQmlError and
# exits non-zero on objectCreationFailed, so new QML regressions fail here.
if(ENABLE_QT_UI)
    add_test(NAME qml_smoke COMMAND "$<TARGET_FILE:editor-ui>" --qml-smoke)
    set_tests_properties(qml_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
endif()
