.PHONY: all start run test benchmark compare-benchmark configure clean

CMAKE_PRESET ?= dev
CMAKE_BUILD_DIR := build-cmake/dev
COMPARE_DIR := tools/benchmark_compare

all: start

configure:
	cmake --preset $(CMAKE_PRESET)

start: configure
	cmake --build --preset $(CMAKE_PRESET) --target prebyte

run: start
	./$(CMAKE_BUILD_DIR)/prebyte

test: configure
	cmake --build --preset $(CMAKE_PRESET) --target prebyte_tests
	ctest --preset $(CMAKE_PRESET)

benchmark: configure
	cmake --build --preset $(CMAKE_PRESET) --target prebyte_benchmarks
	./$(CMAKE_BUILD_DIR)/prebyte_benchmarks
	$(MAKE) compare-benchmark

compare-benchmark: configure
	cmake --build --preset $(CMAKE_PRESET) --target compare-benchmark

clean:
	rm -rf build build-cmake "$(COMPARE_DIR)/bench_prebyte"
