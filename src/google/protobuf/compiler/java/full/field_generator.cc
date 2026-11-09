// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "google/protobuf/compiler/java/full/field_generator.h"

#include <cstdint>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "google/protobuf/compiler/java/context.h"
#include "google/protobuf/compiler/java/helpers.h"
#include "google/protobuf/compiler/java/internal_helpers.h"
#include "google/protobuf/compiler/java/name_resolver.h"
#include "google/protobuf/wire_format.h"

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

std::string ParserFieldBuilderType(const FieldDescriptor* field,
                                   Context* context) {
  const std::string type =
      context->GetNameResolver()->GetImmutableClassName(field->message_type());
  return absl::StrCat(
      "com.google.protobuf.",
      field->is_repeated() ? "RepeatedFieldBuilder<" : "SingleFieldBuilder<",
      type, ", ", type, ".Builder, ", type, "OrBuilder>");
}

bool ParserMayHandoff(const Descriptor* descriptor) {
  return descriptor->extension_range_count() > 0 ||
         !ParserChildFields(descriptor).empty();
}

std::vector<const FieldDescriptor*> BuilderInitializationFields(
    const Descriptor* descriptor) {
  bool needed = false;
  for (int i = 0; i < descriptor->field_count(); ++i) {
    const auto* field = descriptor->field(i);
    needed |= field->message_type() != nullptr && !IsRealOneof(field) &&
              HasHasbit(field);
  }
  std::vector<const FieldDescriptor*> fields;
  if (needed) {
    for (int i = 0; i < descriptor->field_count(); ++i) {
      const auto* field = descriptor->field(i);
      if (GetJavaType(field) == JAVATYPE_MESSAGE && !field->is_map() &&
          !IsRealOneof(field)) {
        fields.push_back(field);
      }
    }
  }
  return fields;
}

bool IsParserLocalMap(const FieldDescriptor* field) {
  return field->is_map() &&
         GetJavaType(field->message_type()->map_value()) == JAVATYPE_MESSAGE;
}

std::vector<const FieldDescriptor*> ParserLocalFields(
    const Descriptor* descriptor) {
  std::vector<const FieldDescriptor*> fields;
  for (int i = 0; i < descriptor->field_count(); ++i) {
    const auto* field = descriptor->field(i);
    if (IsParserLocalMap(field) ||
        (field->is_repeated() && !field->is_map() &&
         GetJavaType(field) == JAVATYPE_MESSAGE) ||
        (!field->is_repeated() && !IsRealOneof(field) &&
         GetJavaType(field) == JAVATYPE_STRING)) {
      fields.push_back(field);
    }
  }
  return fields;
}

std::vector<const FieldDescriptor*> ParserChildFields(const Descriptor* descriptor) {
  std::vector<const FieldDescriptor*> fields;
  for (int i = 0; i < descriptor->field_count(); ++i) {
    const auto* field = descriptor->field(i);
    if (!field->is_repeated() && GetJavaType(field) == JAVATYPE_MESSAGE) {
      fields.push_back(field);
    }
  }
  return fields;
}

