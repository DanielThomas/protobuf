// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf.benchmarks;

import com.google.protobuf.CodedInputStream;
import com.google.protobuf.UnknownFieldSet;
import java.io.ByteArrayInputStream;
import java.util.concurrent.TimeUnit;
import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Param;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.Setup;
import org.openjdk.jmh.annotations.State;

@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
public class UnknownFieldBenchmarks {
  @State(Scope.Thread)
  public static class AlternatingFields {
    @Param({"200000"})
    public int pairs;

    byte[] wire;

    @Setup
    public void setup() {
      wire = new byte[Math.multiplyExact(pairs, 4)];
      for (int i = 0; i < wire.length; i += 4) {
        wire[i] = 0x08;
        wire[i + 1] = 0x08;
        wire[i + 2] = 0x20;
        wire[i + 3] = 0x20;
      }
    }
  }

  @State(Scope.Thread)
  public static class AddedFields {
    @Param({"100000"})
    public int fields;
  }

  @Benchmark
  public UnknownFieldSet.Builder alternatingFieldNumbers(AlternatingFields state) throws Exception {
    CodedInputStream input = CodedInputStream.newInstance(new ByteArrayInputStream(state.wire));
    UnknownFieldSet.Builder builder = UnknownFieldSet.newBuilder();
    builder.mergeFrom(input);
    return builder;
  }

  @Benchmark
  public UnknownFieldSet addFields(AddedFields state) {
    UnknownFieldSet.Builder builder = UnknownFieldSet.newBuilder();
    for (int i = 1; i <= state.fields; i++) {
      UnknownFieldSet.Field field = UnknownFieldSet.Field.newBuilder().addFixed32(i).build();
      builder.addField(i, field);
    }
    return builder.build();
  }

  public static void main(String[] args) throws Exception {
    UnknownFieldBenchmarks benchmark = new UnknownFieldBenchmarks();
    AlternatingFields alternating = new AlternatingFields();
    alternating.pairs = 200000;
    alternating.setup();
    UnknownFieldSet parsed = benchmark.alternatingFieldNumbers(alternating).build();
    for (int field : new int[] {1, 4}) {
      if (parsed.getField(field).getVarintList().size() != alternating.pairs) {
        throw new AssertionError("Incorrect alternating-field count");
      }
      for (long value : parsed.getField(field).getVarintList()) {
        if (value != (field == 1 ? 8 : 32)) {
          throw new AssertionError("Incorrect alternating-field value");
        }
      }
    }
    AddedFields added = new AddedFields();
    added.fields = 100000;
    UnknownFieldSet built = benchmark.addFields(added);
    if (built.asMap().size() != added.fields) {
      throw new AssertionError("Incorrect added-field count");
    }
    for (int i = 1; i <= added.fields; i++) {
      if (!built.getField(i).getFixed32List().equals(java.util.Collections.singletonList(i))) {
        throw new AssertionError("Incorrect added-field value");
      }
    }
    System.out.println("Existing unknown-field workloads verified.");
  }
}
