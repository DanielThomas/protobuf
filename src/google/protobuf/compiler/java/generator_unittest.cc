// Protocol Buffers - Google's data interchange format
// Copyright 2023 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "google/protobuf/compiler/java/generator.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/compiler/command_line_interface_tester.h"
#include "google/protobuf/compiler/java/java_features.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/testing/file.h"

namespace google {
namespace protobuf {
namespace compiler {
namespace java {
namespace {

#define PACKAGE_PREFIX ""
#define PACKAGE_IMPORT_PREFIX ""

class JavaGeneratorTest : public CommandLineInterfaceTester {
 protected:
  JavaGeneratorTest() {
    RegisterGenerator("--java_out", "--java_opt",
                      std::make_unique<JavaGenerator>(), "Java test generator");

    // Generate built-in protos.
    CreateTempFile(
        "google/protobuf/descriptor.proto",
        google::protobuf::DescriptorProto::descriptor()->file()->DebugString());
    CreateTempFile("third_party/java/protobuf/java_features.proto",
                   pb::JavaFeatures::descriptor()->file()->DebugString());
  }

  std::string DecoderBody(absl::string_view filename,
                          absl::string_view signature, int occurrence = 0) {
    std::string contents;
    EXPECT_TRUE(File::GetContents(absl::StrCat(temp_directory(), "/", filename),
                                  &contents, true)
                    .ok());
    auto method = contents.find(signature);
    while (occurrence-- > 0 && method != std::string::npos) {
      method = contents.find(signature, method + 1);
    }
    EXPECT_NE(method, std::string::npos) << signature;
    if (method == std::string::npos) return "";
    const auto begin = contents.find('{', method);
    int depth = 1;
    auto end = begin + 1;
    while (end < contents.size() && depth != 0) {
      if (contents[end] == '{') ++depth;
      if (contents[end] == '}') --depth;
      ++end;
    }
    std::string body = absl::StrReplaceAll(
        contents.substr(begin, end - begin),
        {{"com.google.protobuf.GeneratedMessageParser.", ""}});
    body = absl::StrReplaceAll(
        body,
        {{"instance.", ""},
         {"internalGetChildFieldBuilder()", "childBuilder_"},
         {"internalGetSelectedFieldBuilder()", "selectedBuilder_"},
         {"internalGetGroupFieldBuilder()", "groupBuilder_"},
         {"clearChoiceHasBits(instance)", "clearChoiceHasBits()"},
         {"ensureChildrenIsMutable(instance)", "ensureChildrenIsMutable()"},
         {"onChanged();", ""},
         {"getParentForChildren()", "null"},
         {"isClean()", "false"},
         {"(instance, ", "("},
         {"(instance)", "()"},
         {"unknown = mergeUnknownVarintField(unknown, ",
          "mergeUnknownVarintField("},
         {"unknown = mergeUnknownLengthDelimitedField(unknown, ",
          "mergeUnknownLengthDelimitedField("},
         {"return unknown;", ""}});
    body.erase(std::remove_if(body.begin(), body.end(),
                              [](char c) {
                                return c == ' ' || c == '\n' || c == '\r' ||
                                       c == '\t';
                              }),
               body.end());
    return body;
  }

  std::string MapDecoderBody(absl::string_view filename,
                             absl::string_view message_name,
                             absl::string_view name,
                             absl::string_view capitalized_name, int tag,
                             bool parser, bool message_value) {
    const std::string accessor =
        absl::StrCat("internalGetMutable", capitalized_name);
    const std::string storage = absl::StrCat(name, "_");
    const auto normalize = [&](absl::string_view body) {
      return absl::StrReplaceAll(body,
                                 {{absl::StrCat("parsed_", name), storage},
                                  {absl::StrCat("Builder.", name, "Converter"),
                                   absl::StrCat(name, "Converter")}});
    };
    std::string body = normalize(
        DecoderBody(filename, absl::StrCat("case ", tag, ":"), parser ? 1 : 0));
    std::string initializer = normalize(DecoderBody(
        filename, absl::StrCat(accessor, "(",
                               parser ? absl::StrCat(message_name, " instance")
                                      : ") {")));
    if (initializer.size() < 2) return "";
    initializer =
        absl::StrReplaceAll(initializer.substr(1, initializer.size() - 2),
                            {{absl::StrCat("return", storage, ";"), ""}});
    if (parser && message_value) {
      body = absl::StrReplaceAll(
          body, {{absl::StrCat("(", storage, "=", accessor, "(", storage, "))"),
                  absl::StrCat(accessor, "()")}});
    }
    return absl::StrReplaceAll(body, {{absl::StrCat(accessor, "()"),
                                       absl::StrCat(initializer, storage)}});
  }

  bool FileGenerated(absl::string_view filename) {
    std::string path = absl::StrCat(temp_directory(), "/", filename);
    return File::Exists(path);
  }

  bool FileContainsSubstring(absl::string_view filename,
                             absl::string_view substring) {
    std::string path = absl::StrCat(temp_directory(), "/", filename);
    std::string contents;
    if (!File::GetContents(path, &contents, true).ok()) {
      return false;
    }
    return contents.find(substring) != std::string::npos;
  }
};

TEST_F(JavaGeneratorTest, Basic) {
  CreateTempFile("foo.proto",
                 R"schema(
    syntax = "proto2";
    message Foo {
      optional int32 bar = 1;
    })schema");

  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir foo.proto");

  ExpectNoErrors();
}

TEST_F(JavaGeneratorTest, BasicError) {
  CreateTempFile("foo.proto",
                 R"schema(
    syntax = "proto2";
    message Foo {
      int32 bar = 1;
    })schema");

  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir foo.proto");

  ExpectErrorSubstring(
      "foo.proto:4:7: Expected \"required\", \"optional\", or \"repeated\"");
}

TEST_F(JavaGeneratorTest, ImplicitPresenceLegacyClosedEnumDisallowed) {
  CreateTempFile("foo.proto",
                 R"schema(
    edition = "2023";
    import "third_party/java/protobuf/java_features.proto";
    option features.field_presence = IMPLICIT;
    enum Bar {
      AAA = 0;
    }
    message Foo {
      Bar bar = 1 [features.(pb.java).legacy_closed_enum = true];
    }
  )schema");

  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir foo.proto");

  ExpectErrorSubstring(
      "foo.proto: Field Foo.bar has a closed enum type with implicit "
      "presence.");
}

TEST_F(JavaGeneratorTest, NestInFileClassFeatureDefaultEdition2024) {
  CreateTempFile("foo.proto",
                 R"schema(
      edition = "2024";
      package proto2_unittest;
      option java_generic_services = true;
      message MessageA {
        int32 unused = 1;
        message NestedMessageA {
          int32 unused = 1;
        }

        enum NestedEnumA {
          FOO_DEFAULT = 0;
          FOO_VALUE = 1;
        }
      }
      service MessageB {
        rpc Method(MessageA) returns (MessageA) {}
      }
  )schema");

  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "--experimental_editions foo.proto");

