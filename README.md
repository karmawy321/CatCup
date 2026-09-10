# Desktop video editor — discovery and build plan

The recommended starting point is a Windows desktop editor with a **C++20 core, Qt 6 Quick/QML interface, and FFmpeg media pipeline**. Build a reliable import → edit → save → export workflow first, then implement the advanced feature families in the parity backlog.

This workspace currently contains research and planning, not an implemented editor.

Latest update: a signed-in non-premium account survey on September 8 added EditPilot's prompt interface, populated Spaces, Dreamina sync, AI generation settings, and voice-catalog/custom-voice entries. Some generation still explicitly requires Pro. See the signed-in follow-up at the top of the feature map.

- [Observed CapCut feature map](research/FEATURE-MAP.md)
- [Architecture, milestones, and first implementation tasks](research/BUILD-PLAN.md)
- [Evidence, sources, version differences, and verification gaps](research/EVIDENCE.md)
- [Synthetic inspection video](research/fixtures/inspection-test.mp4)

Inspection dates: September 7–8, 2026. Initial session: CapCut 8.8.0.3774. Resumed session: CapCut 9.4.0.4015. Most detailed inspector observations belong to the initial version; they have not all been revalidated in 9.4.

The inspection project is named **0907** in CapCut. It contains an eight-second synthetic video and a default text overlay. It was created separately from the pre-existing projects and remains available for further inspection.

The map is a broad, directly observed desktop survey. It is **not a claim that every nested control, catalog asset, cloud service, or Pro operation was fully tested**. The evidence document lists the remaining gaps.

## Build — Stage 1 usable editing loop (done)

Stage 0 (domain, commands, versioned persistence, headless CLI) still builds
with MSVC alone via the `debug` preset. Stage 1 adds the editor on top:

- `src/media/ffmpeg` — real prober + RGBA video / 48 kHz stereo-s16 audio
  decoders + thumbnailer on the pinned FFmpeg n8.1 (`ENABLE_FFMPEG`).
  Only this dir and `src/export/ffmpeg` may include libav*.
- `src/export/ffmpeg` — real MP4 (libx264 + AAC) driven by the same
  `render::Evaluator` as preview; titles burn in via drawtext.
- `src/shell` — the only Qt module: 4-region QML shell (browser, preview,
  inspector, timeline), audio-clocked player, worker-thread export dialog.
- `packaging/` — `windeployqt`-based staging (Release only; Debug links the
  non-redistributable debug CRT and is refused by the script).

Pins (verified 2026-09-10): Qt 6.8.3 (`C:/Qt`, via aqtinstall), BtbN
FFmpeg n8.1 gpl-shared (sha256 in `third_party/ffmpeg/PINNED.md`).

```powershell
scripts/dev-shell.ps1
cmake --preset windows-s1-debug
cmake --build --preset s1
ctest --preset s1 --output-on-failure     # 13/13: units + mp4 + 30 s gate + QML smoke
```

Stage 1 gate (all executed, all green): 30 s source → import → two cuts at
exact thirds → 3 s title → save → reopen (`--check`: 900 mock frames) →
real export (`--export-mp4`): h264 1280x720@30 + aac 48 kHz stereo,
30.016 s, 899 frames. Title-burn is proven frame-exact by `test_mp4_export`
(hash + center-band activity vs title-less export). `editor-ui` staged from
`windows-s1-release` runs standalone (`--qml-smoke` clean, zero warnings).

Next is Stage 2: multitrack overlap/blend, snapping/ripple, transitions
library (registered effect types + preset metadata), proxies/relink UI.
