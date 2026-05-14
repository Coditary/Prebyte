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
    KeywordFor,
    KeywordIn,
    KeywordIf,
    KeywordElseIf,
    KeywordElse,
    KeywordEndIf,
    KeywordEndFor,
    KeywordInclude,
    KeywordSet,
    KeywordFn,
    KeywordEndFn,
    KeywordLua,
    KeywordLuaBlock,
    KeywordEndLua,
    Dot,
    LeftParen,
    RightParen,
    LeftBracket,
    RightBracket,
    Comma,
    Pipe,
    Equal,
    Bang,
    Less,
    Greater,
    LessEqual,
    GreaterEqual,
    AndAnd,
    OrOr,
    EqualEqual,
    BangEqual,
};

struct TemplateToken {
    TemplateTokenType type = TemplateTokenType::EndOfFile;
    std::string lexeme;
    SourceSpan span;
    bool trim_left = false;
    bool trim_right = false;
};

}
