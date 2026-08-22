.PHONY: all start run test coverage analyze lint security static-analysis sanitize tsan msan fuzz fuzz-regression benchmark benchmark-gate compare-benchmark packaging-smoke packaging-smoke-docker ci-full ci-fast configure reqpack reqpack-index clean

CMAKE_PRESET ?= dev
CMAKE_BUILD_DIR := build-cmake/dev
COVERAGE_MIN_LINE ?= 85
PREBYTE_FUZZ_MAX_TOTAL_TIME ?= 60
COVERAGE_BUILD_DIR := build-cmake/coverage
CLANG_TIDY_BUILD_DIR := build-cmake/tidy
CMAKE_CACHE := $(CMAKE_BUILD_DIR)/CMakeCache.txt
COMPARE_DIR := tools/benchmark_compare
PREBYTE_VERSION ?= $(shell python3 -c 'import pathlib,re; text = pathlib.Path("CMakeLists.txt").read_text(encoding="utf-8"); match = re.search(r"project\([^\n]*VERSION\s+([^\s)]+)", text); print(match.group(1) if match else "0.0.0")')
REQPACK_OUTPUT_DIR ?= dist

all: ci-full

ci-full:
	chmod +x scripts/ci/run_all_checks.sh
	./scripts/ci/run_all_checks.sh

ci-fast:
	chmod +x scripts/ci/run_all_checks.sh
	PREBYTE_SKIP_FUZZ=1 ./scripts/ci/run_all_checks.sh

configure:
	@if [ -f "$(CMAKE_CACHE)" ]; then \
		cached_source=$$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$(CMAKE_CACHE)" | head -1); \
		if [ -n "$$cached_source" ] && [ "$$cached_source" != "$(CURDIR)" ]; then \
			printf 'Removing stale CMake cache (%s -> %s)\n' "$$cached_source" "$(CURDIR)"; \
			rm -rf "$(CMAKE_BUILD_DIR)"; \
		fi; \
	fi
	cmake --preset $(CMAKE_PRESET)

start: configure
	cmake --build --preset $(CMAKE_PRESET) --target prebyte

run: start
	./$(CMAKE_BUILD_DIR)/prebyte

test: configure
	cmake --build --preset $(CMAKE_PRESET) --target prebyte_tests
	ctest --preset $(CMAKE_PRESET)

coverage:
	cmake --preset coverage
	cmake --build --preset coverage-tests
	ctest --preset coverage
	COVERAGE_MIN_LINE=$(COVERAGE_MIN_LINE) ./scripts/ci/generate_coverage_report.sh

analyze:
	./scripts/ci/run_clang_tidy.sh analyze

lint:
	./scripts/ci/run_clang_tidy.sh lint

security:
	./scripts/ci/run_clang_tidy.sh security

static-analysis:
	./scripts/ci/run_clang_tidy.sh all

MSAN_CMAKE_ARGS ?=

sanitize:
	./scripts/ci/run_sanitize_tests.sh asan

tsan:
	./scripts/ci/run_sanitize_tests.sh tsan

msan:
	./scripts/ci/run_sanitize_tests.sh msan $(MSAN_CMAKE_ARGS)

fuzz:
	PREBYTE_FUZZ_MAX_TOTAL_TIME=$(PREBYTE_FUZZ_MAX_TOTAL_TIME) ./scripts/ci/run_fuzzers.sh

fuzz-regression:
	chmod +x scripts/ci/run_fuzz_regression.sh
	./scripts/ci/run_fuzz_regression.sh

.NOTPARALLEL: all ci-full ci-fast analyze lint security static-analysis sanitize tsan msan fuzz fuzz-regression

benchmark: configure
	cmake --build --preset $(CMAKE_PRESET) --target prebyte_benchmarks
	./$(CMAKE_BUILD_DIR)/prebyte_benchmarks
	$(MAKE) compare-benchmark

compare-benchmark: configure
	cmake --build --preset $(CMAKE_PRESET) --target compare-benchmark

benchmark-gate: configure
	cmake --build --preset $(CMAKE_PRESET) --target prebyte_benchmarks
	python3 scripts/ci/check_benchmark_regression.py --benchmark-binary $(CMAKE_BUILD_DIR)/prebyte_benchmarks

packaging-smoke: start
	chmod +x scripts/ci/smoke_packaging.py
	python3 scripts/ci/smoke_packaging.py --binary $(CMAKE_BUILD_DIR)/prebyte

packaging-smoke-docker: start
	chmod +x scripts/ci/smoke_packaging.py
	PREBYTE_SMOKE_DOCKER=1 python3 scripts/ci/smoke_packaging.py --binary $(CMAKE_BUILD_DIR)/prebyte --checks docker

reqpack: start
	@host_os="$$(uname -s)"; \
	host_arch="$$(uname -m)"; \
	case "$$host_os" in \
		Linux) platform=linux ;; \
		Darwin) platform=macos ;; \
		*) printf 'ReqPack package build unsupported on host OS: %s\n' "$$host_os"; exit 1 ;; \
	esac; \
	case "$$host_arch" in \
		x86_64|amd64) arch=x86_64 ;; \
		arm64|aarch64) arch=aarch64 ;; \
		*) printf 'ReqPack package build unsupported on host arch: %s\n' "$$host_arch"; exit 1 ;; \
	esac; \
	python3 scripts/ci/package_reqpack.py \
		--version "$(PREBYTE_VERSION)" \
		--platform "$$platform" \
		--arch "$$arch" \
		--binary "$(CMAKE_BUILD_DIR)/prebyte" \
		--output-dir "$(REQPACK_OUTPUT_DIR)"; \
	python3 scripts/ci/build_reqpack_index.py \
		--dist-dir "$(REQPACK_OUTPUT_DIR)" \
		--output "$(REQPACK_OUTPUT_DIR)/index.json"

reqpack-index:
	python3 scripts/ci/build_reqpack_index.py \
		--dist-dir "$(REQPACK_OUTPUT_DIR)" \
		--output "$(REQPACK_OUTPUT_DIR)/index.json"

clean:
	rm -rf build build-cmake "$(COMPARE_DIR)/bench_prebyte" crash-*
