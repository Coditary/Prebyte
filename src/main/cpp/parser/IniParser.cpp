#include "parser/IniParser.h"


namespace prebyte {

Data IniParser::parse(const std::filesystem::path& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filepath.string());
    }

    Data::Map result;
    std::string line;
    std::string current_section;

    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            result[current_section] = Data::Map{};
            continue;
        }

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        if (current_section.empty()) {
            result[key] = value;
        } else {
            result[current_section][key] = value;
        }
    }

    return result;
}

bool IniParser::can_parse(const std::filesystem::path& filepath) const {
    if (filepath.extension() != ".ini" && filepath.extension() != ".cfg") return false;

    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t"));
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') return true;
        if (line.contains('=')) return true;
    }

    return false;
}

Data IniParser::parse_string(const std::string& input) {
    std::istringstream stream(input);
    Data::Map result;
    std::string line;
    std::string current_section;

    while (std::getline(stream, line)) {
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            result[current_section] = Data::Map{};
            continue;
        }

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        if (current_section.empty()) {
            result[key] = value;
        } else {
            result[current_section][key] = value;
        }
    }

    return result;
}

} // namespace prebyte
