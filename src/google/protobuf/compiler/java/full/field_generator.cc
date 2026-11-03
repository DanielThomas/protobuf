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
    const bool grouped =
        target == ParsingTarget::kMessage && generator.UniformParserOneof();
    if (grouped && field != field->containing_oneof()->field(0)) continue;
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
  }
  return vars;
}

void ImmutableFieldGenerator::GenerateCopyAssignment(
    io::Printer* printer, CopyTarget target) const {
  printer->Print(GetCopyingVariables(target),
                 "$copy_destination$ = $copy_source$;\n");
}

void ImmutableFieldGenerator::GenerateScalarCopyCode(
    io::Printer* printer, CopyTarget target) const {
  if (target == CopyTarget::kMessage) {
    printer->Print(variables_, "if ($get_has_field_bit_from_local$) {\n");
    printer->Indent();
  }
  GenerateCopyAssignment(printer, target);
  if (target == CopyTarget::kMessage && HasHasbit()) {
    printer->Print(absl::StrCat(GenerateSetBitToLocal(bit_index_), ";\n"));
  } else if (target == CopyTarget::kBuilder && !HasHasbit()) {
    printer->Print(absl::StrCat(GenerateSetBitToLocal(bit_index_), ";\n"));
  }
  if (target == CopyTarget::kMessage) {
    printer->Outdent();
    printer->Print("}\n");
  }
}

void ImmutableFieldGenerator::GenerateRepeatedCopyCode(
    io::Printer* printer, CopyTarget target) const {
  if (target == CopyTarget::kMessage) {
    printer->Print(variables_,
                   "if ($get_has_field_bit_from_local$) {\n"
                   "  $name$_.makeImmutable();\n");
    printer->Indent();
  }
  GenerateCopyAssignment(printer, target);
  if (target == CopyTarget::kBuilder) {
    printer->Print(variables_, "if (!$name$_.isEmpty()) {\n");
    printer->Print(absl::StrCat("  ", GenerateSetBitToLocal(bit_index_), ";\n"));
  } else {
    printer->Outdent();
  }
  printer->Print("}\n");
}

void ImmutableFieldGenerator::GenerateOneofCopyCode(
    io::Printer* printer) const {
  printer->Print(variables_, "if (other.$oneof_name$Case_ == $number$) {\n");
  printer->Print(absl::StrCat("  ", GenerateSetBitToLocal(bit_index_), ";\n"));
  printer->Print("}\n");
}

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
    const auto presence_prefix = GetJavaType(descriptor_) == JAVATYPE_MESSAGE
        ? "instance." : "to_";
    vars["set_has_field_bit"] = HasHasbit()
        ? absl::StrCat(presence_prefix, GenerateSetBit(bit_index_), ";") : "";
    vars["unknown_receiver"] = "unknown = ";
    vars["unknown_argument"] = "unknown, ";
    auto add = vars.find("repeated_add");
    if (add != vars.end()) add->second = absl::StrCat("instance.", add->second);
  }
  return vars;
}

std::string ImmutableFieldGenerator::GetParsingEnsureMutableCode(
    ParsingTarget target, absl::string_view capacity) const {
  if (target == ParsingTarget::kMessage &&
      GetJavaType(descriptor_) == JAVATYPE_MESSAGE) {
    return GetEnsureIsMutableBody(target, capacity);
  }
  const bool parsing_message = target == ParsingTarget::kMessage;
  return absl::StrCat("ensure", variables_.at("capitalized_name"),
                      "IsMutable(", parsing_message ? "instance" : "",
                      parsing_message && !capacity.empty() ? ", " : "",
                      capacity, ");");
}

std::string ImmutableFieldGenerator::GetEnsureIsMutableBody(
    ParsingTarget target, absl::string_view capacity) const {
  const std::string field = GetParsingVariables(target).at("field");
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

bool ImmutableFieldGenerator::ParserMayNeedBuilder() const {
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
                   "java.lang.Object parsed_$name$ = null;\n");
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
  if (IsRealOneof()) return !UniformParserOneof();
  return !descriptor_->is_repeated() &&
         GetJavaType(descriptor_) == JAVATYPE_MESSAGE;
}

