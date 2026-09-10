#include "shell/FrameCompositor.hpp"
#include "shell/Session.hpp"

#include "render/Evaluator.hpp"

#include <QPainter>

namespace editor::shell {

void FrameCompositor::invalidate() {
    readers_.clear();
}

QImage FrameCompositor::frameAt(const core::Project& project, const core::Sequence& seq,
                                const core::Rational& t) {
    const int w = static_cast<int>(seq.width > 0 ? seq.width : 1280);
    const int h = static_cast<int>(seq.height > 0 ? seq.height : 720);
    QImage canvas(w, h, QImage::Format_RGBA8888);
    canvas.fill(Qt::black);

    const render::FramePlan plan = render::Evaluator::evaluateVideoAt(seq, t);
    const render::PlacedClip* videoLayer = nullptr;
    for (const auto& layer : plan.layers) {
        if (layer.trackKind == core::TrackKind::Video && !layer.assetId.empty()) {
            videoLayer = &layer; // topmost wins in S1
        }
    }
    if (videoLayer != nullptr) {
        Reader& reader = readers_[videoLayer->assetId];
        if (!reader.ready) {
            const auto ait = project.assets.find(videoLayer->assetId);
            if (ait == project.assets.end()) {
                return canvas;
            }
            reader.path = ait->second.path;
            if (reader.decoder.open(reader.path).isErr()) {
                return canvas;
            }
            const auto fps = reader.decoder.fps();
            reader.ready = true;
            if (reader.decoder.seek(videoLayer->sourceTime).isErr()) {
                return canvas;
            }
            reader.lastDelivered = core::Rational(-1, 1);
            (void)fps;
        }
        const auto ait = project.assets.find(videoLayer->assetId);
        const core::Rational srcFps =
            ait == project.assets.end() ? core::Rational(30, 1) : ait->second.fps;
        if (videoLayer->sourceTime < reader.lastDelivered) {
            reader.decoder.seek(videoLayer->sourceTime);
        }
        const double frameDur =
            srcFps.num() > 0 ? 1.0 / static_cast<double>(srcFps) : 1.0 / 30.0;
        for (int guard = 0; guard < 600; ++guard) {
            auto got = reader.decoder.nextFrame();
            if (got.isErr()) {
                break;
            }
            const double pts = static_cast<double>(got.value().pts);
            if (pts + frameDur <= static_cast<double>(videoLayer->sourceTime) && frameDur > 0) {
                continue;
            }
            reader.lastDelivered = got.value().pts;
            const auto& f = got.value();
            QImage img(f.rgba.data(), f.width, f.height, f.width * 4,
                       QImage::Format_RGBA8888);
            // Aspect-fit center (same layout contract as the export graph).
            const QSize fitted =
                QSize(f.width, f.height).scaled(QSize(w, h), Qt::KeepAspectRatio);
            QPainter p(&canvas);
            p.drawImage(QRect(QPoint((w - fitted.width()) / 2, (h - fitted.height()) / 2),
                              fitted),
                        img);
            p.end();
            break;
        }
    }

    // Titles: centered plus clip offset, pixel-size font (same math as
    // drawtext in the exporter: x=(w-tw)/2+dx, y=(h-th)/2+dy).
    QPainter painter(&canvas);
    painter.setPen(Qt::white);
    for (const auto& layer : plan.layers) {
        const auto cit = seq.clips.find(layer.clipId);
        if (cit == seq.clips.end() || cit->second.text.empty()) {
            continue;
        }
        const core::Clip& c = cit->second;
        QFont font(QString::fromStdString(c.fontFamily.empty() ? "Arial" : c.fontFamily));
        font.setPixelSize((std::max)(8, static_cast<int>(c.fontSizePt)));
        painter.setFont(font);
        const QRect bounds = painter.boundingRect(canvas.rect(), Qt::AlignCenter,
                                                  QString::fromStdString(c.text));
        QPoint at((w - bounds.width()) / 2 + static_cast<int>(c.transform.x),
                  (h - bounds.height()) / 2 + static_cast<int>(c.transform.y));
        painter.drawText(QRect(at, bounds.size()), Qt::AlignCenter,
                         QString::fromStdString(c.text));
    }
    painter.end();
    return canvas;
}

} // namespace editor::shell
