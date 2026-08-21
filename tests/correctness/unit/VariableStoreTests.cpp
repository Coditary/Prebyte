#include "TestHarness.h"

#include "runtime/VariableStore.h"

#include <map>
#include <string>

TEST_CASE(VariableStore_set_get_and_case_handling) {
    prebyte::VariableStore store;
    store.set("Name", "Ada");
    store.set_value("Count", prebyte::Value(2.0));

    REQUIRE_EQ(*store.get("Name", true), std::string("Ada"));
    REQUIRE_EQ(*store.get("name", false), std::string("Ada"));
    REQUIRE(store.get("name", true) == std::nullopt);

    const prebyte::Value* count_sensitive = store.get_value("Count", true);
    REQUIRE(count_sensitive != nullptr);
    REQUIRE_EQ(count_sensitive->to_string(), std::string("2"));

    const prebyte::Value* count_insensitive = store.get_value("count", false);
    REQUIRE(count_insensitive != nullptr);
    REQUIRE_EQ(count_insensitive->to_string(), std::string("2"));
}

TEST_CASE(VariableStore_get_view_and_set_all_paths) {
    prebyte::VariableStore store;
    store.set("name", "Ada");
    store.set_value("items", prebyte::Value::list(prebyte::Data::Array{prebyte::Data("x")}));

    REQUIRE_EQ(*store.get_view("name", true), std::string_view("Ada"));
    REQUIRE(!store.get_view("items", true).has_value());

    const std::map<std::string, std::string> strings{{"city", "London"}};
    const std::map<std::string, prebyte::Value> values{{"enabled", prebyte::Value(true)}};
    store.set_all(strings);
    store.set_all(values);

    REQUIRE_EQ(*store.get("city", true), std::string("London"));
    REQUIRE_EQ(store.get_value("enabled", true)->to_string(), std::string("true"));
    REQUIRE_EQ(store.values().size(), static_cast<std::size_t>(4));
}
