# Java parsing benchmarks

JMH benchmarks using the repository's public test and conformance schemas. Small binary inputs exercise individual decoding paths and the transition to child builders. The suite also carries over the existing unknown-field and text-format performance workloads.

## Build and check

```sh
bazel build --java_language_version=8 //java/benchmarks:benchmarks_deploy.jar
bazel test --java_language_version=8 //java/benchmarks:input_test
```

The suite uses the existing Java runtime and proto targets. JMH and its annotation processor are development dependencies. Java trial setup constructs the inputs and checks both parsing and fresh-builder wire merging against independently constructed expected output. JUnit runs these checks for every input, including the existing workloads, without launching measurement forks.

The deploy jar can run independently of the checkout. To list benchmarks and parameters:

```sh
JAR=bazel-bin/java/benchmarks/benchmarks_deploy.jar
"$JDK8_HOME/bin/java" -jar "$JAR" -l
"$JDK8_HOME/bin/java" -jar "$JAR" -lp
```

To run the content checks on a particular JVM and print binary input sizes, merge counts and SHA-256 hashes:

```sh
for check in ParseBenchmark UnknownFieldBenchmarks TextFormatBenchmarks; do
  "$JDK8_HOME/bin/java" -Xmx2g -cp "$JAR" "com.google.protobuf.benchmarks.$check"
done
```

`--java_language_version=8` requests Java 8-compatible bytecode. Bazel selects the compiler, which need not be javac 8. Record its identity and options with results, especially when investigating large decoding methods.

## Workloads

[`BenchmarkInput.java`](src/main/java/com/google/protobuf/benchmarks/BenchmarkInput.java) defines the binary inputs. Each JMH trial uses one fixed input and message type.

| Inputs | Purpose |
| --- | --- |
| `SCALAR_*`, `PRESENCE_*`, `FLAT_PROTO3`, `FLAT_EDITIONS`, `REQUIRED_SCALARS` | Non-nested messages, presence and required fields across proto2, proto3 and Editions |
| `FLAT_PROTO2_WIDE` | Seven scalar fields in the wide conformance schema, with an interpreted decoding loop under the measured HotSpot settings |
| `FLAT_PROTO2_COMPACT` | The same seven scalar fields and wire input in a compact proto2 schema |
| `ORDINARY_*`, `ONEOF_*`, `GROUP_*`, `EXTENSION_*` | First occurrences and exactly one merge of singular children, including groups and extensions |
| `UNKNOWN`, `*_ENUM_*`, `REPEATED_*`, `MAP_*`, `DEPTH*` | Unknown fields, enums, repeated fields, map insertion and replacement, and single chains of depth 2 and 8 |
| `WIDE_*`, `SCALAR494_*`, `SCALAR495_*`, `ENUMS*` | Wide schemas and large decoding methods |
| `EXISTING_BINARY_*` | Small binary counterparts of the existing text-format merge workloads |
| `SCALING_*` | 1,024 and 2,048 occurrences with growing merged child contents |

Scalar payloads are small, with one-byte strings and bytes where used. A `FIRST` input contains one child occurrence. Its `MERGE` counterpart contains two, with both contributing to the expected result. Repeated-message elements append and duplicate map keys replace values, so those are separate cases.

`FLAT_PROTO2_WIDE` was previously named `FLAT_PROTO2`. With javac 8, the baseline builder decoder is 8,456 bytes and the proposed direct parser is 8,220 bytes. Both remain interpreted on HotSpot 8 and 27 because they exceed the default 8,000-byte huge-method cutoff. Report this separately from compiled parsing.

`FLAT_PROTO2_COMPACT` retains the same field numbers, types, proto2 presence and extendable-message base, without the unused fields. Both inputs are 26 bytes. Its builder decoder is 288 bytes and direct parser is 334 bytes, with both decoding loops confirmed in normal C2 compilation on HotSpot 8 and 27. These measurements use baseline c0acd3a and parser 8a7de74, compiled with javac 8. Check compilation separately when changing the generator, compiler or JVM settings.

Wide fixtures retain the full schema while sparse inputs select its first or last field. They cover 65 ordinary children, 128 message alternatives, 494/495 scalar alternatives and 138/139/900 closed-enum fields. Dense cases populate all 65 children or every enum field. These widths support investigation of previously observed compiler boundaries, but field counts alone do not establish compilation behavior.

The existing workloads come from:

- [`UnknownFieldSetPerformanceTest`](../core/src/test/java/com/google/protobuf/UnknownFieldSetPerformanceTest.java): 800,000 bytes alternating field numbers and adding 100,000 fields. The alternating-field benchmark returns the builder without adding finalization.
- [`TextFormatPerformanceTest`](../core/src/test/java/com/google/protobuf/TextFormatPerformanceTest.java): repeated child and extension merges. The original 400,000-occurrence inputs are retained alongside 1,024/2,048-occurrence inputs.

Both existing text-format schemas currently use Edition 2023, despite their proto2/proto3 filenames. The suite includes separate true proto2 and proto3 messages. All schemas and payloads are public.

## Measure

For one binary input:

```sh
"$JDK8_HOME/bin/java" -jar "$JAR" \
  'com.google.protobuf.benchmarks.ParseBenchmark.parse$' \
  -p input=ORDINARY_MERGE -t 1 -f 5 -wi 3 -i 5 -w 1s -r 1s \
  -jvmArgs '-Xms2g -Xmx2g -XX:+UseG1GC -XX:+AlwaysPreTouch' \
  -prof gc \
  -rf json -rff ordinary-merge.json
```

`parse` returns one parsed message. `wireBuild` returns a fresh `Builder.mergeFrom(wire).build()` result. Input construction, reflection, registry setup and assertions are outside the timed operation. `UnknownFieldBenchmarks` and `TextFormatBenchmarks` expose the existing workloads through the same JMH runner.

JMH's GC profiler reports allocation as `gc.alloc.rate.norm` in bytes per operation. Keep profiled and uninstrumented timing results separate.

## Compare revisions

Apply this benchmark contribution to clean baseline and candidate worktrees and build the same target with identical flags. Check the inputs on both jars before measuring. Record source commits, jar hashes, compiler identity, JVM versions, JVM flags and host details alongside the JMH JSON and console output.

For paired comparisons, run complete forks sequentially with `-f 1`, alternating baseline/candidate and candidate/baseline until each has five forks. Reverse the starting variant for the next workload. Keep every fork and its iteration results. Use the same worker count, heap, collector, affinity and compiler settings, without changing compilation budgets. Do not run competing benchmark processes.

Use separate diagnostic runs for profiling or compilation logs. Compilation of a wrapper does not establish that its decoding loop compiled. Report time and allocation per workload rather than an aggregate score.

Tiny inputs isolate costs but do not reproduce a service's message mix, startup or code-cache footprint. A first-versus-merge comparison includes the second decode and final publication as well as the builder transition. The scaling cases exercise growing child contents associated with CVE-2022-3171, alongside the correctness regression tests.

These workloads address the previously reported [non-nested regression and class-file limits](https://github.com/protocolbuffers/protobuf/pull/25543#issuecomment-5119925840), [map and Editions correctness](https://github.com/protocolbuffers/protobuf/pull/25543#issuecomment-3831167488), [large decoding methods](https://github.com/protocolbuffers/protobuf/issues/24828#issuecomment-3878380225), and [CVE-2022-3171](https://github.com/protocolbuffers/protobuf/issues/24828#issuecomment-3705611728).
