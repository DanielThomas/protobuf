import builder_probe.Leaf;
import builder_probe.RepeatedChoices;
import com.google.protobuf.CodedInputStream;
import com.google.protobuf.CodedOutputStream;
import com.google.protobuf.ExtensionRegistryLite;
import com.google.protobuf.GeneratedMessage;
import com.google.protobuf.Internal;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.Message;
import com.google.protobuf.SingleFieldBuilder;
import com.google.protobuf.UnknownFieldSet;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.Base64;
import java.util.List;
import promotion_probe.Envelope;
import promotion_probe.ExchangeCounts;
import promotion_probe.Ordinary;
import promotion_probe.Recursive;

public final class ChildExchangeTest {
  private static final ExtensionRegistryLite REGISTRY = ExtensionRegistryLite.getEmptyRegistry();
  private static final List<Message> TYPES = Arrays.<Message>asList(Leaf.getDefaultInstance(),
      RepeatedChoices.getDefaultInstance(), Envelope.getDefaultInstance(), Ordinary.getDefaultInstance(),
      Recursive.getDefaultInstance());
  private static int comparisons;
  private static boolean oracle;
  private static Path output;

  private static void check(boolean condition, String message) {
    if (!condition) throw new AssertionError(message);
  }

  private static byte[] wire(Object... fields) throws Exception {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    for (int i = 0; i < fields.length; i += 2) {
      if (fields[i + 1] instanceof byte[]) output.writeByteArray((Integer) fields[i], (byte[]) fields[i + 1]);
      else output.writeInt32((Integer) fields[i], (Integer) fields[i + 1]);
    }
    output.flush();
    return bytes.toByteArray();
  }

  private static byte[] concat(byte[]... parts) throws Exception {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    for (byte[] part : parts) bytes.write(part);
    return bytes.toByteArray();
  }

