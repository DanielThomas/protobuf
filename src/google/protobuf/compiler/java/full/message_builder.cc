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
}  // namespace

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

void MessageBuilderGenerator::GenerateParser(io::Printer* printer) {
  GenerateBuilderParsingMethods(printer, ParsingTarget::kMessage);
  for (int i = 0; i < descriptor_->field_count(); ++i) {
    field_generators_.get(descriptor_->field(i))
        .GenerateBuilderMembers(printer, ParsingTarget::kMessage);
  }
  for (const auto& oneof : oneof_generators_) {
    oneof.second->GenerateBuilderClearOneofHasBits(printer,
                                                   ParsingTarget::kMessage);
  }
  for (int i = 0; i < (descriptor_->field_count() + 31) / 32; ++i) {
    if (NeedsParserBuildPartialShard(i)) {
      GenerateBuildPartialShard(printer, i, CopyTarget::kParserInPlace);
    }
  }
}

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
  bool need_maybe_force_builder_init = false;
  for (int i = 0; i < descriptor_->field_count(); i++) {
    if (descriptor_->field(i)->message_type() != nullptr &&
        !IsRealOneof(descriptor_->field(i)) &&
        HasHasbit(descriptor_->field(i))) {
      need_maybe_force_builder_init = true;
      break;
    }
  }

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
  if (need_maybe_force_builder_init) {
    printer->Print("  maybeForceBuilderInitialization();\n");
  }
  printer->Print("}\n");

  if (context_->HasGeneratedMethods(descriptor_)) {
    // Any generated message can be promoted as a singular child, even when
    // its own parser never hands off.
    // Pack locals only at handoff so wide messages stay below the JVM's
    // 255-parameter-slot limit without allocating a parse-state object up
    // front.
    printer->Print(
        "private Builder($classname$ other,\n"
        "    com.google.protobuf.UnknownFieldSet.Builder unknown,\n"
        "    java.lang.Object[] parserLocals) {\n"
        "  super(other, unknown);\n",
        "classname", name_resolver_->GetImmutableClassName(descriptor_));
    for (int i = 0; i < (descriptor_->field_count() + 31) / 32; ++i) {
      printer->Print("  copyFrom_autosplit_$shard$(other, parserLocals);\n",
                     "shard", absl::StrCat(i));
    }
    printer->Print("}\n");
  }

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

  // One buildPartial_autosplit_#() per from_bit_field
  int totalInts = (descriptor_->field_count() + 31) / 32;
  if (totalInts > 0) {
    for (int i = 0; i < totalInts; ++i) {
      printer->Print(
          "if ($bit_field_name$ != 0) { "
          "buildPartial_autosplit_$shard$(result); }\n",
          "bit_field_name", GetBitFieldName(i), "shard", absl::StrCat(i));
    }
  }

  printer->Outdent();
  printer->Print(
      "  onBuilt();\n"
      "  return result;\n"
      "}\n"
      "\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_));

  // Build all fields in shards organized by bitfield membership.
  for (int i = 0; i < totalInts; i++) {
    GenerateBuildPartialShard(printer, i);
    if (context_->HasGeneratedMethods(descriptor_)) {
      GenerateBuilderStateCopyShard(printer, i);
    }
  }
}

