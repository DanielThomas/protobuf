// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf;

import static com.google.common.io.ByteStreams.skipFully;
import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.protobuf.test.CodeSizeAccessors;
import com.google.protobuf.test.WideClosedEnum;
import com.google.protobuf.test.WideMessageFields;
import com.google.protobuf.test.WideMessageOneof;
import com.google.protobuf.test.WideOneof;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Regression tests for generated class-file limits and parsing method size. */
@RunWith(JUnit4.class)
public class GeneratedCodeSizeTest {
  @Test
  public void codeSizePrimitiveAccessorsPreservePresenceAndSnapshots() throws Exception {
    CodeSizeAccessors.Builder builder = CodeSizeAccessors.newBuilder().setNumber(0);
    CodeSizeAccessors snapshot = builder.build();
    assertThat(snapshot.hasNumber()).isTrue();
    assertThat(CodeSizeAccessors.parseFrom(snapshot.toByteArray())).isEqualTo(snapshot);
    assertThat(builder.clone().build()).isEqualTo(snapshot);

    builder.clearNumber().setSelectedNumber(123);
    assertThat(builder.hasNumber()).isFalse();
    assertThat(builder.getSelectedNumber()).isEqualTo(123);
    assertThat(snapshot.hasNumber()).isTrue();
    assertThat(snapshot.hasSelectedNumber()).isFalse();
  }

  @Test
  public void codeSizeEnumAccessorsPreservePresenceAndSnapshots() throws Exception {
    CodeSizeAccessors.Builder builder =
        CodeSizeAccessors.newBuilder()
            .setState(CodeSizeAccessors.State.ZERO)
            .setSelectedState(CodeSizeAccessors.State.ONE);
    CodeSizeAccessors snapshot = builder.build();
    assertThat(builder.hasState()).isTrue();
    assertThat(builder.getSelectedState()).isEqualTo(CodeSizeAccessors.State.ONE);
    assertThat(CodeSizeAccessors.parseFrom(snapshot.toByteArray())).isEqualTo(snapshot);
    builder.clearState().setSelectedNumber(123);
    assertThat(builder.hasState()).isFalse();
    assertThat(builder.getSelectedState()).isEqualTo(CodeSizeAccessors.State.ZERO);
    assertThat(snapshot.hasState()).isTrue();
    assertThat(snapshot.getSelectedState()).isEqualTo(CodeSizeAccessors.State.ONE);
  }

  @Test
  public void codeSizeStringAccessorsPreservePresenceAndSnapshots() throws Exception {
    CodeSizeAccessors.Builder builder =
        CodeSizeAccessors.newBuilder()
            .setText("first")
            .setSelectedTextBytes(ByteString.copyFromUtf8("last"));
    assertThat(builder.getSelectedText()).isEqualTo("last");
    assertThat(builder.getTextBytes()).isEqualTo(ByteString.copyFromUtf8("first"));
    CodeSizeAccessors snapshot = builder.build();
    assertThat(CodeSizeAccessors.parseFrom(snapshot.toByteArray())).isEqualTo(snapshot);
    builder.clearText().setSelectedNumber(123);
    assertThat(builder.hasText()).isFalse();
    assertThat(builder.getSelectedTextBytes()).isEqualTo(ByteString.EMPTY);
    assertThat(snapshot.getText()).isEqualTo("first");
    assertThat(snapshot.getSelectedTextBytes()).isEqualTo(ByteString.copyFromUtf8("last"));
  }

