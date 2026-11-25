// A very small test framework, so the project builds with no downloads.
#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace minitest {

struct Test {
    const char* name;
    std::function<void()> body;
};

inline std::vector<Test>& registry() {
    static std::vector<Test> tests;
    return tests;
}

struct Failure {
    std::string message;
};

struct Register {
    Register(const char* name, std::function<void()> body) { registry().push_back({name, std::move(body)}); }
};

inline int run_all() {
    int failed = 0;
    for (const auto& test : registry()) {
        try {
            test.body();
            std::cout << "[ PASS ] " << test.name << '\n';
        } catch (const Failure& f) {
            ++failed;
            std::cout << "[ FAIL ] " << test.name << ": " << f.message << '\n';
        } catch (const std::exception& e) {
            ++failed;
            std::cout << "[ FAIL ] " << test.name << ": unexpected exception: " << e.what() << '\n';
        }
    }
    std::cout << registry().size() - failed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

}  // namespace minitest

#define MT_CONCAT2(a, b) a##b
#define MT_CONCAT(a, b) MT_CONCAT2(a, b)

#define TEST(name)                                                                  \
    static void name();                                                             \
    static const minitest::Register MT_CONCAT(register_, name){#name, &name};      \
    static void name()

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::ostringstream os_;                                                 \
            os_ << __FILE__ << ':' << __LINE__ << ": CHECK(" #cond ") failed";      \
            throw minitest::Failure{os_.str()};                                     \
        }                                                                           \
    } while (0)

#define CHECK_EQ(actual, expected)                                                  \
    do {                                                                            \
        const auto& a_ = (actual);                                                  \
        const auto& e_ = (expected);                                                \
        if (!(a_ == e_)) {                                                          \
            std::ostringstream os_;                                                 \
            os_ << __FILE__ << ':' << __LINE__ << ": expected <" << e_              \
                << "> but got <" << a_ << ">";                                      \
            throw minitest::Failure{os_.str()};                                     \
        }                                                                           \
    } while (0)

#define CHECK_THROWS(expr, ExceptionType)                                           \
    do {                                                                            \
        bool thrown_ = false;                                                       \
        try {                                                                       \
            (void)(expr);                                                           \
        } catch (const ExceptionType&) {                                            \
            thrown_ = true;                                                         \
        }                                                                           \
        if (!thrown_) {                                                             \
            std::ostringstream os_;                                                 \
            os_ << __FILE__ << ':' << __LINE__ << ": expected " #ExceptionType;     \
            throw minitest::Failure{os_.str()};                                     \
        }                                                                           \
    } while (0)
