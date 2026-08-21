#include "TestHarness.h"

#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--list-tests") {
        for (const std::string& name : prebyte::test::test_names()) {
            std::cout << name << '\n';
        }
        return 0;
    }

    if (argc == 3 && std::string_view(argv[1]) == "--run-test") {
        return prebyte::test::run_test_by_name(argv[2]);
    }

    return prebyte::test::run_all_tests();
}
