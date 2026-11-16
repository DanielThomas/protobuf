#!/usr/bin/env python3
"""All normal field shapes plus explicit legacy compatibility paths."""
import re
import shutil

import check_generated_children as children
import check_generated_oneofs as oneofs
from check_generated_primitives import HERE, ROOT, method_body, run

OUTPUT = HERE / "out/generator/coverage"
EXTRA = {name: (HERE / name).read_text() for name in
         ("all_fields.proto", "all_fields3.proto", "all_fields_editions.proto")}
EXTRA["values.proto"] = '''syntax = "proto2";
package promotion_probe;
option java_multiple_files = true;
message Values {
  optional string text = 1;
  repeated string texts = 2;
  map<int32, int32> numbers = 3;
  optional bytes data = 4 [default = "abc"];
  optional int32 marker = 5 [default = 17];
}
'''
EXTRA["code_size.proto"] = '''syntax = "proto2";
package compatibility;
option java_multiple_files = true;
option optimize_for = CODE_SIZE;
message CodeSize { optional int32 value = 1; }
'''
EXTRA["old_child.proto"] = '''syntax = "proto2";
package old_dependency;
option java_multiple_files = true;
import "retained_children.proto";
message OldChild {
  oneof choice {
    builder_probe.Leaf first = 1;
    builder_probe.Leaf second = 2;
    int32 value = 3;
  }
}
'''
EXTRA["new_parent.proto"] = '''syntax = "proto2";
package old_dependency;
option java_multiple_files = true;
import "old_child.proto";
message NewParent {
  oneof choice { OldChild selected = 1; int32 value = 2; }
  optional OldChild child = 3;
  map<string, OldChild> children = 4;
  repeated OldChild values = 5;
}
'''
EXTRA["outlining.proto"] = ('''syntax = "proto2";
package outlining;
option java_multiple_files = true;
message Child { optional int32 value = 1; }
message Names {
  oneof choice {
    Child state = 1;
    Child partial_from = 2;
    Child first = 3;
    Child first_state = 4;
  }
}
message WideOneof {
  oneof choice {
''' + ''.join(f'    Child value_{i} = {i};\n' for i in range(1, 129))
    + '  }\n  optional Child sibling = 129;\n}\n')
PACKAGES = {
    "builder_probe": oneofs.NAMES,
    "full_shapes": ("Rich", "RequiredChild", "Groups", "Groups$Legacy", "Groups$Legacies",
                    "Extensible", "MessageSet", "CompatibilityParent"),
    "open_shapes": ("OpenRich",),
    "edition_shapes": ("Delimited",),
    "promotion_probe": ("Values",),
    "outlining": ("Child", "Names", "WideOneof"),
    "compatibility": ("CodeSize",),
    "old_dependency": ("OldChild", "NewParent"),
}
original_generate = oneofs.generate
original_tests = oneofs.tests
original_run = oneofs.run


def run_with_precompiled_child(command, log):
    result = original_run(command, log)
    if log.name in ("compile-candidate.log", "compile-instrumented.log"):
        target = log.parent / log.name[len("compile-"):-len(".log")] / "old_dependency"
        for path in (log.parent / "baseline/old_dependency").glob("OldChild*.class"):
            shutil.copyfile(path, target / path.name)
    return result


