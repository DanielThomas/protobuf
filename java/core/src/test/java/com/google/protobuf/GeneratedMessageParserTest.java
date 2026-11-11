// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;
import static org.junit.Assert.assertThrows;

import com.google.protobuf.test.GeneratedMessageParserTestProto;
import com.google.protobuf.test.TestExtensionParsing;
import com.google.protobuf.test.TestMapParsing;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.Arrays;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import proto2_unittest.UnittestProto.TestAllTypes;
import proto2_unittest.UnittestProto.TestExtremeDefaultValues;
import proto3_unittest.UnittestProto3;

@RunWith(JUnit4.class)
public final class GeneratedMessageParserTest {
  @Test
  public void testUnknownFieldHelpersPreserveDiscardAndExistingState() throws Exception {
    for (boolean discard : new boolean[] {false, true}) {
      for (boolean existing : new boolean[] {false, true}) {
        UnknownFieldSet initial =
            existing
                ? UnknownFieldSet.newBuilder().mergeVarintField(99, 1).build()
                : UnknownFieldSet.getDefaultInstance();
        UnknownFieldSet.Builder unknown = existing ? initial.toBuilder() : null;
        GeneratedMessage.Builder<?> builder = TestMapParsing.newBuilder().setUnknownFields(initial);
        CodedInputStream parserInput = CodedInputStream.newInstance(new byte[] {8, 2});
        CodedInputStream builderInput = CodedInputStream.newInstance(new byte[] {8, 2});
        if (discard) {
          parserInput.discardUnknownFields();
          builderInput.discardUnknownFields();
        }
        UnknownFieldSet.Builder prepared =
            GeneratedMessageParser.prepareUnknownFields(unknown, parserInput);
        if (discard || existing) {
          assertThat(prepared).isSameInstanceAs(unknown);
        }
        assertThat(
                GeneratedMessageParser.parseUnknownField(
                    parserInput, prepared, parserInput.readTag()))
            .isEqualTo(
                builder.parseUnknownField(
                    builderInput,
                    ExtensionRegistryLite.getEmptyRegistry(),
                    builderInput.readTag()));
        TestMapParsing result = TestMapParsing.newBuilder().buildPartial();
        GeneratedMessageParser.finishUnknownFields(result, prepared);
        assertThat(result.getUnknownFields()).isEqualTo(builder.getUnknownFields());
        assertThat(result.getUnknownFields())
            .isEqualTo(discard ? initial : initial.toBuilder().mergeVarintField(1, 2).build());
        assertThat(parserInput.isAtEnd()).isTrue();
        assertThat(builderInput.isAtEnd()).isTrue();
        if (discard) {
          java.lang.reflect.Field storage =
              GeneratedMessage.Builder.class.getDeclaredField("unknownFieldsOrBuilder");
          storage.setAccessible(true);
          assertThat(storage.get(builder)).isSameInstanceAs(initial);
        }
      }
    }
  }

  @Test
  public void testParseExceptionHelpersUseTheSameIoPolicy() {
    TestMapParsing.Builder builder = TestMapParsing.newBuilder().setValue(7);
    TestMapParsing message = builder.buildPartial();
    AbstractParser<?> parser = (AbstractParser<?>) TestMapParsing.parser();
    for (IOException failure :
        new IOException[] {
          new IOException("io failure"), new InvalidProtocolBufferException("wire failure")
        }) {
      InvalidProtocolBufferException direct =
          assertThrows(
              InvalidProtocolBufferException.class,
              () -> GeneratedMessageParser.throwParseException(failure, message));
      assertThat(direct.getUnfinishedMessage()).isSameInstanceAs(message);
      InvalidProtocolBufferException wrapped =
          assertThrows(
              InvalidProtocolBufferException.class,
              () -> parser.wrapAndThrowParseException(failure, builder));
      assertThat(wrapped.getUnfinishedMessage()).isEqualTo(message);
      if (failure instanceof InvalidProtocolBufferException) {
        assertThat(direct).isSameInstanceAs(failure);
        assertThat(wrapped).isSameInstanceAs(failure);
      } else {
        assertThat(direct).hasCauseThat().isSameInstanceAs(failure);
        assertThat(wrapped).hasCauseThat().isSameInstanceAs(failure);
      }
    }
    RuntimeException unexpected = new IllegalStateException("unexpected failure");
    assertThat(
            assertThrows(
                IllegalStateException.class,
                () -> parser.wrapAndThrowParseException(unexpected, null)))
        .isSameInstanceAs(unexpected);
    InvalidProtocolBufferException uninitialized =
        assertThrows(
            InvalidProtocolBufferException.class,
            () ->
                parser.wrapAndThrowParseException(
                    new UninitializedMessageException(message), builder));
    assertThat(uninitialized.getUnfinishedMessage()).isEqualTo(message);
  }

  @Test
  public void testPackedReaderBindings() throws Exception {
    GeneratedMessageParser<?> parser = (GeneratedMessageParser<?>) TestMapParsing.parser();
    assertThat(parser.readPackedInt32(CodedInputStream.newInstance(new byte[] {2, 1, 2})))
        .containsExactly(1, 2)
        .inOrder();
    assertThat(parser.readPackedInt64(CodedInputStream.newInstance(new byte[] {2, 1, 2})))
        .containsExactly(1L, 2L)
        .inOrder();
    assertThat(parser.readPackedBool(CodedInputStream.newInstance(new byte[] {2, 0, 1})))
        .containsExactly(false, true)
        .inOrder();
    byte[] floats =
        java.nio.ByteBuffer.allocate(9)
            .order(java.nio.ByteOrder.LITTLE_ENDIAN)
            .put((byte) 8)
            .putFloat(1.0f)
            .putFloat(2.0f)
            .array();
    assertThat(parser.readPackedFloat(CodedInputStream.newInstance(floats)))
        .containsExactly(1.0f, 2.0f)
        .inOrder();
    byte[] doubles =
        java.nio.ByteBuffer.allocate(17)
            .order(java.nio.ByteOrder.LITTLE_ENDIAN)
            .put((byte) 16)
            .putDouble(1.0)
            .putDouble(2.0)
            .array();
    assertThat(parser.readPackedDouble(CodedInputStream.newInstance(doubles)))
        .containsExactly(1.0, 2.0)
        .inOrder();
  }

