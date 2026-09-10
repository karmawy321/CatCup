#include "core/ChangeBus.hpp"

namespace editor::core {

ChangeBus::Token ChangeBus::subscribe(Callback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    const Token token = nextToken_++;
    subscribers_.emplace_back(token, std::move(cb));
    return token;
}

void ChangeBus::unsubscribe(Token token) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = subscribers_.begin(); it != subscribers_.end(); ++it) {
        if (it->first == token) {
            subscribers_.erase(it);
            return;
        }
    }
}

void ChangeBus::publish(const ProjectEvent& event) {
    std::vector<Callback> copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        copy.reserve(subscribers_.size());
        for (const auto& [token, cb] : subscribers_) {
            (void)token;
            copy.push_back(cb);
        }
    }
    for (const auto& cb : copy) {
        cb(event);
    }
}

} // namespace editor::core
