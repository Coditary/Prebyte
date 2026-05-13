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

