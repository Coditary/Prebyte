#include "TestHarness.h"

#include <iostream>

namespace prebyte::test {

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

TestRegistrar::TestRegistrar(std::string name, TestFunction function) {
    registry().push_back(TestCase{std::move(name), function});
}

AssertionFailure::AssertionFailure(const std::string& message)
    : std::runtime_error(message) {}

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
