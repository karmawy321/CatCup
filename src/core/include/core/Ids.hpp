#pragma once

// Stable string identities. Never use vector indices or UI object addresses
// as persistent references — IDs survive save/load and undo/redo.

#include <atomic>
#include <string>

namespace editor::core {

using Id = std::string;

class IdGenerator {
public:
    static Id make(const std::string& prefix);
    /// Deterministic IDs for tests. Not for production use.
    static void resetForTests();
    static void setCounterForTests(unsigned long long value);

private:
    static std::atomic<unsigned long long> counter_;
};

} // namespace editor::core
