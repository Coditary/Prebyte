#include "runtime/CompiledTemplateSerializer.h"

#include "io/InputBuffer.h"
#include "runtime/CompiledTemplateCache.h"
#include "runtime/FileMetadataCache.h"
#include "support/Diagnostic.h"

#include <chrono>
#include <cstring>
#include <limits>

namespace prebyte {

namespace {

constexpr char kMagic[] = {'P', 'B', 'C', '1'};

Diagnostic make_compile_error(const std::string& message, const std::filesystem::path& path = {}) {
    Diagnostic diagnostic;
    diagnostic.code = "CACHE001";
    diagnostic.message = message;
    diagnostic.span.file_path = path.string();
    return diagnostic;
}

void append_u32(std::string& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<char>((value >> shift) & 0xffu));
    }
}

void append_i64(std::string& out, std::int64_t value) {
    const auto raw = static_cast<std::uint64_t>(value);
    for (int shift = 0; shift < 64; shift += 8) {
        out.push_back(static_cast<char>((raw >> shift) & 0xffu));
    }
}

void append_string(std::string& out, std::string_view value) {
    append_u32(out, static_cast<std::uint32_t>(value.size()));
    out.append(value.data(), value.size());
}

std::uint32_t read_u32(std::string_view bytes, std::size_t& offset) {
    if (offset + 4 > bytes.size()) {
        throw DiagnosticError(make_compile_error("Unexpected end of compiled template"));
    }
    std::uint32_t value = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset++])) << shift;
    }
    return value;
}

std::int64_t read_i64(std::string_view bytes, std::size_t& offset) {
    if (offset + 8 > bytes.size()) {
        throw DiagnosticError(make_compile_error("Unexpected end of compiled template"));
    }
    std::uint64_t value = 0;
    for (int shift = 0; shift < 64; shift += 8) {
        value |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[offset++])) << shift;
    }
    return static_cast<std::int64_t>(value);
}

std::string read_string(std::string_view bytes, std::size_t& offset) {
    const std::uint32_t size = read_u32(bytes, offset);
    if (offset + size > bytes.size()) {
        throw DiagnosticError(make_compile_error("Unexpected end of compiled template"));
    }
    std::string value(bytes.substr(offset, size));
    offset += size;
    return value;
}

}

std::string CompiledTemplateSerializer::serialize(const CompiledProgram& program) const {
    std::string out;
    out.append(kMagic, sizeof(kMagic));
    append_u32(out, kCompiledTemplateFormatVersion);
    append_string(out, program.logical_path.string());
    append_string(out, program.source_path.string());
    append_string(out, program.parse_options.variable_prefix);
    append_string(out, program.parse_options.variable_suffix);
    append_u32(out, program.parse_options.replace_tabs ? 1u : 0u);
    append_u32(out, program.parse_options.tab_size);
    append_u32(out, static_cast<std::uint32_t>(program.template_instructions.size()));
    append_u32(out, static_cast<std::uint32_t>(program.expression_instructions.size()));
    append_u32(out, static_cast<std::uint32_t>(program.data_blob.size()));
    append_u32(out, static_cast<std::uint32_t>(program.dependencies.size()));
    append_u32(out, static_cast<std::uint32_t>(program.functions.size()));

    for (const TemplateInstruction& instruction : program.template_instructions) {
        append_u32(out, static_cast<std::uint32_t>(instruction.opcode));
        append_u32(out, instruction.data_offset);
        append_u32(out, instruction.data_length);
        append_u32(out, instruction.arg0);
        append_u32(out, instruction.arg1);
    }
    for (const ExpressionInstruction& instruction : program.expression_instructions) {
        append_u32(out, static_cast<std::uint32_t>(instruction.opcode));
        append_u32(out, instruction.data_offset);
        append_u32(out, instruction.data_length);
        append_u32(out, instruction.arg0);
        append_u32(out, instruction.arg1);
    }

    out.append(program.data_blob);
    for (const CompiledDependency& dependency : program.dependencies) {
        append_string(out, dependency.path.string());
        append_i64(out, dependency.mtime_ticks);
    }
    for (const CompiledFunction& function : program.functions) {
        append_u32(out, static_cast<std::uint32_t>(function.kind));
        append_string(out, function.name);
        append_u32(out, static_cast<std::uint32_t>(function.parameters.size()));
        for (const std::string& parameter : function.parameters) {
            append_string(out, parameter);
        }
        append_u32(out, function.body_range.offset);
        append_u32(out, function.body_range.length);
        append_string(out, function.lua_source);
        append_string(out, function.definition_file.string());
        append_string(out, function.span.file_path);
        append_u32(out, static_cast<std::uint32_t>(function.span.start.line));
        append_u32(out, static_cast<std::uint32_t>(function.span.start.column));
        append_u32(out, static_cast<std::uint32_t>(function.span.end.line));
        append_u32(out, static_cast<std::uint32_t>(function.span.end.column));
    }
    return out;
}

