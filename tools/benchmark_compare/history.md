# Benchmark Compare History

Each `make compare-benchmark` run appends a timestamped report below.

---

# Benchmark Compare Report

Generated: 2026-07-25 19:28:47
Method: in-process render benchmark, median of 5 runs.
Config: `tools/benchmark_compare/manifest.json` (iterations and cases).
Note: Askama compiles templates at build time; cold measures fresh context per render.
Note: Go/Askama warm-memoized mirrors warm-execute (no output memoization).
Note: Values in parentheses are slower than the fastest engine for that case.

## Render benchmarks
Single-template render micro-benchmarks compared across engines.

### Cold (cold)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.726 (48.58×) | 0.616 (135.46×) | 5.536 (189.19×) | 2.385 (60.52×) |
| go-text-template | 1.431 (95.82×) | 2.224 (488.77×) | 9.027 (308.52×) | 6.158 (156.27×) |
| askama | 0.015 | 0.005 | 0.029 | 0.039 |

### Warm execute (warm-execute)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.054 (3.60×) | 0.045 (5.51×) | 0.086 (3.22×) | 0.299 (10.96×) |
| go-text-template | 0.149 (9.87×) | 0.128 (15.79×) | 0.270 (10.13×) | 0.569 (20.90×) |
| askama | 0.015 | 0.008 | 0.027 | 0.027 |

### Warm memoized (warm-memoized)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.023 (1.52×) | 0.023 (2.90×) | 0.038 (1.40×) | 0.019 |
| go-text-template | 0.149 (9.87×) | 0.128 (15.79×) | 0.270 (10.13×) | 0.569 (30.36×) |
| askama | 0.015 | 0.008 | 0.027 | 0.027 (1.45×) |

### MT cold (mt-cold)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.143 (18.46×) | 0.159 (34.49×) | 1.239 (68.53×) | 0.396 (22.09×) |
| go-text-template | 0.566 (73.19×) | 0.733 (158.70×) | 3.176 (175.66×) | 1.704 (95.14×) |
| askama | 0.008 | 0.005 | 0.018 | 0.018 |

### MT warm execute (mt-warm-execute)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.013 (1.60×) | 0.012 (2.44×) | 0.035 (1.84×) | 0.068 (4.21×) |
| go-text-template | 0.044 (5.31×) | 0.040 (8.51×) | 0.094 (4.90×) | 0.125 (7.81×) |
| askama | 0.008 | 0.005 | 0.019 | 0.016 |

### MT warm memoized (mt-warm-memoized)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.008 | 0.008 (1.69×) | 0.028 (1.44×) | 0.027 (1.65×) |
| go-text-template | 0.044 (5.74×) | 0.040 (8.51×) | 0.094 (4.90×) | 0.125 (7.81×) |
| askama | 0.008 (1.08×) | 0.005 | 0.019 | 0.016 |

## Batch benchmarks
Render multiple variable sets from one template. Prebyte uses BatchProcessor; other engines loop single renders. Each case uses 8 entries.

### Batch warm (batch-warm)
_Prebyte-only mode (BatchProcessor API)._

| Engine | batch-variable (µs/entry) |
| --- | :---: |
| prebyte | 0.153 |

### Batch cold (batch-cold)
_Prebyte-only mode (BatchProcessor API)._

| Engine | batch-variable (µs/entry) |
| --- | :---: |
| prebyte | 0.980 |

### Sequential warm (sequential-warm)
| Engine | batch-variable (µs/entry) |
| --- | :---: |
| prebyte | 0.157 (3.74×) |
| go-text-template | 0.268 (6.39×) |
| askama | 0.042 |

### Sequential cold (sequential-cold)
| Engine | batch-variable (µs/entry) |
| --- | :---: |
| prebyte | 2.102 (50.73×) |
| go-text-template | 2.082 (50.24×) |
| askama | 0.041 |


---

# Benchmark Compare Report

Generated: 2026-07-25 19:29:13
Method: in-process render benchmark, median of 5 runs.
Config: `tools/benchmark_compare/manifest.json` (iterations and cases).
Note: Askama compiles templates at build time; cold measures fresh context per render.
Note: Go/Askama warm-memoized mirrors warm-execute (no output memoization).
Note: Values in parentheses are slower than the fastest engine for that case.

## Render benchmarks
Single-template render micro-benchmarks compared across engines.

### Cold (cold)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.707 (47.67×) | 0.624 (138.42×) | 5.524 (185.56×) | 2.399 (62.32×) |
| go-text-template | 1.477 (99.58×) | 2.304 (511.06×) | 8.492 (285.26×) | 6.438 (167.24×) |
| askama | 0.015 | 0.005 | 0.030 | 0.038 |

### Warm execute (warm-execute)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.048 (3.19×) | 0.045 (5.59×) | 0.086 (3.18×) | 0.298 (11.15×) |
| go-text-template | 0.163 (10.93×) | 0.150 (18.64×) | 0.311 (11.54×) | 0.666 (24.89×) |
| askama | 0.015 | 0.008 | 0.027 | 0.027 |

### Warm memoized (warm-memoized)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.022 (1.49×) | 0.024 (3.01×) | 0.037 (1.36×) | 0.019 |
| go-text-template | 0.163 (10.93×) | 0.150 (18.64×) | 0.311 (11.54×) | 0.666 (35.52×) |
| askama | 0.015 | 0.008 | 0.027 | 0.027 (1.43×) |

### MT cold (mt-cold)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.142 (25.71×) | 0.151 (37.30×) | 1.233 (66.21×) | 0.400 (23.55×) |
| go-text-template | 0.524 (94.69×) | 0.694 (171.63×) | 2.346 (126.02×) | 1.608 (94.64×) |
| askama | 0.006 | 0.004 | 0.019 | 0.017 |

