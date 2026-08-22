#include "TestHarness.h"

#include "runtime/CompiledTemplateCache.h"
#include "runtime/CompiledTemplateCompiler.h"
#include "runtime/CompiledTemplateSerializer.h"
#include "runtime/FileMetadataCache.h"

#include <chrono>
#include <filesystem>
#include <thread>

namespace {

prebyte::EffectiveSettings make_settings(std::string prefix = "{{", std::string suffix = "}}") {
    prebyte::EffectiveSettings settings;
    settings.variable_prefix = std::move(prefix);
    settings.variable_suffix = std::move(suffix);
    return settings;
}

prebyte::CompiledProgram compile_source(const std::string& source, const prebyte::EffectiveSettings& settings) {
    prebyte::CompiledTemplateCompiler compiler;
    return compiler.compile_source(source, "inline.pbt", "inline.pbt", settings);
}

std::filesystem::path cache_test_root(const std::string& name) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "prebyte-compiled-template-cache-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

void erase_cache_entry(const std::filesystem::path& compiled_path, const prebyte::EffectiveSettings& settings) {
    prebyte::CompiledTemplateCache::instance().erase(compiled_path, settings);
}

}

TEST_CASE(CompiledTemplateCache_inline_find_misses_before_store) {
    const prebyte::EffectiveSettings settings = make_settings();
    const std::string source = "Hello {{ name }}\n";

    REQUIRE(prebyte::CompiledTemplateCache::instance().find_inline(source, settings) == nullptr);
}

TEST_CASE(CompiledTemplateCache_inline_store_and_find_return_same_program) {
    const prebyte::EffectiveSettings settings = make_settings();
    const std::string source = "Hello {{ name }}\n";
    prebyte::CompiledProgram program = compile_source(source, settings);

    const prebyte::CompiledProgram* stored =
        prebyte::CompiledTemplateCache::instance().store_inline(source, std::move(program), settings);
    const prebyte::CompiledProgram* found =
        prebyte::CompiledTemplateCache::instance().find_inline(source, settings);

    REQUIRE(stored != nullptr);
    REQUIRE(found == stored);
}

TEST_CASE(CompiledTemplateCache_inline_partitions_by_effective_settings) {
    const std::string source = "Hello {{ name }}\n";
    const prebyte::EffectiveSettings default_settings = make_settings();
    const prebyte::EffectiveSettings custom_settings = make_settings("[[", "]]");

    prebyte::CompiledProgram default_program = compile_source(source, default_settings);
    prebyte::CompiledProgram custom_program = compile_source(source, custom_settings);

    const prebyte::CompiledProgram* default_stored = prebyte::CompiledTemplateCache::instance().store_inline(
        source, std::move(default_program), default_settings);
    const prebyte::CompiledProgram* custom_stored = prebyte::CompiledTemplateCache::instance().store_inline(
        source, std::move(custom_program), custom_settings);

    REQUIRE(prebyte::CompiledTemplateCache::instance().find_inline(source, default_settings) == default_stored);
    REQUIRE(prebyte::CompiledTemplateCache::instance().find_inline(source, custom_settings) == custom_stored);
    REQUIRE(default_stored != custom_stored);
}

TEST_CASE(CompiledTemplateCache_file_store_loaded_and_find_roundtrip) {
    const std::filesystem::path root = cache_test_root("file-roundtrip");
    const std::filesystem::path compiled_path = root / "template.pbc";
    const prebyte::EffectiveSettings settings = make_settings();
    erase_cache_entry(compiled_path, settings);

    prebyte::CompiledProgram program = compile_source("Cached {{ name }}\n", settings);
    const prebyte::CompiledProgram* stored = prebyte::CompiledTemplateCache::instance().store_loaded(
        compiled_path, program, settings, 42);
    const prebyte::CompiledProgram* found =
        prebyte::CompiledTemplateCache::instance().find(compiled_path, settings);

    REQUIRE(stored != nullptr);
    REQUIRE(found == stored);
    REQUIRE_EQ(prebyte::CompiledTemplateCache::instance().compiled_mtime(compiled_path, settings), 42);
}

