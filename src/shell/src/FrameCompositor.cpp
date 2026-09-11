#include "shell/FrameCompositor.hpp"
#include "shell/Session.hpp"

#include "effects/PixelPipeline.hpp"
#include "render/Evaluator.hpp"

#include <QPainter>
#include <algorithm>
#include <optional>

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

    auto getDecodedFrame = [&](const core::Id& assetId, const core::Rational& srcTime)
        -> std::optional<media_ffmpeg::DecodedVideoFrame> {
        Reader& reader = readers_[assetId];
        if (!reader.ready) {
            const auto ait = project.assets.find(assetId);
            if (ait == project.assets.end()) {
                return std::nullopt;
            }
            reader.path = ait->second.path;
            if (reader.decoder.open(reader.path).isErr()) {
                return std::nullopt;
            }
            reader.ready = true;
            if (reader.decoder.seek(srcTime).isErr()) {
                return std::nullopt;
            }
            reader.lastDelivered = core::Rational(-1, 1);
        }
        const auto ait = project.assets.find(assetId);
        const core::Rational srcFps =
            (ait == project.assets.end()) ? core::Rational(30, 1) : ait->second.fps;
        if (srcTime < reader.lastDelivered) {
            reader.decoder.seek(srcTime);
        }
        const double frameDur =
            srcFps.num() > 0 ? 1.0 / static_cast<double>(srcFps) : 1.0 / 30.0;
        for (int guard = 0; guard < 600; ++guard) {
            auto got = reader.decoder.nextFrame();
            if (got.isErr()) {
                break;
            }
            const double pts = static_cast<double>(got.value().pts);
            if (pts + frameDur <= static_cast<double>(srcTime) && frameDur > 0) {
                continue;
            }
            reader.lastDelivered = got.value().pts;
            return got.value();
        }
        return std::nullopt;
    };

    QPainter painter(&canvas);

    for (const auto& layer : plan.layers) {
        if (layer.trackKind != core::TrackKind::Video || layer.assetId.empty()) {
            continue;
        }

        if (layer.inTransition && !layer.secondaryAssetId.empty()) {
            auto optA = getDecodedFrame(layer.assetId, layer.sourceTime);
            auto optB = getDecodedFrame(layer.secondaryAssetId, layer.secondarySourceTime);

            QImage imgA(w, h, QImage::Format_RGBA8888);
            imgA.fill(Qt::transparent);
            if (optA) {
                const auto& fA = *optA;
                QImage rawA = QImage(fA.rgba.data(), fA.width, fA.height, fA.width * 4,
                                     QImage::Format_RGBA8888).copy();
                if (!layer.effects.empty()) {
                    effects::PixelPipeline::applyEffects(rawA.bits(), rawA.width(), rawA.height(),
                                                         static_cast<int>(rawA.bytesPerLine()), layer.effects);
                }
                const QSize fitA =
                    QSize(fA.width, fA.height).scaled(QSize(w, h), Qt::KeepAspectRatio);
                QPainter pa(&imgA);
                pa.drawImage(QRect(QPoint((w - fitA.width()) / 2, (h - fitA.height()) / 2),
                                   fitA),
                             rawA);
                pa.end();
            }

            QImage imgB(w, h, QImage::Format_RGBA8888);
            imgB.fill(Qt::transparent);
            if (optB) {
                const auto& fB = *optB;
                QImage rawB = QImage(fB.rgba.data(), fB.width, fB.height, fB.width * 4,
                                     QImage::Format_RGBA8888).copy();
                if (!layer.secondaryEffects.empty()) {
                    effects::PixelPipeline::applyEffects(rawB.bits(), rawB.width(), rawB.height(),
                                                         static_cast<int>(rawB.bytesPerLine()), layer.secondaryEffects);
                }
                const QSize fitB =
                    QSize(fB.width, fB.height).scaled(QSize(w, h), Qt::KeepAspectRatio);
                QPainter pb(&imgB);
                pb.drawImage(QRect(QPoint((w - fitB.width()) / 2, (h - fitB.height()) / 2),
                                   fitB),
                             rawB);
                pb.end();
            }

            QImage blended(w, h, QImage::Format_RGBA8888);
            blended.fill(Qt::transparent);
            QPainter bp(&blended);

            const double bf = std::clamp(layer.blendFactor, 0.0, 1.0);
            if (layer.transitionType == "crossfade") {
                bp.setOpacity(1.0 - bf);
                bp.drawImage(0, 0, imgA);
                bp.setOpacity(bf);
                bp.drawImage(0, 0, imgB);
            } else if (layer.transitionType == "dip_black") {
                if (bf < 0.5) {
                    bp.setOpacity(1.0 - 2.0 * bf);
                    bp.drawImage(0, 0, imgA);
                } else {
                    bp.setOpacity(2.0 * (bf - 0.5));
                    bp.drawImage(0, 0, imgB);
                }
            } else if (layer.transitionType == "dip_white") {
                if (bf < 0.5) {
                    bp.setOpacity(1.0 - 2.0 * bf);
                    bp.drawImage(0, 0, imgA);
                    bp.setOpacity(2.0 * bf);
                    bp.fillRect(0, 0, w, h, Qt::white);
                } else {
                    bp.setOpacity(2.0 * (1.0 - bf));
                    bp.fillRect(0, 0, w, h, Qt::white);
                    bp.setOpacity(2.0 * (bf - 0.5));
                    bp.drawImage(0, 0, imgB);
                }
            } else if (layer.transitionType == "wipe_left") {
                bp.drawImage(0, 0, imgA);
                int splitX = static_cast<int>(w * (1.0 - bf));
                bp.setClipRect(splitX, 0, w - splitX, h);
                bp.drawImage(0, 0, imgB);
            } else if (layer.transitionType == "wipe_right") {
                bp.drawImage(0, 0, imgA);
                int splitX = static_cast<int>(w * bf);
                bp.setClipRect(0, 0, splitX, h);
                bp.drawImage(0, 0, imgB);
            } else if (layer.transitionType == "wipe_up") {
                bp.drawImage(0, 0, imgA);
                int splitY = static_cast<int>(h * (1.0 - bf));
                bp.setClipRect(0, splitY, w, h - splitY);
                bp.drawImage(0, 0, imgB);
            } else if (layer.transitionType == "wipe_down") {
                bp.drawImage(0, 0, imgA);
                int splitY = static_cast<int>(h * bf);
                bp.setClipRect(0, 0, w, splitY);
                bp.drawImage(0, 0, imgB);
            } else {
                bp.setOpacity(1.0 - bf);
                bp.drawImage(0, 0, imgA);
                bp.setOpacity(bf);
                bp.drawImage(0, 0, imgB);
            }
            bp.end();

            painter.save();
            painter.setOpacity(std::clamp(layer.opacity, 0.0, 1.0));
            painter.translate(w / 2.0 + layer.transform.x, h / 2.0 + layer.transform.y);
            painter.scale(layer.transform.scale, layer.transform.scale);
            painter.rotate(layer.transform.rotationDeg);
            painter.drawImage(QRect(-w / 2, -h / 2, w, h), blended);
            painter.restore();

        } else {
            auto opt = getDecodedFrame(layer.assetId, layer.sourceTime);
            if (opt) {
                const auto& f = *opt;
                QImage raw = QImage(f.rgba.data(), f.width, f.height, f.width * 4,
                                    QImage::Format_RGBA8888).copy();
                if (!layer.effects.empty()) {
                    effects::PixelPipeline::applyEffects(raw.bits(), raw.width(), raw.height(),
                                                         static_cast<int>(raw.bytesPerLine()), layer.effects);
                }
                const QSize fitted =
                    QSize(f.width, f.height).scaled(QSize(w, h), Qt::KeepAspectRatio);

                painter.save();
                painter.setOpacity(std::clamp(layer.opacity, 0.0, 1.0));
                painter.translate(w / 2.0 + layer.transform.x, h / 2.0 + layer.transform.y);
                painter.scale(layer.transform.scale, layer.transform.scale);
                painter.rotate(layer.transform.rotationDeg);
                painter.drawImage(QRect(-fitted.width() / 2, -fitted.height() / 2,
                                        fitted.width(), fitted.height()),
                                  raw);
                painter.restore();
            }
        }
    }

    // Titles: centered plus clip offset, pixel-size font
    painter.setPen(Qt::white);
    for (const auto& layer : plan.layers) {
        const auto cit = seq.clips.find(layer.clipId);
        if (cit == seq.clips.end() || cit->second.text.empty()) {
            continue;
        }
        const core::Clip& c = cit->second;
        painter.save();
        painter.setOpacity(std::clamp(c.opacity, 0.0, 1.0));
        QFont font(QString::fromStdString(c.fontFamily.empty() ? "Arial" : c.fontFamily));
        font.setPixelSize((std::max)(8, static_cast<int>(c.fontSizePt)));
        painter.setFont(font);
        const QRect bounds = painter.boundingRect(canvas.rect(), Qt::AlignCenter,
                                                  QString::fromStdString(c.text));
        QPoint at((w - bounds.width()) / 2 + static_cast<int>(c.transform.x),
                  (h - bounds.height()) / 2 + static_cast<int>(c.transform.y));
        painter.drawText(QRect(at, bounds.size()), Qt::AlignCenter,
                         QString::fromStdString(c.text));
        painter.restore();
    }
    painter.end();
    return canvas;
}

} // namespace editor::shell
