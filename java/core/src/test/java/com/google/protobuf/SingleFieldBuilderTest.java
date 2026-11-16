// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import proto2_unittest.UnittestProto.TestAllTypes;
import proto2_unittest.UnittestProto.TestAllTypesOrBuilder;

/**
 * Tests for {@link SingleFieldBuilder}. This tests basic functionality. More extensive testing is
 * provided via other tests that exercise the builder.
 */
@RunWith(JUnit4.class)
public class SingleFieldBuilderTest {

  @Test
  public void testBasicUseAndInvalidations() {
    TestUtil.MockBuilderParent mockParent = new TestUtil.MockBuilderParent();
    SingleFieldBuilder<TestAllTypes, TestAllTypes.Builder, TestAllTypesOrBuilder> builder =
        new SingleFieldBuilder<>(TestAllTypes.getDefaultInstance(), mockParent, false);
    assertThat(builder.getMessage()).isSameInstanceAs(TestAllTypes.getDefaultInstance());
    assertThat(builder.getBuilder().buildPartial()).isEqualTo(TestAllTypes.getDefaultInstance());
    assertThat(mockParent.getInvalidationCount()).isEqualTo(0);

    builder.getBuilder().setOptionalInt32(10);
    assertThat(mockParent.getInvalidationCount()).isEqualTo(0);
    TestAllTypes message = builder.build();
    assertThat(message.getOptionalInt32()).isEqualTo(10);

    // Test that we receive invalidations now that build has been called.
    assertThat(mockParent.getInvalidationCount()).isEqualTo(0);
    builder.getBuilder().setOptionalInt32(20);
    assertThat(mockParent.getInvalidationCount()).isEqualTo(1);

    // Test that we don't keep getting invalidations on every change
    builder.getBuilder().setOptionalInt32(30);
    assertThat(mockParent.getInvalidationCount()).isEqualTo(1);
  }

  @Test
  public void testSnapshotRestorationIsLazyAndParentsTheBuilder() {
    TestUtil.MockBuilderParent parent = new TestUtil.MockBuilderParent();
    TestAllTypes initial = TestAllTypes.newBuilder().setOptionalInt32(7).build();
    SingleFieldBuilder<TestAllTypes, TestAllTypes.Builder, TestAllTypesOrBuilder> field =
        new SingleFieldBuilder<>(initial, parent, true, /* retained= */ true);
    assertThat(field.getMessage()).isSameInstanceAs(initial);
    assertThat(field.getMessageOrBuilder()).isSameInstanceAs(initial);
    TestAllTypes.Builder child = field.getBuilder();
    assertThat(field.getBuilder()).isSameInstanceAs(child);
    child.setOptionalInt32(8);
    assertThat(parent.getInvalidationCount()).isEqualTo(1);
    TestAllTypes snapshot = field.build();
    child.setOptionalInt32(9);
    assertThat(parent.getInvalidationCount()).isEqualTo(2);
    assertThat(field.getMessage().getOptionalInt32()).isEqualTo(9);
    assertThat(snapshot.getOptionalInt32()).isEqualTo(8);
  }

  @Test
  public void testRetainedSnapshotRestorationDoesNotApplyToReplacements() throws Exception {
    for (int operation = 0; operation < 3; operation++) {
      TestAllTypes initial =
          operation == 2
              ? TestAllTypes.getDefaultInstance()
              : TestAllTypes.newBuilder()
                  .setOneofNestedMessage(TestAllTypes.NestedMessage.newBuilder().setBb(7))
                  .build();
      SingleFieldBuilder<TestAllTypes, TestAllTypes.Builder, TestAllTypesOrBuilder> field =
          new SingleFieldBuilder<>(initial, null, false, /* retained= */ true);
      TestAllTypes replacement =
          TestAllTypes.newBuilder()
              .setOneofNestedMessage(TestAllTypes.NestedMessage.newBuilder().setBb(11))
              .build();
      if (operation == 0) {
        field.setMessage(replacement);
      } else if (operation == 1) {
        field.clear();
      } else {
        field.mergeFrom(replacement);
      }
      TestAllTypes.Builder child = field.getBuilder();
      child.mergeFrom(oneofContinuation());
      assertThat(child.getOneofNestedMessage().getBb()).isEqualTo(0);
    }
  }

  @Test
  public void testSnapshotRestorationPreservesTheSelectedChildLifetime() throws Exception {
    TestAllTypes initial =
        TestAllTypes.newBuilder()
            .setOneofNestedMessage(TestAllTypes.NestedMessage.newBuilder().setBb(7))
            .build();
    // Precompiled legacy parsers do not expose retained-snapshot restoration.
    boolean supportsRetained =
        ((AbstractParser<?>) initial.getParserForType()).newBuilderForParsing(initial, true) != null;
    for (boolean retained : new boolean[] {false, true}) {
      SingleFieldBuilder<TestAllTypes, TestAllTypes.Builder, TestAllTypesOrBuilder> field =
          new SingleFieldBuilder<>(initial, null, false, retained);
      TestAllTypes.Builder child = field.getBuilder();
      child.mergeFrom(oneofContinuation());
      assertThat(child.getOneofNestedMessage().getBb())
          .isEqualTo(retained && supportsRetained ? 7 : 0);
      assertThat(initial.getOneofNestedMessage().getBb()).isEqualTo(7);
    }
  }

  private static ByteString oneofContinuation() {
    return TestAllTypes.newBuilder()
        .setOneofUint32(9)
        .build()
        .toByteString()
        .concat(
            TestAllTypes.newBuilder()
                .setOneofNestedMessage(TestAllTypes.NestedMessage.getDefaultInstance())
                .build()
                .toByteString());
  }

