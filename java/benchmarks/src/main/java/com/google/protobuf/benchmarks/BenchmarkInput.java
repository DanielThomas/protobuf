// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf.benchmarks;

import benchmark.fixtures.FlatProto2;
import benchmark.fixtures.Wide495;
import benchmark.fixtures.WideClosedEnum;
import benchmark.fixtures.WideMessageFields;
import benchmark.fixtures.WideMessageOneof;
import benchmark.fixtures.WideOneof;
import com.google.protobuf.CodedOutputStream;
import com.google.protobuf.Int32Value;
import com.google.protobuf.Message;
import com.google.protobuf.benchmarks.ParseBenchmark.Input;
import com.google.protobuf.test.UnittestImport.ImportMessage;
import com.google.protobuf.testing.textformat.performance.proto2.Proto2TextFormatPerformanceProto;
import com.google.protobuf.testing.textformat.performance.proto3.Proto3TextFormatPerformanceProto;
import com.google.protobuf_test_messages.proto2.TestMessagesProto2.TestAllTypesProto2;
import java.io.IOException;
import map_test.MapForProto2TestProto;
import map_test.MapTestProto;
import proto2_unittest.UnittestProto;
import proto3_unittest.UnittestProto3;

final class BenchmarkInput {
  enum Registry {
    NONE,
    UNITTEST,
    TEXT_FORMAT
  }

  final Class<? extends Message> type;
  final byte[] wire;
  final byte[] expected;
  final Registry registry;
  final int merges;

  private BenchmarkInput(
      Class<? extends Message> type, byte[] wire, byte[] expected, Registry registry, int merges) {
    this.type = type;
    this.wire = wire;
    this.expected = expected;
    this.registry = registry;
    this.merges = merges;
  }

  private static BenchmarkInput unchanged(Class<? extends Message> type, byte[] wire) {
    return new BenchmarkInput(type, wire, wire, Registry.NONE, 0);
  }