  private static CodedInputStream input(byte[] bytes, int mode, final int failAfter) {
    if (failAfter >= 0) {
      InputStream stream = new InputStream() {
        int offset;
        @Override public int read() throws IOException {
          if (offset == failAfter) throw new IOException("injected");
          return offset == bytes.length ? -1 : bytes[offset++] & 255;
        }
        @Override public int read(byte[] target, int off, int length) throws IOException {
          if (length == 0) return 0;
          int value = read();
          if (value < 0) return -1;
          target[off] = (byte) value;
          return 1;
        }
      };
      return CodedInputStream.newInstance(stream, 8);
    }
    if (mode == 0) return CodedInputStream.newInstance(bytes);
    if (mode == 1) return CodedInputStream.newInstance(new ByteArrayInputStream(bytes), 8);
    ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.length);
    buffer.put(bytes).flip();
    return CodedInputStream.newInstance(buffer);
  }

  private static void immutable(Message message) {
    for (Object value : message.getAllFields().values()) {
      if (value instanceof Message) immutable((Message) value);
      else if (value instanceof List) {
        List<?> list = (List<?>) value;
        try {
          list.clear();
          throw new AssertionError("Published repeated storage accepted clear()");
        } catch (UnsupportedOperationException expected) {
          // Published repeated values must reject mutation, including empty lists.
        }
      }
    }
  }

  private static String encode(Message message) {
    immutable(message);
    return Base64.getEncoder().encodeToString(message.toByteArray());
  }

  private static void compare(String name, Message type, byte[] bytes, int mode,
      int recursion, int failAfter, boolean discard) throws Exception {
    CodedInputStream input = input(bytes, mode, failAfter);
    input.setRecursionLimit(recursion);
    if (discard) {
      Method method = CodedInputStream.class.getDeclaredMethod("discardUnknownFields");
      method.setAccessible(true);
      method.invoke(input);
    }
    String result;
    try {
      result = "ok:" + encode((Message) type.getParserForType().parsePartialFrom(input, REGISTRY));
    } catch (InvalidProtocolBufferException expected) {
      Message unfinished = (Message) expected.getUnfinishedMessage();
      result = expected.getClass().getName() + ":" + expected.getMessage() + ":"
          + (expected.getCause() == null ? "null" : expected.getCause().getClass().getName()) + ":"
          + (unfinished == null ? "null" : encode(unfinished));
    }
    result += ":cursor=" + input.getTotalBytesRead();
    Path path = output.resolve(name + ".txt");
    if (oracle) Files.write(path, result.getBytes("UTF-8"));
    else {
      String baseline = new String(Files.readAllBytes(path), "UTF-8");
      check(result.equals(baseline), name + "\nexpected " + baseline + "\nactual " + result);
    }
    comparisons++;
  }

  private static byte[][] wires(int type) throws Exception {
    byte[] leaf = wire(1, 7, 2, 11, 2, 13, 90, 17);
    byte[] first = wire(1, leaf);
    byte[] history = wire(1, leaf, 3, 19, 1, wire(2, 23), 3, 29, 90, 31);
    if (type == 0) return new byte[][] {leaf, wire(2, new byte[] {1, 2, 3}), wire(2, new byte[0])};
    if (type == 1) return new byte[][] {first, history, wire(1, leaf, 3, 19, 2, leaf, 1, new byte[0])};
    if (type == 2 || type == 3) return new byte[][] {wire(1, first), wire(1, history),
        wire(1, history, 1, wire(1, new byte[0])), wire(1, first, 1, wire(3, 37), 1, first)};
    byte[] recursive = wire(2, history);
    byte[] clean = wire(2, first);
    for (int i = 0; i < 4; i++) {
      recursive = wire(1, recursive);
      clean = wire(1, clean);
    }
    return new byte[][] {clean, recursive, wire(1, clean, 3, 41, 1, recursive)};
  }

  private static void corpus() throws Exception {
    for (int type = 0; type < TYPES.size(); type++) {
      byte[][] wires = wires(type);
      for (int sample = 0; sample < wires.length; sample++) {
        byte[] bytes = wires[sample];
        for (int mode = 0; mode < 3; mode++) {
          for (int length = 0; length <= bytes.length; length++) {
            compare(type + "-" + sample + "-" + mode + "-prefix-" + length, TYPES.get(type),
                Arrays.copyOf(bytes, length), mode, 100, -1, false);
          }
          for (int limit : new int[] {0, 1, 2, 4, 8}) {
            compare(type + "-" + sample + "-" + mode + "-recursion-" + limit,
                TYPES.get(type), bytes, mode, limit, -1, false);
          }
          compare(type + "-" + sample + "-" + mode + "-discard", TYPES.get(type), bytes, mode, 100, -1, true);
          int index = 0;
          for (byte[] tail : new byte[][] {{0}, {12}, {(byte) 0x80}, {10, 3, 8}, {10, 1, 12}, {10, 2, 8, 0}}) {
            compare(type + "-" + sample + "-" + mode + "-tail-" + index++, TYPES.get(type),
                concat(bytes, tail), mode, 100, -1, false);
          }
        }
        for (int at = 0; at <= bytes.length; at++) {
          compare(type + "-" + sample + "-io-" + at, TYPES.get(type), bytes, 1, 100, at, false);
        }
      }
    }
  }

  private static void happy() throws Exception {
    for (int i = 0; i < 128; i++) {
      UnknownFieldSet unknown = UnknownFieldSet.newBuilder().addField(90,
          UnknownFieldSet.Field.newBuilder().addVarint(i + 7).build()).build();
      Leaf leaf = Leaf.newBuilder().setValue(i).addValues(i + 1).setUnknownFields(unknown).build();
      RepeatedChoices choices = RepeatedChoices.newBuilder().setFirst(leaf).build();
      Recursive recursive = Recursive.newBuilder().setLeaf(choices).build();
      for (int depth = 0; depth < i % 8; depth++) recursive = Recursive.newBuilder().setChild(recursive).build();
      List<Message> messages = Arrays.<Message>asList(leaf, choices,
          Envelope.newBuilder().setChild(choices).build(), Ordinary.newBuilder().setChild(choices).build(), recursive);
      for (Message message : messages) {
        byte[] bytes = message.toByteArray();
        for (int mode = 0; mode < 3; mode++) {
          ExchangeCounts.reset();
          Message result = (Message) message.getParserForType().parsePartialFrom(input(bytes, mode, -1), REGISTRY);
          check(ExchangeCounts.builders == 0, "Happy parse constructed " + ExchangeCounts.builders
              + " Builders: " + message.getDescriptorForType().getName());
          check(Arrays.equals(result.toByteArray(), bytes), "Happy parse changed serialized input");
        }
      }
    }
    System.out.println("PASS: zero Builders on 1,920 serializer-produced happy parses before warmup");
  }

  private static Object field(Object owner, String name) throws Exception {
    Class<?> type = name.equals("unknownFieldsOrBuilder") ? GeneratedMessage.Builder.class : owner.getClass();
    Field field = type.getDeclaredField(name);
    field.setAccessible(true);
    return field.get(owner);
  }

  private static void continuation() throws Exception {
    byte[] history = wire(1, wire(2, 7), 3, 9, 1, wire(2, 11, 90, 8), 3, 13, 90, 17);
    Method parse = Envelope.Builder.class.getMethod("parseState", CodedInputStream.class, ExtensionRegistryLite.class);
    ExchangeCounts.reset();
    Object state = parse.invoke(null, input(wire(1, history), 0, -1), REGISTRY);
    check(state instanceof Envelope.Builder, "Hidden child state was published instead of retained");
    Envelope.Builder owner = (Envelope.Builder) state;
    SingleFieldBuilder<?, ?, ?> wrapper = (SingleFieldBuilder<?, ?, ?>) field(owner, "childBuilder_");
    RepeatedChoices.Builder child = (RepeatedChoices.Builder) wrapper.getBuilder();
    check(child == ExchangeCounts.coldChild, "Parent replaced the actual returned child Builder");
    SingleFieldBuilder<?, ?, ?> first = (SingleFieldBuilder<?, ?, ?>) field(child, "firstBuilder_");
    Leaf.Builder leaf = (Leaf.Builder) first.getBuilder();
    Object values = field(leaf, "values_");
    check(((Internal.ProtobufList<?>) values).isModifiable(), "Child return froze live collection state");
    check(field(child, "unknownFieldsOrBuilder") instanceof UnknownFieldSet.Builder,
        "Child return finalized live unknowns");
    owner.mergeFrom(wire(1, wire(1, wire(2, 19))));
    check(field(leaf, "values_") == values && leaf.getValuesCount() == 3 && leaf.getValues(2) == 19,
        "Continuation copied or lost the live child collection");
    Envelope snapshot = owner.buildPartial();
    byte[] snapshotBytes = snapshot.toByteArray();
    leaf.addValues(23);
    check(owner.buildPartial().getChild().getFirst().getValuesCount() == 4,
        "Returned child did not invalidate ancestors");
    check(Arrays.equals(snapshotBytes, snapshot.toByteArray()), "Later merge changed old snapshot");

    byte[] prefix = wire(1, wire(1, wire(2, 7)));
    byte[] bytes = concat(prefix, wire(1, wire(1, wire(2, 11))));
    Envelope.parseFrom(bytes);
    check(ExchangeCounts.pendingTag == 10 && ExchangeCounts.pendingOffset == prefix.length + 1,
        "Handoff did not retain the pending tag before its payload");
    System.out.println("PASS: actual child continuation, live state, pending payload and snapshots");
  }

  public static void main(String[] args) throws Exception {
    if (args[0].equals("happy")) {
      happy();
      return;
    }
    oracle = args[0].equals("oracle");
    boolean forced = args.length > 2 && args[2].equals("forced");
    if (forced) {
      Field field = GeneratedMessage.class.getDeclaredField("alwaysUseFieldBuilders");
      field.setAccessible(true);
      field.setBoolean(null, true);
    }
    output = Paths.get(args[1]);
    Files.createDirectories(output);
    corpus();
    if (!oracle && !forced) {
      continuation();
      happy();
    }
    System.out.println("PASS: " + comparisons + " child return/failure comparisons against pristine output");
  }
}
