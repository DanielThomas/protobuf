// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf.benchmarks;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import benchmark.fixtures.FlatProto2;
import com.google.protobuf.ByteString;
import com.google.protobuf.benchmarks.ParseBenchmark.Input;
import org.junit.Test;

public class BenchmarkInputsTest {
  @Test
  public void flatProto2InputsDecodeTheSameFields() throws Exception {
    BenchmarkInput wide = BenchmarkInput.create(Input.FLAT_PROTO2_WIDE);
    BenchmarkInput compact = BenchmarkInput.create(Input.FLAT_PROTO2_COMPACT);
    FlatProto2 expected =
        FlatProto2.newBuilder()
            .setOptionalInt32(1)
            .setOptionalInt64(2)
            .setOptionalFixed32(3)
            .setOptionalDouble(1.0)
            .setOptionalBool(true)
            .setOptionalString("a")
            .setOptionalBytes(ByteString.copyFromUtf8("b"))
            .build();
    assertArrayEquals(wide.wire, compact.wire);
    assertArrayEquals(expected.toByteArray(), compact.expected);
    assertEquals(expected, FlatProto2.parseFrom(compact.wire));
  }

  @Test
  public void binaryInputsMatchBuilderAndExpectedContents() throws Exception {
    ParseBenchmark.main(new String[0]);
  }

  @Test
  public void unknownFieldWorkloadsRetainTheirContents() throws Exception {
    UnknownFieldBenchmarks.main(new String[0]);
  }

  @Test
  public void textFormatWorkloadsRetainEveryValue() throws Exception {
    TextFormatBenchmarks.main(new String[0]);
  }
}
