// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.protobuf.test.GeneratedMessageParserTestProto;
import com.google.protobuf.test.TestExtensionParsing;
import com.google.protobuf.test.TestMapParsing;
import java.io.ByteArrayOutputStream;
import java.util.Arrays;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import proto2_unittest.UnittestProto.TestAllTypes;
import proto2_unittest.UnittestProto.TestExtremeDefaultValues;

@RunWith(JUnit4.class)
public final class GeneratedMessageParserTest {
  @Test
  public void testExplicitBytesDefaultsAreShared() throws Exception {
    assertSharedBytesDefaults(TestAllTypes.getDefaultInstance());
    assertSharedBytesDefaults(TestExtremeDefaultValues.getDefaultInstance());
  }

  private static void assertSharedBytesDefaults(Message defaults) throws Exception {
    Message.Builder builder = defaults.newBuilderForType();
    Message parsed = defaults.getParserForType().parseFrom(new byte[0]);
    Message built = builder.build();
    for (Descriptors.FieldDescriptor field : defaults.getDescriptorForType().getFields()) {
      if (field.getType() == Descriptors.FieldDescriptor.Type.BYTES && field.hasDefaultValue()) {
        ByteString value = (ByteString) defaults.getField(field);
        assertThat(builder.getField(field)).isSameInstanceAs(value);
        assertThat(parsed.getField(field)).isSameInstanceAs(value);
        assertThat(built.getField(field)).isSameInstanceAs(value);
        builder.setField(field, ByteString.copyFromUtf8("changed"));
      }
    }
    Message changed = builder.build();
    builder.clear();
    for (Descriptors.FieldDescriptor field : defaults.getDescriptorForType().getFields()) {
      if (field.getType() == Descriptors.FieldDescriptor.Type.BYTES && field.hasDefaultValue()) {
        assertThat(builder.getField(field)).isSameInstanceAs(defaults.getField(field));
        assertThat(changed.getField(field)).isEqualTo(ByteString.copyFromUtf8("changed"));
      }
    }
    assertThat(builder.build()).isEqualTo(defaults);
    assertThat(parsed).isEqualTo(defaults);
  }

  @Test
  public void testMapEntriesAcrossOrdinaryChildHandoff() throws Exception {
    assertMapEntriesAcrossChildHandoff(false);
  }

  @Test
  public void testMapEntriesAcrossOneofChildHandoff() throws Exception {
    assertMapEntriesAcrossChildHandoff(true);
  }

  @Test
  public void testDeclaredChildrenAcrossExtensionHandoff() throws Exception {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    TestExtensionParsing.newBuilder()
        .setChild(TestExtensionParsing.newBuilder().addValues(1))
        .setGroup(TestExtensionParsing.newBuilder().addValues(10))
        .addValues(100)
        .putEntries("before", 1)
        .build()
        .writeTo(bytes);
    TestExtensionParsing.newBuilder()
        .setChild(TestExtensionParsing.newBuilder().addValues(2))
        .setGroup(TestExtensionParsing.newBuilder().addValues(20))
        .build()
        .writeTo(bytes);
    TestExtensionParsing.newBuilder()
        .setExtension(GeneratedMessageParserTestProto.number, 7)
        .setExtension(
            GeneratedMessageParserTestProto.extensionChild,
            TestExtensionParsing.newBuilder().addValues(1000).build())
        .build()
        .writeTo(bytes);
    TestExtensionParsing.newBuilder()
        .setChild(TestExtensionParsing.newBuilder().addValues(3))
        .setGroup(TestExtensionParsing.newBuilder().addValues(30))
        .addValues(200)
        .putEntries("after", 2)
        .setExtension(
            GeneratedMessageParserTestProto.extensionChild,
            TestExtensionParsing.newBuilder().addValues(2000).build())
        .build()
        .writeTo(bytes);
    byte[] wire = bytes.toByteArray();
    ExtensionRegistry registry = ExtensionRegistry.newInstance();
    GeneratedMessageParserTestProto.registerAllExtensions(registry);
    assertExtensionParsingMatchesBuilder(wire, registry);
    assertExtensionParsingMatchesBuilder(wire, ExtensionRegistry.getEmptyRegistry());
    TestExtensionParsing parsed = TestExtensionParsing.parseFrom(wire, registry);
    assertThat(parsed.getChild().getValuesList()).containsExactly(1, 2, 3).inOrder();
    assertThat(parsed.getGroup().getValuesList()).containsExactly(10, 20, 30).inOrder();
    assertThat(parsed.getValuesList()).containsExactly(100, 200).inOrder();
    assertThat(parsed.getEntriesMap()).containsExactly("before", 1, "after", 2);
    assertThat(parsed.getExtension(GeneratedMessageParserTestProto.number)).isEqualTo(7);
    assertThat(parsed.getExtension(GeneratedMessageParserTestProto.extensionChild).getValuesList())
        .containsExactly(1000, 2000)
        .inOrder();
  }

