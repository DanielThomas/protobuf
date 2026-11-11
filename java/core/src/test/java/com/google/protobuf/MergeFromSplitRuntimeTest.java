// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;
import static org.junit.Assert.assertThrows;

import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Descriptors.OneofDescriptor;
import java.io.ByteArrayOutputStream;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class MergeFromSplitRuntimeTest {

  // Zero-based child slots on both sides of the 32- and 64-slot boundaries.
  private static final int[] RETAINED_CHILD_FIELDS = {1, 32, 33, 64, 65};

  private enum MergeFailure {
    NONE,
    RECURSION,
    MESSAGE_END_TAG,
    GROUP_END_TAG
  }

  private void testBoundaryMessage(Message defaultInstance, int fieldCount) throws Exception {
    Descriptor descriptor = defaultInstance.getDescriptorForType();
    Message.Builder sourceBuilder = defaultInstance.newBuilderForType();
    Message.Builder expectedBuilder = defaultInstance.newBuilderForType();

    // Strategy 1: Empty to Full
    for (int i = 1; i <= fieldCount; i++) {
      FieldDescriptor fd = descriptor.findFieldByNumber(i);
      assertThat(fd).isNotNull();
      sourceBuilder.setField(fd, i);
      expectedBuilder.setField(fd, i);
    }

    Message source = sourceBuilder.build();
    Message expected = expectedBuilder.build();

    Message.Builder destinationBuilder = defaultInstance.newBuilderForType();
    destinationBuilder.mergeFrom(source);

    Message actual = destinationBuilder.build();
    assertThat(actual).isEqualTo(expected);

    // Strategy 2: Overwrite Full
    Message.Builder destinationBuilder2 = defaultInstance.newBuilderForType();
    for (int i = 1; i <= fieldCount; i++) {
      FieldDescriptor fd = descriptor.findFieldByNumber(i);
      destinationBuilder2.setField(fd, i * 10);
    }
    destinationBuilder2.mergeFrom(source);

    Message.Builder expectedBuilder2 = defaultInstance.newBuilderForType();
    for (int i = 1; i <= fieldCount; i++) {
      FieldDescriptor fd = descriptor.findFieldByNumber(i);
      expectedBuilder2.setField(fd, i); // mergeFrom OVERWRITES non-repeated primitives
    }
    assertThat(destinationBuilder2.build()).isEqualTo(expectedBuilder2.build());

    // Strategy 3: Sparse merging (odd in source, even in dest)
    Message.Builder sourceBuilderSparse = defaultInstance.newBuilderForType();
    Message.Builder destBuilderSparse = defaultInstance.newBuilderForType();
    Message.Builder expectedBuilderSparse = defaultInstance.newBuilderForType();

    for (int i = 1; i <= fieldCount; i++) {
      FieldDescriptor fd = descriptor.findFieldByNumber(i);
      if (i % 2 != 0) {
        sourceBuilderSparse.setField(fd, i);
        expectedBuilderSparse.setField(fd, i);
      } else {
        destBuilderSparse.setField(fd, i * 10);
        expectedBuilderSparse.setField(fd, i * 10);
      }
    }

    Message sourceSparse = sourceBuilderSparse.build();
    destBuilderSparse.mergeFrom(sourceSparse);

    Message actualSparse = destBuilderSparse.build();
    assertThat(actualSparse).isEqualTo(expectedBuilderSparse.build());

    // Strategy 4: Overlapping Sparse merging
    Message.Builder sourceBuilderOverlap = defaultInstance.newBuilderForType();
    Message.Builder destBuilderOverlap = defaultInstance.newBuilderForType();
    Message.Builder expectedBuilderOverlap = defaultInstance.newBuilderForType();

    for (int i = 1; i <= fieldCount; i++) {
      FieldDescriptor fd = descriptor.findFieldByNumber(i);
      if (i % 2 == 0) {
        sourceBuilderOverlap.setField(fd, i * 100);
        expectedBuilderOverlap.setField(fd, i * 100);
      }
      if (i % 3 == 0) {
        destBuilderOverlap.setField(fd, i * 10);
        if (i % 2 != 0) {
          expectedBuilderOverlap.setField(fd, i * 10);
        }
      }
    }

    Message sourceOverlap = sourceBuilderOverlap.build();
    destBuilderOverlap.mergeFrom(sourceOverlap);

    Message actualOverlap = destBuilderOverlap.build();
    assertThat(actualOverlap).isEqualTo(expectedBuilderOverlap.build());
  }

  @Test
  public void testRetainedOneofHistoryAcrossChildBoundaries() throws Exception {
    for (boolean wrapped : new boolean[] {false, true}) {
      for (boolean extension : new boolean[] {false, true}) {
        for (int field : RETAINED_CHILD_FIELDS) {
          assertRetainedHistory(field, wrapped, extension, MergeFailure.NONE);
        }
      }
    }
  }

  @Test
  public void testRetainedOneofHistoryOnMergeFailure() throws Exception {
    for (boolean wrapped : new boolean[] {false, true}) {
      for (boolean extension : new boolean[] {false, true}) {
        for (int field : RETAINED_CHILD_FIELDS) {
          for (MergeFailure failure :
              new MergeFailure[] {
                MergeFailure.RECURSION, MergeFailure.MESSAGE_END_TAG, MergeFailure.GROUP_END_TAG
              }) {
            assertRetainedHistory(field, wrapped, extension, failure);
          }
        }
      }
    }
  }

  private static void assertRetainedHistory(
      int field, boolean wrapped, boolean extension, MergeFailure failure) throws Exception {
    ExtensionRegistry registry = ExtensionRegistry.newInstance();
    MergeFromSplitTestProto.registerAllExtensions(registry);
    ByteString history = retainedHistory(extension);
    ByteString suffix = retainedSuffix(field, failure);
    ByteString wire =
        wrapped
            ? messageField(RetainedChildrenParent.SELECTED_FIELD_NUMBER, history)
                .concat(
                    RetainedChildrenParent.newBuilder()
                        .setText("replacement")
                        .build()
                        .toByteString())
                .concat(messageField(RetainedChildrenParent.SELECTED_FIELD_NUMBER, suffix))
            : history.concat(suffix);
    Message defaults =
        wrapped
            ? RetainedChildrenParent.getDefaultInstance()
            : RetainedChildren.getDefaultInstance();
    Message.Builder builder = defaults.newBuilderForType();
    CodedInputStream builderInput = wire.newCodedInput();
    CodedInputStream parserInput = wire.newCodedInput();
    int recursionLimit = wrapped ? 4 : 3;
    builderInput.setRecursionLimit(recursionLimit);
    parserInput.setRecursionLimit(recursionLimit);
    Message parsed;
    if (failure == MergeFailure.NONE) {
      builder.mergeFrom(builderInput, registry);
      parsed = (Message) defaults.getParserForType().parseFrom(parserInput, registry);
    } else {
      InvalidProtocolBufferException builderFailure =
          assertThrows(
              InvalidProtocolBufferException.class,
              () -> builder.mergeFrom(builderInput, registry));
      InvalidProtocolBufferException parserFailure =
          assertThrows(
              InvalidProtocolBufferException.class,
              () -> defaults.getParserForType().parseFrom(parserInput, registry));
      assertThat(parserFailure.getMessage()).isEqualTo(builderFailure.getMessage());
      parsed = (Message) parserFailure.getUnfinishedMessage();
    }
    assertThat(parserInput.getTotalBytesRead()).isEqualTo(builderInput.getTotalBytesRead());
    assertThat(parserInput.getLastTag()).isEqualTo(builderInput.getLastTag());
    assertThat(parserInput.getBytesUntilLimit()).isEqualTo(builderInput.getBytesUntilLimit());
    assertThat(parserInput.messageDepth).isEqualTo(builderInput.messageDepth);
    assertThat(parserInput.groupDepth).isEqualTo(builderInput.groupDepth);

    RetainedChildren.Child expectedChild =
        RetainedChildren.Child.newBuilder()
            .addValues(field)
            .addValues(field + 1000)
            .addValues(field + 2000)
            .setChild(
                RetainedChildren.Child.newBuilder()
                    .addValues(field + 100)
                    .addValues(field + 1100)
                    .addValues(field + 2100)
                    .setChild(
                        RetainedChildren.Child.newBuilder()
                            .addValues(field + 200)
                            .addValues(field + 1200)
                            .addValues(field + 2200)))
            .setGroup(
                RetainedChildren.Child.Group.newBuilder()
                    .addValues(field + 300)
                    .addValues(field + 1300))
            .build();
    if (failure == MergeFailure.NONE || failure == MergeFailure.GROUP_END_TAG) {
      expectedChild =
          expectedChild.toBuilder()
              .setGroup(expectedChild.getGroup().toBuilder().addValues(field + 2300))
              .build();
    }
    FieldDescriptor selected = RetainedChildren.getDescriptor().findFieldByNumber(field);
    RetainedChildren.Builder expectedChildren =
        RetainedChildren.newBuilder().setField(selected, expectedChild);
    if (extension) expectedChildren.setExtension(MergeFromSplitTestProto.retainedNumber, 7);
    Message expected =
        wrapped
            ? RetainedChildrenParent.newBuilder().setSelected(expectedChildren).build()
            : expectedChildren.build();
    assertWithMessage(
            "field=%s wrapped=%s extension=%s failure=%s", field, wrapped, extension, failure)
        .that(parsed)
        .isEqualTo(expected);
    assertThat(builder.buildPartial()).isEqualTo(expected);
    assertThat(parsed.toByteString()).isEqualTo(expected.toByteString());

    RetainedChildren children =
        wrapped ? ((RetainedChildrenParent) parsed).getSelected() : (RetainedChildren) parsed;
    RetainedChildren.Child child = (RetainedChildren.Child) children.getField(selected);
    assertThrows(UnsupportedOperationException.class, () -> child.getValuesList().clear());
    assertThrows(
        UnsupportedOperationException.class, () -> child.getChild().getValuesList().clear());
    assertThrows(
        UnsupportedOperationException.class,
        () -> child.getChild().getChild().getValuesList().clear());
    assertThrows(
        UnsupportedOperationException.class, () -> child.getGroup().getValuesList().clear());
    ByteString snapshot = parsed.toByteString();
    Message.Builder continuation = parsed.toBuilder();
    RetainedChildren.Builder continuedChildren =
        wrapped
            ? ((RetainedChildrenParent.Builder) continuation).getSelectedBuilder()
            : (RetainedChildren.Builder) continuation;
    RetainedChildren.Child.Builder continued =
        (RetainedChildren.Child.Builder) continuedChildren.getFieldBuilder(selected);
    continued.addValues(9999);
    continued.getChildBuilder().getChildBuilder().addValues(9998);
    continued.getGroupBuilder().addValues(9997);
    Message continuedMessage = continuation.build();
    RetainedChildren publishedChildren =
        wrapped
            ? ((RetainedChildrenParent) continuedMessage).getSelected()
            : (RetainedChildren) continuedMessage;
    RetainedChildren.Child published =
        (RetainedChildren.Child) publishedChildren.getField(selected);
    assertThat(published.getValuesList())
        .containsExactly(field, field + 1000, field + 2000, 9999)
        .inOrder();
    assertThat(published.getChild().getChild().getValuesList())
        .containsExactly(field + 200, field + 1200, field + 2200, 9998)
        .inOrder();
    assertThat(published.getGroup().getValuesList())
        .containsExactlyElementsIn(
            expectedChild.getGroup().toBuilder().addValues(9997).getValuesList())
        .inOrder();
    assertThat(parsed.toByteString()).isEqualTo(snapshot);
  }

  private static ByteString retainedHistory(boolean extension) throws Exception {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    for (int offset : new int[] {0, 1000}) {
      for (int field : RETAINED_CHILD_FIELDS) {
        output.writeMessage(field, retainedChild(field + offset));
      }
      if (extension && offset == 0) output.writeInt32(100, 7);
    }
    output.flush();
    return ByteString.copyFrom(bytes.toByteArray());
  }

  private static RetainedChildren.Child retainedChild(int value) {
    return RetainedChildren.Child.newBuilder()
        .addValues(value)
        .setChild(
            RetainedChildren.Child.newBuilder()
                .addValues(value + 100)
                .setChild(RetainedChildren.Child.newBuilder().addValues(value + 200)))
        .setGroup(RetainedChildren.Child.Group.newBuilder().addValues(value + 300))
        .build();
  }

  private static ByteString retainedSuffix(int field, MergeFailure failure) throws Exception {
    if (failure == MergeFailure.NONE) {
      return messageField(field, retainedChild(field + 2000).toByteString());
    }
    ByteArrayOutputStream nested = new ByteArrayOutputStream();
    CodedOutputStream nestedOutput = CodedOutputStream.newInstance(nested);
    nestedOutput.writeInt32(RetainedChildren.Child.VALUES_FIELD_NUMBER, field + 2100);
    RetainedChildren.Child.Builder grandchild =
        RetainedChildren.Child.newBuilder().addValues(field + 2200);
    if (failure == MergeFailure.RECURSION) {
      grandchild.setChild(RetainedChildren.Child.newBuilder().addValues(field + 2300));
    }
    nestedOutput.writeMessage(RetainedChildren.Child.CHILD_FIELD_NUMBER, grandchild.build());
    if (failure == MergeFailure.MESSAGE_END_TAG) {
      nestedOutput.writeTag(
          RetainedChildren.Child.CHILD_FIELD_NUMBER, WireFormat.WIRETYPE_END_GROUP);
    }
    nestedOutput.flush();
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    output.writeInt32(RetainedChildren.Child.VALUES_FIELD_NUMBER, field + 2000);
    output.writeByteArray(RetainedChildren.Child.CHILD_FIELD_NUMBER, nested.toByteArray());
    if (failure == MergeFailure.GROUP_END_TAG) {
      output.writeTag(RetainedChildren.Child.GROUP_FIELD_NUMBER, WireFormat.WIRETYPE_START_GROUP);
      output.writeInt32(RetainedChildren.Child.Group.VALUES_FIELD_NUMBER, field + 2300);
      output.writeTag(RetainedChildren.Child.CHILD_FIELD_NUMBER, WireFormat.WIRETYPE_END_GROUP);
    }
    output.flush();
    return messageField(field, ByteString.copyFrom(bytes.toByteArray()));
  }

  private static ByteString messageField(int field, ByteString value) throws Exception {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    output.writeBytes(field, value);
    output.flush();
    return ByteString.copyFrom(bytes.toByteArray());
  }

  @Test
  public void testBoundary32() throws Exception {
    testBoundaryMessage(Boundary32.getDefaultInstance(), 32);
  }

  @Test
  public void testBoundary33() throws Exception {
    testBoundaryMessage(Boundary33.getDefaultInstance(), 33);
  }

  @Test
  public void testBoundary1024() throws Exception {
    testBoundaryMessage(Boundary1024.getDefaultInstance(), 1024);
  }

  @Test
  public void testBoundary1025() throws Exception {
    testBoundaryMessage(Boundary1025.getDefaultInstance(), 1025);
  }

  @Test
  public void testLargeButSafeOneof() throws Exception {
    Descriptor descriptor = LargeButSafeOneof.getDescriptor();
    OneofDescriptor oneof = descriptor.getOneofs().get(0);
    assertThat(oneof.getName()).isEqualTo("safe_oneof");
    assertThat(oneof.getFields()).hasSize(500);

    for (int i = 1; i <= 500; i++) {
      FieldDescriptor fd = descriptor.findFieldByNumber(i);
      assertThat(fd).isNotNull();

      LargeButSafeOneof source = LargeButSafeOneof.newBuilder().setField(fd, i).build();
      LargeButSafeOneof actual = LargeButSafeOneof.newBuilder().mergeFrom(source).build();

      assertThat(actual).isEqualTo(source);
      assertThat(actual.getSafeOneofCase().getNumber()).isEqualTo(i);
      assertThat(actual.getField(fd)).isEqualTo(i);

      // Verify merging with EXISTING different field in oneof
      int otherFieldNum = (i % 500) + 1;
      FieldDescriptor otherFd = descriptor.findFieldByNumber(otherFieldNum);
      assertThat(otherFd).isNotNull();
      LargeButSafeOneof destDifferent =
          LargeButSafeOneof.newBuilder().setField(otherFd, 999).build();
      LargeButSafeOneof mergedDifferent = destDifferent.toBuilder().mergeFrom(source).build();
      assertThat(mergedDifferent).isEqualTo(source);
      assertThat(mergedDifferent.getSafeOneofCase().getNumber()).isEqualTo(i);
      assertThat(mergedDifferent.getField(fd)).isEqualTo(i);
      assertThat(mergedDifferent.hasField(otherFd)).isFalse();

      // Verify merging with EXISTING SAME field in oneof
      LargeButSafeOneof destSame = LargeButSafeOneof.newBuilder().setField(fd, 999).build();
      LargeButSafeOneof mergedSame = destSame.toBuilder().mergeFrom(source).build();
      assertThat(mergedSame).isEqualTo(source); // Overwrites
      assertThat(mergedSame.getField(fd)).isEqualTo(i);
    }
  }

  private void testBoundaryWithOneof(
      Message defaultInstance, int fieldCount, int oneofField1Num, int oneofField2Num)
      throws Exception {
    Descriptor descriptor = defaultInstance.getDescriptorForType();

    // Test with oneofField1 set in source
    for (int i = 1; i <= fieldCount; i++) {
      FieldDescriptor fd = descriptor.findFieldByNumber(i);
      assertThat(fd).isNotNull();
    }
    FieldDescriptor fdOneof1 = descriptor.findFieldByNumber(oneofField1Num);
    FieldDescriptor fdOneof2 = descriptor.findFieldByNumber(oneofField2Num);
    assertThat(fdOneof1).isNotNull();
    assertThat(fdOneof2).isNotNull();

    Message.Builder sourceBuilder = defaultInstance.newBuilderForType();
    Message.Builder expectedBuilder = defaultInstance.newBuilderForType();

    for (int i = 1; i <= fieldCount; i++) {
      FieldDescriptor fd = descriptor.findFieldByNumber(i);
      sourceBuilder.setField(fd, i);
      expectedBuilder.setField(fd, i);
    }
    sourceBuilder.setField(fdOneof1, oneofField1Num);
    expectedBuilder.setField(fdOneof1, oneofField1Num);

    Message source = sourceBuilder.build();
    Message expected = expectedBuilder.build();

    // Merge to empty
    Message.Builder destBuilder = defaultInstance.newBuilderForType();
    destBuilder.mergeFrom(source);
    assertThat(destBuilder.build()).isEqualTo(expected);

    // Merge to dest with oneofField2 set
    Message.Builder destBuilder2 = defaultInstance.newBuilderForType();
    for (int i = 1; i <= fieldCount; i++) {
      FieldDescriptor fd = descriptor.findFieldByNumber(i);
      destBuilder2.setField(fd, i * 10);
    }
    destBuilder2.setField(fdOneof2, 999);
    destBuilder2.mergeFrom(source);

    Message.Builder expectedBuilder2 = defaultInstance.newBuilderForType();
    for (int i = 1; i <= fieldCount; i++) {
      FieldDescriptor fd = descriptor.findFieldByNumber(i);
      expectedBuilder2.setField(fd, i);
    }
    expectedBuilder2.setField(fdOneof1, oneofField1Num);
    assertThat(destBuilder2.build()).isEqualTo(expectedBuilder2.build());

    // Check that oneofField2 was CLEARED
    Message actual2 = destBuilder2.build();
    assertThat(actual2.hasField(fdOneof2)).isFalse();

    // Test with oneofField2 set in source
    Message.Builder sourceBuilder3 = defaultInstance.newBuilderForType();
    Message.Builder expectedBuilder3 = defaultInstance.newBuilderForType();

    for (int i = 1; i <= fieldCount; i++) {
      FieldDescriptor fd = descriptor.findFieldByNumber(i);
      sourceBuilder3.setField(fd, i * 100);
      expectedBuilder3.setField(fd, i * 100);
    }
    sourceBuilder3.setField(fdOneof2, oneofField2Num);
    expectedBuilder3.setField(fdOneof2, oneofField2Num);

    Message.Builder destBuilder3 =
        destBuilder2; // Reuse dest with oneofField1 set from previous merge actual2
    destBuilder3.mergeFrom(sourceBuilder3.build());
    assertThat(destBuilder3.build()).isEqualTo(expectedBuilder3.build());
    Message actual3 = destBuilder3.build();
    assertThat(actual3.hasField(fdOneof1)).isFalse();
  }

  @Test
  public void testBoundary32WithOneof() throws Exception {
    testBoundaryWithOneof(Boundary32WithOneof.getDefaultInstance(), 32, 33, 34);
  }

  @Test
  public void testBoundary1024WithOneof() throws Exception {
    testBoundaryWithOneof(Boundary1024WithOneof.getDefaultInstance(), 1024, 1025, 1026);
  }
}
