#include "runtime/LuaValueBridge.h"

#include "runtime/LuaHeaders.h"

#include "runtime/BuiltinRegistry.h"

namespace prebyte {

namespace {

Data data_from_value(const Value& value) {
    if (value.is_null()) {
        return Data();
    }
    if (const auto bool_value = value.try_as_bool()) {
        return Data(*bool_value);
    }
    if (const auto number_value = value.try_as_number()) {
        return Data(*number_value);
    }
    if (const auto string_value = value.try_as_string_view()) {
        return Data(std::string(*string_value));
    }
    if (const Value::Object* object = value.try_as_object()) {
        Data::Map result;
        for (const auto& [key, item] : *object) {
            result[key] = data_from_value(Value::borrowed_data(item));
        }
        return Data(std::move(result));
    }
    if (const Value::List* list = value.try_as_list()) {
        Data::Array result;
        result.reserve(list->size());
        for (const Data& item : *list) {
            result.push_back(data_from_value(Value::borrowed_data(item)));
        }
        return Data(std::move(result));
    }
    return Data(value.to_string());
}

void push_args_table(lua_State* state, const std::vector<std::string>& args) {
    lua_newtable(state);
    for (std::size_t index = 0; index < args.size(); ++index) {
        const std::string& value = args[index];
        lua_pushlstring(state, value.data(), value.size());
        lua_rawseti(state, -2, static_cast<lua_Integer>(index));
    }
}

bool is_array_like_table(lua_State* state, int index) {
    index = lua_absindex(state, index);
    lua_Integer max_key = 0;
    lua_Integer count = 0;
    bool array_like = true;
    lua_pushnil(state);
    while (lua_next(state, index) != 0) {
        if (lua_type(state, -2) != LUA_TNUMBER) {
            array_like = false;
            lua_pop(state, 2);
            break;
        }
        const lua_Number numeric_key = lua_tonumber(state, -2);
        const lua_Integer integer_key = lua_tointeger(state, -2);
        if (numeric_key != static_cast<lua_Number>(integer_key) || integer_key <= 0) {
            array_like = false;
            lua_pop(state, 2);
            break;
        }
        if (integer_key > max_key) {
            max_key = integer_key;
        }
        ++count;
        lua_pop(state, 1);
    }
    return array_like && max_key == count;
}

void push_value(lua_State* state, const Value& value) {
    if (value.is_null()) {
        lua_pushnil(state);
        return;
    }
    if (const auto bool_value = value.try_as_bool()) {
        lua_pushboolean(state, *bool_value ? 1 : 0);
        return;
    }
    if (const auto number_value = value.try_as_number()) {
        lua_pushnumber(state, static_cast<lua_Number>(*number_value));
        return;
    }
    if (const auto string_value = value.try_as_string_view()) {
        lua_pushlstring(state, string_value->data(), string_value->size());
        return;
    }
    if (const Value::Object* object = value.try_as_object()) {
        lua_newtable(state);
        for (const auto& [key, item] : *object) {
            push_value(state, Value::borrowed_data(item));
            lua_setfield(state, -2, key.c_str());
        }
        return;
    }
    if (const Value::List* list = value.try_as_list()) {
        lua_newtable(state);
        for (std::size_t index = 0; index < list->size(); ++index) {
            push_value(state, Value::borrowed_data((*list)[index]));
            lua_rawseti(state, -2, static_cast<lua_Integer>(index + 1));
        }
        return;
    }
    const std::string text = value.to_string();
    lua_pushlstring(state, text.data(), text.size());
}

void push_scalar_value(lua_State* state, const Value& value) {
    if (value.is_null()) {
        lua_pushnil(state);
        return;
    }
    if (const auto string_value = value.try_as_string_view()) {
        lua_pushlstring(state, string_value->data(), string_value->size());
        return;
    }
    const std::string text = value.to_string();
    lua_pushlstring(state, text.data(), text.size());
}

void push_loop_frame_scalars(lua_State* state, const RenderSession::LoopFrame& frame) {
    push_value(state, frame.binding_value_0);
    lua_setfield(state, -2, frame.binding_name_0.c_str());
    if (frame.has_second_binding) {
        push_value(state, frame.binding_value_1);
        lua_setfield(state, -2, frame.binding_name_1.c_str());
    }

    lua_newtable(state);
    lua_pushinteger(state, static_cast<lua_Integer>(frame.loop_index0 + 1));
    lua_setfield(state, -2, "index");
    lua_pushinteger(state, static_cast<lua_Integer>(frame.loop_index0));
    lua_setfield(state, -2, "index0");
    lua_pushboolean(state, frame.loop_index0 == 0 ? 1 : 0);
    lua_setfield(state, -2, "first");
    lua_pushboolean(state, frame.loop_index0 + 1 == frame.loop_size ? 1 : 0);
    lua_setfield(state, -2, "last");
    lua_setfield(state, -2, "loop");
}

}

void LuaValueBridge::push_context(lua_State* state, const EffectiveSettings& settings, const RenderSession& session,
                                  const std::filesystem::path& current_file, std::size_t line) const {
    lua_newtable(state);

    const VariableStore variables = session.visible_variables();
    const auto& values = variables.values();
    for (const auto& [name, value] : values) {
        push_value(state, value);
        lua_setfield(state, -2, name.c_str());
    }

    lua_newtable(state);
    for (const auto& [name, value] : values) {
        push_value(state, value);
        lua_setfield(state, -2, name.c_str());
    }
    lua_setfield(state, -2, "vars");

    push_args_table(state, session.args_view());
    lua_setfield(state, -2, "ARGS");

    BuiltinRegistry builtins;
    SourceSpan span;
    span.file_path = current_file.string();
    span.start.line = line;

    constexpr std::string_view builtin_names[] = {
        "__TIME__", "__LINE__", "__FILE__", "__FILENAME__", "__DIR__", "__EXTENSION__",
        "__DATE__", "__TIMESTAMP__", "__YEAR__", "__MONTH__", "__DAY__", "__UNIX_EPOCH__",
        "__USER__", "__HOST__", "__OS__", "__WORKING_DIR__", "__UUID__", "__RANDOM__",
    };
    for (const std::string_view builtin_name : builtin_names) {
        if (const auto builtin_value = builtins.resolve(std::string(builtin_name), span, current_file, session)) {
            lua_pushlstring(state, builtin_value->data(), builtin_value->size());
            lua_setfield(state, -2, std::string(builtin_name).c_str());
        }
    }

    lua_pushboolean(state, settings.strict_variables ? 1 : 0);
    lua_setfield(state, -2, "strict_variables");

    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "_G");
    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "_ENV");

    lua_newtable(state);
    lua_pushglobaltable(state);
    lua_setfield(state, -2, "__index");
    lua_pushboolean(state, 0);
    lua_setfield(state, -2, "__metatable");
    lua_setmetatable(state, -2);
}

