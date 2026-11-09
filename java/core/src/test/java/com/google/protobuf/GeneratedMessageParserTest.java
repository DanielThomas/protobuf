// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf;

import static com.google.common.truth.Truth.assertThat;

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
}
