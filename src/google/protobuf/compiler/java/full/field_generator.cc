// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "google/protobuf/compiler/java/full/field_generator.h"

#include "absl/strings/str_cat.h"
#include "google/protobuf/compiler/java/context.h"
#include "google/protobuf/compiler/java/helpers.h"
#include "google/protobuf/compiler/java/name_resolver.h"

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

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
    if (GetJavaType(field) == JAVATYPE_MESSAGE &&
        (!field->is_map() || IsParserLocalMap(field))) {
      fields.push_back(field);
    }
  }
  return fields;
}

std::vector<const FieldDescriptor*> ParserChildFields(
    const Descriptor* descriptor) {
  std::vector<const FieldDescriptor*> fields;
  for (int i = 0; i < descriptor->field_count(); ++i) {
    const auto* field = descriptor->field(i);
    if (!field->is_repeated() && GetJavaType(field) == JAVATYPE_MESSAGE) {
      fields.push_back(field);
    }
  }
  return fields;
}

ImmutableFieldGenerator::ImmutableFieldGenerator(
    const FieldDescriptor* descriptor, int bit_index, Context* context)
    : descriptor_(descriptor),
      bit_index_(bit_index),
      context_(context),
      name_resolver_(context->GetNameResolver()) {}

bool ImmutableFieldGenerator::HasHasbit() const {
  return ::google::protobuf::compiler::java::HasHasbit(descriptor_);
}

bool ImmutableFieldGenerator::IsRealOneof() const {
  return ::google::protobuf::compiler::java::IsRealOneof(descriptor_);
}

ImmutableFieldGenerator::FieldVariables
ImmutableFieldGenerator::GetParsingVariables(ParsingTarget target) const {
  auto vars = variables_;
  const bool message = target == ParsingTarget::kMessage;
  const std::string name = vars.at("name");
  const std::string slot =
      absl::StrCat(IsRealOneof() ? vars.at("oneof_name") : name, "_");
  const std::string message_type =
      name_resolver_->GetImmutableClassName(descriptor_->containing_type());
  vars["field"] = absl::StrCat(message ? "instance." : "", slot);
  vars["result_field"] = absl::StrCat(message ? "instance." : "result.", slot);
  vars["static"] = message ? "static " : "";
  vars["instance_value"] = message ? "instance" : "";
  vars["instance_argument"] = message ? "instance, " : "";
  vars["instance_value_parameter"] =
      message ? absl::StrCat(message_type, " instance") : "";
  vars["instance_parameter"] =
      message ? absl::StrCat(message_type, " instance, ") : "";
  vars["runtime"] =
      message ? "com.google.protobuf.GeneratedMessageParser." : "";
  vars["packed_reader_receiver"] =
      message ? "com.google.protobuf.GeneratedMessageParser" : "super";
  vars["unknown_receiver"] = message ? "unknown = " : "";
  vars["unknown_argument"] = message ? "unknown, " : "";
  const bool unknown = message && GetJavaType(descriptor_) == JAVATYPE_ENUM &&
                       !SupportUnknownEnumValue(descriptor_);
  vars["parse_return"] =
      unknown ? "com.google.protobuf.UnknownFieldSet.Builder" : "void";
  vars["parse_unknown_parameter"] =
      unknown ? ", com.google.protobuf.UnknownFieldSet.Builder unknown" : "";
  vars["parse_unknown_argument"] = unknown ? ", unknown" : "";
  vars["parse_unknown_receiver"] = unknown ? "unknown = " : "";
  vars["field_get"] = absl::StrCat(message ? "instance." : "", "get",
                                   vars.at("capitalized_name"), "()");
  vars["field_parent"] = message ? "null" : "getParentForChildren()";
  vars["field_clean"] = message ? "false" : "isClean()";
  vars["field_builder_get"] =
      message ? absl::StrCat(name, "Builder_")
              : absl::StrCat("internalGet", vars.at("capitalized_name"),
                             "FieldBuilder()");
  vars["field_builder_parameter"] = "";
  vars["field_builder_parameters"] = "";
  vars["field_builder_argument"] = "";
  if (message && GetJavaType(descriptor_) == JAVATYPE_MESSAGE &&
      !descriptor_->is_map()) {
    const std::string local =
        absl::StrCat(ParserLocalType(), " ", ParserLocalName());
    vars["field_builder_parameter"] = absl::StrCat(",\n    ", local);
    vars["field_builder_parameters"] =
        absl::StrCat(message_type, " instance, ", local);
    vars["field_builder_argument"] = absl::StrCat(", ", ParserLocalName());
  }
  if (message) {
    vars["set_has_field_bit"] =
        absl::StrCat("instance.", GenerateSetBit(bit_index_), ";");
    vars["on_changed"] = "";
    for (const auto* key : {"repeated_add", "name_make_immutable"}) {
      auto found = vars.find(key);
      if (found != vars.end())
        found->second = absl::StrCat("instance.", found->second);
    }
  }
  if (IsRealOneof()) {
    vars["oneof_case"] = absl::StrCat(message ? "instance." : "",
                                      vars.at("oneof_name"), "Case_");
    vars["set_oneof_case_message"] =
        absl::StrCat(vars.at("oneof_case"), " = ", vars.at("number"));
    if (message) {
      vars["has_oneof_case_message"] =
          absl::StrCat(vars.at("oneof_case"), " == ", vars.at("number"));
    }
  }
  if (descriptor_->is_map()) {
    if (message && IsParserLocalMap(descriptor_))
      vars["field"] = ParserLocalName();
    vars["map_parameters"] =
        message ? absl::StrCat(message_type, " instance",
                               IsParserLocalMap(descriptor_)
                                   ? absl::StrCat(", ", ParserLocalType(), " ",
                                                  ParserLocalName())
                                   : "")
                : "";
    vars["converter_prefix"] = message ? "Builder." : "";
    std::string accessor =
        absl::StrCat("internalGetMutable", vars.at("capitalized_name"), "(",
                     message ? "instance" : "",
                     message && IsParserLocalMap(descriptor_)
                         ? absl::StrCat(", ", ParserLocalName())
                         : "",
                     ")");
    if (message && IsParserLocalMap(descriptor_)) {
      accessor = absl::StrCat("(", ParserLocalName(), " = ", accessor, ")");
    }
    vars["mutable_map"] = accessor;
  }
  return vars;
}

