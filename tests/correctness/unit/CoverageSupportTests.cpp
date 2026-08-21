#include "TestHarness.h"

#include "config/ProfileMerger.h"
#include "config/SettingsLoader.h"
#include "runtime/FileMetadataCache.h"
#include "runtime/IncludeResolver.h"
#include "support/Diagnostic.h"

#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path resolver_test_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-resolver-tests" / name;
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root);
    return root;
}

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

}

TEST_CASE(ProfileMerger_merges_profiles_and_rejects_unknown_names) {
    prebyte::SettingsData settings;
    settings.variables["name"] = "Ada";
    prebyte::ProfileConfig profile;
    profile.variables["mode"] = "debug";
    profile.include_paths.push_back("profile/includes");
    profile.ignore_names.push_back("secret");
    profile.rules["strict_variables"] = "true";
    profile.file_rules.push_back(
        prebyte::FileRule{prebyte::RuleMatchKind::Extension, ".txt", "default_variable_value", "Fallback"});
    settings.profiles["dev"] = profile;

    prebyte::ProfileMerger merger;
    const prebyte::SettingsData merged = merger.merge(settings, {"dev"});

    REQUIRE_EQ(merged.variables.at("name"), std::string("Ada"));
    REQUIRE_EQ(merged.variables.at("mode"), std::string("debug"));
    REQUIRE_EQ(merged.include_paths.back().string(), std::string("profile/includes"));
    REQUIRE_EQ(merged.ignore_names.back(), std::string("secret"));
    REQUIRE_EQ(merged.rules.at("strict_variables"), std::string("true"));
    REQUIRE_EQ(merged.file_rules.back().value, std::string("Fallback"));
    REQUIRE_THROWS_AS(merger.merge(settings, {"missing"}), prebyte::DiagnosticError);
}

TEST_CASE(FileMetadataCache_clear_and_reuse_probe_results) {
    const std::filesystem::path root = resolver_test_root("metadata-cache");
    const std::filesystem::path path = root / "exists.txt";
    write_file(path, "hello");

    prebyte::FileMetadataCache::instance().clear();
    const prebyte::FileMetadata first = prebyte::FileMetadataCache::instance().probe(path);
    const prebyte::FileMetadata second = prebyte::FileMetadataCache::instance().probe(path);

    REQUIRE(first.exists);
    REQUIRE(second.exists);
    REQUIRE_EQ(first.mtime_ticks, second.mtime_ticks);

    prebyte::FileMetadataCache::instance().clear();
    REQUIRE(!prebyte::FileMetadataCache::instance().probe("").exists);
}

TEST_CASE(IncludeResolver_rejects_overlong_and_overdeep_paths) {
    const std::filesystem::path root = resolver_test_root("include-limits");
    write_file(root / "main.txt", "Hello\n");

    prebyte::IncludeResolver resolver;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;
    settings.allow_includes = true;
    settings.include_paths.push_back(root);

    const std::string long_path(5000, 'a');
    REQUIRE_THROWS_AS(resolver.load(long_path, root / "main.txt", settings, session), prebyte::DiagnosticError);

    std::string deep_path;
    for (int index = 0; index < 100; ++index) {
        deep_path += "../";
    }
    deep_path += "main.txt";
    REQUIRE_THROWS_AS(resolver.load(deep_path, root / "main.txt", settings, session), prebyte::DiagnosticError);
}
