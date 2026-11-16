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

bool HasOnlyMessageFields(const OneofDescriptor* oneof) {
  for (int i = 0; i < oneof->field_count(); ++i) {
    if (GetType(oneof->field(i)) != FieldDescriptor::TYPE_MESSAGE) return false;
  }
  return true;
}

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

bool HasBuilderStaticParser(const Descriptor* descriptor,
                            const Context* context) {
  if (!context->HasGeneratedMethods(descriptor) ||
      descriptor->extension_range_count() != 0)
    return false;
  for (int i = 0; i < descriptor->field_count(); ++i) {
    if (descriptor->field(i)->is_repeated() &&
        !google::protobuf::internal::IsOss())
      return false;
  }
  return true;
}
void MessageBuilderGenerator::Generate(io::Printer* printer) {
  auto vars = printer->WithVars(
      {{"static_parser",
        HasBuilderStaticParser(descriptor_, context_) ? "true" : "false"},
       {"parse_locals", "false"},
       {"field_access", "private "},
       {"unknown_receiver", ""},
       {"unknown_argument", ""}});
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
    if (HasBuilderStaticParser(descriptor_, context_)) {
      GenerateBuilderParsingState(printer);
      GenerateBuilderParsingMethods(printer, /*for_parse=*/true);
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
  bool need_maybe_force_builder_init = false;
  for (int i = 0; i < descriptor_->field_count(); i++) {
    if (descriptor_->field(i)->message_type() != nullptr &&
        !IsRealOneof(descriptor_->field(i)) &&
        HasHasbit(descriptor_->field(i))) {
      need_maybe_force_builder_init = true;
      break;
    }
  }

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

void MessageBuilderGenerator::GenerateBuildPartial(io::Printer* printer,
                                                   bool for_parse) {
  if (!for_parse) {
    printer->Print(
        "@java.lang.Override\n"
        "public $classname$ buildPartial() {\n",
        "classname", name_resolver_->GetImmutableClassName(descriptor_));
  }
  printer->Print("  $classname$ result = new $classname$($builder$);\n",
                 "classname",
                 name_resolver_->GetImmutableClassName(descriptor_), "builder",
                 for_parse ? "null" : "this");

  printer->Indent();

  // One buildPartial_autosplit_#() per from_bit_field
  int totalInts = (descriptor_->field_count() + 31) / 32;
  if (totalInts > 0) {
    for (int i = 0; i < totalInts; ++i) {
      printer->Print(
          "if ($bit_field_name$ != 0) { "
          "buildPartial_autosplit_$shard$(result",
          "bit_field_name", GetBitFieldName(i), "shard", absl::StrCat(i));
      if (HasBuilderStaticParser(descriptor_, context_)) {
        GenerateBuilderParsingArguments(printer, i);
      }
      printer->Print("); }\n");
    }
  }

  printer->Outdent();
  if (for_parse) {
    printer->Print(
        "  setUnknownFieldsForParsing(result, unknownFields == null\n"
        "      ? com.google.protobuf.UnknownFieldSet.getDefaultInstance()\n"
        "      : unknownFields.build());\n"
        "  if (failure != null) {\n"
        "    throw failure.setUnfinishedMessage(result);\n"
        "  }\n"
        "  return result;\n");
    return;
  }
  printer->Print(
      "  onBuilt();\n"
      "  return result;\n"
      "}\n"
      "\n");

  // Build all fields in shards organized by bitfield membership.
  for (int i = 0; i < totalInts; i++) {
    GenerateBuildPartialShard(printer, i);
  }
}

void MessageBuilderGenerator::GenerateBuildPartialShard(io::Printer* printer,
                                                        int shard) {
  bool is_static = HasBuilderStaticParser(descriptor_, context_);
  printer->Print(
      "private $static$void buildPartial_autosplit_$shard$($classname$ result",
      "static", is_static ? "static " : "", "classname",
      name_resolver_->GetImmutableClassName(descriptor_), "shard",
      absl::StrCat(shard));
  if (is_static) {
    GenerateBuilderParsingParameters(printer, shard);
  }
  printer->Print(
      ") {\n"
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
    field.GenerateBuildingCode(printer);
  }

  for (const auto& kv : oneof_generators_) {
    if (kv.second->HasScalarFields(shard)) {
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

void MessageBuilderGenerator::GenerateBuilderParsingArguments(
    io::Printer* printer, int shard) {
  printer->Print(", $bit_field$", "bit_field", GetBitFieldName(shard));
  int end = std::min((shard + 1) * 32, descriptor_->field_count());
  for (int i = shard * 32; i < end; ++i) {
    const auto& field = field_generators_.getInInsertOrder(i);
    if (!field.IsRealOneof()) {
      printer->Print(", $name$_", "name", field.GetName());
    }
    if (GetJavaType(field.descriptor()) == JAVATYPE_MESSAGE &&
        !field.descriptor()->is_map()) {
      printer->Print(", $name$Builder_", "name", field.GetName());
    }
  }
  for (const auto& kv : oneof_generators_) {
    if (kv.second->HasFieldsInShard(shard)) {
      printer->Print(", $name$Case_, $name$_", "name", kv.second->GetName());
    }
  }
}

void MessageBuilderGenerator::GenerateBuilderParsingParameters(
    io::Printer* printer, int shard) {
  auto vars =
      printer->WithVars({{"parse_locals", "true"}, {"field_access", ""}});
  printer->Print(", int $bit_field$", "bit_field", GetBitFieldName(shard));
  int end = std::min((shard + 1) * 32, descriptor_->field_count());
  printer->Indent();
  printer->Indent();
  for (int i = shard * 32; i < end; ++i) {
    const auto& field = field_generators_.getInInsertOrder(i);
    if (field.IsRealOneof() &&
        GetJavaType(field.descriptor()) != JAVATYPE_MESSAGE)
      continue;
    printer->Print(",\n");
    field.GenerateBuilderMembers(printer, /*initialize=*/false);
  }
  for (const auto& kv : oneof_generators_) {
    if (kv.second->HasFieldsInShard(shard)) {
      printer->Print(",\n");
      kv.second->GenerateCommonBuilderMethods(printer, /*initialize=*/false);
    }
  }
  printer->Outdent();
  printer->Outdent();
}

void MessageBuilderGenerator::GenerateBuilderParsingState(
    io::Printer* printer) {
  auto vars = printer->WithVars(
      {{"classname", name_resolver_->GetImmutableClassName(descriptor_)}});
  printer->Print(
      "private Builder(java.lang.Object unknownState) {\n"
      "  super(null, unknownState);\n"
      "}\n"
      "static Builder restoreState($classname$ other, boolean retained) {\n"
      "  Builder target = new Builder((java.lang.Object) other.getUnknownFields());\n");
  int shards = (descriptor_->field_count() + 31) / 32;
  bool has_children = false;
  for (int i = 0; i < descriptor_->field_count(); ++i) {
    has_children |= GetJavaType(descriptor_->field(i)) == JAVATYPE_MESSAGE;
  }
  for (int i = 0; i < shards; ++i) {
    printer->Print("  target.restorePublishedState_$shard$(other, retained);\n",
                   "shard", absl::StrCat(i));
  }
  printer->Print("  return target;\n}\n");
  for (int shard = 0; shard < shards; ++shard) {
    auto shard_vars =
        printer->WithVars({{"shard", absl::StrCat(shard)},
                           {"bit_field", GetBitFieldName(shard)}});
    int end = std::min((shard + 1) * 32, descriptor_->field_count());
    printer->Print(
        "private void restorePublishedState_$shard$(\n"
        "    $classname$ other, boolean retained) {\n");
    printer->Indent();
    for (const auto& kv : oneof_generators_) {
      if (kv.second->HasFieldsInShard(shard)) {
        kv.second->GenerateBuilderRestoreFromMessage(printer);
      }
    }
    for (int i = shard * 32; i < end; ++i) {
      field_generators_.getInInsertOrder(i).GenerateBuilderRestoreFromMessage(
          printer);
    }
    printer->Outdent();
    printer->Print("}\n");
    if (!has_children && oneof_generators_.empty()) continue;
    printer->Print(
        "private static void installParsingState_$shard$(Builder target");
    GenerateBuilderParsingParameters(printer, shard);
    printer->Print(") {\n  target.$bit_field$ = $bit_field$;\n");
    for (int i = shard * 32; i < end; ++i) {
      const auto& field = field_generators_.getInInsertOrder(i);
      if (!field.IsRealOneof()) {
        printer->Print("  target.$name$_ = $name$_;\n", "name",
                       field.GetName());
      }
      if (GetJavaType(field.descriptor()) == JAVATYPE_MESSAGE &&
          !field.descriptor()->is_map()) {
        printer->Print("  target.$name$Builder_ = $name$Builder_;\n", "name",
                       field.GetName());
      }
    }
    for (const auto& kv : oneof_generators_) {
      if (kv.second->HasFieldsInShard(shard)) {
        printer->Print(
            "  target.$name$Case_ = $name$Case_;\n"
            "  target.$name$_ = $name$_;\n",
            "name", kv.second->GetName());
      }
    }
    printer->Print("}\n");
  }
  int children = 0;
  for (int i = 0; i < descriptor_->field_count(); ++i) {
    const auto* field = descriptor_->field(i);
    if (!IsRealOneof(field) || GetJavaType(field) != JAVATYPE_MESSAGE) continue;
    if (children % 32 == 0) {
      if (children != 0) printer->Print("}\n");
      printer->Print("private void attachParsingChildren_$shard$() {\n",
                     "shard", absl::StrCat(children / 32));
    }
    printer->Indent();
    field_generators_.get(field).GenerateBuilderParsingChildAttachment(printer);
    printer->Outdent();
    ++children;
  }
  if (children != 0) printer->Print("}\n");
}

void MessageBuilderGenerator::GenerateBuilderParsingMethods(
    io::Printer* printer, bool for_parse) {
  auto vars = printer->WithVars(
      {{"parse_locals", for_parse ? "true" : "false"},
       {"field_access", for_parse ? "" : "private "},
       {"unknown_receiver", for_parse ? "unknownFields = " : ""},
       {"unknown_argument", for_parse ? "unknownFields, " : ""}});
  bool has_children = false;
  int oneof_children = 0;
  std::vector<const OneofDescriptor*> message_oneofs;
  for (int i = 0; i < descriptor_->field_count(); ++i) {
    const auto* field = descriptor_->field(i);
    if (GetJavaType(field) == JAVATYPE_MESSAGE) {
      has_children = true;
      if (IsRealOneof(field)) ++oneof_children;
    }
  }
  for (const auto& kv : oneof_generators_) {
    const auto* oneof = descriptor_->oneof_decl(kv.first);
    for (int i = 0; i < oneof->field_count(); ++i) {
      if (GetJavaType(oneof->field(i)) == JAVATYPE_MESSAGE) {
        message_oneofs.push_back(oneof);
        break;
      }
    }
  }
  bool continuation = HasBuilderStaticParser(descriptor_, context_) &&
                      (has_children || !oneof_generators_.empty());
  if (for_parse) {
    printer->Print(
        "private static $classname$ parsePartialFrom_(\n"
        "    com.google.protobuf.CodedInputStream input,\n"
        "    com.google.protobuf.ExtensionRegistryLite extensionRegistry)\n"
        "    throws com.google.protobuf.InvalidProtocolBufferException {\n",
        "classname", name_resolver_->GetImmutableClassName(descriptor_));
    if (continuation) {
      printer->Print(
          "  java.lang.Object state = parseState_(input, extensionRegistry);\n"
          "  return state instanceof $classname$ ? ($classname$) state\n"
          "      : ((Builder) state).buildPartial();\n",
          "classname", name_resolver_->GetImmutableClassName(descriptor_));
    } else {
      printer->Print(
          "  return ($classname$) parseState_(input, extensionRegistry);\n",
          "classname", name_resolver_->GetImmutableClassName(descriptor_));
    }
    printer->Print("}\n");
  }
  if (!for_parse && continuation) {
    printer->Print(
        "@java.lang.Override\n"
        "public Builder mergeFrom(\n"
        "    com.google.protobuf.CodedInputStream input,\n"
        "    com.google.protobuf.ExtensionRegistryLite extensionRegistry)\n"
        "    throws java.io.IOException {\n"
        "  return mergeFrom(input, extensionRegistry, 0);\n"
        "}\n");
  }
  if (for_parse && !message_oneofs.empty()) {
    printer->Print("@java.lang.SuppressWarnings(\"unchecked\")\n");
  }
  printer->Print(
      for_parse      ? "static java.lang.Object parseState_(\n"
      : continuation ? "private Builder mergeFrom(\n"
                     : "@java.lang.Override\npublic Builder mergeFrom(\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_));
  printer->Print(
      "    com.google.protobuf.CodedInputStream input,\n"
      "    com.google.protobuf.ExtensionRegistryLite extensionRegistry");
  if (!for_parse && continuation) {
    printer->Print(",\n    int pendingTag");
  }
  printer->Print(
      ")\n"
      "    throws $exception$ {\n"
      "  java.util.Objects.requireNonNull(extensionRegistry);\n",
      "exception",
      for_parse ? "com.google.protobuf.InvalidProtocolBufferException"
                : "java.io.IOException");
  if (for_parse) {
    printer->Indent();
    int total_ints = (descriptor_->field_count() + 31) / 32;
    for (int i = 0; i < total_ints; ++i) {
      printer->Print("int $bit_field$ = 0;\n", "bit_field", GetBitFieldName(i));
    }
    for (int i = 0; i < descriptor_->field_count(); ++i) {
      field_generators_.getInInsertOrder(i).GenerateBuilderMembers(
          printer, /*initialize=*/true);
    }
    for (const auto& kv : oneof_generators_) {
      kv.second->GenerateCommonBuilderMethods(printer, /*initialize=*/true);
    }
    printer->Print(
        "com.google.protobuf.UnknownFieldSet.Builder unknownFields = null;\n"
        "com.google.protobuf.InvalidProtocolBufferException failure = null;\n");
    if (!message_oneofs.empty()) printer->Print("int tag = 0;\n");
    if (continuation) {
      printer->Print("Builder continuation = null;\nint pendingTag = 0;\n");
    }
    printer->Outdent();
  }
  printer->Print("  try {\n");
  if (for_parse && continuation) {
    printer->Print(
        "    if (useFieldBuildersForParsing()) {\n"
        "      continuation = newBuilder();\n"
        "      continuation.mergeFrom(input, extensionRegistry);\n"
        "    }\n"
        "    boolean done = continuation != null;\n"
        "    parsing:\n");
  } else {
    printer->Print("    boolean done = false;\n");
  }
  printer->Print("    while (!done) {\n");
  if (!for_parse && continuation) {
    printer->Print(
        "      int tag;\n"
        "      if (pendingTag != 0) {\n"
        "        tag = pendingTag;\n"
        "        pendingTag = 0;\n"
        "      } else {\n"
        "        tag = input.readTag();\n"
        "      }\n");
  } else {
    printer->Print(for_parse && !message_oneofs.empty()
                       ? "      tag = 0;\n      tag = input.readTag();\n"
                       : "      int tag = input.readTag();\n");
  }
  if (for_parse) {
    for (const auto* oneof : message_oneofs) {
      if (!HasOnlyMessageFields(oneof)) continue;
      const auto* info = context_->GetOneofGeneratorInfo(oneof);
      printer->Print(
          "      if ($name$Case_ != 0 && (tag & 7) == 2\n"
          "          && $capitalized_name$Case.forNumber(tag >>> 3) != null) {\n"
          "        pendingTag = tag;\n"
          "        break parsing;\n"
          "      }\n",
          "name", info->name, "capitalized_name", info->capitalized_name);
    }
  }
  printer->Print(
      "      switch (tag) {\n"
      "        case 0:\n"  // zero signals EOF / limit reached
      "          done = true;\n"
      "          break;\n");
  printer->Indent();  // method
  printer->Indent();  // try
  printer->Indent();  // while
  printer->Indent();  // switch
  GenerateBuilderFieldParsingCases(printer);
  printer->Outdent();  // switch
  printer->Outdent();  // while
  printer->Outdent();  // try
  printer->Outdent();  // method
  printer->Print("        default: {\n");
  if (for_parse) {
    printer->Print(
        "          if (unknownFields == null) {\n"
        "            unknownFields = "
        "com.google.protobuf.UnknownFieldSet.newBuilder();\n"
        "          }\n");
  }
  printer->Print(
      "          if (!$unknown_field_decoder$) {\n", "unknown_field_decoder",
      for_parse ? "parseUnknownField(getDefaultInstance(),\n"
                  "              input, unknownFields, extensionRegistry, tag)"
                : "super.parseUnknownField(input, extensionRegistry, tag)");
  printer->Print(
      "            done = true; // was an endgroup tag\n"
      "          }\n"
      "          break;\n"
      "        } // default:\n"
      "      } // switch (tag)\n");
  if (for_parse) {
    for (const auto* oneof : message_oneofs) {
      printer->Print(
          "      if ($name$_ instanceof com.google.protobuf.SingleFieldBuilder) $name$_ = null;\n",
          "name", context_->GetOneofGeneratorInfo(oneof)->name);
    }
  }
  printer->Print("    } // while (!done)\n");
  if (for_parse && continuation) {
    printer->Indent();
    printer->Indent();
    printer->Print("if (pendingTag != 0");
    for (const auto* oneof : message_oneofs) {
      if (HasOnlyMessageFields(oneof)) {
        // Case changes leave the local phase before consuming the next child.
        printer->Print(" || ($name$Case_ != 0 && $name$_ == null)", "name",
                       context_->GetOneofGeneratorInfo(oneof)->name);
      } else {
        // Mixed oneofs can retain inactive message children after scalar cases.
        for (int i = 0; i < oneof->field_count(); ++i) {
          const auto* field = oneof->field(i);
          if (GetJavaType(field) == JAVATYPE_MESSAGE) {
            printer->Print(" || $name$Builder_ != null", "name",
                           context_->GetFieldGeneratorInfo(field)->name);
          }
        }
      }
    }
    printer->Print(
        ") {\n"
        "  continuation = new Builder((java.lang.Object) unknownFields);\n");
    int shards = (descriptor_->field_count() + 31) / 32;
    for (int i = 0; i < shards; ++i) {
      printer->Print("  installParsingState_$shard$(continuation", "shard",
                     absl::StrCat(i));
      GenerateBuilderParsingArguments(printer, i);
      printer->Print(");\n");
    }
    for (int i = 0; i < (oneof_children + 31) / 32; ++i) {
      printer->Print("  continuation.attachParsingChildren_$shard$();\n",
                     "shard", absl::StrCat(i));
    }
    printer->Print(
        "}\n"
        "if (pendingTag != 0) {\n"
        "  continuation.mergeFrom(input, extensionRegistry, pendingTag);\n"
        "}\n");
    printer->Outdent();
    printer->Outdent();
  }
  printer->Print(
      "  } catch (com.google.protobuf.InvalidProtocolBufferException e) {\n");
  if (for_parse) {
    for (const auto* oneof : message_oneofs) {
      printer->Print(
          "    if ((tag >>> 3) == $name$Case_\n"
          "        && $name$_ instanceof com.google.protobuf.MessageLite\n"
          "        && e.getUnfinishedMessage() != null) {\n"
          "      $name$_ = e.getUnfinishedMessage();\n"
          "    }\n",
          "name", context_->GetOneofGeneratorInfo(oneof)->name);
    }
    printer->Print(
        "    java.io.IOException cause = e.unwrapIOException();\n"
        "    failure = cause == e ? e\n"
        "        : new "
        "com.google.protobuf.InvalidProtocolBufferException(cause);\n"
        "  } catch (java.io.IOException e) {\n"
        "    failure = new "
        "com.google.protobuf.InvalidProtocolBufferException(e);\n"
        "  }\n");
    if (continuation) {
      printer->Print(
          "  if (continuation != null) {\n"
          "    if (failure != null) {\n"
          "      throw failure.setUnfinishedMessage(continuation.buildPartial());\n"
          "    }\n"
          "    return continuation;\n"
          "  }\n");
    }
    GenerateBuildPartial(printer, /*for_parse=*/true);
    printer->Print("}\n");
  } else {
    printer->Print(
        "    throw e.unwrapIOException();\n"
        "  } finally {\n"
        "    onChanged();\n"
        "  } // finally\n"
        "  return this;\n"
        "}\n");
  }
}

void MessageBuilderGenerator::GenerateBuilderFieldParsingCases(
    io::Printer* printer) {
  std::vector<const FieldDescriptor*> sorted_fields(
      SortFieldsByNumber(descriptor_));
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor* field = sorted_fields[i];
    GenerateBuilderFieldParsingCase(printer, field);
    if (field->is_packable()) {
      GenerateBuilderPackedFieldParsingCase(printer, field);
    }
  }
}

void MessageBuilderGenerator::GenerateBuilderFieldParsingCase(
    io::Printer* printer, const FieldDescriptor* field) {
  uint32_t tag = WireFormatLite::MakeTag(
      field->number(), WireFormat::WireTypeForFieldType(field->type()));
  std::string tagString = absl::StrCat(static_cast<int32_t>(tag));
  printer->Print("case $tag$: {\n", "tag", tagString);
  printer->Indent();

  if (printer->LookupVar("parse_locals") == "true") {
    std::string condition;
    if (IsRealOneof(field)) {
      if (!HasOnlyMessageFields(field->containing_oneof()) &&
          !(GetJavaType(field) == JAVATYPE_ENUM &&
            !SupportUnknownEnumValue(field))) {
        const auto& name =
            context_->GetOneofGeneratorInfo(field->containing_oneof())->name;
        condition = absl::StrCat(name, "Case_ != 0");
        if (GetJavaType(field) != JAVATYPE_MESSAGE) {
          absl::StrAppend(&condition, " && ", name, "Case_ != ", field->number());
        }
      }
    } else if (GetJavaType(field) == JAVATYPE_MESSAGE &&
               !field->is_repeated()) {
      const auto& name = context_->GetFieldGeneratorInfo(field)->name;
      condition = absl::StrCat(name, "_ != null || ", name, "Builder_ != null");
    }
    if (!condition.empty()) {
      printer->Print(
          "if ($condition$) {\n"
          "  pendingTag = tag;\n"
          "  break parsing;\n"
          "}\n",
          "condition", condition);
    }
  }
  printer->Emit(
      {{"clear_oneof_has_bits",
        [&] {
          oneof_generators_.at(field->containing_oneof()->index())
              ->GenerateBuilderClearOneofHasBits(
                  printer, true,
                  GetJavaType(field) == JAVATYPE_ENUM &&
                          !SupportUnknownEnumValue(field)
                      ? &field_generators_
                      : nullptr);
        }},
       {"parse_field",
        [&] {
          field_generators_.get(field).GenerateBuilderParsingCode(printer);
        }}},
      "$parse_field$");

  printer->Outdent();
  printer->Print(
      "  break;\n"
      "} // case $tag$\n",
      "tag", tagString);
}

void MessageBuilderGenerator::GenerateBuilderPackedFieldParsingCase(
    io::Printer* printer, const FieldDescriptor* field) {
  // To make packed = true wire compatible, we generate parsing code from a
  // packed version of this field regardless of field->options().packed().
  uint32_t tag = WireFormatLite::MakeTag(
      field->number(), WireFormatLite::WIRETYPE_LENGTH_DELIMITED);
  std::string tagString = absl::StrCat(static_cast<int32_t>(tag));
  printer->Print("case $tag$: {\n", "tag", tagString);
  printer->Indent();

  field_generators_.get(field).GenerateBuilderParsingCodeFromPacked(printer);

  printer->Outdent();
  printer->Print(
      "  break;\n"
      "} // case $tag$\n",
      "tag", tagString);
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
