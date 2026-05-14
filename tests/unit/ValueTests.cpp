#include "TestHarness.h"

#include "datatypes/Data.h"
#include "runtime/Value.h"

TEST_CASE(Value_string_falsey_tokens_to_bool_false) {
    REQUIRE(!prebyte::Value(std::string("false")).to_bool());
    REQUIRE(!prebyte::Value(std::string("0")).to_bool());
    REQUIRE(!prebyte::Value(std::string("off")).to_bool());
    REQUIRE(!prebyte::Value(std::string(" no ")).to_bool());
    REQUIRE(!prebyte::Value(std::string()).to_bool());
}

TEST_CASE(Value_string_truthy_tokens_to_bool_true) {
    REQUIRE(prebyte::Value(std::string("true")).to_bool());
    REQUIRE(prebyte::Value(std::string("1")).to_bool());
    REQUIRE(prebyte::Value(std::string("Ada")).to_bool());
    REQUIRE(prebyte::Value(std::string(" yes ")).to_bool());
}

TEST_CASE(Value_object_truthiness_depends_on_members) {
    prebyte::Data::Map user;
    user["name"] = prebyte::Data("Ada");

    REQUIRE(prebyte::Value::object(user).to_bool());
    REQUIRE(!prebyte::Value::object({}).to_bool());
}

TEST_CASE(Value_list_truthiness_depends_on_items) {
    prebyte::Data::Array items;
    items.push_back(prebyte::Data("Ada"));

    REQUIRE(prebyte::Value::list(items).to_bool());
    REQUIRE(!prebyte::Value::list({}).to_bool());
}

TEST_CASE(Value_length_follows_len_semantics) {
    prebyte::Data::Map user;
    user["name"] = prebyte::Data("Ada");

    prebyte::Data::Array items;
    items.push_back(prebyte::Data("Ada"));
    items.push_back(prebyte::Data("Grace"));

    REQUIRE_EQ(prebyte::Value(std::string("Ada")).length(), static_cast<std::size_t>(3));
    REQUIRE_EQ(prebyte::Value::object(user).length(), static_cast<std::size_t>(1));
    REQUIRE_EQ(prebyte::Value::list(items).length(), static_cast<std::size_t>(2));
    REQUIRE_EQ(prebyte::Value().length(), static_cast<std::size_t>(0));
    REQUIRE_EQ(prebyte::Value(true).length(), static_cast<std::size_t>(0));
    REQUIRE_EQ(prebyte::Value(42.0).length(), static_cast<std::size_t>(0));
}
