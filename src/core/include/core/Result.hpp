#pragma once

// Small expected-like result type so domain code reports useful failures
// (missing asset, corrupt file, bad time range) without exceptions for
// control flow. Real programmer errors (overflow, contract violation) still
// throw.

#include <string>
#include <utility>

namespace editor::core {

template <typename T>
class Result {
public:
    static Result ok(T value) { return Result(true, std::move(value), {}); }
    static Result fail(std::string error) { return Result(false, T{}, std::move(error)); }

    [[nodiscard]] bool isOk() const noexcept { return ok_; }
    [[nodiscard]] bool isErr() const noexcept { return !ok_; }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }

    T& value() & { return value_; }
    const T& value() const& { return value_; }
    T&& value() && { return std::move(value_); }

private:
    Result(bool ok, T value, std::string error)
        : ok_(ok), value_(std::move(value)), error_(std::move(error)) {}

    bool ok_;
    T value_;
    std::string error_;
};

template <>
class Result<void> {
public:
    static Result ok() { return Result(true, {}); }
    static Result fail(std::string error) { return Result(false, std::move(error)); }

    [[nodiscard]] bool isOk() const noexcept { return ok_; }
    [[nodiscard]] bool isErr() const noexcept { return !ok_; }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }

private:
    Result(bool ok, std::string error) : ok_(ok), error_(std::move(error)) {}
    bool ok_;
    std::string error_;
};

} // namespace editor::core
