cmake_minimum_required(VERSION 3.31)

project(Prebyte VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PREBYTE_BUILD_TESTS "Build test binary" ON)
option(PREBYTE_BUILD_BENCHMARKS "Build benchmark binary" ON)

find_package(Lua REQUIRED)

if(TARGET Lua::Lua)
    set(PREBYTE_LUA_TARGET Lua::Lua)
else()
    set(PREBYTE_LUA_TARGET ${LUA_LIBRARIES})
endif()

file(GLOB APP_SOURCES CONFIGURE_DEPENDS src/main/cpp/app/*.cpp)
file(GLOB CLI_SOURCES CONFIGURE_DEPENDS src/main/cpp/cli/*.cpp)
file(GLOB CONFIG_SOURCES CONFIGURE_DEPENDS src/main/cpp/config/*.cpp)
file(GLOB IO_SOURCES CONFIGURE_DEPENDS src/main/cpp/io/*.cpp)
file(GLOB RUNTIME_SOURCES CONFIGURE_DEPENDS src/main/cpp/runtime/*.cpp)
file(GLOB SUPPORT_SOURCES CONFIGURE_DEPENDS src/main/cpp/support/*.cpp)
file(GLOB TEMPLATE_AST_SOURCES CONFIGURE_DEPENDS src/main/cpp/template/ast/*.cpp)
file(GLOB TEMPLATE_LEXER_SOURCES CONFIGURE_DEPENDS src/main/cpp/template/lexer/*.cpp)
file(GLOB TEMPLATE_PARSER_SOURCES CONFIGURE_DEPENDS src/main/cpp/template/parser/*.cpp)

set(PREBYTE_CORE_SOURCES
    ${APP_SOURCES}
    ${CLI_SOURCES}
    ${CONFIG_SOURCES}
    ${IO_SOURCES}
    ${RUNTIME_SOURCES}
    ${SUPPORT_SOURCES}
    ${TEMPLATE_AST_SOURCES}
    ${TEMPLATE_LEXER_SOURCES}
    ${TEMPLATE_PARSER_SOURCES}
    src/main/cpp/PrebyteEngine.cpp
    src/main/cpp/datatypes/Data.cpp
    src/main/cpp/parser/EnvParser.cpp
    src/main/cpp/parser/FileParser.cpp
    src/main/cpp/parser/IniParser.cpp
    src/main/cpp/parser/JsonParser.cpp
    src/main/cpp/parser/TomlParser.cpp
    src/main/cpp/parser/YamlParser.cpp
)

add_library(prebyte_core STATIC ${PREBYTE_CORE_SOURCES})
target_include_directories(prebyte_core
    PUBLIC
        src/main/include
        ${LUA_INCLUDE_DIR}
)
target_link_libraries(prebyte_core
    PUBLIC
        ${PREBYTE_LUA_TARGET}
        m
        dl
)

add_executable(prebyte src/main/cpp/main.cpp)
target_link_libraries(prebyte PRIVATE prebyte_core)

if(PREBYTE_BUILD_TESTS)
    file(GLOB TEST_UNIT_SOURCES CONFIGURE_DEPENDS tests/unit/*.cpp)
    file(GLOB TEST_INTEGRATION_SOURCES CONFIGURE_DEPENDS tests/integration/*.cpp)

    add_executable(prebyte_tests
        tests/TestHarness.cpp
        tests/TestMain.cpp
        ${TEST_UNIT_SOURCES}
        ${TEST_INTEGRATION_SOURCES}
    )
    target_include_directories(prebyte_tests PRIVATE tests)
    target_link_libraries(prebyte_tests PRIVATE prebyte_core)

    include(CTest)
    enable_testing()
    add_test(NAME prebyte_tests COMMAND prebyte_tests)
    set_tests_properties(prebyte_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(PREBYTE_BUILD_BENCHMARKS)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_executable(prebyte_benchmarks tests/BenchmarkMain.cpp)
    target_link_libraries(prebyte_benchmarks PRIVATE prebyte_core)

    add_executable(benchmark_compare_prebyte tools/benchmark_compare/bench_prebyte.cpp)
    target_link_libraries(benchmark_compare_prebyte PRIVATE prebyte_core)

    add_custom_target(compare-benchmark
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/benchmark_compare/run_compare.py $<TARGET_FILE:benchmark_compare_prebyte>
        DEPENDS benchmark_compare_prebyte
        USES_TERMINAL
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
