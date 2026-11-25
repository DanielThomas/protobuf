// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef GOOGLE_PROTOBUF_COMPILER_JAVA_IMMUTABLE_FIELD_GENERATOR_H__
#define GOOGLE_PROTOBUF_COMPILER_JAVA_IMMUTABLE_FIELD_GENERATOR_H__

#include <string>

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
  virtual void GenerateInterfaceMembers(io::Printer* printer) const = 0;
  virtual void GenerateMembers(io::Printer* printer) const = 0;
  virtual void GenerateBuilderMembers(io::Printer* printer) const = 0;
  virtual void GenerateInitializationCode(io::Printer* printer) const = 0;
  virtual void GenerateConstructorInitializationCode(io::Printer* printer) const {}
  virtual void GenerateBuilderInitializationCode(io::Printer* printer) const {}
  virtual void GenerateBuilderClearCode(io::Printer* printer) const = 0;
  virtual void GenerateMergingCode(io::Printer* printer) const = 0;
  virtual void GenerateBuildingCode(io::Printer* printer,
                                    bool static_parser) const = 0;
  virtual void GenerateSerializedSizeCode(io::Printer* printer) const = 0;
  virtual void GenerateFieldBuilderInitializationCode(
      io::Printer* printer) const = 0;

  // The static parser (see MessageBuilderGenerator::GenerateStaticParser)
  // holds the Builder's state for a field in locals of the same name, and
  // parses with the Builder's parsing code. Fields the Builder parses through
  // its own methods are parsed into the message, `result`, through static
  // twins of those methods, generated with the same code.
  virtual void GenerateBuilderParsingCode(io::Printer* printer,
                                          bool static_parser) const = 0;
  virtual void GenerateBuilderParsingCodeFromPacked(io::Printer* printer,
                                                    bool static_parser) const {
    ReportUnexpectedPackedFieldsCall();
  }
  virtual void GenerateStaticParsingMethods(io::Printer* printer) const {}

  virtual void GenerateEqualsCode(io::Printer* printer) const = 0;
  virtual void GenerateHashCode(io::Printer* printer) const = 0;

  virtual std::string GetBoxedType() const = 0;

 protected:
  // Variables for parsing and building code shared by the Builder and the
  // static parser, whose twins of the Builder's methods take the message as
  // `result`.
  absl::flat_hash_map<absl::string_view, std::string> ParsingVariables(
      bool static_parser) const;
  // For a oneof alternative's parse method, whose static twin returns the tag
  // of a later assignment to the oneof, before its payload, for the Builder to
  // continue from.
  void PrintOneofParseGuard(io::Printer* printer, bool static_parser) const;
  // Sets the oneof's case to this alternative, as the Builder does through a
  // switch that clears the presence bits of the others, and the static twin,
  // which only parses a first assignment, does directly.
  void PrintSetOneofCase(io::Printer* printer, bool static_parser) const;

  const FieldDescriptor* descriptor_;
  int bit_index_;
  Context* context_;
  ClassNameResolver* name_resolver_;
  absl::flat_hash_map<absl::string_view, std::string> variables_;
};

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_COMPILER_JAVA_IMMUTABLE_FIELD_GENERATOR_H__
