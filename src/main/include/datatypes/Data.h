#pragma once

#include <string>
#include <variant>
#include <map>
#include <vector>
#include <stdexcept>
#include <type_traits>
#include <stdexcept>
#include <sstream>

namespace prebyte {

/**
 * @brief A dynamic data container that can hold different types of values.
 *
 * This class acts as a flexible, JSON-like data structure capable of holding
 * primitive types (string, int, double, bool), as well as nested maps and arrays.
 * It is designed to simplify the handling of multiple data types.
 *
 * Internally, the data is stored using a `std::variant`, allowing for efficient
 * type-safe access and modification.
 *
 * ### Supported types:
 * - `null` (std::monostate)
 * - `std::string`
 * - `int`
 * - `double`
 * - `bool`
 * - `Map` (string to Data)
 * - `Array` (vector of Data)
 */
class Data {
public:
    /// Map of string keys to nested Data values (like a JSON object).
    using Map    = std::map<std::string, Data>;
    /// Array of nested Data values (like a JSON array).
    using Array  = std::vector<Data>;
    /// Variant holding all supported data types.
    using Value  = std::variant<std::monostate, std::string, int, double, bool, Map, Array>;

private:
    Value value;  ///< Internal storage for the data.

public:
    /** @brief Constructs a null (empty) Data value. */
    Data() = default;

    /** @brief Constructs a Data object from a C-style string. */
    Data(const char* v) : value(std::string(v)) {}

    /** @brief Constructs a Data object from a std::string. */
    Data(std::string v) : value(std::move(v)) {}

    /** @brief Constructs a Data object from an integer. */
    Data(int v) : value(v) {}

    /** @brief Constructs a Data object from a double. */
    Data(double v) : value(v) {}

    /** @brief Constructs a Data object from a boolean. */
    Data(bool v) : value(v) {}

    /** @brief Constructs a Data object from a Map. */
    Data(Map v) : value(std::move(v)) {}

    /** @brief Constructs a Data object from an Array. */
    Data(Array v) : value(std::move(v)) {}

    /** @brief Returns true if the stored value is null. */
    bool is_null() const { return std::holds_alternative<std::monostate>(value); }

    /** @brief Returns true if the stored value is a string. */
    bool is_string() const { return std::holds_alternative<std::string>(value); }

    /** @brief Returns true if the stored value is an integer. */
    bool is_int() const { return std::holds_alternative<int>(value); }

    /** @brief Returns true if the stored value is a double. */
    bool is_double() const { return std::holds_alternative<double>(value); }

    /** @brief Returns true if the stored value is a boolean. */
    bool is_bool() const { return std::holds_alternative<bool>(value); }

    /** @brief Returns true if the stored value is a map (object). */
    bool is_map() const { return std::holds_alternative<Map>(value); }

    /** @brief Returns true if the stored value is an array. */
    bool is_array() const { return std::holds_alternative<Array>(value); }

    /**
     * @brief Returns the value as a string.
     * @throws std::bad_variant_access if the type is not string.
     */
    std::string as_string() const;
    const std::string& as_string_ref() const;

    /**
     * @brief Returns the value as an int.
     * @throws std::bad_variant_access if the type is not int.
     */
    int as_int() const;

    /**
     * @brief Returns the value as a double.
     * @throws std::bad_variant_access if the type is not double.
     */
    double as_double() const;

    /**
     * @brief Returns the value as a boolean.
     * @throws std::bad_variant_access if the type is not bool.
     */
    bool as_bool() const;

    /**
     * @brief Returns the value as a map.
     * @throws std::bad_variant_access if the type is not Map.
     */
    const Map& as_map() const;

    /**
     * @brief Returns the value as an array.
     * @throws std::bad_variant_access if the type is not Array.
     */
    const Array& as_array() const;

    /**
     * @brief Accesses a map value by key (const).
     * @param key The string key.
     * @return Reference to the value.
     * @throws std::out_of_range or std::bad_variant_access if not a map.
     */
    const Data& operator[](const std::string& key) const;

    /**
     * @brief Accesses a map value by key (non-const).
     * @param key The string key.
     * @return Reference to the value.
     * @throws std::bad_variant_access if not a map.
     */
    Data& operator[](const std::string& key);

    /**
     * @brief Accesses an array element by index (const).
     * @param index Zero-based array index.
     * @return Reference to the element.
     * @throws std::out_of_range or std::bad_variant_access if not an array.
     */
    const Data& operator[](size_t index) const;

    /**
     * @brief Accesses an array element by index (non-const).
     * @param index Zero-based array index.
     * @return Reference to the element.
     * @throws std::out_of_range or std::bad_variant_access if not an array.
     */
    Data& operator[](size_t index);
};

} 
