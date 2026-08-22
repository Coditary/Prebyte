cmake_minimum_required(VERSION 3.31)

project(Prebyte VERSION 1.0.5 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

option(PREBYTE_BUILD_TESTS "Build test binary" ON)
option(PREBYTE_BUILD_BENCHMARKS "Build benchmark binary" ON)
option(PREBYTE_BUILD_FUZZERS "Build libFuzzer targets" OFF)
option(PREBYTE_ENABLE_COVERAGE "Enable GCC/GCov coverage instrumentation" OFF)
option(PREBYTE_ENABLE_SANITIZERS "Enable Clang sanitizer instrumentation" OFF)
set(PREBYTE_SANITIZERS "address,undefined"
    CACHE STRING "Comma-separated Clang sanitizers: address, undefined, thread, memory")

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
file(GLOB_RECURSE RUNTIME_SOURCES CONFIGURE_DEPENDS src/main/cpp/runtime/*.cpp)
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

if(PREBYTE_ENABLE_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        message(FATAL_ERROR "PREBYTE_ENABLE_SANITIZERS requires Clang")
    endif()

    if(PREBYTE_SANITIZERS STREQUAL "")
        set(PREBYTE_SANITIZERS "address,undefined")
    endif()

    string(REPLACE "," ";" PREBYTE_SANITIZER_LIST "${PREBYTE_SANITIZERS}")
    set(PREBYTE_HAS_ADDRESS_SANITIZER OFF)
    set(PREBYTE_HAS_THREAD_SANITIZER OFF)
    set(PREBYTE_HAS_MEMORY_SANITIZER OFF)

    foreach(PREBYTE_SANITIZER IN LISTS PREBYTE_SANITIZER_LIST)
        string(STRIP "${PREBYTE_SANITIZER}" PREBYTE_SANITIZER)
        if(PREBYTE_SANITIZER STREQUAL "address")
            set(PREBYTE_HAS_ADDRESS_SANITIZER ON)
        elseif(PREBYTE_SANITIZER STREQUAL "thread")
            set(PREBYTE_HAS_THREAD_SANITIZER ON)
        elseif(PREBYTE_SANITIZER STREQUAL "memory")
            set(PREBYTE_HAS_MEMORY_SANITIZER ON)
        endif()
    endforeach()

    if(PREBYTE_HAS_THREAD_SANITIZER AND PREBYTE_HAS_MEMORY_SANITIZER)
        message(FATAL_ERROR "TSan and MSan cannot be combined")
    endif()
    if(PREBYTE_HAS_ADDRESS_SANITIZER AND PREBYTE_HAS_THREAD_SANITIZER)
        message(FATAL_ERROR "ASan and TSan cannot be combined")
    endif()
    if(PREBYTE_HAS_ADDRESS_SANITIZER AND PREBYTE_HAS_MEMORY_SANITIZER)
        message(FATAL_ERROR "ASan and MSan cannot be combined")
    endif()

    set(PREBYTE_SANITIZER_COMPILE_OPTIONS -fsanitize=${PREBYTE_SANITIZERS} -fno-omit-frame-pointer -g)
    if(PREBYTE_HAS_MEMORY_SANITIZER)
        list(APPEND PREBYTE_SANITIZER_COMPILE_OPTIONS -fsanitize-memory-track-origins=1)
    endif()
    set(PREBYTE_SANITIZER_LINK_OPTIONS -fsanitize=${PREBYTE_SANITIZERS})

    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${PREBYTE_SANITIZER_COMPILE_OPTIONS}>")
    add_link_options(${PREBYTE_SANITIZER_LINK_OPTIONS})
endif()

add_executable(prebyte src/main/cpp/main.cpp)
target_link_libraries(prebyte PRIVATE prebyte_core)
install(TARGETS prebyte RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/prebyte)

if(PREBYTE_BUILD_TESTS)
    file(GLOB TEST_CORRECTNESS_UNIT_SOURCES CONFIGURE_DEPENDS tests/correctness/unit/*.cpp)
    file(GLOB TEST_CORRECTNESS_INTEGRATION_SOURCES CONFIGURE_DEPENDS tests/correctness/integration/*.cpp)
    file(GLOB TEST_CORRECTNESS_PROPERTY_SOURCES CONFIGURE_DEPENDS tests/correctness/property/*.cpp)
    file(GLOB TEST_FAULT_TOLERANCE_REGRESSION_SOURCES CONFIGURE_DEPENDS tests/fault_tolerance/regression/*.cpp)
    file(GLOB TEST_SECURITY_SOURCES CONFIGURE_DEPENDS tests/security/*.cpp)
    file(GLOB TEST_CONCURRENCY_SOURCES CONFIGURE_DEPENDS tests/concurrency/*.cpp)
    file(GLOB TEST_PORTABILITY_CLI_SOURCES CONFIGURE_DEPENDS tests/portability/cli/*.cpp)
    file(GLOB TEST_PORTABILITY_PACKAGING_SOURCES CONFIGURE_DEPENDS tests/portability/packaging/*.cpp)

    set(PREBYTE_TEST_SOURCES
        ${TEST_CORRECTNESS_UNIT_SOURCES}
        ${TEST_CORRECTNESS_INTEGRATION_SOURCES}
        ${TEST_CORRECTNESS_PROPERTY_SOURCES}
        ${TEST_FAULT_TOLERANCE_REGRESSION_SOURCES}
        ${TEST_SECURITY_SOURCES}
        ${TEST_CONCURRENCY_SOURCES}
        ${TEST_PORTABILITY_CLI_SOURCES}
        ${TEST_PORTABILITY_PACKAGING_SOURCES}
    )

    add_executable(prebyte_tests
        tests/harness/TestHarness.cpp
        tests/harness/TestMain.cpp
        tests/support/CliProcess.cpp
        ${PREBYTE_TEST_SOURCES}
    )
    target_include_directories(prebyte_tests PRIVATE tests tests/harness tests/support)
    target_link_libraries(prebyte_tests PRIVATE prebyte_core)
    target_compile_definitions(prebyte_tests PRIVATE
        PREBYTE_CLI_BINARY="$<TARGET_FILE:prebyte>"
        PREBYTE_CLI_WORKDIR="${CMAKE_SOURCE_DIR}"
    )
    add_dependencies(prebyte_tests prebyte)

    if(PREBYTE_ENABLE_COVERAGE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
            target_compile_options(prebyte_tests PRIVATE --coverage -O0 -g)
            target_link_options(prebyte_tests PRIVATE --coverage)
        endif()
    endif()

    enable_testing()

    set(PREBYTE_TEST_DISCOVERY_SOURCES ${PREBYTE_TEST_SOURCES})
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

if(PREBYTE_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        message(FATAL_ERROR "PREBYTE_BUILD_FUZZERS requires Clang with libFuzzer support")
    endif()

    if(NOT PREBYTE_ENABLE_SANITIZERS)
        message(FATAL_ERROR "PREBYTE_BUILD_FUZZERS requires PREBYTE_ENABLE_SANITIZERS")
    endif()

    enable_testing()

    set(PREBYTE_FUZZ_TARGETS
        fuzz_template_lexer
        fuzz_template_parser
        fuzz_json_parser
        fuzz_yaml_parser
        fuzz_toml_parser
        fuzz_ini_parser
        fuzz_env_parser
        fuzz_compiled_template_serializer
        fuzz_settings_loader
        fuzz_include_resolver
        fuzz_file_parser
        fuzz_lua_chunk
        fuzz_render_pbt
        fuzz_app_runner
        fuzz_batch_render
        fuzz_structured_import
        fuzz_lua_sandbox
    )

    set(PREBYTE_FUZZ_DIR tests/fault_tolerance/fuzz)

    set(PREBYTE_FUZZ_SOURCES
        ${PREBYTE_FUZZ_DIR}/TemplateLexerFuzz.cpp
        ${PREBYTE_FUZZ_DIR}/TemplateParserFuzz.cpp
        ${PREBYTE_FUZZ_DIR}/JsonParserFuzz.cpp
        ${PREBYTE_FUZZ_DIR}/YamlParserFuzz.cpp
        ${PREBYTE_FUZZ_DIR}/TomlParserFuzz.cpp
        ${PREBYTE_FUZZ_DIR}/IniParserFuzz.cpp
        ${PREBYTE_FUZZ_DIR}/EnvParserFuzz.cpp
        ${PREBYTE_FUZZ_DIR}/CompiledTemplateSerializerFuzz.cpp
        ${PREBYTE_FUZZ_DIR}/SettingsLoaderFuzz.cpp
        ${PREBYTE_FUZZ_DIR}/IncludeResolverFuzz.cpp
        ${PREBYTE_FUZZ_DIR}/FileParserFuzz.cpp
        ${PREBYTE_FUZZ_DIR}/LuaChunkFuzz.cpp
        ${PREBYTE_FUZZ_DIR}/RenderPbtFuzz.cpp
        ${PREBYTE_FUZZ_DIR}/AppRunnerFuzz.cpp
        ${PREBYTE_FUZZ_DIR}/BatchRenderFuzz.cpp
        ${PREBYTE_FUZZ_DIR}/StructuredImportFuzz.cpp
        ${PREBYTE_FUZZ_DIR}/LuaSandboxFuzz.cpp
    )

    list(LENGTH PREBYTE_FUZZ_TARGETS PREBYTE_FUZZ_TARGET_COUNT)
    math(EXPR PREBYTE_FUZZ_LAST_INDEX "${PREBYTE_FUZZ_TARGET_COUNT} - 1")

    if(DEFINED ENV{PREBYTE_FUZZ_MAX_TOTAL_TIME})
        set(PREBYTE_FUZZ_MAX_TOTAL_TIME "$ENV{PREBYTE_FUZZ_MAX_TOTAL_TIME}")
    else()
        set(PREBYTE_FUZZ_MAX_TOTAL_TIME 60)
    endif()

    foreach(PREBYTE_FUZZ_INDEX RANGE ${PREBYTE_FUZZ_LAST_INDEX})
        list(GET PREBYTE_FUZZ_TARGETS ${PREBYTE_FUZZ_INDEX} PREBYTE_FUZZ_TARGET)
        list(GET PREBYTE_FUZZ_SOURCES ${PREBYTE_FUZZ_INDEX} PREBYTE_FUZZ_SOURCE)

        add_executable(${PREBYTE_FUZZ_TARGET} ${PREBYTE_FUZZ_SOURCE})
        target_include_directories(${PREBYTE_FUZZ_TARGET} PRIVATE ${PREBYTE_FUZZ_DIR}/support)
        target_link_libraries(${PREBYTE_FUZZ_TARGET} PRIVATE prebyte_core)
        target_compile_options(${PREBYTE_FUZZ_TARGET} PRIVATE -fsanitize=fuzzer)
        target_link_options(${PREBYTE_FUZZ_TARGET} PRIVATE -fsanitize=fuzzer)

        set(PREBYTE_FUZZ_CORPUS "${CMAKE_SOURCE_DIR}/${PREBYTE_FUZZ_DIR}/corpus/${PREBYTE_FUZZ_TARGET}")
        add_test(
            NAME ${PREBYTE_FUZZ_TARGET}
            COMMAND ${PREBYTE_FUZZ_TARGET}
                -max_total_time=${PREBYTE_FUZZ_MAX_TOTAL_TIME}
                -close_fd_mask=3
                ${PREBYTE_FUZZ_CORPUS}
        )
        set_tests_properties(${PREBYTE_FUZZ_TARGET} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            TIMEOUT 300
        )
    endforeach()

    add_executable(generate_fuzz_pbc_seed ${PREBYTE_FUZZ_DIR}/GeneratePbcSeed.cpp)
    target_link_libraries(generate_fuzz_pbc_seed PRIVATE prebyte_core)

    set(PREBYTE_FUZZ_PBC_SEED ${CMAKE_SOURCE_DIR}/${PREBYTE_FUZZ_DIR}/seeds/pbc/minimal.pbc)
    add_custom_command(
        OUTPUT ${PREBYTE_FUZZ_PBC_SEED}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/${PREBYTE_FUZZ_DIR}/seeds/pbc
        COMMAND generate_fuzz_pbc_seed ${PREBYTE_FUZZ_PBC_SEED}
        DEPENDS generate_fuzz_pbc_seed
        VERBATIM
    )
    add_custom_target(fuzz_pbc_seed DEPENDS ${PREBYTE_FUZZ_PBC_SEED})
    add_dependencies(fuzz_compiled_template_serializer fuzz_pbc_seed)

    add_custom_target(fuzz_regression
        COMMAND ${CMAKE_COMMAND} -E env FUZZ_BUILD_DIR=${CMAKE_BINARY_DIR}
            ${CMAKE_SOURCE_DIR}/scripts/ci/run_fuzz_regression.sh
        DEPENDS ${PREBYTE_FUZZ_TARGETS}
        USES_TERMINAL
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

if(PREBYTE_BUILD_BENCHMARKS)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_executable(prebyte_benchmarks tests/performance/BenchmarkMain.cpp)
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
