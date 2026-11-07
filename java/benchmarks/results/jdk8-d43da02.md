# JDK 8 parsing results

Comparison of the builder baseline [`c0acd3a`](https://github.com/DanielThomas/protobuf/commit/c0acd3a8f5aedaa6705530b9704c2af527f51575) and the direct parser [`d43da02`](https://github.com/DanielThomas/protobuf/commit/d43da02858ec898079832d5dd94d1d54c40cd59d) from [PR #1](https://github.com/DanielThomas/protobuf/pull/1), using benchmark revision [`0c313de`](https://github.com/DanielThomas/protobuf/commit/0c313dec156cb6cbea2d36ee557d85190c8ead7c).

The run measures `ParseBenchmark.parse` for all 73 public binary inputs and a separate private production request. The `TextFormatBenchmarks` and `UnknownFieldBenchmarks` workloads pass their content checks but are not timed here. The private schema, payload and generated classes are not included in this contribution.

All 1,184 forks passed, with every fork's CPU and memory placement verified. Timing completed before any allocation profiling began. No retries, discarded measurements or pooling with previous runs.

| Measurement | Lower | Higher | Unresolved |
| --- | ---: | ---: | ---: |
| Time | 56 | 8 | 10 |
| Allocation | 58 | 7 | 9 |

These counts include the private request. Results use paired fork means and two-sided 95% Student t intervals, with four degrees of freedom for timing and two for allocation. The intervals are exploratory per-input comparisons without multiplicity correction. An interval spanning zero is unresolved, not evidence of equivalence. Differences below are parser minus builder.

## Run configuration

- Intel Xeon 6975P-C bare-metal host, 96 physical cores across three NUMA nodes.
- Zulu 8.98.0.106, OpenJDK 8u512-b08, for both execution and `javac`.
- Native protoc generation and Bazel JMH annotation processing, followed by actual `javac` 8 recompilation of all first-party Java sources, including generated JMH sources, with `-g -parameters -proc:none`. Third-party dependencies and JMH metadata are retained from the Bazel deploy jar.
- Fresh candidate generation for the public suite and the private request's 751-schema closure. The unchanged baseline artifacts are reused, with new measurements.
- 74 single-threaded inputs run concurrently on distinct physical cores. Each input keeps the same CPU and local NUMA memory binding across variants and rounds. Benchmark forks do not use SMT siblings. The cores are not OS-isolated.
- Measured JVM flags: `-XX:ActiveProcessorCount=1 -XX:+UseSerialGC -Xms2g -Xmx2g -XX:+AlwaysPreTouch`.
- Timing: five paired fresh forks per variant and input, three one-second warmup iterations and five one-second measured iterations. No profiler or compiler logging.
- Allocation: three paired fresh forks per variant and input, three one-second warmup and measured iterations, after all timing finishes. A separate worker-thread `ThreadMXBean` allocation-counter profiler reports bytes per operation. That profiler is not part of the benchmark target.
- Variant order alternates by round, with the starting order reversed across inputs. Each 74-fork wave completes before the next starts.

The public jars have byte-identical benchmark classes, JMH classes and JMH metadata. Input sizes, wire hashes and expected-output hashes match across variants. The public jar SHA-256 hashes are:

```text
builder: de50af705ebd4171a534e9c76dd19ad2b28c18f285fdfd66a285e5baf757cbe3
parser:  9c164d9a3b781c29a66317083bbc4beeb4fa622e302e0e50bf8c05265f02531f
```

For one public timing fork, with `JAR` selecting the corresponding actual-javac8 artifact:

```sh
numactl --physcpubind="$CPU" --membind="$NODE" \
  "$JDK8_HOME/bin/java" \
  -XX:ActiveProcessorCount=1 -XX:+UseSerialGC -Xms32m -Xmx256m \
  -Djmh.ignoreLock=true -cp "$JAR" org.openjdk.jmh.Main \
  '^com\.google\.protobuf\.benchmarks\.ParseBenchmark\.parse$' \
  -bm avgt -tu ns -jvm "$JDK8_HOME/bin/java" -t 1 -f 1 \
  -wi 3 -w 1s -i 5 -r 1s -foe true \
  -jvmArgs '-XX:ActiveProcessorCount=1 -XX:+UseSerialGC -Xms2g -Xmx2g -XX:+AlwaysPreTouch' \
  -p input="$INPUT" -rf json -rff "$RESULT"
```

The command specifies one fork. The reported results use the paired, concurrent schedule above rather than isolated runs. The private request and allocation profiler are outside the public target, so this command alone does not reproduce the complete run.

Earlier compiler diagnostics identified interpreted decoding loops for `FLAT_PROTO2_WIDE` and `ENUMS900`. No fresh compilation evidence was collected during this timing run. Treat those separately from compiled parsing.

## Time

Values are ns/op, including complete message publication. Each mean has five forks per variant.

| Input | Builder | Parser | Change | 95% CI of difference |
| --- | ---: | ---: | ---: | ---: |
| `CLOSED_ENUM_KNOWN` | 60.09 | 37.47 | -37.65% | [-23.35, -21.90] |
| `CLOSED_ENUM_UNKNOWN` | 237.72 | 227.47 | -4.31% | [-40.36, +19.85] |
| `DEPTH2` | 76.82 | 43.06 | -43.95% | [-35.84, -31.68] |
| `DEPTH8` | 226.16 | 129.70 | -42.65% | [-99.09, -93.83] |
| `ENUMS138` | 1217.01 | 1080.04 | -11.25% | [-197.29, -76.63] |
| `ENUMS139` | 7487.20 | 1081.20 | -85.56% | [-6472.17, -6339.83] |
| `ENUMS900` | 57627.52 | 74169.00 | +28.70% | [+16235.92, +16847.04] |
| `EXISTING_BINARY_EXPANDED_FIRST` | 101.56 | 67.10 | -33.93% | [-35.59, -33.34] |
| `EXISTING_BINARY_EXPANDED_MERGE` | 220.10 | 183.41 | -16.67% | [-61.29, -12.09] |
| `EXISTING_BINARY_EXTENSION_FIRST` | 214.06 | 219.76 | +2.67% | [+2.23, +9.19] |
| `EXISTING_BINARY_EXTENSION_MERGE` | 236.36 | 243.25 | +2.91% | [+4.59, +9.18] |
| `EXISTING_BINARY_PACKED_FIRST` | 86.73 | 48.67 | -43.88% | [-41.18, -34.94] |
| `EXISTING_BINARY_PACKED_MERGE` | 153.83 | 131.29 | -14.65% | [-25.14, -19.93] |
| `EXTENSION_FIRST` | 197.77 | 202.43 | +2.36% | [-0.87, +10.18] |
| `EXTENSION_MERGE` | 208.21 | 208.11 | -0.05% | [-5.43, +5.22] |
| `FLAT_EDITIONS` | 421.89 | 188.66 | -55.28% | [-272.86, -193.58] |
| `FLAT_PROTO2_COMPACT` | 95.55 | 72.88 | -23.73% | [-25.85, -19.49] |
| `FLAT_PROTO2_WIDE` | 946.47 | 665.51 | -29.68% | [-322.70, -239.21] |
| `FLAT_PROTO3` | 282.03 | 185.00 | -34.40% | [-101.24, -92.81] |
| `GROUP_FIRST` | 801.30 | 492.81 | -38.50% | [-355.73, -261.25] |
| `GROUP_MERGE` | 878.66 | 560.31 | -36.23% | [-359.75, -276.95] |
| `MAP_ENUM_UNKNOWN_EXPLICIT_CLOSED` | 254.98 | 241.88 | -5.13% | [-16.88, -9.31] |
| `MAP_ENUM_UNKNOWN_IMPLICIT_OPEN` | 160.70 | 139.34 | -13.29% | [-23.27, -19.44] |
| `MAP_MESSAGE_EXPLICIT_CLOSED` | 238.52 | 191.36 | -19.77% | [-51.72, -42.58] |
| `MAP_MESSAGE_IMPLICIT_OPEN` | 214.15 | 164.09 | -23.37% | [-53.48, -46.62] |
| `MAP_MESSAGE_REPLACE_EXPLICIT_CLOSED` | 281.09 | 230.92 | -17.85% | [-61.07, -39.27] |
| `MAP_MESSAGE_REPLACE_IMPLICIT_OPEN` | 273.40 | 216.48 | -20.82% | [-60.63, -53.20] |
| `MAP_REPLACE_EXPLICIT_CLOSED` | 212.74 | 193.22 | -9.17% | [-21.26, -17.77] |
| `MAP_REPLACE_IMPLICIT_OPEN` | 185.27 | 168.20 | -9.21% | [-19.46, -14.68] |
| `MAP_SCALAR_EXPLICIT_CLOSED` | 178.25 | 158.08 | -11.32% | [-22.24, -18.10] |
| `MAP_SCALAR_IMPLICIT_OPEN` | 164.23 | 144.46 | -12.04% | [-22.55, -16.99] |
| `ONEOF_FIRST` | 142.77 | 81.23 | -43.10% | [-63.68, -59.39] |
| `ONEOF_GROUP_FIRST` | 129.39 | 68.24 | -47.26% | [-65.21, -57.09] |
| `ONEOF_GROUP_MERGE` | 153.04 | 176.88 | +15.57% | [+20.85, +26.81] |
| `ONEOF_MERGE` | 148.46 | 198.57 | +33.75% | [+42.82, +57.40] |
| `ONEOF_SCALARS_FIRST` | 392.49 | 165.86 | -57.74% | [-234.34, -218.92] |
| `ONEOF_SCALARS_MERGE` | 436.51 | 439.72 | +0.74% | [-24.23, +30.65] |
| `OPEN_ENUM_UNKNOWN` | 57.16 | 36.24 | -36.60% | [-21.61, -20.24] |
| `ORDINARY_FIRST` | 102.06 | 66.63 | -34.72% | [-37.65, -33.23] |
| `ORDINARY_MERGE` | 204.45 | 190.61 | -6.77% | [-61.97, +34.28] |
| `ORDINARY_SCALARS_FIRST` | 391.87 | 183.92 | -53.07% | [-214.87, -201.02] |
| `ORDINARY_SCALARS_MERGE` | 425.72 | 219.93 | -48.34% | [-210.16, -201.41] |
| `PRESENCE_EDITIONS_ZERO` | 37.21 | 30.49 | -18.05% | [-7.04, -6.39] |
| `PRESENCE_PROTO2_ZERO` | 37.26 | 30.08 | -19.25% | [-9.20, -5.15] |
| `PRESENCE_PROTO3_ZERO` | 35.36 | 29.09 | -17.73% | [-6.72, -5.82] |
| `PRODUCTION_REQUEST` | 14687754.96 | 6370224.97 | -56.63% | [-9217889.80, -7417170.16] |
| `REPEATED_EXPANDED` | 384.08 | 192.06 | -49.99% | [-196.12, -187.91] |
| `REPEATED_MESSAGE` | 411.75 | 200.78 | -51.24% | [-217.55, -204.38] |
| `REPEATED_PACKED` | 119.20 | 66.35 | -44.34% | [-55.49, -50.22] |
| `REQUIRED_SCALARS` | 158.27 | 100.14 | -36.73% | [-68.30, -47.97] |
| `SCALAR494_FIELD1` | 87.76 | 49.85 | -43.19% | [-38.62, -37.18] |
| `SCALAR494_FIELD494` | 85.55 | 49.45 | -42.20% | [-36.81, -35.38] |
| `SCALAR495_FIELD1` | 150.64 | 48.52 | -67.79% | [-111.45, -92.79] |
| `SCALAR495_FIELD495` | 151.77 | 50.24 | -66.90% | [-102.93, -100.14] |
| `SCALAR_EDITIONS` | 37.68 | 30.38 | -19.37% | [-7.43, -7.17] |
| `SCALAR_PROTO2` | 36.85 | 29.32 | -20.42% | [-8.00, -7.05] |
| `SCALAR_PROTO3` | 36.57 | 28.41 | -22.30% | [-9.02, -7.28] |
| `SCALING_EXTENSION_1024` | 42796.21 | 42414.66 | -0.89% | [-1318.72, +555.61] |
| `SCALING_EXTENSION_2048` | 80635.43 | 81704.05 | +1.33% | [-2214.48, +4351.73] |
| `SCALING_ONEOF_1024` | 20495.12 | 21452.34 | +4.67% | [+149.31, +1765.14] |
| `SCALING_ONEOF_2048` | 38997.31 | 39004.24 | +0.02% | [-4333.35, +4347.19] |
| `SCALING_ORDINARY_1024` | 23349.39 | 22702.12 | -2.77% | [-1789.67, +495.13] |
| `SCALING_ORDINARY_2048` | 41574.80 | 40074.30 | -3.61% | [-3371.31, +370.30] |
| `UNKNOWN` | 491.89 | 308.51 | -37.28% | [-207.02, -159.73] |
| `WIDE_ONEOF_FIELD128_FIRST` | 293.84 | 72.74 | -75.25% | [-226.54, -215.67] |
| `WIDE_ONEOF_FIELD128_MERGE` | 291.44 | 352.34 | +20.90% | [+18.73, +103.08] |
| `WIDE_ONEOF_FIELD1_FIRST` | 280.56 | 72.80 | -74.05% | [-211.03, -204.51] |
| `WIDE_ONEOF_FIELD1_MERGE` | 321.01 | 414.37 | +29.08% | [+72.14, +114.59] |
| `WIDE_ORDINARY_ALL_FIRST` | 3497.07 | 2357.52 | -32.59% | [-1251.38, -1027.71] |
| `WIDE_ORDINARY_ALL_MERGE` | 7630.89 | 6222.17 | -18.46% | [-1527.89, -1289.55] |
| `WIDE_ORDINARY_FIELD1_FIRST` | 331.10 | 138.48 | -58.18% | [-196.33, -188.92] |
| `WIDE_ORDINARY_FIELD1_MERGE` | 398.05 | 280.95 | -29.42% | [-135.02, -99.19] |
| `WIDE_ORDINARY_FIELD65_FIRST` | 336.35 | 142.72 | -57.57% | [-196.18, -191.09] |
| `WIDE_ORDINARY_FIELD65_MERGE` | 398.34 | 292.37 | -26.60% | [-120.95, -91.00] |

## Allocation

Values are bytes/op from separate allocation forks. Each mean has three forks per variant.

| Input | Builder | Parser | Change | 95% CI of difference |
| --- | ---: | ---: | ---: | ---: |
| `CLOSED_ENUM_KNOWN` | 216.00 | 144.00 | -33.33% | [-72.00, -72.00] |
| `CLOSED_ENUM_UNKNOWN` | 704.00 | 632.00 | -10.23% | [-72.00, -72.00] |
| `DEPTH2` | 248.00 | 152.00 | -38.71% | [-96.00, -96.00] |
| `DEPTH8` | 776.00 | 392.00 | -49.48% | [-384.00, -384.00] |
| `ENUMS138` | 1272.00 | 672.00 | -47.17% | [-600.00, -600.00] |
| `ENUMS139` | 1288.00 | 680.00 | -47.21% | [-608.00, -608.00] |
| `ENUMS900` | 7560.03 | 3816.04 | -49.52% | [-3743.99, -3743.99] |
| `EXISTING_BINARY_EXPANDED_FIRST` | 320.00 | 240.00 | -25.00% | [-80.00, -80.00] |
| `EXISTING_BINARY_EXPANDED_MERGE` | 504.00 | 432.00 | -14.29% | [-72.00, -72.00] |
| `EXISTING_BINARY_EXTENSION_FIRST` | 664.00 | 696.00 | +4.82% | [+32.00, +32.00] |
| `EXISTING_BINARY_EXTENSION_MERGE` | 680.00 | 712.00 | +4.71% | [+32.00, +32.00] |
| `EXISTING_BINARY_PACKED_FIRST` | 288.00 | 208.00 | -27.78% | [-80.00, -80.00] |
| `EXISTING_BINARY_PACKED_MERGE` | 472.00 | 400.00 | -15.25% | [-72.00, -72.00] |
| `EXTENSION_FIRST` | 576.00 | 586.67 | +1.85% | [-35.23, +56.56] |
| `EXTENSION_MERGE` | 592.00 | 602.67 | +1.80% | [-35.23, +56.56] |
| `FLAT_EDITIONS` | 1317.33 | 552.00 | -58.10% | [-845.65, -685.02] |
| `FLAT_PROTO2_COMPACT` | 312.00 | 240.00 | -23.08% | [-72.00, -72.00] |
| `FLAT_PROTO2_WIDE` | 1928.00 | 984.00 | -48.96% | [-944.00, -944.00] |
| `FLAT_PROTO3` | 840.00 | 528.00 | -37.14% | [-312.00, -312.00] |
| `GROUP_FIRST` | 1976.00 | 992.00 | -49.80% | [-984.00, -984.00] |
| `GROUP_MERGE` | 2168.00 | 1216.00 | -43.91% | [-952.00, -952.00] |
| `MAP_ENUM_UNKNOWN_EXPLICIT_CLOSED` | 912.00 | 848.00 | -7.02% | [-64.00, -64.00] |
| `MAP_ENUM_UNKNOWN_IMPLICIT_OPEN` | 512.00 | 448.00 | -12.50% | [-64.00, -64.00] |
| `MAP_MESSAGE_EXPLICIT_CLOSED` | 832.00 | 600.00 | -27.88% | [-232.00, -232.00] |
| `MAP_MESSAGE_IMPLICIT_OPEN` | 760.00 | 528.00 | -30.53% | [-232.00, -232.00] |
| `MAP_MESSAGE_REPLACE_EXPLICIT_CLOSED` | 976.00 | 744.00 | -23.77% | [-232.00, -232.00] |
| `MAP_MESSAGE_REPLACE_IMPLICIT_OPEN` | 904.00 | 672.00 | -25.66% | [-232.00, -232.00] |
| `MAP_REPLACE_EXPLICIT_CLOSED` | 648.00 | 584.00 | -9.88% | [-64.00, -64.00] |
| `MAP_REPLACE_IMPLICIT_OPEN` | 549.33 | 512.00 | -6.80% | [-152.07, +77.40] |
| `MAP_SCALAR_EXPLICIT_CLOSED` | 584.00 | 520.00 | -10.96% | [-64.00, -64.00] |
| `MAP_SCALAR_IMPLICIT_OPEN` | 512.00 | 448.00 | -12.50% | [-64.00, -64.00] |
| `ONEOF_FIRST` | 440.00 | 264.00 | -40.00% | [-176.00, -176.00] |
| `ONEOF_GROUP_FIRST` | 392.00 | 232.00 | -40.82% | [-160.00, -160.00] |
| `ONEOF_GROUP_MERGE` | 440.00 | 488.00 | +10.91% | [+48.00, +48.00] |
| `ONEOF_MERGE` | 440.00 | 488.00 | +10.91% | [+48.00, +48.00] |
| `ONEOF_SCALARS_FIRST` | 1320.00 | 496.00 | -62.42% | [-824.00, -824.00] |
| `ONEOF_SCALARS_MERGE` | 1320.00 | 1376.00 | +4.24% | [+56.00, +56.00] |
| `OPEN_ENUM_UNKNOWN` | 216.00 | 144.00 | -33.33% | [-72.00, -72.00] |
| `ORDINARY_FIRST` | 328.00 | 240.00 | -26.83% | [-88.00, -88.00] |
| `ORDINARY_MERGE` | 520.00 | 440.00 | -15.38% | [-80.00, -80.00] |
| `ORDINARY_SCALARS_FIRST` | 1264.00 | 496.00 | -60.76% | [-768.00, -768.00] |
| `ORDINARY_SCALARS_MERGE` | 1392.00 | 664.00 | -52.30% | [-728.00, -728.00] |
| `PRESENCE_EDITIONS_ZERO` | 152.00 | 112.00 | -26.32% | [-40.00, -40.00] |
| `PRESENCE_PROTO2_ZERO` | 152.00 | 112.00 | -26.32% | [-40.00, -40.00] |
| `PRESENCE_PROTO3_ZERO` | 152.00 | 112.00 | -26.32% | [-40.00, -40.00] |
| `PRODUCTION_REQUEST` | 41502103.68 | 13773789.32 | -66.81% | [-27767903.53, -27688725.19] |
| `REPEATED_EXPANDED` | 1272.00 | 544.00 | -57.23% | [-728.00, -728.00] |
| `REPEATED_MESSAGE` | 1424.00 | 616.00 | -56.74% | [-808.00, -808.00] |
| `REPEATED_PACKED` | 360.00 | 272.00 | -24.44% | [-88.00, -88.00] |
| `REQUIRED_SCALARS` | 504.00 | 320.00 | -36.51% | [-184.00, -184.00] |
| `SCALAR494_FIELD1` | 280.00 | 176.00 | -37.14% | [-104.00, -104.00] |
| `SCALAR494_FIELD494` | 280.00 | 176.00 | -37.14% | [-104.00, -104.00] |
| `SCALAR495_FIELD1` | 280.00 | 176.00 | -37.14% | [-104.00, -104.00] |
| `SCALAR495_FIELD495` | 280.00 | 176.00 | -37.14% | [-104.00, -104.00] |
| `SCALAR_EDITIONS` | 152.00 | 112.00 | -26.32% | [-40.00, -40.00] |
| `SCALAR_PROTO2` | 152.00 | 112.00 | -26.32% | [-40.00, -40.00] |
| `SCALAR_PROTO3` | 152.00 | 112.00 | -26.32% | [-40.00, -40.00] |
| `SCALING_EXTENSION_1024` | 73061.37 | 67613.75 | -7.46% | [-28906.53, +18011.28] |
| `SCALING_EXTENSION_2048` | 126928.04 | 116045.19 | -8.57% | [-104858.94, +83093.23] |
| `SCALING_ONEOF_1024` | 45536.01 | 51538.68 | +13.18% | [-17518.50, +29523.84] |
| `SCALING_ONEOF_2048` | 83029.36 | 83560.02 | +0.64% | [-80889.46, +81950.78] |
| `SCALING_ORDINARY_1024` | 52586.68 | 51864.01 | -1.37% | [-41442.65, +39997.32] |
| `SCALING_ORDINARY_2048` | 84608.02 | 83885.36 | -0.85% | [-82162.64, +80717.31] |
| `UNKNOWN` | 1693.33 | 976.00 | -42.36% | [-800.07, -634.60] |
| `WIDE_ONEOF_FIELD128_FIRST` | 912.00 | 256.00 | -71.93% | [-656.00, -656.00] |
| `WIDE_ONEOF_FIELD128_MERGE` | 912.00 | 952.00 | +4.39% | [+40.00, +40.00] |
| `WIDE_ONEOF_FIELD1_FIRST` | 912.00 | 256.00 | -71.93% | [-656.00, -656.00] |
| `WIDE_ONEOF_FIELD1_MERGE` | 912.00 | 952.00 | +4.39% | [+40.00, +40.00] |
| `WIDE_ORDINARY_ALL_FIRST` | 11856.00 | 8696.00 | -26.65% | [-3160.00, -3160.00] |
| `WIDE_ORDINARY_ALL_MERGE` | 22792.00 | 19896.00 | -12.71% | [-2896.00, -2896.00] |
| `WIDE_ORDINARY_FIELD1_FIRST` | 1104.00 | 504.00 | -54.35% | [-600.00, -600.00] |
| `WIDE_ORDINARY_FIELD1_MERGE` | 1288.00 | 952.00 | -26.09% | [-336.00, -336.00] |
| `WIDE_ORDINARY_FIELD65_FIRST` | 1104.00 | 504.00 | -54.35% | [-600.00, -600.00] |
| `WIDE_ORDINARY_FIELD65_MERGE` | 1288.00 | 952.00 | -26.09% | [-336.00, -336.00] |
