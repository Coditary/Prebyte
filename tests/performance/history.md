# Benchmark History

Every benchmark run appends a new timestamped section. Compare sections to track speed over time.
## 2026-05-12 22:06:31

| Case | Time (us) | Output bytes |
| --- | ---: | ---: |
| simple-variable | 39 | 10 |
| if-include | 34 | 31 |
| profile-merge | 9 | 5 |

## 2026-05-12 22:14:56

| Case | Time (us) | Output bytes |
| --- | ---: | ---: |
| simple-variable | 41 | 10 |
| if-include | 30 | 31 |
| profile-merge | 8 | 5 |

## 2026-05-12 22:44:14

| Case | Time (us) | Output bytes |
| --- | ---: | ---: |
| simple-variable | 36 | 10 |
| if-include | 32 | 31 |
| profile-merge | 8 | 5 |
| lua-inline | 77 | 4 |
| lua-condition | 49 | 3 |

## 2026-05-12 22:45:45

| Case | Time (us) | Output bytes |
| --- | ---: | ---: |
| simple-variable | 36 | 10 |
| if-include | 32 | 31 |
| profile-merge | 8 | 5 |
| lua-inline | 77 | 4 |
| lua-condition | 49 | 3 |

## 2026-05-12 22:57:34

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 41 | baseline | +5 | 0 | 0 | 10 |
| if-include | 31 | -10 | -1 | 0 | 0 | 31 |
| profile-merge | 8 | -33 | +0 | 0 | 0 | 5 |
| lua-inline | 79 | +38 | +2 | 0 | 1 | 4 |
| lua-condition | 52 | +11 | +3 | 0 | 1 | 3 |

## 2026-05-12 23:03:48

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 45 | baseline | +4 | 0 | 0 | 10 |
| if-include | 30 | -15 | -1 | 0 | 0 | 31 |
| profile-merge | 7 | -38 | -1 | 0 | 0 | 5 |
| lua-inline | 79 | +34 | +0 | 0 | 1 | 4 |
| lua-condition | 51 | +6 | -1 | 0 | 1 | 3 |

## 2026-05-12 23:14:33

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 26 | baseline | -19 | 0 | 0 | 10 |
| if-include | 27 | +1 | -3 | 0 | 0 | 31 |
| profile-merge | 7 | -19 | +0 | 0 | 0 | 5 |
| lua-inline | 68 | +42 | -11 | 0 | 1 | 4 |
| lua-condition | 43 | +17 | -8 | 0 | 1 | 3 |

## 2026-05-12 23:21:36

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 25 | baseline | -1 | 0 | 0 | 10 |
| if-include | 27 | +2 | +0 | 0 | 0 | 31 |
| profile-merge | 7 | -18 | +0 | 0 | 0 | 5 |
| lua-inline | 71 | +46 | +3 | 0 | 1 | 4 |
| lua-condition | 43 | +18 | +0 | 0 | 1 | 3 |

## 2026-05-12 23:28:39

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 25 | baseline | +0 | 0 | 0 | 10 |
| if-include | 28 | +3 | +1 | 0 | 0 | 31 |
| profile-merge | 8 | -17 | +1 | 0 | 0 | 5 |
| lua-inline | 70 | +45 | -1 | 0 | 1 | 4 |
| lua-repeated | 38 | +13 | new | 1 | 1 | 8 |
| lua-condition | 42 | +17 | -1 | 0 | 1 | 3 |

## 2026-05-12 23:38:21

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 33 | baseline | +8 | 0 | 0 | 10 |
| if-include | 28 | -5 | +0 | 0 | 0 | 31 |
| profile-merge | 7 | -26 | -1 | 0 | 0 | 5 |
| lua-inline | 75 | +42 | +5 | 0 | 1 | 4 |
| lua-repeated | 38 | +5 | +0 | 1 | 1 | 8 |
| lua-condition | 41 | +8 | -1 | 0 | 1 | 3 |