  @Test
  public void testForcedBuilderStorageUsesTheSameInitializationAndHandoff() throws Exception {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    UnittestProto3.TestAllTypes.NestedMessage child =
        UnittestProto3.TestAllTypes.NestedMessage.newBuilder().setBb(1).build();
    output.writeMessage(UnittestProto3.TestAllTypes.REPEATED_NESTED_MESSAGE_FIELD_NUMBER, child);
    output.writeMessage(UnittestProto3.TestAllTypes.ONEOF_NESTED_MESSAGE_FIELD_NUMBER, child);
    output.writeMessage(UnittestProto3.TestAllTypes.ONEOF_NESTED_MESSAGE_FIELD_NUMBER, child);
    output.flush();
    for (boolean forced : new boolean[] {false, true}) {
      GeneratedMessage.setAlwaysUseFieldBuildersForTesting(forced);
      try {
        for (byte[] wire : new byte[][] {new byte[0], bytes.toByteArray()}) {
          UnittestProto3.TestAllTypes expected =
              UnittestProto3.TestAllTypes.newBuilder().mergeFrom(wire).buildPartial();
          assertThat(UnittestProto3.TestAllTypes.parser().parsePartialFrom(wire))
              .isEqualTo(expected);
        }
      } finally {
        GeneratedMessage.setAlwaysUseFieldBuildersForTesting(false);
      }
    }
  }

  @Test
  public void testHandoffAdoptsLiveRepeatedFieldBuilder() throws Exception {
    TestAllTypes.Builder original = TestAllTypes.newBuilder();
    TestAllTypes.NestedMessage.Builder child = original.addRepeatedNestedMessageBuilder().setBb(1);
    TestAllTypes.Builder transferred = handoffDecodedState(original);
    assertThat(fieldStorage(transferred, "repeatedNestedMessageBuilder_"))
        .isSameInstanceAs(fieldStorage(original, "repeatedNestedMessageBuilder_"));
    TestUtil.MockBuilderParent parent = new TestUtil.MockBuilderParent();
    java.lang.reflect.Field builderParent =
        GeneratedMessage.Builder.class.getDeclaredField("builderParent");
    builderParent.setAccessible(true);
    builderParent.set(transferred, parent);
    TestAllTypes first = transferred.build();
    child.setBb(2);
    assertThat(parent.getInvalidationCount()).isEqualTo(1);
    assertThat(first.getRepeatedNestedMessage(0).getBb()).isEqualTo(1);
    assertThat(transferred.build().getRepeatedNestedMessage(0).getBb()).isEqualTo(2);
  }

  @Test
  public void testExtensionHandoffDoesNotMergeCompletedMessageState() {
    UnknownFieldSet unknown = UnknownFieldSet.newBuilder().mergeVarintField(99, 1).build();
    TestExtensionParsing message =
        TestExtensionParsing.newBuilder().setUnknownFields(unknown).build();
    ExtensionHandoffBuilder builder = new ExtensionHandoffBuilder(message);
    assertThat(builder.getUnknownFields()).isSameInstanceAs(unknown);
  }

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
  public void testHandoffConstructorAssignsParserLocalsDirectly() throws Exception {
    TestMapParsing value = TestMapParsing.newBuilder().setValue(1).build();
    TestMapParsing.Builder source =
        TestMapParsing.newBuilder()
            .setChild(value)
            .setSelected(value)
            .putChildren("key", value)
            .setText("before");
    Object map = fieldStorage(source, "children_");
    TestMapParsing parsed = source.build();
    SingleFieldBuilder<?, ?, ?> child =
        new SingleFieldBuilder<>(TestMapParsing.newBuilder().setValue(2).build(), null, false);
    SingleFieldBuilder<?, ?, ?> selected =
        new SingleFieldBuilder<>(TestMapParsing.newBuilder().setValue(3).build(), null, false);
    UnknownFieldSet.Builder unknown = UnknownFieldSet.newBuilder().mergeVarintField(100, 4);
    java.lang.reflect.Constructor<TestMapParsing.Builder> constructor =
        TestMapParsing.Builder.class.getDeclaredConstructor(
            TestMapParsing.class,
            SingleFieldBuilder[].class,
            UnknownFieldSet.Builder.class,
            Object[].class);
    constructor.setAccessible(true);
    TestMapParsing.Builder builder =
        constructor.newInstance(
            parsed,
            new SingleFieldBuilder<?, ?, ?>[] {child, selected},
            unknown,
            new Object[] {map, "local text"});
    assertThat(fieldStorage(builder, "children_")).isSameInstanceAs(map);
    assertThat(fieldStorage(builder, "childBuilder_")).isSameInstanceAs(child);
    assertThat(fieldStorage(builder, "selectedBuilder_")).isSameInstanceAs(selected);
    assertThat(fieldStorage(builder, "child_")).isNull();
    assertThat(fieldStorage(builder, "choice_")).isNull();
    assertThat(fieldStorage(builder, "text_")).isEqualTo("local text");
    java.lang.reflect.Field unknownStorage =
        GeneratedMessage.Builder.class.getDeclaredField("unknownFieldsOrBuilder");
    unknownStorage.setAccessible(true);
    assertThat(unknownStorage.get(builder)).isSameInstanceAs(unknown);
    assertThat(builder.getChild().getValue()).isEqualTo(2);
    assertThat(builder.getSelected().getValue()).isEqualTo(3);
  }