  static BenchmarkInput create(Input input) throws IOException {
    boolean merging = input.name().endsWith("_MERGE");
    switch (input) {
      case SCALAR_PROTO2:
        return unchanged(ImportMessage.class, integer(1, 1));
      case SCALAR_PROTO3:
        return unchanged(Int32Value.class, integer(1, 1));
      case SCALAR_EDITIONS:
        return unchanged(UnittestProto.TestAllTypes.NestedMessage.class, integer(1, 1));
      case PRESENCE_PROTO2_ZERO:
        return unchanged(ImportMessage.class, integer(1, 0));
      case PRESENCE_PROTO3_ZERO:
        return new BenchmarkInput(Int32Value.class, integer(1, 0), new byte[0], Registry.NONE, 0);
      case PRESENCE_EDITIONS_ZERO:
        return unchanged(UnittestProto.TestAllTypes.NestedMessage.class, integer(1, 0));
      case FLAT_PROTO2_WIDE:
        return unchanged(TestAllTypesProto2.class, flat());
      case FLAT_PROTO2_COMPACT:
        return unchanged(FlatProto2.class, flat());
      case FLAT_PROTO3:
        return unchanged(UnittestProto3.TestAllTypes.class, flat());
      case FLAT_EDITIONS:
        return unchanged(UnittestProto.TestAllTypes.class, flat());
      case REQUIRED_SCALARS:
        return unchanged(
            UnittestProto.TestRequired.class, concat(integer(1, 1), integer(3, 2), integer(33, 3)));
      case ORDINARY_SCALARS_FIRST:
      case ORDINARY_SCALARS_MERGE:
        return pair(
            UnittestProto.TestAllTypes.class,
            19,
            integer(1, 1),
            integer(2, 2),
            merging,
            false,
            Registry.NONE);
      case ONEOF_SCALARS_FIRST:
      case ONEOF_SCALARS_MERGE:
        return pair(
            UnittestProto.TestOneof.class,
            3,
            integer(1, 1),
            integer(2, 2),
            merging,
            false,
            Registry.NONE);
      case ORDINARY_FIRST:
      case ORDINARY_MERGE:
        return pair(
            UnittestProto.TestNestedMessageHasBits.class,
            1,
            integer(1, 1),
            integer(1, 2),
            merging,
            false,
            Registry.NONE);
      case ONEOF_FIRST:
      case ONEOF_MERGE:
        return pair(
            UnittestProto.TestOneof2.class,
            7,
            integer(2, 1),
            integer(2, 2),
            merging,
            false,
            Registry.NONE);
      case ONEOF_GROUP_FIRST:
      case ONEOF_GROUP_MERGE:
        return pair(
            UnittestProto.TestOneof2.class,
            8,
            integer(9, 1),
            message(10, new byte[] {'a'}),
            merging,
            true,
            Registry.NONE);
      case GROUP_FIRST:
      case GROUP_MERGE:
        return pair(
            TestAllTypesProto2.class,
            201,
            integer(202, 1),
            integer(203, 2),
            merging,
            true,
            Registry.NONE);
      case EXTENSION_FIRST:
      case EXTENSION_MERGE:
        return pair(
            UnittestProto.TestAllExtensions.class,
            19,
            integer(1, 1),
            integer(2, 2),
            merging,
            false,
            Registry.UNITTEST);
      case UNKNOWN:
        return unchanged(UnittestProto.TestAllTypes.class, concat(integer(1, 1), integer(1000, 1)));
      case CLOSED_ENUM_KNOWN:
        return unchanged(UnittestProto.OpenEnumMessage.class, integer(5, 1));
      case CLOSED_ENUM_UNKNOWN:
        return unchanged(UnittestProto.OpenEnumMessage.class, integer(5, 3));
      case OPEN_ENUM_UNKNOWN:
        return unchanged(UnittestProto.OpenEnumMessage.class, integer(1, 3));
      case REPEATED_EXPANDED:
        return unchanged(UnittestProto.TestAllTypes.class, concat(integer(31, 1), integer(31, 2)));
      case REPEATED_PACKED:
        return unchanged(UnittestProto.TestPackedTypes.class, message(90, new byte[] {1, 2}));
      case REPEATED_MESSAGE:
        return unchanged(
            UnittestProto.TestAllTypes.class,
            concat(message(48, integer(1, 1)), message(48, integer(1, 2))));
      case MAP_SCALAR_IMPLICIT_OPEN:
      case MAP_REPLACE_IMPLICIT_OPEN:
      case MAP_MESSAGE_IMPLICIT_OPEN:
      case MAP_MESSAGE_REPLACE_IMPLICIT_OPEN:
      case MAP_ENUM_UNKNOWN_IMPLICIT_OPEN:
      case MAP_SCALAR_EXPLICIT_CLOSED:
      case MAP_REPLACE_EXPLICIT_CLOSED:
      case MAP_MESSAGE_EXPLICIT_CLOSED:
      case MAP_MESSAGE_REPLACE_EXPLICIT_CLOSED:
      case MAP_ENUM_UNKNOWN_EXPLICIT_CLOSED:
        return map(input);
      case WIDE_ORDINARY_FIELD1_FIRST:
      case WIDE_ORDINARY_FIELD1_MERGE:
      case WIDE_ORDINARY_FIELD65_FIRST:
      case WIDE_ORDINARY_FIELD65_MERGE:
        return pair(
            WideMessageFields.class,
            input.name().contains("FIELD65") ? 65 : 1,
            integer(1, 1),
            integer(1, 2),
            merging,
            false,
            Registry.NONE);
      case WIDE_ONEOF_FIELD1_FIRST:
      case WIDE_ONEOF_FIELD1_MERGE:
      case WIDE_ONEOF_FIELD128_FIRST:
      case WIDE_ONEOF_FIELD128_MERGE:
        return pair(
            WideMessageOneof.class,
            input.name().contains("FIELD128") ? 128 : 1,
            integer(1, 1),
            integer(1, 2),
            merging,
            false,
            Registry.NONE);
      case SCALAR494_FIELD1:
        return unchanged(WideOneof.class, integer(1, 1));
      case SCALAR494_FIELD494:
        return unchanged(WideOneof.class, integer(494, 1));
      case SCALAR495_FIELD1:
        return unchanged(Wide495.class, integer(1, 1));
      case SCALAR495_FIELD495:
        return unchanged(Wide495.class, integer(495, 1));
      case ENUMS138:
        return unchanged(benchmark.fixtures.enum138.ClosedEnums.class, enumFields(138));
      case ENUMS139:
        return unchanged(benchmark.fixtures.enum139.ClosedEnums.class, enumFields(139));
      case ENUMS900:
        return unchanged(WideClosedEnum.class, enumFields(900));
      case DEPTH2:
      case DEPTH8:
        byte[] chain = integer(2, 1);
        int depth = input == Input.DEPTH2 ? 2 : 8;
        for (int i = 1; i < depth; i++) {
          chain = message(1, chain);
        }
        return unchanged(UnittestProto.TestRecursiveMessage.class, chain);
      case EXISTING_BINARY_EXPANDED_FIRST:
      case EXISTING_BINARY_EXPANDED_MERGE:
        return pair(
            Proto2TextFormatPerformanceProto.ContainsSubMessageWithRepeatedInt32.class,
            1,
            integer(2, 1),
            integer(2, 2),
            merging,
            false,
            Registry.NONE);
      case EXISTING_BINARY_EXTENSION_FIRST:
      case EXISTING_BINARY_EXTENSION_MERGE:
        return pair(
            Proto2TextFormatPerformanceProto.ContainsExtensionSubMessage.class,
            1,
            integer(2, 1),
            integer(2, 2),
            merging,
            false,
            Registry.TEXT_FORMAT);
      case EXISTING_BINARY_PACKED_FIRST:
      case EXISTING_BINARY_PACKED_MERGE:
        byte[] first = message(1, message(2, new byte[] {1}));
        return new BenchmarkInput(
            Proto3TextFormatPerformanceProto.ContainsSubMessageWithRepeatedInt32.class,
            merging ? concat(first, message(1, message(2, new byte[] {2}))) : first,
            merging ? message(1, message(2, new byte[] {1, 2})) : first,
            Registry.NONE,
            merging ? 1 : 0);
      case SCALING_ORDINARY_1024:
      case SCALING_ORDINARY_2048:
      case SCALING_ONEOF_1024:
      case SCALING_ONEOF_2048:
      case SCALING_EXTENSION_1024:
      case SCALING_EXTENSION_2048:
        return scaling(input);
      case WIDE_ORDINARY_ALL_FIRST:
      case WIDE_ORDINARY_ALL_MERGE:
        byte[][] initial = new byte[65][];
        byte[][] second = new byte[65][];
        byte[][] expected = new byte[65][];
        for (int i = 0; i < 65; i++) {
          initial[i] = message(i + 1, integer(1, 1));
          second[i] = message(i + 1, integer(1, 2));
          expected[i] = message(i + 1, concat(integer(1, 1), integer(1, 2)));
        }
        return new BenchmarkInput(
            WideMessageFields.class,
            merging ? concat(concat(initial), concat(second)) : concat(initial),
            merging ? concat(expected) : concat(initial),
            Registry.NONE,
            merging ? 65 : 0);
      default:
        throw new AssertionError(input);
    }
  }

