.PHONY: all start run test coverage benchmark compare-benchmark configure reqpack reqpack-index clean

CMAKE_PRESET ?= dev
CMAKE_BUILD_DIR := build-cmake/dev
COVERAGE_MIN_LINE ?= 85
COVERAGE_BUILD_DIR := build-cmake/coverage
CMAKE_CACHE := $(CMAKE_BUILD_DIR)/CMakeCache.txt
COMPARE_DIR := tools/benchmark_compare
PREBYTE_VERSION ?= $(shell python3 -c 'import pathlib,re; text = pathlib.Path("CMakeLists.txt").read_text(encoding="utf-8"); match = re.search(r"project\([^\n]*VERSION\s+([^\s)]+)", text); print(match.group(1) if match else "0.0.0")')
REQPACK_OUTPUT_DIR ?= dist

all: start

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

benchmark: configure
	cmake --build --preset $(CMAKE_PRESET) --target prebyte_benchmarks
	./$(CMAKE_BUILD_DIR)/prebyte_benchmarks
	$(MAKE) compare-benchmark

compare-benchmark: configure
	cmake --build --preset $(CMAKE_PRESET) --target compare-benchmark

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
	rm -rf build build-cmake "$(COMPARE_DIR)/bench_prebyte"
