// Protocol Buffers - Google's data interchange format
// Copyright 2023 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "google/protobuf/compiler/java/generator.h"

#include <memory>
#include <string>

#include "google/protobuf/testing/file.h"
#include "google/protobuf/testing/file.h"
#include "google/protobuf/testing/file.h"
#include "google/protobuf/descriptor.pb.h"
#include <gtest/gtest.h>
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/compiler/java/java_features.pb.h"
#include "google/protobuf/compiler/command_line_interface_tester.h"


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
      "to_bitField0_ |= (from_bitField0_ & 0x00000006);"));
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

TEST_F(JavaGeneratorTest, DirectParsingKeepsStateInParser) {
  CreateTempFile("parser_entry.proto", R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    message Scalar {
      oneof choice {
        int32 number = 1;
        string text = 2;
      }
    }
    message Repeated {
      repeated int32 numbers = 1;
    }
    message Parent {
      optional Scalar child = 1;
      optional Repeated other = 2;
      oneof selection {
        Scalar selected = 3;
        int32 number = 4;
      }
    }
    message Extendable {
      extensions 100 to max;
    }
    message Choice {
      oneof choice {
        Repeated numbers = 1;
        Parent parent = 2;
        string text = 3;
      }
    })schema");
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "parser_entry.proto");
  ExpectNoErrors();
  for (const char* name : {"Scalar", "Repeated", "Parent", "Extendable", "Choice"}) {
    SCOPED_TRACE(name);
    const std::string file = absl::StrCat(name, ".java");
    EXPECT_TRUE(FileContainsSubstring(
        file, absl::StrCat("new com.google.protobuf.GeneratedMessageParser<", name,
                          ">()")));
    EXPECT_TRUE(FileContainsSubstring(
        file, absl::StrCat(name, " instance = new ", name, "();")));
    EXPECT_FALSE(FileContainsSubstring(file, "readFields("));
    EXPECT_TRUE(FileContainsSubstring(file, "java.io.IOException parseFailure = null;"));
    EXPECT_TRUE(FileContainsSubstring(file, "parseFailure = e;"));
    EXPECT_EQ(FileContainsSubstring(file, "children = null;"),
              std::string(name) == "Parent" || std::string(name) == "Choice");
    EXPECT_EQ(FileContainsSubstring(file, "java.lang.Object extensions = null;"),
              std::string(name) == "Extendable");
  }
  EXPECT_TRUE(FileContainsSubstring(
      "Choice.java",
      "readChoice(instance, tag >>> 3, input, extensionRegistry, children, false);"));
  EXPECT_TRUE(FileContainsSubstring("Choice.java", "switch (field) {"));
  EXPECT_TRUE(FileContainsSubstring(
      "Repeated.java",
      "private void ensureNumbersIsMutable(Repeated instance, int minCapacity)"));
  EXPECT_TRUE(FileContainsSubstring(
      "Repeated.java", "ensureNumbersIsMutable(instance, /* capacity= */ -1);"));
  EXPECT_TRUE(FileContainsSubstring(
      "Repeated.java", "ensureNumbersIsMutable(instance, count);"));
  EXPECT_FALSE(FileContainsSubstring("Parent.java", "childValue0("));
  EXPECT_TRUE(FileContainsSubstring(
      "Parent.java",
      "instance.child_ = (Scalar) buildChild(children, 1, instance.child_);\n"
      "        instance.other_ = (Repeated) buildChild(children, 2, instance.other_);\n"
      "        instance.selection_ = buildChild(children, instance.selectionCase_, instance.selection_);"));
}

TEST_F(JavaGeneratorTest, DirectParsingBoundsWideChildMethods) {
  std::string schema = R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    message Child { repeated int32 values = 1; }
    message Parent {
  )schema";
  for (int i = 1; i <= 65; ++i) {
    absl::StrAppend(&schema, "optional Child child_", i, " = ", i * 2, ";\n");
  }
  absl::StrAppend(&schema, "}");
  CreateTempFile("parser_children.proto", schema);
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "parser_children.proto");
  ExpectNoErrors();
  for (int group = 0; group < 3; ++group) {
    EXPECT_TRUE(FileContainsSubstring(
        "Parent.java",
        absl::StrCat("if (childValue", group,
                     "(instance, tag >>> 3) != null) children = childBuilders(children);")));
    EXPECT_TRUE(FileContainsSubstring(
        "Parent.java",
        absl::StrCat("readChild", group,
                     "(instance, tag >>> 3, input, extensionRegistry, children);")));
    EXPECT_TRUE(FileContainsSubstring(
        "Parent.java", absl::StrCat("finishChildren", group, "(instance, children);")));
  }
  EXPECT_FALSE(FileContainsSubstring("Parent.java", "childValue3("));
  EXPECT_FALSE(FileContainsSubstring("Parent.java", "readChild3("));
  EXPECT_FALSE(FileContainsSubstring("Parent.java", "finishChildren3("));
}

TEST_F(JavaGeneratorTest, CodeSizeParsingRetainsBuilderEntryPoint) {
  CreateTempFile("code_size_parser.proto", R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    option optimize_for = CODE_SIZE;
    message CodeSizeParser {
      optional int32 number = 1;
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
