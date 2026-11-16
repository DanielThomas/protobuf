#!/usr/bin/env python3
"""Exercise retained children in actual protoc output, not generated-source algorithms."""
import json
import re

from check_generated_children import (JAVA, JAVA8, OLD_PROTOC, OLD_RUNTIME, RUNTIME,
                                      bytecode, preservation)
from check_generated_primitives import HERE, ROOT, method_body, run

OUTPUT = HERE / "out/generator/oneofs"
NAMES = ("Leaf", "RepeatedChoices", "Envelope", "Ordinary", "Recursive", "ScalarChoice",
         "Multi", "ManyChoices", "WideChoices", "MutualA", "MutualB")


def generate():
    schema = (HERE / "retained_children.proto").read_text()
    schema += "\nmessage ManyChoices {\n" + "".join(
        f"  oneof choice_{i} {{ Leaf child_{i} = {i}; }}\n" for i in range(1, 33)) + "}\n"
    schema += "\nmessage WideChoices {\n  oneof choice {\n" + "".join(
        f"    Leaf child_{i} = {i};\n" for i in range(1, 34)) + "    int32 value = 34;\n  }\n}\n"
    sources = {}
    for variant, compiler in (("baseline", OLD_PROTOC), ("candidate", ROOT / "bazel-bin/protoc")):
        proto = OUTPUT / variant / "proto"
        source = OUTPUT / variant / "source"
        proto.mkdir(parents=True, exist_ok=True)
        source.mkdir(parents=True, exist_ok=True)
        (proto / "retained_children.proto").write_text(schema)
        run([compiler, f"-I{proto}", f"--java_out={source}", "retained_children.proto"],
            OUTPUT / f"generate-{variant}.log")
        sources[variant] = source
    counted = OUTPUT / "instrumented"
    for path in sources["candidate"].rglob("*.java"):
        text = path.read_text()
        if path.stem in NAMES:
            text, ordinary = re.subn(r"(?m)^([ \t]*)super\(parent\);$",
                r"\1super(parent);\n\1promotion_probe.ExchangeCounts.builders++;", text)
            text, raw = re.subn(r"(?m)^([ \t]*)super\(null, unknownState\);$",
                r"\1super(null, unknownState);\n\1promotion_probe.ExchangeCounts.builders++;", text)
            assert ordinary == raw == 1, path
            text = re.sub(r"(?m)^([ \t]*)(return com.google.protobuf.SingleFieldBuilder.fromBuilder\()",
                r"\1promotion_probe.ExchangeCounts.coldChild = state;\n"
                r"\1promotion_probe.ExchangeCounts.wrappers++;\n\1\2", text)
            text = re.sub(r"(?m)^([ \t]*)((?:continuation\.)?\w+Builder_ = new com.google.protobuf.SingleFieldBuilder[<(])",
                r"\1promotion_probe.ExchangeCounts.wrappers++;\n\1\2", text)
            call = "continuation.mergeFrom(input, extensionRegistry, pendingTag);"
            text = text.replace(call, "promotion_probe.ExchangeCounts.handoff(input, pendingTag);\n" + call)
        target = counted / path.relative_to(sources["candidate"])
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)
    sources["instrumented"] = counted
    return sources