  @Test
  public void testRepeatedChildRetainsBuilderStateForContinuation() throws Exception {
    TestMapParsing value = TestMapParsing.newBuilder().setValue(1).build();
    TestMapParsing input =
        TestMapParsing.newBuilder()
            .setChild(TestMapParsing.newBuilder().putChildren("key", value))
            .build();
    TestMapParsing.Builder expected = TestMapParsing.newBuilder().mergeFrom(input.toByteArray());
    TestMapParsing.Builder first = handoffDecodedState(expected);
    assertThat(fieldStorage(expected, "childBuilder_")).isNull();
    assertThat(fieldStorage(first, "childBuilder_")).isNull();
    TestMapParsing.Builder builder =
        handoffDecodedState(
            TestMapParsing.newBuilder()
                .mergeFrom(input.toByteString().concat(input.toByteString())));
    SingleFieldBuilder<?, ?, ?> child =
        (SingleFieldBuilder<?, ?, ?>) fieldStorage(builder, "childBuilder_");
    assertThat(fieldStorage(child.getBuilder(), "children_")).isInstanceOf(MapFieldBuilder.class);
    builder.mergeFrom(input.toByteArray());
    assertThat(builder.build()).isEqualTo(input);
    TestMapParsing.Builder inner = builder.getChildBuilder();
    TestUtil.MockBuilderParent parent = new TestUtil.MockBuilderParent();
    java.lang.reflect.Field builderParent =
        GeneratedMessage.Builder.class.getDeclaredField("builderParent");
    builderParent.setAccessible(true);
    builderParent.set(builder, parent);
    assertThat(builder.build()).isEqualTo(input);
    inner.setValue(2);
    assertThat(parent.getInvalidationCount()).isEqualTo(1);
    assertThat(builder.build().getChild().getValue()).isEqualTo(2);
  }

  @Test
  public void testOneofParsingRetainsBuilderBookkeeping() throws Exception {
    TestExtensionParsing child = TestExtensionParsing.newBuilder().setValue(1).build();
    TestExtensionParsing[] values = {
      TestExtensionParsing.newBuilder().setSelected(child).build(),
      TestExtensionParsing.newBuilder().setOther(child).build(),
      TestExtensionParsing.newBuilder().setText(ByteString.copyFromUtf8("text")).build()
    };
    for (TestExtensionParsing first : values) {
      for (TestExtensionParsing second : values) {
        byte[] bytes = first.toByteString().concat(second.toByteString()).toByteArray();
        TestExtensionParsing parsed = TestExtensionParsing.parseFrom(bytes);
        TestExtensionParsing.Builder expected = TestExtensionParsing.newBuilder().mergeFrom(bytes);
        assertThat(fieldStorage(parsed, "bitField0_"))
            .isEqualTo(fieldStorage(expected, "bitField0_"));
      }
    }
  }

  @Test
  public void testHandoffDoesNotMarkDefaultImplicitScalarsForPublication() throws Exception {
    TestMapParsing parsed = TestMapParsing.parseFrom(new byte[0]);
    TestMapParsing.Builder builder = copyForParsing(parsed, TestMapParsing.Builder.class);
    assertThat(fieldStorage(builder, "bitField0_")).isEqualTo(0);
    assertThat(builder.build()).isEqualTo(parsed);
  }

  @Test
  public void testHandoffPreservesImplicitScalarContinuation() throws Exception {
    UnittestProto3.TestAllTypes values =
        UnittestProto3.TestAllTypes.newBuilder()
            .setOptionalInt32(1)
            .setOptionalInt64(2)
            .setOptionalUint32(3)
            .setOptionalUint64(4)
            .setOptionalSint32(-5)
            .setOptionalSint64(-6)
            .setOptionalFixed32(7)
            .setOptionalFixed64(8)
            .setOptionalSfixed32(-9)
            .setOptionalSfixed64(-10)
            .setOptionalFloat(1.5f)
            .setOptionalDouble(2.5)
            .setOptionalBool(true)
            .setOptionalString("text")
            .setOptionalBytes(ByteString.copyFromUtf8("bytes"))
            .setOptionalNestedEnum(UnittestProto3.TestAllTypes.NestedEnum.FOO)
            .setOptionalForeignEnum(UnittestProto3.ForeignEnum.FOREIGN_FOO)
            .setOptionalStringPiece("piece")
            .setOptionalCord("cord")
            .build();
    for (UnittestProto3.TestAllTypes nonDefaults :
        new UnittestProto3.TestAllTypes[] {
          values,
          values.toBuilder()
              .setOptionalFloat(-0.0f)
              .setOptionalDouble(-0.0)
              .setOptionalNestedEnumValue(12345)
              .build(),
          values.toBuilder().setOptionalFloat(Float.NaN).setOptionalDouble(Double.NaN).build()
        }) {
      for (Descriptors.FieldDescriptor field : values.getDescriptorForType().getFields()) {
        if (field.isRepeated() || field.hasPresence()) {
          continue;
        }
        byte[][] occurrences = {
          new byte[0], encodeField(field, field.getDefaultValue()),
          encodeField(field, nonDefaults.getField(field))
        };
        for (byte[] first : occurrences) {
          UnittestProto3.TestAllTypes parsed = UnittestProto3.TestAllTypes.parseFrom(first);
          ByteString snapshot = parsed.toByteString();
          UnittestProto3.TestAllTypes.Builder decoded =
              UnittestProto3.TestAllTypes.newBuilder().mergeFrom(first);
          UnittestProto3.TestAllTypes.Builder copied =
              copyForParsing(decoded.build(), UnittestProto3.TestAllTypes.Builder.class);
          assertThat(fieldStorage(copied, "bitField0_"))
              .isEqualTo(fieldStorage(decoded, "bitField0_"));
          assertThat(copied.build()).isEqualTo(decoded.build());
          for (byte[] next : occurrences) {
            UnittestProto3.TestAllTypes.Builder builder =
                copyForParsing(parsed, UnittestProto3.TestAllTypes.Builder.class);
            UnittestProto3.TestAllTypes expected =
                UnittestProto3.TestAllTypes.newBuilder().mergeFrom(first).mergeFrom(next).build();
            assertThat(fieldStorage(builder, "bitField0_"))
                .isEqualTo(fieldStorage(decoded, "bitField0_"));
            UnittestProto3.TestAllTypes actual = builder.mergeFrom(next).build();
            assertThat(actual).isEqualTo(expected);
            assertThat(actual.toByteString()).isEqualTo(expected.toByteString());
            assertThat(parsed.toByteString()).isEqualTo(snapshot);
          }
        }
      }
    }
  }