## 2026-05-12 23:41:15

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 26 | baseline | -7 | 0 | 0 | 10 |
| if-include | 28 | +2 | +0 | 0 | 0 | 31 |
| profile-merge | 8 | -18 | +1 | 0 | 0 | 5 |
| lua-inline | 77 | +51 | +2 | 0 | 1 | 4 |
| lua-repeated | 39 | +13 | +1 | 1 | 1 | 8 |
| lua-condition | 42 | +16 | +1 | 0 | 1 | 3 |

## 2026-05-13 00:25:55

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 39 | baseline | +13 | 0 | 0 | 10 |
| if-include | 25 | -14 | -3 | 0 | 0 | 31 |
| profile-merge | 7 | -32 | -1 | 0 | 0 | 5 |
| lua-inline | 70 | +31 | -7 | 0 | 1 | 4 |
| lua-repeated | 38 | -1 | -1 | 1 | 1 | 8 |
| lua-condition | 41 | +2 | -1 | 0 | 1 | 3 |

## 2026-05-13 00:30:21

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 49 | baseline | +10 | 0 | 0 | 10 |
| if-include | 34 | -15 | +9 | 0 | 0 | 31 |
| profile-merge | 16 | -33 | +9 | 0 | 0 | 5 |
| lua-inline | 98 | +49 | +28 | 0 | 1 | 4 |
| lua-repeated | 50 | +1 | +12 | 1 | 1 | 8 |
| lua-condition | 55 | +6 | +14 | 0 | 1 | 3 |

## 2026-05-13 15:17:24

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 42 | baseline | -7 | 0 | 0 | 10 |
| if-include | 34 | -8 | +0 | 0 | 0 | 31 |
| profile-merge | 10 | -32 | -6 | 0 | 0 | 5 |
| lua-inline | 78 | +36 | -20 | 0 | 1 | 4 |
| lua-repeated | 44 | +2 | -6 | 1 | 1 | 8 |
| lua-condition | 42 | baseline | -13 | 0 | 1 | 3 |

## 2026-05-13 15:19:43

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 49 | baseline | +7 | 0 | 0 | 10 |
| if-include | 31 | -18 | -3 | 0 | 0 | 31 |
| profile-merge | 9 | -40 | -1 | 0 | 0 | 5 |
| lua-inline | 88 | +39 | +10 | 0 | 1 | 4 |
| lua-repeated | 48 | -1 | +4 | 1 | 1 | 8 |
| lua-condition | 53 | +4 | +11 | 0 | 1 | 3 |

## 2026-05-13 16:51:17

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 55 | baseline | +6 | 0 | 0 | 10 |
| if-include | 29 | -26 | -2 | 0 | 0 | 31 |
| profile-merge | 9 | -46 | +0 | 0 | 0 | 5 |
| lua-inline | 85 | +30 | -3 | 0 | 1 | 4 |
| lua-repeated | 43 | -12 | -5 | 1 | 1 | 8 |
| lua-condition | 49 | -6 | -4 | 0 | 1 | 3 |

## 2026-05-13 17:02:58

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 57 | baseline | +2 | 0 | 0 | 10 |
| if-include | 47 | -10 | +18 | 0 | 0 | 31 |
| profile-merge | 15 | -42 | +6 | 0 | 0 | 5 |
| lua-inline | 83 | +26 | -2 | 0 | 1 | 4 |
| lua-repeated | 88 | +31 | +45 | 1 | 1 | 8 |
| lua-condition | 52 | -5 | +3 | 0 | 1 | 3 |

## 2026-05-14 13:49:48

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 286 | baseline | +229 | 0 | 0 | 10 |
| if-include | 411 | +125 | +364 | 0 | 0 | 31 |
| profile-merge | 102 | -184 | +87 | 0 | 0 | 5 |
| lua-inline | 229 | -57 | +146 | 0 | 1 | 4 |
| lua-repeated | 586 | +300 | +498 | 1 | 1 | 8 |
| lua-condition | 161 | -125 | +109 | 0 | 1 | 3 |