def tests():
    directory = OUTPUT / "tests"
    directory.mkdir(exist_ok=True)
    for name in ("ChildExchangeTest", "CleanPromotionTest"):
        text = (HERE / f"{name}.java").read_text()
        for message in ("Envelope", "Ordinary", "Recursive"):
            text = text.replace(f"import promotion_probe.{message};", f"import builder_probe.{message};")
        if name == "ChildExchangeTest":
            text = text.replace("Recursive.getDefaultInstance());",
                "Recursive.getDefaultInstance(), builder_probe.ScalarChoice.getDefaultInstance(),\n"
                "      builder_probe.Multi.getDefaultInstance(), builder_probe.ManyChoices.getDefaultInstance(),\n"
                "      builder_probe.WideChoices.getDefaultInstance(), builder_probe.MutualA.getDefaultInstance(),\n"
                "      builder_probe.MutualB.getDefaultInstance());")
            text = text.replace("private static byte[][] wires(int type) throws Exception {",
                "private static byte[][] wires(int type) throws Exception {\n"
                "    if (type >= 5) return RetainedChildCases.wires(type - 5);")
            text = text.replace("List<Message> messages = Arrays.<Message>asList(leaf, choices,",
                "List<Message> messages = new java.util.ArrayList<Message>(Arrays.<Message>asList(leaf, choices,")
            text = text.replace("Ordinary.newBuilder().setChild(choices).build(), recursive);",
                "Ordinary.newBuilder().setChild(choices).build(), recursive));\n"
                "      messages.addAll(RetainedChildCases.happy(i));")
            text = text.replace("1,920", "4,224")
            text = text.replace("          check(Arrays.equals(result.toByteArray(), bytes),",
                '          check(ExchangeCounts.wrappers == 0, "Happy parse constructed field wrappers");\n'
                "          check(Arrays.equals(result.toByteArray(), bytes),")
            text = text.replace("zero Builders on 4,224", "zero Builders or field wrappers on 4,224")
            text = text.replace('Envelope.Builder.class.getMethod("parseState",',
                'Envelope.Builder.class.getDeclaredMethod("parseState_",')
            text = text.replace("    ExchangeCounts.reset();\n    Object state = parse.invoke",
                "    parse.setAccessible(true);\n    ExchangeCounts.reset();\n    Object state = parse.invoke")
            text = text.replace("      continuation();", "      continuation();\n      RetainedChildCases.continuation();")
        else:
            text = text.replace("import promotion_probe.Values;\n", "")
            text = text.replace('builder.getMethod(retained ? "restoreParsingState" : "restorePublishedState", message.getClass())',
                'builder.getDeclaredMethod("restoreState", message.getClass(), boolean.class)')
            text = text.replace("    return (GeneratedMessage.Builder<?>) method.invoke(null, message);",
                "    method.setAccessible(true);\n    return (GeneratedMessage.Builder<?>) method.invoke(null, message, retained);")
            body = method_body(text, "private static void sharing()")
            text = text.replace(body, body[:body.index("    byte[] raw =")] + "  }")
        (directory / f"{name}.java").write_text(text)
    return [directory / "ChildExchangeTest.java", directory / "CleanPromotionTest.java",
            HERE / "ExchangeCounts.java", HERE / "RetainedChildCases.java"]


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    sources = generate()
    preservation(sources, NAMES, "builder_probe")
    test_sources = tests()
    metrics = {}
    preservation_failures = []
    for release in (8, 17):
        directory = OUTPUT / f"java{release}"
        directory.mkdir(exist_ok=True)
        classes = {}
        for variant, source in sources.items():
            target = directory / variant
            target.mkdir(exist_ok=True)
            runtime = OLD_RUNTIME if variant == "baseline" else RUNTIME
            run([JAVA / "bin/javac", "--release", release, "-cp", runtime, "-d", target,
                 *sorted(source.rglob("*.java")), *(test_sources if variant != "candidate" else [])],
                directory / f"compile-{variant}.log")
            classes[variant] = target
        metrics[release] = {}
        for variant in ("baseline", "candidate"):
            dumps = directory / f"{variant}-bytecode"
            dumps.mkdir(exist_ok=True)
            metrics[release][variant] = bytecode(classes[variant], dumps, NAMES, "builder_probe", 98)
        for name, after in metrics[release]["candidate"].items():
            before = metrics[release]["baseline"][name]
            for key in ("message_class_bytes", "message_methods", "message_access_bridges"):
                if after[key] > before[key]:
                    preservation_failures.append((release, name, key, before[key], after[key]))
        assert metrics[release]["candidate"]["ManyChoices"]["max_shard_parameter_slots"] == 98
        (OUTPUT / "sizes.json").write_text(json.dumps(metrics, indent=2) + "\n")
        deltas = {name: after["message_class_bytes"] - metrics[release]["baseline"][name]["message_class_bytes"]
                  for name, after in metrics[release]["candidate"].items()}
        print(f"release {release}: no added message members/bridges; class-byte deltas {deltas}; max 98 slots", flush=True)
        for forced in (False, True):
            oracle = directory / ("oracle-forced" if forced else "oracle")
            extra = ["forced"] if forced else []
            run([JAVA / "bin/java", "-Xint", "-Xmx256m", "-cp", f"{classes['baseline']}:{OLD_RUNTIME}",
                 "ChildExchangeTest", "oracle", oracle, *extra], directory / f"oracle-{forced}.log")
            for home in ((JAVA, JAVA8) if release == 8 else (JAVA,)):
                result = run([home / "bin/java", "-Xint", "-Xmx256m", "-cp",
                              f"{classes['instrumented']}:{RUNTIME}", "ChildExchangeTest",
                              "candidate", oracle, *extra], directory / f"test-{forced}-{home.name}.log")
                print(f"release {release}, {home.name}: {result.strip()}", flush=True)
        oracle = directory / "promotion-oracle"
        run([JAVA / "bin/java", "-Xint", "-Xmx256m", "-cp", f"{classes['baseline']}:{OLD_RUNTIME}",
             "CleanPromotionTest", "oracle", oracle], directory / "promotion-oracle.log")
        for home in ((JAVA, JAVA8) if release == 8 else (JAVA,)):
            result = run([home / "bin/java", "-Xint", "-Xmx256m", "-cp",
                          f"{classes['instrumented']}:{RUNTIME}", "CleanPromotionTest", "candidate", oracle],
                         directory / f"promotion-{home.name}.log")
            print(f"release {release}, {home.name}: {result.strip()}", flush=True)
    assert not preservation_failures, preservation_failures


if __name__ == "__main__":
    main()
