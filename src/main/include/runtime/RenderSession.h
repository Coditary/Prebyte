#pragma once

#include <chrono>
#include <cstdint>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "config/ConfigTypes.h"
#include "runtime/CompiledTemplateProgram.h"
#include "runtime/VariableStore.h"

namespace prebyte {

struct RenderSession {
    static constexpr std::size_t kLinearIncludeDepth = 4;
    static constexpr std::size_t kMaxFunctionCallDepth = 64;

    struct LoopFrame {
        std::string binding_name_0;
        Value binding_value_0;
        std::string binding_name_1;
        Value binding_value_1;
        bool has_second_binding = false;
        std::size_t loop_index0 = 0;
        std::size_t loop_size = 0;
    };

    struct LocalScopeFrame {
        std::map<std::string, Value, std::less<>> values;
    };

    struct FunctionDefinition {
        enum class Kind {
            Template,
            Lua,
        };

        Kind kind = Kind::Template;
        std::vector<std::string> parameters;
        const CompiledProgram* program = nullptr;
        InstructionRange body_range;
        std::string_view lua_source;
        std::filesystem::path definition_file;
        SourceSpan definition_span;
    };

    struct FunctionScopeFrame {
        std::map<std::string, FunctionDefinition, std::less<>> values;
    };

    struct BuiltinSnapshot {
        std::string time;
        std::string date;
        std::string timestamp;
        std::string year;
        std::string month;
        std::string day;
        std::string unix_epoch;
        std::string user;
        std::string host;
        std::string os;
        std::string working_dir;
        std::string uuid;
        std::string random;
    };

    enum class ScopeEntryKind {
        Local,
        Loop,
    };

    struct ScopeEntry {
        ScopeEntryKind kind = ScopeEntryKind::Local;
        std::size_t index = 0;
    };

    struct PreparedIncludeKey {
        const CompiledProgram* parent_program = nullptr;
        std::uint32_t instruction = 0;

        bool operator==(const PreparedIncludeKey& other) const = default;
    };

    struct PreparedIncludeKeyHash {
        std::size_t operator()(const PreparedIncludeKey& key) const {
            return std::hash<const CompiledProgram*>{}(key.parent_program)
                ^ (std::hash<std::uint32_t>{}(key.instruction) << 1);
        }
    };

    struct PreparedIncludeEntry {
        const CompiledProgram* program = nullptr;
        const EffectiveSettings* settings = nullptr;
        std::filesystem::path logical_path;
        std::chrono::steady_clock::time_point valid_until = std::chrono::steady_clock::time_point::min();
    };

    ResolvedConfiguration configuration;
    VariableStore variables;
    std::vector<std::string> args;
    std::set<std::string> ignore_names;
    const ResolvedConfiguration* configuration_ref = nullptr;
    const VariableStore* variables_ref = nullptr;
    const std::vector<std::string>* args_ref = nullptr;
    const std::set<std::string>* ignore_names_ref = nullptr;
    std::map<std::filesystem::path, EffectiveSettings>* effective_settings_cache_ref = nullptr;
    std::unordered_map<PreparedIncludeKey, PreparedIncludeEntry, PreparedIncludeKeyHash>* prepared_include_cache_ref = nullptr;
    std::vector<ScopeEntry> scope_stack;
    std::vector<LocalScopeFrame> local_scopes;
    std::vector<FunctionScopeFrame> function_scopes;
    std::vector<LoopFrame> loop_frames;
    std::vector<const std::filesystem::path*> include_stack;
    std::unordered_set<std::filesystem::path> include_stack_set;
    std::chrono::steady_clock::time_point start_time;
    mutable std::optional<BuiltinSnapshot> builtin_snapshot;
    mutable std::shared_ptr<class LuaRuntime> lua_runtime;
    mutable std::size_t lua_cache_hits = 0;
    mutable std::size_t lua_cache_misses = 0;
    mutable std::size_t function_call_depth = 0;
    std::size_t output_bytes_emitted = 0;

    RenderSession() {
        include_stack.reserve(kLinearIncludeDepth);
        function_scopes.emplace_back();
    }

    const ResolvedConfiguration& configuration_view() const {
        return configuration_ref != nullptr ? *configuration_ref : configuration;
    }

    const VariableStore& variables_view() const {
        return variables_ref != nullptr ? *variables_ref : variables;
    }

    void push_local_scope() {
        local_scopes.emplace_back();
        function_scopes.emplace_back();
        scope_stack.push_back(ScopeEntry{.kind = ScopeEntryKind::Local, .index = local_scopes.size() - 1});
    }

