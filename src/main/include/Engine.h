#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "config/ConfigTypes.h"
#include "runtime/Value.h"

namespace prebyte {

struct CompileOptions {
    std::string variable_prefix = "{{";
    std::string variable_suffix = "}}";
    bool replace_tabs = false;
    std::size_t tab_size = 4;
};

struct RenderOptions {
    EffectiveSettings settings;
};

class RenderContext {
public:
    void set(std::string name, std::string value);
    void set(std::string name, const char* value);
    void set(std::string name, Value value);
    void set_args(std::vector<std::string> args);

private:
    friend class Engine;

    std::map<std::string, Value, std::less<>> variables_;
    std::vector<std::string> args_;
};

class CompiledTemplate {
public:
    CompiledTemplate() = default;

    const std::filesystem::path& source_path() const;
    const std::filesystem::path& logical_path() const;

private:
    struct Impl;

    explicit CompiledTemplate(std::shared_ptr<const Impl> impl);

    friend class Engine;

    std::shared_ptr<const Impl> impl_;
};

// Thread-safe for concurrent render()/render_to() calls on the same instance.
// Each call uses its own render session; share one Engine across threads.
class Engine {
public:
    using ChunkSink = std::function<void(std::string_view)>;

    Engine();

    CompiledTemplate compile(std::string_view source,
                             const std::filesystem::path& source_path = {},
                             const std::filesystem::path& logical_path = {},
                             const CompileOptions& options = {}) const;
    CompiledTemplate compile_file(const std::filesystem::path& path,
                                  const CompileOptions& options = {}) const;
    CompiledTemplate load_compiled_file(const std::filesystem::path& path) const;

    std::string render(const CompiledTemplate& tpl,
                       const RenderContext& ctx = {},
                       const RenderOptions& opts = {}) const;
    void render_to(const CompiledTemplate& tpl,
                   const ChunkSink& sink,
                   const RenderContext& ctx = {},
                   const RenderOptions& opts = {}) const;

private:
    struct Impl;

    std::shared_ptr<Impl> impl_;
};

}
