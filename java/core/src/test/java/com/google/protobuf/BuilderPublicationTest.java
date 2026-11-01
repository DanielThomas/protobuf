// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.protobuf.Descriptors.FieldDescriptor;
import java.io.ByteArrayOutputStream;
import java.util.Map;
import map_test.MapTestProto.TestMap;
import map_test.MapTestProto.TestMap.MessageValue;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import proto2_unittest.UnittestOptimizeFor.TestOptimizedForSize;
import proto2_unittest.UnittestOptimizeFor.TestRequiredOptimizedForSize;
import proto2_unittest.UnittestProto;
import proto2_unittest.UnittestProto.TestAllExtensions;
import proto2_unittest.UnittestProto.TestAllTypes;
import proto2_unittest.UnittestProto.TestAllTypes.NestedMessage;
import proto2_unittest.UnittestProto.TestOneof2;
import proto2_unittest.UnittestProto.TestRequired;

@RunWith(JUnit4.class)
@SuppressWarnings("deprecation")
public final class BuilderPublicationTest {
  @Test
  public void singularChildEditsAloneUpdateSuccessiveBuilds() throws Exception {
    TestAllTypes.Builder parent = TestAllTypes.newBuilder();
    NestedMessage.Builder child = parent.getOptionalNestedMessageBuilder().setBb(1);
    TestAllTypes first = parent.build();
    Snapshot firstSnapshot = new Snapshot(first);

    child.setBb(200);
    TestAllTypes second = parent.buildPartial();
    Snapshot secondSnapshot = new Snapshot(second);
    assertValue(
        second,
        TestAllTypes.newBuilder()
            .setOptionalNestedMessage(NestedMessage.newBuilder().setBb(200))
            .build());

    child.setBb(30000);
    assertValue(
        parent.build(),
        TestAllTypes.newBuilder()
            .setOptionalNestedMessage(NestedMessage.newBuilder().setBb(30000))
            .build());
    firstSnapshot.assertUnchanged();
    secondSnapshot.assertUnchanged();
    assertThat(first.getOptionalNestedMessage().getBb()).isEqualTo(1);
    assertThat(second.getOptionalNestedMessage().getBb()).isEqualTo(200);
  }

  @Test
  public void grandchildEditsAloneInvalidateEveryAncestor() throws Exception {
    UnittestProto.NestedTestAllTypes.Builder parent = UnittestProto.NestedTestAllTypes.newBuilder();
    NestedMessage.Builder child =
        parent.getChildBuilder().getPayloadBuilder().getOptionalNestedMessageBuilder().setBb(1);
    UnittestProto.NestedTestAllTypes first = parent.build();
    Snapshot firstSnapshot = new Snapshot(first);

    child.setBb(200);
    UnittestProto.NestedTestAllTypes second = parent.buildPartial();
    Snapshot secondSnapshot = new Snapshot(second);
    assertThat(second.getChild().getPayload().getOptionalNestedMessage().getBb()).isEqualTo(200);

    child.setBb(30000);
    assertThat(parent.build().getChild().getPayload().getOptionalNestedMessage().getBb())
        .isEqualTo(30000);
    firstSnapshot.assertUnchanged();
    secondSnapshot.assertUnchanged();
  }

  @Test
  public void repeatedChildEditsAloneUpdateSuccessiveBuilds() throws Exception {
    TestAllTypes.Builder parent = TestAllTypes.newBuilder();
    NestedMessage.Builder child = parent.addRepeatedNestedMessageBuilder().setBb(1);
    TestAllTypes first = parent.buildPartial();
    Snapshot snapshot = new Snapshot(first);

    child.setBb(200);
    assertValue(
        parent.build(),
        TestAllTypes.newBuilder()
            .addRepeatedNestedMessage(NestedMessage.newBuilder().setBb(200))
            .build());
    parent.removeRepeatedNestedMessage(0);
    child.setBb(30000);
    assertValue(parent.build(), TestAllTypes.getDefaultInstance());
    snapshot.assertUnchanged();
    assertThat(first.getRepeatedNestedMessage(0).getBb()).isEqualTo(1);
    assertThrows(
        UnsupportedOperationException.class, () -> first.getRepeatedNestedMessageList().clear());
  }

