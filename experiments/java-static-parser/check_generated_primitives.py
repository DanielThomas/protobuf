#!/usr/bin/env python3
import argparse
import json
import os
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
BASELINE = Path(os.environ.get("PROTOBUF_BASELINE_ROOT",
                               ROOT.parent / "protobuf-static-parser-baseline"))
JAVA = Path(os.environ["JAVA_HOME"])
JAVA8 = Path(os.environ["JAVA8_HOME"])


def run(command, log):
    result = subprocess.run([str(part) for part in command], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    log.write_text(result.stdout)
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}); see {log}")
    return result.stdout


def method_body(source, signature):
    start = source.index("{", source.index(signature))
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def check_preservation(generated):
    for name in ("Scalars", "Empty", "Wide", "Repeated"):
        before = (generated["baseline"] / "static_parser" / f"{name}.java").read_text()
        after = (generated["candidate"] / "static_parser" / f"{name}.java").read_text()
        signature = "public Builder mergeFrom(\n        com.google.protobuf.CodedInputStream input,"
        assert method_body(before, signature) == method_body(after, signature), name
        for shard in re.findall(r"private void (buildPartial_autosplit_\d+)\(", before):
            assert method_body(before, "private void " + shard + "(") == method_body(
                after, "private static void " + shard + "("), shard
        for helper in re.findall(r"private void (ensure\w+IsMutable)\(", before):
            signature = "private void " + helper + "("
            assert method_body(before, signature) == method_body(after, signature), helper

        def message_source(source):
            for signature in ("public static final class Builder extends",
                              "PARSER = new com.google.protobuf.AbstractParser<"):
                source = source.replace(method_body(source, signature), "{}")
            return source

        assert message_source(before) == message_source(after), name


