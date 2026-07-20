// Minimal deterministic test harness used by portable firmware unit tests.
#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace test {

using Case = std::pair<const char*, std::function<void()>>;

inline std::vector<Case>& cases() {
    static std::vector<Case> all;
    return all;
}

struct Register {
    Register(const char* name, std::function<void()> body) {
        cases().emplace_back(name, body);
    }
};

template <typename A, typename B>
void equal(const A& actual, const B& expected, const char* expression, const char* file, int line) {
    if (!(actual == expected)) {
        std::ostringstream message;
        message << file << ':' << line << ": expected " << expression << " to equal expected value";
        throw std::runtime_error(message.str());
    }
}

inline void require(bool value, const char* expression, const char* file, int line) {
    if (!value) {
        std::ostringstream message;
        message << file << ':' << line << ": requirement failed: " << expression;
        throw std::runtime_error(message.str());
    }
}

}  // namespace test

#define TEST_CASE(name)                                      \
    static void name();                                     \
    static test::Register name##_registration(#name, name); \
    static void name()
#define REQUIRE(value) test::require((value), #value, __FILE__, __LINE__)
#define REQUIRE_EQ(actual, expected) test::equal((actual), (expected), #actual, __FILE__, __LINE__)