  ExpectWarningSubstring("Generic services");
  EXPECT_TRUE(FileGenerated(PACKAGE_PREFIX "proto2_unittest/FooProto.java"));
  EXPECT_TRUE(FileGenerated(PACKAGE_PREFIX "proto2_unittest/MessageA.java"));
  EXPECT_TRUE(FileGenerated(PACKAGE_PREFIX "proto2_unittest/MessageB.java"));
  EXPECT_FALSE(
      FileGenerated(PACKAGE_PREFIX "proto2_unittest/NestedMessageA.java"));
  EXPECT_FALSE(
      FileGenerated(PACKAGE_PREFIX "proto2_unittest/NestedEnumA.java"));
}

TEST_F(JavaGeneratorTest, NestInFileClassFeatureInNetsedMessageError) {
  CreateTempFile("foo.proto",
                 R"schema(
      edition = "2024";
      package proto2_unittest;
      import "third_party/java/protobuf/java_features.proto";
      message Message {
        int32 unused = 1;
        message NestedMessage {
          option features.(pb.java).nest_in_file_class = YES;
          int32 unused = 1;
        }
      }
  )schema");

  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "--experimental_editions foo.proto");

  ExpectErrorSubstring(
      "Feature pb.java.nest_in_file_class only applies to top-level types and "
      "is not allowed to be set on the nested type: "
      "proto2_unittest.Message.NestedMessage");
}

TEST_F(JavaGeneratorTest, NestInFileClassFeatureInNetsedEnumError) {
  CreateTempFile("foo.proto",
                 R"schema(
      edition = "2024";
      package proto2_unittest;
      import "third_party/java/protobuf/java_features.proto";
      message Message {
        int32 unused = 1;
        enum NestedEnum {
          option features.(pb.java).nest_in_file_class = YES;
          FOO_DEFAULT = 0;
          FOO_VALUE = 1;
        }
      }
  )schema");

  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "--experimental_editions foo.proto");

  ExpectErrorSubstring(
      "Feature pb.java.nest_in_file_class only applies to top-level types and "
      "is not allowed to be set on the nested type: "
      "proto2_unittest.Message.NestedEnum");
}

TEST_F(JavaGeneratorTest, SplitNestInFileClassMessageFeatureEdition2024) {
  CreateTempFile("foo.proto",
                 R"schema(
      edition = "2024";
      import "third_party/java/protobuf/java_features.proto";
      package proto2_unittest;
      message NestedInFileClassMessage {
        option features.(pb.java).nest_in_file_class = YES;
        int32 unused = 1;
      }
      message UnnestedMessage {
        int32 unused = 1;
      }
  )schema");

  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "--experimental_editions foo.proto");

  ExpectNoErrors();
  EXPECT_TRUE(FileGenerated(PACKAGE_PREFIX "proto2_unittest/FooProto.java"));
  EXPECT_TRUE(
      FileGenerated(PACKAGE_PREFIX "proto2_unittest/UnnestedMessage.java"));
  EXPECT_FALSE(FileGenerated(PACKAGE_PREFIX
                             "proto2_unittest/NestedInFileClassMessage.java"));
}

TEST_F(JavaGeneratorTest, SplitNestInFileClassServiceFeatureEdition2024) {
  CreateTempFile("foo.proto",
                 R"schema(
      edition = "2024";
      import "third_party/java/protobuf/java_features.proto";
      package proto2_unittest;
      option java_generic_services = true;
      message Dummy {}
      service NestedInFileClassService {
        option features.(pb.java).nest_in_file_class = YES;
        rpc Method(Dummy) returns (Dummy) {}
      }
      service UnnestedService {
        rpc Method(Dummy) returns (Dummy) {}
      }
  )schema");

  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "--experimental_editions foo.proto");

  ExpectWarningSubstring("Generic services");
  EXPECT_TRUE(FileGenerated(PACKAGE_PREFIX "proto2_unittest/FooProto.java"));
  EXPECT_TRUE(
      FileGenerated(PACKAGE_PREFIX "proto2_unittest/UnnestedService.java"));
  EXPECT_FALSE(FileGenerated(PACKAGE_PREFIX
                             "proto2_unittest/NestedInFileClassService.java"));
}

TEST_F(JavaGeneratorTest, SplitNestInFileClassEnumFeatureEdition2024) {
  CreateTempFile("foo.proto",
                 R"schema(
      edition = "2024";
      import "third_party/java/protobuf/java_features.proto";
      package proto2_unittest;
      enum NestedInFileClassEnum {
        option features.(pb.java).nest_in_file_class = YES;

        FOO_DEFAULT = 0;
        FOO_VALUE = 1;
      }

      enum UnnestedEnum {
        BAR_DEFAULT = 0;
        BAR_VALUE = 1;
      }
  )schema");

  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "--experimental_editions foo.proto");

  ExpectNoErrors();
  EXPECT_TRUE(FileGenerated(PACKAGE_PREFIX "proto2_unittest/FooProto.java"));
  EXPECT_TRUE(
      FileGenerated(PACKAGE_PREFIX "proto2_unittest/UnnestedEnum.java"));
  EXPECT_FALSE(FileGenerated(PACKAGE_PREFIX
                             "proto2_unittest/NestedInFileClassEnum.java"));
}