void MessageBuilderGenerator::GenerateBuildPartialShard(io::Printer* printer,
                                                        int shard,
                                                        CopyTarget target) {
  if (target == CopyTarget::kParserInPlace) {
    printer->Print(
        "private static void buildPartial_autosplit_$shard$($classname$ "
        "instance",
        "classname", name_resolver_->GetImmutableClassName(descriptor_),
        "shard", absl::StrCat(shard));
    for (const auto* field : ParserLocalFields(descriptor_)) {
      const auto& generator = field_generators_.get(field);
      if (generator.GetBitIndex() / 32 == shard) {
        printer->Print(",\n    $type$ $local$", "type",
                       generator.ParserLocalType(), "local",
                       generator.ParserLocalName());
      }
    }
    printer->Print(
        ") {\n  int from_$bit_field_name$ = instance.$bit_field_name$;\n",
        "bit_field_name", GetBitFieldName(shard));
  } else {
    // Retain parsing flags for direct child promotion, including
    // implicit-presence scalars whose bits were previously discarded by
    // publication.
    printer->Print(
        "private void buildPartial_autosplit_$shard$($classname$ result) {\n"
        "  int from_$bit_field_name$ = $bit_field_name$;\n"
        "  int to_$bit_field_name$ = from_$bit_field_name$;\n",
        "classname", name_resolver_->GetImmutableClassName(descriptor_),
        "shard", absl::StrCat(shard), "bit_field_name", GetBitFieldName(shard));
  }
  printer->Indent();

  int i = shard * 32;
  int shard_end = std::min(i + 32, static_cast<int>(field_generators_.size()));
  for (; i < shard_end; ++i) {
    const ImmutableFieldGenerator& field =
        field_generators_.getInInsertOrder(i);

    // Copy the field from the builder to the message
    field.GenerateBuildingCode(printer, target);
  }

  for (const auto& kv : oneof_generators_) {
    if (target == CopyTarget::kMessage && kv.second->HasScalarFields(shard)) {
      kv.second->GenerateBuildingCode(printer, shard);
    }
  }

  printer->Outdent();

  // Copy the bit field results to the generated message
  if (target == CopyTarget::kMessage) {
    printer->Print("  result.$bit_field_name$ |= to_$bit_field_name$;\n",
                   "bit_field_name", GetBitFieldName(shard));
  }
  printer->Print("}\n\n");
}

// ===================================================================

void MessageBuilderGenerator::GenerateBuilderStateCopyShard(
    io::Printer* printer, int shard) {
  const auto locals = ParserLocalFields(descriptor_);
  std::vector<int> local_indices;
  for (int i = 0; i < locals.size(); ++i) {
    if (field_generators_.get(locals[i]).GetBitIndex() / 32 == shard) {
      local_indices.push_back(i);
    }
  }
  if (!local_indices.empty()) {
    printer->Print("@java.lang.SuppressWarnings(\"unchecked\")\n");
  }
  printer->Print(
      "private void copyFrom_autosplit_$shard$($classname$ other, "
      "java.lang.Object[] parserLocals) {\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_), "shard",
      absl::StrCat(shard));
  printer->Indent();
  const int end =
      std::min(shard * 32 + 32, static_cast<int>(field_generators_.size()));
  for (int i = shard * 32; i < end; ++i) {
    field_generators_.getInInsertOrder(i).GenerateBuilderStateCode(printer);
  }
  for (const auto& oneof : oneof_generators_) {
    oneof.second->GenerateBuilderStateCode(printer, shard);
  }
  if (!local_indices.empty()) {
    printer->Print("if (parserLocals != null) {\n");
    printer->Indent();
    for (int i : local_indices) {
      field_generators_.get(locals[i]).GenerateParserLocalTransferCode(printer,
                                                                       i);
    }
    printer->Outdent();
    printer->Print("}\n");
  }
  printer->Outdent();
  printer->Print("  $bit_field_name$ = other.$bit_field_name$;\n}\n\n",
                 "bit_field_name", GetBitFieldName(shard));
}

