# The real FFmpeg backend is Stage 1 work and is intentionally NOT started.
#
# Before writing code here, pin the dependency manifest:
#   1. FFmpeg release tag + Windows build flavour (shared vs static).
#   2. Expected DLL set (avcodec/avformat/avfilter/swscale/swresample versions).
#   3. License review (LGPL/GPL implications for our distribution).
#   4. Hardware decode policy (which HW Accels are allowed, CPU fallback rule).
#
# Only src/media/ffmpeg/ may #include <libav*.h>. Everything else programs
# against include/media/MediaInterfaces.hpp so the domain never depends on
# vendored C headers. Gate the target on ENABLE_FFMPEG.
