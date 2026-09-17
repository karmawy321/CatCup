#include "shell/TimelineModel.hpp"
#include "shell/Session.hpp"

#include "media_ffmpeg/FrameDecoder.hpp"
#include "render/Evaluator.hpp"

#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>
#include <filesystem>

namespace editor::shell {

class PeakDecodeTask final : public QRunnable {
public:
    PeakDecodeTask(QPointer<TimelineModel> model, std::string mediaKey, std::string assetId, std::string path)
        : model_(model), mediaKey_(std::move(mediaKey)), assetId_(std::move(assetId)), path_(std::move(path)) {
        setAutoDelete(true);
    }

    void run() override {
        if (!model_) return;
        media_ffmpeg::AudioDecoder dec;
        if (dec.open(path_).isErr() || !dec.hasAudio()) {
            if (model_) {
                QMetaObject::invokeMethod(model_.data(), [model = model_, key = mediaKey_, aid = assetId_]() {
                    if (model) model->onPeaksDecoded(key, aid, {});
                }, Qt::QueuedConnection);
            }
            return;
        }
        constexpr int kSamplesPerBucket = 2400; // 0.05s at 48kHz
        std::vector<float> peaks;
        int curBucketCount = 0;
        float curBucketMax = 0.0f;
        while (true) {
            if (!model_) return;
            auto res = dec.nextChunk(4096);
            if (res.isErr()) break;
            const auto& chunk = res.value();
            if (chunk.pcm.empty()) break;
            for (size_t i = 0; i < chunk.pcm.size(); i += 2) {
                float val = std::abs(static_cast<float>(chunk.pcm[i]) / 32768.0f);
                if (i + 1 < chunk.pcm.size()) {
                    val = (std::max)(val, std::abs(static_cast<float>(chunk.pcm[i + 1]) / 32768.0f));
                }
                if (val > curBucketMax) curBucketMax = val;
                curBucketCount++;
                if (curBucketCount >= kSamplesPerBucket) {
                    peaks.push_back(curBucketMax);
                    curBucketMax = 0.0f;
                    curBucketCount = 0;
                }
            }
        }
        if (curBucketCount > 0) {
            peaks.push_back(curBucketMax);
        }
        if (model_) {
            QMetaObject::invokeMethod(model_.data(), [model = model_, key = mediaKey_, aid = assetId_, p = std::move(peaks)]() mutable {
                if (model) model->onPeaksDecoded(key, aid, std::move(p));
            }, Qt::QueuedConnection);
        }
    }

private:
    QPointer<TimelineModel> model_;
    std::string mediaKey_;
    std::string assetId_;
    std::string path_;
};

TimelineModel::TimelineModel(QObject* parent) : QAbstractListModel(parent) {}

TimelineModel::~TimelineModel() = default;

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
                tm["muted"] = track.muted;
                tm["locked"] = track.locked;
                tm["visible"] = track.visible;
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
    QString trackId = "";
    for (const auto& track : seq->tracks) {
        for (const auto& id : track.clipIds) {
            if (id == clip->id) {
                trackId = QString::fromStdString(track.id);
                kind = track.kind == core::TrackKind::Audio
                           ? "audio"
                           : (track.kind == core::TrackKind::Text ? "text" : "video");
            }
        }
    }
    m["clipId"] = clipId;
    m["trackId"] = trackId;
    m["assetId"] = QString::fromStdString(clip->assetId);
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
    QString blendMode = "normal";
    double highlights = 0.0, shadows = 0.0;
    double liftY = 0.0, gammaY = 1.0, gainY = 1.0;
    double offsetR = 0.0, offsetG = 0.0, offsetB = 0.0, lumaMix = 1.0;
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
        } else if (e.type == "blend_mode") {
            auto it = e.strParams.find("mode"); if (it != e.strParams.end()) blendMode = QString::fromStdString(it->second);
        } else if (e.type == "highlights_shadows") {
            auto itH = e.params.find("highlights"); if (itH != e.params.end()) highlights = itH->second;
            auto itS = e.params.find("shadows"); if (itS != e.params.end()) shadows = itS->second;
        } else if (e.type == "color_wheels") {
            auto it = e.params.find("liftY"); if (it != e.params.end()) liftY = it->second;
            it = e.params.find("gammaY"); if (it != e.params.end()) gammaY = it->second;
            it = e.params.find("gainY"); if (it != e.params.end()) gainY = it->second;
            it = e.params.find("offsetR"); if (it != e.params.end()) offsetR = it->second;
            it = e.params.find("offsetG"); if (it != e.params.end()) offsetG = it->second;
            it = e.params.find("offsetB"); if (it != e.params.end()) offsetB = it->second;
            it = e.params.find("lumaMix"); if (it != e.params.end()) lumaMix = it->second;
        }
    }
    m["effects"] = effs;
    m["brightness"] = brightness;
    m["contrast"] = contrast;
    m["saturation"] = saturation;
    m["temperature"] = temp;
    m["tint"] = tint;
    m["blendMode"] = blendMode;
    m["highlights"] = highlights;
    m["shadows"] = shadows;
    m["liftY"] = liftY;
    m["gammaY"] = gammaY;
    m["gainY"] = gainY;
    m["offsetR"] = offsetR;
    m["offsetG"] = offsetG;
    m["offsetB"] = offsetB;
    m["lumaMix"] = lumaMix;
    m["fadeInSec"] = clip->fadeInSec;
    m["fadeOutSec"] = clip->fadeOutSec;
    m["keyframeCount"] = static_cast<int>(clip->keyframes.size());
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