CompiledProgram CompiledTemplateSerializer::deserialize(std::string_view bytes,
                                                        const std::filesystem::path& compiled_path) const {
    if (bytes.size() < sizeof(kMagic) + 4 || std::memcmp(bytes.data(), kMagic, sizeof(kMagic)) != 0) {
        throw DiagnosticError(make_compile_error("Invalid compiled template header", compiled_path));
    }

    std::size_t offset = sizeof(kMagic);
    const std::uint32_t version = read_u32(bytes, offset);
    if (version != kCompiledTemplateFormatVersion) {
        throw DiagnosticError(make_compile_error("Unsupported compiled template version", compiled_path));
    }

    CompiledProgram program;
    program.logical_path = read_string(bytes, offset);
    program.source_path = read_string(bytes, offset);
    program.parse_options.variable_prefix = read_string(bytes, offset);
    program.parse_options.variable_suffix = read_string(bytes, offset);
    program.parse_options.replace_tabs = read_u32(bytes, offset) != 0;
    program.parse_options.tab_size = read_u32(bytes, offset);
    const std::uint32_t template_count = read_u32(bytes, offset);
    const std::uint32_t expression_count = read_u32(bytes, offset);
    const std::uint32_t data_size = read_u32(bytes, offset);
    const std::uint32_t dependency_count = read_u32(bytes, offset);
    const std::uint32_t function_count = read_u32(bytes, offset);

    program.template_instructions.reserve(template_count);
    for (std::uint32_t index = 0; index < template_count; ++index) {
        program.template_instructions.push_back(TemplateInstruction{
            .opcode = static_cast<TemplateOpcode>(read_u32(bytes, offset)),
            .data_offset = read_u32(bytes, offset),
            .data_length = read_u32(bytes, offset),
            .arg0 = read_u32(bytes, offset),
            .arg1 = read_u32(bytes, offset),
        });
    }

    program.expression_instructions.reserve(expression_count);
    for (std::uint32_t index = 0; index < expression_count; ++index) {
        program.expression_instructions.push_back(ExpressionInstruction{
            .opcode = static_cast<ExpressionOpcode>(read_u32(bytes, offset)),
            .data_offset = read_u32(bytes, offset),
            .data_length = read_u32(bytes, offset),
            .arg0 = read_u32(bytes, offset),
            .arg1 = read_u32(bytes, offset),
        });
    }

    if (offset + data_size > bytes.size()) {
        throw DiagnosticError(make_compile_error("Invalid compiled template data section", compiled_path));
    }
    program.data_blob.assign(bytes.substr(offset, data_size));
    offset += data_size;

    program.dependencies.reserve(dependency_count);
    for (std::uint32_t index = 0; index < dependency_count; ++index) {
        program.dependencies.push_back(CompiledDependency{read_string(bytes, offset), read_i64(bytes, offset)});
    }

    program.functions.reserve(function_count);
    for (std::uint32_t index = 0; index < function_count; ++index) {
        CompiledFunction function;
        function.kind = static_cast<CompiledFunction::Kind>(read_u32(bytes, offset));
        function.name = read_string(bytes, offset);
        const std::uint32_t parameter_count = read_u32(bytes, offset);
        function.parameters.reserve(parameter_count);
        for (std::uint32_t parameter_index = 0; parameter_index < parameter_count; ++parameter_index) {
            function.parameters.push_back(read_string(bytes, offset));
        }
        function.body_range.offset = read_u32(bytes, offset);
        function.body_range.length = read_u32(bytes, offset);
        function.lua_source = read_string(bytes, offset);
        function.definition_file = read_string(bytes, offset);
        function.span.file_path = read_string(bytes, offset);
        function.span.start.line = read_u32(bytes, offset);
        function.span.start.column = read_u32(bytes, offset);
        function.span.end.line = read_u32(bytes, offset);
        function.span.end.column = read_u32(bytes, offset);
        program.functions.push_back(std::move(function));
    }

    return program;
}

