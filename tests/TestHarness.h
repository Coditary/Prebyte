#pragma once

#include <functional>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace prebyte::test {

using TestFunction = void(*)();

struct TestCase {
    std::string name;
    TestFunction function;
};

std::vector<TestCase>& registry();

struct TestRegistrar {
    TestRegistrar(std::string name, TestFunction function);
};

class AssertionFailure : public std::runtime_error {
public:
    explicit AssertionFailure(const std::string& message);
};

class ScopedEnvironmentVariable {
public:
    ScopedEnvironmentVariable(std::string name, std::string value);
    ScopedEnvironmentVariable(const ScopedEnvironmentVariable&) = delete;
    ScopedEnvironmentVariable& operator=(const ScopedEnvironmentVariable&) = delete;
    ~ScopedEnvironmentVariable();

private:
    std::string name_;
    std::optional<std::string> previous_value_;
};

int run_all_tests();

template<typename Left, typename Right>
void assert_equal(const Left& left, const Right& right, const char* left_expr, const char* right_expr,
                  const char* file, int line) {
    if (!(left == right)) {
        std::ostringstream stream;
        stream << file << ':' << line << " expected equality: " << left_expr << " == " << right_expr
               << " (left='" << left << "', right='" << right << "')";
        throw AssertionFailure(stream.str());
    }
}

inline void assert_true(bool value, const char* expr, const char* file, int line) {
    if (!value) {
        std::ostringstream stream;
        stream << file << ':' << line << " expected true: " << expr;
        throw AssertionFailure(stream.str());
    }
}

template<typename Exception, typename Function>
void assert_throws(Function&& function, const char* expr, const char* file, int line) {
    try {
        function();
    } catch (const Exception&) {
        return;
    } catch (const std::exception& error) {
        std::ostringstream stream;
        stream << file << ':' << line << " expected exception " << typeid(Exception).name()
               << " from " << expr << ", got: " << error.what();
        throw AssertionFailure(stream.str());
    }

    std::ostringstream stream;
    stream << file << ':' << line << " expected exception " << typeid(Exception).name()
           << " from " << expr;
    throw AssertionFailure(stream.str());
}

}

#define TEST_CASE(name) \
    static void name(); \
    static ::prebyte::test::TestRegistrar name##_registrar(#name, &name); \
    static void name()

#define REQUIRE(expr) ::prebyte::test::assert_true((expr), #expr, __FILE__, __LINE__)
#define REQUIRE_EQ(left, right) ::prebyte::test::assert_equal((left), (right), #left, #right, __FILE__, __LINE__)
#define REQUIRE_THROWS_AS(expr, exception_type) \
    ::prebyte::test::assert_throws<exception_type>([&]() { (void)(expr); }, #expr, __FILE__, __LINE__)
