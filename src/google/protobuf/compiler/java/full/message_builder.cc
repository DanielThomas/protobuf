// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

// Author: dweis@google.com (Daniel Weis)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include "google/protobuf/compiler/java/full/message_builder.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "google/protobuf/compiler/code_generator_lite.h"
#include "google/protobuf/compiler/java/context.h"
#include "google/protobuf/compiler/java/doc_comment.h"
#include "google/protobuf/compiler/java/field_common.h"
#include "google/protobuf/compiler/java/generator_common.h"
#include "google/protobuf/compiler/java/helpers.h"
#include "google/protobuf/compiler/java/full/enum.h"
#include "google/protobuf/compiler/java/full/extension.h"
#include "google/protobuf/compiler/java/full/field_generator.h"
#include "google/protobuf/compiler/java/full/make_field_gens.h"
#include "google/protobuf/compiler/java/full/oneof_generator.h"
#include "google/protobuf/compiler/java/name_resolver.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/wire_format.h"
#include "google/protobuf/wire_format_lite.h"

// Must be last.
#include "google/protobuf/port_def.inc"

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

using internal::WireFormat;
using internal::WireFormatLite;

namespace {

// The maximum number of merging code snippets generated per method.
// If a message has more fields/oneofs than this threshold, the mergeFrom
// method is split into smaller subfunctions to ensure that Java can compile
// and optimize them.
constexpr int kMergeMethodSplitThreshold = 32;

// Represents a contiguous chunk of field/oneof merging code snippets
// that will be generated together in a single subfunction.
struct MergeCodeChunk {
  int start_index;
  int size;
  std::string method_suffix;
};

std::string MapValueImmutableClassdName(const Descriptor* descriptor,
                                        ClassNameResolver* name_resolver) {
  const FieldDescriptor* value_field = descriptor->map_value();
  ABSL_CHECK_EQ(FieldDescriptor::TYPE_MESSAGE, value_field->type());
  return name_resolver->GetImmutableClassName(value_field->message_type());
}

bool NeedsForcedFieldBuilders(const Descriptor* descriptor) {
  for (int i = 0; i < descriptor->field_count(); i++) {
    if (descriptor->field(i)->message_type() != nullptr &&
        !IsRealOneof(descriptor->field(i)) &&
        HasHasbit(descriptor->field(i))) {
      return true;
    }
  }
  return false;
}

// The static parser holds the Builder's state for these fields, a value and
// presence bit, in locals. Other fields are parsed into the message.
bool IsHeldInLocal(const FieldDescriptor* field) {
  return !field->is_repeated() && !IsRealOneof(field) &&
         GetJavaType(field) != JAVATYPE_MESSAGE;
}

// A Builder keeps a message-valued oneof alternative's field builder, and that
// Builder's own state, across changes of case and later merges. Whether a
// type's Builder can retain state its message doesn't show: it has a
// message-valued oneof or extensions, or a singular message field of a type
// that does, which a duplicate merges into.
bool CanRetainBuilderState(const Descriptor* descriptor) {
  absl::flat_hash_set<const Descriptor*> visited;
  std::vector<const Descriptor*> pending = {descriptor};
  while (!pending.empty()) {
    const Descriptor* type = pending.back();
    pending.pop_back();
    if (!visited.insert(type).second) continue;
    if (type->extension_range_count() > 0) return true;
    for (int i = 0; i < type->field_count(); ++i) {
      const FieldDescriptor* field = type->field(i);
      if (field->message_type() == nullptr || field->is_repeated()) continue;
      if (IsRealOneof(field)) return true;
      pending.push_back(field->message_type());
    }
  }
  return false;
}

bool IsMessageAlternative(const FieldDescriptor* field) {
  return IsRealOneof(field) && GetJavaType(field) == JAVATYPE_MESSAGE;
}

// Whether the static parser can hand over at this field's tag: a later
// assignment to a oneof, or a duplicate of a singular message, which the
// Builder merges into what it has.
bool HandsOverAt(const FieldDescriptor* field) {
  return !field->is_repeated() &&
         (IsRealOneof(field) || GetJavaType(field) == JAVATYPE_MESSAGE);
}

// How a oneof alternative is parsed. Alternatives whose Builder can't retain
// more than their message are parsed into a message. Others are hosted: parsed
// with the child's static parser, which hands control back to this one rather
// than handing over to its own Builder, so this one's Builder can hold the
// child's. Hosted children leave such alternatives to their parent's Builder,
// which keeps that to one level. Groups and types without a static parser are
// always left to the Builder.
enum class AlternativeKind { kParsed, kHosted, kLeftToBuilder };

AlternativeKind GetAlternativeKind(const FieldDescriptor* field,
                                   const Context* context) {
  if (!IsMessageAlternative(field) ||
      !CanRetainBuilderState(field->message_type())) {
    return AlternativeKind::kParsed;
  }
  if (GetType(field) == FieldDescriptor::TYPE_GROUP ||
      !HasStaticParser(field->message_type(), context)) {
    return AlternativeKind::kLeftToBuilder;
  }
  return AlternativeKind::kHosted;
}

bool HasMessageAlternatives(const Descriptor* descriptor) {
  for (int i = 0; i < descriptor->field_count(); ++i) {
    if (IsMessageAlternative(descriptor->field(i))) return true;
  }
  return false;
}

std::string StaticParsingLocalType(const FieldDescriptor* field) {
  switch (GetJavaType(field)) {
    case JAVATYPE_STRING:
      return "java.lang.Object";
    case JAVATYPE_ENUM:
      return "int";
    default:
      return std::string(PrimitiveTypeName(GetJavaType(field)));
  }
}

// HotSpot doesn't compile methods of more than 8000 bytes of bytecode,
// HugeMethodLimit, by default.
constexpr int kHugeMethodLimit = 8000;

bool IsClosedEnum(const FieldDescriptor* field) {
  return GetJavaType(field) == JAVATYPE_ENUM &&
         !SupportUnknownEnumValue(field);
}

// Bounds on the bytecode javac generates for the static parser, in the manner
// of the static initializer's estimates (see MaybeRestartJavaMethod). Each is
// the largest of javac 8 and later for a kind of field's case, local and
// building arguments, with room for wider constants in cases that have them.
int StaticParserSizeEstimate(const Descriptor* descriptor) {
  int size = 270;
  for (int i = 0; i < descriptor->field_count(); ++i) {
    const FieldDescriptor* field = descriptor->field(i);
    JavaType type = GetJavaType(field);
    if (field->is_map()) {
      size += IsClosedEnum(field->message_type()->map_value()) ? 106 : 65;
    } else if (field->is_repeated()) {
      size += type == JAVATYPE_MESSAGE  ? 59
              : type == JAVATYPE_STRING ? 32
              : type == JAVATYPE_BYTES  ? 39
              : IsClosedEnum(field)     ? 184
              : FixedSize(GetType(field)) == -1 ? 126
                                                : 139;
    } else if (IsRealOneof(field)) {
      size += type == JAVATYPE_MESSAGE ? 20 : IsClosedEnum(field) ? 37 : 18;
    } else if (type == JAVATYPE_MESSAGE) {
      size += 19;
    } else {
      size += IsClosedEnum(field) ? 63 : 31;
    }
  }
  return size;
}

absl::string_view StaticParsingLocalZero(const FieldDescriptor* field) {
  switch (GetJavaType(field)) {
    case JAVATYPE_BOOLEAN:
      return "false";
    case JAVATYPE_STRING:
    case JAVATYPE_BYTES:
      return "null";
    default:
      return "0";
  }
}

}  // namespace

