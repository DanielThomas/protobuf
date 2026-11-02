// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf.benchmarks;

import org.junit.Test;

public class BenchmarkInputsTest {
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
