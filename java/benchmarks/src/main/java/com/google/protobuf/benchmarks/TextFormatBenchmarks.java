// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf.benchmarks;

import com.google.protobuf.ExtensionRegistry;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;
import com.google.protobuf.testing.textformat.performance.proto2.Proto2TextFormatPerformanceProto;
import com.google.protobuf.testing.textformat.performance.proto3.Proto3TextFormatPerformanceProto;
import java.util.concurrent.TimeUnit;
import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Param;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.Setup;
import org.openjdk.jmh.annotations.State;

@State(Scope.Thread)
@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
public class TextFormatBenchmarks {
  @Param({"expanded", "packed", "extension"})
  public String shape;

  @Param({"1024", "2048", "400000"})
  public int occurrences;

  private String text;
  private Message prototype;
  private ExtensionRegistry registry;

  @Setup
  public void setup() throws Exception {
    registry = ExtensionRegistry.newInstance();
    String occurrence;
    switch (shape) {
      case "expanded":
        prototype =
            Proto2TextFormatPerformanceProto.ContainsSubMessageWithRepeatedInt32
                .getDefaultInstance();
        occurrence = "sub_msg { value: 123 }";
        break;
      case "packed":
        prototype =
            Proto3TextFormatPerformanceProto.ContainsSubMessageWithRepeatedInt32
                .getDefaultInstance();
        occurrence = "sub_msg { value: 123 }";
        break;
      case "extension":
        prototype =
            Proto2TextFormatPerformanceProto.ContainsExtensionSubMessage.getDefaultInstance();
        Proto2TextFormatPerformanceProto.registerAllExtensions(registry);
        occurrence = "[protobuf.testing.textformat.performance.proto2.sub_msg_ext] { value: 123 }";
        break;
      default:
        throw new IllegalArgumentException(shape);
    }
    StringBuilder input = new StringBuilder(Math.multiplyExact(occurrence.length(), occurrences));
    for (int i = 0; i < occurrences; i++) {
      input.append(occurrence);
    }
    text = input.toString();
    Message parsed = merge();
    Message child =
        (Message)
            parsed.getField(
                shape.equals("extension")
                    ? Proto2TextFormatPerformanceProto.subMsgExt.getDescriptor()
                    : parsed.getDescriptorForType().findFieldByName("sub_msg"));
    com.google.protobuf.Descriptors.FieldDescriptor values =
        child.getDescriptorForType().findFieldByName("value");
    if (!parsed.isInitialized() || child.getRepeatedFieldCount(values) != occurrences) {
      throw new AssertionError("Incorrect text-format merge count");
    }
    for (int i = 0; i < occurrences; i++) {
      if (!child.getRepeatedField(values, i).equals(123)) {
        throw new AssertionError("Incorrect text-format value");
      }
    }
  }

  @Benchmark
  public Message merge() throws Exception {
    Message.Builder builder = prototype.newBuilderForType();
    TextFormat.merge(text, registry, builder);
    return builder.build();
  }

  public static void main(String[] args) throws Exception {
    for (String shape : new String[] {"expanded", "packed", "extension"}) {
      for (int count : new int[] {1024, 2048, 400000}) {
        TextFormatBenchmarks benchmark = new TextFormatBenchmarks();
        benchmark.shape = shape;
        benchmark.occurrences = count;
        benchmark.setup();
        System.out.println("Text-format " + shape + " " + count + " occurrences verified.");
      }
    }
  }
}