void MessageBuilderGenerator::GenerateBuilderParsingMethods(
    io::Printer* printer, ParsingTarget target) {
  const bool message = target == ParsingTarget::kMessage;
  auto vars = printer->WithVars(
      {{"declaration", message ? absl::StrCat("static ", descriptor_->name(),
                                              " parsePartialFrom")
                               : "public Builder mergeFrom"},
       {"exception", message
                         ? "com.google.protobuf.InvalidProtocolBufferException"
                         : "java.io.IOException"}});
  if (!message) printer->Print("@java.lang.Override\n");
  printer->Print(
      "$declaration$(\n"
      "    com.google.protobuf.CodedInputStream input,\n"
      "    com.google.protobuf.ExtensionRegistryLite extensionRegistry)\n"
      "    throws $exception$ {\n"
      "  java.util.Objects.requireNonNull(extensionRegistry);\n");
  if (message) {
    printer->Indent();
    // Resolve the builder parameter type for HotSpot 8 constructor inlining.
    printer->Print(
        "$classname$ instance = new $classname$((Builder) null);\n"
        "com.google.protobuf.UnknownFieldSet.Builder unknown = null;\n"
        "java.io.IOException parseFailure = null;\n");
    if (ParserMayHandoff(descriptor_)) printer->Print("int pendingTag = 0;\n");
    for (const auto* field : ParserLocalFields(descriptor_)) {
      const auto& generator = field_generators_.get(field);
      printer->Print("$type$ $local$ = null;\n", "type",
                     generator.ParserLocalType(), "local",
                     generator.ParserLocalName());
    }
    const auto initialization_fields = BuilderInitializationFields(descriptor_);
    if (!initialization_fields.empty()) {
      printer->Print(
          "if (com.google.protobuf.GeneratedMessage.alwaysUseFieldBuilders) "
          "{\n");
      printer->Indent();
      for (const auto* field : initialization_fields) {
        field_generators_.get(field).GenerateFieldBuilderInitializationCode(
            printer, target);
      }
      printer->Outdent();
      printer->Print("}\n");
    }
    printer->Outdent();
  }
  printer->Print(
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
  GenerateBuilderFieldParsingCases(printer, target);
  printer->Outdent();  // switch
  printer->Outdent();  // while
  printer->Outdent();  // try
  printer->Outdent();  // method
  printer->Print("        default: {\n");
  if (message && descriptor_->extension_range_count() > 0) {
    printer->Print("          pendingTag = tag;\n          done = true;\n");
  } else {
    if (message) {
      printer->Print(
          "          unknown = com.google.protobuf.GeneratedMessageParser."
          "prepareUnknownFields(unknown, input);\n");
    }
    printer->Print(
        "          if (!$parse_unknown$) {\n"
        "            done = true; // was an endgroup tag\n"
        "          }\n",
        "parse_unknown",
        message ? "instance.parseUnknownField(input, unknown, "
                  "extensionRegistry, tag)"
                : "super.parseUnknownField(input, extensionRegistry, tag)");
  }
  printer->Print(
      "          break;\n"
      "        } // default:\n"
      "      } // switch (tag)\n"
      "    } // while (!done)\n");
  if (message) {
    printer->Print(
        "  } catch (java.io.IOException e) {\n    parseFailure = e;\n  }\n");
    printer->Indent();
    if (ParserMayHandoff(descriptor_)) GenerateParserHandoff(printer);
    printer->Print(
        "if (unknown != null) {\n"
        "  instance.unknownFields = unknown.build();\n"
        "}\n");
    GenerateParserBuildPartialCalls(printer);
    printer->Print(
        "if (parseFailure != null) {\n"
        "  "
        "com.google.protobuf.GeneratedMessageParser.throwParseException("
        "parseFailure, instance);\n"
        "}\n"
        "return instance;\n");
    printer->Outdent();
  } else {
    printer->Print(
        "  } catch (com.google.protobuf.InvalidProtocolBufferException e) {\n"
        "    throw e.unwrapIOException();\n"
        "  } finally {\n"
        "    onChanged();\n"
        "  } // finally\n"
        "  return this;\n");
  }
  printer->Print("}\n");
  if (!message && ParserMayHandoff(descriptor_)) {
    GenerateBuilderPendingFieldParsing(printer);
  }
}

void MessageBuilderGenerator::GenerateBuilderFieldParsingCases(
    io::Printer* printer, ParsingTarget target) {
  std::vector<const FieldDescriptor*> sorted_fields(
      SortFieldsByNumber(descriptor_));
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor* field = sorted_fields[i];
    GenerateBuilderFieldParsingCase(printer, field, target);
    if (field->is_packable()) {
      GenerateBuilderPackedFieldParsingCase(printer, field, target);
    }
  }
}

void MessageBuilderGenerator::GenerateBuilderFieldParsingCase(
    io::Printer* printer, const FieldDescriptor* field, ParsingTarget target,
    bool pending) {
  uint32_t tag = WireFormatLite::MakeTag(
      field->number(), WireFormat::WireTypeForFieldType(field->type()));
  std::string tagString = absl::StrCat(static_cast<int32_t>(tag));
  printer->Print("case $tag$: {\n", "tag", tagString);
  printer->Indent();

  if (target == ParsingTarget::kMessage && !field->is_repeated() &&
      GetJavaType(field) == JAVATYPE_MESSAGE) {
    const auto* info = context_->GetFieldGeneratorInfo(field);
    printer->Print(
        IsRealOneof(field)
            ? "if ($name$Builder_ != null) {\n"
            : "if (instance.$name$_ != null || $name$Builder_ != null) {\n",
        "name", info->name);
    printer->Print("  pendingTag = tag;\n  done = true;\n  break;\n}\n");
    if (IsRealOneof(field)) {
      field_generators_.get(field).GenerateFieldBuilderInitializationCode(
          printer, target);
    }
  }
  field_generators_.get(field).GenerateBuilderParsingCode(printer, target);

  printer->Outdent();
  printer->Print(
      "$end_case$\n"
      "} // case $tag$\n",
      "end_case", pending ? "  return true;" : "  break;", "tag", tagString);
}

