cmake_minimum_required(VERSION 3.31)

project(Prebyte VERSION 1.0.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

option(PREBYTE_BUILD_TESTS "Build test binary" ON)
option(PREBYTE_BUILD_BENCHMARKS "Build benchmark binary" ON)
option(PREBYTE_ENABLE_COVERAGE "Enable GCC/GCov coverage instrumentation" OFF)

find_package(Lua REQUIRED)

if(TARGET Lua::Lua)
    set(PREBYTE_LUA_TARGET Lua::Lua)
else()
    set(PREBYTE_LUA_TARGET ${LUA_LIBRARIES})
endif()

set(PREBYTE_PLATFORM_LIBS)
if(UNIX AND NOT APPLE)
    list(APPEND PREBYTE_PLATFORM_LIBS m)
endif()
if(CMAKE_DL_LIBS)
    list(APPEND PREBYTE_PLATFORM_LIBS ${CMAKE_DL_LIBS})
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
        ${PREBYTE_PLATFORM_LIBS}
)

if(PREBYTE_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(prebyte_core PUBLIC --coverage -O0 -g)
        target_link_options(prebyte_core PUBLIC --coverage)
    else()
        message(FATAL_ERROR "PREBYTE_ENABLE_COVERAGE requires GCC-compatible coverage flags")
    endif()
endif()

add_executable(prebyte src/main/cpp/main.cpp)
target_link_libraries(prebyte PRIVATE prebyte_core)
install(TARGETS prebyte RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/prebyte)

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

    if(PREBYTE_ENABLE_COVERAGE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
            target_compile_options(prebyte_tests PRIVATE --coverage -O0 -g)
            target_link_options(prebyte_tests PRIVATE --coverage)
        endif()
    endif()

    enable_testing()

    set(PREBYTE_TEST_DISCOVERY_SOURCES
        ${TEST_UNIT_SOURCES}
        ${TEST_INTEGRATION_SOURCES}
    )
    set(PREBYTE_DISCOVERED_TEST_NAMES)
    set(PREBYTE_DISABLED_TEST_NAMES)

    if(WIN32)
        list(APPEND PREBYTE_DISABLED_TEST_NAMES
            AppRunner_render_source_file_from_adjacent_pbc_without_reading_source
            AppRunner_render_source_file_from_adjacent_pbc_with_named_structured_imports
            AppRunner_reuse_in_memory_compiled_template_when_pbc_write_fails
        )
    endif()

    foreach(PREBYTE_TEST_SOURCE IN LISTS PREBYTE_TEST_DISCOVERY_SOURCES)
        file(STRINGS ${PREBYTE_TEST_SOURCE} PREBYTE_TEST_DECLARATIONS REGEX "^[ \t]*TEST_CASE\\([A-Za-z_][A-Za-z0-9_]*\\)")

        foreach(PREBYTE_TEST_DECLARATION IN LISTS PREBYTE_TEST_DECLARATIONS)
            string(REGEX MATCH "TEST_CASE\\(([A-Za-z_][A-Za-z0-9_]*)\\)" PREBYTE_TEST_MATCH "${PREBYTE_TEST_DECLARATION}")
            set(PREBYTE_TEST_NAME ${CMAKE_MATCH_1})

            if(PREBYTE_TEST_NAME STREQUAL "")
                continue()
            endif()

            if(PREBYTE_TEST_NAME IN_LIST PREBYTE_DISABLED_TEST_NAMES)
                continue()
            endif()

            if(PREBYTE_TEST_NAME IN_LIST PREBYTE_DISCOVERED_TEST_NAMES)
                continue()
            endif()

            list(APPEND PREBYTE_DISCOVERED_TEST_NAMES ${PREBYTE_TEST_NAME})
            add_test(
                NAME ${PREBYTE_TEST_NAME}
                COMMAND $<TARGET_FILE:prebyte_tests> --run-test ${PREBYTE_TEST_NAME}
            )
            set_tests_properties(${PREBYTE_TEST_NAME} PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        endforeach()
    endforeach()
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