  @Test
  public void childGetterPublishesAnIndependentValue() throws Exception {
    TestAllTypes.Builder parent = TestAllTypes.newBuilder();
    NestedMessage.Builder child = parent.getOptionalNestedMessageBuilder().setBb(1);
    NestedMessage first = parent.getOptionalNestedMessage();
    Snapshot snapshot = new Snapshot(first);

    child.setBb(200);
    assertValue(parent.getOptionalNestedMessage(), NestedMessage.newBuilder().setBb(200).build());
    assertValue(
        parent.build(),
        TestAllTypes.newBuilder()
            .setOptionalNestedMessage(NestedMessage.newBuilder().setBb(200))
            .build());
    snapshot.assertUnchanged();
    assertThat(first.getBb()).isEqualTo(1);
  }

  @Test
  public void obtainingChildBuilderAfterPublicationChangesOnlyTheNextMessage() throws Exception {
    TestAllTypes.Builder parent = TestAllTypes.newBuilder();
    TestAllTypes first = parent.build();
    Snapshot snapshot = new Snapshot(first);

    parent.getOptionalNestedMessageBuilder();
    TestAllTypes second = parent.buildPartial();
    assertThat(second.hasOptionalNestedMessage()).isTrue();
    assertValue(
        second,
        TestAllTypes.newBuilder()
            .setOptionalNestedMessage(NestedMessage.getDefaultInstance())
            .build());
    assertThat(first.hasOptionalNestedMessage()).isFalse();
    snapshot.assertUnchanged();
  }

  @Test
  public void oneofChildEditsAndCaseChangesPreservePublishedValues() throws Exception {
    TestOneof2.Builder parent = TestOneof2.newBuilder();
    TestOneof2.NestedMessage.Builder child = parent.getFooMessageBuilder().addCorgeInt(1);
    TestOneof2 first = parent.buildPartial();
    Snapshot firstSnapshot = new Snapshot(first);

    child.addCorgeInt(200);
    TestOneof2 second = parent.build();
    Snapshot secondSnapshot = new Snapshot(second);
    assertValue(
        second,
        TestOneof2.newBuilder()
            .setFooMessage(TestOneof2.NestedMessage.newBuilder().addCorgeInt(1).addCorgeInt(200))
            .build());

    parent.setFooString("replacement");
    child.addCorgeInt(30000);
    assertValue(parent.build(), TestOneof2.newBuilder().setFooString("replacement").build());
    firstSnapshot.assertUnchanged();
    secondSnapshot.assertUnchanged();
    assertThat(first.getFooMessage().getCorgeIntList()).containsExactly(1);
    assertThrows(
        UnsupportedOperationException.class, () -> first.getFooMessage().getCorgeIntList().clear());
  }

  @Test
  public void typedEditsAfterPublicationPreservePresenceAndCollectionSnapshots() throws Exception {
    TestAllTypes.Builder builder =
        TestAllTypes.newBuilder()
            .setOptionalInt32(0)
            .setOptionalStringBytes(ByteString.copyFromUtf8("first"))
            .addRepeatedInt32(1);
    TestAllTypes first = builder.buildPartial();
    Snapshot snapshot = new Snapshot(first);

    builder.clearOptionalInt32().setOptionalString("second").setRepeatedInt32(0, 200);
    assertValue(
        builder.build(),
        TestAllTypes.newBuilder().setOptionalString("second").addRepeatedInt32(200).build());
    builder.clearRepeatedInt32().setOptionalInt32(30000);
    assertValue(
        builder.build(),
        TestAllTypes.newBuilder().setOptionalInt32(30000).setOptionalString("second").build());
    assertThat(first.hasOptionalInt32()).isTrue();
    assertThat(first.getOptionalInt32()).isEqualTo(0);
    assertThat(first.getOptionalStringBytes()).isEqualTo(ByteString.copyFromUtf8("first"));
    assertThat(first.getRepeatedInt32List()).containsExactly(1);
    assertThrows(UnsupportedOperationException.class, () -> first.getRepeatedInt32List().clear());
    snapshot.assertUnchanged();
  }

