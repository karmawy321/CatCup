#include "shell/Selection.hpp"

namespace editor::shell {

Selection::Selection(QObject* parent) : QObject(parent) {}

void Selection::select(const QString& clipId) {
    setSelectedTransitionId({});
    setSelectedClipId(clipId);
}

void Selection::selectTransition(const QString& transId) {
    setSelectedClipId({});
    setSelectedTransitionId(transId);
}

void Selection::clearSelection() {
    setSelectedClipId({});
    setSelectedTransitionId({});
}

void Selection::setSelectedClipId(const QString& id) {
    if (selectedClipId_ != id) {
        selectedClipId_ = id;
        emit selectionChanged();
    }
}

void Selection::setSelectedTransitionId(const QString& id) {
    if (selectedTransitionId_ != id) {
        selectedTransitionId_ = id;
        emit transitionSelectionChanged();
    }
}

void Selection::setPlayheadSec(double sec) {
    if (sec < 0) {
        sec = 0;
    }
    if (playheadSec_ != sec) {
        playheadSec_ = sec;
        emit playheadChanged();
    }
}

void Selection::setPxPerSec(double px) {
    if (px < 4) {
        px = 4;
    }
    if (px > 800) {
        px = 800;
    }
    if (pxPerSec_ != px) {
        pxPerSec_ = px;
        emit zoomChanged();
    }
}

void Selection::zoomIn() {
    setPxPerSec(pxPerSec_ * 1.25);
}

void Selection::zoomOut() {
    setPxPerSec(pxPerSec_ / 1.25);
}

} // namespace editor::shell
