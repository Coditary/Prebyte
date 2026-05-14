#include "TestHarness.h"

#include "config/SettingsLoader.h"
#include "support/Diagnostic.h"

#include <filesystem>
#include <fstream>

namespace {

void write_settings_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path settings_test_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-settings-loader-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

}

TEST_CASE(SettingsLoader_loads_variables_rules_file_rules_and_profiles) {
    const std::filesystem::path root = settings_test_root("valid");
    const std::filesystem::path path = root / "settings.json";
    write_settings_file(path, R"({
        "variables": {"name": "Ada", "empty": null},
        "include_paths": ["includes/a", "includes/b"],
        "ignore": ["secret", "token"],
        "rules": {"trim": "true", "default_variable_value": null},
        "file_rules": {
            ".md": {"trim": "true"},
            "README.md": {"default_variable_value": "Fallback"}
        },
        "profiles": {
            "dev": {
                "variables": {"mode": "debug"},
                "include_paths": ["profile/includes"],
                "ignore": ["profile-secret"],
                "rules": {"strict_variables": "true"},
                "file_rules": {
                    ".txt": {"default_variable_value": "ProfileFallback"}
                }
            }
        }
    })");

    prebyte::SettingsLoader loader;
    const prebyte::SettingsData settings = loader.load(path);

    REQUIRE_EQ(settings.variables.at("name"), std::string("Ada"));
    REQUIRE_EQ(settings.variables.at("empty"), std::string());
    REQUIRE_EQ(settings.include_paths.size(), static_cast<std::size_t>(2));
    REQUIRE_EQ(settings.include_paths[0].string(), std::string("includes/a"));
    REQUIRE_EQ(settings.ignore_names[1], std::string("token"));
    REQUIRE_EQ(settings.rules.at("trim"), std::string("true"));
    REQUIRE_EQ(settings.rules.at("default_variable_value"), std::string());
    REQUIRE_EQ(settings.file_rules.size(), static_cast<std::size_t>(2));
    REQUIRE_EQ(settings.file_rules[0].pattern, std::string(".md"));
    REQUIRE_EQ(settings.file_rules[0].name, std::string("trim"));
    REQUIRE_EQ(settings.file_rules[1].pattern, std::string("README.md"));
    REQUIRE_EQ(settings.file_rules[1].value, std::string("Fallback"));

    const auto& profile = settings.profiles.at("dev");
    REQUIRE_EQ(profile.variables.at("mode"), std::string("debug"));
    REQUIRE_EQ(profile.include_paths[0].string(), std::string("profile/includes"));
    REQUIRE_EQ(profile.ignore_names[0], std::string("profile-secret"));
    REQUIRE_EQ(profile.rules.at("strict_variables"), std::string("true"));
    REQUIRE_EQ(profile.file_rules[0].pattern, std::string(".txt"));
    REQUIRE_EQ(profile.file_rules[0].value, std::string("ProfileFallback"));
}

TEST_CASE(SettingsLoader_reports_missing_files_and_non_map_roots) {
    const std::filesystem::path root = settings_test_root("missing-root");
    const std::filesystem::path array_root = root / "array.json";
    write_settings_file(array_root, "[]");

    prebyte::SettingsLoader loader;
    REQUIRE_THROWS_AS(loader.load(root / "missing.json"), prebyte::DiagnosticError);
    REQUIRE_THROWS_AS(loader.load(array_root), prebyte::DiagnosticError);
}

TEST_CASE(SettingsLoader_reports_invalid_collection_shapes) {
    const std::filesystem::path root = settings_test_root("invalid-shapes");
    const std::filesystem::path variables_path = root / "variables.json";
    const std::filesystem::path include_paths_path = root / "include-paths.json";
    const std::filesystem::path file_rules_path = root / "file-rules.json";
    const std::filesystem::path profile_path = root / "profile.json";
    write_settings_file(variables_path, R"({"variables": []})");
    write_settings_file(include_paths_path, R"({"include_paths": {}})");
    write_settings_file(file_rules_path, R"({"file_rules": {".md": []}})");
    write_settings_file(profile_path, R"({"profiles": {"dev": []}})");

    prebyte::SettingsLoader loader;
    REQUIRE_THROWS_AS(loader.load(variables_path), prebyte::DiagnosticError);
    REQUIRE_THROWS_AS(loader.load(include_paths_path), prebyte::DiagnosticError);
    REQUIRE_THROWS_AS(loader.load(file_rules_path), prebyte::DiagnosticError);
    REQUIRE_THROWS_AS(loader.load(profile_path), prebyte::DiagnosticError);
}
