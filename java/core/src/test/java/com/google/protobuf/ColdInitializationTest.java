// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf;

import static com.google.common.truth.Truth.assertThat;
import static java.util.concurrent.TimeUnit.SECONDS;

import com.google.protobuf.JavaFeaturesProto.JavaFeatures.NestInFileClassFeature.NestInFileClass;
import com.google.protobuf.test.ColdInitializationTestProto;
import com.google.protobuf.test.ColdLeft;
import com.google.protobuf.test.ColdRight;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class ColdInitializationTest {
  @Test(timeout = 60000)
  public void concurrentRuntimeBootstrap() throws Exception {
    assertColdInitialization(3, 4);
  }

  @Test(timeout = 60000)
  public void concurrentParsingAndFeatureResolution() throws Exception {
    assertColdInitialization(0, 1, 2, 3, 4);
  }

  private static void assertColdInitialization(int... operations) throws Exception {
    AtomicInteger workerId = new AtomicInteger();
    ExecutorService executor =
        Executors.newFixedThreadPool(
            operations.length,
            task -> {
              Thread worker = new Thread(task, "protobuf-cold-init-" + workerId.getAndIncrement());
              // A thread waiting for class initialization cannot be interrupted after a deadlock.
              worker.setDaemon(true);
              return worker;
            });
    try {
      for (int iteration = 0; iteration < 16; iteration++) {
        // Isolate the runtime as well as the fixtures; a fresh message class alone is not cold.
        try (URLClassLoader loader =
            new URLClassLoader(classPath(), ClassLoader.getPlatformClassLoader())) {
          Method probe = loader.loadClass(Probe.class.getName()).getMethod("run", int.class);
          assertThat(probe.getDeclaringClass().getClassLoader()).isSameInstanceAs(loader);
          CountDownLatch ready = new CountDownLatch(operations.length);
          CountDownLatch start = new CountDownLatch(1);
          List<Future<?>> futures = new ArrayList<>();
          try {
            for (int worker = 0; worker < operations.length; worker++) {
              int operation = operations[(worker + iteration) % operations.length];
              futures.add(
                  executor.submit(
                      () -> {
                        ready.countDown();
                        start.await();
                        probe.invoke(null, operation);
                        return null;
                      }));
            }
            assertThat(ready.await(10, SECONDS)).isTrue();
            start.countDown();
            for (Future<?> future : futures) {
              future.get(10, SECONDS);
            }
          } catch (TimeoutException failure) {
            throw new AssertionError(threadDump(iteration), failure);
          } finally {
            start.countDown();
          }
        }
      }
    } finally {
      executor.shutdownNow();
    }
  }

  private static URL[] classPath() {
    Set<URL> locations = new LinkedHashSet<>();
    for (Class<?> type :
        new Class<?>[] {
          ColdInitializationTest.class,
          GeneratedMessage.class,
          MessageLite.class,
          DescriptorProtos.class,
          JavaFeaturesProto.class,
          ColdLeft.class,
          ColdRight.class
        }) {
      locations.add(type.getProtectionDomain().getCodeSource().getLocation());
    }
    return locations.toArray(new URL[0]);
  }

  private static String threadDump(int iteration) {
    StringBuilder dump = new StringBuilder("Cold initialization timed out in iteration " + iteration);
    for (Map.Entry<Thread, StackTraceElement[]> entry : Thread.getAllStackTraces().entrySet()) {
      Thread thread = entry.getKey();
      if (thread.getName().startsWith("protobuf-cold-init-")) {
        dump.append('\n').append(thread.getName()).append(' ').append(thread.getState());
        for (StackTraceElement frame : entry.getValue()) {
          dump.append("\n  at ").append(frame);
        }
      }
    }
    return dump.toString();
  }

  // Loaded by the isolated loader; keep this probe independent of JUnit and Truth.
  public static final class Probe {
    // Both messages have the same wire layout, with only a value in each nested message.
    private static final byte[] INPUT = {
      8, 42, 18, 2, 8, 7, 26, 2, 8, 8, 34, 9, 10, 3, 'k', 'e', 'y', 18, 2, 8, 9,
      42, 4, 'c', 'o', 'l', 'd'
    };

    public static void run(int operation) throws Exception {
      switch (operation) {
        case 0:
          ColdLeft left = ColdLeft.parseFrom(INPUT);
          if (left.getValue() != 42
              || !left.hasChild()
              || left.getChild().getValue() != 7
              || !left.hasSelected()
              || left.getSelected().getValue() != 8
              || left.getChildrenCount() != 1
              || left.getChildrenOrThrow("key").getValue() != 9
              || !left.getText().equals("cold")) {
            throw new AssertionError("Incorrect ColdLeft contents");
          }
          assertRoundTrip(left);
          return;
        case 1:
          ColdRight right = ColdRight.parseFrom(INPUT);
          if (right.getValue() != 42
              || !right.hasChild()
              || right.getChild().getValue() != 7
              || !right.hasSelected()
              || right.getSelected().getValue() != 8
              || right.getChildrenCount() != 1
              || right.getChildrenOrThrow("key").getValue() != 9
              || !right.getText().equals("cold")) {
            throw new AssertionError("Incorrect ColdRight contents");
          }
          assertRoundTrip(right);
          return;
        case 2:
          assertDescriptors();
          return;
        case 3:
          Descriptors.FileDescriptor descriptorFile = DescriptorProtos.getDescriptor();
          if (descriptorFile.findMessageTypeByName("FileDescriptorProto")
              != DescriptorProtos.FileDescriptorProto.getDescriptor()) {
            throw new AssertionError("Incorrect descriptor.proto descriptors");
          }
          DescriptorProtos.FeatureSet descriptorFeatures = descriptorFile.getFeatures();
          if (descriptorFeatures.getFieldPresence()
                  != DescriptorProtos.FeatureSet.FieldPresence.EXPLICIT
              || !descriptorFeatures.getExtension(JavaFeaturesProto.java_).getLegacyClosedEnum()
              || descriptorFeatures.getExtension(JavaFeaturesProto.java_).getNestInFileClass()
                  != NestInFileClass.LEGACY) {
            throw new AssertionError("Incorrect descriptor.proto feature defaults");
          }
          if (!DescriptorProtos.FileDescriptorProto.parseFrom(new byte[] {10, 1, 'x'})
              .getName()
              .equals("x")) {
            throw new AssertionError("Incorrect descriptor.proto parsing");
          }
          return;
        case 4:
          ExtensionRegistry registry = ExtensionRegistry.newInstance();
          JavaFeaturesProto.registerAllExtensions(registry);
          DescriptorProtos.FeatureSet features =
              DescriptorProtos.FeatureSet.parseFrom(
                  new byte[] {(byte) 0xca, 0x3e, 2, 40, 1}, registry);
          if (!features.hasExtension(JavaFeaturesProto.java_)
              || features.getExtension(JavaFeaturesProto.java_).getNestInFileClass()
                  != NestInFileClass.NO) {
            throw new AssertionError("Incorrect JavaFeatures extension parsing");
          }
          return;
        default:
          throw new AssertionError(operation);
      }
    }

    private static void assertRoundTrip(MessageLite message) {
      if (!Arrays.equals(INPUT, message.toByteArray())) {
        throw new AssertionError("Incorrect round trip for " + message.getClass().getName());
      }
    }

    private static void assertDescriptors() {
      Descriptors.FileDescriptor file = ColdInitializationTestProto.getDescriptor();
      Descriptors.Descriptor left = file.findMessageTypeByName("ColdLeft");
      Descriptors.Descriptor right = file.findMessageTypeByName("ColdRight");
      if (left.findFieldByName("child").getMessageType() != right
          || right.findFieldByName("child").getMessageType() != left
          || !left.findFieldByName("value").hasPresence()
          || !right.findFieldByName("value").hasPresence()
          || !left.findFieldByName("text").needsUtf8Check()
          || !right.findFieldByName("text").needsUtf8Check()
          || left.getFeatures().getExtension(JavaFeaturesProto.java_).getNestInFileClass()
              != NestInFileClass.NO
          || right.getFeatures().getExtension(JavaFeaturesProto.java_).getNestInFileClass()
              != NestInFileClass.NO) {
        throw new AssertionError("Incorrect Editions descriptors or resolved features");
      }
    }
  }
}
