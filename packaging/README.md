# Packaging (Stage 1)

`deploy-editor-ui.ps1` stages a runnable folder, it does not publish one:

```powershell
scripts/dev-shell.ps1
cmake --preset windows-s1-release
cmake --build --preset s1release
packaging/deploy-editor-ui.ps1            # -> build/s1-release/dist/editor-ui/
```

The folder contains `editor-ui.exe`, the Qt runtime from `windeployqt`
(using `src/shell/qml` as `--qmldir` so all QML imports resolve), the six
pinned FFmpeg DLLs, and the FFmpeg license/pin notes.

Notes from the first staging run (kept so the next one doesn't regress):

1. Only Release builds are packaged — Debug links the non-redistributable
   debug CRT (`MSVCP140D`, `ucrtbased`) and the script refuses Debug dirs.
2. Target machines still need the MSVC redistributable (vcredist); plain
   DLL copies do not cover the release CRT.
3. `windeployqt` also stages Qt's own bundled FFmpeg 7 DLLs (transitive
   deps of Qt6Multimedia, e.g. `swscale-8.dll`). Those coexist with our
   pinned FFmpeg 8.1 (`swscale-9.dll`) under distinct filenames — the loader
   binds each to its exact name, so this is bloat, not a conflict.
4. No `--plugindir` override: the default layout (plugin subdirs beside the
   exe) is what Qt resolves without a `qt.conf`.

Distribution rules (unchanged from `third_party/ffmpeg/PINNED.md`):

1. Local QA folders are fine.
2. Any public build needs the GPL compliance decision first (gpl-shared pin
   ships libx264), or a re-pin to lgpl-shared + a full export re-test.
3. `build/` output is never committed; only this script and this note live
   in the repo.
