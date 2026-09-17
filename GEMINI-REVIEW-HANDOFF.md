# Review Gemini's editor improvements

## Request for the reviewing assistant

Review Gemini's work against the requirements below. Inspect the actual code, run relevant checks, and inspect the running UI. Report regressions, incomplete fixes, misleading controls, and unsupported completion claims. Give a prioritized verdict before changing implementation files. Do not assume Gemini's report or existing milestone documents are accurate.

This is a Windows desktop video editor using C++20, Qt 6 Quick/QML, and FFmpeg. The project folder was C:\Users\karam\OneDrive\Desktop\capcut on the work PC; use the actual project location on this machine.

Gemini was asked to implement the improvements below. The user wants an independent assessment of whether the work is complete and reliable, not another round of feature additions.

## Evidence available before Gemini's work

The initial review inspected source and successfully ran the packaged editor's --qml-smoke check. It could not capture a live editor window. Visual judgments were therefore source-based, not verified screenshots. The findings below describe that baseline; verify whether Gemini has fixed them.

- Main.qml used session.save() directly for Ctrl+S. Session::save() returned "No file name — use Save As" for unnamed projects.
- New/Open/Quit lacked visible unsaved-change guards.
- MediaBrowser.qml and Inspector.qml wrote persisted clip effects and also set a global QML preview MultiEffect using different values. This risked duplicate effects, incorrect clip scope, and preview/export disagreement.
- ExportDialog.qml displayed 1080p, 720p, and 4K pills without selection behavior. 1080p was always highlighted, although new sequences defaulted to 1280x720.
- Share and some preview controls had hover styling but no action.
- TimelineView.qml generated waveforms from sine/cosine expressions, not audio samples. Video strips lacked useful sampled frame thumbnails.
- Many labels were 9–11 px, some controls 22–26 px tall, and secondary text was dim. Verify actual readability and layout in the running app.
- User-facing messages included implementation language such as "Stage 2 build from your dev PC" and "Live GPU Shader Effect".
- README.md and PROGRESS.md had conflicting milestone information.

## Required improvements and acceptance criteria

### 1. Baseline and change attribution

- Read applicable repository instructions and inspect build configuration.
- Identify the executable launched by run.bat/scripts/run.ps1 and verify it matches current source.
- Use git history/diff if available; otherwise distinguish observed current defects from proven regressions. Do not invent a before/after comparison.
- Record pre-existing failures separately when evidence permits.
- Inspect empty, populated, selected-clip, playback, and export UI states using a disposable project and synthetic media.

### 2. Project protection — highest priority

- Ctrl+S opens Save As for unnamed projects.
- New, Open, Quit, and window close share Save / Discard / Cancel handling.
- Pending actions proceed only after successful save or explicit discard.
- Canceling Save As and save failures preserve the current project and edits.
- Failed Save As does not incorrectly adopt an invalid destination.
- Debounced autosave writes separate recovery data rather than silently overwriting the saved project.
- Startup offers usable recovery with project identity and recovery time.
- Saved/unsaved/saving/failed states are understandable.
- Verify recovery and ordinary save/reopen preserve media references.

### 3. Preview/export consistency — highest priority

- Persisted clip parameters are authoritative; preview and export use equivalent effect semantics.
- Remove or reconcile the duplicate global shader path.
- Effects apply only to the intended clip and time range.
- Inspector values follow selection and persisted state.
- Presets, reset, undo, redo, save, and reopen agree.
- Any temporary effect audition mode is explicitly identified and separate from committed effects.
- Compare preview and export at matching timestamps, allowing expected encoding differences.
- Check supported color adjustments, blur, sharpen, vignette, transforms, and transitions.

### 4. Functional controls and export

- Resolution choices change actual export output and summary; selection is not decorative.
- Project settings and export overrides are clearly distinguished; aspect ratio is preserved.
- Expose only supported codec, format, frame-rate, and quality options.
- Validate output paths and handle existing-file overwrite decisions.
- Prevent conflicting changes during export.
- Closing a dialog must not unexpectedly cancel export; cancellation behavior is clear.
- Success offers Open File and Open Output Folder.
- No enabled button/menu/toggle/shortcut silently does nothing.
- Unsupported controls are removed or disabled with a useful explanation.
- Decorative Pro branding is removed unless it represents real capability.
- Repeated export, cancellation, failure, retry, and shutdown are safe, including worker lifetime and progress handling.
- Verify exported metadata matches selected settings.

