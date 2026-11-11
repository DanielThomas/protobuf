// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef GOOGLE_PROTOBUF_COMPILER_JAVA_IMMUTABLE_FIELD_GENERATOR_H__
#define GOOGLE_PROTOBUF_COMPILER_JAVA_IMMUTABLE_FIELD_GENERATOR_H__

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/compiler/java/generator_common.h"
#include "google/protobuf/io/printer.h"

namespace google {
namespace protobuf {
namespace compiler {
namespace java {
class Context;            // context.h
class ClassNameResolver;  // name_resolver.h

enum class ParsingTarget { kBuilder, kMessage };
enum class CopyTarget { kMessage, kParserInPlace };

class ImmutableFieldGenerator : public FieldGenerator {
 public:
  explicit ImmutableFieldGenerator(const FieldDescriptor* descriptor,
                                   int bit_index, Context* context);
  ImmutableFieldGenerator(const ImmutableFieldGenerator&) = delete;
  ImmutableFieldGenerator& operator=(const ImmutableFieldGenerator&) = delete;
  ~ImmutableFieldGenerator() override = default;

  bool HasHasbit() const;
  bool IsRealOneof() const;

  int GetBitIndex() const { return bit_index_; }
  constexpr int GetNumBits() const { return 1; }
  std::string ParserLocalType() const;
  std::string ParserLocalName() const;
  void GenerateParserLocalTransferCode(io::Printer* printer, int index) const;
  void GenerateBuilderStateCode(io::Printer* printer) const;

  virtual void GenerateInterfaceMembers(io::Printer* printer) const = 0;
  virtual void GenerateMembers(io::Printer* printer) const = 0;
  virtual void GenerateBuilderMembers(
      io::Printer* printer,
      ParsingTarget target = ParsingTarget::kBuilder) const = 0;
  virtual void GenerateInitializationCode(io::Printer* printer) const = 0;
  virtual void GenerateConstructorInitializationCode(io::Printer* printer) const {}
  virtual void GenerateBuilderInitializationCode(io::Printer* printer) const {
    GenerateInitializationCode(printer);
  }
  virtual void GenerateBuilderClearCode(io::Printer* printer) const = 0;
  virtual void GenerateMergingCode(io::Printer* printer) const = 0;
  virtual void GenerateBuildingCode(
      io::Printer* printer, CopyTarget target = CopyTarget::kMessage) const = 0;
  virtual void GenerateBuilderParsingCode(
      io::Printer* printer,
      ParsingTarget target = ParsingTarget::kBuilder) const = 0;
  virtual void GenerateSerializedSizeCode(io::Printer* printer) const = 0;
  virtual void GenerateFieldBuilderInitializationCode(
      io::Printer* printer,
      ParsingTarget target = ParsingTarget::kBuilder) const = 0;

  virtual void GenerateBuilderParsingCodeFromPacked(
      io::Printer* printer,
      ParsingTarget target = ParsingTarget::kBuilder) const {
    ReportUnexpectedPackedFieldsCall();
  }

  virtual void GenerateEqualsCode(io::Printer* printer) const = 0;
  virtual void GenerateHashCode(io::Printer* printer) const = 0;

  virtual std::string GetBoxedType() const = 0;

 protected:
  using FieldVariables = absl::flat_hash_map<absl::string_view, std::string>;
  FieldVariables GetParsingVariables(ParsingTarget target) const;
  FieldVariables GetBuildingVariables(CopyTarget target) const;

  const FieldDescriptor* descriptor_;
  int bit_index_;
  Context* context_;
  ClassNameResolver* name_resolver_;
  FieldVariables variables_;
};

bool ParserMayHandoff(const Descriptor* descriptor);
std::vector<const FieldDescriptor*> BuilderInitializationFields(
    const Descriptor* descriptor);
bool IsParserLocalMap(const FieldDescriptor* field);
std::vector<const FieldDescriptor*> ParserLocalFields(
    const Descriptor* descriptor);
std::vector<const FieldDescriptor*> ParserChildFields(
    const Descriptor* descriptor);

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_COMPILER_JAVA_IMMUTABLE_FIELD_GENERATOR_H__