  @Test
  public void testOneofChildrenAcrossExtensionHandoff() throws Exception {
    for (boolean scalarBeforeExtension : new boolean[] {false, true}) {
      ByteArrayOutputStream bytes = new ByteArrayOutputStream();
      TestExtensionParsing.newBuilder()
          .setSelected(TestExtensionParsing.newBuilder().addValues(1))
          .build()
          .writeTo(bytes);
      TestExtensionParsing.newBuilder()
          .setOther(TestExtensionParsing.newBuilder().addValues(10))
          .build()
          .writeTo(bytes);
      TestExtensionParsing.newBuilder()
          .setSelected(TestExtensionParsing.newBuilder().addValues(2))
          .build()
          .writeTo(bytes);
      if (scalarBeforeExtension) {
        TestExtensionParsing.newBuilder()
            .setText(ByteString.copyFromUtf8("before extension"))
            .build()
            .writeTo(bytes);
      }
      TestExtensionParsing.newBuilder()
          .setExtension(GeneratedMessageParserTestProto.number, 7)
          .build()
          .writeTo(bytes);
      TestExtensionParsing.newBuilder()
          .setOther(TestExtensionParsing.newBuilder().addValues(20))
          .build()
          .writeTo(bytes);
      if (scalarBeforeExtension) {
        TestExtensionParsing.newBuilder()
            .setSelected(TestExtensionParsing.newBuilder().addValues(3))
            .build()
            .writeTo(bytes);
      }
      byte[] wire = bytes.toByteArray();
      ExtensionRegistry registry = ExtensionRegistry.newInstance();
      GeneratedMessageParserTestProto.registerAllExtensions(registry);
      assertExtensionParsingMatchesBuilder(wire, registry);
      TestExtensionParsing parsed = TestExtensionParsing.parseFrom(wire, registry);
      if (scalarBeforeExtension) {
        assertThat(parsed.getSelected().getValuesList()).containsExactly(1, 2, 3).inOrder();
      } else {
        assertThat(parsed.getOther().getValuesList()).containsExactly(10, 20).inOrder();
      }
    }
  }

  private static void assertExtensionParsingMatchesBuilder(byte[] wire, ExtensionRegistry registry)
      throws Exception {
    assertExtensionPrefixesMatchBuilder(wire, registry, 0);
    GeneratedMessage.setAlwaysUseFieldBuildersForTesting(true);
    try {
      // Eager initialization marks absent children present. Compare after both
      // paths have reached the parent builder, not at earlier truncations.
      assertExtensionPrefixesMatchBuilder(wire, registry, wire.length);
    } finally {
      GeneratedMessage.setAlwaysUseFieldBuildersForTesting(false);
    }
  }