## 2026-07-25 16:43:10

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 243 | baseline | -43 | 0 | 0 | 10 |
| if-include | 227 | -16 | -184 | 0 | 0 | 31 |
| profile-merge | 91 | -152 | -11 | 0 | 0 | 5 |
| lua-inline | 242 | -1 | +13 | 0 | 1 | 4 |
| lua-repeated | 67 | -176 | -519 | 1 | 1 | 8 |
| lua-condition | 139 | -104 | -22 | 0 | 1 | 3 |

## 2026-07-25 17:25:03

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 73 | baseline | -170 | 0 | 0 | 10 |
| if-include | 44 | -29 | -183 | 0 | 0 | 31 |
| profile-merge | 13 | -60 | -78 | 0 | 0 | 5 |
| lua-inline | 126 | +53 | -116 | 0 | 1 | 4 |
| lua-repeated | 81 | +8 | +14 | 1 | 1 | 8 |
| lua-condition | 69 | -4 | -70 | 0 | 1 | 3 |

## 2026-07-25 17:48:43

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 54 | baseline | -19 | 0 | 0 | 10 |
| if-include | 38 | -16 | -6 | 0 | 0 | 31 |
| profile-merge | 10 | -44 | -3 | 0 | 0 | 5 |
| lua-inline | 112 | +58 | -14 | 0 | 1 | 4 |
| lua-repeated | 140 | +86 | +59 | 1 | 1 | 8 |
| lua-condition | 65 | +11 | -4 | 0 | 1 | 3 |

## 2026-07-25 18:17:54

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 51 | baseline | -3 | 0 | 0 | 10 |
| if-include | 33 | -18 | -5 | 0 | 0 | 31 |
| profile-merge | 9 | -42 | -1 | 0 | 0 | 5 |
| lua-inline | 104 | +53 | -8 | 0 | 1 | 4 |
| lua-repeated | 65 | +14 | -75 | 1 | 1 | 8 |
| lua-condition | 56 | +5 | -9 | 0 | 1 | 3 |

## 2026-07-25 18:29:57

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 54 | baseline | +3 | 0 | 0 | 10 |
| if-include | 34 | -20 | +1 | 0 | 0 | 31 |
| profile-merge | 10 | -44 | +1 | 0 | 0 | 5 |
| lua-inline | 105 | +51 | +1 | 0 | 1 | 4 |
| lua-repeated | 137 | +83 | +72 | 1 | 1 | 8 |
| lua-condition | 61 | +7 | +5 | 0 | 1 | 3 |

## 2026-07-25 18:39:04

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 62 | baseline | +8 | 0 | 0 | 10 |
| if-include | 38 | -24 | +4 | 0 | 0 | 31 |
| profile-merge | 9 | -53 | -1 | 0 | 0 | 5 |
| lua-inline | 109 | +47 | +4 | 0 | 1 | 4 |
| lua-repeated | 56 | -6 | -81 | 1 | 1 | 8 |
| lua-condition | 56 | -6 | -5 | 0 | 1 | 3 |

## 2026-07-25 18:53:54

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 50 | baseline | -12 | 0 | 0 | 10 |
| if-include | 29 | -21 | -9 | 0 | 0 | 31 |
| profile-merge | 9 | -41 | +0 | 0 | 0 | 5 |
| lua-inline | 101 | +51 | -8 | 0 | 1 | 4 |
| lua-repeated | 56 | +6 | +0 | 1 | 1 | 8 |
| lua-condition | 56 | +6 | +0 | 0 | 1 | 3 |

## 2026-07-25 18:59:09

| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 48 | baseline | -2 | 0 | 0 | 10 |
| if-include | 32 | -16 | +3 | 0 | 0 | 31 |
| profile-merge | 9 | -39 | +0 | 0 | 0 | 5 |
| lua-inline | 99 | +51 | -2 | 0 | 1 | 4 |
| lua-repeated | 59 | +11 | +3 | 1 | 1 | 8 |
| lua-condition | 55 | +7 | -1 | 0 | 1 | 3 |

