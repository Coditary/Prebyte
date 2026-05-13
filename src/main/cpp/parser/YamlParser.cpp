#include "parser/YamlParser.h"

#include "support/TextUtil.h"

#include <fstream>
#include <iterator>
#include <regex>

namespace prebyte {

namespace {

struct YamlLine {
    std::size_t indent = 0;
    std::string text;
};

Data parse_scalar(const std::string& raw) {
    const std::string value = text::trim(raw);
    if (value == "true") {
        return Data(true);
    }
    if (value == "false") {
        return Data(false);
    }
    static const std::regex int_regex(R"(^-?\d+$)");
    static const std::regex double_regex(R"(^-?\d+\.\d+$)");
    if (std::regex_match(value, int_regex)) {
        return Data(std::stoi(value));
    }
    if (std::regex_match(value, double_regex)) {
        return Data(std::stod(value));
    }
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return Data(value.substr(1, value.size() - 2));
    }
    return Data(value);
}

std::vector<YamlLine> tokenize_yaml(const std::string& input) {
    std::vector<YamlLine> lines;
    std::istringstream stream(input);
    std::string line;
    while (std::getline(stream, line)) {
        const std::size_t first = line.find_first_not_of(' ');
        if (first == std::string::npos) {
            continue;
        }
        const std::string trimmed = text::trim(line.substr(first));
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        lines.push_back(YamlLine{first, trimmed});
    }
    return lines;
}

class YamlValueParser {
public:
    explicit YamlValueParser(std::vector<YamlLine> lines) : lines_(std::move(lines)) {}

    Data parse() {
        if (lines_.empty()) {
            return Data(Data::Map{});
        }
        return parse_block(lines_[0].indent);
    }

private:
    Data parse_block(std::size_t indent) {
        if (index_ >= lines_.size()) {
            return Data();
        }
        if (text::starts_with(lines_[index_].text, "- ")) {
            return parse_array(indent);
        }
        return parse_map(indent);
    }

    Data parse_map(std::size_t indent) {
        Data::Map map;
        while (index_ < lines_.size() && lines_[index_].indent == indent && !text::starts_with(lines_[index_].text, "- ")) {
            const std::string line = lines_[index_].text;
            const std::size_t colon = line.find(':');
            if (colon == std::string::npos) {
                throw std::runtime_error("Invalid YAML mapping line: " + line);
            }

            const std::string key = text::trim(line.substr(0, colon));
            const std::string rest = text::trim(line.substr(colon + 1));
            ++index_;

            if (!rest.empty()) {
                map[key] = parse_scalar(rest);
                continue;
            }

            if (index_ < lines_.size() && lines_[index_].indent > indent) {
                map[key] = parse_block(lines_[index_].indent);
            } else {
                map[key] = Data(Data::Map{});
            }
        }
        return Data(std::move(map));
    }

    Data parse_array(std::size_t indent) {
        Data::Array array;
        while (index_ < lines_.size() && lines_[index_].indent == indent && text::starts_with(lines_[index_].text, "- ")) {
            const std::string rest = text::trim(lines_[index_].text.substr(2));
            ++index_;

            if (!rest.empty()) {
                array.push_back(parse_scalar(rest));
                continue;
            }

            if (index_ < lines_.size() && lines_[index_].indent > indent) {
                array.push_back(parse_block(lines_[index_].indent));
            } else {
                array.push_back(Data());
            }
        }
        return Data(std::move(array));
    }

    std::vector<YamlLine> lines_;
    std::size_t index_ = 0;
};

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("Could not open YAML file: " + path.string());
    }
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

}

Data YamlParser::parse(const std::filesystem::path& filepath) {
    return parse_string(read_file(filepath));
}

bool YamlParser::can_parse(const std::filesystem::path& filepath) const {
    if (filepath.extension() != ".yaml" && filepath.extension() != ".yml") {
        return false;
    }
    try {
        YamlParser parser;
        parser.parse(filepath);
        return true;
    } catch (...) {
        return false;
    }
}

Data YamlParser::parse_string(const std::string& yaml_string) {
    YamlValueParser parser(tokenize_yaml(yaml_string));
    return parser.parse();
}

}