TEST_F(JavaGeneratorTest, LargeClosedEnumDisallowedEdition2024) {
  CreateTempFile("foo.proto",
                 R"schema(
    edition = "2024";

    import "third_party/java/protobuf/java_features.proto";

    option features.enum_type = CLOSED;

    enum Bar {
      option features.(pb.java).large_enum = true;

      AAA = 0;
      BBB = 1;
    }
  )schema");

  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir foo.proto "
      "--experimental_editions");

  ExpectErrorSubstring(
      "foo.proto: Bar is a closed enum and can not be used with the large_enum "
      "feature.  Please migrate to an open enum first, which is a better fit "
      "for extremely large enums.");
}

TEST_F(JavaGeneratorTest, LargeOpenEnumAllowedEdition2024) {
  CreateTempFile("foo.proto",
                 R"schema(
    edition = "2024";

    import "third_party/java/protobuf/java_features.proto";

    enum Bar {
      option features.(pb.java).large_enum = true;

      AAA = 0;
      BBB = 1;
    }
  )schema");

  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "foo.proto "
      "--experimental_editions");

  ExpectNoErrors();
}
TEST_F(JavaGeneratorTest, LargeEnumDisallowedEdition2023) {
  CreateTempFile("foo.proto",
                 R"schema(
edition = "2023";

import "third_party/java/protobuf/java_features.proto";

enum Bar {
option features.(pb.java).large_enum = true;

AAA = 0;
BBB = 1;
}
)schema");

  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir foo.proto "
      "--experimental_editions");

  ExpectErrorSubstring(
      "foo.proto:6:6: pb.JavaFeatures.large_enum wasn't introduced "
      "until edition 2024 and can't be used in edition 2023");
}

TEST_F(JavaGeneratorTest,
       InvalidConflictingProtoSuffixedMessageNameEdition2024) {
  CreateTempFile("test_file_name.proto",
                 R"schema(
      edition = "2024";
      package foo;
      message TestFileNameProto {
        int32 field = 1;
      }
      )schema");

  RunProtoc(
      "protocol_compiler --experimental_editions --java_out=$tmpdir "
      "-I$tmpdir test_file_name.proto");

  ExpectErrorSubstring(
      "Cannot generate Java output because the file's outer "
      "class name, \"TestFileNameProto\", matches the name "
      "of one of the types declared inside it");
}

TEST_F(JavaGeneratorTest, InvalidConflictingNestedEnumName) {
  CreateTempFile("test_file_name.proto",
                 R"schema(
      edition = "2024";
      package foo;
      message TestNameConflict {
        enum TestNameConflict {
          UNSPECIFIED = 0;
        }
      }
      )schema");

  RunProtoc(
      "protocol_compiler --experimental_editions --java_out=$tmpdir "
      "-I$tmpdir test_file_name.proto");

  ExpectErrorSubstring(
      "would be an enum nested inside a class with the same name");
}

TEST_F(JavaGeneratorTest, ExtensionsOptionImportsAreUnknown) {
  CreateTempFile("custom_option.proto", R"schema(
      edition = "2024";
      package foo;
      import "google/protobuf/descriptor.proto";
      extend google.protobuf.FileOptions {
        int32 option_import_file_opt = 9998;
      }
      extend google.protobuf.MessageOptions {
        int32 option_import_message_opt = 9998;
      }
      extend google.protobuf.FieldOptions {
        int32 option_import_field_opt = 9998;
      }
      )schema");
  CreateTempFile("foo.proto", R"schema(
      edition = "2024";
      package foo;
      import "google/protobuf/descriptor.proto";
      import option "custom_option.proto";

      // Should be an unknown custom option for the Java generator if not polluted by pool.
      option (option_import_file_opt) = 123;
      option (file_opt) = 123;

      message Foo {
        option (option_import_message_opt) = 456;
        option (message_opt) = 456;
        int32 bar = 1 [(option_import_field_opt) = 789, (field_opt) = 789];
      }
      extend google.protobuf.FileOptions {
        int32 file_opt = 9999;
      }
      extend google.protobuf.MessageOptions {
        int32 message_opt = 9999;
      }
      extend google.protobuf.FieldOptions {
        int32 field_opt = 9999;
      }
      )schema");

  RunProtoc(
      "protocol_compiler --java_out=$tmpdir --experimental_editions "
      "-I$tmpdir foo.proto");

  ExpectNoErrors();
  EXPECT_TRUE(FileGenerated(PACKAGE_PREFIX "foo/FooProto.java"));

  EXPECT_FALSE(
      FileContainsSubstring(PACKAGE_PREFIX "foo/FooProto.java",
                            "foo.CustomOptionProto.optionImportFileOpt"));
  EXPECT_FALSE(
      FileContainsSubstring(PACKAGE_PREFIX "foo/FooProto.java",
                            "foo.CustomOptionProto.optionImportMessageOpt"));
  EXPECT_FALSE(
      FileContainsSubstring(PACKAGE_PREFIX "foo/FooProto.java",
                            "foo.CustomOptionProto.optionImportFieldOpt"));

  EXPECT_TRUE(
      FileContainsSubstring(PACKAGE_PREFIX "foo/FooProto.java",
                            absl::StrCat("registry.add(", PACKAGE_IMPORT_PREFIX,
                                         "foo.FooProto.fileOpt);")));
  EXPECT_TRUE(
      FileContainsSubstring(PACKAGE_PREFIX "foo/FooProto.java",
                            absl::StrCat("registry.add(", PACKAGE_IMPORT_PREFIX,
                                         "foo.FooProto.messageOpt);")));
  EXPECT_TRUE(
      FileContainsSubstring(PACKAGE_PREFIX "foo/FooProto.java",
                            absl::StrCat("registry.add(", PACKAGE_IMPORT_PREFIX,
                                         "foo.FooProto.fieldOpt);")));
}


