#include "shell/Session.hpp"

#include "commands/ClipCommands.hpp"
#include "commands/TransitionCommands.hpp"
#include "core/Ids.hpp"
#include "media_ffmpeg/FfmpegProber.hpp"
#include "persist/ProjectSerializer.hpp"

#include <QFileInfo>
#include <QUrl>
#include <cmath>

namespace editor::shell {

core::Rational rationalFromSeconds(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) {
        return core::Rational(0);
    }
    const auto micros = static_cast<std::int64_t>(std::llround(seconds * 1000000.0));
    return core::Rational(micros, 1000000);
}

Session::Session(QObject* parent) : QObject(parent), undo_(100, &bus_) {
    newProject();
}

QString Session::projectName() const {
    return QString::fromStdString(project_.name);
}

bool Session::canUndo() const {
    return undo_.canUndo();
}

bool Session::canRedo() const {
    return undo_.canRedo();
}

void Session::newProject() {
    project_ = core::Project{};
    project_.name = "Untitled";
    core::Sequence seq;
    seq.id = core::IdGenerator::make("seq");
    seq.name = "Sequence 01";
    seq.fps = core::Rational(30, 1);
    seq.width = 1280;
    seq.height = 720;
    project_.sequences.push_back(std::move(seq));
    project_.activeSequenceId = project_.sequences.front().id;
    ensureTimelineTracks(project_.sequences.front());
    undo_.clear();
    filePath_.clear();
    dirty_ = false;
    emit projectChanged();
}

bool Session::openFile(const QUrl& url) {
    const QString local = url.toLocalFile();
    auto loaded = persist::loadProject(local.toStdString());
    if (loaded.isErr()) {
        emit error(QString::fromStdString(loaded.error()));
        return false;
    }
    project_ = std::move(loaded.value());
    if (core::Sequence* seq = project_.activeSequence()) {
        ensureTimelineTracks(*seq);
    }
    undo_.clear();
    filePath_ = local;
    dirty_ = false;
    emit projectChanged();
    return true;
}

bool Session::save() {
    if (filePath_.isEmpty()) {
        emit error("No file name — use Save As");
        return false;
    }
    auto r = persist::saveProject(project_, filePath_.toStdString());
    if (r.isErr()) {
        emit error(QString::fromStdString(r.error()));
        return false;
    }
    dirty_ = false;
    emit projectChanged();
    return true;
}

bool Session::saveAs(const QUrl& url) {
    filePath_ = url.toLocalFile();
    return save();
}

QString Session::importMedia(const QUrl& url) {
    const QString local = url.toLocalFile();
    media_ffmpeg::FfmpegProber prober;
    auto probed = prober.probe(local.toStdString());
    if (probed.isErr()) {
        emit error(QString::fromStdString(probed.error()));
        return {};
    }
    const media::ProbeResult& pr = probed.value();
    core::Asset asset;
    asset.id = core::IdGenerator::make("asset");
    asset.path = local.toStdString();
    asset.duration = pr.duration.num() > 0 ? pr.duration : core::Rational(4, 1);
    asset.fps = pr.fps.num() > 0 ? pr.fps : core::Rational(30, 1);
    asset.width = pr.width;
    asset.height = pr.height;
    if (pr.hasVideo) {
        asset.kind = core::AssetKind::Video;
    } else if (pr.hasAudio) {
        asset.kind = core::AssetKind::Audio;
    } else {
        asset.kind = core::AssetKind::Image;
    }
    const QString assetId = QString::fromStdString(asset.id);
    project_.assets.emplace(asset.id, std::move(asset));
    dirty_ = true;
    emit projectChanged();
    return assetId;
}

QString Session::addClipToTimeline(const QString& assetId) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) {
        emit error("No active sequence");
        return {};
    }
    const auto ait = project_.assets.find(assetId.toStdString());
    if (ait == project_.assets.end()) {
        emit error("Unknown asset");
        return {};
    }
    const core::Asset& asset = ait->second;
    core::TrackKind want = core::TrackKind::Video;
    if (asset.kind == core::AssetKind::Audio) {
        want = core::TrackKind::Audio;
    }
    core::Track* track = findTrackForKind(*seq, want);
    if (track == nullptr) {
        emit error("No timeline track for this media kind");
        return {};
    }
    core::Clip clip;
    clip.id = core::IdGenerator::make("clip");
    clip.assetId = asset.id;
    clip.name = QFileInfo(QString::fromStdString(asset.path)).completeBaseName().toStdString();
    clip.sourceIn = core::Rational(0);
    clip.sourceOut = asset.duration;
    clip.seqStart = trackEnd(*seq, *track);
    const QString clipId = QString::fromStdString(clip.id);
    if (!execute(commands::makeAddClipCommand(track->id, std::move(clip)))) {
        return {};
    }
    return clipId;
}

