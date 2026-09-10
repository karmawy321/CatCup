#include "media/MediaInterfaces.hpp"

namespace editor::media {

core::Result<ProbeResult> StubProber::probe(const std::string& path) {
    // The synthetic inspection fixture from research/fixtures.
    if (path.find("inspection-test.mp4") != std::string::npos) {
        ProbeResult r;
        r.duration = core::Rational(8, 1);
        r.fps = core::Rational(30, 1);
        r.width = 1280;
        r.height = 720;
        r.hasVideo = true;
        r.hasAudio = true;
        r.videoCodec = "mpeg4";
        r.audioCodec = "aac";
        return core::Result<ProbeResult>::ok(r);
    }
    return core::Result<ProbeResult>::fail("stub prober knows nothing about: " + path);
}

} // namespace editor::media