TEST_F(JavaGeneratorTest, ExtensionsPublicImportsAreKnown) {
  CreateTempFile("custom_option.proto", R"schema(
      edition = "2024";
      package foo;
      import "google/protobuf/descriptor.proto";
      extend google.protobuf.FileOptions {
        int32 transitive_public_import_file_opt = 9997;
      }
      )schema");
  CreateTempFile("import_public2.proto", R"schema(
      edition = "2024";
      package foo;
      import "google/protobuf/descriptor.proto";
      import public "custom_option.proto";
      extend google.protobuf.FileOptions {
        int32 public_import_file_opt = 9998;
      }
      )schema");
  CreateTempFile("import_public1.proto", R"schema(
      edition = "2024";
      package foo;
      import public "import_public1.proto";
      message MyMessage {}
      )schema");
  CreateTempFile("foo.proto", R"schema(
      edition = "2024";
      package foo;
      import "google/protobuf/descriptor.proto";
      import public "import_public2.proto";

      option (transitive_public_import_file_opt) = 123;
      option (public_import_file_opt) = 123;
      option (file_opt) = 123;

      extend google.protobuf.FileOptions {
        int32 file_opt = 9999;
      }
      )schema");
  RunProtoc(
      "protocol_compiler --java_out=$tmpdir --experimental_editions -I$tmpdir "
      "foo.proto");
  ExpectNoErrors();
  EXPECT_TRUE(FileGenerated(PACKAGE_PREFIX "foo/FooProto.java"));
  EXPECT_TRUE(FileContainsSubstring(
      PACKAGE_PREFIX "foo/FooProto.java",
      absl::StrCat("registry.add(", PACKAGE_IMPORT_PREFIX,
                   "foo.ImportPublic2Proto.publicImportFileOpt);")));
  EXPECT_TRUE(FileContainsSubstring(
      PACKAGE_PREFIX "foo/FooProto.java",
      absl::StrCat("registry.add(", PACKAGE_IMPORT_PREFIX,
                   "foo.CustomOptionProto.transitivePublicImportFileOpt);")));
  EXPECT_TRUE(
      FileContainsSubstring(PACKAGE_PREFIX "foo/FooProto.java",
                            absl::StrCat("registry.add(", PACKAGE_IMPORT_PREFIX,
                                         "foo.FooProto.fileOpt);")));
}


TEST_F(JavaGeneratorTest, OneofHasBitsBuildPartial) {
  CreateTempFile("oneof_has_bits.proto",
                 R"schema(
    syntax = "proto2";
    package com.google.protos;
    option java_multiple_files = true;
    message SubMsg {
      optional int32 x = 1;
    }
    message TestOneofHasBits {
      optional int32 regular_int = 1;
      oneof my_oneof {
        int32 oneof_int = 2;
        string oneof_str = 3;
        SubMsg oneof_msg = 4;
      }
    })schema");

  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "oneof_has_bits.proto");

  ExpectNoErrors();
  EXPECT_TRUE(
      FileGenerated(PACKAGE_PREFIX "com/google/protos/TestOneofHasBits.java"));
  // Verify message typed field in oneof is treated like regular message typed
  // fields (checked via from_bitField0_ & 0x00000008).
  EXPECT_TRUE(FileContainsSubstring(PACKAGE_PREFIX
                                    "com/google/protos/TestOneofHasBits.java",
                                    "((from_bitField0_ & 0x00000008) != 0)"));
  // Verify non-message oneof fields are grouped together by bitField
  // membership (0x00000002 | 0x00000004 = 0x00000006).
  EXPECT_TRUE(FileContainsSubstring(
      PACKAGE_PREFIX "com/google/protos/TestOneofHasBits.java",
      "if ((from_bitField0_ & 0x00000006) != 0) {"));
  // Verify clear<FieldName>() clears the individual has-bit.
  EXPECT_TRUE(FileContainsSubstring(
      PACKAGE_PREFIX "com/google/protos/TestOneofHasBits.java",
      "public Builder clearOneofInt() {\n"
      "      if (myOneofCase_ == 2) {\n"
      "        myOneofCase_ = 0;\n"
      "        bitField0_ = (bitField0_ & ~0x00000002);\n"
      "        myOneof_ = null;\n"
      "        onChanged();\n"
      "      }\n"
      "      return this;\n"
      "    }"));
  EXPECT_TRUE(FileContainsSubstring(
      PACKAGE_PREFIX "com/google/protos/TestOneofHasBits.java",
      "public Builder clearOneofStr() {\n"
      "      if (myOneofCase_ == 3) {\n"
      "        myOneofCase_ = 0;\n"
      "        bitField0_ = (bitField0_ & ~0x00000004);\n"
      "        myOneof_ = null;\n"
      "        onChanged();\n"
      "      }\n"
      "      return this;\n"
      "    }"));
  EXPECT_TRUE(FileContainsSubstring(
      PACKAGE_PREFIX "com/google/protos/TestOneofHasBits.java",
      "public Builder clearOneofMsg() {\n"
      "      if (oneofMsgBuilder_ == null) {\n"
      "        if (myOneofCase_ == 4) {\n"
      "          myOneofCase_ = 0;\n"
      "          bitField0_ = (bitField0_ & ~0x00000008);\n"
      "          myOneof_ = null;\n"
      "          onChanged();\n"
      "        }\n"
      "      } else {\n"
      "        if (myOneofCase_ == 4) {\n"
      "          myOneofCase_ = 0;\n"
      "          bitField0_ = (bitField0_ & ~0x00000008);\n"
      "          myOneof_ = null;\n"
      "        }\n"
      "        oneofMsgBuilder_.clear();\n"
      "      }\n"
      "      return this;\n"
      "    }"));
  // Verify parse<FieldName>() methods encapsulate oneof parsing and
  // bookkeeping.
  EXPECT_TRUE(FileContainsSubstring(
      PACKAGE_PREFIX "com/google/protos/TestOneofHasBits.java",
      "private void parseOneofInt(\n"
      "        com.google.protobuf.CodedInputStream input)\n"
      "        throws java.io.IOException {\n"
      "      myOneof_ = input.readInt32();\n"
      "      switch (myOneofCase_) {\n"
      "      default:\n"
      "        clearMyOneofHasBits(); // fallthrough\n"
      "      case 0:\n"
      "        myOneofCase_ = 2;\n"
      "        bitField0_ |= 0x00000002; // fallthrough\n"
      "      case 2:\n"
      "        break;\n"
      "      }\n"
      "    }"));
  EXPECT_TRUE(FileContainsSubstring(
      PACKAGE_PREFIX "com/google/protos/TestOneofHasBits.java",
      "private void parseOneofStr(\n"
      "        com.google.protobuf.CodedInputStream input)\n"
      "        throws java.io.IOException {"));
  EXPECT_TRUE(FileContainsSubstring(
      PACKAGE_PREFIX "com/google/protos/TestOneofHasBits.java",
      "private void parseOneofMsg(\n"
      "        com.google.protobuf.CodedInputStream input,\n"
      "        com.google.protobuf.ExtensionRegistryLite extensionRegistry)\n"
      "        throws java.io.IOException {"));
  EXPECT_TRUE(FileContainsSubstring(PACKAGE_PREFIX
                                    "com/google/protos/TestOneofHasBits.java",
                                    "parseOneofInt(input);"));
  EXPECT_TRUE(FileContainsSubstring(PACKAGE_PREFIX
                                    "com/google/protos/TestOneofHasBits.java",
                                    "parseOneofStr(input);"));
  EXPECT_TRUE(FileContainsSubstring(
      PACKAGE_PREFIX "com/google/protos/TestOneofHasBits.java",
      "parseOneofMsg(input, extensionRegistry);"));
}

