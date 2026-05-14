#include "TestHarness.h"

#include "datatypes/Data.h"

#include <string>

TEST_CASE(Data_as_string_covers_scalar_types) {
    REQUIRE_EQ(prebyte::Data(true).as_string(), std::string("true"));
    REQUIRE_EQ(prebyte::Data(false).as_string(), std::string("false"));
    REQUIRE_EQ(prebyte::Data(42).as_string(), std::string("42"));
    REQUIRE_EQ(prebyte::Data(3.5).as_string(), std::string("3.500000"));
    REQUIRE_EQ(prebyte::Data("Ada").as_string(), std::string("Ada"));
}

TEST_CASE(Data_scalar_conversion_and_error_paths) {
    const prebyte::Data text("Ada");
    REQUIRE_EQ(text.as_string_ref(), std::string("Ada"));

    REQUIRE_EQ(prebyte::Data("42").as_int(), 42);
    REQUIRE_EQ(prebyte::Data(7).as_int(), 7);
    REQUIRE_THROWS_AS(prebyte::Data("Ada").as_int(), std::bad_variant_access);
    REQUIRE_THROWS_AS(prebyte::Data("999999999999999999999999").as_int(), std::bad_variant_access);

    REQUIRE_EQ(prebyte::Data("3.25").as_double(), 3.25);
    REQUIRE_EQ(prebyte::Data(2.5).as_double(), 2.5);
    REQUIRE_THROWS_AS(prebyte::Data("Ada").as_double(), std::bad_variant_access);
    REQUIRE_THROWS_AS(prebyte::Data("1e9999").as_double(), std::bad_variant_access);

    REQUIRE(prebyte::Data("true").as_bool());
    REQUIRE(prebyte::Data("1").as_bool());
    REQUIRE(!prebyte::Data("false").as_bool());
    REQUIRE(!prebyte::Data("0").as_bool());
    REQUIRE_THROWS_AS(prebyte::Data("maybe").as_bool(), std::bad_variant_access);
    REQUIRE_THROWS_AS(prebyte::Data(1).as_bool(), std::bad_variant_access);

    REQUIRE_THROWS_AS(prebyte::Data(true).as_string_ref(), std::bad_variant_access);
    REQUIRE_EQ(prebyte::Data(true).as_string(), std::string("true"));
}

TEST_CASE(Data_map_and_array_accessors_cover_const_and_mutable_paths) {
    prebyte::Data object(prebyte::Data::Map{});
    object["name"] = prebyte::Data("Ada");
    object["age"] = prebyte::Data(42);

    REQUIRE_EQ(object.as_map().at("name").as_string(), std::string("Ada"));
    REQUIRE_EQ(object[std::string("age")].as_int(), 42);

    const prebyte::Data const_object = object;
    REQUIRE_EQ(const_object[std::string("name")].as_string(), std::string("Ada"));
    REQUIRE_THROWS_AS(const_object[std::string("missing")], std::out_of_range);

    prebyte::Data array(prebyte::Data::Array{prebyte::Data("Ada"), prebyte::Data("Grace")});
    REQUIRE_EQ(array.as_array()[1].as_string(), std::string("Grace"));
    array[1] = prebyte::Data("Linus");
    REQUIRE_EQ(array[1].as_string(), std::string("Linus"));

    const prebyte::Data const_array = array;
    REQUIRE_EQ(const_array[0].as_string(), std::string("Ada"));
    REQUIRE_THROWS_AS(const_array[9], std::out_of_range);

    REQUIRE_THROWS_AS(prebyte::Data("Ada").as_map(), std::bad_variant_access);
    REQUIRE_THROWS_AS(prebyte::Data("Ada").as_array(), std::bad_variant_access);
    REQUIRE_THROWS_AS(prebyte::Data("Ada")[std::string("name")], std::runtime_error);
    REQUIRE_THROWS_AS(prebyte::Data("Ada")[0], std::runtime_error);
}