  @Test
  public void codeSizeChildBuildersDetachOnClear() throws Exception {
    CodeSizeAccessors.Builder builder = CodeSizeAccessors.newBuilder();
    CodeSizeAccessors.Child.Builder child = builder.getChildBuilder().setValue(123);
    builder.getSelectedChildBuilder().setValue(456);
    assertThat(builder.getChildOrBuilder().getValue()).isEqualTo(123);
    assertThat(builder.getSelectedChildOrBuilder().getValue()).isEqualTo(456);
    CodeSizeAccessors snapshot = builder.build();
    assertThat(CodeSizeAccessors.parseFrom(snapshot.toByteArray())).isEqualTo(snapshot);
    builder.clear();
    child.setValue(789);
    assertThat(builder.build()).isEqualTo(CodeSizeAccessors.getDefaultInstance());
    assertThat(snapshot.getChild().getValue()).isEqualTo(123);
    assertThat(snapshot.getSelectedChild().getValue()).isEqualTo(456);
  }

  @Test
  public void wideMessageOneofParserFitsHotSpotHugeMethodLimit() throws Exception {
    assertThat(methodCodeSize(WideMessageOneof.parser().getClass(), "parsePartialFrom"))
        .isAtMost(8000);
  }

  @Test
  public void wideMessageOneofParsesEveryAlternative() throws Exception {
    WideMessageOneof.Child child = WideMessageOneof.Child.newBuilder().addValues(17).build();
    for (Descriptors.FieldDescriptor field : WideMessageOneof.getDescriptor().getFields()) {
      WideMessageOneof expected = WideMessageOneof.newBuilder().setField(field, child).build();
      assertThat(WideMessageOneof.parseFrom(expected.toByteArray())).isEqualTo(expected);
    }
  }

  @Test
  public void wideMessageOneofRetainsMergedChildOnFailure() throws Exception {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    output.writeMessage(1, WideMessageOneof.Child.newBuilder().addValues(17).build());
    output.writeTag(1, WireFormat.WIRETYPE_LENGTH_DELIMITED);
    output.writeUInt32NoTag(5);
    output.writeInt32(1, 23);
    output.writeTag(2, WireFormat.WIRETYPE_LENGTH_DELIMITED);
    output.writeUInt32NoTag(5);
    output.writeRawByte(1);
    output.flush();

    InvalidProtocolBufferException failure = assertThrows(
        InvalidProtocolBufferException.class,
        () -> WideMessageOneof.parseFrom(bytes.toByteArray()));
    WideMessageOneof partial = (WideMessageOneof) failure.getUnfinishedMessage();
    assertThat(partial.getValue1().getValuesList()).containsExactly(17, 23).inOrder();
    assertThrows(UnsupportedOperationException.class,
        () -> partial.getValue1().getValuesList().add(42));
  }

  @Test
  public void wideMessageFieldsMergeEveryChild() throws Exception {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    WideMessageOneof.Child.Builder expected = WideMessageOneof.Child.newBuilder();
    for (int value : new int[] {17, 23}) {
      WideMessageOneof.Child child = WideMessageOneof.Child.newBuilder().addValues(value).build();
      for (Descriptors.FieldDescriptor field : WideMessageFields.getDescriptor().getFields()) {
        output.writeMessage(field.getNumber(), child);
      }
      output.flush();
      expected.addValues(value);

      WideMessageFields parsed = WideMessageFields.parseFrom(bytes.toByteArray());
      assertThat(parsed)
          .isEqualTo(WideMessageFields.newBuilder().mergeFrom(bytes.toByteArray()).build());
      assertThat(parsed.getAllFields()).hasSize(65);
      for (Object parsedChild : parsed.getAllFields().values()) {
        assertThat(parsedChild).isEqualTo(expected.build());
      }
    }
  }