## 2026-07-25 19:16:57

### Single render (CLI)

| Case | Time (us) | Per entry (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 74 | - | baseline | +26 | 0 | 0 | 10 |
| if-include | 43 | - | -31 | +11 | 0 | 0 | 31 |
| profile-merge | 11 | - | -63 | +2 | 0 | 0 | 5 |
| lua-inline | 123 | - | +49 | +24 | 0 | 1 | 4 |
| lua-repeated | 70 | - | -4 | +11 | 1 | 1 | 8 |
| lua-condition | 67 | - | -7 | +12 | 0 | 1 | 3 |

### Batch (CLI, 8 entries)

| Case | Time (us) | Per entry (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| batch-variable | 128 | 16 | +54 | new | 0 | 0 | 107 |

## 2026-07-25 19:17:45

### Single render (CLI)

| Case | Time (us) | Per entry (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 69 | - | baseline | -5 | 0 | 0 | 10 |
| if-include | 41 | - | -28 | -2 | 0 | 0 | 31 |
| profile-merge | 11 | - | -58 | +0 | 0 | 0 | 5 |
| lua-inline | 122 | - | +53 | -1 | 0 | 1 | 4 |
| lua-repeated | 71 | - | +2 | +1 | 1 | 1 | 8 |
| lua-condition | 67 | - | -2 | +0 | 0 | 1 | 3 |

### Batch (CLI, 8 entries)

| Case | Time (us) | Per entry (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| batch-variable | 45 | 5 | -24 | -83 | 0 | 0 | 107 |

## 2026-07-25 19:20:04

### Single render (CLI)

| Case | Time (us) | Per entry (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 70 | - | baseline | +1 | 0 | 0 | 10 |
| if-include | 41 | - | -29 | +0 | 0 | 0 | 31 |
| profile-merge | 11 | - | -59 | +0 | 0 | 0 | 5 |
| lua-inline | 124 | - | +54 | +2 | 0 | 1 | 4 |
| lua-repeated | 117 | - | +47 | +46 | 1 | 1 | 8 |
| lua-condition | 94 | - | +24 | +27 | 0 | 1 | 3 |

### Batch (CLI, 8 entries)

| Case | Time (us) | Per entry (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| batch-variable | 47 | 5 | -23 | +2 | 0 | 0 | 107 |

## 2026-07-25 19:29:10

### Single render (CLI)

| Case | Time (us) | Per entry (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 71 | - | baseline | +1 | 0 | 0 | 10 |
| if-include | 40 | - | -31 | -1 | 0 | 0 | 31 |
| profile-merge | 10 | - | -61 | -1 | 0 | 0 | 5 |
| lua-inline | 123 | - | +52 | -1 | 0 | 1 | 4 |
| lua-repeated | 70 | - | -1 | -47 | 1 | 1 | 8 |
| lua-condition | 66 | - | -5 | -28 | 0 | 1 | 3 |

### Batch (CLI, 8 entries)

| Case | Time (us) | Per entry (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| batch-variable | 42 | 5 | -29 | -5 | 0 | 0 | 107 |

## 2026-07-25 19:33:39

### Single render (CLI)

| Case | Time (us) | Per entry (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| simple-variable | 48 | - | baseline | -23 | 0 | 0 | 10 |
| if-include | 30 | - | -18 | -10 | 0 | 0 | 31 |
| profile-merge | 9 | - | -39 | -1 | 0 | 0 | 5 |
| lua-inline | 99 | - | +51 | -24 | 0 | 1 | 4 |
| lua-repeated | 57 | - | +9 | -13 | 1 | 1 | 8 |
| lua-condition | 53 | - | +5 | -13 | 0 | 1 | 3 |

### Batch (CLI, 8 entries)

| Case | Time (us) | Per entry (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| batch-variable | 98 | 12 | +50 | +56 | 0 | 0 | 107 |

