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
    transitions_.clear();
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
            for (const auto& tr : seq->transitions) {
                QVariantMap trm;
                trm["id"] = QString::fromStdString(tr.id);
                trm["trackId"] = QString::fromStdString(tr.trackId);
                trm["fromClipId"] = QString::fromStdString(tr.fromClipId);
                trm["toClipId"] = QString::fromStdString(tr.toClipId);
                trm["type"] = QString::fromStdString(tr.type);
                trm["durationSec"] = static_cast<double>(tr.duration);
                trm["alignment"] = QString::fromStdString(core::toString(tr.alignment));
                trm["easing"] = QString::fromStdString(tr.easing);
                const auto* from = seq->findClip(tr.fromClipId);
                const auto* to = seq->findClip(tr.toClipId);
                if (from != nullptr && to != nullptr) {
                    const auto range = tr.timeRange(*from, *to);
                    trm["startSec"] = static_cast<double>(range.start);
                    trm["rangeDurationSec"] = static_cast<double>(range.duration);
                } else {
                    trm["startSec"] = 0.0;
                    trm["rangeDurationSec"] = static_cast<double>(tr.duration);
                }
                transitions_.push_back(trm);
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
    m["opacity"] = clip->opacity;
    m["speed"] = static_cast<double>(clip->speed);
    m["text"] = QString::fromStdString(clip->text);
    m["fontFamily"] = QString::fromStdString(clip->fontFamily);
    m["fontSizePt"] = clip->fontSizePt;
    m["scale"] = clip->transform.scale;
    m["x"] = clip->transform.x;
    m["y"] = clip->transform.y;
    m["rotation"] = clip->transform.rotationDeg;

    QVariantList effs;
    double brightness = 0.0, contrast = 1.0, saturation = 1.0, temp = 0.0, tint = 0.0;
    for (const auto& e : clip->effects) {
        QVariantMap em;
        em["type"] = QString::fromStdString(e.type);
        em["enabled"] = e.enabled;
        em["order"] = e.order;
        QVariantMap pm;
        for (const auto& [k, v] : e.params) {
            pm[QString::fromStdString(k)] = v;
        }
        em["params"] = pm;
        QVariantMap spm;
        for (const auto& [k, v] : e.strParams) {
            spm[QString::fromStdString(k)] = QString::fromStdString(v);
        }
        em["strParams"] = spm;
        effs.push_back(em);

        if (e.type == "color_adjust") {
            auto it = e.params.find("brightness"); if (it != e.params.end()) brightness = it->second;
            it = e.params.find("contrast"); if (it != e.params.end()) contrast = it->second;
            it = e.params.find("saturation"); if (it != e.params.end()) saturation = it->second;
            it = e.params.find("temperature"); if (it != e.params.end()) temp = it->second;
            it = e.params.find("tint"); if (it != e.params.end()) tint = it->second;
        }
    }
    m["effects"] = effs;
    m["brightness"] = brightness;
    m["contrast"] = contrast;
    m["saturation"] = saturation;
    m["temperature"] = temp;
    m["tint"] = tint;
    return m;
}

QVariantMap TimelineModel::transitionInfo(const QString& transId) const {
    QVariantMap m;
    if (session_ == nullptr) {
        return m;
    }
    const core::Sequence* seq = session_->project().activeSequence();
    if (seq == nullptr) {
        return m;
    }
    const core::Transition* tr = seq->findTransition(transId.toStdString());
    if (tr == nullptr) {
        return m;
    }
    m["id"] = transId;
    m["trackId"] = QString::fromStdString(tr->trackId);
    m["fromClipId"] = QString::fromStdString(tr->fromClipId);
    m["toClipId"] = QString::fromStdString(tr->toClipId);
    m["type"] = QString::fromStdString(tr->type);
    m["durationSec"] = static_cast<double>(tr->duration);
    m["alignment"] = QString::fromStdString(core::toString(tr->alignment));
    m["easing"] = QString::fromStdString(tr->easing);
    const auto* from = seq->findClip(tr->fromClipId);
    const auto* to = seq->findClip(tr->toClipId);
    if (from != nullptr && to != nullptr) {
        const auto range = tr->timeRange(*from, *to);
        m["startSec"] = static_cast<double>(range.start);
        m["rangeDurationSec"] = static_cast<double>(range.duration);
    }
    return m;
}

QString TimelineModel::adjacentClipId(const QString& clipId, bool next) const {
    if (session_ == nullptr) return {};
    const core::Sequence* seq = session_->project().activeSequence();
    if (seq == nullptr) return {};
    const std::string cid = clipId.toStdString();
    for (const auto& track : seq->tracks) {
        for (size_t i = 0; i < track.clipIds.size(); ++i) {
            if (track.clipIds[i] == cid) {
                if (next && i + 1 < track.clipIds.size()) {
                    return QString::fromStdString(track.clipIds[i + 1]);
                } else if (!next && i > 0) {
                    return QString::fromStdString(track.clipIds[i - 1]);
                }
                return {};
            }
        }
    }
    return {};
}

} // namespace editor::shell
