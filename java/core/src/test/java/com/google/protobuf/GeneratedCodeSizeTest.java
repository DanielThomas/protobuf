// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf;

import static com.google.common.truth.Truth.assertThat;

import com.google.common.io.ByteStreams;
import com.google.protobuf_test_messages.proto2.TestMessagesProto2.TestAllTypesProto2;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Regression tests for generated decoder bytecode size. */
@RunWith(JUnit4.class)
public class GeneratedCodeSizeTest {
  @Test
  public void proto2DecoderFitsHotSpotHugeMethodLimit() throws Exception {
    // HotSpot's default DontCompileHugeMethods policy excludes methods over 8000 code bytes.
    assertThat(
            methodCodeSize(
                TestAllTypesProto2.Builder.class,
                "mergeFrom",
                "(Lcom/google/protobuf/CodedInputStream;"
                    + "Lcom/google/protobuf/ExtensionRegistryLite;)"
                    + "Lcom/google/protobuf_test_messages/proto2/"
                    + "TestMessagesProto2$TestAllTypesProto2$Builder;"))
        .isAtMost(8000);
  }

  private static int methodCodeSize(Class<?> type, String methodName, String descriptor)
      throws IOException {
    String resourceName = "/" + type.getName().replace('.', '/') + ".class";
    try (InputStream resource = type.getResourceAsStream(resourceName)) {
      if (resource == null) {
        throw new IOException("Missing class resource: " + resourceName);
      }
      DataInputStream input = new DataInputStream(resource);
      assertThat(input.readInt()).isEqualTo(0xcafebabe);
      ByteStreams.skipFully(input, 4);
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
            ByteStreams.skipFully(input, 4);
            break;
          case 5:
          case 6:
            ByteStreams.skipFully(input, 8);
            i++;
            break;
          case 7:
          case 8:
          case 16:
          case 19:
          case 20:
            ByteStreams.skipFully(input, 2);
            break;
          case 15:
            ByteStreams.skipFully(input, 3);
            break;
          default:
            throw new IOException("Unknown constant pool tag: " + tag);
        }
      }
      ByteStreams.skipFully(input, 6);
      ByteStreams.skipFully(input, 2L * input.readUnsignedShort());
      int fields = input.readUnsignedShort();
      for (int i = 0; i < fields; i++) {
        ByteStreams.skipFully(input, 6);
        skipAttributes(input);
      }
      int methods = input.readUnsignedShort();
      for (int i = 0; i < methods; i++) {
        input.readUnsignedShort();
        String name = constants[input.readUnsignedShort()];
        String methodDescriptor = constants[input.readUnsignedShort()];
        int attributes = input.readUnsignedShort();
        for (int j = 0; j < attributes; j++) {
          String attribute = constants[input.readUnsignedShort()];
          long length = Integer.toUnsignedLong(input.readInt());
          if (name.equals(methodName)
              && methodDescriptor.equals(descriptor)
              && attribute.equals("Code")) {
            ByteStreams.skipFully(input, 4);
            return input.readInt();
          }
          ByteStreams.skipFully(input, length);
        }
      }
    }
    throw new IOException(
        "No bytecode found for " + type.getName() + "." + methodName + descriptor);
  }

  private static void skipAttributes(DataInputStream input) throws IOException {
    int attributes = input.readUnsignedShort();
    for (int i = 0; i < attributes; i++) {
      input.readUnsignedShort();
      ByteStreams.skipFully(input, Integer.toUnsignedLong(input.readInt()));
    }
  }
}
