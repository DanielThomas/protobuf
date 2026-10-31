// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "google/protobuf/compiler/java/full/field_generator.h"

#include <cstdint>

#include "absl/strings/str_cat.h"
#include "google/protobuf/compiler/java/context.h"
#include "google/protobuf/compiler/java/helpers.h"
#include "google/protobuf/compiler/java/internal_helpers.h"
#include "google/protobuf/compiler/java/name_resolver.h"
#include "google/protobuf/wire_format.h"

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

namespace {
bool IsOrdinaryParserChild(const FieldDescriptor* field) {
  return !field->is_repeated() && !IsRealOneof(field) &&
         GetJavaType(field) == JAVATYPE_MESSAGE;
}
}  // namespace

ParserChildFieldGroups GetParserChildFieldGroups(const Descriptor* descriptor) {
  // Match the builder's 32-field publication shards and bound each child switch.
  constexpr int kFieldsPerMethod = 32;
  ParserChildFieldGroups groups;
  for (const auto* field : SortFieldsByNumber(descriptor)) {
    if (!IsOrdinaryParserChild(field)) continue;
    if (groups.empty() || groups.back().size() == kFieldsPerMethod) {
      groups.emplace_back();
    }
    groups.back().push_back(field);
  }
  if (groups.size() <= 1) groups.clear();
  return groups;
}

void GenerateParserChildHelpers(
    const Descriptor* descriptor,
    const FieldGeneratorMap<ImmutableFieldGenerator>& fields,
    io::Printer* printer) {
  const auto groups = GetParserChildFieldGroups(descriptor);
  for (int i = 0; i < groups.size(); ++i) {
    auto vars = printer->WithVars({{"shard", absl::StrCat(i)}});
    printer->Print(
        "private com.google.protobuf.GeneratedMessage childValue$shard$(\n"
        "    $classname$ instance, int field) {\n"
        "  switch (field) {\n");
    printer->Indent();
    printer->Indent();
    for (const auto* field : groups[i]) {
      fields.get(field).GenerateParserChildAccessorCode(printer);
    }
    printer->Print("default: throw new java.lang.IllegalArgumentException();\n");
    printer->Outdent();
    printer->Outdent();
    printer->Print(
        "  }\n"
        "}\n"
        "private void readChild$shard$(\n"
        "    $classname$ instance, int field, com.google.protobuf.CodedInputStream input,\n"
        "    com.google.protobuf.ExtensionRegistryLite extensionRegistry,\n"
        "    java.util.Map<java.lang.Integer, com.google.protobuf.SingleFieldBuilder> children)\n"
        "    throws java.io.IOException {\n"
        "  switch (field) {\n");
    printer->Indent();
    printer->Indent();
    for (const auto* field : groups[i]) {
      printer->Print("case $number$: {\n", "number", absl::StrCat(field->number()));
      printer->Indent();
      fields.get(field).GenerateParsingCode(printer, ParsingTarget::kMessage);
      printer->Print("return;\n");
      printer->Outdent();
      printer->Print("}\n");
    }
    printer->Print("default: throw new java.lang.IllegalArgumentException();\n");
    printer->Outdent();
    printer->Outdent();
    printer->Print("  }\n}\n");
    if (groups.size() > 1) {
      printer->Print(
          "private void finishChildren$shard$($classname$ instance,\n"
          "    java.util.Map<java.lang.Integer, com.google.protobuf.SingleFieldBuilder> children) {\n");
      printer->Indent();
      for (const auto* field : groups[i]) {
        fields.get(field).GenerateParserFinalizationCode(printer);
      }
      printer->Outdent();
      printer->Print("}\n");
    }
  }
}