  @Test
  public void testRetainedBuilderKeepsMutableCollectionsAndUnknownFields() throws Exception {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    output.writeInt32(UnittestProto3.TestAllTypes.OPTIONAL_INT32_FIELD_NUMBER, 0);
    output.writeInt32(UnittestProto3.TestAllTypes.REPEATED_INT32_FIELD_NUMBER, 1);
    output.writeMessage(
        UnittestProto3.TestAllTypes.ONEOF_NESTED_MESSAGE_FIELD_NUMBER,
        UnittestProto3.TestAllTypes.NestedMessage.newBuilder().setBb(1).build());
    output.writeMessage(
        UnittestProto3.TestAllTypes.ONEOF_NESTED_MESSAGE_FIELD_NUMBER,
        UnittestProto3.TestAllTypes.NestedMessage.newBuilder().setBb(2).build());
    output.writeInt32(20000, 1);
    output.flush();
    UnittestProto3.TestAllTypes.Builder builder =
        handoffDecodedState(
            UnittestProto3.TestAllTypes.newBuilder().mergeFrom(bytes.toByteArray()));
    Internal.IntList values = (Internal.IntList) fieldStorage(builder, "repeatedInt32_");
    assertThat(values.isModifiable()).isTrue();
    java.lang.reflect.Field unknownField =
        GeneratedMessage.Builder.class.getDeclaredField("unknownFieldsOrBuilder");
    unknownField.setAccessible(true);
    Object unknown = unknownField.get(builder);
    assertThat(unknown).isInstanceOf(UnknownFieldSet.Builder.class);

    UnittestProto3.TestAllTypes.Builder expected =
        UnittestProto3.TestAllTypes.newBuilder().mergeFrom(bytes.toByteArray());
    int bitFields = (UnittestProto3.TestAllTypes.getDescriptor().getFields().size() + 31) / 32;
    for (int i = 0; i < bitFields; i++) {
      assertThat(fieldStorage(builder, "bitField" + i + "_"))
          .isEqualTo(fieldStorage(expected, "bitField" + i + "_"));
    }
    byte[] suffix = encodeField(
        UnittestProto3.TestAllTypes.getDescriptor()
            .findFieldByNumber(UnittestProto3.TestAllTypes.REPEATED_INT32_FIELD_NUMBER), 2);
    builder.mergeFrom(suffix);
    builder.mergeUnknownFields(UnknownFieldSet.newBuilder().mergeVarintField(20000, 2).build());
    assertThat(fieldStorage(builder, "repeatedInt32_")).isSameInstanceAs(values);
    assertThat(unknownField.get(builder)).isSameInstanceAs(unknown);
    expected.mergeFrom(suffix)
        .mergeUnknownFields(UnknownFieldSet.newBuilder().mergeVarintField(20000, 2).build());
    UnittestProto3.TestAllTypes result = builder.build();
    assertThat(result).isEqualTo(expected.build());
    assertThat(values.isModifiable()).isFalse();
    builder.addRepeatedInt32(3);
    builder.mergeUnknownFields(UnknownFieldSet.newBuilder().mergeVarintField(20000, 3).build());
    assertThat(result.getRepeatedInt32List()).containsExactly(1, 2).inOrder();
    assertThat(result.getUnknownFields().getField(20000).getVarintList())
        .containsExactly(1L, 2L).inOrder();
  }

  @Test
  public void testRetainedBuilderKeepsMutableMap() throws Exception {
    ByteString wire =
        TestExtensionParsing.newBuilder()
            .putEntries("first", 1)
            .setSelected(TestExtensionParsing.newBuilder().addValues(1))
            .build().toByteString()
            .concat(TestExtensionParsing.newBuilder()
                .setSelected(TestExtensionParsing.newBuilder().addValues(2))
                .build().toByteString());
    TestExtensionParsing.Builder builder =
        handoffDecodedState(TestExtensionParsing.newBuilder().mergeFrom(wire));
    MapField<?, ?> entries = (MapField<?, ?>) fieldStorage(builder, "entries_");
    assertThat(entries.isMutable()).isTrue();
    builder.putEntries("second", 2);
    assertThat(fieldStorage(builder, "entries_")).isSameInstanceAs(entries);
    TestExtensionParsing result = builder.build();
    assertThat(entries.isMutable()).isFalse();
    builder.putEntries("third", 3);
    assertThat(result.getEntriesMap()).containsExactly("first", 1, "second", 2);
    assertThat(result.getSelected().getValuesList()).containsExactly(1, 2).inOrder();
  }