bool HasStaticParser(const Descriptor* descriptor, const Context* context) {
  if (!context->HasGeneratedMethods(descriptor) ||
      descriptor->extension_range_count() > 0) {
    return false;
  }
  // Locals beyond 256 slots would need wide instructions. Slots hold the
  // parameters, result, unknown fields, pending tag, failure, loop state, bit
  // fields and two for temporaries in the Builder's parsing code. A static
  // parser the JIT won't compile would be slower than the Builder whose loop
  // it would compile.
  int slots = 10 + (descriptor->field_count() + 31) / 32;
  for (int i = 0; i < descriptor->field_count(); ++i) {
    const FieldDescriptor* field = descriptor->field(i);
    if (!IsHeldInLocal(field)) continue;
    JavaType type = GetJavaType(field);
    slots += type == JAVATYPE_LONG || type == JAVATYPE_DOUBLE ? 2 : 1;
  }
  return slots <= 256 &&
         StaticParserSizeEstimate(descriptor) <= kHugeMethodLimit;
}

bool CanHandOver(const Descriptor* descriptor) {
  for (int i = 0; i < descriptor->field_count(); ++i) {
    if (HandsOverAt(descriptor->field(i))) return true;
  }
  return false;
}

MessageBuilderGenerator::MessageBuilderGenerator(
    const Descriptor* descriptor, Context* context,
    const absl::btree_map<int, std::unique_ptr<OneofGenerator>>&
        oneof_generators)
    : descriptor_(descriptor),
      context_(context),
      name_resolver_(context->GetNameResolver()),
      field_generators_(MakeImmutableFieldGenerators(descriptor, context_)),
      oneof_generators_(oneof_generators) {
  ABSL_CHECK(HasDescriptorMethods(descriptor->file(), context->EnforceLite()))
      << "Generator factory error: A non-lite message generator is used to "
         "generate lite messages.";
}

MessageBuilderGenerator::~MessageBuilderGenerator() = default;

void MessageBuilderGenerator::Generate(io::Printer* printer) {
  WriteMessageDocComment(printer, descriptor_, context_->options());
  if (descriptor_->extension_range_count() > 0) {
    printer->Print(
        "public static final class Builder extends\n"
        "    com.google.protobuf.GeneratedMessage.ExtendableBuilder<\n"
        "      $classname$, Builder> implements\n"
        "    $extra_interfaces$\n"
        "    $classname$OrBuilder {\n",
        "classname", name_resolver_->GetImmutableClassName(descriptor_),
        "extra_interfaces", ExtraBuilderInterfaces(descriptor_));
  } else {
    printer->Print(
        "public static final class Builder extends\n"
        "    com.google.protobuf.GeneratedMessage.Builder<Builder> "
        "implements\n"
        "    $extra_interfaces$\n"
        "    $classname$OrBuilder {\n",
        "classname", name_resolver_->GetImmutableClassName(descriptor_),
        "extra_interfaces", ExtraBuilderInterfaces(descriptor_));
  }
  printer->Indent();

  GenerateDescriptorMethods(printer);
  GenerateCommonBuilderMethods(printer);

  if (context_->HasGeneratedMethods(descriptor_)) {
    GenerateIsInitialized(printer);
    GenerateBuilderParsingMethods(printer);
    if (HasStaticParser(descriptor_, context_)) {
      GenerateStaticParser(printer);
    }
  }

  // oneof
  for (const auto& kv : oneof_generators_) {
    kv.second->GenerateCommonBuilderMethods(printer);
  }

  // Integers for bit fields.
  int totalInts = (descriptor_->field_count() + 31) / 32;
  for (int i = 0; i < totalInts; i++) {
    printer->Print("private int $bit_field_name$;\n", "bit_field_name",
                   GetBitFieldName(i));
  }

  for (int i = 0; i < descriptor_->field_count(); i++) {
    printer->Print("\n");
    field_generators_.get(descriptor_->field(i))
        .GenerateBuilderMembers(printer);
  }

  printer->Print(
      "\n"
      "// @@protoc_insertion_point(builder_scope:$full_name$)\n",
      "full_name", descriptor_->full_name());

  printer->Outdent();
  printer->Print("}\n");
}

// ===================================================================

