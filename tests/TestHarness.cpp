#include "TestHarness.h"

#include <cstdlib>
#include <iostream>

namespace prebyte::test {

namespace {

bool set_environment_variable(const std::string& name, const std::string& value) {
#ifdef _WIN32
    return _putenv_s(name.c_str(), value.c_str()) == 0;
#else
    return ::setenv(name.c_str(), value.c_str(), 1) == 0;
#endif
}

bool unset_environment_variable(const std::string& name) {
#ifdef _WIN32
    return _putenv_s(name.c_str(), "") == 0;
#else
    return ::unsetenv(name.c_str()) == 0;
#endif
}

}

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

TestRegistrar::TestRegistrar(std::string name, TestFunction function) {
    registry().push_back(TestCase{std::move(name), function});
}

AssertionFailure::AssertionFailure(const std::string& message)
    : std::runtime_error(message) {}

ScopedEnvironmentVariable::ScopedEnvironmentVariable(std::string name, std::string value)
    : name_(std::move(name)) {
    if (const char* current = std::getenv(name_.c_str())) {
        previous_value_ = current;
    }
    if (!set_environment_variable(name_, value)) {
        throw std::runtime_error("environment update failed");
    }
}

ScopedEnvironmentVariable::~ScopedEnvironmentVariable() {
    if (previous_value_.has_value()) {
        static_cast<void>(set_environment_variable(name_, *previous_value_));
    } else {
        static_cast<void>(unset_environment_variable(name_));
    }
}

int run_all_tests() {
    int failed = 0;
    for (const TestCase& test_case : registry()) {
        try {
            test_case.function();
            std::cout << "[PASS] " << test_case.name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "[FAIL] " << test_case.name << " - " << error.what() << '\n';
        }
    }
    std::cout << "Ran " << registry().size() << " tests\n";
    return failed == 0 ? 0 : 1;
}

}
