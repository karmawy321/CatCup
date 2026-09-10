#include "shell/TimelineModel.hpp"
#include "shell/Session.hpp"

#include "render/Evaluator.hpp"

namespace editor::shell {

TimelineModel::TimelineModel(QObject* parent) : QAbstractListModel(parent) {}

void TimelineModel::setSession(Session* session) {
    session_ = session;
    if (session_ != nullptr) {
        connect(session_, &Session::projectChanged, this, &TimelineModel::refresh);
    }
    refresh();
}

int TimelineModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant TimelineModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= static_cast<int>(rows_.size())) {
        return {};
    }
    const Row& r = rows_[static_cast<size_t>(index.row())];
    switch (role) {
    case ClipIdRole: return r.clipId;
    case TrackIdRole: return r.trackId;
    case TrackKindRole: return r.trackKind;
    case NameRole: return r.name;
    case StartSecRole: return r.startSec;
    case DurationSecRole: return r.durationSec;
    case DisplayTextRole: return r.displayText;
    default: return {};
    }
}

QHash<int, QByteArray> TimelineModel::roleNames() const {
    return {
        {ClipIdRole, "clipId"},
        {TrackIdRole, "trackId"},
        {TrackKindRole, "trackKind"},
        {NameRole, "clipName"},
        {StartSecRole, "startSec"},
        {DurationSecRole, "durationSec"},
        {DisplayTextRole, "displayText"},
    };
}

double TimelineModel::durationSec() const {
    return durationSec_;
}

double TimelineModel::fps() const {
    return fps_;
}

QVariantList TimelineModel::tracks() const {
    return tracks_;
}

void TimelineModel::refresh() {
    beginResetModel();
    rows_.clear();
    tracks_.clear();
    durationSec_ = 0.0;
    fps_ = 30.0;
    if (session_ != nullptr) {
        if (const core::Sequence* seq = session_->project().activeSequence()) {
            fps_ = static_cast<double>(seq->fps);
            durationSec_ = static_cast<double>(render::Evaluator::sequenceDuration(*seq));
            for (const auto& track : seq->tracks) {
                QVariantMap tm;
                tm["trackId"] = QString::fromStdString(track.id);
                tm["kind"] = track.kind == core::TrackKind::Video
                                 ? "video"
                                 : (track.kind == core::TrackKind::Audio ? "audio" : "text");
                tm["name"] = QString::fromStdString(track.name);
                tracks_.push_back(tm);
                for (const auto& clipId : track.clipIds) {
                    const auto it = seq->clips.find(clipId);
                    if (it == seq->clips.end()) {
                        continue;
                    }
                    const core::Clip& c = it->second;
                    Row r;
                    r.clipId = QString::fromStdString(c.id);
                    r.trackId = QString::fromStdString(track.id);
                    r.trackKind = tm["kind"].toString();
                    r.name = QString::fromStdString(c.name);
                    r.startSec = static_cast<double>(c.seqStart);
                    r.durationSec = static_cast<double>(c.seqDuration());
                    r.displayText = !c.text.empty() ? QString::fromStdString(c.text) : r.name;
                    rows_.push_back(std::move(r));
                }
            }
        }
    }
    endResetModel();
    emit modelChanged();
}

int TimelineModel::rowForClipId(const QString& clipId) const {
    for (size_t i = 0; i < rows_.size(); ++i) {
        if (rows_[i].clipId == clipId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

QVariantMap TimelineModel::clipInfo(const QString& clipId) const {
    QVariantMap m;
    if (session_ == nullptr) {
        return m;
    }
    const core::Sequence* seq = session_->project().activeSequence();
    if (seq == nullptr) {
        return m;
    }
    const core::Clip* clip = seq->findClip(clipId.toStdString());
    if (clip == nullptr) {
        return m;
    }
    QString kind = "video";
    for (const auto& track : seq->tracks) {
        for (const auto& id : track.clipIds) {
            if (id == clip->id) {
                kind = track.kind == core::TrackKind::Audio
                           ? "audio"
                           : (track.kind == core::TrackKind::Text ? "text" : "video");
            }
        }
    }
    m["clipId"] = clipId;
    m["name"] = QString::fromStdString(clip->name);
    m["kind"] = kind;
    m["startSec"] = static_cast<double>(clip->seqStart);
    m["durationSec"] = static_cast<double>(clip->seqDuration());
    m["sourceInSec"] = static_cast<double>(clip->sourceIn);
    m["text"] = QString::fromStdString(clip->text);
    m["fontFamily"] = QString::fromStdString(clip->fontFamily);
    m["fontSizePt"] = clip->fontSizePt;
    m["scale"] = clip->transform.scale;
    m["x"] = clip->transform.x;
    m["y"] = clip->transform.y;
    m["rotation"] = clip->transform.rotationDeg;
    return m;
}

} // namespace editor::shell