void MessageBuilderGenerator::GenerateDescriptorMethods(io::Printer* printer) {
  if (!descriptor_->options().no_standard_descriptor_accessor()) {
    printer->Print(
        "public static final com.google.protobuf.Descriptors.Descriptor\n"
        "    getDescriptor() {\n"
        "  return $fileclass$.internal_$identifier$_descriptor;\n"
        "}\n"
        "\n",
        "fileclass", name_resolver_->GetImmutableClassName(descriptor_->file()),
        "identifier", UniqueFileScopeIdentifier(descriptor_));
  }
  std::vector<const FieldDescriptor*> map_fields;
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor* field = descriptor_->field(i);
    if (GetJavaType(field) == JAVATYPE_MESSAGE &&
        IsMapEntry(field->message_type())) {
      map_fields.push_back(field);
    }
  }
  if (!map_fields.empty()) {
    printer->Print(
        "@SuppressWarnings({\"rawtypes\"})\n"
        "protected com.google.protobuf.MapFieldReflectionAccessor "
        "internalGetMapFieldReflection(\n"
        "    int number) {\n"
        "  switch (number) {\n");
    printer->Indent();
    printer->Indent();
    for (const FieldDescriptor* field : map_fields) {
      const FieldGeneratorInfo* info = context_->GetFieldGeneratorInfo(field);
      printer->Print(
          "case $number$:\n"
          "  return internalGet$capitalized_name$();\n",
          "number", absl::StrCat(field->number()), "capitalized_name",
          info->capitalized_name);
    }
    printer->Print(
        "default:\n"
        "  throw new RuntimeException(\n"
        "      \"Invalid map field number: \" + number);\n");
    printer->Outdent();
    printer->Outdent();
    printer->Print(
        "  }\n"
        "}\n");
    printer->Print(
        "@SuppressWarnings({\"rawtypes\"})\n"
        "protected com.google.protobuf.MapFieldReflectionAccessor "
        "internalGetMutableMapFieldReflection(\n"
        "    int number) {\n"
        "  switch (number) {\n");
    printer->Indent();
    printer->Indent();
    for (const FieldDescriptor* field : map_fields) {
      const FieldGeneratorInfo* info = context_->GetFieldGeneratorInfo(field);
      printer->Print(
          "case $number$:\n"
          "  return internalGetMutable$capitalized_name$();\n",
          "number", absl::StrCat(field->number()), "capitalized_name",
          info->capitalized_name);
    }
    printer->Print(
        "default:\n"
        "  throw new RuntimeException(\n"
        "      \"Invalid map field number: \" + number);\n");
    printer->Outdent();
    printer->Outdent();
    printer->Print(
        "  }\n"
        "}\n");
  }
  printer->Print(
      "@java.lang.Override\n"
      "protected com.google.protobuf.GeneratedMessage.FieldAccessorTable\n"
      "    internalGetFieldAccessorTable() {\n"
      "  return $fileclass$.internal_$identifier$_fieldAccessorTable\n"
      "      .ensureFieldAccessorsInitialized(\n"
      "          $classname$.class, $classname$.Builder.class);\n"
      "}\n"
      "\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_),
      "fileclass", name_resolver_->GetImmutableClassName(descriptor_->file()),
      "identifier", UniqueFileScopeIdentifier(descriptor_));
}

// ===================================================================

void MessageBuilderGenerator::GenerateCommonBuilderMethods(
    io::Printer* printer) {
  GenerateBuilderConstructors(printer);
  GenerateBuilderClearMethod(printer);
  GenerateBuilderGetDescriptorForTypeMethod(printer);
  GenerateBuilderGetDefaultInstanceForTypeMethod(printer);
  GenerateBuilderBuildMethod(printer);
  GenerateBuildPartial(printer);
  GenerateBuilderExtensionMethods(printer);

  // -----------------------------------------------------------------

  if (context_->HasGeneratedMethods(descriptor_)) {
    GenerateBuilderMergeFromMethods(printer);
  }
}

