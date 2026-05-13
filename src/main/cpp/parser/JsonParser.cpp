#include "parser/JsonParser.h"

#include "support/Diagnostic.h"

#include <fstream>
#include <iterator>

namespace prebyte {

namespace {

class JsonValueParser {
public:
    explicit JsonValueParser(std::string input) : input_(std::move(input)) {}

    Data parse() {
        skip_whitespace();
        Data value = parse_value();
        skip_whitespace();
        if (!is_at_end()) {
            throw std::runtime_error("Unexpected trailing JSON content");
        }
        return value;
    }

private:
    Data parse_value() {
        skip_whitespace();
        if (is_at_end()) {
            throw std::runtime_error("Unexpected end of JSON input");
        }

        const char ch = peek();
        if (ch == '{') {
            return parse_object();
        }
        if (ch == '[') {
            return parse_array();
        }
        if (ch == '"') {
            return Data(parse_string());
        }
        if (ch == 't') {
            consume_literal("true");
            return Data(true);
        }
        if (ch == 'f') {
            consume_literal("false");
            return Data(false);
        }
        if (ch == 'n') {
            consume_literal("null");
            return Data();
        }
        return parse_number();
    }

    Data parse_object() {
        advance();
        Data::Map object;
        skip_whitespace();
        if (peek() == '}') {
            advance();
            return Data(object);
        }

        while (true) {
            skip_whitespace();
            const std::string key = parse_string();
            skip_whitespace();
            consume(':');
            skip_whitespace();
            object[key] = parse_value();
            skip_whitespace();
            if (peek() == '}') {
                advance();
                break;
            }
            consume(',');
        }

        return Data(std::move(object));
    }

    Data parse_array() {
        advance();
        Data::Array array;
        skip_whitespace();
        if (peek() == ']') {
            advance();
            return Data(array);
        }

        while (true) {
            skip_whitespace();
            array.push_back(parse_value());
            skip_whitespace();
            if (peek() == ']') {
                advance();
                break;
            }
            consume(',');
        }

        return Data(std::move(array));
    }

    std::string parse_string() {
        consume('"');
        std::string value;
        while (!is_at_end() && peek() != '"') {
            char ch = advance();
            if (ch == '\\') {
                if (is_at_end()) {
                    throw std::runtime_error("Unterminated escape in JSON string");
                }
                const char escaped = advance();
                switch (escaped) {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default:
                    throw std::runtime_error("Unsupported JSON escape sequence");
                }
                continue;
            }
            value.push_back(ch);
        }

        if (is_at_end()) {
            throw std::runtime_error("Unterminated JSON string");
        }
        consume('"');
        return value;
    }

    Data parse_number() {
        std::string value;
        if (peek() == '-') {
            value.push_back(advance());
        }
        while (!is_at_end() && std::isdigit(static_cast<unsigned char>(peek())) != 0) {
            value.push_back(advance());
        }
        bool is_double = false;
        if (!is_at_end() && peek() == '.') {
            is_double = true;
            value.push_back(advance());
            while (!is_at_end() && std::isdigit(static_cast<unsigned char>(peek())) != 0) {
                value.push_back(advance());
            }
        }

        try {
            if (is_double) {
                return Data(std::stod(value));
            }
            return Data(std::stoi(value));
        } catch (const std::exception&) {
            throw std::runtime_error("Invalid JSON number: " + value);
        }
    }

    void consume(char expected) {
        if (is_at_end() || peek() != expected) {
            throw std::runtime_error(std::string("Expected '") + expected + "' in JSON");
        }
        advance();
    }

    void consume_literal(const std::string& literal) {
        for (char ch : literal) {
            consume(ch);
        }
    }

    void skip_whitespace() {
        while (!is_at_end() && std::isspace(static_cast<unsigned char>(peek())) != 0) {
            advance();
        }
    }

    bool is_at_end() const {
        return index_ >= input_.size();
    }

    char peek() const {
        return is_at_end() ? '\0' : input_[index_];
    }

    char advance() {
        return input_[index_++];
    }

    std::string input_;
    std::size_t index_ = 0;
};

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("Could not open JSON file: " + path.string());
    }
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

}

Data JsonParser::parse(const std::filesystem::path& filepath) {
    return parse_string(read_file(filepath));
}

bool JsonParser::can_parse(const std::filesystem::path& filepath) const {
    if (filepath.extension() != ".json") {
        return false;
    }
    try {
        JsonParser parser;
        parser.parse(filepath);
        return true;
    } catch (...) {
        return false;
    }
}

Data JsonParser::parse_string(const std::string& json_string) {
    JsonValueParser parser(json_string);
    return parser.parse();
}

}
