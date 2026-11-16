# Builder-hosted static parser validation

This directory contains supplemental synthetic validation for the production generator change. It does not rewrite a prototype decoder into generated source. The candidate is actual `protoc` output; instrumented copies count Builder and field-wrapper constructions and observe handoff state. Pristine baseline output runs in a separate JVM to provide the differential oracle.

The production patch and core/generator regression tests are in the preceding implementation commit. See [the design](../../docs/design/java-direct-parser.md) for invariants and [source-review.md](source-review.md) for exact source accounting and review boundaries. These manually invoked checks supplement, rather than replace, the repository tests.

## Run

Use a Unix-like environment with Python 3.8+, the repository's Bazel/C++ build prerequisites, a current JDK that supports `javac --release 8` and `--release 17`, and a separate JDK 8. Set both JDK paths explicitly; the scripts require them. The recorded runs used JDK 27 and JDK 8.

From the candidate repository root:

```bash
export JAVA_HOME=/path/to/current-jdk
export JAVA8_HOME=/path/to/jdk8
export PROTOBUF_BASELINE_ROOT="$(cd .. && pwd)/protobuf-static-parser-baseline"

git worktree add --detach "$PROTOBUF_BASELINE_ROOT" eda222ac5f2b8f826923117460587ccf09922844
(
  cd "$PROTOBUF_BASELINE_ROOT"
  bazel build //:protoc //java/core:core_bundle
)
bazel build //:protoc //java/core:core_bundle

python3 experiments/java-static-parser/check_generated_primitives.py
python3 experiments/java-static-parser/check_generated_children.py
python3 experiments/java-static-parser/check_generated_coverage.py
```

Use an unmodified baseline worktree. The scripts default `PROTOBUF_BASELINE_ROOT` to the sibling directory shown above and use each worktree's `bazel-bin/protoc` and `bazel-bin/java/core/libcore_bundle.jar`. The primitive check also accepts explicit compiler, runtime and JDK flags; run it with `--help` after setting the JDK variables.

Every check compiles Java 8 and Java 17 targets. Java 8 output runs on both JDKs; Java 17 output runs on the current JDK. Correctness and constructor-count checks use `-Xint`, so their happy-path Builder-avoidance assertions do not depend on inlining or escape analysis. These are not throughput benchmarks.

Results, generated sources, class files, bytecode dumps and oracle files stay under ignored `out/generator/`. Python caches are also ignored. On failure, inspect the log named in the exception before rerunning. The checks use assertions: do not run Python with `-O`.

The narrower retained-oneof gate can also be run independently:

```bash
python3 experiments/java-static-parser/check_generated_oneofs.py
```

Run the repository regression tests separately:

```bash
bazel test //java/core/... //src/google/protobuf/compiler/java:generator_unittest
```

## Coverage

- `check_generated_primitives.py`: scalar types, explicit defaults, presence, repeated and packed fields, unknown fields, truncation, empty and wide messages. Checks the existing instance loop/helpers/publication bodies and the generated message source outside Builder/Parser delegation. Counts constructors on serializer-produced happy inputs.
- `check_generated_children.py`: ordinary singular and repeated children, recursive and mutually recursive types, groups, failure publication, pending-tag/payload position, and direct transfer of live repeated/unknown state. Tests normal and forced-field-builder modes against pristine generation.
- `check_generated_oneofs.py`: retained child identity and hidden history across case changes, nested returns and later merges; clean-message restoration; collection sharing; immutable snapshots; bounded direct installation/attachment shards.
- `check_generated_coverage.py`: extends the retained-oneof gate with proto2/proto3/editions field shapes, maps, enums, extensions, MessageSet, CODE_SIZE and precompiled legacy children. Includes helper-name collisions and a 128-alternative message-only oneof to guard decode-loop growth.

The Java files are test drivers and instrumentation helpers. `ChildExchangeTest.java` and `CleanPromotionTest.java` are templates specialized by the Python drivers before compilation; they are not standalone compilable repository tests. The `.proto` files and schema strings are synthetic inputs. Wide schemas are expanded programmatically rather than checked in as generated source.

The source and bytecode gates report Builder and message classes separately. They reject added message members/access bridges or message-class growth in the fixtures and bound generated shard parameters to 98 JVM slots. Compatibility fixtures deliberately retain a baseline-generated child rather than regenerating it with the candidate.

## Recorded validation

The audited production patch passed 258 targets: the Java core targets and `//src/google/protobuf/compiler/java:generator_unittest`. The final source cleanup preserved all 93 generated Java sources across the synthetic fixture families byte-for-byte.

For each tested target/runtime combination, the primitive gate performs 18,740 differential comparisons. The ordinary-child gate performs 5,176 comparisons normally and 5,306 with forced field builders, with 432 happy inputs constructing no Builders. The all-shape gate performs 9,914 comparisons normally and 10,094 with forced field builders, plus 36 clean-restoration checks. The retained-child and additional all-shape happy corpora cover 4,224 and 864 parses respectively with zero Builders or field wrappers.

Legacy parsers, extensions, CODE_SIZE and other compatibility fallbacks intentionally retain the ordinary Builder path. The zero-Builder contract is for eligible happy-path static decoding, not every possible input or compatibility mode. Correctness and allocation checks do not settle the source-size acceptance question or establish a general performance result.