### 5. Honest, responsive timeline

- Waveforms represent actual audio peaks, computed in background work and cached.
- Real sampled video thumbnails appear across clips.
- Cache invalidation and zoom detail are appropriate.
- Trim offsets and speed changes preserve waveform/thumbnail alignment.
- Silent or audio-free media is represented accurately.
- Media decoding does not block QML delegates or rendering paths.
- Scrolling/playback remain responsive during asset generation.
- Labels, selection, trim handles, and playhead remain readable.
- Missing media uses an honest placeholder.
- Test silence and known audio impulses, plus trimming, moving, zooming, and speed changes.

### 6. Readability and interaction

- Consistent typography, spacing, icons, control heights, and practical click targets.
- Improve small important labels and low-contrast secondary text.
- Verify Qt font configuration rather than assuming CSS-style fallback lists work.
- Clear hover, pressed, disabled, selected, and keyboard-focus states.
- Accessible controls and concise tooltips/shortcut hints where appropriate.
- Global shortcuts do not interfere with text entry.
- Resizable panels remain usable at the supported minimum window size and Windows scaling of 100%, 125%, and 150% where testable.
- No overlapping/clipped controls; preview and timeline remain visually dominant.

### 7. Everyday workflow

- Empty library provides Import Media and next-step guidance.
- Empty timeline explains how to add imported media.
- Inspector without selection shows useful guidance.
- Actions requiring selected/adjacent clips explain unmet requirements.
- Important errors are recoverable and not available only as tiny transient status text.
- Missing media is explained and has a practical relink workflow.
- Remove developer-facing implementation messages and inconsistent naming.

## End-to-end verification

1. Create a disposable project; import synthetic video and audio.
2. Add, split, trim, and move clips; verify existing snapping/ripple functionality.
3. Apply and adjust an effect; undo and redo; inspect another clip for leakage.
4. Add supported text/captions and a transition.
5. Save, close, reopen, and verify persistence.
6. Export multiple supported settings; inspect dimensions, frame rate, audio, duration, and appearance.
7. Exercise invalid paths, missing media, cancellation, repeat export, and recovery.
8. Inspect relevant window sizes and display scaling.

Use targeted tests for data-loss prevention, effect parity, export settings, waveform alignment, and discovered correctness bugs. A QML smoke check only proves loading; it does not prove the editing workflow works. If a check cannot be performed, say exactly what remains unverified.

## Useful source locations

- src/shell/qml/Main.qml
- src/shell/qml/Theme.qml
- src/shell/qml/MediaBrowser.qml
- src/shell/qml/Inspector.qml
- src/shell/qml/PreviewView.qml
- src/shell/qml/TimelineView.qml
- src/shell/qml/ExportDialog.qml
- src/shell/src/Session.cpp
- src/shell/src/ExportController.cpp
- src/render, src/effects, src/media, src/export, src/persist
- tests, scripts, packaging

Previously documented build commands include cmake --preset windows-s1-debug, cmake --build --preset s1, and ctest --preset s1 --output-on-failure. Verify local prerequisites and current presets first. The packaged executable was build/s1-release/dist/editor-ui/editor-ui.exe; a debug executable also existed. Do not mistake an old packaged binary for current source.

## Requested review output

- Lead with whether the application is ready for ordinary editing and why.
- List actionable findings by severity with file/line references, concrete reproduction or code evidence, user impact, and suggested correction.
- Identify completed, partial, missing, and unverified requirements.
- Distinguish proven bugs from suspected risks and visual preferences.
- Summarize builds/tests actually run and their outcomes.
- Include actual UI screenshots when available; do not present mockups as evidence.
- Recommend the next bounded fixes, prioritizing data loss and output correctness.
- Do not modify implementation until the user requests fixes.

## Transfer reminder

This file carries the review context, not the project itself. Ensure Gemini's completed source changes and final report are saved and available on the home PC. Builds and installed Qt/FFmpeg dependencies may need to be recreated there. OneDrive synchronization has not been verified by the assistant.
