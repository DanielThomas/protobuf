# Generated Java comparison

This snapshot shows the generated Java changes from [the direct parsing proposal](https://github.com/DanielThomas/protobuf/pull/1).

Compare [`java-generated-baseline...java-generated-parser`](https://github.com/DanielThomas/protobuf/compare/java-generated-baseline...java-generated-parser). Both branches use the same proto inputs and generation script. Only the compiler changes:

| Branch | Compiler revision |
| --- | --- |
| `java-generated-baseline` | `c0acd3a8f5aedaa6705530b9704c2af527f51575` |
| `java-generated-parser` | `fbb6ed1b219c14efc98577263810b0eb194f04fe` |

The input set is frozen at `4e295241ef232d7b6627493b9ab27f9b1d757ad7` and contains its 482 tracked proto files, plus the wide regression schema captured from `//java/core:generated_code_size_test_proto_src` at `daa7707fb130a6db29322cd5c7caf377008bc231`. The target has since been removed. Its captured input remains in `inputs/generated_code_size_test.proto`.

Each proto has its own directory under `java/`, preserving its repository path. The additional regression schema is under `java/fixtures/generated_code_size_test/`. This keeps unrelated test schemas with the same Java class names separate.

Generation succeeds for 466 inputs and produces 1,246 Java files. The 17 rejected fixtures include private options and imports unavailable in the public repository, invalid type references, an obsolete enum value and a Java outer-class name collision. `manifest.json` records every input, its hash and its result. Compiler diagnostics are retained under `diagnostics/`. No proto files are edited to make generation succeed.

`MessageBuilderGenerator::GenerateBuilderParsingMethods` emits both entry points through the existing field-case and per-field builder emitters. Their bodies remain in their original methods: the change binds storage, rather than extracting the builder's algorithms into a new common layer. Scalars, singular strings, presence bits and oneof state use existing message fields. Only absent builder storage becomes parse-method locals: single/repeated field builders, message-valued map builders and the mutable unknown-field builder.

The direct parsing loop and its static helpers live in the message class beside those fields, avoiding Java 8 private-access bridges in the decoding loop. The named `MessageParser` delegates to that loop and provides the child-promotion factory. Private parse helpers, collection mutability, field-builder creation and oneof bookkeeping use the original builder emitters with storage bindings. Closed-enum validation remains inline where the builder already emitted it. Unknown-field decoding calls the message's existing runtime decoder; the builder's runtime decoder is unchanged.

Before reading a duplicate singular child or an extension, one private constructor installs the current message storage and live locals directly into the enclosing builder. Its superclass installs unknown-field storage without a setter. Nothing is finalized or merged before transfer, and normal defaults are not initialized and overwritten. The already-read pending tag uses the original builder case emitter, then decoding continues through `Builder.mergeFrom()`. Child promotion uses the same state constructor via `getBuilderForParsing()`, including for the default instance; already-live builders are reused and retained field builders are reparented. Published map and extension storage is restored without message merging. The forced-field-builder hook and legacy/`CODE_SIZE` fallbacks are retained. Ordinary `toBuilder()` still uses `new Builder().mergeFrom(this)`.

Publication uses the original `GenerateBuildPartialShard` and field `GenerateBuildingCode` bodies with in-place bindings, omitting self-copies. There is no separate parser decoder or child/collection finalizer. Builder defaults move into the ordinary constructor so the state constructor bypasses them. Publication retains the source flags, including implicit-presence scalar flags needed when a parsed child is promoted; the original per-field publication masks remain in place. Pre-existing private helpers in `CODE_SIZE` output are left alone rather than being cleaned up as part of this feature.

An independent comparison against the pre-feature compiler at `eda222ac5f2b8f826923117460587ccf09922844` checked all 2,777 generated builders. Their existing tokens match after accounting for the added state-transfer methods, moved defaults, retained flags and direct child-promotion calls. All 1,528 pending-field cases also match the normal builder cases, apart from their terminal `return true`/`break`. This complements the surgical generator-source review; it is not a substitute for it. A scoped reference scan found no unreferenced new parse, mutability, state-copy, pending-field or publication helpers among 14,231 declarations; it is not a whole-program dead-code proof.

All 82 generator/Java core targets pass on JDK 8 and JDK 25, both at the standalone production feature commit and with the following feature-test commit. The wide fixture compiles and the existing 260-string handoff probe runs on both JDKs. Input hashes, generation outcomes and diagnostics are unchanged. Performance and bytecode measurements have not been rerun for this implementation.

## Regeneration

Build the desired compiler in a checkout of its revision:

```sh
bazel build //:protoc
```

Then run this script in the snapshot worktree with Python 3.9 or later:

```sh
python3 generated-java-review/generate.py \
  --protoc /path/to/compiler-checkout/bazel-bin/protoc \
  --generator-revision <compiler-commit>
```

The script uses the default Java generator options and enables experimental Editions. Import roots include the repository's proto source directories and a mapping for the Go feature import used by its codegen fixtures. It replaces only `java/`, `diagnostics/` and `manifest.json` in this directory. The manifest records the compiler revision, version and executable hash.
