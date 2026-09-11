# Progress + handoff (Stage 1 complete, 2026-09-10)

This file is the resume point. If you are picking this up on another machine,
read sections 1–3, then work from section 5.

## 1. Where things stand

| Stage | State | Proof |
|---|---|---|
| 0 — domain, commands, versioned JSON persistence, headless CLI | Done | `ctest --preset debug` (Qt/FFmpeg-free, MSVC only) |
| 1 — Qt shell, FFmpeg media, audio-clocked preview, MP4 export | Done | `ctest --preset s1`: **13/13 green** (units + real MP4 + 30 s gate + QML smoke) |
| 2 — multitrack blend, snapping/ripple, transitions library | Done | Transitions engine, Center/Start/End alignment, wipe/crossfade/dip |
| 3 — creative tools (color grading, chroma key, filters, speed) | Done | Pure PixelPipeline parity, Color adjust, ChromaKey, Vignette, Blur, Sharpen, Clip Speed |
| 4 — AI & smart automation | Next | AI transcription, auto-captioning, smart cuts, scene detection |

Stage 1 gate, all executed: 30 s source → import → two cuts at exact thirds →
3 s title → save → reopen (900 mock frames) → real export: **h264 1280x720@30
+ aac 48 kHz stereo, 30.016 s, 899 frames**. `editor-ui` staged from Release
runs standalone with zero-warning QML.

## 2. Moving to another machine

> **Git warning (do this first):** the only `.git` around here is at
> `C:\Users\karim` with **zero commits** — i.e. the whole user profile looks
> like one repo. Do NOT commit from there. Give this project its own repo:
>
> ```powershell
> cd C:\Users\karim\OneDrive\Desktop\capcut
> git init
> git add -A          # .gitignore already excludes build/, third_party blobs, journals
> git commit -m "Stage 1: usable editing loop (Qt 6.8 + FFmpeg n8.1)"
> git remote add origin <your-repo-url>
> git push -u origin main
> ```
>
> Then clone that repo on the other machine and continue below.

### Fresh-machine bootstrap (all verified 2026-09-10)

1. Visual Studio Build Tools 2026 with MSVC + Windows SDK (any recent 18.x).
2. `winget install Kitware.CMake Ninja-build.Ninja` (need CMake ≥ 3.28).
3. Qt 6.8.3 to `C:/Qt`: `pip install aqtinstall`, then
   `aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -O C:/Qt -m qtmultimedia qtimageformats qtshadertools`
4. FFmpeg pin: follow **`third_party/ffmpeg/PINNED.md`** (asset name + sha256;
   blobs are git-ignored and must be re-fetched, ~80 MB).
5. Build + test from an x64 Native Tools prompt (or `scripts/dev-shell.ps1`):
   `cmake --preset windows-s1-debug` → `cmake --build --preset s1` →
   `ctest --preset s1 --output-on-failure`.

### What travels with git vs what you re-fetch

- In git: all source, QML, `CMakePresets.json`, `third_party/ffmpeg/PINNED.md`
  (the pin, not the binaries), `packaging/`, `docs/`, `research/`, this file.
- NOT in git (re-fetch/rebuild): `third_party/ffmpeg/*.zip` + extracted tree,
  `C:/Qt`, `build/` (includes all test artifacts and the staged dist).

## 3. Pins (do not drift casually)

- Qt **6.8.3**, `win64_msvc2022_64` (+ qtmultimedia, qtimageformats, qtshadertools).
- FFmpeg **n8.1 gpl-shared** (`ffmpeg-n8.1-latest-win64-gpl-shared-8.1.zip`),
  sha256 `f598acdf…6ae6eb` — full hash in `third_party/ffmpeg/PINNED.md`.
  DLL majors: avcodec-62, avformat-62, avutil-60, swscale-9, swresample-6, avfilter-11.
- **GPL note:** the gpl-shared pin ships libx264. Local QA builds are fine;
  public distribution needs the compliance review (or an lgpl-shared re-pin +
  full export re-test). Tracked, not resolved.

## 4. Hard-won traps (read before touching these areas)

1. **Drawtext + Windows paths:** drive-letter colons do not survive
   `avfilter_graph_parse_ptr` even escaped (FFmpeg 8.1). The exporter builds
   the graph filter-by-filter (`makeFilter` in `Mp4Exporter.cpp`) with
   two-phase init (alloc → `av_opt_set(..., AV_OPT_SEARCH_CHILDREN)` →
   `avfilter_init_*`), because drawtext validates textfile in its init
   callback. Never go back to a format-string graph for titles.
2. **QML module cycle:** QML files in the `NativeEditor` module must NOT
   `import NativeEditor`. C++ helper types live in the separate `EditorCpp`
   URI (`main_ui.cpp`). `PreviewItem::setSession` takes `QObject*`, not
   `Session*` (QML can't marshal the latter; validated `qobject_cast` inside).
3. **Timeline polish loop:** playhead overlays are Flickable-level siblings,
   never `Column` children; all lanes bind one shared `laneW` expression.
4. **Asset paths:** on-disk paths are project-relative, in-memory always
   absolute (`resolveAssetPath`/`storeAssetPath`). Tests that write the .json
   into the build dir must pass absolute fixture paths.
5. **PowerShell 5.1:** no `&&`, no `head`, `>` writes UTF-16 (breaks file
   reads — pipe instead), non-ASCII chars break BOM-less `.ps1` scripts.
6. **Packaging:** Release only — Debug links the non-redistributable debug
   CRT and `deploy-editor-ui.ps1` refuses Debug dirs. No `--plugindir`
   override (breaks plugin lookup without `qt.conf`). `qoffscreen.dll` is
   copied explicitly for headless CI smoke.
7. **CTest PowerShell strings:** CMake eats `\d` (write `[\\d.]`), and
   `-notmatch` on an array is a filter, not a boolean — use
   `-not ($x -match ...)`.

## 5. Next: Stage 2 entry points

Per `research/BUILD-PLAN.md` ("Everyday editor"): multitrack overlap/blend,
snapping/ripple, **transitions library** (browse → preview card → drop on
cut → tune duration/alignment/easing → save preset), proxies, relinking UI.

Concrete starting threads (in dependency order):

1. `render::FramePlan` must carry **two source frames + blend factor** at a
   transition (today: one `PlacedClip`). Preview and export share it, so both
   upgrade together.
2. `core::Model`: `Transition` (type, duration, alignment, easing) + overlap
   representation; `EffectRegistry` gains transition entries with parameter
   schemas — the studio UI generates from these, no hard-coded panels.
3. `commands/`: overlap-aware move/trim/ripple + undo; evaluator + serializer
   tests first (rational thirds, preview/export agreement).
4. Shell: transition browser category + inspector controls + preset save.

Suggested first commit on the new machine: re-run `ctest --preset s1`
(green baseline), then branch `stage-2`.
