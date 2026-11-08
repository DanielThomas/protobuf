# JDK 8 handoff results

Builder baseline [`c0acd3a`](https://github.com/DanielThomas/protobuf/commit/c0acd3a8f5aedaa6705530b9704c2af527f51575) versus parser base [`d43da02`](https://github.com/DanielThomas/protobuf/commit/d43da02858ec898079832d5dd94d1d54c40cd59d) plus the current handoff changes, using unchanged benchmark revision [`0c313de`](https://github.com/DanielThomas/protobuf/commit/0c313dec156cb6cbea2d36ee557d85190c8ead7c). The handoff changes remain an unpublished working-tree patch, SHA-256 `78f185ad47d410d60aae21c70d98e2fce349ede8aba5e32970da3cc3b78a0770`. These results should be attributed to that candidate rather than the published PR #1 revision alone.

All 73 public binary inputs and the representative private request were measured. All 1,184 fresh forks passed, with CPU and memory placement verified. Timing completed before allocation profiling began. No retries, discarded results or pooling with earlier measurements. The separate text-format and unknown-field workloads passed their input checks but were not timed.

| Measurement | Higher | Lower | Unresolved |
| --- | ---: | ---: | ---: |
| Parse time | 7 | 59 | 8 |
| Allocation | 9 | 61 | 4 |

Parse time is the mean of five timing forks, in ns/op. Allocation is the mean of three separate allocation forks, in B/op. Lower is better for both. Delta columns show parser minus builder as the paired mean difference +/- its 95% confidence margin. Values are rounded to one decimal place. Intervals spanning zero are unresolved, not evidence of equivalence. The two-sided Student t intervals are exploratory per-input comparisons without multiplicity correction, with four degrees of freedom for time and two for allocation.

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

## Parse time and allocation

Complete parses, including final publication.

| Input | Builder ns/op | Parser ns/op | Change | Delta ns/op (95% CI) | Builder B/op | Parser B/op | Change | Delta B/op (95% CI) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `PRODUCTION_REQUEST` | 14,781,333.3 | 7,278,149.1 | -50.8% | -7,503,184.2 +/- 799,579.8 | 41,497,640.6 | 13,787,707.8 | -66.8% | -27,709,932.8 +/- 49,100.2 |
| `SCALAR_PROTO2` | 36.9 | 29.8 | -19.3% | -7.1 +/- 0.9 | 152.0 | 112.0 | -26.3% | -40.0 +/- 0.0 |
| `SCALAR_PROTO3` | 36.7 | 28.4 | -22.7% | -8.3 +/- 0.3 | 152.0 | 112.0 | -26.3% | -40.0 +/- 0.0 |
| `SCALAR_EDITIONS` | 38.9 | 30.1 | -22.7% | -8.8 +/- 0.9 | 152.0 | 112.0 | -26.3% | -40.0 +/- 0.0 |
| `PRESENCE_PROTO2_ZERO` | 38.4 | 29.5 | -23.2% | -8.9 +/- 2.0 | 152.0 | 112.0 | -26.3% | -40.0 +/- 0.0 |
| `PRESENCE_PROTO3_ZERO` | 35.2 | 29.3 | -16.8% | -5.9 +/- 0.6 | 152.0 | 112.0 | -26.3% | -40.0 +/- 0.0 |
| `PRESENCE_EDITIONS_ZERO` | 38.5 | 30.1 | -21.8% | -8.4 +/- 2.2 | 152.0 | 112.0 | -26.3% | -40.0 +/- 0.0 |
| `FLAT_PROTO2_WIDE` | 960.7 | 670.4 | -30.2% | -290.4 +/- 25.6 | 1,946.7 | 984.0 | -49.5% | -962.7 +/- 80.3 |
| `FLAT_PROTO2_COMPACT` | 95.3 | 72.9 | -23.4% | -22.3 +/- 3.5 | 312.0 | 240.0 | -23.1% | -72.0 +/- 0.0 |
| `FLAT_PROTO3` | 287.8 | 185.4 | -35.6% | -102.4 +/- 16.4 | 840.0 | 528.0 | -37.1% | -312.0 +/- 0.0 |
| `FLAT_EDITIONS` | 406.4 | 194.0 | -52.3% | -212.3 +/- 38.6 | 1,298.7 | 552.0 | -57.5% | -746.7 +/- 80.3 |
| `REQUIRED_SCALARS` | 155.3 | 92.7 | -40.3% | -62.6 +/- 2.6 | 504.0 | 320.0 | -36.5% | -184.0 +/- 0.0 |
| `ORDINARY_SCALARS_FIRST` | 392.8 | 182.1 | -53.6% | -210.7 +/- 7.1 | 1,264.0 | 496.0 | -60.8% | -768.0 +/- 0.0 |
| `ORDINARY_SCALARS_MERGE` | 425.3 | 220.0 | -48.3% | -205.3 +/- 7.0 | 1,392.0 | 664.0 | -52.3% | -728.0 +/- 0.0 |
| `ONEOF_SCALARS_FIRST` | 394.5 | 165.7 | -58.0% | -228.8 +/- 5.5 | 1,320.0 | 496.0 | -62.4% | -824.0 +/- 0.0 |
| `ONEOF_SCALARS_MERGE` | 427.8 | 434.2 | +1.5% | +6.4 +/- 11.3 | 1,320.0 | 1,376.0 | +4.2% | +56.0 +/- 0.0 |
| `ORDINARY_FIRST` | 102.9 | 65.7 | -36.1% | -37.2 +/- 2.2 | 328.0 | 240.0 | -26.8% | -88.0 +/- 0.0 |
| `ORDINARY_MERGE` | 204.6 | 181.2 | -11.4% | -23.4 +/- 14.9 | 520.0 | 440.0 | -15.4% | -80.0 +/- 0.0 |
| `ONEOF_FIRST` | 142.9 | 81.3 | -43.1% | -61.5 +/- 1.7 | 440.0 | 264.0 | -40.0% | -176.0 +/- 0.0 |
| `ONEOF_MERGE` | 148.3 | 182.0 | +22.7% | +33.7 +/- 35.6 | 440.0 | 488.0 | +10.9% | +48.0 +/- 0.0 |
| `ONEOF_GROUP_FIRST` | 130.8 | 68.0 | -48.0% | -62.7 +/- 2.5 | 392.0 | 232.0 | -40.8% | -160.0 +/- 0.0 |
| `ONEOF_GROUP_MERGE` | 152.8 | 170.7 | +11.7% | +17.9 +/- 3.6 | 440.0 | 488.0 | +10.9% | +48.0 +/- 0.0 |
| `GROUP_FIRST` | 815.4 | 482.8 | -40.8% | -332.6 +/- 30.3 | 1,976.0 | 992.0 | -49.8% | -984.0 +/- 0.0 |
| `GROUP_MERGE` | 839.8 | 565.1 | -32.7% | -274.7 +/- 41.2 | 2,186.7 | 1,216.0 | -44.4% | -970.7 +/- 80.3 |
| `EXTENSION_FIRST` | 199.7 | 207.9 | +4.1% | +8.2 +/- 2.8 | 576.0 | 608.0 | +5.6% | +32.0 +/- 0.0 |
| `EXTENSION_MERGE` | 209.0 | 215.0 | +2.9% | +6.0 +/- 1.7 | 592.0 | 624.0 | +5.4% | +32.0 +/- 0.0 |
| `UNKNOWN` | 496.1 | 324.1 | -34.7% | -172.1 +/- 14.9 | 1,688.0 | 965.3 | -42.8% | -722.7 +/- 22.9 |
| `CLOSED_ENUM_KNOWN` | 59.9 | 37.4 | -37.5% | -22.4 +/- 1.0 | 216.0 | 144.0 | -33.3% | -72.0 +/- 0.0 |
| `CLOSED_ENUM_UNKNOWN` | 278.5 | 220.0 | -21.0% | -58.5 +/- 17.6 | 704.0 | 632.0 | -10.2% | -72.0 +/- 0.0 |
| `OPEN_ENUM_UNKNOWN` | 56.9 | 36.6 | -35.7% | -20.3 +/- 0.6 | 216.0 | 144.0 | -33.3% | -72.0 +/- 0.0 |
| `REPEATED_EXPANDED` | 385.4 | 191.4 | -50.3% | -193.9 +/- 4.7 | 1,272.0 | 544.0 | -57.2% | -728.0 +/- 0.0 |
| `REPEATED_PACKED` | 119.5 | 66.4 | -44.5% | -53.1 +/- 4.2 | 360.0 | 272.0 | -24.4% | -88.0 +/- 0.0 |
| `REPEATED_MESSAGE` | 416.2 | 200.3 | -51.9% | -215.9 +/- 6.1 | 1,424.0 | 616.0 | -56.7% | -808.0 +/- 0.0 |
| `MAP_SCALAR_IMPLICIT_OPEN` | 164.2 | 144.3 | -12.2% | -20.0 +/- 1.8 | 512.0 | 448.0 | -12.5% | -64.0 +/- 0.0 |
| `MAP_REPLACE_IMPLICIT_OPEN` | 187.2 | 168.0 | -10.3% | -19.2 +/- 3.1 | 576.0 | 496.0 | -13.9% | -80.0 +/- 68.8 |
| `MAP_MESSAGE_IMPLICIT_OPEN` | 211.4 | 164.8 | -22.1% | -46.7 +/- 2.3 | 760.0 | 514.7 | -32.3% | -245.3 +/- 57.4 |
| `MAP_MESSAGE_REPLACE_IMPLICIT_OPEN` | 271.8 | 214.1 | -21.2% | -57.7 +/- 2.0 | 904.0 | 672.0 | -25.7% | -232.0 +/- 0.0 |
| `MAP_ENUM_UNKNOWN_IMPLICIT_OPEN` | 161.7 | 140.9 | -12.8% | -20.8 +/- 3.9 | 504.0 | 448.0 | -11.1% | -56.0 +/- 34.4 |
| `MAP_SCALAR_EXPLICIT_CLOSED` | 180.9 | 158.0 | -12.7% | -23.0 +/- 1.2 | 584.0 | 520.0 | -11.0% | -64.0 +/- 0.0 |
| `MAP_REPLACE_EXPLICIT_CLOSED` | 211.9 | 193.7 | -8.6% | -18.2 +/- 2.9 | 648.0 | 584.0 | -9.9% | -64.0 +/- 0.0 |
| `MAP_MESSAGE_EXPLICIT_CLOSED` | 242.3 | 191.3 | -21.0% | -51.0 +/- 2.9 | 832.0 | 600.0 | -27.9% | -232.0 +/- 0.0 |
| `MAP_MESSAGE_REPLACE_EXPLICIT_CLOSED` | 279.2 | 231.7 | -17.0% | -47.5 +/- 14.7 | 976.0 | 744.0 | -23.8% | -232.0 +/- 0.0 |
| `MAP_ENUM_UNKNOWN_EXPLICIT_CLOSED` | 253.9 | 240.8 | -5.2% | -13.2 +/- 4.7 | 912.0 | 848.0 | -7.0% | -64.0 +/- 0.0 |
| `WIDE_ORDINARY_FIELD1_FIRST` | 334.4 | 139.6 | -58.2% | -194.7 +/- 2.5 | 1,104.0 | 504.0 | -54.3% | -600.0 +/- 0.0 |
| `WIDE_ORDINARY_FIELD1_MERGE` | 398.5 | 292.1 | -26.7% | -106.4 +/- 4.4 | 1,288.0 | 952.0 | -26.1% | -336.0 +/- 0.0 |
| `WIDE_ORDINARY_FIELD65_FIRST` | 333.8 | 143.5 | -57.0% | -190.3 +/- 6.0 | 1,104.0 | 504.0 | -54.3% | -600.0 +/- 0.0 |
| `WIDE_ORDINARY_FIELD65_MERGE` | 397.4 | 307.3 | -22.7% | -90.2 +/- 12.0 | 1,288.0 | 952.0 | -26.1% | -336.0 +/- 0.0 |
| `WIDE_ONEOF_FIELD1_FIRST` | 280.6 | 73.6 | -73.8% | -206.9 +/- 2.4 | 912.0 | 256.0 | -71.9% | -656.0 +/- 0.0 |
| `WIDE_ONEOF_FIELD1_MERGE` | 335.9 | 388.7 | +15.7% | +52.8 +/- 50.9 | 912.0 | 952.0 | +4.4% | +40.0 +/- 0.0 |
| `WIDE_ONEOF_FIELD128_FIRST` | 294.8 | 73.8 | -75.0% | -221.0 +/- 5.4 | 912.0 | 256.0 | -71.9% | -656.0 +/- 0.0 |
| `WIDE_ONEOF_FIELD128_MERGE` | 292.1 | 379.3 | +29.9% | +87.3 +/- 21.4 | 912.0 | 952.0 | +4.4% | +40.0 +/- 0.0 |
| `SCALAR494_FIELD1` | 87.5 | 50.0 | -42.9% | -37.5 +/- 1.2 | 280.0 | 176.0 | -37.1% | -104.0 +/- 0.0 |
| `SCALAR494_FIELD494` | 86.7 | 49.3 | -43.1% | -37.4 +/- 1.6 | 280.0 | 176.0 | -37.1% | -104.0 +/- 0.0 |
| `SCALAR495_FIELD1` | 145.1 | 48.7 | -66.4% | -96.4 +/- 0.9 | 280.0 | 176.0 | -37.1% | -104.0 +/- 0.0 |
| `SCALAR495_FIELD495` | 153.7 | 50.2 | -67.4% | -103.5 +/- 4.1 | 280.0 | 176.0 | -37.1% | -104.0 +/- 0.0 |
| `ENUMS138` | 1,226.2 | 1,089.0 | -11.2% | -137.2 +/- 55.0 | 1,272.0 | 672.0 | -47.2% | -600.0 +/- 0.0 |
| `ENUMS139` | 7,492.0 | 1,077.3 | -85.6% | -6,414.8 +/- 31.0 | 1,288.0 | 680.0 | -47.2% | -608.0 +/- 0.0 |
| `ENUMS900` | 57,631.9 | 74,372.4 | +29.0% | +16,740.5 +/- 358.6 | 7,560.0 | 3,816.0 | -49.5% | -3,744.0 +/- 0.0 |
| `DEPTH2` | 77.5 | 42.7 | -44.9% | -34.8 +/- 1.4 | 248.0 | 152.0 | -38.7% | -96.0 +/- 0.0 |
| `DEPTH8` | 224.1 | 130.6 | -41.7% | -93.4 +/- 5.1 | 776.0 | 392.0 | -49.5% | -384.0 +/- 0.0 |
| `EXISTING_BINARY_EXPANDED_FIRST` | 101.5 | 66.7 | -34.3% | -34.8 +/- 1.4 | 320.0 | 240.0 | -25.0% | -80.0 +/- 0.0 |
| `EXISTING_BINARY_EXPANDED_MERGE` | 210.0 | 163.8 | -22.0% | -46.3 +/- 35.2 | 504.0 | 432.0 | -14.3% | -72.0 +/- 0.0 |
| `EXISTING_BINARY_EXTENSION_FIRST` | 215.6 | 216.2 | +0.3% | +0.6 +/- 2.4 | 664.0 | 696.0 | +4.8% | +32.0 +/- 0.0 |
| `EXISTING_BINARY_EXTENSION_MERGE` | 235.5 | 243.5 | +3.4% | +7.9 +/- 2.6 | 680.0 | 712.0 | +4.7% | +32.0 +/- 0.0 |
| `EXISTING_BINARY_PACKED_FIRST` | 87.1 | 48.3 | -44.6% | -38.9 +/- 2.2 | 288.0 | 208.0 | -27.8% | -80.0 +/- 0.0 |
| `EXISTING_BINARY_PACKED_MERGE` | 155.9 | 131.6 | -15.6% | -24.3 +/- 2.7 | 472.0 | 400.0 | -15.3% | -72.0 +/- 0.0 |
| `SCALING_ORDINARY_1024` | 23,855.9 | 22,266.1 | -6.7% | -1,589.9 +/- 807.7 | 52,586.7 | 51,858.7 | -1.4% | -728.0 +/- 0.0 |
| `SCALING_ORDINARY_2048` | 41,554.1 | 42,095.3 | +1.3% | +541.1 +/- 2,399.6 | 73,680.0 | 72,968.0 | -1.0% | -712.0 +/- 0.0 |
| `SCALING_ONEOF_1024` | 20,851.8 | 21,504.5 | +3.1% | +652.7 +/- 744.3 | 50,997.3 | 46,072.0 | -9.7% | -4,925.3 +/- 23,498.2 |
| `SCALING_ONEOF_2048` | 39,283.8 | 38,007.9 | -3.2% | -1,275.9 +/- 1,891.1 | 83,018.7 | 83,560.0 | +0.7% | +541.3 +/- 81,420.1 |
| `SCALING_EXTENSION_1024` | 41,794.5 | 42,733.8 | +2.2% | +939.4 +/- 2,732.6 | 67,600.0 | 78,536.3 | +16.2% | +10,936.3 +/- 23,526.8 |
| `SCALING_EXTENSION_2048` | 80,296.9 | 81,492.3 | +1.5% | +1,195.4 +/- 5,334.1 | 137,872.0 | 116,058.7 | -15.8% | -21,813.3 +/- 46,996.4 |
| `WIDE_ORDINARY_ALL_FIRST` | 3,448.2 | 2,329.5 | -32.4% | -1,118.6 +/- 127.1 | 11,856.0 | 8,696.0 | -26.7% | -3,160.0 +/- 0.0 |
| `WIDE_ORDINARY_ALL_MERGE` | 7,640.0 | 6,318.5 | -17.3% | -1,321.5 +/- 120.4 | 22,792.0 | 19,896.0 | -12.7% | -2,896.0 +/- 0.0 |
