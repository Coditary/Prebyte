#include "TestHarness.h"

#include "runtime/FilterRegistry.h"
#include "support/Diagnostic.h"

TEST_CASE(FilterRegistry_apply_string_filters_and_default) {
    prebyte::FilterRegistry filters;

    REQUIRE_EQ(filters.apply("trim", {prebyte::Value(std::string("  Ada  "))}).to_string(), std::string("Ada"));
    REQUIRE_EQ(filters.apply("upper", {prebyte::Value(std::string("Ada"))}).to_string(), std::string("ADA"));
    REQUIRE_EQ(filters.apply("lower", {prebyte::Value(std::string("Ada"))}).to_string(), std::string("ada"));
    REQUIRE_EQ(filters.apply("replace", {prebyte::Value(std::string("a-b-a")), prebyte::Value(std::string("a")), prebyte::Value(std::string("x"))}).to_string(), std::string("x-b-x"));
    REQUIRE_EQ(filters.apply("replace", {prebyte::Value(std::string("Ada")), prebyte::Value(std::string()), prebyte::Value(std::string("x"))}).to_string(), std::string("Ada"));
    REQUIRE_EQ(filters.apply("default", {prebyte::Value(), prebyte::Value(std::string("fallback"))}).to_string(), std::string("fallback"));
    REQUIRE_EQ(filters.apply("default", {prebyte::Value(std::string("value")), prebyte::Value(std::string("fallback"))}).to_string(), std::string("value"));
}

TEST_CASE(FilterRegistry_reports_unknown_filter_bad_arity_and_structured_input) {
    prebyte::FilterRegistry filters;

    REQUIRE_THROWS_AS(filters.apply("trim", {}), prebyte::DiagnosticError);
    REQUIRE_THROWS_AS(filters.apply("replace", {prebyte::Value(std::string("a")), prebyte::Value(std::string("b"))}), prebyte::DiagnosticError);
    REQUIRE_THROWS_AS(filters.apply("trim", {prebyte::Value::object({})}), prebyte::DiagnosticError);
    REQUIRE_THROWS_AS(filters.apply("unknown", {prebyte::Value(std::string("Ada"))}), prebyte::DiagnosticError);
}
