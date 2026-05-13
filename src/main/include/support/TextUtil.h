#pragma once

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace prebyte::text {

inline std::string trim(std::string value) {
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char ch) { return !is_space(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char ch) { return !is_space(ch); }).base(), value.end());
    return value;
}

inline std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

inline bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

inline bool ends_with(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

inline std::vector<std::string> split(std::string_view value, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    for (char ch : value) {
        if (ch == delimiter) {
            parts.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    parts.push_back(current);
    return parts;
}

inline std::string join(const std::vector<std::string>& parts, std::string_view delimiter) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        if (index != 0) {
            stream << delimiter;
        }
        stream << parts[index];
    }
    return stream.str();
}

inline std::string repeat(char ch, std::size_t count) {
    return std::string(count, ch);
}

inline std::string replace_tabs(const std::string& input, std::size_t tab_size) {
    std::string output;
    output.reserve(input.size());
    const std::string replacement = repeat(' ', tab_size);
    for (char ch : input) {
        if (ch == '\t') {
            output += replacement;
        } else {
            output.push_back(ch);
        }
    }
    return output;
}

inline bool is_identifier_start(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

inline bool is_identifier_part(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '.' || ch == ':'
        || ch == '[' || ch == ']';
}

}
