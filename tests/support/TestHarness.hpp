#pragma once

// Dependency-free test harness (no Catch2/gtest download needed for Stage 0).
// Each test .cpp defines TEST_CASE functions via the macro and provides its
// own main() that returns the failure count.

#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace editor::tests {

struct Case {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

inline void registerCase(std::string name, std::function<void()> fn) {
    registry().push_back(Case{std::move(name), std::move(fn)});
}

struct Registrar {
    Registrar(std::string name, std::function<void()> fn) {
        registerCase(std::move(name), std::move(fn));
    }
};

inline int runAll() {
    int failed = 0;
    for (const auto& c : registry()) {
        try {
            c.fn();
            std::cout << "PASS " << c.name << "\n";
        } catch (const std::exception& e) {
            ++failed;
            std::cout << "FAIL " << c.name << " : " << e.what() << "\n";
        } catch (...) {
            ++failed;
            std::cout << "FAIL " << c.name << " : unknown exception\n";
        }
    }
    std::cout << (failed == 0 ? "ALL PASS" : "FAILURES") << " (" << registry().size()
              << " cases, " << failed << " failed)\n";
    return failed;
}

} // namespace editor::tests

#define TEST_CONCAT_IMPL(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_IMPL(a, b)
#define TEST_CASE(name)                                                                       \
    static void TEST_CONCAT(test_fn_, __LINE__)();                                            \
    static ::editor::tests::Registrar TEST_CONCAT(registrar_, __LINE__)(                      \
        name, TEST_CONCAT(test_fn_, __LINE__));                                               \
    static void TEST_CONCAT(test_fn_, __LINE__)()

#define CHECK(cond)                                                                           \
    do {                                                                                      \
        if (!(cond)) {                                                                        \
            throw std::runtime_error(std::string("check failed: ") + #cond + " @ " +          \
                                     __FILE__ + ":" + std::to_string(__LINE__));               \
        }                                                                                     \
    } while (0)

#define CHECK_EQ(a, b)                                                                        \
    do {                                                                                      \
        if (!((a) == (b))) {                                                                  \
            throw std::runtime_error(std::string("check failed: ") + #a + " == " + #b +        \
                                     " @ " + __FILE__ + ":" + std::to_string(__LINE__));       \
        }                                                                                     \
    } while (0)
