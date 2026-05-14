#include "cli/CommandParser.h"

#include "support/Diagnostic.h"
#include "support/TextUtil.h"

namespace prebyte {

namespace {

Diagnostic make_cli_error(const std::string& message) {
    Diagnostic diagnostic;
    diagnostic.code = "CLI001";
    diagnostic.message = message;
    return diagnostic;
}

std::optional<CommandMode> parse_list_mode(const std::string& value) {
    if (value == "rules") {
        return CommandMode::ListRules;
    }
    if (value == "vars") {
        return CommandMode::ListVars;
    }
    if (value == "profiles") {
        return CommandMode::ListProfiles;
    }
    if (value == "ignore" || value == "ignores") {
        return CommandMode::ListIgnores;
    }
    return std::nullopt;
}

}

Command CommandParser::parse(const std::vector<std::string>& args) const {
    Command command;

    std::size_t start_index = 0;
    bool collect_render_args = false;

    if (!args.empty()) {
        if (args[0] == "list") {
            const std::optional<CommandMode> list_mode = args.size() >= 2 ? parse_list_mode(args[1]) : std::nullopt;
            if (!list_mode.has_value()) {
                throw DiagnosticError(make_cli_error("Usage: list rules|vars|profiles|ignore|ignores"));
            }
            command.mode = *list_mode;
            start_index = 2;
        }

        if (start_index == 0 && (args[0] == "-h" || args[0] == "--help")) {
            command.mode = CommandMode::Help;
            return command;
        }

        if (start_index == 0 && (args[0] == "-v" || args[0] == "--version")) {
            command.mode = CommandMode::Version;
            return command;
        }

        if (start_index == 0 && (args[0] == "-e" || args[0] == "--explain")) {
            if (args.size() < 2) {
                throw DiagnosticError(make_cli_error("Missing topic after explain flag"));
            }
            command.mode = CommandMode::Explain;
            command.explain_topic = args[1];
            return command;
        }
    }

    for (std::size_t index = start_index; index < args.size(); ++index) {
        const std::string& arg = args[index];

        if (collect_render_args) {
            command.render_args.push_back(arg);
            continue;
        }

        if (arg == "--") {
            if (command.mode != CommandMode::Render) {
                throw DiagnosticError(make_cli_error("Render argument terminator is only valid for render mode"));
            }
            collect_render_args = true;
            continue;
        }

        if (arg == "-o" || arg == "--output") {
            if (index + 1 >= args.size()) {
                throw DiagnosticError(make_cli_error("Missing output path"));
            }
            command.output_path = args[++index];
            continue;
        }

        if (arg == "-I" || arg == "--include-path") {
            if (index + 1 >= args.size()) {
                throw DiagnosticError(make_cli_error("Missing include path"));
            }
            command.include_paths.push_back(args[++index]);
            continue;
        }

        if (arg == "-d" || arg == "--define") {
            if (index + 1 >= args.size()) {
                throw DiagnosticError(make_cli_error("Missing define value"));
            }
            command.define_args.push_back(args[++index]);
            continue;
        }

        if (text::starts_with(arg, "-D")) {
            if (arg == "-D") {
                throw DiagnosticError(make_cli_error("Missing inline define value"));
            }
            command.define_args.push_back(arg.substr(2));
            continue;
        }

        if (arg == "-r" || arg == "--rule") {
            if (index + 1 >= args.size()) {
                throw DiagnosticError(make_cli_error("Missing rule value"));
            }
            command.rule_args.push_back(args[++index]);
            continue;
        }

        if (arg == "-s" || arg == "--settings") {
            if (index + 1 >= args.size()) {
                throw DiagnosticError(make_cli_error("Missing settings path"));
            }
            command.settings_path = args[++index];
            continue;
        }

        if (arg == "-i" || arg == "--ignore") {
            if (index + 1 >= args.size()) {
                throw DiagnosticError(make_cli_error("Missing ignore value"));
            }
            command.ignore_names.push_back(args[++index]);
            continue;
        }

        if (arg == "-p" || arg == "--profile") {
            if (index + 1 >= args.size()) {
                throw DiagnosticError(make_cli_error("Missing profile value"));
            }
            command.profile_names.push_back(args[++index]);
            continue;
        }

        if (arg == "--benchmark") {
            command.benchmark = true;
            continue;
        }

        if (arg == "-X" || arg == "--debug") {
            command.debug = true;
            continue;
        }

        if (!text::starts_with(arg, "-")) {
            if (command.mode != CommandMode::Render) {
                throw DiagnosticError(make_cli_error("Positional input path is only valid for render mode"));
            }
            if (!command.input_path.has_value()) {
                command.input_path = arg;
                continue;
            }
            command.render_args.push_back(arg);
            continue;
        }

        throw DiagnosticError(make_cli_error("Unknown argument: " + arg));
    }

    return command;
}

}