TEST_F(JavaGeneratorTest, DirectParsingConstructsMessagesWithoutBuilders) {
  std::string schema = R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    message Scalar {
      optional int32 number = 1;
    }
    message Repeated {
      repeated int32 numbers = 1;
      repeated string names = 2;
      repeated bytes payloads = 3;
      repeated Scalar children = 4;
    }
    message Parent {
      optional Scalar child = 1;
      oneof selection {
        Scalar selected = 2;
        int32 number = 3;
      }
    }
    message Extendable {
      extensions 100 to max;
    }
    message WideParent {
  )schema";
  for (int i = 1; i <= 65; ++i) {
    absl::StrAppend(&schema, "optional Scalar child_", i, " = ", i, ";\n");
  }
  absl::StrAppend(&schema, "}");
  CreateTempFile("parser_entry.proto", schema);
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "parser_entry.proto");
  ExpectNoErrors();
  for (const char* name :
       {"Scalar", "Repeated", "Parent", "WideParent", "Extendable"}) {
    SCOPED_TRACE(name);
    const std::string file = absl::StrCat(name, ".java");
    EXPECT_TRUE(FileContainsSubstring(file, "PARSER = new MessageParser();"));
    EXPECT_TRUE(FileContainsSubstring(
        file, "private static final class MessageParser"));
    EXPECT_TRUE(FileContainsSubstring(
        file,
        absl::StrCat("extends com.google.protobuf.GeneratedMessageParser<",
                     name, ">")));
    EXPECT_TRUE(FileContainsSubstring(
        file,
        absl::StrCat(name, " instance = new ", name, "((Builder) null);")));
    EXPECT_TRUE(FileContainsSubstring(
        file,
        "static {\n"
        "      java.lang.reflect.Constructor<?>[] unused =\n"
        "          Builder.class.getDeclaredConstructors();\n"
        "    }"));
  }
  EXPECT_TRUE(FileContainsSubstring(
      "Parent.java",
      "parseChildField(instance, input, extensionRegistry, childBuilder_);"));
  EXPECT_TRUE(FileContainsSubstring(
      "Extendable.java",
      "Builder builder = new Builder(instance, unknown, null);"));
  EXPECT_TRUE(
      FileContainsSubstring("Extendable.java", "super(other, unknown);"));
  EXPECT_TRUE(FileContainsSubstring("Parent.java", "childBuilder_ = null;"));
  EXPECT_TRUE(
      FileContainsSubstring("WideParent.java", "child65Builder_ = null;"));
  for (const char* name : {"Names", "Children"}) {
    EXPECT_TRUE(FileContainsSubstring(
        "Repeated.java", absl::StrCat("ensure", name, "IsMutable(instance);")));
  }
  EXPECT_TRUE(FileContainsSubstring(
      "Repeated.java",
      "ensurePayloadsIsMutable(instance, /* capacity= */ -1);"));
  EXPECT_TRUE(FileContainsSubstring(
      "Parent.java",
      "selectedBuilder_ = internalGetSelectedFieldBuilder(instance, "
      "selectedBuilder_);"));
  EXPECT_TRUE(FileContainsSubstring("Parent.java",
                                    "switch (instance.selectionCase_) {"));
  EXPECT_TRUE(FileContainsSubstring(
      "Parent.java",
      "buildPartial_autosplit_0(instance, childBuilder_, selectedBuilder_);"));
  EXPECT_TRUE(FileContainsSubstring("Parent.java",
                                    "parseSelected(instance, input, "
                                    "extensionRegistry, selectedBuilder_);"));
  EXPECT_TRUE(FileContainsSubstring("Parent.java",
                                    "childBuilder_ = adoptFieldBuilder("));
  EXPECT_TRUE(FileContainsSubstring("WideParent.java",
                                    "parseChild65Field(instance, input, "
                                    "extensionRegistry, child65Builder_);"));

  CreateTempFile("parser_class_name.proto", R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    option java_outer_classname = "MessageParser___";
    message MessageParser {
      message MessageParser_ {}
      enum MessageParser__ {
        ZERO = 0;
      }
    })schema");
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "parser_class_name.proto");
  ExpectNoErrors();
  EXPECT_TRUE(FileContainsSubstring("MessageParser.java",
                                    "PARSER = new MessageParser____();"));
  EXPECT_TRUE(FileContainsSubstring(
      "MessageParser.java", "private static final class MessageParser____"));
}

