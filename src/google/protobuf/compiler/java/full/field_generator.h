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
enum class CopyTarget { kBuilder, kMessage };

class ImmutableFieldGenerator : public FieldGenerator {
 public:
  explicit ImmutableFieldGenerator(const FieldDescriptor* descriptor,
                                   int bit_index, Context* context);
  ImmutableFieldGenerator(const ImmutableFieldGenerator&) = delete;
  ImmutableFieldGenerator& operator=(const ImmutableFieldGenerator&) = delete;
  ~ImmutableFieldGenerator() override = default;

  void GenerateParserLocalCode(io::Printer* printer) const;
  void GenerateParserParsingCode(io::Printer* printer, bool packed = false) const;
  virtual void GenerateParserHelperCode(io::Printer* printer) const;
  virtual void GenerateParserMergingHelpers(io::Printer* printer) const {}
  virtual void GenerateParserMergingCode(io::Printer* printer) const {}
  virtual void GenerateParserChildFinalizationCode(io::Printer* printer) const {}
  virtual void GenerateParserChildTransferCode(io::Printer* printer) const {}
  virtual void GenerateParserOneofFinalizationCode(io::Printer* printer) const {}
  bool UniformParserOneof() const;
  void GenerateParserFinalizationCode(io::Printer* printer) const;

  bool HasHasbit() const;
  bool IsRealOneof() const;

  int GetBitIndex() const { return bit_index_; }
  constexpr int GetNumBits() const { return 1; }
  virtual void GenerateInterfaceMembers(io::Printer* printer) const = 0;
  virtual void GenerateMembers(io::Printer* printer) const = 0;
  virtual void GenerateBuilderMembers(io::Printer* printer) const = 0;
  virtual void GenerateInitializationCode(io::Printer* printer) const = 0;
  virtual void GenerateConstructorInitializationCode(io::Printer* printer) const {}
  virtual void GenerateBuilderInitializationCode(io::Printer* printer) const {
    GenerateInitializationCode(printer);
  }
  virtual void GenerateBuilderClearCode(io::Printer* printer) const = 0;
  virtual void GenerateMergingCode(io::Printer* printer) const = 0;
  virtual void GenerateBuildingCode(
      io::Printer* printer, CopyTarget target = CopyTarget::kMessage) const = 0;
  void GenerateBuilderParsingCode(io::Printer* printer) const {
    GenerateParsingCode(printer, ParsingTarget::kBuilder);
  }
  virtual void GenerateParsingCode(io::Printer* printer,
                                   ParsingTarget target) const = 0;
  virtual void GenerateSerializedSizeCode(io::Printer* printer) const = 0;
  virtual void GenerateFieldBuilderInitializationCode(
      io::Printer* printer) const = 0;

  void GenerateBuilderParsingCodeFromPacked(io::Printer* printer) const {
    GenerateParsingCodeFromPacked(printer, ParsingTarget::kBuilder);
  }
  virtual void GenerateParsingCodeFromPacked(io::Printer* printer,
                                             ParsingTarget target) const {
    ReportUnexpectedPackedFieldsCall();
  }

  virtual void GenerateEqualsCode(io::Printer* printer) const = 0;
  virtual void GenerateHashCode(io::Printer* printer) const = 0;

  virtual std::string GetBoxedType() const = 0;

 protected:
  using FieldVariables = absl::flat_hash_map<absl::string_view, std::string>;

  FieldVariables GetCopyingVariables(CopyTarget target) const;
  void GenerateCopyAssignment(io::Printer* printer, CopyTarget target) const;
  void GenerateScalarCopyCode(io::Printer* printer, CopyTarget target) const;
  void GenerateRepeatedCopyCode(io::Printer* printer, CopyTarget target) const;
  void GenerateOneofCopyCode(io::Printer* printer) const;
  FieldVariables GetParsingVariables(ParsingTarget target) const;
  std::string GetParsingEnsureMutableCode(
      ParsingTarget target, absl::string_view capacity = "") const;
  std::string GetEnsureIsMutableBody(
      ParsingTarget target, absl::string_view capacity = "") const;
  void GenerateOneofParsingCaseUpdate(io::Printer* printer,
                                      ParsingTarget target) const;
  bool ParserMayNeedBuilder() const;
  bool ParserNeedsHelper() const;

  const FieldDescriptor* descriptor_;
  int bit_index_;
  Context* context_;
  ClassNameResolver* name_resolver_;
  absl::flat_hash_map<absl::string_view, std::string> variables_;
};

void GenerateFieldParsingCases(
    const Descriptor* descriptor,
    const FieldGeneratorMap<ImmutableFieldGenerator>& fields,
    io::Printer* printer, ParsingTarget target);

std::vector<const FieldDescriptor*> ParserChildFields(
    const Descriptor* descriptor);

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_COMPILER_JAVA_IMMUTABLE_FIELD_GENERATOR_H__