  private static byte[] encodeField(Descriptors.FieldDescriptor field, Object value)
      throws Exception {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    FieldSet.writeElement(output, field.getLiteType(), field.getNumber(), value);
    output.flush();
    return bytes.toByteArray();
  }

  @Test
  public void testHandoffSharesRepeatedCollectionsUntilMutation() throws Exception {
    TestAllTypes first =
        TestAllTypes.newBuilder()
            .addRepeatedInt32(1)
            .addRepeatedString("first")
            .addRepeatedBytes(ByteString.copyFromUtf8("first"))
            .addRepeatedNestedEnum(TestAllTypes.NestedEnum.FOO)
            .addRepeatedNestedMessage(TestAllTypes.NestedMessage.newBuilder().setBb(1))
            .build();
    byte[] wire = first.toByteArray();
    TestAllTypes parsed = TestAllTypes.parseFrom(wire);
    TestAllTypes.Builder builder = copyForParsing(parsed, TestAllTypes.Builder.class);
    String[] fields = {
      "repeatedInt32_", "repeatedString_", "repeatedBytes_",
      "repeatedNestedEnum_", "repeatedNestedMessage_"
    };
    Object[] storage = new Object[fields.length];
    for (int i = 0; i < fields.length; i++) {
      storage[i] = fieldStorage(builder, fields[i]);
      assertThat(storage[i]).isSameInstanceAs(fieldStorage(parsed, fields[i]));
      assertThat(((Internal.ProtobufList<?>) storage[i]).isModifiable()).isFalse();
    }
    builder.addRepeatedInt32(2);
    builder.addRepeatedString("second");
    builder.addRepeatedBytes(ByteString.copyFromUtf8("second"));
    builder.addRepeatedNestedEnum(TestAllTypes.NestedEnum.BAR);
    builder.addRepeatedNestedMessage(TestAllTypes.NestedMessage.newBuilder().setBb(2));
    for (int i = 0; i < fields.length; i++) {
      Object mutable = fieldStorage(builder, fields[i]);
      assertThat(mutable).isNotSameInstanceAs(storage[i]);
      assertThat(((Internal.ProtobufList<?>) mutable).isModifiable()).isTrue();
      storage[i] = mutable;
    }
    TestAllTypes result = builder.build();
    assertThat(result.getRepeatedInt32List()).containsExactly(1, 2).inOrder();
    assertThat(result.getRepeatedStringList()).containsExactly("first", "second").inOrder();
    assertThat(result.getRepeatedBytesList())
        .containsExactly(ByteString.copyFromUtf8("first"), ByteString.copyFromUtf8("second"))
        .inOrder();
    assertThat(result.getRepeatedNestedEnumList())
        .containsExactly(TestAllTypes.NestedEnum.FOO, TestAllTypes.NestedEnum.BAR).inOrder();
    assertThat(result.getRepeatedNestedMessage(1).getBb()).isEqualTo(2);
    assertThat(parsed.toByteArray()).isEqualTo(wire);
    builder.addRepeatedInt32(3);
    assertThat(result.getRepeatedInt32List()).containsExactly(1, 2).inOrder();
    for (Object list : storage) {
      assertThat(((Internal.ProtobufList<?>) list).isModifiable()).isFalse();
    }
  }

  @Test
  public void testHandoffSharesScalarMapUntilMutation() throws Exception {
    TestExtensionParsing first =
        TestExtensionParsing.newBuilder().putEntries("first", 1).build();
    TestExtensionParsing parsed = TestExtensionParsing.parseFrom(first.toByteArray());
    TestExtensionParsing.Builder builder =
        copyForParsing(parsed, TestExtensionParsing.Builder.class);
    MapField<?, ?> shared = (MapField<?, ?>) fieldStorage(builder, "entries_");
    assertThat(shared).isSameInstanceAs(fieldStorage(parsed, "entries_"));
    assertThat(shared.isMutable()).isFalse();
    builder.putEntries("second", 2);
    MapField<?, ?> storage = (MapField<?, ?>) fieldStorage(builder, "entries_");
    assertThat(storage).isNotSameInstanceAs(shared);
    assertThat(storage.isMutable()).isTrue();
    TestExtensionParsing result = builder.build();
    assertThat(result.getEntriesMap()).containsExactly("first", 1, "second", 2);
    assertThat(parsed).isEqualTo(first);
    assertThat(storage.isMutable()).isFalse();
    builder.putEntries("third", 3);
    assertThat(result.getEntriesMap()).containsExactly("first", 1, "second", 2);
  }

  @Test
  public void testHandoffPreservesSharedEmptyDefaults() throws Exception {
    TestAllTypes defaults = TestAllTypes.getDefaultInstance();
    TestAllTypes.Builder builder = copyForParsing(defaults, TestAllTypes.Builder.class);
    assertThat(fieldStorage(builder, "repeatedInt32_"))
        .isSameInstanceAs(defaults.getRepeatedInt32List());
    builder.addRepeatedInt32(1);
    assertThat(builder.build().getRepeatedInt32List()).containsExactly(1);
    assertThat(defaults.getRepeatedInt32List()).isEmpty();
    assertThat(((Internal.ProtobufList<?>) defaults.getRepeatedInt32List()).isModifiable())
        .isFalse();
  }

