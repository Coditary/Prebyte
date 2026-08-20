#include "support/Diagnostic.h"
#include "template/lexer/TemplateLexer.h"
#include "template/parser/TemplateParser.h"

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input(reinterpret_cast<const char*>(data), size);

    try {
        prebyte::TemplateLexer lexer(input, "fuzz.txt");
        prebyte::TemplateParser parser(lexer.lex());
        (void)parser.parse_document();
    } catch (const prebyte::DiagnosticError&) {
    } catch (const std::exception&) {
    }

    return 0;
}