def generate():
    sources = original_generate()
    for variant, compiler in (("baseline", oneofs.OLD_PROTOC), ("candidate", ROOT / "bazel-bin/protoc")):
        proto = OUTPUT / variant / "proto"
        for filename, content in EXTRA.items():
            (proto / filename).write_text(content)
        run([compiler, f"-I{proto}", f"--java_out={sources[variant]}", *EXTRA],
            OUTPUT / f"generate-extra-{variant}.log")
    for path in (sources["baseline"] / "old_dependency").glob("OldChild*.java"):
        (sources["candidate"] / "old_dependency" / path.name).write_text(path.read_text())
    for path in sources["candidate"].rglob("*.java"):
        text = path.read_text()
        text = re.sub(r"(?m)^([ \t]*)super\(parent\);$",
            r"\1super(parent);\n\1promotion_probe.ExchangeCounts.builders++;", text)
        text = re.sub(r"(?m)^([ \t]*)super\(null, unknownState\);$",
            r"\1super(null, unknownState);\n\1promotion_probe.ExchangeCounts.builders++;", text)
        text = re.sub(r"(?m)^([ \t]*)(return com.google.protobuf.SingleFieldBuilder.fromBuilder\()",
            r"\1promotion_probe.ExchangeCounts.coldChild = state;\n"
            r"\1promotion_probe.ExchangeCounts.wrappers++;\n\1\2", text)
        text = re.sub(r"(?m)^([ \t]*)((?:continuation\.)?\w+Builder_ = new com.google.protobuf.(?:Single|Repeated)FieldBuilder[<(])",
            r"\1promotion_probe.ExchangeCounts.wrappers++;\n\1\2", text)
        call = "continuation.mergeFrom(input, extensionRegistry, pendingTag);"
        text = text.replace(call, "promotion_probe.ExchangeCounts.handoff(input, pendingTag);\n" + call)
        target = sources["instrumented"] / path.relative_to(sources["candidate"])
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)
    return sources


def bodies(source, signature):
    result = []
    at = 0
    while (at := source.find(signature, at)) >= 0:
        body = method_body(source[at:], signature)
        start = source.index("{", at)
        result.append((start, start + len(body), body))
        at = start + len(body)
    return result


def preservation(sources, *unused):
    for package, names in PACKAGES.items():
        for name in (n for n in names if "$" not in n):
            path = package + "/" + name + ".java"
            before = (sources["baseline"] / path).read_text()
            after = (sources["candidate"] / path).read_text()
            signature = "public static final class Builder extends"
            expected = bodies(before, signature)
            actual = bodies(after, signature)
            assert len(expected) == len(actual), (name, "Builder classes")
            for (_, _, old), (_, _, new) in zip(expected, actual):
                signature = re.search(r"public Builder mergeFrom\(\n( +)com.google.protobuf.CodedInputStream input,", old)
                if signature is None:
                    continue
                extra_indent = len(signature[1]) - 8
                if extra_indent:
                    old = re.sub(r"(?m)^ {" + str(extra_indent) + "}", "", old)
                    new = re.sub(r"(?m)^ {" + str(extra_indent) + "}", "", new)
                public = "public Builder mergeFrom(\n        com.google.protobuf.CodedInputStream input,"
                private = public.replace("public", "private")
                pending = ("          int tag;\n          if (pendingTag != 0) {\n"
                           "            tag = pendingTag;\n            pendingTag = 0;\n"
                           "          } else {\n            tag = input.readTag();\n          }")
                decode = method_body(new, private if private in new else public)
                assert method_body(old, public) == decode.replace(pending, "          int tag = input.readTag();"), (name, "decode")
                for helper in re.findall(r"private void ((?:parse\w+|ensure\w+|clear\w+HasBits|buildPartial_autosplit_\d+))\(", old):
                    sig = "private void " + helper + "("
                    static = sig.replace("private void", "private static void")
                    actual_body = method_body(new, static if static in new else sig)
                    old_body = method_body(old, sig)
                    if helper.startswith("buildPartial_autosplit_"):
                        for capitalized in re.findall(r"internalGet(\w+)\(\)", old_body):
                            field = capitalized[0].lower() + capitalized[1:] + "_"
                            actual_body = re.sub(r"(?<== )" + field + r"(?=[.;])",
                                                "internalGet" + capitalized + "()", actual_body)
                    assert old_body == actual_body, (name, helper, old_body, actual_body)
                for helper in re.findall(r"(internalGet\w+FieldBuilder)\(\) \{", old):
                    actual_body = method_body(new, helper + "()")
                    actual_body = actual_body.replace(", /* retained= */ false", "")
                    assert method_body(old, helper + "()") == actual_body, (name, helper)
            for signature in ("public static final class Builder extends", "PARSER = new com.google.protobuf.AbstractParser<"):
                for label in ("before", "after"):
                    value = before if label == "before" else after
                    for start, end, body in reversed(bodies(value, signature)):
                        value = value[:start] + "{}" + value[end:]
                    if label == "before": before = value
                    else: after = value
            assert before == after, (name, "message source")
    print("PASS: all instance decoders/helpers preserved; only map publication storage receivers rebound", flush=True)