void ImmutableFieldGenerator::GenerateParserHelperCode(io::Printer* printer) const {
  if (descriptor_->is_repeated() &&
      GetJavaType(descriptor_) != JAVATYPE_MESSAGE) {
    auto vars = GetParsingVariables(ParsingTarget::kMessage);
    vars["message_type"] =
        name_resolver_->GetImmutableClassName(descriptor_->containing_type());
    const bool string = GetJavaType(descriptor_) == JAVATYPE_STRING;
    vars["capacity_parameter"] = string ? "" : ", int minCapacity";
    printer->Print(vars,
        "private static void ensure$capitalized_name$IsMutable($message_type$ instance$capacity_parameter$) {\n");
    printer->Indent();
    printer->Print(absl::StrCat(GetEnsureIsMutableBody(
        ParsingTarget::kMessage, string ? "" : "minCapacity"), "\n"));
    printer->Outdent();
    printer->Print("}\n");
  }
  if (!ParserNeedsHelper()) return;
  auto vars = variables_;
  const bool unknown = GetJavaType(descriptor_) == JAVATYPE_ENUM;
  const bool returns_tag = ParserMayNeedBuilder() && !unknown;
  vars["message_type"] = name_resolver_->GetImmutableClassName(descriptor_->containing_type());
  vars["return_type"] = returns_tag ? "int" : unknown ? "java.lang.Object" : "void";
  vars["unknown_parameter"] = unknown ? ", java.lang.Object unknown" : "";
  vars["builder_parameter"] = returns_tag
      ? ", com.google.protobuf.SingleFieldBuilder<?, ?, ?>[] fieldBuilders" : "";
  if (returns_tag && GetJavaType(descriptor_) == JAVATYPE_MESSAGE) {
    printer->Print("@java.lang.SuppressWarnings(\"unchecked\")\n");
  }
  printer->Print(vars,
      "private static $return_type$ parse$capitalized_name$Field(\n"
      "    $message_type$ instance, com.google.protobuf.CodedInputStream input,\n"
      "    com.google.protobuf.ExtensionRegistryLite extensionRegistry$unknown_parameter$$builder_parameter$)\n"
      "    throws java.io.IOException {\n");
  printer->Indent();
  if (returns_tag) {
    vars["tag"] = absl::StrCat(static_cast<int32_t>(
        internal::WireFormatLite::MakeTag(
            descriptor_->number(),
            internal::WireFormat::WireTypeForFieldType(descriptor_->type()))));
    printer->Print(vars, IsRealOneof()
        ? "if (fieldBuilders == null && instance.$oneof_name$_ instanceof com.google.protobuf.GeneratedMessage) {\n"
        : "if (fieldBuilders == null && instance.$name$_ != null) {\n");
    printer->Print(vars, "  return $tag$;\n}\n");
    if (GetJavaType(descriptor_) == JAVATYPE_MESSAGE) {
      printer->Print("if (fieldBuilders != null) {\n");
      printer->Indent();
      GenerateParserMergingCode(printer);
      printer->Print("return 0;\n");
      printer->Outdent();
      printer->Print("}\n");
    }
  }
  GenerateParsingCode(printer, ParsingTarget::kMessage);
  if (returns_tag) printer->Print("return 0;\n");
  if (unknown) printer->Print("return unknown;\n");
  printer->Outdent();
  printer->Print("}\n");
  if (!descriptor_->is_repeated() &&
      GetJavaType(descriptor_) == JAVATYPE_MESSAGE) {
    GenerateParserMergingHelpers(printer);
  }
}

void ImmutableFieldGenerator::GenerateParserParsingCode(
    io::Printer* printer, bool packed) const {
  // Enum helpers return unknown-field state, so retain the child before calling them.
  const bool unknown = GetJavaType(descriptor_) == JAVATYPE_ENUM;
  if (ParserMayNeedBuilder() && unknown) {
    printer->Print(variables_,
        "if (fieldBuilders == null && instance.$oneof_name$_ instanceof com.google.protobuf.GeneratedMessage) {\n");
    printer->Print("  pendingTag = tag;\n");
    printer->Print("  break;\n");
    printer->Print("}\n");
  }
  if (ParserNeedsHelper()) {
    auto vars = variables_;
    vars["assignment"] = unknown ? "unknown = "
        : ParserMayNeedBuilder() ? "pendingTag = " : "";
    vars["unknown_argument"] = unknown ? ", unknown" : "";
    vars["builder_argument"] = ParserMayNeedBuilder() && !unknown
        ? ", fieldBuilders" : "";
    printer->Print(vars,
        "$assignment$parse$capitalized_name$Field(instance, input, extensionRegistry$unknown_argument$$builder_argument$);\n");
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
  } else if (GetJavaType(descriptor_) == JAVATYPE_STRING) {
    printer->Print(variables_,
                   "if (parsed_$name$ != null) instance.$name$_ = parsed_$name$;\n");
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