  @Test
  public void editsAfterPublicationPreserveUndecodedStringBytes() throws Exception {
    ByteString invalidUtf8 = ByteString.copyFrom(new byte[] {(byte) 0xff});
    TestAllTypes.Builder builder =
        TestAllTypes.newBuilder().setOptionalStringBytes(invalidUtf8).setOptionalInt32(1);
    TestAllTypes first = builder.build();
    Snapshot snapshot = new Snapshot(first);

    builder.setOptionalInt32(200);
    TestAllTypes second = builder.buildPartial();
    assertThat(second.getOptionalStringBytes()).isEqualTo(invalidUtf8);
    assertValue(
        second,
        TestAllTypes.newBuilder()
            .setOptionalStringBytes(invalidUtf8)
            .setOptionalInt32(200)
            .build());
    assertThat(first.getOptionalStringBytes()).isEqualTo(invalidUtf8);
    snapshot.assertUnchanged();
  }

  @Test
  public void messageAndWireMergesAfterPublicationPreserveEarlierVersions() throws Exception {
    NestedMessage immutableChild = NestedMessage.newBuilder().setBb(1).build();
    TestAllTypes source =
        TestAllTypes.newBuilder()
            .setOptionalNestedMessage(immutableChild)
            .addRepeatedInt32(1)
            .build();
    TestAllTypes.Builder builder = TestAllTypes.newBuilder().mergeFrom(source);
    TestAllTypes first = builder.build();
    Snapshot snapshot = new Snapshot(first);
    assertThat(first.getOptionalNestedMessage()).isSameInstanceAs(immutableChild);
    assertThat(first.getRepeatedInt32List()).isSameInstanceAs(source.getRepeatedInt32List());

    builder.mergeFrom(TestAllTypes.newBuilder().setOptionalInt32(200).addRepeatedInt32(2).build());
    builder.mergeFrom(
        TestAllTypes.newBuilder()
            .setOptionalString("third")
            .addRepeatedInt32(3)
            .build()
            .toByteArray());
    assertValue(
        builder.build(),
        TestAllTypes.newBuilder()
            .setOptionalNestedMessage(immutableChild)
            .setOptionalInt32(200)
            .setOptionalString("third")
            .addRepeatedInt32(1)
            .addRepeatedInt32(2)
            .addRepeatedInt32(3)
            .build());
    snapshot.assertUnchanged();
  }

  @Test
  public void reflectionEditsAndRetainedChildBuildersRespectPublication() throws Exception {
    FieldDescriptor number = TestAllTypes.getDescriptor().findFieldByName("optional_int32");
    FieldDescriptor repeated = TestAllTypes.getDescriptor().findFieldByName("repeated_int32");
    FieldDescriptor nested =
        TestAllTypes.getDescriptor().findFieldByName("optional_nested_message");
    FieldDescriptor bb = NestedMessage.getDescriptor().findFieldByName("bb");
    Message.Builder builder = TestAllTypes.newBuilder();
    builder.setField(number, 1).addRepeatedField(repeated, 1);
    Message.Builder child = builder.getFieldBuilder(nested).setField(bb, 1);
    Message first = builder.build();
    Snapshot firstSnapshot = new Snapshot(first);

    child.setField(bb, 200);
    Message second = builder.buildPartial();
    Snapshot secondSnapshot = new Snapshot(second);
    assertValue(
        second,
        TestAllTypes.newBuilder()
            .setOptionalInt32(1)
            .addRepeatedInt32(1)
            .setOptionalNestedMessage(NestedMessage.newBuilder().setBb(200))
            .build());

    builder.clearField(number).setRepeatedField(repeated, 0, 30000).addRepeatedField(repeated, 2);
    assertValue(
        builder.build(),
        TestAllTypes.newBuilder()
            .addRepeatedInt32(30000)
            .addRepeatedInt32(2)
            .setOptionalNestedMessage(NestedMessage.newBuilder().setBb(200))
            .build());
    firstSnapshot.assertUnchanged();
    secondSnapshot.assertUnchanged();
  }