  private static final class ExtensionHandoffBuilder
      extends GeneratedMessage.ExtendableBuilder<TestExtensionParsing, ExtensionHandoffBuilder> {
    ExtensionHandoffBuilder(TestExtensionParsing message) {
      super(message, null);
    }

    @Override
    protected void mergeExtensionFields(GeneratedMessage.ExtendableMessage<?> message) {
      throw new AssertionError("Parser handoff must not reconstruct extension state");
    }

    @Override
    protected GeneratedMessage.FieldAccessorTable internalGetFieldAccessorTable() {
      throw new UnsupportedOperationException();
    }

    @Override
    public Descriptors.Descriptor getDescriptorForType() {
      return TestExtensionParsing.getDescriptor();
    }

    @Override
    public TestExtensionParsing getDefaultInstanceForType() {
      return TestExtensionParsing.getDefaultInstance();
    }

    @Override
    public TestExtensionParsing build() {
      throw new UnsupportedOperationException();
    }

    @Override
    public TestExtensionParsing buildPartial() {
      throw new UnsupportedOperationException();
    }
  }

  private static <B> B copyForParsing(Message message, Class<B> builderType) throws Exception {
    java.lang.reflect.Constructor<B> constructor =
        builderType.getDeclaredConstructor(
            message.getClass(),
            SingleFieldBuilder[].class,
            UnknownFieldSet.Builder.class,
            Object[].class);
    constructor.setAccessible(true);
    return constructor.newInstance(message, null, null, null);
  }

  @SuppressWarnings("unchecked")
  private static <B extends GeneratedMessage.Builder<B>> B handoffDecodedState(B decoded)
      throws Exception {
    Class<?> messageType = decoded.getDefaultInstanceForType().getClass();
    java.lang.reflect.Constructor<?> messageConstructor = messageType.getDeclaredConstructor();
    messageConstructor.setAccessible(true);
    GeneratedMessage instance = (GeneratedMessage) messageConstructor.newInstance();
    for (java.lang.reflect.Field field : decoded.getClass().getDeclaredFields()) {
      if (java.lang.reflect.Modifier.isStatic(field.getModifiers())
          || SingleFieldBuilder.class.isAssignableFrom(field.getType())
          || RepeatedFieldBuilder.class.isAssignableFrom(field.getType())) {
        continue;
      }
      field.setAccessible(true);
      Object value = field.get(decoded);
      java.lang.reflect.Field messageField = messageType.getDeclaredField(field.getName());
      messageField.setAccessible(true);
      if (!(value instanceof MapFieldBuilder)) messageField.set(instance, value);
    }
    java.util.List<Object> locals = new java.util.ArrayList<>();
    java.util.List<SingleFieldBuilder<?, ?, ?>> children = new java.util.ArrayList<>();
    for (Descriptors.FieldDescriptor field : decoded.getDescriptorForType().getFields()) {
      String name = field.getJsonName();
      if (field.getType() == Descriptors.FieldDescriptor.Type.GROUP
          && field.getName().equalsIgnoreCase(field.getMessageType().getName())) {
        String group = field.getMessageType().getName();
        name = Character.toLowerCase(group.charAt(0)) + group.substring(1);
      }
      if ((field.isMapField()
              && field.getMessageType().findFieldByNumber(2).getJavaType()
                  == Descriptors.FieldDescriptor.JavaType.MESSAGE)
          || (!field.isRepeated()
              && field.getContainingOneof() == null
              && field.getJavaType() == Descriptors.FieldDescriptor.JavaType.STRING)) {
        locals.add(fieldStorage(decoded, name + "_"));
      } else if (field.isRepeated()
          && !field.isMapField()
          && field.getJavaType() == Descriptors.FieldDescriptor.JavaType.MESSAGE) {
        locals.add(fieldStorage(decoded, name + "Builder_"));
      }
      if (!field.isRepeated()
          && field.getJavaType() == Descriptors.FieldDescriptor.JavaType.MESSAGE) {
        children.add((SingleFieldBuilder<?, ?, ?>) fieldStorage(decoded, name + "Builder_"));
      }
    }
    java.lang.reflect.Field unknownStorage =
        GeneratedMessage.Builder.class.getDeclaredField("unknownFieldsOrBuilder");
    unknownStorage.setAccessible(true);
    Object unknown = unknownStorage.get(decoded);
    if (unknown instanceof UnknownFieldSet) instance.unknownFields = (UnknownFieldSet) unknown;
    java.lang.reflect.Constructor<?> constructor =
        decoded
            .getClass()
            .getDeclaredConstructor(
                messageType,
                SingleFieldBuilder[].class,
                UnknownFieldSet.Builder.class,
                Object[].class);
    constructor.setAccessible(true);
    return (B)
        constructor.newInstance(
            instance,
            children.toArray(new SingleFieldBuilder<?, ?, ?>[0]),
            unknown instanceof UnknownFieldSet.Builder ? unknown : null,
            locals.toArray());
  }