  @Test
  public void testSnapshotRestorationHonorsForcedFieldBuilders() throws Exception {
    boolean previous = GeneratedMessage.alwaysUseFieldBuilders;
    try {
      GeneratedMessage.alwaysUseFieldBuilders = true;
      TestAllTypes initial =
          TestAllTypes.newBuilder()
              .setOneofNestedMessage(TestAllTypes.NestedMessage.newBuilder().setBb(7))
              .build();
      SingleFieldBuilder<TestAllTypes, TestAllTypes.Builder, TestAllTypesOrBuilder> field =
          new SingleFieldBuilder<>(initial, null, false, /* retained= */ true);
      TestAllTypes.Builder child = field.getBuilder();
      child.mergeFrom(oneofContinuation());
      assertThat(child.getOneofNestedMessage().getBb()).isEqualTo(0);
    } finally {
      GeneratedMessage.alwaysUseFieldBuilders = previous;
    }
  }

  @Test
  public void testAdoptBuilderWithoutPublishingIt() {
    TestAllTypes.Builder child = TestAllTypes.newBuilder().setOptionalInt32(7);
    SingleFieldBuilder<TestAllTypes, TestAllTypes.Builder, TestAllTypesOrBuilder> field =
        SingleFieldBuilder.fromBuilder(child, null, false);
    assertThat(field.getBuilder()).isSameInstanceAs(child);
    TestUtil.MockBuilderParent parent = new TestUtil.MockBuilderParent();
    field.setParentForParsing(parent);
    assertThrows(IllegalStateException.class, () -> field.setParentForParsing(parent));
    TestAllTypes snapshot = field.build();
    child.setOptionalInt32(8);
    assertThat(parent.getInvalidationCount()).isEqualTo(1);
    assertThat(field.getMessage().getOptionalInt32()).isEqualTo(8);
    assertThat(snapshot.getOptionalInt32()).isEqualTo(7);
    SingleFieldBuilder<TestAllTypes, TestAllTypes.Builder, TestAllTypesOrBuilder> original = field;
    assertThrows(
        IllegalStateException.class,
        () -> SingleFieldBuilder.fromBuilder(original.getBuilder(), null, false));
  }

  @Test
  public void testSetMessage() {
    TestUtil.MockBuilderParent mockParent = new TestUtil.MockBuilderParent();
    SingleFieldBuilder<TestAllTypes, TestAllTypes.Builder, TestAllTypesOrBuilder> builder =
        new SingleFieldBuilder<>(TestAllTypes.getDefaultInstance(), mockParent, false);
    builder.setMessage(TestAllTypes.newBuilder().setOptionalInt32(0).build());
    assertThat(builder.getMessage().getOptionalInt32()).isEqualTo(0);

    // Update message using the builder
    builder.getBuilder().setOptionalInt32(1);
    assertThat(mockParent.getInvalidationCount()).isEqualTo(0);
    assertThat(builder.getBuilder().getOptionalInt32()).isEqualTo(1);
    assertThat(builder.getMessage().getOptionalInt32()).isEqualTo(1);
    TestAllTypes unused = builder.build();
    builder.getBuilder().setOptionalInt32(2);
    assertThat(builder.getBuilder().getOptionalInt32()).isEqualTo(2);
    assertThat(builder.getMessage().getOptionalInt32()).isEqualTo(2);

    // Make sure message stays cached
    assertThat(builder.getMessage()).isSameInstanceAs(builder.getMessage());
  }

  @Test
  public void testClear() {
    TestUtil.MockBuilderParent mockParent = new TestUtil.MockBuilderParent();
    SingleFieldBuilder<TestAllTypes, TestAllTypes.Builder, TestAllTypesOrBuilder> builder =
        new SingleFieldBuilder<>(TestAllTypes.getDefaultInstance(), mockParent, false);
    builder.setMessage(TestAllTypes.newBuilder().setOptionalInt32(0).build());
    assertThat(TestAllTypes.getDefaultInstance()).isNotSameInstanceAs(builder.getMessage());
    builder.clear();
    assertThat(builder.getMessage()).isSameInstanceAs(TestAllTypes.getDefaultInstance());

    builder.getBuilder().setOptionalInt32(1);
    assertThat(TestAllTypes.getDefaultInstance()).isNotSameInstanceAs(builder.getMessage());
    builder.clear();
    assertThat(builder.getMessage()).isSameInstanceAs(TestAllTypes.getDefaultInstance());
  }

  @Test
  public void testMerge() {
    TestUtil.MockBuilderParent mockParent = new TestUtil.MockBuilderParent();
    SingleFieldBuilder<TestAllTypes, TestAllTypes.Builder, TestAllTypesOrBuilder> builder =
        new SingleFieldBuilder<>(TestAllTypes.getDefaultInstance(), mockParent, false);

    // Merge into default field.
    builder.mergeFrom(TestAllTypes.getDefaultInstance());
    assertThat(builder.getMessage()).isSameInstanceAs(TestAllTypes.getDefaultInstance());

    // Merge into non-default field on existing builder.
    builder.getBuilder().setOptionalInt32(2);
    builder.mergeFrom(TestAllTypes.newBuilder().setOptionalDouble(4.0).buildPartial());
    assertThat(builder.getMessage().getOptionalInt32()).isEqualTo(2);
    assertThat(builder.getMessage().getOptionalDouble()).isEqualTo(4.0);

    // Merge into non-default field on existing message
    builder.setMessage(TestAllTypes.newBuilder().setOptionalInt32(10).buildPartial());
    builder.mergeFrom(TestAllTypes.newBuilder().setOptionalDouble(5.0).buildPartial());
    assertThat(builder.getMessage().getOptionalInt32()).isEqualTo(10);
    assertThat(builder.getMessage().getOptionalDouble()).isEqualTo(5.0);
  }
}
