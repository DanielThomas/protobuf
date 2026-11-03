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

  std::string BuilderParsingCode(absl::string_view filename) {
    std::string contents;
    EXPECT_TRUE(File::GetContents(absl::StrCat(temp_directory(), "/", filename),
                                  &contents, true)
                    .ok());
    const size_t start = contents.find(
        "public Builder mergeFrom(\n"
        "        com.google.protobuf.CodedInputStream input,");
    if (start == std::string::npos) {
      ADD_FAILURE() << "No builder parsing method in " << filename;
      return "";
    }
    const size_t end = contents.find("return this;", start);
    EXPECT_NE(end, std::string::npos);
    return contents.substr(start, end - start);
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

TEST_F(JavaGeneratorTest, CompactPackedParsingPreservesLoopShape) {
  CreateTempFile("compact.proto", R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    message Compact {
      enum State { ZERO = 0; ONE = 1; }
      optional State state = 1;
      repeated sint32 numbers = 2;
      repeated fixed64 fixed = 3;
      repeated State states = 4;
      optional string text = 5;
    }
  )schema");
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "compact.proto");
  ExpectNoErrors();

  const std::string code = BuilderParsingCode(PACKAGE_PREFIX "Compact.java");
  // Both direct exits and a shared packed-limit epilogue regressed C2's
  // hot-inlined child decoder in the SCALING_ONEOF_2048 benchmark.
  EXPECT_NE(code.find("boolean done = false;"), std::string::npos);
  EXPECT_NE(code.find("while (!done)"), std::string::npos);
  EXPECT_EQ(code.find("parseLoop"), std::string::npos);
  EXPECT_EQ(code.find("int limit;"), std::string::npos);
  EXPECT_NE(code.find("int limit ="), std::string::npos);
  int pop_count = 0;
  for (size_t pop = code.find("input.popLimit(limit);");
       pop != std::string::npos;
       pop = code.find("input.popLimit(limit);", pop + 1)) {
    ++pop_count;
  }
  EXPECT_EQ(pop_count, 3);
  EXPECT_NE(
      code.find("ensureNumbersIsMutable(input.countPackedVarints(length));"),
      std::string::npos);
  EXPECT_NE(
      code.find("ensureFixedIsMutable(java.lang.Math.min(length, 4096) / 8);"),
      std::string::npos);
  EXPECT_EQ(code.find("tmpValue"), std::string::npos);
  EXPECT_EQ(code.find("int count ="), std::string::npos);
  EXPECT_EQ(code.find("int alloc ="), std::string::npos);
}

TEST_F(JavaGeneratorTest, BuilderParsingWithoutPackedFields) {
  CreateTempFile("unpacked.proto", R"schema(
    syntax = "proto3";
    option java_multiple_files = true;
    message Empty {}
    message Unpacked { string text = 1; }
  )schema");
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir "
      "unpacked.proto");
  ExpectNoErrors();

  for (const char* file :
       {PACKAGE_PREFIX "Empty.java", PACKAGE_PREFIX "Unpacked.java"}) {
    const std::string code = BuilderParsingCode(file);
    EXPECT_NE(code.find("while (!done)"), std::string::npos);
    EXPECT_EQ(code.find("int limit"), std::string::npos);
    EXPECT_EQ(code.find("popLimit"), std::string::npos);
  }
}

TEST_F(JavaGeneratorTest, MapParsingDoesNotRepeatMutableAccessorBookkeeping) {
  CreateTempFile("maps.proto", R"schema(
    syntax = "proto2";
    option java_multiple_files = true;
    message Maps {
      enum State { ZERO = 0; ONE = 1; }
      message Value { optional int32 number = 1; }
      map<int32, int32> scalars = 1;
      map<int32, State> enums = 2;
      map<int32, Value> messages = 3;
    }
  )schema");
  RunProtoc(
      "protocol_compiler --proto_path=$tmpdir --java_out=$tmpdir maps.proto");
  ExpectNoErrors();

  const std::string code = BuilderParsingCode(PACKAGE_PREFIX "Maps.java");
  EXPECT_NE(code.find("internalGetMutableScalars()"), std::string::npos);
  EXPECT_NE(code.find("internalGetMutableEnums()"), std::string::npos);
  EXPECT_NE(code.find("internalGetMutableMessages()"), std::string::npos);
  EXPECT_EQ(code.find("bitField"), std::string::npos);
}

}  // namespace
}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
