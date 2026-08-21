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

TEST_CASE(Value_borrowed_data_and_scalar_helpers) {
    const prebyte::Data string_data("hello");
    const prebyte::Value borrowed = prebyte::Value::borrowed_data(string_data);
    REQUIRE_EQ(borrowed.to_string(), std::string("hello"));
    REQUIRE(borrowed.to_bool());
    REQUIRE_EQ(borrowed.length(), static_cast<std::size_t>(5));

    const prebyte::Data int_data(7);
    const prebyte::Value borrowed_int = prebyte::Value::borrowed_data(int_data);
    REQUIRE(borrowed_int.to_bool());
    REQUIRE_EQ(borrowed_int.try_as_number().value(), 7.0);

    REQUIRE(prebyte::Value(1.0).compare_scalar(prebyte::Value(2.0)).value() == std::strong_ordering::less);
    REQUIRE(prebyte::Value(3.0).compare_scalar(prebyte::Value(3.0)).value() == std::strong_ordering::equal);
    REQUIRE(prebyte::Value(std::string("b")).compare_scalar(prebyte::Value(std::string("a"))).value()
        == std::strong_ordering::greater);
    REQUIRE(prebyte::Value(1.0).equals(prebyte::Value(1.0)));
}

TEST_CASE(Value_member_index_and_collection_views) {
    prebyte::Data::Map object;
    object["name"] = prebyte::Data("Ada");
    prebyte::Data::Array list;
    list.push_back(prebyte::Data("Ada"));
    list.push_back(prebyte::Data("Grace"));

    const prebyte::Value object_value = prebyte::Value::object(object);
    const prebyte::Value list_value = prebyte::Value::list(list);

    REQUIRE(object_value.member("name").has_value());
    REQUIRE_EQ(object_value.member("name")->to_string(), std::string("Ada"));
    REQUIRE(list_value.index(1).has_value());
    REQUIRE_EQ(list_value.index(1)->to_string(), std::string("Grace"));
    REQUIRE_EQ(object_value.object_items().size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(list_value.list_items().size(), static_cast<std::size_t>(2));

    std::string output;
    prebyte::Value(std::string("hello")).append_to(output);
    REQUIRE_EQ(output, std::string("hello"));
}
