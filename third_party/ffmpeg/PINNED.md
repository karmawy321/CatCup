# Pinned FFmpeg dependency (Stage 1)

Do NOT upgrade casually — the pin below is what CI/dev links against.
Only `src/media/ffmpeg/` and `src/export/ffmpeg/` may include these headers.

- **Upstream:** BtbN FFmpeg-Builds (`https://github.com/BtbN/FFmpeg-Builds`)
- **Asset:** `ffmpeg-n8.1-latest-win64-gpl-shared-8.1.zip` (n8.1 stable branch)
- **SHA-256:** `f598acdf87c4188c53cd1ee51e4ba5e20b1fc2c24dabad04e3cf8ddf7b6ae6eb`
  (verified against the release `checksums.sha256` on 2026-09-10)
- **Extract to:** `third_party/ffmpeg/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/`
  (contains `include/`, `lib/` import libs, `bin/` DLLs + `ffmpeg.exe`)
- **Library versions:** avcodec-62, avformat-62, avutil-60, swscale-9,
  swresample-6, avfilter-11, avdevice-62 (FFmpeg 8.1)
- **Why gpl-shared:** the S1 MP4 path needs `libx264` (H.264) + native `aac`.
  The lgpl-shared flavour lacks libx264 (would force mfenc with worse
  control/quality).

## License note

`gpl-shared` binaries are GPL. Local dev builds are fine, but **before any
public distribution** either complete GPL compliance review (sources +
license notices in `packaging/`) or switch the pin to lgpl-shared + mfenc
and re-run the export acceptance. This decision is tracked, not resolved.

## Re-fetch procedure

1. Download the asset + `checksums.sha256` from the BtbN latest release.
2. `Get-FileHash <zip> -Algorithm SHA256` must match the pinned hash above.
   If upstream rolled the `latest` tag past n8.1, keep this zip (do not
   silently follow) and open a deliberate upgrade task.
3. `Expand-Archive` into `third_party/ffmpeg/`. The zip itself and the
   extracted tree are git-ignored (see `.gitignore`); only this file is tracked.