  private static Object fieldStorage(Object messageOrBuilder, String name) throws Exception {
    java.lang.reflect.Field field = messageOrBuilder.getClass().getDeclaredField(name);
    field.setAccessible(true);
    return field.get(messageOrBuilder);
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
  public void testFirstOneofChildRetainsUnpublishedState() throws Exception {
    TestExtensionParsing child =
        TestExtensionParsing.newBuilder()
            .setValue(1)
            .addValues(2)
            .putEntries("key", 3)
            .setUnknownFields(UnknownFieldSet.newBuilder().mergeVarintField(99, 4).build())
            .build();
    byte[] wire = TestExtensionParsing.newBuilder().setSelected(child).build().toByteArray();
    TestExtensionParsing.Builder expected = TestExtensionParsing.newBuilder().mergeFrom(wire);
    assertParsingStateEquals(expected, handoffDecodedState(expected));
  }

  @Test
  public void testNestedOneofHistoryMatchesBuilderAcrossChildKinds() throws Exception {
    ByteString firstChild =
        TestExtensionParsing.newBuilder()
            .setSelected(TestExtensionParsing.newBuilder().addValues(1))
            .build()
            .toByteString()
            .concat(
                TestExtensionParsing.newBuilder()
                    .setOther(TestExtensionParsing.newBuilder().addValues(10))
                    .build()
                    .toByteString());
    TestExtensionParsing secondChild =
        TestExtensionParsing.newBuilder()
            .setSelected(TestExtensionParsing.newBuilder().addValues(2))
            .build();
    for (int field :
        new int[] {
          TestExtensionParsing.CHILD_FIELD_NUMBER,
          TestExtensionParsing.SELECTED_FIELD_NUMBER,
          TestExtensionParsing.OTHER_FIELD_NUMBER,
          TestExtensionParsing.GROUP_FIELD_NUMBER
        }) {
      for (boolean retainSibling : new boolean[] {false, true}) {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        CodedOutputStream output = CodedOutputStream.newInstance(bytes);
        if (retainSibling) {
          output.writeMessage(
              TestExtensionParsing.CHILD_FIELD_NUMBER, TestExtensionParsing.getDefaultInstance());
          output.writeMessage(
              TestExtensionParsing.CHILD_FIELD_NUMBER, TestExtensionParsing.getDefaultInstance());
        }
        if (field == TestExtensionParsing.GROUP_FIELD_NUMBER) {
          output.writeTag(field, WireFormat.WIRETYPE_START_GROUP);
          output.writeRawBytes(firstChild);
          output.writeTag(field, WireFormat.WIRETYPE_END_GROUP);
          output.writeGroup(field, secondChild);
        } else {
          output.writeBytes(field, firstChild);
          output.writeMessage(field, secondChild);
        }
        output.flush();
        byte[] wire = bytes.toByteArray();
        TestExtensionParsing.Builder expected = TestExtensionParsing.newBuilder().mergeFrom(wire);
        assertParsingStateEquals(expected, handoffDecodedState(expected));
        assertWithMessage("field=%s, retainSibling=%s", field, retainSibling)
            .that(TestExtensionParsing.parseFrom(wire))
            .isEqualTo(expected.build());
        byte[] truncated = Arrays.copyOf(wire, wire.length + 1);
        truncated[wire.length] = (byte) 0x80;
        InvalidProtocolBufferException failure =
            assertThrows(
                InvalidProtocolBufferException.class,
                () -> TestExtensionParsing.parseFrom(truncated));
        assertThat(failure.getUnfinishedMessage()).isEqualTo(expected.build());
      }
    }
  }

  private static void assertParsingStateEquals(
      GeneratedMessage.Builder<?> expected, GeneratedMessage.Builder<?> actual) throws Exception {
    assertThat(actual).isNotNull();
    assertThat(actual.getClass()).isEqualTo(expected.getClass());
    java.lang.reflect.Field unknown =
        GeneratedMessage.Builder.class.getDeclaredField("unknownFieldsOrBuilder");
    unknown.setAccessible(true);
    assertThat(unknown.get(actual).getClass()).isEqualTo(unknown.get(expected).getClass());
    for (java.lang.reflect.Field field : expected.getClass().getDeclaredFields()) {
      if (java.lang.reflect.Modifier.isStatic(field.getModifiers())) continue;
      field.setAccessible(true);
      Object expectedValue = field.get(expected);
      Object actualValue = field.get(actual);
      if (expectedValue instanceof SingleFieldBuilder) {
        assertWithMessage(field.getName()).that(actualValue).isInstanceOf(SingleFieldBuilder.class);
        java.lang.reflect.Field builder = SingleFieldBuilder.class.getDeclaredField("builder");
        builder.setAccessible(true);
        assertParsingStateEquals(
            (GeneratedMessage.Builder<?>) builder.get(expectedValue),
            (GeneratedMessage.Builder<?>) builder.get(actualValue));
      } else {
        assertWithMessage(field.getName()).that(actualValue).isEqualTo(expectedValue);
        if (expectedValue instanceof Internal.ProtobufList) {
          assertThat(((Internal.ProtobufList<?>) actualValue).isModifiable())
              .isEqualTo(((Internal.ProtobufList<?>) expectedValue).isModifiable());
        } else if (expectedValue instanceof MapField) {
          assertThat(((MapField<?, ?>) actualValue).isMutable())
              .isEqualTo(((MapField<?, ?>) expectedValue).isMutable());
        }
      }
    }
  }

  @Test
  public void testNestedOneofHistorySurvivesOuterChildMerge() throws Exception {
    ByteString firstChild =
        TestExtensionParsing.newBuilder()
            .setSelected(TestExtensionParsing.newBuilder().addValues(1))
            .build()
            .toByteString()
            .concat(
                TestExtensionParsing.newBuilder()
                    .setOther(TestExtensionParsing.newBuilder().addValues(10))
                    .build()
                    .toByteString());
    TestExtensionParsing secondChild =
        TestExtensionParsing.newBuilder()
            .setSelected(TestExtensionParsing.newBuilder().addValues(2))
            .build();
    ExtensionRegistry registry = ExtensionRegistry.newInstance();
    GeneratedMessageParserTestProto.registerAllExtensions(registry);
    for (int extensionPosition = -1; extensionPosition < 3; extensionPosition++) {
      ByteArrayOutputStream bytes = new ByteArrayOutputStream();
      CodedOutputStream output = CodedOutputStream.newInstance(bytes);
      if (extensionPosition == 0) output.writeInt32(100, 7);
      output.writeBytes(TestExtensionParsing.SELECTED_FIELD_NUMBER, firstChild);
      if (extensionPosition == 1) output.writeInt32(100, 7);
      output.writeMessage(TestExtensionParsing.SELECTED_FIELD_NUMBER, secondChild);
      if (extensionPosition == 2) output.writeInt32(100, 7);
      output.flush();
      for (boolean wrapped : new boolean[] {false, true}) {
        byte[] wire = bytes.toByteArray();
        if (wrapped) {
          ByteArrayOutputStream parent = new ByteArrayOutputStream();
          CodedOutputStream wrapper = CodedOutputStream.newInstance(parent);
          wrapper.writeByteArray(TestExtensionParsing.CHILD_FIELD_NUMBER, wire);
          wrapper.flush();
          wire = parent.toByteArray();
        }
        TestExtensionParsing expected =
            TestExtensionParsing.newBuilder().mergeFrom(wire, registry).build();
        TestExtensionParsing parsed = TestExtensionParsing.parseFrom(wire, registry);
        TestExtensionParsing child = wrapped ? parsed.getChild() : parsed;
        assertThat(child.getSelected().getSelected().getValuesList())
            .containsExactly(1, 2)
            .inOrder();
        assertThat(parsed).isEqualTo(expected);
        assertThrows(
            UnsupportedOperationException.class,
            () -> child.getSelected().getSelected().getValuesList().clear());

        byte[] truncated = Arrays.copyOf(wire, wire.length + 1);
        truncated[wire.length] = (byte) 0x80;
        InvalidProtocolBufferException failure =
            assertThrows(
                InvalidProtocolBufferException.class,
                () -> TestExtensionParsing.parseFrom(truncated, registry));
        TestExtensionParsing unfinished = (TestExtensionParsing) failure.getUnfinishedMessage();
        assertThat(unfinished).isEqualTo(expected);
        TestExtensionParsing retained =
            (wrapped ? unfinished.getChild() : unfinished).getSelected().getSelected();
        assertThat(retained.getValuesList()).containsExactly(1, 2).inOrder();
        assertThrows(UnsupportedOperationException.class, () -> retained.getValuesList().clear());
        TestExtensionParsing.Builder continuation = unfinished.toBuilder();
        if (wrapped) continuation = continuation.getChildBuilder();
        continuation.getSelectedBuilder().getSelectedBuilder().addValues(3);
        assertThat(continuation.build().getSelected().getSelected().getValuesList())
            .containsExactly(1, 2, 3).inOrder();
        assertThat(retained.getValuesList()).containsExactly(1, 2).inOrder();
      }
    }
  }

  @Test
  public void testUnfinishedOneofAfterTruncatedString() throws Exception {
    assertUnfinishedOneofStringMatchesBuilder(new byte[0]);
  }

  @Test
  public void testUnfinishedOneofAfterInvalidUtf8String() throws Exception {
    assertUnfinishedOneofStringMatchesBuilder(new byte[] {(byte) 0xff});
  }

  private static void assertUnfinishedOneofStringMatchesBuilder(byte[] payload) throws Exception {
    UnittestProto3.TestAllTypes[] prefixes = {
      UnittestProto3.TestAllTypes.getDefaultInstance(),
      UnittestProto3.TestAllTypes.newBuilder().setOneofUint32(1).build(),
      UnittestProto3.TestAllTypes.newBuilder()
          .setOneofNestedMessage(UnittestProto3.TestAllTypes.NestedMessage.newBuilder().setBb(1))
          .build(),
      UnittestProto3.TestAllTypes.newBuilder().setOneofString("previous").build()
    };
    for (UnittestProto3.TestAllTypes prefix : prefixes) {
      ByteArrayOutputStream bytes = new ByteArrayOutputStream();
      prefix.writeTo(bytes);
      CodedOutputStream output = CodedOutputStream.newInstance(bytes);
      output.writeTag(
          UnittestProto3.TestAllTypes.ONEOF_STRING_FIELD_NUMBER,
          WireFormat.WIRETYPE_LENGTH_DELIMITED);
      // Declare one byte even when the truncated input omits it.
      output.writeUInt32NoTag(1);
      output.writeRawBytes(payload);
      output.flush();
      byte[] wire = bytes.toByteArray();
      UnittestProto3.TestAllTypes.Builder builder = UnittestProto3.TestAllTypes.newBuilder();
      assertThrows(InvalidProtocolBufferException.class, () -> builder.mergeFrom(wire));
      UnittestProto3.TestAllTypes expected = builder.buildPartial();
      assertThat(expected.getOneofFieldCase())
          .isEqualTo(UnittestProto3.TestAllTypes.OneofFieldCase.ONEOF_STRING);

      InvalidProtocolBufferException failure =
          assertThrows(
              InvalidProtocolBufferException.class,
              () -> UnittestProto3.TestAllTypes.parseFrom(wire));
      UnittestProto3.TestAllTypes partial =
          (UnittestProto3.TestAllTypes) failure.getUnfinishedMessage();
      assertThat(partial.getOneofFieldCase()).isEqualTo(expected.getOneofFieldCase());
    }
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
      assertExtensionPrefixesMatchBuilder(wire, registry, 0);
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