### MT warm execute (mt-warm-execute)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.012 (2.09×) | 0.011 (2.41×) | 0.036 (1.97×) | 0.063 (4.07×) |
| go-text-template | 0.037 (6.35×) | 0.042 (9.21×) | 0.080 (4.43×) | 0.123 (7.91×) |
| askama | 0.006 | 0.005 | 0.018 | 0.015 |

### MT warm memoized (mt-warm-memoized)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.008 (1.37×) | 0.008 (1.72×) | 0.027 (1.49×) | 0.022 (1.43×) |
| go-text-template | 0.037 (6.35×) | 0.042 (9.21×) | 0.080 (4.43×) | 0.123 (7.91×) |
| askama | 0.006 | 0.005 | 0.018 | 0.015 |

## Batch benchmarks
Render multiple variable sets from one template. Prebyte uses BatchProcessor; other engines loop single renders. Each case uses 8 entries.

### Batch warm (batch-warm)
_Prebyte-only mode (BatchProcessor API)._

| Engine | batch-variable (µs/entry) |
| --- | :---: |
| prebyte | 0.146 |

### Batch cold (batch-cold)
_Prebyte-only mode (BatchProcessor API)._

| Engine | batch-variable (µs/entry) |
| --- | :---: |
| prebyte | 0.943 |

### Sequential warm (sequential-warm)
| Engine | batch-variable (µs/entry) |
| --- | :---: |
| prebyte | 0.145 (3.47×) |
| go-text-template | 0.259 (6.20×) |
| askama | 0.042 |

### Sequential cold (sequential-cold)
| Engine | batch-variable (µs/entry) |
| --- | :---: |
| prebyte | 2.083 (51.40×) |
| go-text-template | 2.099 (51.79×) |
| askama | 0.041 |


---

# Benchmark Compare Report

Generated: 2026-07-25 19:33:51
Method: in-process render benchmark, median of 5 runs.
Config: `tools/benchmark_compare/manifest.json` (iterations and cases).
Note: Askama compiles templates at build time; cold measures fresh context per render.
Note: Go/Askama warm-memoized mirrors warm-execute (no output memoization).
Note: Values in parentheses are slower than the fastest engine for that case.

## Render benchmarks
Single-template render micro-benchmarks compared across engines.

### Cold (cold)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.559 (37.58×) | 0.610 (143.54×) | 5.478 (186.40×) | 2.370 (60.47×) |
| go-text-template | 1.418 (95.22×) | 2.306 (542.87×) | 8.568 (291.54×) | 6.372 (162.56×) |
| askama | 0.015 | 0.004 | 0.029 | 0.039 |

### Warm execute (warm-execute)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.047 (3.13×) | 0.045 (5.47×) | 0.087 (3.24×) | 0.297 (11.17×) |
| go-text-template | 0.162 (10.71×) | 0.142 (17.29×) | 0.275 (10.23×) | 0.603 (22.67×) |
| askama | 0.015 | 0.008 | 0.027 | 0.027 |

### Warm memoized (warm-memoized)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.022 (1.47×) | 0.024 (2.94×) | 0.037 (1.36×) | 0.019 |
| go-text-template | 0.162 (10.71×) | 0.142 (17.29×) | 0.275 (10.23×) | 0.603 (32.11×) |
| askama | 0.015 | 0.008 | 0.027 | 0.027 (1.42×) |

### MT cold (mt-cold)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.140 (22.78×) | 0.156 (35.57×) | 1.223 (68.91×) | 0.383 (21.49×) |
| go-text-template | 0.520 (84.89×) | 0.677 (154.10×) | 2.992 (168.64×) | 1.599 (89.80×) |
| askama | 0.006 | 0.004 | 0.018 | 0.018 |

### MT warm execute (mt-warm-execute)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.012 (2.33×) | 0.012 (2.53×) | 0.037 (2.20×) | 0.064 (4.15×) |
| go-text-template | 0.043 (8.02×) | 0.035 (7.57×) | 0.086 (5.06×) | 0.154 (9.92×) |
| askama | 0.005 | 0.005 | 0.017 | 0.016 |

### MT warm memoized (mt-warm-memoized)
| Engine | simple-variable (µs/render) | conditional (µs/render) | include-if (µs/render) | control-flow (µs/render) |
| --- | :---: | :---: | :---: | :---: |
| prebyte | 0.008 (1.48×) | 0.008 (1.81×) | 0.028 (1.64×) | 0.021 (1.38×) |
| go-text-template | 0.043 (8.02×) | 0.035 (7.57×) | 0.086 (5.06×) | 0.154 (9.92×) |
| askama | 0.005 | 0.005 | 0.017 | 0.016 |

## Batch benchmarks
Render multiple variable sets from one template. Prebyte uses BatchProcessor; other engines loop single renders. Each case uses 8 entries.

### Batch warm (batch-warm)
_Prebyte-only mode (BatchProcessor API)._

| Engine | batch-variable (µs/entry) |
| --- | :---: |
| prebyte | 0.145 |

### Batch cold (batch-cold)
_Prebyte-only mode (BatchProcessor API)._

| Engine | batch-variable (µs/entry) |
| --- | :---: |
| prebyte | 0.928 |

### Sequential warm (sequential-warm)
| Engine | batch-variable (µs/entry) |
| --- | :---: |
| prebyte | 0.146 (3.53×) |
| go-text-template | 0.257 (6.23×) |
| askama | 0.041 |

### Sequential cold (sequential-cold)
| Engine | batch-variable (µs/entry) |
| --- | :---: |
| prebyte | 2.009 (48.16×) |
| go-text-template | 2.151 (51.58×) |
| askama | 0.042 |