Value LuaValueBridge::read_value(lua_State* state, int index) const {
    switch (lua_type(state, index)) {
    case LUA_TBOOLEAN:
        return Value(lua_toboolean(state, index) != 0);
    case LUA_TNUMBER:
        return Value(static_cast<double>(lua_tonumber(state, index)));
    case LUA_TSTRING:
        return Value(std::string(lua_tostring(state, index)));
    case LUA_TNIL:
        return Value();
    case LUA_TTABLE: {
        index = lua_absindex(state, index);
        if (is_array_like_table(state, index)) {
            Value::List list;
            const lua_Integer size = lua_rawlen(state, index);
            list.reserve(static_cast<std::size_t>(size));
            for (lua_Integer item_index = 1; item_index <= size; ++item_index) {
                lua_rawgeti(state, index, item_index);
                const Value item = read_value(state, -1);
                list.push_back(data_from_value(item));
                lua_pop(state, 1);
            }
            return Value::list(std::move(list));
        }

        Value::Object object;
        lua_pushnil(state);
        while (lua_next(state, index) != 0) {
            if (const char* key = lua_tostring(state, -2)) {
                object[key] = data_from_value(read_value(state, -1));
            }
            lua_pop(state, 1);
        }
        return Value::object(std::move(object));
    }
    default:
        return Value(std::string());
    }
}

}
