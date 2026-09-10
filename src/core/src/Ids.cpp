#include "core/Ids.hpp"

#include <sstream>

namespace editor::core {

std::atomic<unsigned long long> IdGenerator::counter_{1};

Id IdGenerator::make(const std::string& prefix) {
    const unsigned long long n = counter_.fetch_add(1, std::memory_order_relaxed);
    std::ostringstream os;
    os << prefix << '-' << n;
    return os.str();
}

void IdGenerator::resetForTests() {
    counter_.store(1, std::memory_order_relaxed);
}

void IdGenerator::setCounterForTests(unsigned long long value) {
    counter_.store(value, std::memory_order_relaxed);
}

} // namespace editor::core
