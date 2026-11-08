# JDK 8 handoff results

Builder baseline [`c0acd3a`](https://github.com/DanielThomas/protobuf/commit/c0acd3a8f5aedaa6705530b9704c2af527f51575) versus parser base [`d43da02`](https://github.com/DanielThomas/protobuf/commit/d43da02858ec898079832d5dd94d1d54c40cd59d) plus the current handoff changes, using unchanged benchmark revision [`0c313de`](https://github.com/DanielThomas/protobuf/commit/0c313dec156cb6cbea2d36ee557d85190c8ead7c). The handoff changes remain an unpublished working-tree patch, SHA-256 `78f185ad47d410d60aae21c70d98e2fce349ede8aba5e32970da3cc3b78a0770`. These results should be attributed to that candidate rather than the published PR #1 revision alone.

All 73 public binary inputs and the representative private request were measured. All 1,184 fresh forks passed, with CPU and memory placement verified. Timing completed before allocation profiling began. No retries, discarded results or pooling with earlier measurements. The separate text-format and unknown-field workloads passed their input checks but were not timed.

| Measurement | Higher | Lower | Unresolved |
| --- | ---: | ---: | ---: |
| Throughput | 59 | 8 | 7 |
| Allocation | 9 | 61 | 4 |

Throughput is the mean of the five per-fork rates, each calculated as `1e9 / mean ns/op`. It is derived from average-time measurements, not a separate JMH throughput-mode run. Its confidence intervals use paired fork rates. Allocation is bytes per parse from three separate paired forks. Higher throughput and lower allocation are better. Differences are parser minus builder. Intervals spanning zero are unresolved, not evidence of equivalence. The two-sided 95% Student t intervals are exploratory per-input comparisons without multiplicity correction, with four degrees of freedom for throughput and two for allocation.

## Configuration

- Bare-metal Intel Xeon 6975P-C, 96 physical cores over three NUMA nodes. Zulu OpenJDK 8u512-b08 for both javac and execution.
- Same 74-lane schedule as the [previous full run](jdk8-d43da02.md): one thread and distinct physical core per input, fixed local NUMA memory binding across variants and rounds. Benchmark forks do not use SMT siblings. Cores are not OS-isolated.
- Measured JVM flags: `-XX:ActiveProcessorCount=1 -XX:+UseSerialGC -Xms2g -Xmx2g -XX:+AlwaysPreTouch`.
- Five paired timing forks per variant/input, three one-second warmups and five one-second measurements. No profiler, agent, forced inlining or compiler logging.
- Three separate paired allocation forks per variant/input, three one-second warmups and measurements, after all 740 timing forks finish. The unchanged worker-thread allocation-counter profiler and `gc:alloc=false` are used for these 444 forks. The worker profiler is outside the public target.
- Variant order alternates by round and starting order reverses across inputs. A barrier separates each 74-fork wave. Placement is observed during startup or early warmup, with no further process-file reads during measurement.
- Frozen public jars compiled with actual javac 8, using `-g -parameters -proc:none`, are reused with new measurements. All 385 benchmark/JMH classes and metadata entries remain byte-identical across variants. Every public wire and expected-output hash matches.
- The private request's complete 751-schema closure was freshly generated with the candidate generator and compiled with actual javac 8. Its immutable baseline, harness and input are unchanged. Only aggregate private measurements are published.

Public benchmark jar SHA-256:

```text
builder: de50af705ebd4171a534e9c76dd19ad2b28c18f285fdfd66a285e5baf757cbe3
parser:  083de04c61e7c6095d4b428bd0e240772a230d0c0578201465d37753c464ccf4
```

The [previous report's timing command](jdk8-d43da02.md#run-configuration) applies with these jars and the same paired schedule. The private request and worker allocation profiler remain outside the public target. The unpublished candidate patch is an additional prerequisite for reproducing its generated classes.

Earlier diagnostics identified interpreted decoding loops for `FLAT_PROTO2_WIDE` and `ENUMS900`. No new compilation evidence was collected during this run. The nested inactive-oneof-history and malformed UTF-8 unfinished-oneof correctness gaps remain separate from this performance evaluation.

## Throughput and allocation

Complete parses, including final publication. Throughput and its interval are parses/second. Allocation and its interval are bytes/parse. All values are rounded for display.

| Input | Builder parses/s | Parser parses/s | Change | 95% CI of throughput difference | Builder bytes/parse | Parser bytes/parse | Change | 95% CI of allocation difference |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `PRODUCTION_REQUEST` | 67.76 | 139.72 | +106.20% | [+50.41, +93.50] | 41,497,640.59 | 13,787,707.81 | -66.77% | [-27,759,032.97, -27,660,832.59] |
| `SCALAR_PROTO2` | 27,109,716.99 | 33,592,320.97 | +23.91% | [+5,569,354.26, +7,395,853.69] | 152.00 | 112.00 | -26.32% | [-40.00, -40.00] |
| `SCALAR_PROTO3` | 27,235,194.73 | 35,213,815.67 | +29.30% | [+7,643,355.64, +8,313,886.25] | 152.00 | 112.00 | -26.32% | [-40.00, -40.00] |
| `SCALAR_EDITIONS` | 25,723,030.63 | 33,264,656.92 | +29.32% | [+7,239,212.24, +7,844,040.34] | 152.00 | 112.00 | -26.32% | [-40.00, -40.00] |
| `PRESENCE_PROTO2_ZERO` | 26,065,594.41 | 33,919,654.96 | +30.13% | [+6,360,265.27, +9,347,855.83] | 152.00 | 112.00 | -26.32% | [-40.00, -40.00] |
| `PRESENCE_PROTO3_ZERO` | 28,387,326.95 | 34,133,008.14 | +20.24% | [+5,159,244.60, +6,332,117.78] | 152.00 | 112.00 | -26.32% | [-40.00, -40.00] |
| `PRESENCE_EDITIONS_ZERO` | 26,026,762.77 | 33,225,797.21 | +27.66% | [+5,616,141.80, +8,781,927.08] | 152.00 | 112.00 | -26.32% | [-40.00, -40.00] |
| `FLAT_PROTO2_WIDE` | 1,041,356.30 | 1,491,799.50 | +43.26% | [+425,402.41, +475,483.99] | 1,946.67 | 984.00 | -49.45% | [-1,042.98, -882.35] |
| `FLAT_PROTO2_COMPACT` | 10,505,776.50 | 13,709,444.98 | +30.49% | [+2,809,104.89, +3,598,232.07] | 312.00 | 240.00 | -23.08% | [-72.00, -72.00] |
| `FLAT_PROTO3` | 3,480,016.94 | 5,394,956.96 | +55.03% | [+1,715,664.13, +2,114,215.91] | 840.00 | 528.00 | -37.14% | [-312.00, -312.00] |
| `FLAT_EDITIONS` | 2,468,202.93 | 5,157,263.47 | +108.95% | [+2,324,964.13, +3,053,156.95] | 1,298.67 | 552.00 | -57.49% | [-826.98, -666.35] |
| `REQUIRED_SCALARS` | 6,440,567.02 | 10,789,594.72 | +67.53% | [+4,251,258.32, +4,446,797.07] | 504.00 | 320.00 | -36.51% | [-184.00, -184.00] |
| `ORDINARY_SCALARS_FIRST` | 2,546,145.03 | 5,491,197.84 | +115.67% | [+2,855,453.33, +3,034,652.29] | 1,264.00 | 496.00 | -60.76% | [-768.00, -768.00] |
| `ORDINARY_SCALARS_MERGE` | 2,351,424.38 | 4,546,265.54 | +93.34% | [+2,116,577.48, +2,273,104.85] | 1,392.00 | 664.00 | -52.30% | [-728.00, -728.00] |
| `ONEOF_SCALARS_FIRST` | 2,535,350.33 | 6,035,316.09 | +138.05% | [+3,454,886.84, +3,545,044.69] | 1,320.00 | 496.00 | -62.42% | [-824.00, -824.00] |
| `ONEOF_SCALARS_MERGE` | 2,338,967.58 | 2,303,234.62 | -1.53% | [-97,430.30, +25,964.39] | 1,320.00 | 1,376.00 | +4.24% | [+56.00, +56.00] |
| `ORDINARY_FIRST` | 9,719,810.25 | 15,224,745.58 | +56.64% | [+5,081,382.12, +5,928,488.55] | 328.00 | 240.00 | -26.83% | [-88.00, -88.00] |
| `ORDINARY_MERGE` | 4,958,995.50 | 5,605,911.18 | +13.05% | [+240,966.53, +1,052,864.84] | 520.00 | 440.00 | -15.38% | [-80.00, -80.00] |
| `ONEOF_FIRST` | 7,000,076.89 | 12,298,645.18 | +75.69% | [+5,109,511.21, +5,487,625.36] | 440.00 | 264.00 | -40.00% | [-176.00, -176.00] |
| `ONEOF_MERGE` | 6,745,093.49 | 5,596,547.84 | -17.03% | [-2,196,621.41, -100,469.89] | 440.00 | 488.00 | +10.91% | [+48.00, +48.00] |
| `ONEOF_GROUP_FIRST` | 7,647,412.14 | 14,707,091.29 | +92.31% | [+6,623,833.18, +7,495,525.12] | 392.00 | 232.00 | -40.82% | [-160.00, -160.00] |
| `ONEOF_GROUP_MERGE` | 6,545,382.36 | 5,859,360.15 | -10.48% | [-836,937.09, -535,107.33] | 440.00 | 488.00 | +10.91% | [+48.00, +48.00] |
| `GROUP_FIRST` | 1,227,501.40 | 2,071,886.83 | +68.79% | [+797,204.69, +891,566.16] | 1,976.00 | 992.00 | -49.80% | [-984.00, -984.00] |
| `GROUP_MERGE` | 1,191,374.04 | 1,770,419.92 | +48.60% | [+492,173.94, +665,917.81] | 2,186.67 | 1,216.00 | -44.39% | [-1,050.98, -890.35] |
| `EXTENSION_FIRST` | 5,006,982.66 | 4,810,098.04 | -3.93% | [-261,881.73, -131,887.51] | 576.00 | 608.00 | +5.56% | [+32.00, +32.00] |
| `EXTENSION_MERGE` | 4,785,364.32 | 4,650,913.23 | -2.81% | [-171,154.22, -97,747.96] | 592.00 | 624.00 | +5.41% | [+32.00, +32.00] |
| `UNKNOWN` | 2,015,564.08 | 3,089,973.47 | +53.31% | [+919,667.36, +1,229,151.42] | 1,688.00 | 965.33 | -42.81% | [-745.61, -699.72] |
| `CLOSED_ENUM_KNOWN` | 16,708,159.28 | 26,732,859.24 | +60.00% | [+9,620,015.08, +10,429,384.84] | 216.00 | 144.00 | -33.33% | [-72.00, -72.00] |
| `CLOSED_ENUM_UNKNOWN` | 3,593,899.20 | 4,549,338.91 | +26.59% | [+667,955.50, +1,242,923.94] | 704.00 | 632.00 | -10.23% | [-72.00, -72.00] |
| `OPEN_ENUM_UNKNOWN` | 17,571,567.89 | 27,337,990.38 | +55.58% | [+9,542,545.57, +9,990,299.40] | 216.00 | 144.00 | -33.33% | [-72.00, -72.00] |
| `REPEATED_EXPANDED` | 2,595,055.68 | 5,224,462.58 | +101.32% | [+2,535,774.79, +2,723,039.01] | 1,272.00 | 544.00 | -57.23% | [-728.00, -728.00] |
| `REPEATED_PACKED` | 8,366,970.15 | 15,083,511.02 | +80.27% | [+5,893,408.05, +7,539,673.70] | 360.00 | 272.00 | -24.44% | [-88.00, -88.00] |
| `REPEATED_MESSAGE` | 2,402,948.77 | 4,993,941.55 | +107.83% | [+2,528,874.58, +2,653,110.98] | 1,424.00 | 616.00 | -56.74% | [-808.00, -808.00] |
| `MAP_SCALAR_IMPLICIT_OPEN` | 6,089,768.81 | 6,932,558.46 | +13.84% | [+773,202.00, +912,377.30] | 512.00 | 448.00 | -12.50% | [-64.00, -64.00] |
| `MAP_REPLACE_IMPLICIT_OPEN` | 5,342,039.16 | 5,954,069.91 | +11.46% | [+515,374.33, +708,687.19] | 576.00 | 496.00 | -13.89% | [-148.84, -11.16] |
| `MAP_MESSAGE_IMPLICIT_OPEN` | 4,729,880.20 | 6,069,312.53 | +28.32% | [+1,268,037.35, +1,410,827.31] | 760.00 | 514.67 | -32.28% | [-302.70, -187.96] |
| `MAP_MESSAGE_REPLACE_IMPLICIT_OPEN` | 3,679,284.17 | 4,670,476.81 | +26.94% | [+947,498.68, +1,034,886.59] | 904.00 | 672.00 | -25.66% | [-232.00, -232.00] |
| `MAP_ENUM_UNKNOWN_IMPLICIT_OPEN` | 6,186,336.25 | 7,098,833.15 | +14.75% | [+729,059.13, +1,095,934.67] | 504.00 | 448.00 | -11.11% | [-90.42, -21.58] |
| `MAP_SCALAR_EXPLICIT_CLOSED` | 5,527,004.44 | 6,330,015.12 | +14.53% | [+775,734.66, +830,286.70] | 584.00 | 520.00 | -10.96% | [-64.00, -64.00] |
| `MAP_REPLACE_EXPLICIT_CLOSED` | 4,719,874.10 | 5,162,889.26 | +9.39% | [+380,132.92, +505,897.40] | 648.00 | 584.00 | -9.88% | [-64.00, -64.00] |
| `MAP_MESSAGE_EXPLICIT_CLOSED` | 4,127,509.16 | 5,227,245.91 | +26.64% | [+1,025,564.30, +1,173,909.20] | 832.00 | 600.00 | -27.88% | [-232.00, -232.00] |
| `MAP_MESSAGE_REPLACE_EXPLICIT_CLOSED` | 3,581,773.28 | 4,324,542.04 | +20.74% | [+459,628.68, +1,025,908.84] | 976.00 | 744.00 | -23.77% | [-232.00, -232.00] |
| `MAP_ENUM_UNKNOWN_EXPLICIT_CLOSED` | 3,937,902.23 | 4,153,843.82 | +5.48% | [+137,086.78, +294,796.39] | 912.00 | 848.00 | -7.02% | [-64.00, -64.00] |
| `WIDE_ORDINARY_FIELD1_FIRST` | 2,990,657.88 | 7,162,022.65 | +139.48% | [+4,085,135.95, +4,257,593.59] | 1,104.00 | 504.00 | -54.35% | [-600.00, -600.00] |
| `WIDE_ORDINARY_FIELD1_MERGE` | 2,509,413.71 | 3,423,205.86 | +36.41% | [+874,447.26, +953,137.03] | 1,288.00 | 952.00 | -26.09% | [-336.00, -336.00] |
| `WIDE_ORDINARY_FIELD65_FIRST` | 2,996,152.34 | 6,969,014.44 | +132.60% | [+3,920,203.84, +4,025,520.36] | 1,104.00 | 504.00 | -54.35% | [-600.00, -600.00] |
| `WIDE_ORDINARY_FIELD65_MERGE` | 2,516,968.93 | 3,254,848.55 | +29.32% | [+653,612.70, +822,146.54] | 1,288.00 | 952.00 | -26.09% | [-336.00, -336.00] |
| `WIDE_ONEOF_FIELD1_FIRST` | 3,564,540.73 | 13,582,583.58 | +281.05% | [+9,784,215.15, +10,251,870.54] | 912.00 | 256.00 | -71.93% | [-656.00, -656.00] |
| `WIDE_ONEOF_FIELD1_MERGE` | 2,978,276.84 | 2,588,561.44 | -13.09% | [-755,577.33, -23,853.48] | 912.00 | 952.00 | +4.39% | [+40.00, +40.00] |
| `WIDE_ONEOF_FIELD128_FIRST` | 3,393,098.78 | 13,554,637.00 | +299.48% | [+9,938,955.55, +10,384,120.90] | 912.00 | 256.00 | -71.93% | [-656.00, -656.00] |
| `WIDE_ONEOF_FIELD128_MERGE` | 3,424,947.38 | 2,638,529.77 | -22.96% | [-964,334.27, -608,500.96] | 912.00 | 952.00 | +4.39% | [+40.00, +40.00] |
| `SCALAR494_FIELD1` | 11,427,603.00 | 20,000,678.73 | +75.02% | [+8,377,491.38, +8,768,660.09] | 280.00 | 176.00 | -37.14% | [-104.00, -104.00] |
| `SCALAR494_FIELD494` | 11,535,544.78 | 20,272,849.86 | +75.74% | [+8,363,941.92, +9,110,668.24] | 280.00 | 176.00 | -37.14% | [-104.00, -104.00] |
| `SCALAR495_FIELD1` | 6,890,095.98 | 20,528,127.69 | +197.94% | [+13,457,371.04, +13,818,692.37] | 280.00 | 176.00 | -37.14% | [-104.00, -104.00] |
| `SCALAR495_FIELD495` | 6,508,701.84 | 19,932,739.87 | +206.25% | [+13,324,675.12, +13,523,400.94] | 280.00 | 176.00 | -37.14% | [-104.00, -104.00] |
| `ENUMS138` | 816,464.69 | 918,314.57 | +12.47% | [+65,347.22, +138,352.54] | 1,272.00 | 672.00 | -47.17% | [-600.00, -600.00] |
| `ENUMS139` | 133,476.47 | 928,292.50 | +595.47% | [+787,118.42, +802,513.64] | 1,288.00 | 680.00 | -47.21% | [-608.00, -608.00] |
| `ENUMS900` | 17,351.67 | 13,445.89 | -22.51% | [-3,998.77, -3,812.80] | 7,560.03 | 3,816.04 | -49.52% | [-3,743.99, -3,743.99] |
| `DEPTH2` | 12,912,930.83 | 23,443,476.78 | +81.55% | [+10,170,428.69, +10,890,663.21] | 248.00 | 152.00 | -38.71% | [-96.00, -96.00] |
| `DEPTH8` | 4,464,127.09 | 7,655,598.80 | +71.49% | [+3,092,327.11, +3,290,616.30] | 776.00 | 392.00 | -49.48% | [-384.00, -384.00] |
| `EXISTING_BINARY_EXPANDED_FIRST` | 9,847,775.90 | 14,986,412.23 | +52.18% | [+4,887,736.21, +5,389,536.46] | 320.00 | 240.00 | -25.00% | [-80.00, -80.00] |
| `EXISTING_BINARY_EXPANDED_MERGE` | 4,822,401.24 | 6,232,030.38 | +29.23% | [+272,155.27, +2,547,103.01] | 504.00 | 432.00 | -14.29% | [-72.00, -72.00] |
| `EXISTING_BINARY_EXTENSION_FIRST` | 4,639,265.74 | 4,625,662.42 | -0.29% | [-64,485.72, +37,279.09] | 664.00 | 696.00 | +4.82% | [+32.00, +32.00] |
| `EXISTING_BINARY_EXTENSION_MERGE` | 4,246,116.52 | 4,107,321.35 | -3.27% | [-184,869.40, -92,720.94] | 680.00 | 712.00 | +4.71% | [+32.00, +32.00] |
| `EXISTING_BINARY_PACKED_FIRST` | 11,479,619.94 | 20,718,487.17 | +80.48% | [+8,848,719.52, +9,629,014.93] | 288.00 | 208.00 | -27.78% | [-80.00, -80.00] |
| `EXISTING_BINARY_PACKED_MERGE` | 6,415,058.74 | 7,602,392.06 | +18.51% | [+1,042,855.90, +1,331,810.75] | 472.00 | 400.00 | -15.25% | [-72.00, -72.00] |
| `SCALING_ORDINARY_1024` | 41,931.66 | 44,914.40 | +7.11% | [+1,501.72, +4,463.76] | 52,586.68 | 51,858.68 | -1.38% | [-728.00, -728.00] |
| `SCALING_ORDINARY_2048` | 24,102.33 | 23,756.97 | -1.43% | [-1,690.52, +999.81] | 73,680.02 | 72,968.02 | -0.97% | [-712.00, -712.00] |
| `SCALING_ONEOF_1024` | 47,961.42 | 46,522.43 | -3.00% | [-3,069.95, +191.95] | 50,997.34 | 46,072.01 | -9.66% | [-28,423.55, +18,572.89] |
| `SCALING_ONEOF_2048` | 25,487.02 | 26,310.46 | +3.23% | [-399.27, +2,046.13] | 83,018.70 | 83,560.02 | +0.65% | [-80,878.75, +81,961.39] |
| `SCALING_EXTENSION_1024` | 23,929.65 | 23,433.00 | -2.08% | [-1,991.75, +998.45] | 67,600.03 | 78,536.31 | +16.18% | [-12,590.49, +34,463.05] |
| `SCALING_EXTENSION_2048` | 12,462.72 | 12,278.76 | -1.48% | [-993.03, +625.12] | 137,872.04 | 116,058.73 | -15.82% | [-68,809.75, +25,183.14] |
| `WIDE_ORDINARY_ALL_FIRST` | 290,144.38 | 429,352.91 | +47.98% | [+125,244.36, +153,172.70] | 11,856.00 | 8,696.00 | -26.65% | [-3,160.00, -3,160.00] |
| `WIDE_ORDINARY_ALL_MERGE` | 130,906.87 | 158,269.85 | +20.90% | [+25,195.32, +29,530.64] | 22,792.00 | 19,896.00 | -12.71% | [-2,896.00, -2,896.00] |
