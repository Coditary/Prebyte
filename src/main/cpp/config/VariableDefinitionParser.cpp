#include "config/VariableDefinitionParser.h"

#include "datatypes/Data.h"
#include "parser/FileParser.h"
#include "runtime/FileMetadataCache.h"
#include "support/Diagnostic.h"

#include <fstream>
#include <iterator>
#include <map>
#include <mutex>

namespace prebyte {

namespace {

Diagnostic make_variable_error(const std::string& message) {
    Diagnostic diagnostic;
    diagnostic.code = "CFG004";
    diagnostic.message = message;
    return diagnostic;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw DiagnosticError(make_variable_error("Cannot open file for variable import: " + path.string()));
    }
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

Value data_to_value(const Data& data) {
    return Value::from_data(data);
}

struct StructuredImportCacheEntry {
    std::int64_t mtime_ticks = 0;
    Value value;
};

class StructuredImportCache {
public:
    static StructuredImportCache& instance() {
        static StructuredImportCache cache;
        return cache;
    }

    std::optional<Value> load(const std::filesystem::path& path) {
        const std::filesystem::path normalized = normalize(path);
        const FileMetadata metadata = FileMetadataCache::instance().probe(normalized);
        if (!metadata.exists) {
            return std::nullopt;
        }

        std::lock_guard lock(mutex_);
        auto it = cache_.find(normalized);
        if (it == cache_.end() || it->second.mtime_ticks != metadata.mtime_ticks) {
            return std::nullopt;
        }
        return it->second.value;
    }

    void store(const std::filesystem::path& path, Value value) {
        const std::filesystem::path normalized = normalize(path);
        const FileMetadata metadata = FileMetadataCache::instance().probe(normalized);
        if (!metadata.exists) {
            return;
        }

        std::lock_guard lock(mutex_);
        cache_[normalized] = StructuredImportCacheEntry{.mtime_ticks = metadata.mtime_ticks, .value = std::move(value)};
    }

private:
    static std::filesystem::path normalize(const std::filesystem::path& path) {
        if (path.empty()) {
            return {};
        }
        if (path.is_absolute()) {
            return path.lexically_normal();
        }
        std::error_code error;
        const std::filesystem::path cwd = std::filesystem::current_path(error);
        if (error) {
            return path.lexically_normal();
        }
        return (cwd / path).lexically_normal();
    }

    std::mutex mutex_;
    std::map<std::filesystem::path, StructuredImportCacheEntry> cache_;
};

}

VariableContext VariableDefinitionParser::parse(const std::vector<std::string>& define_args,
                                                const std::map<std::string, std::string>& base_variables,
                                                const std::set<std::string>& base_ignore_names) const {
    VariableContext context;
    context.variables = base_variables;
    context.ignore_names = base_ignore_names;

    for (const std::string& define_arg : define_args) {
        parse_define(define_arg, context);
    }

    return context;
}

void VariableDefinitionParser::parse_define(const std::string& define_arg, VariableContext& context) const {
    const std::size_t equals = define_arg.find('=');
    if (equals == std::string::npos) {
        import_file(define_arg, context);
        return;
    }

    if (equals == 0) {
        throw DiagnosticError(make_variable_error("Variable name cannot be empty"));
    }

    const std::string name = define_arg.substr(0, equals);
    std::string value = define_arg.substr(equals + 1);

    if (value.size() >= 2 && value[0] == '@' && value[1] == '@') {
        context.structured_variables.erase(name);
        context.variables[name] = value.substr(1);
        return;
    }

    if (!value.empty() && value[0] == '@') {
        import_named_file(name, value.substr(1), context);
        return;
    }

    context.structured_variables.erase(name);
    context.variables[name] = value;
}

void VariableDefinitionParser::import_file(const std::filesystem::path& path, VariableContext& context) const {
    FileParser file_parser;
    Data data;
    try {
        data = file_parser.parse(path.string());
    } catch (const std::exception& error) {
        throw DiagnosticError(make_variable_error(error.what()));
    }
    flatten_data("", data, context);
}

void VariableDefinitionParser::import_named_file(const std::string& name, const std::filesystem::path& path,
                                                 VariableContext& context) const {
    if (!is_structured_import_path(path)) {
        context.structured_variables.erase(name);
        context.variables[name] = read_file(path);
        return;
    }

    if (const auto cached = StructuredImportCache::instance().load(path)) {
        context.variables.erase(name);
        context.structured_variables[name] = *cached;
        return;
    }

    FileParser file_parser;
    Data data;
    try {
        data = file_parser.parse(path.string());
    } catch (const std::exception& error) {
        throw DiagnosticError(make_variable_error(error.what()));
    }
    context.variables.erase(name);
    Value value = data_to_value(data);
    StructuredImportCache::instance().store(path, value);
    context.structured_variables[name] = std::move(value);
}

void VariableDefinitionParser::flatten_data(const std::string& prefix, const Data& data, VariableContext& context) const {
    if (data.is_map()) {
        for (const auto& [key, value] : data.as_map()) {
            const std::string full_key = prefix.empty() ? key : prefix + '.' + key;
            flatten_data(full_key, value, context);
        }
        return;
    }

    if (data.is_array()) {
        for (std::size_t index = 0; index < data.as_array().size(); ++index) {
            const std::string full_key = prefix + '.' + std::to_string(index);
            flatten_data(full_key, data[index], context);
        }
        return;
    }

    context.variables[prefix] = data.is_null() ? std::string() : data.as_string();
}

bool VariableDefinitionParser::is_structured_import_path(const std::filesystem::path& path) const {
    const std::string extension = path.extension().string();
    return extension == ".json" || extension == ".yaml" || extension == ".yml"
        || extension == ".toml" || extension == ".ini" || extension == ".env";
}

}