void MessageBuilderGenerator::GenerateBuilderPackedFieldParsingCase(
    io::Printer* printer, const FieldDescriptor* field, ParsingTarget target) {
  // To make packed = true wire compatible, we generate parsing code from a
  // packed version of this field regardless of field->options().packed().
  uint32_t tag = WireFormatLite::MakeTag(
      field->number(), WireFormatLite::WIRETYPE_LENGTH_DELIMITED);
  std::string tagString = absl::StrCat(static_cast<int32_t>(tag));
  printer->Print("case $tag$: {\n", "tag", tagString);
  printer->Indent();

  field_generators_.get(field).GenerateBuilderParsingCodeFromPacked(printer,
                                                                    target);

  printer->Outdent();
  printer->Print(
      "  break;\n"
      "} // case $tag$\n",
      "tag", tagString);
}

// ===================================================================

bool MessageBuilderGenerator::NeedsParserBuildPartialShard(int shard) const {
  for (int i = 0; i < descriptor_->field_count(); ++i) {
    const auto* field = descriptor_->field(i);
    if (field_generators_.get(field).GetBitIndex() / 32 == shard &&
        (field->is_repeated() || GetJavaType(field) == JAVATYPE_MESSAGE))
      return true;
  }
  return false;
}

void MessageBuilderGenerator::GenerateParserBuildPartialCalls(
    io::Printer* printer) {
  for (int shard = 0; shard < (descriptor_->field_count() + 31) / 32; ++shard) {
    if (!NeedsParserBuildPartialShard(shard)) continue;
    printer->Print("buildPartial_autosplit_$shard$(instance", "shard",
                   absl::StrCat(shard));
    for (const auto* field : ParserLocalFields(descriptor_)) {
      const auto& generator = field_generators_.get(field);
      if (generator.GetBitIndex() / 32 == shard) {
        printer->Print(", $local$", "local", generator.ParserLocalName());
      }
    }
    printer->Print(");\n");
  }
}

void MessageBuilderGenerator::GenerateParserHandoff(io::Printer* printer) {
  std::string arguments = "null";
  const auto locals = ParserLocalFields(descriptor_);
  if (!locals.empty()) {
    arguments = "new java.lang.Object[] {";
    for (const auto* field : locals) {
      if (field != locals.front()) arguments += ", ";
      arguments += field_generators_.get(field).ParserLocalName();
    }
    arguments += "}";
  }
  printer->Print(
      "if (parseFailure == null && pendingTag != 0) {\n"
      "  Builder builder = new Builder(instance, unknown, $locals$);\n"
      "  try {\n"
      "    if (builder.mergeFromPendingField(input, extensionRegistry, "
      "pendingTag)) {\n"
      "      builder.mergeFrom(input, extensionRegistry);\n"
      "    }\n"
      "  } catch (java.io.IOException e) {\n"
      "    com.google.protobuf.GeneratedMessageParser.throwParseException(e, "
      "builder.buildPartial());\n"
      "  }\n"
      "  return builder.buildPartial();\n"
      "}\n",
      "locals", arguments);
}

void MessageBuilderGenerator::GenerateBuilderPendingFieldParsing(
    io::Printer* printer) {
  printer->Print(
      "private boolean mergeFromPendingField(\n"
      "    com.google.protobuf.CodedInputStream input,\n"
      "    com.google.protobuf.ExtensionRegistryLite extensionRegistry, int "
      "tag)\n"
      "    throws java.io.IOException {\n"
      "  switch (tag) {\n");
  printer->Indent();
  printer->Indent();
  for (const auto* field : ParserChildFields(descriptor_)) {
    GenerateBuilderFieldParsingCase(printer, field, ParsingTarget::kBuilder,
                                    true);
  }
  printer->Print(
      "default: return super.parseUnknownField(input, extensionRegistry, "
      "tag);\n");
  printer->Outdent();
  printer->Outdent();
  printer->Print("  }\n}\n");
}

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