  @Test
  public void scalarMapViewsFreezeWhilePublishedMessagesRemainIndependent() throws Exception {
    TestMap.Builder builder = TestMap.newBuilder();
    Map<Integer, Integer> retained = builder.getMutableInt32ToInt32Field();
    retained.put(1, 1);
    TestMap first = builder.buildPartial();
    Snapshot snapshot = new Snapshot(first);

    assertThrows(UnsupportedOperationException.class, () -> retained.put(2, 200));
    builder.getMutableInt32ToInt32Field().put(2, 200);
    assertValue(
        builder.build(),
        TestMap.newBuilder().putInt32ToInt32Field(1, 1).putInt32ToInt32Field(2, 200).build());
    assertThat(first.getInt32ToInt32FieldMap()).containsExactly(1, 1);
    assertThrows(
        UnsupportedOperationException.class, () -> first.getInt32ToInt32FieldMap().clear());
    snapshot.assertUnchanged();
  }

  @Test
  public void messageMapViewsDetachWhileRetainedValueBuildersRemainLive() throws Exception {
    TestMap.Builder builder = TestMap.newBuilder();
    Map<Integer, MessageValue> retained = builder.getMutableInt32ToMessageField();
    retained.put(1, MessageValue.newBuilder().setValue(1).build());
    TestMap first = builder.build();
    Snapshot firstSnapshot = new Snapshot(first);

    retained.put(2, MessageValue.newBuilder().setValue(200).build());
    assertThat(builder.getInt32ToMessageFieldMap())
        .containsExactly(1, first.getInt32ToMessageFieldOrThrow(1));
    MessageValue.Builder child = builder.putInt32ToMessageFieldBuilderIfAbsent(1);
    TestMap second = builder.buildPartial();
    Snapshot secondSnapshot = new Snapshot(second);

    child.setValue(30000);
    assertValue(
        builder.build(),
        TestMap.newBuilder()
            .putInt32ToMessageField(1, MessageValue.newBuilder().setValue(30000).build())
            .build());
    firstSnapshot.assertUnchanged();
    secondSnapshot.assertUnchanged();
    assertThat(first.getInt32ToMessageFieldOrThrow(1).getValue()).isEqualTo(1);
    assertThrows(
        UnsupportedOperationException.class, () -> first.getInt32ToMessageFieldMap().clear());
  }

  @Test
  public void retainedUnknownFieldBuilderCannotChangePublishedMessages() throws Exception {
    TestAllTypes.Builder builder = TestAllTypes.newBuilder().setOptionalInt32(1);
    UnknownFieldSet.Builder retained = builder.getUnknownFieldSetBuilder();
    retained.mergeVarintField(20000, 1);
    TestAllTypes first = builder.buildPartial();
    Snapshot firstSnapshot = new Snapshot(first);

    retained.mergeVarintField(20000, 200);
    TestAllTypes second = builder.build();
    Snapshot secondSnapshot = new Snapshot(second);
    assertThat(second.getUnknownFields().getField(20000).getVarintList())
        .containsExactly(1L, 200L)
        .inOrder();
    assertValue(
        second,
        TestAllTypes.newBuilder()
            .setOptionalInt32(1)
            .setUnknownFields(
                UnknownFieldSet.newBuilder()
                    .mergeVarintField(20000, 1)
                    .mergeVarintField(20000, 200)
                    .build())
            .build());

    builder.setUnknownFields(UnknownFieldSet.getDefaultInstance());
    retained.mergeVarintField(20000, 30000);
    assertValue(builder.build(), TestAllTypes.newBuilder().setOptionalInt32(1).build());
    assertThat(first.getUnknownFields().getField(20000).getVarintList()).containsExactly(1L);
    firstSnapshot.assertUnchanged();
    secondSnapshot.assertUnchanged();
  }