def bytecode(classes, destination, *unused, **options):
    result = {}
    for package, names in PACKAGES.items():
        for name in names:
            path = destination / package / name
            path.mkdir(parents=True, exist_ok=True)
            metrics = children.bytecode(classes, path, (name,), package, 98)[name]
            if (package == "outlining" and name == "WideOneof"
                    and destination.name == "candidate-bytecode"):
                builder = (path / f"{name}-builder.javap").read_text()
                method = next(m for m in re.split(r"(?m)^  (?=\S.*\(.*\))", builder)
                              if " java.lang.Object parseState_(" in m.splitlines()[0])
                size = int(re.findall(r"(?m)^\s+(\d+): [a-z]", method)[-1]) + 1
                assert size < 8000, (name, "static decoder bytes", size)
                print(f"release output: {name} static decoder {size} bytes", flush=True)
            result[name if package == "builder_probe" else package + "." + name] = metrics
    return result


def tests():
    paths = original_tests()
    directory = OUTPUT / "tests"
    path = directory / "ChildExchangeTest.java"
    text = path.read_text()
    text = text.replace("private static void compare(", "static void compare(")
    text = text.replace("private static CodedInputStream input(", "static CodedInputStream input(")
    text = text.replace("ExtensionRegistryLite.getEmptyRegistry();", "CoverageCases.registry();")
    body = method_body(text, "private static void corpus()")
    text = text.replace(body, body[:-1] + "  CoverageCases.corpus();\n  }")
    text = text.replace("      RetainedChildCases.continuation();", "      RetainedChildCases.continuation();\n      CoverageCases.continuation();")
    text = text.replace('    System.out.println("PASS: zero Builders or field wrappers on 4,224',
                        '    CoverageCases.happy();\n    System.out.println("PASS: zero Builders or field wrappers on 4,224')
    # The pristine UTF-8 oneof helper can publish an active case with no value when decoding fails.
    # Compare its failure-to-serialize too, rather than changing the unfinished message contract.
    body = method_body(text, "private static String encode(")
    text = text.replace(body, '''{
    try {
      immutable(message);
      return Base64.getEncoder().encodeToString(message.toByteArray());
    } catch (RuntimeException failure) {
      return "unserializable:" + failure.getClass().getName();
    }
  }''')
    path.write_text(text)
    text = (HERE / "CleanPromotionTest.java").read_text()
    for message in ("Envelope", "Ordinary", "Recursive"):
        text = text.replace(f"import promotion_probe.{message};", f"import builder_probe.{message};")
    text = text.replace('builder.getMethod(retained ? "restoreParsingState" : "restorePublishedState", message.getClass())',
                        'builder.getDeclaredMethod("restoreState", message.getClass(), boolean.class)')
    text = text.replace("    return (GeneratedMessage.Builder<?>) method.invoke(null, message);",
                        "    method.setAccessible(true);\n    return (GeneratedMessage.Builder<?>) method.invoke(null, message, retained);")
    (directory / "CleanPromotionTest.java").write_text(text)
    return paths + [HERE / "CoverageCases.java"]


if __name__ == "__main__":
    oneofs.OUTPUT = OUTPUT
    oneofs.generate = generate
    oneofs.tests = tests
    oneofs.preservation = preservation
    oneofs.bytecode = bytecode
    oneofs.run = run_with_precompiled_child
    oneofs.main()