  private static void assertExtensionPrefixesMatchBuilder(
      byte[] wire, ExtensionRegistry registry, int firstLength) throws Exception {
    for (int length = firstLength; length <= wire.length; length++) {
      byte[] prefix = Arrays.copyOf(wire, length);
      TestExtensionParsing.Builder builder = TestExtensionParsing.newBuilder();
      boolean failed = false;
      try {
        builder.mergeFrom(prefix, registry);
      } catch (InvalidProtocolBufferException failure) {
        failed = true;
      }
      TestExtensionParsing parsed;
      if (failed) {
        InvalidProtocolBufferException failure =
            assertThrows(
                InvalidProtocolBufferException.class,
                () -> TestExtensionParsing.parseFrom(prefix, registry));
        parsed = (TestExtensionParsing) failure.getUnfinishedMessage();
      } else {
        parsed = TestExtensionParsing.parseFrom(prefix, registry);
      }
      assertThat(parsed).isEqualTo(builder.buildPartial());
      assertThat(parsed.toByteString()).isEqualTo(builder.buildPartial().toByteString());
      assertThrows(UnsupportedOperationException.class, () -> parsed.getValuesList().clear());
      assertThrows(
          UnsupportedOperationException.class, () -> parsed.getEntriesMap().put("blocked", 0));
    }
  }

  private static void assertMapEntriesAcrossChildHandoff(boolean oneof) throws Exception {
    TestMapParsing prefix =
        TestMapParsing.newBuilder()
            .putChildren("before", TestMapParsing.newBuilder().setValue(10).build())
            .build();
    TestMapParsing.Builder first = TestMapParsing.newBuilder();
    TestMapParsing.Builder second = TestMapParsing.newBuilder();
    if (oneof) {
      first.setSelected(TestMapParsing.newBuilder().setValue(1));
      second.setSelected(TestMapParsing.newBuilder().setText("merged"));
    } else {
      first.setChild(TestMapParsing.newBuilder().setValue(1));
      second.setChild(TestMapParsing.newBuilder().setText("merged"));
    }
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    first.build().writeTo(bytes);
    second.build().writeTo(bytes);
    TestMapParsing.newBuilder()
        .putChildren("after", TestMapParsing.newBuilder().setValue(20).build())
        .putChildren("before", TestMapParsing.newBuilder().setValue(30).build())
        .build()
        .writeTo(bytes);
    byte[] wire =
        prefix.toByteString().concat(ByteString.copyFrom(bytes.toByteArray())).toByteArray();
    TestMapParsing expected = TestMapParsing.newBuilder().mergeFrom(wire).build();

    TestMapParsing parsed = TestMapParsing.parseFrom(wire);
    assertThat(parsed).isEqualTo(expected);
    assertThat(parsed.toByteString()).isEqualTo(expected.toByteString());
    assertThat(parsed.getChildrenOrThrow("before").getValue()).isEqualTo(30);
    assertThat(parsed.getChildrenOrThrow("after").getValue()).isEqualTo(20);
    assertThat(oneof ? parsed.getSelected().getValue() : parsed.getChild().getValue()).isEqualTo(1);
    assertThat(oneof ? parsed.getSelected().getText() : parsed.getChild().getText())
        .isEqualTo("merged");
    assertThrows(UnsupportedOperationException.class, () -> parsed.getChildrenMap().clear());

    for (int length = 0; length < wire.length; length++) {
      byte[] truncated = Arrays.copyOf(wire, length);
      TestMapParsing.Builder builder = TestMapParsing.newBuilder();
      boolean failed = false;
      try {
        builder.mergeFrom(truncated);
      } catch (InvalidProtocolBufferException failure) {
        failed = true;
      }
      TestMapParsing partial;
      if (failed) {
        InvalidProtocolBufferException failure =
            assertThrows(
                InvalidProtocolBufferException.class, () -> TestMapParsing.parseFrom(truncated));
        partial = (TestMapParsing) failure.getUnfinishedMessage();
      } else {
        partial = TestMapParsing.parseFrom(truncated);
      }
      assertThat(partial).isEqualTo(builder.buildPartial());
      assertThat(partial.toByteString()).isEqualTo(builder.buildPartial().toByteString());
      assertThrows(
          UnsupportedOperationException.class,
          () -> partial.getChildrenMap().put("blocked", TestMapParsing.getDefaultInstance()));
    }
  }
}