  @Test
  public void wideMessageFieldsPublishMergedChildrenOnFailure() throws Exception {
    for (int number : new int[] {1, 32, 33, 64, 65}) {
      ByteArrayOutputStream bytes = new ByteArrayOutputStream();
      CodedOutputStream output = CodedOutputStream.newInstance(bytes);
      output.writeMessage(number, WideMessageOneof.Child.newBuilder().addValues(17).build());
      output.writeTag(number, WireFormat.WIRETYPE_LENGTH_DELIMITED);
      output.writeUInt32NoTag(5);
      output.writeInt32(1, 23);
      output.writeTag(2, WireFormat.WIRETYPE_LENGTH_DELIMITED);
      output.writeUInt32NoTag(5);
      output.writeRawByte(1);
      output.flush();

      InvalidProtocolBufferException failure = assertThrows(
          InvalidProtocolBufferException.class,
          () -> WideMessageFields.parseFrom(bytes.toByteArray()));
      WideMessageFields partial = (WideMessageFields) failure.getUnfinishedMessage();
      WideMessageFields.Builder builder = WideMessageFields.newBuilder();
      assertThrows(InvalidProtocolBufferException.class, () -> builder.mergeFrom(bytes.toByteArray()));
      assertThat(partial).isEqualTo(builder.buildPartial());
      WideMessageOneof.Child child = (WideMessageOneof.Child) partial.getField(
          WideMessageFields.getDescriptor().findFieldByNumber(number));
      assertThat(child.getValuesList()).containsExactly(17, 23).inOrder();
      assertThrows(UnsupportedOperationException.class, () -> child.getValuesList().add(42));
    }
  }

  @Test
  public void wideOneofParserFitsHotSpotHugeMethodLimit() throws Exception {
    assertThat(methodCodeSize(WideOneof.parser().getClass(), "parsePartialFrom")).isAtMost(8000);
  }

  @Test
  public void wideOneofParsesLastAlternative() throws Exception {
    WideOneof expected = WideOneof.newBuilder().setValue494(123).build();
    assertThat(WideOneof.parseFrom(expected.toByteArray())).isEqualTo(expected);
  }

  @Test
  @SuppressWarnings("deprecation")
  public void wideOneofStopsAtEndGroupAndPreservesUnknownFields() throws Exception {
    UnknownFieldSet unknown = UnknownFieldSet.newBuilder().mergeVarintField(20000, 456).build();
    WideOneof expected =
        WideOneof.newBuilder().setValue494(123).mergeUnknownFields(unknown).build();
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    output.writeTag(1, WireFormat.WIRETYPE_START_GROUP);
    expected.writeTo(output);
    output.writeTag(1, WireFormat.WIRETYPE_END_GROUP);
    output.writeInt32(2, 789);
    output.flush();

    for (boolean editing : new boolean[] {false, true}) {
      CodedInputStream input = CodedInputStream.newInstance(bytes.toByteArray());
      assertThat(input.readTag()).isEqualTo(WireFormat.makeTag(1, WireFormat.WIRETYPE_START_GROUP));
      WideOneof actual;
      if (editing) {
        WideOneof.Builder builder = WideOneof.newBuilder();
        input.readGroup(1, builder, ExtensionRegistryLite.getEmptyRegistry());
        actual = builder.build();
      } else {
        actual = input.readGroup(1, WideOneof.parser(), ExtensionRegistryLite.getEmptyRegistry());
      }
      assertThat(actual).isEqualTo(expected);
      assertThat(input.getLastTag()).isEqualTo(WireFormat.makeTag(1, WireFormat.WIRETYPE_END_GROUP));
      assertThat(input.readTag()).isEqualTo(WireFormat.makeTag(2, WireFormat.WIRETYPE_VARINT));
      assertThat(input.readInt32()).isEqualTo(789);
      assertThat(input.readTag()).isEqualTo(0);
    }
  }

  @Test
  public void wideClosedEnumParsesKnownValues() throws Exception {
    byte[] input = enumInput(1);
    WideClosedEnum parsed = WideClosedEnum.parseFrom(input);
    assertThat(parsed.getAllFields()).hasSize(900);
    assertThat(parsed.getState900()).isEqualTo(WideClosedEnum.Status.ONE);
    assertThat(parsed.toByteArray()).isEqualTo(input);
  }

