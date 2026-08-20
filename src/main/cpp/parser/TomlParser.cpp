#include "parser/TomlParser.h"

#include "support/TextUtil.h"

#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>

namespace prebyte {

namespace {

bool is_integer_token(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    std::size_t index = 0;
    if (value[index] == '-') {
        ++index;
    }
    if (index >= value.size()) {
        return false;
    }
    for (; index < value.size(); ++index) {
        if (std::isdigit(static_cast<unsigned char>(value[index])) == 0) {
            return false;
        }
    }
    return true;
}

bool is_double_token(std::string_view value) {
    const std::size_t dot = value.find('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 >= value.size()) {
        return false;
    }
    return is_integer_token(value.substr(0, dot))
        && is_integer_token(value.substr(dot + 1));
}

constexpr std::size_t kMaxTomlNestingDepth = 128;
constexpr std::size_t kMaxTomlArrayDepth = 64;

Data parse_toml_value(const std::string& raw, std::size_t array_depth = 0) {
    const std::string value = text::trim(raw);
    if (value == "true") {
        return Data(true);
    }
    if (value == "false") {
        return Data(false);
    }
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return Data(value.substr(1, value.size() - 2));
    }
    if (value.size() >= 2 && value.front() == '[' && value.back() == ']') {
        if (array_depth >= kMaxTomlArrayDepth) {
            throw std::runtime_error("TOML array nesting is too deep");
        }
        Data::Array array;
        const std::string inner = value.substr(1, value.size() - 2);
        for (const std::string& item : text::split(inner, ',')) {
            const std::string trimmed = text::trim(item);
            if (!trimmed.empty()) {
                array.push_back(parse_toml_value(trimmed, array_depth + 1));
            }
        }
        return Data(std::move(array));
    }

    if (is_integer_token(value)) {
        return Data(std::stoi(value));
    }
    if (is_double_token(value)) {
        return Data(std::stod(value));
    }
    return Data(value);
}

void ensure_map(Data& data) {
    if (data.is_null()) {
        data = Data(Data::Map{});
    }
    if (!data.is_map()) {
        throw std::runtime_error("Expected TOML table");
    }
}

void set_nested_value(Data& root, const std::vector<std::string>& path, const Data& value) {
    if (path.size() > kMaxTomlNestingDepth) {
        throw std::runtime_error("TOML nesting is too deep");
    }
    ensure_map(root);
    Data* current = &root;
    for (std::size_t index = 0; index + 1 < path.size(); ++index) {
        ensure_map(*current);
        Data& next = (*current)[path[index]];
        ensure_map(next);
        current = &next;
    }
    ensure_map(*current);
    (*current)[path.back()] = value;
}

std::string strip_comment(const std::string& line) {
    bool in_string = false;
    std::string result;
    for (char ch : line) {
        if (ch == '"') {
            in_string = !in_string;
        }
        if (ch == '#' && !in_string) {
            break;
        }
        result.push_back(ch);
    }
    return result;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("Could not open TOML file: " + path.string());
    }
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

}

Data TomlParser::parse(const std::filesystem::path& filepath) {
    return parse_string(read_file(filepath));
}

bool TomlParser::can_parse(const std::filesystem::path& filepath) const {
    if (filepath.extension() != ".toml") {
        return false;
    }
    try {
        TomlParser parser;
        parser.parse(filepath);
        return true;
    } catch (...) {
        return false;
    }
}

Data TomlParser::parse_string(const std::string& toml_string) {
    Data root(Data::Map{});
    std::vector<std::string> current_section;
    std::istringstream stream(toml_string);
    std::string line;

    while (std::getline(stream, line)) {
        line = text::trim(strip_comment(line));
        if (line.empty()) {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            current_section = text::split(line.substr(1, line.size() - 2), '.');
            for (std::string& part : current_section) {
                part = text::trim(part);
            }
            continue;
        }

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            throw std::runtime_error("Invalid TOML line: " + line);
        }

        std::vector<std::string> key_path = current_section;
        std::vector<std::string> keys = text::split(line.substr(0, equals), '.');
        for (std::string& key : keys) {
            key_path.push_back(text::trim(key));
        }
        set_nested_value(root, key_path, parse_toml_value(line.substr(equals + 1)));
    }

    return root;
}

}
