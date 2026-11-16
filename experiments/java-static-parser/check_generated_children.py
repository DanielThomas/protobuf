#!/usr/bin/env python3
"""Validate production-generated ordinary-child continuation against pristine Java."""
import json
import re

from check_generated_primitives import BASELINE, HERE, JAVA, JAVA8, ROOT, method_body, run

OLD_PROTOC = BASELINE / "bazel-bin/protoc"
OLD_RUNTIME = BASELINE / "bazel-bin/java/core/libcore_bundle.jar"
RUNTIME = ROOT / "bazel-bin/java/core/libcore_bundle.jar"
OUTPUT = HERE / "out/generator/children"
NAMES = ("Leaf", "Parent", "Recursive", "MutualA", "MutualB", "Wide",
         "Proto3Leaf", "Proto3Parent", "Proto3Recursive")


def generate():
    wide = ("\nmessage Wide {\n"
            + "".join(f"  optional int64 value_{i} = {i};\n" for i in range(1, 261))
            + "  repeated int32 values = 261;\n  optional Leaf child = 262;\n"
              '  optional bytes data = 263 [default = "wide"];\n}\n')
    schemas = {
        "static_children.proto": (HERE / "static_children.proto").read_text() + wide,
        "static_children3.proto": '''syntax = "proto3";
package static_children;
option java_multiple_files = true;
message Proto3Leaf {
  int32 value = 1;
  float real = 2;
  repeated int32 values = 3;
  bytes data = 4;
  optional bool present = 5;
}
message Proto3Parent { Proto3Leaf child = 1; repeated int32 values = 2; }
message Proto3Recursive { Proto3Recursive child = 1; int32 value = 2; }
''',
    }
    sources = {}
    for variant, compiler in (("baseline", OLD_PROTOC), ("candidate", ROOT / "bazel-bin/protoc")):
        proto = OUTPUT / variant / "proto"
        source = OUTPUT / variant / "source"
        proto.mkdir(parents=True, exist_ok=True)
        source.mkdir(parents=True, exist_ok=True)
        for filename, content in schemas.items():
            (proto / filename).write_text(content)
        run([compiler, f"-I{proto}", f"--java_out={source}", *schemas],
            OUTPUT / f"generate-{variant}.log")
        sources[variant] = source
    counted = OUTPUT / "instrumented"
    for path in sources["candidate"].rglob("*.java"):
        text = path.read_text()
        if path.stem in NAMES:
            text, ordinary = re.subn(r"(?m)^([ \t]*)super\(parent\);$",
                r"\1super(parent);\n\1builder_probe.ChildParserCounts.ordinary++;", text)
            text, raw = re.subn(r"(?m)^([ \t]*)super\(null, unknownState\);$",
                r"\1super(null, unknownState);\n\1builder_probe.ChildParserCounts.raw++;", text)
            assert ordinary == raw == 1, path
            call = "continuation.mergeFrom(input, extensionRegistry, pendingTag);"
            if call in text:
                parse = method_body(text, "static java.lang.Object parseState_(")
                values = "values_" if re.search(r"\bvalues_\b", parse) else "null"
                observed = f"continuation, {values}, unknownFields, liveValues"
                text = text.replace(call, "boolean liveValues = false;\n"
                    "if (pendingTag != 0) liveValues = builder_probe.ChildParserCounts.handoff("
                    f"continuation, input, pendingTag, {values}, unknownFields);\n"
                    + call + "\nif (pendingTag != 0) builder_probe.ChildParserCounts.checkState("
                    + observed + ");")
        target = counted / path.relative_to(sources["candidate"])
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)
    sources["instrumented"] = counted
    return sources


def preservation(sources, names=NAMES, package="static_children"):
    for name in names:
        before = (sources["baseline"] / package / f"{name}.java").read_text()
        after = (sources["candidate"] / package / f"{name}.java").read_text()
        signature = "public Builder mergeFrom(\n        com.google.protobuf.CodedInputStream input,"
        expected = method_body(before, signature)
        if "int pendingTag)" in after:
            signature = "private Builder mergeFrom(\n        com.google.protobuf.CodedInputStream input,"
            actual = method_body(after, signature)
            pending = ("          int tag;\n          if (pendingTag != 0) {\n"
                       "            tag = pendingTag;\n            pendingTag = 0;\n"
                       "          } else {\n            tag = input.readTag();\n          }")
            actual = actual.replace(pending, "          int tag = input.readTag();")
        else:
            actual = method_body(after, signature)
        assert expected == actual, (name, "stateful decode")
        for helper in re.findall(r"private void ((?:parse\w+|ensure\w+|clear\w+HasBits|buildPartial_autosplit_\d+))\(", before):
            signature = "private void " + helper + "("
            candidate_signature = signature.replace("private void", "private static void") \
                if helper.startswith("buildPartial_autosplit_") else signature
            assert method_body(before, signature) == method_body(after, candidate_signature), (name, helper)
        for helper in re.findall(r"(internalGet\w+FieldBuilder)\(\) \{", before):
            signature = helper + "()"
            actual = method_body(after, signature)
            actual = actual.replace(", /* retained= */ false", "")
            assert method_body(before, signature) == actual, (name, helper)
        for signature in ("public static final class Builder extends",
                          "PARSER = new com.google.protobuf.AbstractParser<"):
            before = before.replace(method_body(before, signature), "{}")
            after = after.replace(method_body(after, signature), "{}")
        assert before == after, (name, "message source")
    print("PASS: existing instance decoder, parsing/mutability/publication bodies and message source preserved",
          flush=True)


