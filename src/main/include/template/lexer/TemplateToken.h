#pragma once

#include <string>

#include "support/SourceSpan.h"

namespace prebyte {

enum class TemplateTokenType {
    EndOfFile,
    Text,
    TagOpen,
    TagClose,
    Identifier,
    String,
    Number,
    Boolean,
    KeywordIf,
    KeywordElseIf,
    KeywordElse,
    KeywordEndIf,
    KeywordInclude,
    KeywordLua,
    KeywordLuaBlock,
    KeywordEndLua,
    LeftParen,
    RightParen,
    Bang,
    AndAnd,
    OrOr,
    EqualEqual,
    BangEqual,
};

struct TemplateToken {
    TemplateTokenType type = TemplateTokenType::EndOfFile;
    std::string lexeme;
    SourceSpan span;
};

}