    void pop_local_scope() {
        if (!local_scopes.empty() && !scope_stack.empty() && scope_stack.back().kind == ScopeEntryKind::Local) {
            local_scopes.pop_back();
            scope_stack.pop_back();
            if (function_scopes.size() > 1) {
                function_scopes.pop_back();
            }
        }
    }

    void set_local_value(std::string name, Value value) {
        if (local_scopes.empty()) {
            push_local_scope();
        }
        local_scopes.back().values[std::move(name)] = std::move(value);
    }

    static bool names_equal(std::string_view left, std::string_view right, bool case_sensitive) {
        if (left.size() != right.size()) {
            return false;
        }
        if (case_sensitive) {
            return left == right;
        }
        for (std::size_t index = 0; index < left.size(); ++index) {
            if (std::tolower(static_cast<unsigned char>(left[index]))
                != std::tolower(static_cast<unsigned char>(right[index]))) {
                return false;
            }
        }
        return true;
    }

    static const Value* loop_metadata_value(const LoopFrame& frame, std::string_view member, bool case_sensitive) {
        static thread_local Value value;
        if (names_equal(member, "index", case_sensitive)) {
            value = Value(static_cast<double>(frame.loop_index0 + 1));
            return &value;
        }
        if (names_equal(member, "index0", case_sensitive)) {
            value = Value(static_cast<double>(frame.loop_index0));
            return &value;
        }
        if (names_equal(member, "first", case_sensitive)) {
            value = Value(frame.loop_index0 == 0);
            return &value;
        }
        if (names_equal(member, "last", case_sensitive)) {
            value = Value(frame.loop_index0 + 1 == frame.loop_size);
            return &value;
        }
        return nullptr;
    }

    static const Value* local_scope_value(const LocalScopeFrame& frame, std::string_view name, bool case_sensitive) {
        if (case_sensitive) {
            auto it = frame.values.find(name);
            return it == frame.values.end() ? nullptr : &it->second;
        }
        for (const auto& [candidate, value] : frame.values) {
            if (names_equal(candidate, name, false)) {
                return &value;
            }
        }
        return nullptr;
    }

    static const FunctionDefinition* function_scope_value(const FunctionScopeFrame& frame,
                                                          std::string_view name, bool case_sensitive) {
        if (case_sensitive) {
            auto it = frame.values.find(name);
            return it == frame.values.end() ? nullptr : &it->second;
        }
        for (const auto& [candidate, value] : frame.values) {
            if (names_equal(candidate, name, false)) {
                return &value;
            }
        }
        return nullptr;
    }