TEST_CASE(CompiledTemplateCache_erase_removes_file_entry) {
    const std::filesystem::path root = cache_test_root("erase-file");
    const std::filesystem::path compiled_path = root / "template.pbc";
    const prebyte::EffectiveSettings settings = make_settings();
    erase_cache_entry(compiled_path, settings);

    prebyte::CompiledProgram program = compile_source("Hello\n", settings);
    prebyte::CompiledTemplateCache::instance().store_loaded(compiled_path, program, settings, 1);
    REQUIRE(prebyte::CompiledTemplateCache::instance().find(compiled_path, settings) != nullptr);

    prebyte::CompiledTemplateCache::instance().erase(compiled_path, settings);
    REQUIRE(prebyte::CompiledTemplateCache::instance().find(compiled_path, settings) == nullptr);
}

TEST_CASE(CompiledTemplateCache_store_in_memory_is_recently_validated) {
    const std::filesystem::path root = cache_test_root("in-memory");
    const std::filesystem::path compiled_path = root / "template.pbc";
    const prebyte::EffectiveSettings settings = make_settings();
    erase_cache_entry(compiled_path, settings);

    prebyte::CompiledProgram program = compile_source("Hello\n", settings);
    prebyte::CompiledTemplateCache::instance().store_in_memory(compiled_path, program, settings);
    REQUIRE(prebyte::CompiledTemplateCache::instance().recently_validated(compiled_path, settings));
}

TEST_CASE(CompiledTemplateCache_store_loaded_is_recently_validated) {
    const std::filesystem::path root = cache_test_root("loaded-validated");
    const std::filesystem::path compiled_path = root / "template.pbc";
    const prebyte::EffectiveSettings settings = make_settings();
    erase_cache_entry(compiled_path, settings);

    prebyte::CompiledProgram program = compile_source("Hello\n", settings);
    prebyte::CompiledTemplateCache::instance().store_loaded(compiled_path, program, settings, 7);
    REQUIRE(prebyte::CompiledTemplateCache::instance().recently_validated(compiled_path, settings));
}

TEST_CASE(CompiledTemplateCache_mark_validated_refreshes_ttl_after_expiry) {
    const std::filesystem::path root = cache_test_root("validated-ttl");
    const std::filesystem::path compiled_path = root / "template.pbc";
    const prebyte::EffectiveSettings settings = make_settings();
    erase_cache_entry(compiled_path, settings);

    prebyte::CompiledProgram program = compile_source("Hello\n", settings);
    prebyte::CompiledTemplateCache::instance().store_loaded(compiled_path, program, settings, 7);
    REQUIRE(prebyte::CompiledTemplateCache::instance().recently_validated(compiled_path, settings));

    std::this_thread::sleep_for(prebyte::FileMetadataCache::ttl() + std::chrono::milliseconds(50));
    REQUIRE(!prebyte::CompiledTemplateCache::instance().recently_validated(compiled_path, settings));

    prebyte::CompiledTemplateCache::instance().mark_validated(compiled_path, settings);
    REQUIRE(prebyte::CompiledTemplateCache::instance().recently_validated(compiled_path, settings));
}

TEST_CASE(CompiledTemplateCache_serializer_try_load_valid_uses_cached_program) {
    const std::filesystem::path root = cache_test_root("serializer-cache");
    const std::filesystem::path source_path = root / "template.pbt";
    const prebyte::EffectiveSettings settings = make_settings();

    prebyte::CompiledTemplateCompiler compiler;
    prebyte::CompiledTemplateSerializer serializer;
    const prebyte::CompiledProgram program =
        compiler.compile_source("Hello {{ name }}\n", source_path, source_path, settings);
    const std::filesystem::path compiled_path = serializer.compiled_path_for_source(source_path);
    erase_cache_entry(compiled_path, settings);

    prebyte::CompiledTemplateCache::instance().store_loaded(compiled_path, program, settings, 99);
    const prebyte::CompiledProgram* loaded = serializer.try_load_valid(compiled_path, settings);

    REQUIRE(loaded != nullptr);
    REQUIRE_EQ(loaded->logical_path.string(), source_path.string());
}
