#include "shell/PreviewItem.hpp"
#include "shell/Session.hpp"

#include "core/Model.hpp"
#include "shell/FrameCompositor.hpp"

#include <QPainter>

namespace editor::shell {

PreviewItem::PreviewItem(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setPerformanceHint(QQuickPaintedItem::FastFBOResizing);
}

void PreviewItem::setSession(QObject* session) {
    auto* typed = qobject_cast<Session*>(session);
    if (typed == nullptr) {
        return; // wrong object or null: stay session-less, never crash QML
    }
    session_ = typed;
    if (session_ != nullptr) {
        connect(session_, &Session::projectChanged, this, [this] {
            compositor_.invalidate(); // mappings may have changed: never serve stale frames
            renderAt(positionSec_);
        });
    }
}

void PreviewItem::renderAt(double seconds) {
    if (session_ == nullptr) {
        return;
    }
    if (seconds < 0) {
        seconds = 0;
    }
    const core::Project& project = session_->project();
    const core::Sequence* seq = project.activeSequence();
    if (seq == nullptr) {
        return;
    }
    // The member compositor re-seeks automatically when time moves
    // backwards; project mutations invalidate it via projectChanged.
    current_ = compositor_.frameAt(project, *seq, rationalFromSeconds(seconds));
    positionSec_ = seconds;
    update();
    emit frameChanged();
}

void PreviewItem::showImage(const QImage& image, double seconds) {
    if (image.isNull()) {
        return;
    }
    current_ = image;
    positionSec_ = seconds;
    update();
    emit frameChanged();
}

void PreviewItem::paint(QPainter* painter) {
    painter->fillRect(contentsBoundingRect(), Qt::black);
    if (current_.isNull()) {
        painter->setPen(Qt::gray);
        painter->drawText(contentsBoundingRect(), Qt::AlignCenter, "No frame");
        return;
    }
    const QSize fitted = current_.size().scaled(contentsBoundingRect().size().toSize(),
                                                Qt::KeepAspectRatio);
    const QPoint at((static_cast<int>(contentsBoundingRect().width()) - fitted.width()) / 2,
                    (static_cast<int>(contentsBoundingRect().height()) - fitted.height()) / 2);
    painter->drawImage(QRect(at, fitted), current_);
}

} // namespace editor::shell