ImmutableFieldGenerator::FieldVariables
ImmutableFieldGenerator::GetBuildingVariables(CopyTarget target) const {
  auto vars = GetParsingVariables(target == CopyTarget::kParserInPlace
                                      ? ParsingTarget::kMessage
                                      : ParsingTarget::kBuilder);
  vars["map_get"] =
      target == CopyTarget::kParserInPlace
          ? vars.at("field")
          : absl::StrCat("internalGet", vars.at("capitalized_name"), "()");
  return vars;
}

void ImmutableFieldGenerator::GenerateBuilderStateCode(
    io::Printer* printer) const {
  if (IsRealOneof()) return;
  auto vars = variables_;
  if (IsParserLocalMap(descriptor_)) {
    printer->Print(
        vars,
        "if (parserLocals == null && other.$name$_ != null) {\n"
        "  $name$_ = new "
        "com.google.protobuf.MapFieldBuilder<>($name$Converter);\n"
        "  $name$_.ensureBuilderMap().putAll(other.$name$_.getMap());\n"
        "}\n");
    return;
  }
  vars["state_source"] = absl::StrCat("other.", vars.at("name"), "_");
  // Reuse scalar getters rather than adding Java 8 private-access bridges.
  const auto type = GetJavaType(descriptor_);
  if (!descriptor_->is_repeated() && type != JAVATYPE_MESSAGE &&
      type != JAVATYPE_STRING && type != JAVATYPE_ENUM) {
    vars["state_source"] =
        absl::StrCat("other.get", vars.at("capitalized_name"), "()");
  } else if (!descriptor_->is_repeated() && type == JAVATYPE_ENUM &&
             SupportUnknownEnumValue(descriptor_)) {
    vars["state_source"] =
        absl::StrCat("other.get", vars.at("capitalized_name"), "Value()");
  }
  printer->Print(vars, "$name$_ = $state_source$;\n");
}

std::string ImmutableFieldGenerator::ParserLocalType() const {
  if (descriptor_->is_map()) {
    return absl::StrCat("com.google.protobuf.MapFieldBuilder<",
                        variables_.at("builder_type_parameters"), ">");
  }
  const auto& type = variables_.at("type");
  return absl::StrCat("com.google.protobuf.",
                      descriptor_->is_repeated() ? "RepeatedFieldBuilder<"
                                                 : "SingleFieldBuilder<",
                      type, ", ", type, ".Builder, ", type, "OrBuilder>");
}

std::string ImmutableFieldGenerator::ParserLocalName() const {
  return descriptor_->is_map()
             ? absl::StrCat("parsed_", variables_.at("name"))
             : absl::StrCat(variables_.at("name"), "Builder_");
}

void ImmutableFieldGenerator::GenerateParserLocalTransferCode(
    io::Printer* printer, int index) const {
  auto vars = variables_;
  vars["index"] = absl::StrCat(index);
  vars["local_type"] = ParserLocalType();
  vars["destination"] = descriptor_->is_map()
                            ? absl::StrCat(vars.at("name"), "_")
                            : ParserLocalName();
  vars["adopt"] = descriptor_->is_map() ? "" : "adoptFieldBuilder(";
  vars["close_adopt"] = descriptor_->is_map() ? "" : ")";
  printer->Print(vars,
                 "if (parserLocals[$index$] != null) {\n"
                 "  $destination$ = $adopt$($local_type$) "
                 "parserLocals[$index$]$close_adopt$;\n");
  if (!descriptor_->is_map()) {
    if (IsRealOneof()) {
      printer->Print(
          vars, "  if ($oneof_name$Case_ == $number$) $oneof_name$_ = null;\n");
    } else {
      printer->Print(vars, "  $name$_ = null;\n");
    }
  }
  printer->Print("}\n");
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