QString Session::addTitle(const QString& text) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) {
        emit error("No active sequence");
        return {};
    }
    core::Track* track = findTrackForKind(*seq, core::TrackKind::Text);
    if (track == nullptr) {
        emit error("No title track");
        return {};
    }
    core::Clip title;
    title.id = core::IdGenerator::make("clip");
    title.name = "Title";
    title.sourceIn = core::Rational(0);
    title.sourceOut = core::Rational(3, 1);
    title.seqStart = trackEnd(*seq, *track);
    title.text = text.toStdString();
    title.fontFamily = "Arial";
    title.fontSizePt = 72.0;
    const QString clipId = QString::fromStdString(title.id);
    if (!execute(commands::makeAddClipCommand(track->id, std::move(title)))) {
        return {};
    }
    return clipId;
}

bool Session::splitSelectedAtPlayhead(const QString& clipId, double playheadSec) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr || clipId.isEmpty()) {
        return false;
    }
    const core::Clip* clip = seq->findClip(clipId.toStdString());
    if (clip == nullptr) {
        emit error("Clip not found");
        return false;
    }
    const core::Rational at = rationalFromSeconds(playheadSec);
    if (!(clip->seqStart < at && at < clip->seqEnd())) {
        emit error("Playhead is not inside the selected clip");
        return false;
    }
    return execute(commands::makeSplitClipCommand(
        clip->id, at, core::IdGenerator::make("clip")));
}

void Session::setRippleMode(bool v) {
    if (rippleMode_ != v) {
        rippleMode_ = v;
        emit rippleModeChanged();
    }
}

void Session::setSnappingEnabled(bool v) {
    if (snappingEnabled_ != v) {
        snappingEnabled_ = v;
        emit snappingEnabledChanged();
    }
}

double Session::snapTime(double targetSec, double thresholdSec) const {
    if (!snappingEnabled_) {
        return targetSec;
    }
    const core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) {
        return targetSec;
    }
    double bestSnap = targetSec;
    double bestDist = thresholdSec;

    if (std::abs(targetSec) < bestDist) {
        bestDist = std::abs(targetSec);
        bestSnap = 0.0;
    }

    for (const auto& [id, clip] : seq->clips) {
        const double start = static_cast<double>(clip.seqStart);
        const double end = static_cast<double>(clip.seqEnd());
        const double distStart = std::abs(targetSec - start);
        if (distStart < bestDist) {
            bestDist = distStart;
            bestSnap = start;
        }
        const double distEnd = std::abs(targetSec - end);
        if (distEnd < bestDist) {
            bestDist = distEnd;
            bestSnap = end;
        }
    }
    return bestSnap;
}

bool Session::deleteClip(const QString& clipId) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr || clipId.isEmpty()) {
        return false;
    }
    for (const auto& track : seq->tracks) {
        for (const auto& id : track.clipIds) {
            if (id == clipId.toStdString()) {
                if (track.locked) {
                    emit error("Track is locked");
                    return false;
                }
                if (rippleMode_) {
                    return execute(commands::makeRippleDeleteClipCommand(track.id, id));
                }
                return execute(commands::makeRemoveClipCommand(track.id, id));
            }
        }
    }
    emit error("Clip is not on any track");
    return false;
}

bool Session::moveClipTo(const QString& clipId, double newStartSec) {
    if (clipId.isEmpty()) {
        return false;
    }
    return execute(commands::makeMoveClipCommand(clipId.toStdString(),
                                                 rationalFromSeconds(newStartSec)));
}

bool Session::trimClip(const QString& clipId, double newInSec, double newOutSec,
                       double newStartSec) {
    if (clipId.isEmpty()) {
        return false;
    }
    if (rippleMode_) {
        return execute(commands::makeRippleTrimClipCommand(
            clipId.toStdString(), rationalFromSeconds(newInSec), rationalFromSeconds(newOutSec),
            rationalFromSeconds(newStartSec)));
    }
    return execute(commands::makeTrimClipCommand(
        clipId.toStdString(), rationalFromSeconds(newInSec), rationalFromSeconds(newOutSec),
        rationalFromSeconds(newStartSec)));
}

bool Session::setClipOpacity(const QString& clipId, double opacity) {
    if (clipId.isEmpty()) {
        return false;
    }
    return execute(commands::makeSetOpacityCommand(clipId.toStdString(), opacity));
}