std::string TimelineModel::computeMediaKey(const std::string& assetId, const std::string& path) const {
    std::error_code ec;
    auto fsPath = std::filesystem::path(QDir::toNativeSeparators(QString::fromStdString(path)).toStdWString());
    auto canPath = std::filesystem::weakly_canonical(fsPath, ec);
    if (ec) canPath = fsPath;
    uintmax_t fsize = 0;
    int64_t mtime = 0;
    if (std::filesystem::exists(canPath, ec)) {
        ec.clear();
        fsize = std::filesystem::file_size(canPath, ec);
        ec.clear();
        auto lw = std::filesystem::last_write_time(canPath, ec);
        if (!ec) {
            mtime = lw.time_since_epoch().count();
        }
    }
    return assetId + "#" + canPath.string() + "#" + std::to_string(fsize) + "#" + std::to_string(mtime);
}

void TimelineModel::onPeaksDecoded(const std::string& mediaKey, const std::string& assetId, std::vector<float> peaks) {
    {
        std::lock_guard<std::mutex> lock(peakMutex_);
        peakCache_[mediaKey] = std::move(peaks);
        inFlightKeys_.erase(mediaKey);
    }
    peaksVersion_++;
    emit peaksVersionChanged();
    emit audioPeaksReady(QString::fromStdString(assetId));
}

QVariantList TimelineModel::clipAudioPeaks(const QString& clipId, int barCount) const {
    QVariantList list;
    if (barCount <= 0) {
        return list;
    }
    list.reserve(barCount);
    if (session_ == nullptr) {
        for (int i = 0; i < barCount; ++i) list.push_back(0.0);
        return list;
    }
    const core::Sequence* seq = session_->project().activeSequence();
    if (seq == nullptr) {
        for (int i = 0; i < barCount; ++i) list.push_back(0.0);
        return list;
    }
    const core::Clip* clip = seq->findClip(clipId.toStdString());
    if (clip == nullptr || clip->assetId.empty()) {
        for (int i = 0; i < barCount; ++i) list.push_back(0.0);
        return list;
    }
    const auto ait = session_->project().assets.find(clip->assetId);
    if (ait == session_->project().assets.end()) {
        for (int i = 0; i < barCount; ++i) list.push_back(0.0);
        return list;
    }
    const core::Asset& asset = ait->second;
    if (asset.kind == core::AssetKind::Image) {
        for (int i = 0; i < barCount; ++i) list.push_back(0.0);
        return list;
    }

    const std::string mediaKey = computeMediaKey(clip->assetId, asset.path);
    std::vector<float> peaksCopy;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(peakMutex_);
        auto it = peakCache_.find(mediaKey);
        if (it != peakCache_.end()) {
            peaksCopy = it->second;
            found = true;
        } else {
            if (inFlightKeys_.find(mediaKey) == inFlightKeys_.end()) {
                inFlightKeys_.insert(mediaKey);
                auto* task = new PeakDecodeTask(const_cast<TimelineModel*>(this), mediaKey, clip->assetId, asset.path);
                QThreadPool::globalInstance()->start(task);
            }
        }
    }

    if (!found || peaksCopy.empty()) {
        for (int i = 0; i < barCount; ++i) list.push_back(0.0);
        return list;
    }

    const double sourceIn = static_cast<double>(clip->sourceIn);
    const double duration = static_cast<double>(clip->seqDuration());
    const double speed = static_cast<double>(clip->speed);
    constexpr double kSecPerBucket = 0.05;

    for (int i = 0; i < barCount; ++i) {
        double barStartFrac = static_cast<double>(i) / static_cast<double>(barCount);
        double barEndFrac = static_cast<double>(i + 1) / static_cast<double>(barCount);
        double t0 = sourceIn + (barStartFrac * duration * speed);
        double t1 = sourceIn + (barEndFrac * duration * speed);
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        if (t1 < 0.0) {
            list.push_back(0.0);
            continue;
        }
        if (t0 < 0.0) {
            t0 = 0.0;
        }
        const size_t bStart = static_cast<size_t>(std::floor(t0 / kSecPerBucket));
        const size_t bEnd = (t1 > t0)
                                ? static_cast<size_t>(std::floor((t1 - 1e-9) / kSecPerBucket))
                                : bStart;
        if (bStart >= peaksCopy.size()) {
            list.push_back(0.0);
            continue;
        }
        float barMax = 0.0f;
        for (size_t b = bStart; b <= bEnd && b < peaksCopy.size(); ++b) {
            barMax = (std::max)(barMax, peaksCopy[b]);
        }
        list.push_back(static_cast<double>(barMax));
    }
    return list;
}

} // namespace editor::shell