TEST_F(JavaGeneratorTest,
       ParserLowersBuilderChildStorageWithoutForkingDecoders) {
  CreateTempFile("lowering.proto", R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    message Lowering {
      optional Lowering child = 1;
      oneof choice { Lowering selected = 2; int32 number = 3; }
      optional group Group = 4 { optional int32 value = 1; }
      repeated Lowering children = 5;
    }
    message RepeatedLowering { repeated Lowering children = 1; }
  )schema");
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "lowering.proto");
  ExpectNoErrors();
  for (const char* name : {"Child", "Group"}) {
    SCOPED_TRACE(name);
    EXPECT_EQ(
        DecoderBody("Lowering.java",
                    absl::StrCat("private void parse", name, "Field(")),
        DecoderBody("Lowering.java",
                    absl::StrCat("private static void parse", name, "Field(")));
  }
  EXPECT_EQ(DecoderBody("Lowering.java", "private void parseSelected("),
            DecoderBody("Lowering.java", "private static void parseSelected("));
  EXPECT_EQ(DecoderBody("RepeatedLowering.java", "case 10:", 0),
            DecoderBody("RepeatedLowering.java", "case 10:", 1));
  for (const char* name : {"Child", "Selected", "Group", "Children"}) {
    SCOPED_TRACE(name);
    EXPECT_EQ(DecoderBody("Lowering.java", absl::StrCat("internalGet", name,
                                                        "FieldBuilder() {")),
              DecoderBody("Lowering.java",
                          absl::StrCat("internalGet", name,
                                       "FieldBuilder(Lowering instance, ")));
  }
  EXPECT_TRUE(FileContainsSubstring(
      "RepeatedLowering.java", "LoweringOrBuilder> childrenBuilder_ = null;"));
  for (const char* field : {"child", "selected", "group"}) {
    EXPECT_TRUE(FileContainsSubstring("Lowering.java",
                                      absl::StrCat(field, "Builder_ = null;")));
  }
  for (const char* removed :
       {"readMessageForMerge", "readGroupForMerge", "parseForMerge",
        "prepareChildren", "retryTag:", "newFieldBuilderForParsing"}) {
    EXPECT_FALSE(FileContainsSubstring("Lowering.java", removed)) << removed;
  }
}

TEST_F(JavaGeneratorTest, ParserLowersScalarAndCollectionDecoders) {
  CreateTempFile("value_lowering.proto", R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    message ValueLowering {
      enum State { ZERO = 0; }
      optional int32 number = 1;
      optional string text = 2;
      optional bytes payload = 3;
      optional State state = 4;
      repeated int32 numbers = 5 [packed = true];
      repeated string names = 6;
      repeated State states = 7 [packed = true];
      repeated bytes payloads = 8;
      oneof choice {
        int32 selected_number = 9;
        string selected_text = 10;
        State selected_state = 11;
      }
    }
  )schema");
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "value_lowering.proto");
  ExpectNoErrors();
  for (int tag : {8, 18, 26, 32, 40, 42, 50, 56, 58, 66}) {
    SCOPED_TRACE(tag);
    const std::string marker = absl::StrCat("case ", tag, ":");
    EXPECT_EQ(DecoderBody("ValueLowering.java", marker, 0),
              DecoderBody("ValueLowering.java", marker, 1));
  }
  for (const char* name : {"SelectedNumber", "SelectedText", "SelectedState"}) {
    SCOPED_TRACE(name);
    const std::string type = std::string(name) == "SelectedState"
                                 ? "com.google.protobuf.UnknownFieldSet.Builder"
                                 : "void";
    EXPECT_EQ(
        DecoderBody("ValueLowering.java",
                    absl::StrCat("private void parse", name, "(")),
        DecoderBody("ValueLowering.java", absl::StrCat("private static ", type,
                                                       " parse", name, "(")));
  }
}

TEST_F(JavaGeneratorTest, ParserLowersBuilderCollectionHelpersAndPublication) {
  CreateTempFile("publication.proto", R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    message Publication {
      enum State { ZERO = 0; }
      repeated int32 numbers = 1;
      repeated string names = 2;
      repeated State states = 3;
      repeated bytes payloads = 4;
      repeated Publication children = 5;
      map<int32, int32> counts = 6;
      map<string, State> statuses = 7;
      map<string, string> labels = 8;
      map<string, Publication> messages = 9;
      optional string text = 10;
    }
  )schema");
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "publication.proto");
  ExpectNoErrors();
  for (const char* name :
       {"Numbers", "Names", "States", "Payloads", "Children"}) {
    SCOPED_TRACE(name);
    EXPECT_EQ(
        DecoderBody("Publication.java",
                    absl::StrCat("private void ensure", name, "IsMutable(")),
        absl::StrReplaceAll(
            DecoderBody(
                "Publication.java",
                absl::StrCat("private static void ensure", name, "IsMutable(")),
            {{"minCapacity", "capacity"}}));
  }
  std::string expected = "{intfrom_bitField0_=bitField0_;";
  int mask = 2;
  for (const auto& names :
       {std::make_pair("numbers", "Numbers"), std::make_pair("names", "Names"),
        std::make_pair("states", "States"),
        std::make_pair("payloads", "Payloads"),
        std::make_pair("children", "Children"),
        std::make_pair("counts", "Counts"),
        std::make_pair("statuses", "Statuses"),
        std::make_pair("labels", "Labels")}) {
    const std::string hex =
        absl::StrCat("0x", absl::Hex(mask, absl::kZeroPad8));
    const std::string guard =
        absl::StrCat("if (((from_bitField0_ & ", hex, ") != 0))");
    const std::string field = absl::StrCat(names.first, "_");
    std::string body = DecoderBody("Publication.java", guard);
    // In-place publication has no destination copy or map accessor.
    body = absl::StrReplaceAll(
        body,
        {{absl::StrCat("result.", field, "=", field, ";"), ""},
         {absl::StrCat("result.", field, "=internalGet", names.second, "();"),
          ""},
         {"result.", ""}});
    absl::StrAppend(&expected, "if(((from_bitField0_&", hex, ")!=0))", body);
    mask <<= 1;
  }
  const std::string message_guard =
      "if (((from_bitField0_ & 0x00000200) != 0))";
  absl::StrAppend(
      &expected, "if(((from_bitField0_&0x00000200)!=0))",
      absl::StrReplaceAll(
          DecoderBody("Publication.java", message_guard),
          {{"result.", ""}, {"internalGetMessages()", "parsed_messages"}}));
  expected += "}";
  EXPECT_EQ(expected,
            DecoderBody("Publication.java",
                        "private static void buildPartial_autosplit_0("));
}

