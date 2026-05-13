#include <iostream>
#include <string>
#include <vector>

#include "app/AppRunner.h"
#include "cli/CommandParser.h"
#include "support/Diagnostic.h"

int main(int argc, char** argv) {
    try {
        std::vector<std::string> args;
        args.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U);
        for (int index = 1; index < argc; ++index) {
            args.emplace_back(argv[index]);
        }

        prebyte::CommandParser parser;
        prebyte::Command command = parser.parse(args);

        prebyte::AppRunner runner;
        runner.run(command);
        return 0;
    } catch (const prebyte::DiagnosticError& error) {
        std::cerr << error.what() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "error[UNEXPECTED]: " << error.what() << '\n';
    }

    return 1;
}