  @Test
  public void wideClosedEnumRetainsUnknownValuesAcrossBuilderMerges() throws Exception {
    byte[] input = enumInput(123);
    WideClosedEnum parsed = WideClosedEnum.parseFrom(input);
    assertThat(parsed.getAllFields()).isEmpty();
    assertThat(parsed.getUnknownFields().asMap()).hasSize(900);
    assertThat(parsed.toByteArray()).isEqualTo(input);

    WideClosedEnum.Builder builder = parsed.toBuilder();
    builder.mergeFrom(enumInput(456));
    assertThat(builder.build().getUnknownFields().getField(900).getVarintList())
        .containsExactly(123L, 456L)
        .inOrder();
    assertThat(parsed.getUnknownFields().getField(900).getVarintList()).containsExactly(123L);
  }

  @Test
  public void wideClosedEnumPreservesUnknownValuesOnParseFailure() throws Exception {
    byte[] input = enumInput(123);
    WideClosedEnum expected = WideClosedEnum.parseFrom(input);
    byte[] truncated = Arrays.copyOf(input, input.length + 1);
    truncated[input.length] = (byte) 0x80;

    InvalidProtocolBufferException failure =
        assertThrows(InvalidProtocolBufferException.class, () -> WideClosedEnum.parseFrom(truncated));
    assertThat(failure.getUnfinishedMessage()).isEqualTo(expected);

    WideClosedEnum.Builder builder = WideClosedEnum.newBuilder();
    assertThrows(InvalidProtocolBufferException.class, () -> builder.mergeFrom(truncated));
    assertThat(builder.buildPartial()).isEqualTo(expected);
  }

  private static byte[] enumInput(int value) throws IOException {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    for (int number = 1; number <= 900; number++) {
      output.writeEnum(number, value);
    }
    output.flush();
    return bytes.toByteArray();
  }

  private static int methodCodeSize(Class<?> type, String methodName) throws IOException {
    String resourceName = "/" + type.getName().replace('.', '/') + ".class";
    try (InputStream resource = type.getResourceAsStream(resourceName);
        DataInputStream input = new DataInputStream(resource)) {
      assertThat(input.readInt()).isEqualTo(0xcafebabe);
      skipFully(input, 4);
      String[] constants = new String[input.readUnsignedShort()];
      for (int i = 1; i < constants.length; i++) {
        int tag = input.readUnsignedByte();
        switch (tag) {
          case 1:
            constants[i] = input.readUTF();
            break;
          case 3:
          case 4:
          case 9:
          case 10:
          case 11:
          case 12:
          case 17:
          case 18:
            skipFully(input, 4);
            break;
          case 5:
          case 6:
            skipFully(input, 8);
            i++;
            break;
          case 7:
          case 8:
          case 16:
          case 19:
          case 20:
            skipFully(input, 2);
            break;
          case 15:
            skipFully(input, 3);
            break;
          default:
            throw new IOException("Unknown constant pool tag: " + tag);
        }
      }
      skipFully(input, 6);
      skipFully(input, 2L * input.readUnsignedShort());
      int fields = input.readUnsignedShort();
      for (int i = 0; i < fields; i++) {
        skipFully(input, 6);
        skipAttributes(input);
      }
      int methods = input.readUnsignedShort();
      for (int i = 0; i < methods; i++) {
        input.readUnsignedShort();
        String name = constants[input.readUnsignedShort()];
        input.readUnsignedShort();
        int attributes = input.readUnsignedShort();
        for (int j = 0; j < attributes; j++) {
          String attribute = constants[input.readUnsignedShort()];
          long length = Integer.toUnsignedLong(input.readInt());
          if (name.equals(methodName) && attribute.equals("Code")) {
            skipFully(input, 4);
            return input.readInt();
          }
          skipFully(input, length);
        }
      }
    }
    throw new IOException("No bytecode found for " + type.getName() + "." + methodName);
  }

  private static void skipAttributes(DataInputStream input) throws IOException {
    int attributes = input.readUnsignedShort();
    for (int i = 0; i < attributes; i++) {
      input.readUnsignedShort();
      skipFully(input, Integer.toUnsignedLong(input.readInt()));
    }
  }
}
