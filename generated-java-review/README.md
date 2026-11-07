# Generated Java comparison

This snapshot shows the generated Java changes from [the direct parsing proposal](https://github.com/DanielThomas/protobuf/pull/1).

Compare [`java-generated-baseline...java-generated-parser`](https://github.com/DanielThomas/protobuf/compare/java-generated-baseline...java-generated-parser). Both branches use the same proto inputs and generation script. Only the compiler changes:

| Branch | Compiler revision |
| --- | --- |
| `java-generated-baseline` | `c0acd3a8f5aedaa6705530b9704c2af527f51575` |
| `java-generated-parser` | `d43da02858ec898079832d5dd94d1d54c40cd59d` |

The input set is frozen at `4e295241ef232d7b6627493b9ab27f9b1d757ad7` and contains its 482 tracked proto files, plus the wide regression schema captured from `//java/core:generated_code_size_test_proto_src` at `daa7707fb130a6db29322cd5c7caf377008bc231`. The target has since been removed. Its captured input remains in `inputs/generated_code_size_test.proto`.

Each proto has its own directory under `java/`, preserving its repository path. The additional regression schema is under `java/fixtures/generated_code_size_test/`. This keeps unrelated test schemas with the same Java class names separate.

Generation succeeds for 466 inputs and produces 1,246 Java files. The 17 rejected fixtures include private options and imports unavailable in the public repository, invalid type references, an obsolete enum value and a Java outer-class name collision. `manifest.json` records every input, its hash and its result. Compiler diagnostics are retained under `diagnostics/`. No proto files are edited to make generation succeed.

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