void GenerateFieldParsingCases(
    const Descriptor* descriptor,
    const FieldGeneratorMap<ImmutableFieldGenerator>& fields,
    io::Printer* printer, ParsingTarget target) {
  const auto children = target == ParsingTarget::kMessage
      ? GetParserChildFieldGroups(descriptor) : ParserChildFieldGroups{};
  for (int i = 0; i < children.size(); ++i) {
    for (const auto* field : children[i]) {
      printer->Print("case $tag$:\n", "tag", absl::StrCat(static_cast<int32_t>(
          internal::WireFormatLite::MakeTag(
              field->number(),
              internal::WireFormat::WireTypeForFieldType(field->type())))));
    }
    auto vars = printer->WithVars({{"shard", absl::StrCat(i)}});
    // Retain a newly allocated map before reading can fail inside the helper.
    printer->Print(
        "{\n"
        "  if (childValue$shard$(instance, tag >>> 3) != null) children = childBuilders(children);\n"
        "  readChild$shard$(instance, tag >>> 3, input, extensionRegistry, children);\n"
        "  break;\n"
        "}\n");
  }
  for (const auto* field : SortFieldsByNumber(descriptor)) {
    if (!children.empty() && IsOrdinaryParserChild(field)) continue;
    const auto& generator = fields.get(field);
    const bool uniform =
        target == ParsingTarget::kMessage && generator.UniformParserOneof();
    const bool message_oneof = target == ParsingTarget::kMessage &&
                               generator.IsRealOneof() &&
                               GetJavaType(field) == JAVATYPE_MESSAGE;
    const bool grouped = uniform || message_oneof;
    if (grouped) {
      const auto* oneof = field->containing_oneof();
      const FieldDescriptor* first = nullptr;
      for (int i = 0; i < oneof->field_count(); ++i) {
        if (uniform || oneof->field(i)->type() == field->type()) {
          first = oneof->field(i);
          break;
        }
      }
      if (field != first) continue;
    }
    for (bool packed : {false, true}) {
      // Accept packed wire input for every packable field, regardless of its declared encoding.
      if (packed && !field->is_packable()) continue;
      const auto wire_type = packed
          ? internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED
          : internal::WireFormat::WireTypeForFieldType(field->type());
      const std::string tag = absl::StrCat(static_cast<int32_t>(
          internal::WireFormatLite::MakeTag(field->number(), wire_type)));
      if (grouped) {
        const auto* oneof = field->containing_oneof();
        for (int i = 0; i < oneof->field_count(); ++i) {
          if (!uniform && oneof->field(i)->type() != field->type()) continue;
          printer->Print("case $tag$:\n", "tag", absl::StrCat(static_cast<int32_t>(
              internal::WireFormatLite::MakeTag(oneof->field(i)->number(), wire_type))));
        }
        printer->Print("{\n");
      } else {
        printer->Print("case $tag$: {\n", "tag", tag);
      }
      printer->Indent();
      if (target == ParsingTarget::kMessage) {
        generator.GenerateParserParsingCode(printer, packed);
      } else if (packed) {
        generator.GenerateBuilderParsingCodeFromPacked(printer);
      } else {
        generator.GenerateBuilderParsingCode(printer);
      }
      printer->Outdent();
      if (grouped) {
        printer->Print("  break;\n}\n");
      } else {
        printer->Print("  break;\n} // case $tag$\n", "tag", tag);
      }
    }
  }
}

ImmutableFieldGenerator::ImmutableFieldGenerator(
    const FieldDescriptor* descriptor, int bit_index, Context* context)
    : descriptor_(descriptor),
      bit_index_(bit_index),
      context_(context),
      name_resolver_(context->GetNameResolver()) {}

ImmutableFieldGenerator::FieldVariables
ImmutableFieldGenerator::GetParsingVariables(ParsingTarget target) const {
  auto vars = variables_;
  vars["field"] = absl::StrCat(
      IsRealOneof() ? vars.at("oneof_name") : vars.at("name"), "_");
  vars["unknown_receiver"] = "";
  vars["unknown_argument"] = "";
  if (target == ParsingTarget::kMessage) {
    vars["field"] = absl::StrCat("instance.", vars.at("field"));
    if (!descriptor_->is_repeated() && !IsRealOneof() &&
        GetJavaType(descriptor_) == JAVATYPE_STRING) {
      vars["field"] = absl::StrCat("parsed_", vars.at("name"));
    }
    vars["set_has_field_bit"] = HasHasbit()
        ? absl::StrCat("instance.", GenerateSetBit(bit_index_), ";") : "";
    vars["unknown_receiver"] = "unknown = ";
    vars["unknown_argument"] = "unknown, ";
    auto add = vars.find("repeated_add");
    if (add != vars.end()) add->second = absl::StrCat("instance.", add->second);
  }
  return vars;
}