def measure(java_home, classes, source, output):
    results = {}
    for name in ("Scalars", "Empty", "Wide", "Repeated"):
        bytecode = run([java_home / "bin/javap", "-private", "-c", "-s", "-classpath", classes,
                        f"static_parser.{name}$Builder"], output / f"{name}-builder.javap")
        outer_bytecode = run([java_home / "bin/javap", "-private", "-c", "-s", "-classpath", classes,
                              f"static_parser.{name}"], output / f"{name}-message.javap")
        methods = re.split(r"(?m)^  (?=\S.*\(.*\))", bytecode)[1:]
        sizes = {}
        publication_slots = []
        for method in methods:
            signature = method.splitlines()[0].strip()
            offsets = re.findall(r"(?m)^\s+(\d+): [a-z]", method)
            if offsets:
                sizes[signature] = int(offsets[-1]) + 1
            if " buildPartial_autosplit_" in signature:
                descriptor = re.search(r"descriptor: \(([^)]*)\)", method).group(1)
                types = re.findall(r"\[*(?:L[^;]*;|[BCDFIJSZ])", descriptor)
                slots = sum(2 if t in ("J", "D") else 1 for t in types)
                slots += 0 if " static " in signature else 1
                assert slots <= 255, (name, signature, slots)
                publication_slots.append(slots)
            if " parseState_(" in signature:
                assert f"Lstatic_parser/{name}$Builder;" not in method, name
                assert not re.search(r"\bnew\s+.*class static_parser/.*\$Builder\b", method), name
        class_files = list((classes / "static_parser").glob(f"{name}*.class"))
        results[name] = {
            "source_bytes": (source / "static_parser" / f"{name}.java").stat().st_size,
            "class_bytes": sum(path.stat().st_size for path in class_files),
            "message_class_bytes": (classes / "static_parser" / f"{name}.class").stat().st_size,
            "message_methods": len(re.findall(r"(?m)^  \S.*\(.*\)", outer_bytecode)),
            "builder_methods": len(methods),
            "builder_access_bridges": len(re.findall(r"(?m)^  .* access\$\d+\(", bytecode)),
            "message_access_bridges": len(re.findall(r"(?m)^  .* access\$\d+\(", outer_bytecode)),
            "publication_shards": len(publication_slots),
            "publication_parameter_slots": max(publication_slots, default=0),
            "largest_builder_method": max(sizes.values()),
            "static_decode_bytes": next((size for method, size in sizes.items()
                                          if " parseState_(" in method), 0),
        }
    return results


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-protoc", type=Path,
                        default=BASELINE / "bazel-bin/protoc")
    parser.add_argument("--protoc", type=Path, default=ROOT / "bazel-bin/protoc")
    parser.add_argument("--runtime", type=Path,
                        default=ROOT / "bazel-bin/java/core/libcore_bundle.jar")
    parser.add_argument("--java-home", type=Path, default=JAVA)
    parser.add_argument("--java8-home", type=Path, default=JAVA8)
    args = parser.parse_args()
    output = HERE / "out/generator/primitives"
    output.mkdir(parents=True, exist_ok=True)
    schema = (HERE / "static_parser.proto").read_text()
    wide = ('syntax = "proto2";\npackage static_parser;\n'
            'option java_multiple_files = true;\nmessage Wide {\n'
            + "".join(f"  optional int64 value_{i} = {i};\n" for i in range(1, 261))
            + "  repeated int32 values = 261;\n  optional int32 marker = 262;\n}\n")
    generated = {}
    for variant, compiler in (("baseline", args.baseline_protoc), ("control", args.baseline_protoc),
                              ("candidate", args.protoc)):
        proto = output / variant / "proto"
        source = output / variant / "source"
        proto.mkdir(parents=True, exist_ok=True)
        source.mkdir(parents=True, exist_ok=True)
        for filename, content in (("static_parser.proto", schema), ("wide_schema.proto", wide)):
            if variant == "control":
                content = content.replace("package static_parser;", "package static_control;")
            (proto / filename).write_text(content)
        run([compiler, f"-I{proto}", f"--java_out={source}", "static_parser.proto", "wide_schema.proto"],
            output / f"generate-{variant}.log")
        generated[variant] = source
    check_preservation(generated)
    print("PASS: existing stateful decode, helper and publication bodies are unchanged; "
          "message source is unchanged outside Builder and Parser entry", flush=True)
    instrumented = output / "instrumented-source"
    count = 0
    for path in generated["candidate"].rglob("*.java"):
        target = instrumented / path.relative_to(generated["candidate"])
        target.parent.mkdir(parents=True, exist_ok=True)
        content, constructors = re.subn(
            r"(?m)^([ \t]*)(super\((?:parent|null, unknownState)\);)$",
            r"\1\2\n\1builder_probe.ConstructorCounts.builders++;", path.read_text())
        count += constructors
        target.write_text(content)
    assert count == 8, count
    metrics = {}
    for release in (8, 17):
        destination = output / f"java{release}"
        destination.mkdir(exist_ok=True)
        classes = {}
        for variant, source in {**generated, "instrumented": instrumented}.items():
            target = destination / variant
            target.mkdir(exist_ok=True)
            sources = sorted(source.rglob("*.java"))
            if variant == "instrumented":
                sources.append(HERE / "ConstructorCounts.java")
            run([args.java_home / "bin/javac", "--release", release, "-cp", args.runtime,
                 "-d", target, *sources], destination / f"compile-{variant}.log")
            classes[variant] = target
        tests = destination / "tests"
        tests.mkdir(exist_ok=True)
        classpath = ":".join(str(path) for path in
                             (classes["instrumented"], classes["control"], args.runtime))
        run([args.java_home / "bin/javac", "--release", 8, "-cp", classpath, "-d", tests,
             HERE / "GeneratedStaticParserTest.java"], destination / "compile-tests.log")
        homes = (args.java_home, args.java8_home) if release == 8 else (args.java_home,)
        for home in homes:
            text = run([home / "bin/java", "-Xint", "-Xmx256m", "-cp", f"{tests}:{classpath}",
                        "GeneratedStaticParserTest"], destination / f"test-{home.name}.log")
            print(f"release {release}, {home.name}: {text.strip()}")
        metrics[release] = {}
        for variant in ("baseline", "candidate"):
            dumps = destination / f"{variant}-bytecode"
            dumps.mkdir(exist_ok=True)
            metrics[release][variant] = measure(args.java_home, classes[variant],
                                                generated[variant], dumps)
        for name, candidate in metrics[release]["candidate"].items():
            baseline = metrics[release]["baseline"][name]
            for metric in ("message_class_bytes", "message_methods", "message_access_bridges"):
                assert candidate[metric] <= baseline[metric], (release, name, metric)
        print(f"release {release}: no message class growth, new members or access bridges", flush=True)
    (output / "sizes.json").write_text(json.dumps(metrics, indent=2) + "\n")
    print(json.dumps(metrics, indent=2))


if __name__ == "__main__":
    main()