QString Session::addTransition(const QString& trackId, const QString& fromClipId,
                               const QString& toClipId, const QString& type,
                               double durationSec, int alignment) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) {
        emit error("No active sequence");
        return {};
    }
    std::string tid = trackId.toStdString();
    if (tid.empty() && !fromClipId.isEmpty()) {
        for (const auto& t : seq->tracks) {
            for (const auto& cid : t.clipIds) {
                if (cid == fromClipId.toStdString()) {
                    tid = t.id;
                    break;
                }
            }
            if (!tid.empty()) break;
        }
    }
    if (tid.empty()) {
        emit error("Could not determine track for transition");
        return {};
    }
    core::Transition t;
    t.id = core::IdGenerator::make("trans");
    t.trackId = tid;
    t.fromClipId = fromClipId.toStdString();
    t.toClipId = toClipId.toStdString();
    t.type = type.isEmpty() ? "crossfade" : type.toStdString();
    t.duration = rationalFromSeconds(durationSec);
    if (alignment == 1) {
        t.alignment = core::TransitionAlignment::StartOnCut;
    } else if (alignment == 2) {
        t.alignment = core::TransitionAlignment::EndOnCut;
    } else {
        t.alignment = core::TransitionAlignment::CenterOnCut;
    }
    t.easing = "linear";

    const QString transId = QString::fromStdString(t.id);
    if (!execute(commands::makeAddTransitionCommand(std::move(t)))) {
        return {};
    }
    return transId;
}

bool Session::removeTransition(const QString& transitionId) {
    if (transitionId.isEmpty()) {
        return false;
    }
    return execute(commands::makeRemoveTransitionCommand(transitionId.toStdString()));
}

bool Session::updateTransition(const QString& transitionId, double durationSec,
                               int alignment, const QString& type, const QString& easing) {
    if (transitionId.isEmpty()) {
        return false;
    }
    core::TransitionAlignment align = core::TransitionAlignment::CenterOnCut;
    if (alignment == 1) {
        align = core::TransitionAlignment::StartOnCut;
    } else if (alignment == 2) {
        align = core::TransitionAlignment::EndOnCut;
    }

    return execute(commands::makeUpdateTransitionCommand(
        transitionId.toStdString(),
        rationalFromSeconds(durationSec),
        align,
        type.toStdString(),
        easing.toStdString()));
}

bool Session::setClipTransform(const QString& clipId, double scale, double x, double y,
                               double rotationDeg) {
    if (clipId.isEmpty()) {
        return false;
    }
    core::Transform tr;
    tr.scale = scale;
    tr.x = x;
    tr.y = y;
    tr.rotationDeg = rotationDeg;
    return execute(commands::makeSetTransformCommand(clipId.toStdString(), tr));
}

bool Session::setClipText(const QString& clipId, const QString& text,
                          const QString& fontFamily, double fontSizePt) {
    if (clipId.isEmpty()) {
        return false;
    }
    return execute(commands::makeSetTextCommand(clipId.toStdString(), text.toStdString(),
                                                fontFamily.toStdString(), fontSizePt));
}

void Session::undo() {
    if (undo_.undo(project_)) {
        dirty_ = true;
        emit projectChanged();
    }
}

void Session::redo() {
    std::string err;
    if (undo_.redo(project_, err)) {
        dirty_ = true;
        emit projectChanged();
    } else if (!err.empty()) {
        emit error(QString::fromStdString(err));
    }
}

bool Session::execute(std::unique_ptr<commands::ICommand> cmd) {
    std::string err;
    if (!undo_.execute(std::move(cmd), project_, err)) {
        emit error(QString::fromStdString(err));
        return false;
    }
    if (auto r = project_.validate(); r.isErr()) {
        // Defensive: commands validate preconditions, so this is unreachable
        // in practice — but never let the document go invalid silently.
        undo_.undo(project_);
        emit error("Edit rejected: " + QString::fromStdString(r.error()));
        return false;
    }
    dirty_ = true;
    emit projectChanged();
    return true;
}

void Session::ensureTimelineTracks(core::Sequence& seq) {
    const auto has = [&](core::TrackKind kind) {
        for (const auto& t : seq.tracks) {
            if (t.kind == kind) {
                return true;
            }
        }
        return false;
    };
    const auto add = [&](core::TrackKind kind, const char* name) {
        core::Track t;
        t.id = core::IdGenerator::make("track");
        t.kind = kind;
        t.name = name;
        seq.tracks.push_back(std::move(t));
    };
    if (!has(core::TrackKind::Video)) {
        add(core::TrackKind::Video, "V1");
    }
    if (!has(core::TrackKind::Audio)) {
        add(core::TrackKind::Audio, "A1");
    }
    if (!has(core::TrackKind::Text)) {
        add(core::TrackKind::Text, "T1");
    }
}

core::Track* Session::findTrackForKind(core::Sequence& seq, core::TrackKind kind) {
    for (auto& t : seq.tracks) {
        if (t.kind == kind) {
            return &t;
        }
    }
    return nullptr;
}

core::Rational Session::trackEnd(const core::Sequence& seq, const core::Track& track) const {
    core::Rational end{0};
    for (const auto& id : track.clipIds) {
        const auto it = seq.clips.find(id);
        if (it != seq.clips.end() && end < it->second.seqEnd()) {
            end = it->second.seqEnd();
        }
    }
    return end;
}

} // namespace editor::shell
