#include "parser/YamlParser.h"

#include "support/FileUtil.h"
#include "support/TextUtil.h"

#include <cctype>

namespace prebyte {

namespace {

constexpr std::size_t kMaxYamlLines = 8192;
constexpr std::size_t kMaxYamlCollectionEntries = 8192;
constexpr std::size_t kMaxYamlScalarLength = 4096;

struct YamlLine {
    std::size_t indent = 0;
    std::string text;
};

bool looks_like_integer(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    std::size_t index = 0;
    if (value[index] == '-') {
        if (value.size() == 1) {
            return false;
        }
        ++index;
    }
    for (; index < value.size(); ++index) {
        if (!std::isdigit(static_cast<unsigned char>(value[index]))) {
            return false;
        }
    }
    return true;
}

bool looks_like_double(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    std::size_t index = 0;
    bool saw_dot = false;
    if (value[index] == '-') {
        if (value.size() == 1) {
            return false;
        }
        ++index;
    }
    for (; index < value.size(); ++index) {
        const char ch = value[index];
        if (ch == '.') {
            if (saw_dot) {
                return false;
            }
            saw_dot = true;
            continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return saw_dot;
}

Data parse_scalar(const std::string& raw) {
    const std::string value = text::trim(raw);
    if (value == "true") {
        return Data(true);
    }
    if (value == "false") {
        return Data(false);
    }
    if (value.size() <= kMaxYamlScalarLength) {
        if (looks_like_integer(value)) {
            try {
                return Data(std::stoi(value));
            } catch (const std::exception&) { // NOLINT(bugprone-empty-catch)
            }
        }
        if (looks_like_double(value)) {
            try {
                return Data(std::stod(value));
            } catch (const std::exception&) { // NOLINT(bugprone-empty-catch)
            }
        }
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
        if (lines.size() >= kMaxYamlLines) {
            throw std::runtime_error("YAML input exceeds supported line limit");
        }
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
            if (map.size() >= kMaxYamlCollectionEntries) {
                throw std::runtime_error("YAML map exceeds supported entry limit");
            }
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
            if (array.size() >= kMaxYamlCollectionEntries) {
                throw std::runtime_error("YAML array exceeds supported entry limit");
            }
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

}

Data YamlParser::parse(const std::filesystem::path& filepath) {
    return parse_string(file_util::read_text_file(filepath));
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