  @Test
  public void extensionChildEditsAloneUpdateSuccessiveBuilds() throws Exception {
    TestAllExtensions.Builder builder = TestAllExtensions.newBuilder();
    FieldDescriptor optional = UnittestProto.optionalNestedMessageExtension.getDescriptor();
    FieldDescriptor repeated = UnittestProto.repeatedNestedMessageExtension.getDescriptor();
    FieldDescriptor bb = NestedMessage.getDescriptor().findFieldByName("bb");
    Message.Builder child = builder.getFieldBuilder(optional).setField(bb, 1);
    builder.addRepeatedField(repeated, NestedMessage.newBuilder().setBb(1).build());
    Message.Builder repeatedChild = builder.getRepeatedFieldBuilder(repeated, 0);
    TestAllExtensions first = builder.build();
    Snapshot firstSnapshot = new Snapshot(first);

    child.setField(bb, 200);
    repeatedChild.setField(bb, 30000);
    TestAllExtensions second = builder.buildPartial();
    Snapshot secondSnapshot = new Snapshot(second);
    assertValue(
        second,
        TestAllExtensions.newBuilder()
            .setExtension(
                UnittestProto.optionalNestedMessageExtension,
                NestedMessage.newBuilder().setBb(200).build())
            .addExtension(
                UnittestProto.repeatedNestedMessageExtension,
                NestedMessage.newBuilder().setBb(30000).build())
            .build());

    builder.clearExtension(UnittestProto.optionalNestedMessageExtension);
    child.setField(bb, 40000);
    repeatedChild.setField(bb, 50000);
    assertValue(
        builder.build(),
        TestAllExtensions.newBuilder()
            .addExtension(
                UnittestProto.repeatedNestedMessageExtension,
                NestedMessage.newBuilder().setBb(50000).build())
            .build());
    assertThat(first.getExtension(UnittestProto.optionalNestedMessageExtension).getBb())
        .isEqualTo(1);
    firstSnapshot.assertUnchanged();
    secondSnapshot.assertUnchanged();
  }

  @Test
  public void failedBuildAndRequiredFieldEditsDoNotReuseInitializationResults() throws Exception {
    TestRequired.Builder builder = TestRequired.newBuilder().setA(1);
    TestRequired first = builder.buildPartial();
    Snapshot firstSnapshot = new Snapshot(first);
    assertThat(first.isInitialized()).isFalse();
    assertThrows(UninitializedMessageException.class, builder::build);

    builder.setB(200).setC(30000);
    TestRequired second = builder.build();
    Snapshot secondSnapshot = new Snapshot(second);
    assertValue(second, TestRequired.newBuilder().setA(1).setB(200).setC(30000).build());

    builder.clearA();
    assertThrows(UninitializedMessageException.class, builder::build);
    assertThat(builder.buildPartial().isInitialized()).isFalse();
    firstSnapshot.assertUnchanged();
    secondSnapshot.assertUnchanged();
  }

  @Test
  public void requiredChildEditsInvalidateCachedParentInitialization() throws Exception {
    UnittestProto.TestRequiredForeign.Builder parent =
        UnittestProto.TestRequiredForeign.newBuilder();
    TestRequired.Builder child = parent.getOptionalMessageBuilder().setA(1);
    UnittestProto.TestRequiredForeign first = parent.buildPartial();
    Snapshot firstSnapshot = new Snapshot(first);
    assertThat(first.isInitialized()).isFalse();

    child.setB(200).setC(30000);
    UnittestProto.TestRequiredForeign second = parent.build();
    Snapshot secondSnapshot = new Snapshot(second);
    assertValue(
        second,
        UnittestProto.TestRequiredForeign.newBuilder()
            .setOptionalMessage(TestRequired.newBuilder().setA(1).setB(200).setC(30000))
            .build());

    child.clearB();
    assertThrows(UninitializedMessageException.class, parent::build);
    assertThat(parent.buildPartial().isInitialized()).isFalse();
    firstSnapshot.assertUnchanged();
    secondSnapshot.assertUnchanged();
  }