void GenerateFieldParsingCases(
    const Descriptor* descriptor,
    const FieldGeneratorMap<ImmutableFieldGenerator>& fields,
    io::Printer* printer, ParsingTarget target) {
  for (const auto* field : SortFieldsByNumber(descriptor)) {
    const auto& generator = fields.get(field);
    for (bool packed : {false, true}) {
      // Accept packed wire input for every packable field, regardless of its declared encoding.
      if (packed && !field->is_packable()) continue;
      const auto wire_type = packed
          ? internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED
          : internal::WireFormat::WireTypeForFieldType(field->type());
      const std::string tag = absl::StrCat(static_cast<int32_t>(
          internal::WireFormatLite::MakeTag(field->number(), wire_type)));
      printer->Print("case $tag$: {\n", "tag", tag);
      printer->Indent();
      if (target == ParsingTarget::kMessage) {
        generator.GenerateParserParsingCode(printer, packed);
      } else if (packed) {
        generator.GenerateBuilderParsingCodeFromPacked(printer);
      } else {
        generator.GenerateBuilderParsingCode(printer);
      }
      printer->Outdent();
      printer->Print("  break;\n} // case $tag$\n", "tag", tag);
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
ImmutableFieldGenerator::GetCopyingVariables(CopyTarget target) const {
  auto vars = variables_;
  const std::string field = absl::StrCat(vars.at("name"), "_");
  vars["copy_source"] = field;
  vars["copy_destination"] = absl::StrCat("result.", field);
  if (target == CopyTarget::kBuilder) {
    vars["copy_source"] = absl::StrCat("other.", field);
    vars["copy_destination"] = field;
    // Reuse public scalar getters rather than adding Java 8 field bridges.
    const JavaType type = GetJavaType(descriptor_);
    if (!descriptor_->is_repeated() && !IsRealOneof() &&
        type != JAVATYPE_MESSAGE && type != JAVATYPE_STRING &&
        type != JAVATYPE_ENUM) {
      vars["copy_source"] =
          absl::StrCat("other.get", vars.at("capitalized_name"), "()");
    } else if (!descriptor_->is_repeated() && type == JAVATYPE_ENUM &&
               SupportUnknownEnumValue(descriptor_)) {
      vars["copy_source"] =
          absl::StrCat("other.get", vars.at("capitalized_name"), "Value()");
    }
  } else if (target == CopyTarget::kParserInPlace) {
    vars["copy_source"] =
        GetParsingVariables(ParsingTarget::kMessage).at("field");
    vars["copy_destination"] = absl::StrCat("instance.", field);
    vars["get_has_field_bit_from_local"] = absl::StrReplaceAll(
        GenerateGetBit(bit_index_),
        {{GetBitFieldNameForBit(bit_index_),
          absl::StrCat("instance.", GetBitFieldNameForBit(bit_index_))}});
  }
  return vars;
}

void ImmutableFieldGenerator::GenerateCopyAssignment(
    io::Printer* printer, CopyTarget target) const {
  const auto vars = GetCopyingVariables(target);
  if (vars.at("copy_source") != vars.at("copy_destination")) {
    printer->Print(vars, "$copy_destination$ = $copy_source$;\n");
  }
}

void ImmutableFieldGenerator::GenerateScalarCopyCode(
    io::Printer* printer, CopyTarget target) const {
  const auto vars = GetCopyingVariables(target);
  if (vars.at("copy_source") == vars.at("copy_destination")) return;
  if (target != CopyTarget::kBuilder) {
    printer->Print(vars, "if ($get_has_field_bit_from_local$) {\n");
    printer->Indent();
  }
  GenerateCopyAssignment(printer, target);
  if (target != CopyTarget::kBuilder) {
    printer->Outdent();
    printer->Print("}\n");
  }
}

void ImmutableFieldGenerator::GenerateRepeatedCopyCode(
    io::Printer* printer, CopyTarget target) const {
  if (target != CopyTarget::kBuilder) {
    printer->Print(GetCopyingVariables(target),
                   "if ($get_has_field_bit_from_local$) {\n"
                   "  $copy_source$.makeImmutable();\n");
    printer->Indent();
  }
  GenerateCopyAssignment(printer, target);
  if (target != CopyTarget::kBuilder) {
    printer->Outdent();
    printer->Print("}\n");
  }
}

ImmutableFieldGenerator::FieldVariables
ImmutableFieldGenerator::GetParsingVariables(ParsingTarget target) const {
  auto vars = variables_;
  vars["field"] = absl::StrCat(
      IsRealOneof() ? vars.at("oneof_name") : vars.at("name"), "_");
  vars["unknown_receiver"] = "";
  vars["unknown_argument"] = "";
  if (IsRealOneof()) {
    vars["oneof_case"] = absl::StrCat(vars.at("oneof_name"), "Case_");
    vars["clear_oneof_has_bits"] =
        absl::StrCat("clear", vars.at("oneof_capitalized_name"), "HasBits();");
  }
  if (target == ParsingTarget::kMessage) {
    vars["field"] = absl::StrCat("instance.", vars.at("field"));
    if (IsParserLocalMap(descriptor_) ||
        (!descriptor_->is_repeated() && !IsRealOneof() &&
         GetJavaType(descriptor_) == JAVATYPE_STRING)) {
      vars["field"] = absl::StrCat("parsed_", vars.at("name"));
    }
    const bool local_presence = !descriptor_->is_repeated() &&
                                GetJavaType(descriptor_) != JAVATYPE_MESSAGE;
    const auto presence_prefix = local_presence ? "to_" : "instance.";
    vars["set_has_field_bit"] =
        absl::StrCat(IsRealOneof() ? "instance." : presence_prefix,
                     GenerateSetBit(bit_index_), ";");
    if (IsRealOneof()) {
      vars["oneof_case"] = absl::StrCat("instance.", vars.at("oneof_case"));
      vars["set_oneof_case_message"] = absl::StrCat(
          "instance.", vars.at("oneof_name"), "Case_ = ", vars.at("number"));
      vars["clear_oneof_has_bits"] = absl::StrCat(
          "clear", vars.at("oneof_capitalized_name"), "HasBits(instance);");
    }
    vars["on_changed"] = "";
    vars["unknown_receiver"] = "unknown = ";
    vars["unknown_argument"] = "unknown, ";
    auto add = vars.find("repeated_add");
    if (add != vars.end()) add->second = absl::StrCat("instance.", add->second);
  }
  return vars;
}

std::string ImmutableFieldGenerator::GetParsingEnsureMutableCode(
    ParsingTarget target, absl::string_view capacity) const {
  const bool parsing_message = target == ParsingTarget::kMessage;
  return absl::StrCat("ensure", variables_.at("capitalized_name"),
                      "IsMutable(", parsing_message ? "instance" : "",
                      parsing_message && !capacity.empty() ? ", " : "",
                      capacity, ");");
}

void ImmutableFieldGenerator::GenerateEnsureIsMutableBody(
    io::Printer* printer, ParsingTarget target,
    absl::string_view capacity) const {
  const auto vars = GetParsingVariables(target);
  const std::string field = vars.at("field");
  std::string copy;
  if (GetJavaType(descriptor_) == JAVATYPE_STRING) {
    copy = absl::StrCat("new com.google.protobuf.LazyStringArrayList(", field, ")");
  } else if (capacity.empty() || capacity == "/* capacity= */ -1") {
    copy = absl::StrCat("makeMutableCopy(", field, ")");
  } else {
    copy = absl::StrCat("makeMutableCopy(", field, ", ", capacity, ")");
  }
  printer->Print(absl::StrCat("if (!", field, ".isModifiable()) {\n  ", field,
                              " = ", copy, ";\n}\n"));
  printer->Print(vars, "$set_has_field_bit$\n");
}

void ImmutableFieldGenerator::GenerateOneofParsingCaseUpdate(
    io::Printer* printer, ParsingTarget target) const {
  printer->Print(GetParsingVariables(target),
                 "switch ($oneof_case$) {\n"
                 "default:\n"
                 "  $clear_oneof_has_bits$ // fallthrough\n"
                 "case 0:\n"
                 "  $set_oneof_case_message$;\n"
                 "  $set_has_field_bit$ // fallthrough\n"
                 "case $number$:\n"
                 "  break;\n"
                 "}\n");
}

void ImmutableFieldGenerator::GenerateParserLocalTransferCode(
    io::Printer* printer, int index) const {
  auto vars = variables_;
  vars["index"] = absl::StrCat(index);
  printer->Print(vars,
                 "if (parserLocals[$index$] != null) {\n"
                 "  $name$_ = parserLocals[$index$];\n"
                 "}\n");
}

void ImmutableFieldGenerator::GenerateParserLocalCode(io::Printer* printer) const {
  if (!descriptor_->is_repeated() && !IsRealOneof() &&
      GetJavaType(descriptor_) == JAVATYPE_STRING) {
    printer->Print(variables_,
                   "java.lang.Object parsed_$name$ = null;\n");
  }
}

bool ImmutableFieldGenerator::ParserNeedsHelper() const {
  if (IsRealOneof()) return true;
  return !descriptor_->is_repeated() &&
         GetJavaType(descriptor_) == JAVATYPE_MESSAGE;
}

void ImmutableFieldGenerator::GenerateParserHelperCode(io::Printer* printer) const {
  if (descriptor_->is_repeated() && !descriptor_->is_map()) {
    auto vars = GetParsingVariables(ParsingTarget::kMessage);
    vars["message_type"] =
        name_resolver_->GetImmutableClassName(descriptor_->containing_type());
    const bool capacity = GetJavaType(descriptor_) != JAVATYPE_STRING &&
        GetJavaType(descriptor_) != JAVATYPE_MESSAGE;
    vars["capacity_parameter"] = capacity ? ", int minCapacity" : "";
    printer->Print(vars,
        "private static void ensure$capitalized_name$IsMutable($message_type$ instance$capacity_parameter$) {\n");
    printer->Indent();
    GenerateEnsureIsMutableBody(printer, ParsingTarget::kMessage,
                                capacity ? "minCapacity" : "");
    printer->Outdent();
    printer->Print("}\n");
  }
  if (!ParserNeedsHelper()) {
    if (descriptor_->is_repeated() &&
        GetJavaType(descriptor_) == JAVATYPE_MESSAGE &&
        !BuilderInitializationFields(descriptor_->containing_type()).empty()) {
      GenerateParserFieldBuilderHelper(printer);
    }
    return;
  }
  auto vars = variables_;
  const bool unknown = GetJavaType(descriptor_) == JAVATYPE_ENUM;
  const bool child = GetJavaType(descriptor_) == JAVATYPE_MESSAGE;
  vars["message_type"] = name_resolver_->GetImmutableClassName(descriptor_->containing_type());
  vars["return_type"] =
      unknown ? "com.google.protobuf.UnknownFieldSet.Builder" : "void";
  vars["unknown_parameter"] = unknown
      ? ", com.google.protobuf.UnknownFieldSet.Builder unknown" : "";
  vars["builder_parameter"] =
      child ? absl::StrCat(", ", ParserFieldBuilderType(descriptor_, context_),
                           " ", vars.at("name"), "Builder_")
            : "";
  printer->Print(vars,
      "private static $return_type$ parse$capitalized_name$Field(\n"
      "    $message_type$ instance, com.google.protobuf.CodedInputStream input,\n"
      "    com.google.protobuf.ExtensionRegistryLite extensionRegistry$unknown_parameter$$builder_parameter$)\n"
      "    throws java.io.IOException {\n");
  printer->Indent();
  GenerateParsingCode(printer, ParsingTarget::kMessage);
  if (unknown) printer->Print("return unknown;\n");
  printer->Outdent();
  printer->Print("}\n");
  if (child) GenerateParserFieldBuilderHelper(printer);
}

void ImmutableFieldGenerator::GenerateParserFieldBuilderInitializationCode(
    io::Printer* printer) const {
  printer->Print(
      variables_,
      "$name$Builder_ = internalGet$capitalized_name$FieldBuilder(instance, "
      "$name$Builder_);\n");
}

void ImmutableFieldGenerator::GenerateParserParsingCode(
    io::Printer* printer, bool packed) const {
  if (ParserNeedsHelper()) {
    auto vars = variables_;
    const bool unknown = GetJavaType(descriptor_) == JAVATYPE_ENUM;
    const bool child = GetJavaType(descriptor_) == JAVATYPE_MESSAGE;
    if (child) {
      printer->Print(
          vars,
          IsRealOneof()
              ? "if ($name$Builder_ != null) {\n"
              : "if (instance.$name$_ != null || $name$Builder_ != null) {\n");
      printer->Print(
          "  pendingTag = tag;\n"
          "  break parseLoop;\n"
          "}\n");
      if (IsRealOneof()) {
        GenerateParserFieldBuilderInitializationCode(printer);
      }
    }
    vars["assignment"] = unknown ? "unknown = " : "";
    vars["unknown_argument"] = unknown ? ", unknown" : "";
    vars["builder_argument"] =
        child ? absl::StrCat(", ", vars.at("name"), "Builder_") : "";
    printer->Print(vars,
        "$assignment$parse$capitalized_name$Field(instance, input, extensionRegistry$unknown_argument$$builder_argument$);\n");
  } else if (packed) {
    GenerateParsingCodeFromPacked(printer, ParsingTarget::kMessage);
  } else {
    GenerateParsingCode(printer, ParsingTarget::kMessage);
  }
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