TEST_F(JavaGeneratorTest, SingularChildParsingUsesBuilderFirstOccurrenceCode) {
  CreateTempFile("singular_state.proto", R"schema(
    syntax = "proto3";
    option java_multiple_files = true;
    message SingularChild { SingularChild child = 1; }
  )schema");
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "singular_state.proto");
  ExpectNoErrors();
  EXPECT_TRUE(FileContainsSubstring("SingularChild.java",
                                    "parseChildField(instance, input, "
                                    "extensionRegistry, childBuilder_);"));
  EXPECT_TRUE(FileContainsSubstring(
      "SingularChild.java",
      "instance.child_ = SingularChild.parser().parsePartialFrom(input, "
      "extensionRegistry);"));
  EXPECT_FALSE(
      FileContainsSubstring("SingularChild.java", "readMessageForMerge("));
}

TEST_F(JavaGeneratorTest, ParserDoesNotReconstructOrRetryOneofState) {
  CreateTempFile("retry_state.proto", R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    message Child { optional int32 value = 1; }
    message Ordinary { optional Child child = 1; }
    message MessagesOnly { oneof choice { Child first = 1; Child second = 2; } }
    message Mixed { oneof choice { Child first = 1; int32 second = 2; } }
    message MixedEnum {
      enum Value { ZERO = 0; }
      oneof choice { Child first = 1; Value second = 2; }
    }
    message Extendable { optional Child child = 1; extensions 100 to max; }
  )schema");
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "retry_state.proto");
  ExpectNoErrors();
  for (const char* name :
       {"Ordinary.java", "MessagesOnly.java", "Extendable.java"}) {
    SCOPED_TRACE(name);
    EXPECT_FALSE(FileContainsSubstring(name, "retryTag:"));
    EXPECT_TRUE(FileContainsSubstring(name, "pendingTag = tag;"));
  }
  EXPECT_TRUE(FileContainsSubstring("Extendable.java", "pendingTag = tag;"));
  for (const char* name : {"Mixed.java", "MixedEnum.java"}) {
    SCOPED_TRACE(name);
    EXPECT_FALSE(FileContainsSubstring(name, "retryTag:"));
    EXPECT_TRUE(FileContainsSubstring(name, "pendingTag = tag;"));
  }
}

TEST_F(JavaGeneratorTest, ParserHandsOffLiveStateBeforeDuplicateRead) {
  CreateTempFile("duplicate_handoff.proto", R"schema(
    syntax = "proto3";
    option java_multiple_files = true;
    message DuplicateHandoff {
      DuplicateHandoff child = 1;
      map<string, DuplicateHandoff> values = 2;
      string text = 3;
    }
  )schema");
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "duplicate_handoff.proto");
  ExpectNoErrors();
  EXPECT_TRUE(FileContainsSubstring(
      "DuplicateHandoff.java",
      "if (instance.child_ != null || childBuilder_ != null) {"));
  EXPECT_TRUE(
      FileContainsSubstring("DuplicateHandoff.java", "pendingTag = tag;"));
  EXPECT_TRUE(FileContainsSubstring(
      "DuplicateHandoff.java",
      "new java.lang.Object[] {childBuilder_, parsed_values}"));
  EXPECT_TRUE(FileContainsSubstring("DuplicateHandoff.java",
                                    "builder.mergeFromPendingField("));
}

TEST_F(JavaGeneratorTest, ParserHandoffConstructorHasNoEmptyInitialization) {
  CreateTempFile("constructor_spacing.proto", R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    message Scalar { optional int32 value = 1; extensions 100 to max; }
    message Ordinary { optional Scalar child = 1; }
  )schema");
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "constructor_spacing.proto");
  ExpectNoErrors();
  EXPECT_TRUE(FileContainsSubstring(
      "Scalar.java", "copyFrom_autosplit_0(other, parserLocals);\n    }"));
  EXPECT_TRUE(FileContainsSubstring("Scalar.java", "super(parent);\n    }"));
  EXPECT_FALSE(FileContainsSubstring(
      "Ordinary.java", "if (fieldBuilders == null) {\n      } else {"));
  EXPECT_EQ(DecoderBody("Ordinary.java", "private Builder(Ordinary other,")
                .find("maybeForceBuilderInitialization"),
            std::string::npos);
}

TEST_F(JavaGeneratorTest, ClosedEnumParsingUsesSameValidationForBothTargets) {
  CreateTempFile("enum_decoding.proto", R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    message EnumDecoding {
      enum Value { ZERO = 0; }
      optional Value value = 1;
    }
  )schema");
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "enum_decoding.proto");
  ExpectNoErrors();
  EXPECT_EQ(DecoderBody("EnumDecoding.java", "case 8:", 0),
            DecoderBody("EnumDecoding.java", "case 8:", 1));
}

TEST_F(JavaGeneratorTest, MessageMapParsingKeepsBuilderStorageLocal) {
  CreateTempFile("map_state.proto", R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    message Value { optional int32 number = 1; }
    message MapState { map<string, Value> values = 1; extensions 100 to max; }
  )schema");
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "map_state.proto");
  ExpectNoErrors();
  EXPECT_TRUE(FileContainsSubstring("MapState.java", "parsed_values = null;"));
  EXPECT_TRUE(FileContainsSubstring("MapState.java",
                                    "parsed_values)).ensureBuilderMap().put("));
  EXPECT_TRUE(FileContainsSubstring("MapState.java",
                                    "new java.lang.Object[] {parsed_values}"));
  EXPECT_TRUE(FileContainsSubstring(
      "MapState.java",
      "parsed_values.build(ValuesDefaultEntryHolder.defaultEntry)"));
}