  @Test
  public void cloneClearAndToBuilderDoNotShareMutableChildren() throws Exception {
    TestAllTypes.Builder builder = TestAllTypes.newBuilder().addRepeatedInt32(1);
    NestedMessage.Builder child = builder.getOptionalNestedMessageBuilder().setBb(1);
    TestAllTypes first = builder.build();
    Snapshot snapshot = new Snapshot(first);
    TestAllTypes.Builder clone = builder.clone();
    TestAllTypes.Builder fromMessage = first.toBuilder();

    child.setBb(200);
    clone.getOptionalNestedMessageBuilder().setBb(30000);
    fromMessage.addRepeatedInt32(2);
    assertThat(builder.build().getOptionalNestedMessage().getBb()).isEqualTo(200);
    assertThat(clone.build().getOptionalNestedMessage().getBb()).isEqualTo(30000);
    assertValue(
        fromMessage.build(),
        TestAllTypes.newBuilder()
            .setOptionalNestedMessage(NestedMessage.newBuilder().setBb(1))
            .addRepeatedInt32(1)
            .addRepeatedInt32(2)
            .build());

    builder.clear();
    child.setBb(40000);
    assertValue(builder.build(), TestAllTypes.getDefaultInstance());
    snapshot.assertUnchanged();
  }

  @Test
  public void codeSizePublicationPreservesChildrenExtensionsAndUnknownFields() throws Exception {
    TestOptimizedForSize.Builder builder =
        TestOptimizedForSize.newBuilder()
            .setI(1)
            .setStringField("first")
            .setExtension(TestOptimizedForSize.testExtension, 1)
            .setUnknownFields(UnknownFieldSet.newBuilder().mergeVarintField(20000, 1).build());
    builder.getMsgBuilder().setC(1);
    FieldDescriptor extension = TestOptimizedForSize.testExtension2.getDescriptor();
    FieldDescriptor x = TestRequiredOptimizedForSize.getDescriptor().findFieldByName("x");
    Message.Builder child = builder.getFieldBuilder(extension).setField(x, 1);
    TestOptimizedForSize first = builder.buildPartial();
    Snapshot snapshot = new Snapshot(first);

    child.setField(x, 200);
    builder.getMsgBuilder().setC(30000);
    builder.clearI().setIntegerField(2).setExtension(TestOptimizedForSize.testExtension, 200);
    builder.mergeUnknownFields(UnknownFieldSet.newBuilder().mergeVarintField(20000, 200).build());
    assertValue(
        builder.build(),
        TestOptimizedForSize.newBuilder()
            .setMsg(UnittestProto.ForeignMessage.newBuilder().setC(30000))
            .setIntegerField(2)
            .setExtension(TestOptimizedForSize.testExtension, 200)
            .setExtension(
                TestOptimizedForSize.testExtension2,
                TestRequiredOptimizedForSize.newBuilder().setX(200).build())
            .setUnknownFields(
                UnknownFieldSet.newBuilder()
                    .mergeVarintField(20000, 1)
                    .mergeVarintField(20000, 200)
                    .build())
            .build());
    assertThat(first.getMsg().getC()).isEqualTo(1);
    assertThat(first.getExtension(TestOptimizedForSize.testExtension2).getX()).isEqualTo(1);
    snapshot.assertUnchanged();
  }

