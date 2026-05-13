#include "TestHarness.h"

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