void MessageBuilderGenerator::GenerateBuilderConstructors(
    io::Printer* printer) {
  // Decide if we really need to have the "maybeForceBuilderInitialization()"
  // method.
  // TODO: Remove the need for this entirely
  bool need_maybe_force_builder_init = NeedsForcedFieldBuilders(descriptor_);

  const char* force_builder_init = need_maybe_force_builder_init
                                       ? "  maybeForceBuilderInitialization();"
                                       : "";

  printer->Print(
      "// Resolve parameter types so constructor access bridges generated for\n"
      "// targets before Java 11 can inline.\n"
      "static {\n"
      "  java.lang.reflect.Constructor<?>[] unused =\n"
      "      Builder.class.getDeclaredConstructors();\n"
      "}\n"
      "\n"
      "// Construct using $classname$.newBuilder()\n"
      "private Builder() {\n"
      "  this((com.google.protobuf.GeneratedMessage.BuilderParent) null);\n"
      "}\n"
      "\n"
      "private Builder(\n"
      "    com.google.protobuf.GeneratedMessage.BuilderParent parent) {\n"
      "  super(parent);\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_));
  printer->Indent();
  for (int i = 0; i < descriptor_->field_count(); ++i) {
    const auto* field = descriptor_->field(i);
    if (!IsRealOneof(field)) {
      field_generators_.get(field).GenerateBuilderInitializationCode(printer);
    }
  }
  printer->Outdent();
  printer->Print("$force_builder_init$\n}\n",
                 "force_builder_init", force_builder_init);

  if (need_maybe_force_builder_init) {
    printer->Print(
        "private void maybeForceBuilderInitialization() {\n"
        "  if (com.google.protobuf.GeneratedMessage\n"
        "          .alwaysUseFieldBuilders) {\n");

    printer->Indent();
    printer->Indent();
    for (int i = 0; i < descriptor_->field_count(); i++) {
      if (!IsRealOneof(descriptor_->field(i))) {
        field_generators_.get(descriptor_->field(i))
            .GenerateFieldBuilderInitializationCode(printer);
      }
    }
    printer->Outdent();
    printer->Outdent();

    printer->Print(
        "  }\n"
        "}\n");
  }
}

void MessageBuilderGenerator::GenerateBuilderClearMethod(io::Printer* printer) {
  printer->Print(
      "@java.lang.Override\n"
      "public Builder clear() {\n"
      "  super.clear();\n");

  printer->Indent();
  int totalBuilderInts = (descriptor_->field_count() + 31) / 32;
  for (int i = 0; i < totalBuilderInts; i++) {
    printer->Print("$bit_field_name$ = 0;\n", "bit_field_name",
                   GetBitFieldName(i));
  }

  for (int i = 0; i < descriptor_->field_count(); i++) {
    field_generators_.get(descriptor_->field(i))
        .GenerateBuilderClearCode(printer);
  }

  for (const auto& kv : oneof_generators_) {
    kv.second->GenerateBuilderClearMethod(printer);
  }

  printer->Outdent();

  printer->Print(
      "  return this;\n"
      "}\n"
      "\n");
}

void MessageBuilderGenerator::GenerateBuilderGetDescriptorForTypeMethod(
    io::Printer* printer) {
  printer->Print(
      "@java.lang.Override\n"
      "public com.google.protobuf.Descriptors.Descriptor\n"
      "    getDescriptorForType() {\n"
      "  return $fileclass$.internal_$identifier$_descriptor;\n"
      "}\n"
      "\n",
      "fileclass", name_resolver_->GetImmutableClassName(descriptor_->file()),
      "identifier", UniqueFileScopeIdentifier(descriptor_));
}

void MessageBuilderGenerator::GenerateBuilderGetDefaultInstanceForTypeMethod(
    io::Printer* printer) {
  // LITE runtime implements this in GeneratedMessageLite.
  printer->Print(
      "@java.lang.Override\n"
      "public $classname$ getDefaultInstanceForType() {\n"
      "  return $classname$.getDefaultInstance();\n"
      "}\n"
      "\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_));
}

void MessageBuilderGenerator::GenerateBuilderBuildMethod(io::Printer* printer) {
  printer->Print(
      "@java.lang.Override\n"
      "public $classname$ build() {\n"
      "  $classname$ result = buildPartial();\n"
      "  if (!result.isInitialized()) {\n"
      "    throw newUninitializedMessageException(result);\n"
      "  }\n"
      "  return result;\n"
      "}\n"
      "\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_));
}

void MessageBuilderGenerator::GenerateBuilderExtensionMethods(
    io::Printer* printer) {
  // We include these methods only in open source to maintain long term ABI
  // compatibility, and there should be no need to include them in Google3.
  if (google::protobuf::internal::IsOss() && descriptor_->extension_range_count() > 0) {
    printer->Print(
        "public <Type> Builder setExtension(\n"
        "    com.google.protobuf.GeneratedMessage.GeneratedExtension<\n"
        "        $classname$, Type> extension,\n"
        "    Type value) {\n"
        "  return super.setExtension(extension, value);\n"
        "}\n"
        "public <Type> Builder setExtension(\n"
        "    com.google.protobuf.GeneratedMessage.GeneratedExtension<\n"
        "        $classname$, java.util.List<Type>> extension,\n"
        "    int index, Type value) {\n"
        "  return super.setExtension(extension, index, value);\n"
        "}\n"
        "public <Type> Builder addExtension(\n"
        "    com.google.protobuf.GeneratedMessage.GeneratedExtension<\n"
        "        $classname$, java.util.List<Type>> extension,\n"
        "    Type value) {\n"
        "  return super.addExtension(extension, value);\n"
        "}\n"
        "public <Type> Builder clearExtension(\n"
        "    com.google.protobuf.GeneratedMessage.GeneratedExtension<\n"
        "        $classname$, Type> extension) {\n"
        "  return super.clearExtension(extension);\n"
        "}\n",
        "classname", name_resolver_->GetImmutableClassName(descriptor_));
  }
}

void MessageBuilderGenerator::GenerateBuilderMergeFromMethods(
    io::Printer* printer) {
  printer->Print(
      "@java.lang.Override\n"
      "public Builder mergeFrom(com.google.protobuf.Message other) {\n"
      "  if (other instanceof $classname$) {\n"
      "    return mergeFrom(($classname$)other);\n"
      "  } else {\n"
      "    super.mergeFrom(other);\n"
      "    return this;\n"
      "  }\n"
      "}\n"
      "\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_));

  printer->Print(
      "public Builder mergeFrom($classname$ other) {\n"
      // Optimization:  If other is the default instance, we know none of its
      //   fields are set so we can skip the merge.
      "  if (other == $classname$.getDefaultInstance()) return this;\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_));
  printer->Indent();

  // Prepare a list to store generated code for each field and oneof.
  std::vector<std::string> merging_code_list;

  for (int i = 0; i < descriptor_->field_count(); i++) {
    if (!IsRealOneof(descriptor_->field(i))) {
      std::string field_code;
      {
        google::protobuf::io::StringOutputStream field_stream(&field_code);
        google::protobuf::io::Printer field_printer(&field_stream);
        field_generators_.get(descriptor_->field(i))
            .GenerateMergingCode(&field_printer);
      }
      merging_code_list.push_back(field_code);
    }
  }

  // Merge oneof fields.
  for (const auto& kv : oneof_generators_) {
    std::string oneof_code;
    {
      google::protobuf::io::StringOutputStream oneof_stream(&oneof_code);
      google::protobuf::io::Printer oneof_printer(&oneof_stream);
      kv.second->GenerateMergingCode(&oneof_printer, field_generators_);
    }
    merging_code_list.push_back(oneof_code);
  }

  int total_code_blocks = merging_code_list.size();
  std::vector<MergeCodeChunk> subfunction_chunks;

  if (total_code_blocks <= kMergeMethodSplitThreshold) {
    for (const std::string& code_block : merging_code_list) {
      printer->Print(absl::StrReplaceAll(code_block, {{"$", "$$"}}));
    }
    printer->Outdent();
  } else {
    // Partition the snippets into chunks of roughly equal size, with at most
    // kMergeMethodSplitThreshold chunks.
    int chunk_count =
        std::min(kMergeMethodSplitThreshold,
                 (total_code_blocks + kMergeMethodSplitThreshold - 1) /
                     kMergeMethodSplitThreshold);
    int base_chunk_size = total_code_blocks / chunk_count;
    int remainder_elements = total_code_blocks % chunk_count;
    int current_block_index = 0;

    subfunction_chunks.reserve(chunk_count);
    for (int i = 0; i < chunk_count; ++i) {
      int current_chunk_size =
          base_chunk_size + (i < remainder_elements ? 1 : 0);
      std::string method_suffix = absl::StrCat("_", i);
      subfunction_chunks.push_back(
          {current_block_index, current_chunk_size, method_suffix});
      current_block_index += current_chunk_size;
    }

    // Call the generated subfunctions in order.
    for (const auto& chunk : subfunction_chunks) {
      printer->Print("partialMergeFrom$suffix$(other);\n", "suffix",
                     chunk.method_suffix);
    }

    printer->Outdent();
  }

  // if message type has extensions
  if (descriptor_->extension_range_count() > 0) {
    printer->Print("  this.mergeExtensionFields(other);\n");
  }

  printer->Print("  this.mergeUnknownFields(other.getUnknownFields());\n");

  printer->Print("  onChanged();\n");

  printer->Print(
      "  return this;\n"
      "}\n"
      "\n");

  // Generate the actual subfunctions if we split the logic.
  if (total_code_blocks > kMergeMethodSplitThreshold) {
    for (const auto& chunk : subfunction_chunks) {
      GenerateBuilderMergeFromSubfunction(
          printer,
          absl::MakeConstSpan(merging_code_list)
              .subspan(chunk.start_index, chunk.size),
          chunk.method_suffix);
    }
  }
}

void MessageBuilderGenerator::GenerateBuilderMergeFromSubfunction(
    io::Printer* printer, absl::Span<const std::string> merging_code_blocks,
    absl::string_view method_suffix) {
  printer->Print("private void partialMergeFrom$suffix$($classname$ other) {\n",
                 "suffix", method_suffix, "classname",
                 name_resolver_->GetImmutableClassName(descriptor_));
  printer->Indent();

  for (const std::string& code_block : merging_code_blocks) {
    printer->Print(absl::StrReplaceAll(code_block, {{"$", "$$"}}));
  }

  printer->Outdent();
  printer->Print("}\n\n");
}

void MessageBuilderGenerator::GenerateBuildPartial(io::Printer* printer) {
  printer->Print(
      "@java.lang.Override\n"
      "public $classname$ buildPartial() {\n"
      "  $classname$ result = new $classname$(this);\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_));

  printer->Indent();

  GenerateBuildPartialShardCalls(printer, /*static_parser=*/false);

  printer->Outdent();
  printer->Print(
      "  onBuilt();\n"
      "  return result;\n"
      "}\n"
      "\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_));

  if (HasStaticParser(descriptor_, context_)) {
    // A Builder the static parser hands over to holds every field of the
    // message it parsed, which this builds into rather than another.
    printer->Print(
        "private $classname$ buildPartial($classname$ result) {\n"
        "  setUnknownFields(result, getUnknownFields());\n",
        "classname", name_resolver_->GetImmutableClassName(descriptor_));
    printer->Indent();
    GenerateBuildPartialShardCalls(printer, /*static_parser=*/false);
    printer->Outdent();
    printer->Print(
        "  onBuilt();\n"
        "  return result;\n"
        "}\n"
        "\n");
  }

  // Build all fields in shards organized by bitfield membership.
  int totalInts = (descriptor_->field_count() + 31) / 32;
  for (int i = 0; i < totalInts; i++) {
    GenerateBuildPartialShard(printer, i, /*static_parser=*/false);
  }
}

void MessageBuilderGenerator::GenerateBuildPartialShardCalls(
    io::Printer* printer, bool static_parser) {
  // One buildPartial_autosplit_#() per from_bit_field
  int totalInts = (descriptor_->field_count() + 31) / 32;
  for (int i = 0; i < totalInts; ++i) {
    if (static_parser && !HasStaticBitField(i)) continue;
    printer->Print(
        "if ($bit_field_name$ != 0) { "
        "buildPartial_autosplit_$shard$(result$arguments$); }\n",
        "bit_field_name", GetBitFieldName(i), "shard", absl::StrCat(i),
        "arguments", static_parser ? StaticShardArguments(i) : "");
  }
}

std::vector<const FieldDescriptor*> MessageBuilderGenerator::ShardFields(
    int shard) const {
  // A shard's fields are those with bits in its bit field.
  std::vector<const FieldDescriptor*> fields;
  for (int i = 0; i < descriptor_->field_count(); ++i) {
    const FieldDescriptor* field = descriptor_->field(i);
    if (field_generators_.get(field).GetBitIndex() / 32 == shard) {
      fields.push_back(field);
    }
  }
  std::sort(fields.begin(), fields.end(),
            [this](const FieldDescriptor* a, const FieldDescriptor* b) {
              return field_generators_.get(a).GetBitIndex() <
                     field_generators_.get(b).GetBitIndex();
            });
  return fields;
}

bool MessageBuilderGenerator::HasStaticBitField(int shard) const {
  // Repeated and map fields set their bits too, as in the Builder.
  for (const FieldDescriptor* field : ShardFields(shard)) {
    if (IsHeldInLocal(field) || field->is_repeated()) return true;
  }
  return false;
}

std::string MessageBuilderGenerator::StaticShardArguments(int shard) const {
  std::string arguments = absl::StrCat(", ", GetBitFieldName(shard));
  for (const FieldDescriptor* field : ShardFields(shard)) {
    if (IsHeldInLocal(field)) {
      absl::StrAppend(&arguments, ", ",
                      context_->GetFieldGeneratorInfo(field)->name, "_");
    }
  }
  return arguments;
}

void MessageBuilderGenerator::GenerateBuildPartialShard(io::Printer* printer,
                                                        int shard,
                                                        bool static_parser) {
  if (static_parser) {
    // From the static parser's locals, named for the Builder's fields, which
    // the Builder's building code compiles against.
    printer->Print(
        "private static void buildPartial_autosplit_$shard$(\n"
        "    $classname$ result,\n"
        "    int $bit_field_name$",
        "classname", name_resolver_->GetImmutableClassName(descriptor_),
        "shard", absl::StrCat(shard), "bit_field_name", GetBitFieldName(shard));
    for (const FieldDescriptor* field : ShardFields(shard)) {
      if (IsHeldInLocal(field)) {
        printer->Print(",\n    $type$ $name$_", "type",
                       StaticParsingLocalType(field), "name",
                       context_->GetFieldGeneratorInfo(field)->name);
      }
    }
    printer->Print(") {\n");
  } else {
    printer->Print(
        "private void buildPartial_autosplit_$shard$($classname$ result) {\n",
        "classname", name_resolver_->GetImmutableClassName(descriptor_),
        "shard", absl::StrCat(shard));
  }
  printer->Print(
      "  int from_$bit_field_name$ = $bit_field_name$;\n"
      "  int to_$bit_field_name$ = 0;\n",
      "bit_field_name", GetBitFieldName(shard));
  printer->Indent();

  int i = shard * 32;
  int shard_end = std::min(i + 32, static_cast<int>(field_generators_.size()));
  for (; i < shard_end; ++i) {
    const ImmutableFieldGenerator& field =
        field_generators_.getInInsertOrder(i);

    // Copy the field from the builder to the message
    field.GenerateBuildingCode(printer, static_parser);
  }

  // The static parser parses oneofs into the message.
  for (const auto& kv : oneof_generators_) {
    if (kv.second->HasScalarFields(shard) && !static_parser) {
      kv.second->GenerateBuildingCode(printer, shard);
    }
  }

  printer->Outdent();

  // Copy the bit field results to the generated message
  printer->Print(
      "  result.$bit_field_name$ |= to_$bit_field_name$;\n"
      "}\n\n",
      "bit_field_name", GetBitFieldName(shard));
}

// ===================================================================

void MessageBuilderGenerator::GenerateBuilderParsingMethods(
    io::Printer* printer) {
  printer->Print(
      "@java.lang.Override\n"
      "public Builder mergeFrom(\n"
      "    com.google.protobuf.CodedInputStream input,\n"
      "    com.google.protobuf.ExtensionRegistryLite extensionRegistry)\n"
      "    throws java.io.IOException {\n"
      "  java.util.Objects.requireNonNull(extensionRegistry);\n"
      "  try {\n"
      "    boolean done = false;\n"
      "    while (!done) {\n"
      "      int tag = input.readTag();\n"
      "      switch (tag) {\n"
      "        case 0:\n"  // zero signals EOF / limit reached
      "          done = true;\n"
      "          break;\n");
  printer->Indent();  // method
  printer->Indent();  // try
  printer->Indent();  // while
  printer->Indent();  // switch
  GenerateBuilderFieldParsingCases(printer, /*static_parser=*/false);
  printer->Outdent();  // switch
  printer->Outdent();  // while
  printer->Outdent();  // try
  printer->Outdent();  // method
  printer->Print(
      "        default: {\n"
      "          if (!super.parseUnknownField(input, extensionRegistry, tag)) "
      "{\n"
      "            done = true; // was an endgroup tag\n"
      "          }\n"
      "          break;\n"
      "        } // default:\n"
      "      } // switch (tag)\n"
      "    } // while (!done)\n"
      "  } catch (com.google.protobuf.InvalidProtocolBufferException e) {\n"
      "    throw e.unwrapIOException();\n"
      "  } finally {\n"
      "    onChanged();\n"
      "  } // finally\n"
      "  return this;\n"
      "}\n");
  if (!HasStaticParser(descriptor_, context_) || !CanHandOver(descriptor_)) {
    return;
  }
  // The static parser hands over a tag it has read but not decoded, which this
  // parses as the loop above would, before continuing with the loop.
  printer->Print(
      "\n"
      "private Builder mergeFrom(\n"
      "    com.google.protobuf.CodedInputStream input,\n"
      "    com.google.protobuf.ExtensionRegistryLite extensionRegistry,\n"
      "    int pendingTag)\n"
      "    throws java.io.IOException {\n"
      "  try {\n"
      "    switch (pendingTag) {\n");
  printer->Indent();  // method
  printer->Indent();  // try
  printer->Indent();  // switch
  for (const FieldDescriptor* field : SortFieldsByNumber(descriptor_)) {
    if (HandsOverAt(field)) {
      GenerateBuilderFieldParsingCase(printer, field, /*static_parser=*/false);
    }
  }
  printer->Outdent();  // switch
  printer->Outdent();  // try
  printer->Outdent();  // method
  printer->Print(
      "    }\n"
      "  } finally {\n"
      "    onChanged();\n"
      "  }\n"
      "  return mergeFrom(input, extensionRegistry);\n"
      "}\n");
}

void MessageBuilderGenerator::GenerateBuilderFieldParsingCases(
    io::Printer* printer, bool static_parser) {
  std::vector<const FieldDescriptor*> sorted_fields(
      SortFieldsByNumber(descriptor_));
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor* field = sorted_fields[i];
    GenerateBuilderFieldParsingCase(printer, field, static_parser);
    if (field->is_packable()) {
      GenerateBuilderPackedFieldParsingCase(printer, field, static_parser);
    }
  }
}

void MessageBuilderGenerator::GenerateBuilderFieldParsingCase(
    io::Printer* printer, const FieldDescriptor* field, bool static_parser) {
  uint32_t tag = WireFormatLite::MakeTag(
      field->number(), WireFormat::WireTypeForFieldType(field->type()));
  std::string tagString = absl::StrCat(static_cast<int32_t>(tag));
  printer->Print("case $tag$: {\n", "tag", tagString);
  printer->Indent();

  if (static_parser && IsMessageAlternative(field)) {
    GenerateStaticAlternativeParsingCode(printer, field);
  } else {
    field_generators_.get(field).GenerateBuilderParsingCode(printer,
                                                            static_parser);
  }

  printer->Outdent();
  printer->Print(
      "  break;\n"
      "} // case $tag$\n",
      "tag", tagString);
}

void MessageBuilderGenerator::GenerateBuilderPackedFieldParsingCase(
    io::Printer* printer, const FieldDescriptor* field, bool static_parser) {
  // To make packed = true wire compatible, we generate parsing code from a
  // packed version of this field regardless of field->options().packed().
  uint32_t tag = WireFormatLite::MakeTag(
      field->number(), WireFormatLite::WIRETYPE_LENGTH_DELIMITED);
  std::string tagString = absl::StrCat(static_cast<int32_t>(tag));
  printer->Print("case $tag$: {\n", "tag", tagString);
  printer->Indent();

  field_generators_.get(field).GenerateBuilderParsingCodeFromPacked(
      printer, static_parser);

  printer->Outdent();
  printer->Print(
      "  break;\n"
      "} // case $tag$\n",
      "tag", tagString);
}

void MessageBuilderGenerator::GenerateStaticParser(io::Printer* printer) {
  auto vars = printer->WithVars(
      {{"classname", name_resolver_->GetImmutableClassName(descriptor_)}});
  int total_ints = (descriptor_->field_count() + 31) / 32;
  std::vector<const FieldDescriptor*> hosted;
  for (const FieldDescriptor* field : SortFieldsByNumber(descriptor_)) {
    if (GetAlternativeKind(field, context_) == AlternativeKind::kHosted) {
      hosted.push_back(field);
    }
  }

  // Locals share the names of the Builder fields they stand in for, so the
  // Builder's parsing and building code compiles against them unchanged.
  // Parsed as a hosted alternative, a pending tag returns the message so far to
  // the parent, as below.
  printer->Print(
      "\n"
      "private static $classname$ parsePartialFrom(\n"
      "    com.google.protobuf.CodedInputStream input,\n"
      "    com.google.protobuf.ExtensionRegistryLite extensionRegistry,\n"
      "    boolean hosted)\n"
      "    throws com.google.protobuf.InvalidProtocolBufferException {\n"
      "  java.util.Objects.requireNonNull(extensionRegistry);\n");
  printer->Indent();
  if (NeedsForcedFieldBuilders(descriptor_) || !hosted.empty()) {
    // Forced field builders are initialized as present, so only the Builder
    // parses in that testing mode. A hosted alternative's static parser would
    // build its child, losing the state its forced field builders retain.
    printer->Print(
        "if (alwaysUseFieldBuilders()) {\n"
        "  Builder builder = new Builder();\n"
        "  try {\n"
        "    builder.mergeFrom(input, extensionRegistry);\n"
        "  } catch (java.lang.Exception e) {\n"
        "    wrapAndThrowParseException(parser(), e, builder);\n"
        "  }\n"
        "  return builder.buildPartial();\n"
        "}\n");
  }
  printer->Print(
      "$classname$ result = new $classname$(null);\n"
      "com.google.protobuf.UnknownFieldSet.Builder unknownFields = null;\n"
      "int pendingTag = 0;\n");
  for (int shard = 0; shard < total_ints; ++shard) {
    if (HasStaticBitField(shard)) {
      printer->Print("int $bit_field_name$ = 0;\n", "bit_field_name",
                     GetBitFieldName(shard));
    }
  }
  for (int i = 0; i < descriptor_->field_count(); ++i) {
    const FieldDescriptor* field = descriptor_->field(i);
    if (!IsHeldInLocal(field)) continue;
    printer->Print("$type$ $name$_ = $zero$;\n", "type",
                   StaticParsingLocalType(field), "name",
                   context_->GetFieldGeneratorInfo(field)->name, "zero",
                   StaticParsingLocalZero(field));
  }
  // A pending tag hands over to a Builder, which continues from that tag. A
  // negative one is a hosted alternative's, after the tag last dispatched,
  // and is the complement of the limit to restore after continuing it.
  printer->Print(
      "int tag = 0;\n"
      "java.lang.Exception failure = null;\n"
      "try {\n"
      "  boolean done = false;\n"
      "  while (!done && pendingTag == 0) {\n"
      "    tag = input.readTag();\n"
      "    switch (tag) {\n"
      "      case 0:\n"
      "        done = true;\n"
      "        break;\n");
  printer->Indent();  // try
  printer->Indent();  // while
  printer->Indent();  // switch
  GenerateBuilderFieldParsingCases(printer, /*static_parser=*/true);
  printer->Outdent();  // switch
  printer->Outdent();  // while
  printer->Outdent();  // try
  printer->Print(
      "      default:\n"
      "        if (com.google.protobuf.WireFormat.getTagWireType(tag)\n"
      "            == com.google.protobuf.WireFormat.WIRETYPE_END_GROUP) {\n"
      "          done = true; // As in the Builder\n"
      "        } else {\n"
      "          unknownFields = "
      "mergeUnknownField(unknownFields, input, tag);\n"
      "        }\n"
      "        break;\n"
      "    }\n"
      "  }\n"
      "} catch (java.lang.Exception e) {\n"
      "  failure = e;\n"
      "}\n");
  GenerateBuildPartialShardCalls(printer, /*static_parser=*/true);
  // A hosted alternative returns to the parent, which continues it.
  printer->Print(
      "if (unknownFields != null) {\n"
      "  setUnknownFields(result, unknownFields.build());\n"
      "}\n"
      "if (failure == null && (pendingTag == 0 || hosted)) {\n"
      "  return result;\n"
      "}\n"
      // Merged in the form the Builder's own parsing would have left it, to
      // continue from the pending tag, or for a failure, which has none, to
      // make the Builder's partial state the unfinished message.
      "Builder builder = new Builder();\n"
      "builder.mergeFrom(result);\n");
  if (HasMessageAlternatives(descriptor_)) {
    printer->Print("retainParsedAlternatives(builder);\n");
  }
  printer->Print(
      "try {\n"
      "  if (failure != null) {\n"
      "    throw failure;\n"
      "  }\n");
  if (!hosted.empty()) {
    printer->Print(
        "  if (pendingTag < 0) {\n"
        "    builder.continueParsingAlternative(\n"
        "        input, extensionRegistry, tag, ~pendingTag);\n"
        "    pendingTag = 0;\n"
        "  }\n");
  }
  printer->Print(CanHandOver(descriptor_)
                     ? "  builder.mergeFrom(input, extensionRegistry, "
                       "pendingTag);\n"
                     : "  builder.mergeFrom(input, extensionRegistry);\n");
  printer->Print(
      "} catch (java.lang.Exception e) {\n"
      "  wrapAndThrowParseException(parser(), e, builder);\n"
      "}\n"
      "return builder.buildPartial(result);\n");
  printer->Outdent();
  printer->Print("}\n\n");

  for (int shard = 0; shard < total_ints; ++shard) {
    if (HasStaticBitField(shard)) {
      GenerateBuildPartialShard(printer, shard, /*static_parser=*/true);
    }
  }
  if (HasMessageAlternatives(descriptor_)) {
    printer->Print(
        "// As the Builder parses alternatives, through field builders.\n"
        "private static void retainParsedAlternatives(Builder parsed) {\n");
    for (int i = 0; i < descriptor_->field_count(); ++i) {
      const FieldDescriptor* field = descriptor_->field(i);
      if (!IsRealOneof(field) || GetJavaType(field) != JAVATYPE_MESSAGE) {
        continue;
      }
      printer->Print(
          "  if (parsed.has$capitalized_name$()) {\n"
          "    retainParsedAlternatives(\n"
          "        $type$.parser(), parsed.get$capitalized_name$Builder());\n"
          "  }\n",
          "capitalized_name",
          context_->GetFieldGeneratorInfo(field)->capitalized_name, "type",
          name_resolver_->GetImmutableClassName(field->message_type()));
    }
    printer->Print("}\n\n");
  }
  for (const FieldDescriptor* field : SortFieldsByNumber(descriptor_)) {
    if (!IsMessageAlternative(field)) {
      field_generators_.get(field).GenerateStaticParsingMethods(printer);
    } else if (GetAlternativeKind(field, context_) !=
               AlternativeKind::kLeftToBuilder) {
      GenerateStaticAlternativeParseMethod(printer, field);
    }
  }
  if (hosted.empty()) return;
  // As the Builder's parse$capitalized_name$() continues after reading through
  // the field builder.
  printer->Print(
      "private void continueParsingAlternative(\n"
      "    com.google.protobuf.CodedInputStream input,\n"
      "    com.google.protobuf.ExtensionRegistryLite extensionRegistry,\n"
      "    int tag,\n"
      "    int oldLimit)\n"
      "    throws java.io.IOException {\n"
      "  switch (tag) {\n");
  for (const FieldDescriptor* field : hosted) {
    printer->Print(
        "    case $tag$:\n"
        "      continueAlternative($type$.parser(),\n"
        "          internalGet$capitalized_name$FieldBuilder().getBuilder(),\n"
        "          input, extensionRegistry);\n"
        "      break;\n",
        "tag", absl::StrCat(static_cast<int32_t>(WireFormat::MakeTag(field))),
        "type", name_resolver_->GetImmutableClassName(field->message_type()),
        "capitalized_name",
        context_->GetFieldGeneratorInfo(field)->capitalized_name);
  }
  printer->Print(
      "  }\n"
      "  input.popLimitAfterMessage(oldLimit);\n"
      "}\n\n");
}

void MessageBuilderGenerator::GenerateStaticAlternativeParsingCode(
    io::Printer* printer, const FieldDescriptor* field) {
  switch (GetAlternativeKind(field, context_)) {
    case AlternativeKind::kLeftToBuilder:
      printer->Print("pendingTag = tag;\n");
      break;
    case AlternativeKind::kHosted:
      printer->Print(
          "pendingTag = parse$capitalized_name$(result, input, "
          "extensionRegistry, hosted);\n",
          "capitalized_name",
          context_->GetFieldGeneratorInfo(field)->capitalized_name);
      break;
    case AlternativeKind::kParsed:
      printer->Print(
          "pendingTag = parse$capitalized_name$(result, input, "
          "extensionRegistry);\n",
          "capitalized_name",
          context_->GetFieldGeneratorInfo(field)->capitalized_name);
      break;
  }
}

void MessageBuilderGenerator::GenerateStaticAlternativeParseMethod(
    io::Printer* printer, const FieldDescriptor* field) {
  bool hosted =
      GetAlternativeKind(field, context_) == AlternativeKind::kHosted;
  bool group = GetType(field) == FieldDescriptor::TYPE_GROUP;
  std::string type =
      name_resolver_->GetImmutableClassName(field->message_type());
  auto vars = printer->WithVars(
      {{"capitalized_name",
        context_->GetFieldGeneratorInfo(field)->capitalized_name},
       {"oneof_name",
        context_->GetOneofGeneratorInfo(field->containing_oneof())->name},
       {"oneof_capitalized_name",
        context_->GetOneofGeneratorInfo(field->containing_oneof())
            ->capitalized_name},
       {"number", absl::StrCat(field->number())},
       {"tag", absl::StrCat(static_cast<int32_t>(WireFormat::MakeTag(field)))},
       {"type", type},
       // A hosted parser leaves alternatives it would host to its parent, and
       // a hosted alternative is left to the Builder if its parser can't host
       // it, as with generated code from before this.
       {"hosted_parameter",
        hosted ? ",\n    boolean hosted" : ""},
       {"or_unhosted",
        hosted ? absl::StrCat(" || hosted || !parsesAlternatives(", type,
                              ".parser())")
               : ""}});
  // As the Builder, which sets the case before reading through the field
  // builder, so a failure leaves the child's unfinished message, or the
  // default instance if the child's parse didn't start.
  printer->Print(
      "private static int parse$capitalized_name$(\n"
      "    $classname$ result,\n"
      "    com.google.protobuf.CodedInputStream input,\n"
      "    com.google.protobuf.ExtensionRegistryLite extensionRegistry"
      "$hosted_parameter$)\n"
      "    throws java.io.IOException {\n"
      "  if (result.get$oneof_capitalized_name$Case().getNumber() != 0"
      "$or_unhosted$) {\n"
      "    return $tag$;\n"
      "  }\n"
      "  result.$oneof_name$Case_ = $number$;\n"
      "  result.$oneof_name$_ = $type$.getDefaultInstance();\n");
  printer->Indent();
  if (!group) {
    printer->Print("final int oldLimit = input.pushLimitBeforeMessage();\n");
  }
  printer->Print(
      absl::StrCat(
          "try {\n"
          "  result.$oneof_name$_ = ",
          group    ? "input.readGroup($number$, $type$.parser(), "
                     "extensionRegistry)"
          : hosted ? "parseAlternative($type$.parser(), input, "
                     "extensionRegistry)"
                   : "$type$.parser().parsePartialFrom(input, "
                     "extensionRegistry)",
          ";\n"
          "} catch (com.google.protobuf.InvalidProtocolBufferException e) {\n"
          "  if (e.getUnfinishedMessage() != null) {\n"
          "    result.$oneof_name$_ = e.getUnfinishedMessage();\n"
          "  }\n"
          "  throw e;\n"
          "}\n"));
  if (hosted) {
    // The child returns before a tag only a Builder can parse, which this
    // one's Builder continues from inside the child's limit. The tag returned
    // is the complement of the limit to restore afterwards.
    printer->Print(
        "if (input.getLastTag() != 0\n"
        "    && com.google.protobuf.WireFormat.getTagWireType("
        "input.getLastTag())\n"
        "        != com.google.protobuf.WireFormat.WIRETYPE_END_GROUP) {\n"
        "  return ~oldLimit;\n"
        "}\n");
  }
  if (!group) {
    printer->Print("input.popLimitAfterMessage(oldLimit);\n");
  }
  printer->Print("return 0;\n");
  printer->Outdent();
  printer->Print("}\n\n");
}

// ===================================================================

void MessageBuilderGenerator::GenerateIsInitialized(io::Printer* printer) {
  // If the message transitively has no required fields or extensions,
  // isInitialized() is always true.
  if (!HasRequiredFields(descriptor_)) {
    printer->Print(
        "@java.lang.Override\n"
        "public final boolean isInitialized() {\n"
        "  return true;\n"
        "}\n"
        "\n");
    return;
  }

  printer->Print(
      "@java.lang.Override\n"
      "public final boolean isInitialized() {\n");
  printer->Indent();

  // Check that all required fields in this message are set.
  // TODO:  We can optimize this when we switch to putting all the
  //   "has" fields into a single bitfield.
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor* field = descriptor_->field(i);
    const FieldGeneratorInfo* info = context_->GetFieldGeneratorInfo(field);

    if (field->is_required()) {
      printer->Print(
          "if (!has$name$()) {\n"
          "  return false;\n"
          "}\n",
          "name", info->capitalized_name);
    }
  }

  // Now check that all embedded messages are initialized.
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor* field = descriptor_->field(i);
    const FieldGeneratorInfo* info = context_->GetFieldGeneratorInfo(field);
    if (GetJavaType(field) == JAVATYPE_MESSAGE &&
        HasRequiredFields(field->message_type())) {
      if (field->is_required()) {
        printer->Print(
            "if (!get$name$().isInitialized()) {\n"
            "  return false;\n"
            "}\n",
            "type",
            name_resolver_->GetImmutableClassName(field->message_type()),
            "name", info->capitalized_name);
      } else if (field->is_repeated()) {
        if (IsMapEntry(field->message_type())) {
          printer->Print(
              "for ($type$ item : get$name$Map().values()) {\n"
              "  if (!item.isInitialized()) {\n"
              "    return false;\n"
              "  }\n"
              "}\n",
              "type",
              MapValueImmutableClassdName(field->message_type(),
                                          name_resolver_),
              "name", info->capitalized_name);
        } else {
          printer->Print(
              "for (int i = 0; i < get$name$Count(); i++) {\n"
              "  if (!get$name$(i).isInitialized()) {\n"
              "    return false;\n"
              "  }\n"
              "}\n",
              "type",
              name_resolver_->GetImmutableClassName(field->message_type()),
              "name", info->capitalized_name);
        }
      } else {
        printer->Print(
            "if (has$name$()) {\n"
            "  if (!get$name$().isInitialized()) {\n"
            "    return false;\n"
            "  }\n"
            "}\n",
            "name", info->capitalized_name);
      };
    }
  }

  if (descriptor_->extension_range_count() > 0) {
    printer->Print(
        "if (!extensionsAreInitialized()) {\n"
        "  return false;\n"
        "}\n");
  }

  printer->Outdent();

  printer->Print(
      "  return true;\n"
      "}\n"
      "\n");
}

// ===================================================================

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#include "google/protobuf/port_undef.inc"