  @Test
  public void failedParsePublishesCollectionsAndAnIndependentUnfinishedMessage() throws Exception {
    TestAllTypes contents =
        TestAllTypes.newBuilder()
            .addRepeatedInt32(1)
            .setOptionalNestedMessage(NestedMessage.newBuilder().setBb(1))
            .build();
    byte[] truncated = truncatedAfter(contents);
    InvalidProtocolBufferException failure =
        assertThrows(InvalidProtocolBufferException.class, () -> TestAllTypes.parseFrom(truncated));
    TestAllTypes unfinished = (TestAllTypes) failure.getUnfinishedMessage();
    assertValue(unfinished, contents);
    Snapshot snapshot = new Snapshot(unfinished);
    assertThrows(
        UnsupportedOperationException.class, () -> unfinished.getRepeatedInt32List().clear());

    TestAllTypes.Builder builder = unfinished.toBuilder();
    builder.getOptionalNestedMessageBuilder().setBb(200);
    builder.addRepeatedInt32(2);
    assertValue(
        builder.build(),
        TestAllTypes.newBuilder()
            .addRepeatedInt32(1)
            .addRepeatedInt32(2)
            .setOptionalNestedMessage(NestedMessage.newBuilder().setBb(200))
            .build());
    snapshot.assertUnchanged();
  }

  @Test
  public void failedBuilderMergeAfterPublicationPreservesBothOldAndParsedState() throws Exception {
    TestAllTypes.Builder builder =
        TestAllTypes.newBuilder().setOptionalInt32(1).addRepeatedInt32(1);
    TestAllTypes first = builder.build();
    Snapshot snapshot = new Snapshot(first);
    byte[] truncated = truncatedAfter(TestAllTypes.newBuilder().addRepeatedInt32(200).build());

    assertThrows(InvalidProtocolBufferException.class, () -> builder.mergeFrom(truncated));
    assertValue(
        builder.buildPartial(),
        TestAllTypes.newBuilder()
            .setOptionalInt32(1)
            .addRepeatedInt32(1)
            .addRepeatedInt32(200)
            .build());
    snapshot.assertUnchanged();
  }

  private static byte[] truncatedAfter(TestAllTypes contents) throws Exception {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    bytes.write(contents.toByteArray());
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    output.writeUInt32NoTag(
        WireFormat.makeTag(
            TestAllTypes.OPTIONAL_STRING_FIELD_NUMBER, WireFormat.WIRETYPE_LENGTH_DELIMITED));
    output.writeUInt32NoTag(2);
    output.writeRawByte('x');
    output.flush();
    return bytes.toByteArray();
  }

  private static void assertValue(Message actual, Message expected) {
    ByteString bytes = expected.toByteString();
    assertThat(actual).isEqualTo(expected);
    assertThat(actual.getSerializedSize()).isEqualTo(bytes.size());
    assertThat(actual.toByteString()).isEqualTo(bytes);
    assertThat(actual.hashCode()).isEqualTo(expected.hashCode());
    assertThat(actual.isInitialized()).isEqualTo(expected.isInitialized());
  }

  private static final class Snapshot {
    private final Message message;
    private final Message expected;
    private final ByteString bytes;
    private final int hash;
    private final boolean initialized;

    Snapshot(Message message) throws InvalidProtocolBufferException {
      this.message = message;
      bytes = message.toByteString();
      hash = message.hashCode();
      initialized = message.isInitialized();
      ExtensionRegistry registry = ExtensionRegistry.newInstance();
      UnittestProto.registerAllExtensions(registry);
      registry.add(TestOptimizedForSize.testExtension);
      registry.add(TestOptimizedForSize.testExtension2);
      expected = (Message) message.getParserForType().parsePartialFrom(bytes, registry);
      assertValue(message, expected);
    }

    void assertUnchanged() {
      assertThat(message).isEqualTo(expected);
      assertThat(message.toByteString()).isEqualTo(bytes);
      assertThat(message.getSerializedSize()).isEqualTo(bytes.size());
      assertThat(message.hashCode()).isEqualTo(hash);
      assertThat(message.isInitialized()).isEqualTo(initialized);
    }
  }
}