std::string ImmutableFieldGenerator::GetParsingEnsureMutableCode(
    ParsingTarget target, absl::string_view capacity) const {
  if (target == ParsingTarget::kBuilder) {
    return absl::StrCat("ensure", variables_.at("capitalized_name"),
                        "IsMutable(", capacity, ");");
  }
  if (descriptor_->is_packable()) {
    return absl::StrCat("ensure", variables_.at("capitalized_name"),
                        "IsMutable(instance, ", capacity, ");");
  }
  const std::string field = absl::StrCat("instance.", variables_.at("name"), "_");
  std::string copy;
  if (GetJavaType(descriptor_) == JAVATYPE_STRING) {
    copy = absl::StrCat("new com.google.protobuf.LazyStringArrayList(", field, ")");
  } else if (capacity.empty() || capacity == "/* capacity= */ -1") {
    copy = absl::StrCat("makeMutableCopy(", field, ")");
  } else {
    copy = absl::StrCat("makeMutableCopy(", field, ", ", capacity, ")");
  }
  return absl::StrCat("if (!", field, ".isModifiable()) {\n  ", field,
                      " = ", copy, ";\n}");
}

bool ImmutableFieldGenerator::ParserRetainsChildren() const {
  if (!IsRealOneof()) {
    return !descriptor_->is_repeated() &&
           GetJavaType(descriptor_) == JAVATYPE_MESSAGE;
  }
  const auto* oneof = descriptor_->containing_oneof();
  for (int i = 0; i < oneof->field_count(); ++i) {
    if (GetJavaType(oneof->field(i)) == JAVATYPE_MESSAGE) return true;
  }
  return false;
}

void ImmutableFieldGenerator::GenerateOneofParsingCaseUpdate(
    io::Printer* printer, ParsingTarget target) const {
  if (target == ParsingTarget::kBuilder) {
    printer->Print(variables_,
        "switch ($oneof_name$Case_) {\n"
        "default:\n"
        "  clear$oneof_capitalized_name$HasBits(); // fallthrough\n"
        "case 0:\n"
        "  $set_oneof_case_message$;\n"
        "  $set_has_field_bit$ // fallthrough\n"
        "case $number$:\n"
        "  break;\n"
        "}\n");
  } else if (ParserRetainsChildren()) {
    printer->Print(variables_,
        "children = select$oneof_capitalized_name$(instance, $number$, children);\n");
  } else if (UniformParserOneof()) {
    printer->Print(variables_, "instance.$oneof_name$Case_ = tag >>> 3;\n");
  } else {
    printer->Print(variables_, "instance.$oneof_name$Case_ = $number$;\n");
  }
}

void ImmutableFieldGenerator::GenerateParserLocalCode(io::Printer* printer) const {
  if (!descriptor_->is_repeated() && !IsRealOneof() &&
      GetJavaType(descriptor_) == JAVATYPE_STRING) {
    printer->Print(variables_,
                   "java.lang.Object parsed_$name$ = instance.$name$_;\n");
  }
}

bool ImmutableFieldGenerator::UniformParserOneof() const {
  if (!IsRealOneof() || GetJavaType(descriptor_) == JAVATYPE_MESSAGE ||
      GetJavaType(descriptor_) == JAVATYPE_ENUM) return false;
  const auto* oneof = descriptor_->containing_oneof();
  for (int i = 0; i < oneof->field_count(); ++i) {
    const auto* field = oneof->field(i);
    if (field->type() != descriptor_->type()) return false;
    if (GetJavaType(field) == JAVATYPE_STRING &&
        CheckUtf8(field) != CheckUtf8(descriptor_)) return false;
  }
  return true;
}

bool ImmutableFieldGenerator::ParserNeedsHelper() const {
  if (descriptor_->is_map()) return true;
  if (IsRealOneof()) {
    return GetJavaType(descriptor_) != JAVATYPE_MESSAGE && !UniformParserOneof();
  }
  return IsOrdinaryParserChild(descriptor_) &&
         GetParserChildFieldGroups(descriptor_->containing_type()).empty();
}