  private static BenchmarkInput pair(
      Class<? extends Message> type,
      int field,
      byte[] child1,
      byte[] child2,
      boolean merging,
      boolean delimited,
      Registry registry)
      throws IOException {
    byte[] first = wrap(field, child1, delimited);
    return new BenchmarkInput(
        type,
        merging ? concat(first, wrap(field, child2, delimited)) : first,
        merging ? wrap(field, concat(child1, child2), delimited) : first,
        registry,
        merging ? 1 : 0);
  }

  private static BenchmarkInput map(Input input) throws IOException {
    Class<? extends Message> type =
        input.name().endsWith("IMPLICIT_OPEN")
            ? MapTestProto.TestMap.class
            : MapForProto2TestProto.TestMap.class;
    if (input.name().contains("ENUM_UNKNOWN")) {
      return unchanged(type, message(4, concat(integer(1, 1), integer(2, 99))));
    }
    boolean child = input.name().startsWith("MAP_MESSAGE");
    boolean replace = input.name().contains("REPLACE");
    int field = child ? 5 : 1;
    byte[] first =
        message(field, concat(integer(1, 1), child ? message(2, integer(1, 1)) : integer(2, 1)));
    byte[] second =
        message(field, concat(integer(1, 1), child ? message(2, integer(1, 2)) : integer(2, 2)));
    return new BenchmarkInput(
        type, replace ? concat(first, second) : first, replace ? second : first, Registry.NONE, 0);
  }