TEST_F(JavaGeneratorTest, ParserLowersMapInitializationAndDecodingTogether) {
  for (const char* syntax : {"proto2", "proto3"}) {
    SCOPED_TRACE(syntax);
    CreateTempFile("map_lowering.proto",
                   absl::StrCat("syntax = \"", syntax, R"schema(";
      option java_multiple_files = true;
      message MapLowering {
        enum State { ZERO = 0; }
        map<int32, int32> numbers = 1;
        map<string, MapLowering> children = 2;
        map<string, State> states = 3;
        map<string, string> texts = 4;
      }
    )schema"));
    RunProtoc(
        "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
        "map_lowering.proto");
    ExpectNoErrors();
    int tag = 10;
    for (const auto& names : {std::make_pair("numbers", "Numbers"),
                              std::make_pair("children", "Children"),
                              std::make_pair("states", "States"),
                              std::make_pair("texts", "Texts")}) {
      SCOPED_TRACE(names.first);
      const bool message_value = std::string(names.first) == "children";
      EXPECT_EQ(MapDecoderBody("MapLowering.java", "MapLowering", names.first,
                               names.second, tag, false, message_value),
                MapDecoderBody("MapLowering.java", "MapLowering", names.first,
                               names.second, tag, true, message_value));
      tag += 8;
    }
  }
}

TEST_F(JavaGeneratorTest, WideParserHandoffPacksLocals) {
  std::string schema = R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    message Value { optional int32 number = 1; }
    message WideState {
      map<string, Value> values = 261;
      extensions 1000 to max;
  )schema";
  for (int i = 1; i <= 260; ++i) {
    absl::StrAppend(&schema, "optional string text_", i, " = ", i, ";\n");
  }
  absl::StrAppend(&schema, "}");
  CreateTempFile("wide_state.proto", schema);
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "wide_state.proto");
  ExpectNoErrors();
  EXPECT_TRUE(FileContainsSubstring("WideState.java",
                                    "java.lang.Object[] parserLocals) {"));
  EXPECT_TRUE(FileContainsSubstring(
      "WideState.java", "copyFrom_autosplit_8(other, parserLocals);"));
  EXPECT_TRUE(
      FileContainsSubstring("WideState.java", "text260_ = other.text260_;"));
}

TEST_F(JavaGeneratorTest, ParserHandoffTransfersStateInOneConstructor) {
  CreateTempFile("handoff.proto", R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    message Child { optional int32 value = 1; }
    message Handoff {
      optional Child child = 1;
      oneof choice {
        Child selected = 2;
        int32 number = 3;
      }
      extensions 100 to max;
    })schema");
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "handoff.proto");
  ExpectNoErrors();
  EXPECT_TRUE(FileContainsSubstring(
      "Handoff.java",
      "Builder builder = new Builder(instance, unknown, "
      "new java.lang.Object[] {childBuilder_, selectedBuilder_});"));
  EXPECT_TRUE(
      FileContainsSubstring("Handoff.java", "private Builder(Handoff other,"));
  EXPECT_TRUE(FileContainsSubstring("Handoff.java", "super(other, unknown);"));
  EXPECT_FALSE(FileContainsSubstring(
      "Handoff.java", "transferUnknownFields(builder, unknown);"));
  EXPECT_FALSE(FileContainsSubstring(
      "Handoff.java", "private static Builder newBuilderForParsing("));
  EXPECT_FALSE(
      FileContainsSubstring("Handoff.java", "builder.transferChildren"));
  EXPECT_FALSE(
      FileContainsSubstring("Handoff.java", "newFieldBuilderForParsing"));
}

TEST_F(JavaGeneratorTest, CodeSizeParsingRetainsBuilderEntryPoint) {
  CreateTempFile("code_size_parser.proto", R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    option optimize_for = CODE_SIZE;
    message CodeSizeParser {
      enum State { ZERO = 0; }
      optional int32 number = 1;
      optional CodeSizeParser child = 2;
      oneof choice {
        int32 selected_number = 3;
        string selected_text = 4;
        State selected_state = 5;
        CodeSizeParser selected_child = 6;
      }
      repeated CodeSizeParser children = 7;
    })schema");
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "code_size_parser.proto");
  ExpectNoErrors();
  EXPECT_TRUE(FileContainsSubstring("CodeSizeParser.java",
                                    "Builder builder = newBuilder();"));
  EXPECT_TRUE(FileContainsSubstring(
      "CodeSizeParser.java", "builder.mergeFrom(input, extensionRegistry);"));
  EXPECT_TRUE(FileContainsSubstring("CodeSizeParser.java",
                                    "return builder.buildPartial();"));
  EXPECT_FALSE(
      FileContainsSubstring("CodeSizeParser.java", "GeneratedMessageParser<"));
  for (const char* removed :
       {"copyFrom_autosplit_", "transferChildren", "mergeFromPendingField",
        "private Builder(CodeSizeParser other,"}) {
    EXPECT_FALSE(FileContainsSubstring("CodeSizeParser.java", removed))
        << removed;
  }
}

TEST_F(JavaGeneratorTest, MapParsingUsesJava8CompatibleTypes) {
  CreateTempFile("map_parser.proto", R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    message MapParser {
      enum Status {
        ZERO = 0;
      }
      message Value {}
      map<int32, int32> numbers = 1;
      map<int32, Value> children = 2;
      map<int32, Status> statuses = 3;
    })schema");
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "map_parser.proto");
  ExpectNoErrors();
  EXPECT_FALSE(FileContainsSubstring("MapParser.java", "var entry ="));
  EXPECT_TRUE(FileContainsSubstring(
      "MapParser.java",
      "com.google.protobuf.MapEntry<java.lang.Integer, java.lang.Integer>\n"));
  EXPECT_TRUE(FileContainsSubstring(
      "MapParser.java",
      "com.google.protobuf.MapEntry<java.lang.Integer, MapParser.Value>\n"));
}

}  // namespace
}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