void ImmutableFieldGenerator::GenerateParserHelperCode(io::Printer* printer) const {
  if (descriptor_->is_repeated() && descriptor_->is_packable()) {
    auto vars = GetParsingVariables(ParsingTarget::kMessage);
    vars["message_type"] =
        name_resolver_->GetImmutableClassName(descriptor_->containing_type());
    printer->Print(vars,
        "private void ensure$capitalized_name$IsMutable($message_type$ instance, int minCapacity) {\n"
        "  if (!$field$.isModifiable()) {\n"
        "    $field$ = makeMutableCopy($field$, minCapacity);\n"
        "  }\n"
        "}\n");
  }
  if (!ParserNeedsHelper()) return;
  auto vars = variables_;
  const bool unknown = descriptor_->is_map() || GetJavaType(descriptor_) == JAVATYPE_ENUM;
  vars["message_type"] = name_resolver_->GetImmutableClassName(descriptor_->containing_type());
  vars["return_type"] = unknown ? "java.lang.Object" : "void";
  vars["unknown_parameter"] = unknown ? ", java.lang.Object unknown" : "";
  vars["children_parameter"] = ParserRetainsChildren()
      ? ", java.util.Map<java.lang.Integer, com.google.protobuf.SingleFieldBuilder> children" : "";
  printer->Print(vars,
      "private $return_type$ parse$capitalized_name$Field(\n"
      "    $message_type$ instance, com.google.protobuf.CodedInputStream input,\n"
      "    com.google.protobuf.ExtensionRegistryLite extensionRegistry$unknown_parameter$$children_parameter$)\n"
      "    throws java.io.IOException {\n");
  printer->Indent();
  GenerateParsingCode(printer, ParsingTarget::kMessage);
  if (unknown) printer->Print("return unknown;\n");
  printer->Outdent();
  printer->Print("}\n");
}

void ImmutableFieldGenerator::GenerateParserParsingCode(
    io::Printer* printer, bool packed) const {
  if (ParserNeedsHelper()) {
    auto vars = variables_;
    const bool unknown = descriptor_->is_map() || GetJavaType(descriptor_) == JAVATYPE_ENUM;
    vars["unknown_result"] = unknown ? "unknown = " : "";
    vars["unknown_argument"] = unknown ? ", unknown" : "";
    vars["children_argument"] = ParserRetainsChildren() ? ", children" : "";
    if (ParserRetainsChildren()) {
      // Retain the map before a helper can fail while merging a child.
      printer->Print(variables_, IsRealOneof()
          ? "if (instance.$oneof_name$_ instanceof com.google.protobuf.GeneratedMessage) children = childBuilders(children);\n"
          : "if (instance.$name$_ != null) children = childBuilders(children);\n");
    }
    printer->Print(vars,
        "$unknown_result$parse$capitalized_name$Field(instance, input, extensionRegistry$unknown_argument$$children_argument$);\n");
  } else if (packed) {
    GenerateParsingCodeFromPacked(printer, ParsingTarget::kMessage);
  } else {
    GenerateParsingCode(printer, ParsingTarget::kMessage);
  }
}

void ImmutableFieldGenerator::GenerateParserFinalizationCode(
    io::Printer* printer) const {
  if (IsRealOneof()) return;
  if (descriptor_->is_map()) {
    printer->Print(variables_,
        "if (instance.$name$_ != null) instance.$name$_.makeImmutable();\n");
  } else if (descriptor_->is_repeated()) {
    printer->Print(variables_, "instance.$name$_.makeImmutable();\n");
  } else if (GetJavaType(descriptor_) == JAVATYPE_MESSAGE) {
    printer->Print(variables_,
        "instance.$name$_ = ($type$) buildChild(children, $number$, instance.$name$_);\n");
  } else if (GetJavaType(descriptor_) == JAVATYPE_STRING) {
    printer->Print(variables_, "instance.$name$_ = parsed_$name$;\n");
  }
}

void ImmutableFieldGenerator::GenerateParserChildAccessorCode(
    io::Printer* printer) const {
  printer->Print(variables_, "case $number$: return instance.$name$_;\n");
}

bool ImmutableFieldGenerator::HasHasbit() const {
  return ::google::protobuf::compiler::java::HasHasbit(descriptor_);
}

bool ImmutableFieldGenerator::IsRealOneof() const {
  return ::google::protobuf::compiler::java::IsRealOneof(descriptor_);
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
