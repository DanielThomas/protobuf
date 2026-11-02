// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf.benchmarks;

import com.google.protobuf.ExtensionRegistry;
import com.google.protobuf.ExtensionRegistryLite;
import com.google.protobuf.Message;
import com.google.protobuf.Parser;
import com.google.protobuf.testing.textformat.performance.proto2.Proto2TextFormatPerformanceProto;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.concurrent.TimeUnit;
import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Param;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.Setup;
import org.openjdk.jmh.annotations.State;
import proto2_unittest.UnittestProto;

@State(Scope.Thread)
@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
public class ParseBenchmark {
  public enum Input {
    SCALAR_PROTO2,
    SCALAR_PROTO3,
    SCALAR_EDITIONS,
    PRESENCE_PROTO2_ZERO,
    PRESENCE_PROTO3_ZERO,
    PRESENCE_EDITIONS_ZERO,
    FLAT_PROTO2,
    FLAT_PROTO3,
    FLAT_EDITIONS,
    REQUIRED_SCALARS,
    ORDINARY_SCALARS_FIRST,
    ORDINARY_SCALARS_MERGE,
    ONEOF_SCALARS_FIRST,
    ONEOF_SCALARS_MERGE,
    ORDINARY_FIRST,
    ORDINARY_MERGE,
    ONEOF_FIRST,
    ONEOF_MERGE,
    ONEOF_GROUP_FIRST,
    ONEOF_GROUP_MERGE,
    GROUP_FIRST,
    GROUP_MERGE,
    EXTENSION_FIRST,
    EXTENSION_MERGE,
    UNKNOWN,
    CLOSED_ENUM_KNOWN,
    CLOSED_ENUM_UNKNOWN,
    OPEN_ENUM_UNKNOWN,
    REPEATED_EXPANDED,
    REPEATED_PACKED,
    REPEATED_MESSAGE,
    MAP_SCALAR_IMPLICIT_OPEN,
    MAP_REPLACE_IMPLICIT_OPEN,
    MAP_MESSAGE_IMPLICIT_OPEN,
    MAP_MESSAGE_REPLACE_IMPLICIT_OPEN,
    MAP_ENUM_UNKNOWN_IMPLICIT_OPEN,
    MAP_SCALAR_EXPLICIT_CLOSED,
    MAP_REPLACE_EXPLICIT_CLOSED,
    MAP_MESSAGE_EXPLICIT_CLOSED,
    MAP_MESSAGE_REPLACE_EXPLICIT_CLOSED,
    MAP_ENUM_UNKNOWN_EXPLICIT_CLOSED,
    WIDE_ORDINARY_FIELD1_FIRST,
    WIDE_ORDINARY_FIELD1_MERGE,
    WIDE_ORDINARY_FIELD65_FIRST,
    WIDE_ORDINARY_FIELD65_MERGE,
    WIDE_ONEOF_FIELD1_FIRST,
    WIDE_ONEOF_FIELD1_MERGE,
    WIDE_ONEOF_FIELD128_FIRST,
    WIDE_ONEOF_FIELD128_MERGE,
    SCALAR494_FIELD1,
    SCALAR494_FIELD494,
    SCALAR495_FIELD1,
    SCALAR495_FIELD495,
    ENUMS138,
    ENUMS139,
    ENUMS900,
    DEPTH2,
    DEPTH8,
    EXISTING_BINARY_EXPANDED_FIRST,
    EXISTING_BINARY_EXPANDED_MERGE,
    EXISTING_BINARY_EXTENSION_FIRST,
    EXISTING_BINARY_EXTENSION_MERGE,
    EXISTING_BINARY_PACKED_FIRST,
    EXISTING_BINARY_PACKED_MERGE,
    SCALING_ORDINARY_1024,
    SCALING_ORDINARY_2048,
    SCALING_ONEOF_1024,
    SCALING_ONEOF_2048,
    SCALING_EXTENSION_1024,
    SCALING_EXTENSION_2048,
    WIDE_ORDINARY_ALL_FIRST,
    WIDE_ORDINARY_ALL_MERGE
  }

  @Param public Input input;

  private BenchmarkInput data;
  private Message prototype;
  private Parser<? extends Message> parser;
  private ExtensionRegistryLite registry;

  @Setup
  public void setup() throws Exception {
    data = BenchmarkInput.create(input);
    prototype = (Message) data.type.getMethod("getDefaultInstance").invoke(null);
    parser = prototype.getParserForType();
    switch (data.registry) {
      case UNITTEST:
        ExtensionRegistry unittest = ExtensionRegistry.newInstance();
        UnittestProto.registerAllExtensions(unittest);
        registry = unittest;
        break;
      case TEXT_FORMAT:
        ExtensionRegistry textFormat = ExtensionRegistry.newInstance();
        Proto2TextFormatPerformanceProto.registerAllExtensions(textFormat);
        registry = textFormat;
        break;
      case NONE:
        registry = ExtensionRegistryLite.getEmptyRegistry();
        break;
      default:
        throw new AssertionError(data.registry);
    }
    Message parsed = parse();
    Message built = wireBuild();
    if (!parsed.equals(built)
        || !parsed.isInitialized()
        || !Arrays.equals(data.expected, parsed.toByteArray())
        || !Arrays.equals(data.expected, built.toByteArray())) {
      throw new AssertionError(
          "Incorrect "
              + input
              + ": expected "
              + hex(data.expected)
              + ", parser "
              + hex(parsed.toByteArray())
              + ", builder "
              + hex(built.toByteArray()));
    }
  }

  @Benchmark
  public Message parse() throws Exception {
    return parser.parseFrom(data.wire, registry);
  }

  @Benchmark
  public Message wireBuild() throws Exception {
    return prototype.newBuilderForType().mergeFrom(data.wire, registry).build();
  }

  private static String hex(byte[] bytes) {
    StringBuilder result = new StringBuilder();
    for (byte value : bytes) {
      result.append(Character.forDigit((value >>> 4) & 15, 16));
      result.append(Character.forDigit(value & 15, 16));
    }
    return result.toString();
  }

  public static void main(String[] args) throws Exception {
    for (Input input : Input.values()) {
      ParseBenchmark benchmark = new ParseBenchmark();
      benchmark.input = input;
      benchmark.setup();
      BenchmarkInput data = benchmark.data;
      MessageDigest digest = MessageDigest.getInstance("SHA-256");
      System.out.printf(
          "%s type=%s bytes=%d merges=%d wire=%s expected=%s%n",
          input,
          data.type.getName(),
          data.wire.length,
          data.merges,
          hex(digest.digest(data.wire)),
          hex(digest.digest(data.expected)));
    }
  }
}