    const Value* lookup_scoped_value(std::string_view name, bool case_sensitive) const {
        for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); ++it) {
            if (it->kind == ScopeEntryKind::Local) {
                if (const Value* value = local_scope_value(local_scopes[it->index], name, case_sensitive)) {
                    return value;
                }
                continue;
            }

            const LoopFrame& frame = loop_frames[it->index];
            if (names_equal(frame.binding_name_0, name, case_sensitive)) {
                return &frame.binding_value_0;
            }
            if (frame.has_second_binding && names_equal(frame.binding_name_1, name, case_sensitive)) {
                return &frame.binding_value_1;
            }
            constexpr std::string_view loop_prefix = "loop.";
            if (name.size() > loop_prefix.size() && names_equal(name.substr(0, loop_prefix.size()), loop_prefix, case_sensitive)) {
                if (const Value* value = loop_metadata_value(frame, name.substr(loop_prefix.size()), case_sensitive)) {
                    return value;
                }
            }
        }
        return nullptr;
    }

    const Value* lookup_loop_value(std::string_view name, bool case_sensitive) const {
        for (auto it = loop_frames.rbegin(); it != loop_frames.rend(); ++it) {
            if (names_equal(it->binding_name_0, name, case_sensitive)) {
                return &it->binding_value_0;
            }
            if (it->has_second_binding && names_equal(it->binding_name_1, name, case_sensitive)) {
                return &it->binding_value_1;
            }
            constexpr std::string_view loop_prefix = "loop.";
            if (name.size() > loop_prefix.size() && names_equal(name.substr(0, loop_prefix.size()), loop_prefix, case_sensitive)) {
                if (const Value* value = loop_metadata_value(*it, name.substr(loop_prefix.size()), case_sensitive)) {
                    return value;
                }
            }
        }
        return nullptr;
    }

    bool current_function_scope_contains(std::string_view name, bool case_sensitive) const {
        if (function_scopes.empty()) {
            return false;
        }
        return function_scope_value(function_scopes.back(), name, case_sensitive) != nullptr;
    }

    const FunctionDefinition* lookup_function(std::string_view name, bool case_sensitive) const {
        for (auto it = function_scopes.rbegin(); it != function_scopes.rend(); ++it) {
            if (const FunctionDefinition* function = function_scope_value(*it, name, case_sensitive)) {
                return function;
            }
        }
        return nullptr;
    }

    void set_function(std::string name, FunctionDefinition function) {
        if (function_scopes.empty()) {
            function_scopes.emplace_back();
        }
        function_scopes.back().values[std::move(name)] = std::move(function);
    }

    VariableStore visible_variables() const {
        VariableStore merged;
        merged.set_all(variables_view().values());
        for (const ScopeEntry& entry : scope_stack) {
            if (entry.kind == ScopeEntryKind::Local) {
                for (const auto& [name, value] : local_scopes[entry.index].values) {
                    merged.set_value(name, value);
                }
                continue;
            }

            const LoopFrame& frame = loop_frames[entry.index];
            merged.set_value(frame.binding_name_0, frame.binding_value_0);
            if (frame.has_second_binding) {
                merged.set_value(frame.binding_name_1, frame.binding_value_1);
            }

            Data::Map loop_data;
            loop_data["index"] = Data(static_cast<int>(frame.loop_index0 + 1));
            loop_data["index0"] = Data(static_cast<int>(frame.loop_index0));
            loop_data["first"] = Data(frame.loop_index0 == 0);
            loop_data["last"] = Data(frame.loop_index0 + 1 == frame.loop_size);
            merged.set_value("loop", Value::object(std::move(loop_data)));
        }
        return merged;
    }

    void push_loop_frame(LoopFrame frame) {
        loop_frames.push_back(std::move(frame));
        scope_stack.push_back(ScopeEntry{.kind = ScopeEntryKind::Loop, .index = loop_frames.size() - 1});
    }

    void pop_loop_frame() {
        if (!loop_frames.empty() && !scope_stack.empty() && scope_stack.back().kind == ScopeEntryKind::Loop) {
            loop_frames.pop_back();
            scope_stack.pop_back();
        }
    }

    const std::vector<std::string>& args_view() const {
        return args_ref != nullptr ? *args_ref : args;
    }

    const std::set<std::string>& ignore_names_view() const {
        return ignore_names_ref != nullptr ? *ignore_names_ref : ignore_names;
    }

    bool render_time_exceeded(const EffectiveSettings& settings) const {
        if (settings.max_render_time_ms == std::numeric_limits<std::size_t>::max()
            || start_time == std::chrono::steady_clock::time_point{}) {
            return false;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        return static_cast<std::size_t>(elapsed.count()) > settings.max_render_time_ms;
    }

    bool output_limit_would_exceed(const EffectiveSettings& settings, std::size_t extra_bytes) const {
        if (settings.max_output_size_bytes == std::numeric_limits<std::size_t>::max()) {
            return false;
        }
        return extra_bytes > settings.max_output_size_bytes - output_bytes_emitted;
    }

    void note_output_bytes(std::size_t extra_bytes) {
        output_bytes_emitted += extra_bytes;
    }

    bool contains_include(const std::filesystem::path& path) const {
        if (include_stack.size() < kLinearIncludeDepth) {
            return std::any_of(include_stack.begin(), include_stack.end(), [&](const std::filesystem::path* current) {
                return current != nullptr && *current == path;
            });
        }
        return include_stack_set.contains(path);
    }

    void push_include(const std::filesystem::path& path) {
        include_stack.push_back(&path);
        if (include_stack.size() == kLinearIncludeDepth) {
            include_stack_set.clear();
            include_stack_set.reserve(kLinearIncludeDepth * 2);
            for (const std::filesystem::path* current : include_stack) {
                if (current != nullptr) {
                    include_stack_set.insert(*current);
                }
            }
            return;
        }
        if (include_stack.size() > kLinearIncludeDepth) {
            include_stack_set.insert(path);
        }
    }

    void pop_include() {
        if (include_stack.empty()) {
            return;
        }
        if (include_stack.size() == kLinearIncludeDepth) {
            include_stack_set.clear();
        } else if (include_stack.size() > kLinearIncludeDepth) {
            const std::filesystem::path* current = include_stack.back();
            if (current != nullptr) {
                include_stack_set.erase(*current);
            }
        }
        include_stack.pop_back();
    }
};

}