def bytecode(classes, destination, names=NAMES, package="static_children", max_slots_limit=66):
    result = {}
    for name in names:
        message = run([JAVA / "bin/javap", "-p", "-c", "-s", "-cp", classes,
                       package + "." + name], destination / f"{name}-message.javap")
        builder = run([JAVA / "bin/javap", "-p", "-c", "-s", "-cp", classes,
                       package + "." + name + "$Builder"], destination / f"{name}-builder.javap")
        methods = re.split(r"(?m)^  (?=\S.*\(.*\))", builder)[1:]
        max_slots = 0
        for method in methods:
            signature = method.splitlines()[0]
            if " installParsingState_" in signature or " buildPartial_autosplit_" in signature:
                descriptor = re.search(r"descriptor: \(([^)]*)\)", method).group(1)
                types = re.findall(r"\[*(?:L[^;]*;|[BCDFIJSZ])", descriptor)
                slots = sum(2 if t in ("J", "D") else 1 for t in types)
                slots += 0 if " static " in signature else 1
                max_slots = max(slots, max_slots)
                assert slots <= max_slots_limit, (name, signature, slots)
            if " installParsingState_" in signature:
                ops = re.findall(r"(?m)^\s+\d+: (\w+)", method)
                assert all(op.startswith(("aload", "iload", "lload", "dload", "fload"))
                           or op in ("putfield", "return") for op in ops), (name, ops)
            if "$Builder(java.lang.Object);" in signature:
                ops = re.findall(r"(?m)^\s+\d+: (\w+)", method)
                assert ops == ["aload_0", "aconst_null", "aload_1", "invokespecial", "return"], name
            if " parsePartialFrom_(" in signature or " parseState_(" in signature:
                assert not re.search(r"\b(?:anewarray|newarray|invokedynamic)\b", method), name
            if " restore" in signature:
                assert not re.search(r"// Method .*\b(?:mergeFrom|toBuilder):", method), name
        result[name] = {
            "message_class_bytes": (classes / package / f"{name}.class").stat().st_size,
            "message_methods": len(re.findall(r"(?m)^  \S.*\(.*\)", message)),
            "message_access_bridges": len(re.findall(r"(?m)^  .* access\$\d+\(", message)),
            "builder_class_bytes": (classes / package / f"{name}$Builder.class").stat().st_size,
            "max_shard_parameter_slots": max_slots,
        }
    return result


def imported_compatibility():
    directory = OUTPUT / "old-dependency"
    directory.mkdir(exist_ok=True)
    (directory / "old_child.proto").write_text('''syntax = "proto2";
package old_dependency;
option java_multiple_files = true;
message OldChild { optional int32 value = 1; }
''')
    (directory / "new_parent.proto").write_text('''syntax = "proto2";
package old_dependency;
import "old_child.proto";
option java_multiple_files = true;
message NewParent { optional OldChild child = 1; }
''')
    run([OLD_PROTOC, f"-I{directory}", f"--java_out={directory}", "old_child.proto"],
        directory / "generate-old.log")
    run([ROOT / "bazel-bin/protoc", f"-I{directory}", f"--java_out={directory}", "new_parent.proto"],
        directory / "generate-new.log")
    parent = (directory / "old_dependency/NewParent.java").read_text()
    assert "Builder.parsePartialFrom_(input," in parent
    run([JAVA / "bin/javac", "--release", 8, "-cp", RUNTIME, "-d", directory,
         *sorted((directory / "old_dependency").glob("*.java"))], directory / "compile.log")
    print("PASS: imported old generated child remains source-compatible with the current runtime", flush=True)


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    sources = generate()
    preservation(sources)
    imported_compatibility()
    metrics = {}
    for release in (8, 17):
        directory = OUTPUT / f"java{release}"
        directory.mkdir(exist_ok=True)
        classes = {}
        for variant, source in sources.items():
            target = directory / variant
            target.mkdir(exist_ok=True)
            runtime = OLD_RUNTIME if variant == "baseline" else RUNTIME
            tests = [HERE / "ChildParserCounts.java", HERE / "GeneratedChildParserTest.java"] \
                if variant != "candidate" else []
            run([JAVA / "bin/javac", "--release", release, "-cp", runtime, "-d", target,
                 *sorted(source.rglob("*.java")), *tests], directory / f"compile-{variant}.log")
            classes[variant] = target
        metrics[release] = {}
        for variant in ("baseline", "candidate"):
            dumps = directory / f"{variant}-bytecode"
            dumps.mkdir(exist_ok=True)
            metrics[release][variant] = bytecode(classes[variant], dumps)
        for name, after in metrics[release]["candidate"].items():
            before = metrics[release]["baseline"][name]
            for key in ("message_class_bytes", "message_methods", "message_access_bridges"):
                assert after[key] <= before[key], (release, name, key, before[key], after[key])
        print(f"release {release}: no message class growth, members or access bridges", flush=True)
        for forced in (False, True):
            oracle = directory / ("oracle-forced.txt" if forced else "oracle.txt")
            extra = ["forced"] if forced else []
            run([JAVA / "bin/java", "-Xint", "-Xmx256m", "-cp", f"{classes['baseline']}:{OLD_RUNTIME}",
                 "GeneratedChildParserTest", "oracle", oracle, *extra],
                directory / f"oracle-{forced}.log")
            for home in ((JAVA, JAVA8) if release == 8 else (JAVA,)):
                result = run([home / "bin/java", "-Xint", "-Xmx256m", "-cp",
                              f"{classes['instrumented']}:{RUNTIME}", "GeneratedChildParserTest",
                              "candidate", oracle, *extra], directory / f"test-{forced}-{home.name}.log")
                print(f"release {release}, {home.name}: {result.strip()}", flush=True)
    (OUTPUT / "sizes.json").write_text(json.dumps(metrics, indent=2) + "\n")


if __name__ == "__main__":
    main()