const CompiledProgram* CompiledTemplateSerializer::try_load_valid(const std::filesystem::path& path,
                                                                  const EffectiveSettings& settings) const {
    try {
        if (const CompiledProgram* cached = CompiledTemplateCache::instance().find(path, settings)) {
            if (CompiledTemplateCache::instance().recently_validated(path, settings)) {
                return cached;
            }

            const FileMetadata cached_metadata = FileMetadataCache::instance().probe(path);
            if (!cached_metadata.exists
                || cached_metadata.mtime_ticks != CompiledTemplateCache::instance().compiled_mtime(path, settings)) {
                CompiledTemplateCache::instance().erase(path, settings);
            } else if (is_fresh(*cached, settings)) {
                CompiledTemplateCache::instance().mark_validated(path, settings);
                return cached;
            } else {
                CompiledTemplateCache::instance().erase(path, settings);
            }
        }

        const FileMetadata metadata = FileMetadataCache::instance().probe(path);
        if (!metadata.exists) {
            CompiledTemplateCache::instance().erase(path, settings);
            return nullptr;
        }

        const InputBuffer input = InputBuffer::from_file(path);
        CompiledProgram program = deserialize(input.view(), path);
        if (!is_fresh(program, settings)) {
            return nullptr;
        }
        return CompiledTemplateCache::instance().store_loaded(path, program, settings, metadata.mtime_ticks);
    } catch (const std::exception&) {
        return nullptr;
    }
}

bool CompiledTemplateSerializer::is_fresh(const CompiledProgram& program, const EffectiveSettings& settings) const {
    if (program.parse_options.variable_prefix != settings.variable_prefix
        || program.parse_options.variable_suffix != settings.variable_suffix
        || program.parse_options.replace_tabs != settings.replace_tabs
        || program.parse_options.tab_size != settings.tab_size) {
        return false;
    }
    for (const CompiledDependency& dependency : program.dependencies) {
        const FileMetadata metadata = FileMetadataCache::instance().probe(dependency.path);
        if (!metadata.exists || metadata.mtime_ticks > dependency.mtime_ticks) {
            return false;
        }
    }
    return true;
}

std::filesystem::path CompiledTemplateSerializer::compiled_path_for_source(const std::filesystem::path& source_path) const {
    if (source_path.extension() == ".pbc") {
        return source_path;
    }
    if (source_path.extension() == ".pbt") {
        std::filesystem::path result = source_path;
        result.replace_extension(".pbc");
        return result;
    }
    return std::filesystem::path(source_path.string() + ".pbc");
}

std::filesystem::path CompiledTemplateSerializer::logical_path_for_source(const std::filesystem::path& source_path) const {
    if (source_path.extension() == ".pbt" || source_path.extension() == ".pbc") {
        std::filesystem::path result = source_path;
        result.replace_extension();
        return result;
    }
    return source_path;
}

std::int64_t CompiledTemplateSerializer::mtime_ticks(const std::filesystem::path& path) const {
    std::error_code error;
    const auto time = std::filesystem::last_write_time(path, error);
    if (error) {
        return std::numeric_limits<std::int64_t>::max();
    }
    return std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count();
}

}