  private static BenchmarkInput scaling(Input input) throws IOException {
    int count = input.name().endsWith("1024") ? 1024 : 2048;
    Class<? extends Message> type;
    int field;
    Registry registry;
    if (input.name().startsWith("SCALING_ORDINARY")) {
      type = UnittestProto.TestAllTypes.class;
      field = 18;
      registry = Registry.NONE;
    } else if (input.name().startsWith("SCALING_ONEOF")) {
      type = UnittestProto.TestOneof2.class;
      field = 7;
      registry = Registry.NONE;
    } else {
      type = UnittestProto.TestAllExtensions.class;
      field = 18;
      registry = Registry.UNITTEST;
    }
    // Unknown varints accumulate, so each merge increases the child contents.
    byte[] child = integer(20000, 1);
    return new BenchmarkInput(
        type,
        repeat(message(field, child), count),
        message(field, repeat(child, count)),
        registry,
        count - 1);
  }

  private static byte[] flat() throws IOException {
    byte[] bytes = new byte[26];
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    output.writeInt32(1, 1);
    output.writeInt32(2, 2);
    output.writeFixed32(7, 3);
    output.writeDouble(12, 1.0);
    output.writeBool(13, true);
    output.writeString(14, "a");
    output.writeByteArray(15, new byte[] {'b'});
    output.checkNoSpaceLeft();
    return bytes;
  }

  private static byte[] enumFields(int count) throws IOException {
    byte[][] fields = new byte[count][];
    for (int i = 0; i < count; i++) {
      fields[i] = integer(i + 1, 1);
    }
    return concat(fields);
  }

  private static byte[] integer(int field, int value) throws IOException {
    byte[] bytes = new byte[CodedOutputStream.computeInt32Size(field, value)];
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    output.writeInt32(field, value);
    output.checkNoSpaceLeft();
    return bytes;
  }

  private static byte[] message(int field, byte[] child) throws IOException {
    byte[] bytes = new byte[CodedOutputStream.computeByteArraySize(field, child)];
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    output.writeByteArray(field, child);
    output.checkNoSpaceLeft();
    return bytes;
  }

  private static byte[] wrap(int field, byte[] child, boolean delimited) throws IOException {
    if (!delimited) {
      return message(field, child);
    }
    byte[] bytes = new byte[2 * CodedOutputStream.computeTagSize(field) + child.length];
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    output.writeTag(field, 3);
    output.writeRawBytes(child);
    output.writeTag(field, 4);
    output.checkNoSpaceLeft();
    return bytes;
  }

  private static byte[] concat(byte[]... parts) {
    int length = 0;
    for (byte[] part : parts) {
      length += part.length;
    }
    byte[] result = new byte[length];
    int offset = 0;
    for (byte[] part : parts) {
      System.arraycopy(part, 0, result, offset, part.length);
      offset += part.length;
    }
    return result;
  }

  private static byte[] repeat(byte[] bytes, int count) {
    byte[] result = new byte[bytes.length * count];
    for (int i = 0; i < count; i++) {
      System.arraycopy(bytes, 0, result, i * bytes.length, bytes.length);
    }
    return result;
  }
}
