# Evidence and limitations

## Signed-in follow-up

On September 8 the user reported completing login manually. The returned CapCut process remained version 9.4.0.4015. EditPilot showed its prompt and suggested edits, and the editor's Spaces browser populated with cloud-media cards and an account-specific storage meter. This updates the earlier sign-in-gated observations below.

Additional panels inspected: Dreamina same-account sync entry; AI image prompt/model/settings (Seedream 4.3 displayed); AI video references/model/output settings (Dreamina Seedance 2.5 displayed, still requiring Pro); AI dialogue scene; populated multilingual text-to-speech catalog; custom voice entry. These were read-only workflow observations, not successful processing tests. Differences in previously unopened panels cannot all be attributed to login.

No assistant prompt, generation job, upload, sync, subscription purchase, speech generation, or custom voice creation was submitted. The existing synthetic inspection project remained the work surface. Earlier statements below that describe sign-in being unavailable refer to the earlier sessions; algorithm, cloud-mutation, and assistant-execution tests remain outstanding.

## Session record

| Session | Observed executable | What was covered |
|---|---|---|
| September 7, 2026 | `C:\Users\karim\AppData\Local\CapCut\Apps\8.8.0.3774\CapCut.exe` | Home, template browser, signed-out Spaces, editor asset categories, video/audio/speed/animation/color/text inspectors, timeline context menu, export dialog |
| September 8, 2026 continuation | `C:\Users\karim\AppData\Local\CapCut\Apps\9.4.0.4015\CapCut.exe` | Existing test project reopened; editor layout; menu; EditPilot sign-in panel; installation components rechecked |

Version was obtained from returned window process paths, with file-version metadata additionally checked for the resumed executable. The reason for the version change was not investigated. The earlier inspector inventory must not be silently treated as a complete 9.4 audit.

## Local technical evidence

Read-only directory inspection found the following component families in both installations:

- `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`.
- `Qt6Qml.dll`, `Qt6Quick.dll`, `Qt6QuickControls2.dll`, Quick layouts/dialogs/templates/shapes, and `QtQml`/`QtQuick` directories.
- `avcodec-61.dll`, `avformat-61.dll`, `avfilter-10.dll`, `swscale-8.dll`, `swresample-5.dll`, `ffmpeg.dll`, and `ffmpeg.exe`.
- `vcruntime140.dll` and `vcruntime140_1.dll`.
- `cef` directory, consistent with embedded Chromium components; usage by a particular panel was not established.

The resumed `Qt6Core.dll` reports file/product version `6.2.2.617`. `CapCut.exe` reports file version `9.4.0.4015` and product version `9.4.0.1000dd36`.

**Inference:** C++ with Qt Quick/QML and FFmpeg is an evidence-backed direction for a similar native editor. These files do not identify every source language, the C++ language standard, the backend stack, the exact renderer, or which algorithms run locally versus remotely. QML itself can include JavaScript; presence of a DLL does not prove a particular feature uses it.

No CapCut source code, model weights, private service protocols, or internal project schema was extracted or used as implementation material.

## Exercised actions

1. Activated the open CapCut window.
2. Expanded the home tool list; inspected a loaded template browser and signed-out cloud entry.
3. Created a separate project, named automatically `0907`.
4. Generated `fixtures/inspection-test.mp4`, an eight-second 1280×720/30 fps synthetic test pattern with sine-wave audio.
5. Imported that local test file through CapCut's native file picker.
6. Added it to the timeline with the asset card plus button and observed preview, clip thumbnails, duration, and audio waveform.
7. Added Default text with the text card plus button and observed a separate title track and text bounding box.
8. Opened inspectors and export settings; canceled export.
9. During the resumed session, observed the same test project with its clip and text and inspected the gated EditPilot entry.

The initial synthetic-generation command requested `libx264`, which was not included in the installed FFmpeg utility. A subsequent command used its available `mpeg4` encoder with AAC successfully. This was fixture preparation with an existing tool, not the proposed application's FFmpeg dependency setup.

The Windows file dialog's accessibility focus report remained inconsistent with visual focus. The filename field was visually focused, the typed path was verified in that field, and import succeeded. A drag attempt did not insert the media; the card plus button did. These are tool-session observations, not general CapCut defects.

## Not verified

- Complete feature parity or every nested control.
- Successful premium processing, algorithm quality, entitlement enforcement, or completed export.
- Every downloadable template, filter, sticker, effect, voice, or regional catalog variant.
- Authenticated cloud sync, sharing, subscriptions, avatar generation, or EditPilot editing.
- Every independent home tool destination, including recording and design/marketing studios.
- Full preferences and shortcut editor. Transient-menu interaction did not reliably open Settings.
- Detailed timeline edit semantics, keyframe curves, compound clips, multicamera, crash recovery, HDR, long-form performance, or 8K output.
- Whether any specific AI feature is local or cloud-backed. The build plan's processing approaches are proposals.

Some catalog panels initially showed loading states. Filters and home templates populated; other catalogs were recorded at their shell/navigation level. Availability can vary with network, account, region, and app version.

## Source register

Public sources checked September 7–8, 2026. Local UI observations take precedence for claims about what appeared in this installation.

| Source | Used for |
|---|---|
| [Qt Quick Scene Graph](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html) | Qt Quick rendering architecture and native extension support; informs our framework choice |
| [FFmpeg documentation](https://ffmpeg.org/documentation.html) | Media component/library responsibilities; informs decode/encode/filter design |
| [ONNX Runtime DirectML provider](https://onnxruntime.ai/docs/execution-providers/DirectML-ExecutionProvider.html) | Windows inference-provider direction; documentation points new Windows deployment work toward Windows ML |
| [CapCut: What is CapCut Pro?](https://www.capcut.com/help/capcut-pro) | Official confirmation of premium text styles/templates, transitions/effects, body effects/filters, and cloud services as feature families |

The CapCut Pro PC resource article was found in search but direct retrieval returned HTTP 451. Its body was not used as evidence. Public marketing/help pages do not prove complete entitlement rules for this local version.

## Next audit procedure

Use the existing isolated project. Start by recording the current executable version and account state. For each feature, record route, input fixture, visible controls, Pro/access marker, processing location if established, output, failure/cancel behavior, and export consistency. Mark controls as observed separately from completed tests. Recheck dynamic menus after they settle; do not reuse coordinates from an earlier layout.

Use synthetic or explicitly chosen test assets for each class: speech, portrait/hair, camera motion, low light/noise, fast movement, scene cuts, music, portrait video, VFR, and HDR. Keep the user’s existing projects untouched during the audit.
