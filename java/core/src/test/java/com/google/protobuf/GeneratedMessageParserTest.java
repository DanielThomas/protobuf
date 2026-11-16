// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import java.io.ByteArrayOutputStream;
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

  @Test
  public void testLegacyParsingStateReturnsActualBuilder() throws Exception {
    TestAllTypes value = TestAllTypes.newBuilder().setOptionalInt32(7).build();
    DynamicMessage defaults = DynamicMessage.getDefaultInstance(TestAllTypes.getDescriptor());
    ExtensionRegistryLite registry = ExtensionRegistryLite.getEmptyRegistry();
    Object state =
        AbstractParser.parseStateForParsing(
            defaults, value.toByteString().newCodedInput(), registry);
    assertThat(state).isInstanceOf(DynamicMessage.Builder.class);
    MessageLite.Builder builder = (MessageLite.Builder) state;
    MessageLite snapshot = builder.buildPartial();
    assertThat(snapshot.toByteString()).isEqualTo(value.toByteString());
    for (boolean retained : new boolean[] {false, true}) {
      MessageLite.Builder restored = AbstractParser.restoreStateForParsing(snapshot, retained);
      assertThat(restored).isInstanceOf(DynamicMessage.Builder.class);
      assertThat(restored.buildPartial()).isEqualTo(snapshot);
      assertThat(restored).isNotSameInstanceAs(builder);
    }

    ByteString truncated = value.toByteString().concat(ByteString.copyFrom(new byte[] {18, 1}));
    InvalidProtocolBufferException failure =
        assertThrows(
            InvalidProtocolBufferException.class,
            () ->
                AbstractParser.parseStateForParsing(defaults, truncated.newCodedInput(), registry));
    assertThat(failure.getUnfinishedMessage().toByteString()).isEqualTo(value.toByteString());
  }

  @Test
  public void testGeneratedParsingStatePromotesOnlyForDuplicateMessage() throws Exception {
    TestAllTypes value =
        TestAllTypes.newBuilder()
            .setOptionalNestedMessage(TestAllTypes.NestedMessage.newBuilder().setBb(7))
            .addRepeatedInt32(11)
            .build();
    ExtensionRegistryLite registry = ExtensionRegistryLite.getEmptyRegistry();
    Object first =
        AbstractParser.parseStateForParsing(
            TestAllTypes.getDefaultInstance(), value.toByteString().newCodedInput(), registry);
    assertThat(first).isInstanceOf(TestAllTypes.class);
    assertThat(first).isEqualTo(value);
    ByteString duplicate = value.toByteString().concat(value.toByteString());
    Object second =
        AbstractParser.parseStateForParsing(
            TestAllTypes.getDefaultInstance(), duplicate.newCodedInput(), registry);
    assertThat(second).isInstanceOf(TestAllTypes.Builder.class);
    assertThat(((TestAllTypes.Builder) second).buildPartial())
        .isEqualTo(TestAllTypes.newBuilder().mergeFrom(duplicate).buildPartial());
    for (boolean retained : new boolean[] {false, true}) {
      TestAllTypes.Builder restored =
          (TestAllTypes.Builder)
              AbstractParser.restoreStateForParsing((TestAllTypes) first, retained);
      assertThat(restored.buildPartial()).isEqualTo(value);
      restored.addRepeatedInt32(12);
      assertThat(value.getRepeatedInt32List()).containsExactly(11);
      assertThat(restored.getRepeatedInt32List()).containsExactly(11, 12).inOrder();
    }
  }

  @Test
  public void testFramedParsingStateRetainsBuilderAcrossOccurrences() throws Exception {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    for (int value = 1; value <= 3; value++) {
      output.writeMessageNoTag(TestAllTypes.newBuilder().addRepeatedInt32(value).build());
    }
    output.flush();
    CodedInputStream input = CodedInputStream.newInstance(bytes.toByteArray());
    ExtensionRegistryLite registry = ExtensionRegistryLite.getEmptyRegistry();
    Object first =
        AbstractParser.mergeMessageForParsing(
            null, TestAllTypes.getDefaultInstance(), input, registry);
    assertThat(first).isInstanceOf(TestAllTypes.class);
    Object second =
        AbstractParser.mergeMessageForParsing(
            first, TestAllTypes.getDefaultInstance(), input, registry);
    assertThat(second).isInstanceOf(TestAllTypes.Builder.class);
    Object third =
        AbstractParser.mergeMessageForParsing(
            second, TestAllTypes.getDefaultInstance(), input, registry);
    assertThat(third).isSameInstanceAs(second);
    assertThat(((TestAllTypes.Builder) third).getRepeatedInt32List())
        .containsExactly(1, 2, 3)
        .inOrder();
    assertThat(((TestAllTypes) first).getRepeatedInt32List()).containsExactly(1);
    assertThat(input.isAtEnd()).isTrue();
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
}
