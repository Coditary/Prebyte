#include "template/lexer/TemplateLexerKeywords.h"

namespace prebyte::template_lexer {

namespace {

struct KeywordRule {
    std::string_view word;
    TemplateTokenType type;
};

constexpr KeywordRule kKeywordRules[] = {
    {"if", TemplateTokenType::KeywordIf},
    {"for", TemplateTokenType::KeywordFor},
    {"in", TemplateTokenType::KeywordIn},
    {"elseif", TemplateTokenType::KeywordElseIf},
    {"else", TemplateTokenType::KeywordElse},
    {"endif", TemplateTokenType::KeywordEndIf},
    {"endfor", TemplateTokenType::KeywordEndFor},
    {"include", TemplateTokenType::KeywordInclude},
    {"set", TemplateTokenType::KeywordSet},
    {"fn", TemplateTokenType::KeywordFn},
    {"endfn", TemplateTokenType::KeywordEndFn},
    {"lua", TemplateTokenType::KeywordLua},
    {"lua:block", TemplateTokenType::KeywordLuaBlock},
    {"endlua", TemplateTokenType::KeywordEndLua},
    {"true", TemplateTokenType::Boolean},
    {"false", TemplateTokenType::Boolean},
};

} // namespace

std::optional<TemplateTokenType> lookup_keyword(std::string_view lexeme) {
    for (const KeywordRule& rule : kKeywordRules) {
        if (lexeme == rule.word) {
            return rule.type;
        }
    }
    return std::nullopt;
}

}
