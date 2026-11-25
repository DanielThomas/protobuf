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

absl::flat_hash_map<absl::string_view, std::string>
ImmutableFieldGenerator::ParsingVariables(bool static_parser) const {
  if (!static_parser) {
    return {{"static", ""},
            {"target", ""},
            {"result_parameter", ""},
            {"result_argument", ""},
            {"only_result_parameter", ""},
            {"only_result_argument", ""},
            {"pending_tag", ""},
            {"pending_returns", "void"},
            {"unknown_fields", ""},
            {"unknown_fields_argument", ""},
            {"ensure_returns", "void"},
            {"bit_field_parameter", ""},
            {"bit_field_argument", ""},
            {"bit_field_assignment", ""},
            {"return_bit_field", ""}};
  }
  const std::string& classname = variables_.at("classname");
  std::string bit_field = GetBitFieldNameForBit(bit_index_);
  return {{"static", "static "},
          {"target", "result."},
          {"result_parameter", absl::StrCat(classname, " result, ")},
          {"result_argument", "result, "},
          {"only_result_parameter", absl::StrCat(classname, " result")},
          {"only_result_argument", "result"},
          {"pending_tag", "pendingTag = "},
          {"pending_returns", "int"},
          {"unknown_fields", "unknownFields = "},
          {"unknown_fields_argument", "unknownFields, "},
          // The Builder's ensure$capitalized_name$IsMutable() sets the field's
          // bit, which the twin sets in the static parser's local of the same
          // name, passed and returned.
          {"ensure_returns", "int"},
          {"bit_field_parameter", absl::StrCat("int ", bit_field, ", ")},
          {"bit_field_argument", absl::StrCat(bit_field, ", ")},
          {"bit_field_assignment", absl::StrCat(bit_field, " = ")},
          {"return_bit_field", absl::StrCat("  return ", bit_field, ";\n")}};
}

void ImmutableFieldGenerator::PrintOneofParseGuard(io::Printer* printer,
                                                   bool static_parser) const {
  if (!static_parser) return;
  printer->Print(variables_,
                 "if (result.get$oneof_capitalized_name$Case().getNumber() "
                 "!= 0) {\n"
                 "  return $tag$;\n"
                 "}\n");
}

void ImmutableFieldGenerator::PrintSetOneofCase(io::Printer* printer,
                                                bool static_parser) const {
  if (static_parser) {
    printer->Print(variables_,
                   "$target$$set_oneof_case_message$;\n"
                   "$target$$set_has_field_bit$\n");
    return;
  }
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
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
