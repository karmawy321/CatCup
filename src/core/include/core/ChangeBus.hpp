#pragma once

// Observer bus for project mutations. The undo stack and persistence layer
// publish; the future QML layer subscribes. Payloads are plain data so the
// UI thread can marshal them safely.

#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace editor::core {

struct ProjectEvent {
    enum class Type {
        AssetAdded,
        AssetRemoved,
        ClipAdded,
        ClipRemoved,
        ClipModified,
        TrackModified,
        SequenceModified,
        ProjectReloaded,
        UndoPerformed,
        RedoPerformed,
    };
    Type type = Type::ProjectReloaded;
    std::string detail;
};

class ChangeBus {
public:
    using Callback = std::function<void(const ProjectEvent&)>;
    using Token = std::size_t;

    Token subscribe(Callback cb);
    void unsubscribe(Token token);
    void publish(const ProjectEvent& event);

private:
    std::mutex mutex_;
    std::vector<std::pair<Token, Callback>> subscribers_;
    Token nextToken_ = 1;
};

} // namespace editor::core
