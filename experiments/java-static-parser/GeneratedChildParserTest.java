import builder_probe.ChildParserCounts;
import com.google.protobuf.ByteString;
import com.google.protobuf.CodedInputStream;
import com.google.protobuf.CodedOutputStream;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.DiscardUnknownFieldsParser;
import com.google.protobuf.GeneratedMessage;
import com.google.protobuf.Internal;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.Message;
import com.google.protobuf.MessageLite;
import com.google.protobuf.Parser;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Random;

public final class GeneratedChildParserTest {
  private static final List<String> results = new ArrayList<>();
  private static boolean candidate;
  private static boolean forced;
  private static int happy;

  private static Message prototype(String type) throws Exception {
    return (Message) Class.forName("static_children." + type)
        .getMethod("getDefaultInstance").invoke(null);
  }

  private static final class FailingInput extends InputStream {
    final byte[] wire;
    final int failure;
    int position;

    FailingInput(byte[] wire, int failure) {
      this.wire = wire;
      this.failure = failure;
    }

    @Override
    public int read() throws IOException {
      if (position == failure) throw new IOException("injected failure");
      return position == wire.length ? -1 : wire[position++] & 255;
    }
  }

  private static void checkImmutable(Message message) {
    for (FieldDescriptor field : message.getDescriptorForType().getFields()) {
      Object value = message.getField(field);
      if (field.isRepeated()) {
        if (!(value instanceof Internal.ProtobufList)
            || ((Internal.ProtobufList<?>) value).isModifiable()) {
          throw new AssertionError("Mutable published collection: " + field);
        }
      } else if (field.getJavaType() == FieldDescriptor.JavaType.MESSAGE
          && message.hasField(field)) {
        checkImmutable((Message) value);
      }
    }
  }

  private static Message serialized(Message prototype, Random random, int depth) {
    Message.Builder builder = prototype.newBuilderForType();
    for (FieldDescriptor field : prototype.getDescriptorForType().getFields()) {
      if (field.getJavaType() != FieldDescriptor.JavaType.MESSAGE && random.nextBoolean()) continue;
      Object value;
      switch (field.getJavaType()) {
        case INT: value = random.nextInt(); break;
        case LONG: value = random.nextLong(); break;
        case BOOLEAN: value = random.nextBoolean(); break;
        case FLOAT: value = Float.intBitsToFloat(random.nextInt()); break;
        case BYTE_STRING: value = ByteString.copyFromUtf8("bytes"); break;
        case MESSAGE:
          if (depth == 0) continue;
          value = serialized(builder.newBuilderForField(field).getDefaultInstanceForType(),
              random, depth - 1);
          break;
        default: throw new AssertionError(field);
      }
      if (field.isRepeated()) {
        for (int i = random.nextInt(4); i > 0; i--) builder.addRepeatedField(field, value);
      } else {
        builder.setField(field, value);
      }
    }
    return builder.build();
  }

  private static byte[] concat(byte[]... pieces) throws IOException {
    ByteArrayOutputStream output = new ByteArrayOutputStream();
    for (byte[] piece : pieces) output.write(piece);
    return output.toByteArray();
  }

  private static byte[] unknown(int number) throws IOException {
    ByteArrayOutputStream output = new ByteArrayOutputStream();
    CodedOutputStream coded = CodedOutputStream.newInstance(output);
    coded.writeInt32(number, 91);
    coded.flush();
    return output.toByteArray();
  }

  @SuppressWarnings("unchecked")
  private static void parse(Message prototype, byte[] wire, int backend, boolean discard,
      int failure, int recursion, boolean canonical) throws Exception {
    CodedInputStream input;
    if (backend == 0) {
      input = CodedInputStream.newInstance(wire);
    } else if (backend == 1) {
      input = CodedInputStream.newInstance(failure < 0
          ? new ByteArrayInputStream(wire) : new FailingInput(wire, failure), 16);
    } else {
      ByteBuffer direct = ByteBuffer.allocateDirect(wire.length);
      direct.put(wire);
      direct.flip();
      input = CodedInputStream.newInstance(direct);
    }
    input.setRecursionLimit(recursion);
    Parser<Message> parser = (Parser<Message>) prototype.getParserForType();
    if (discard) parser = DiscardUnknownFieldsParser.wrap(parser);
    ChildParserCounts.reset();
    String result;
    try {
      Message parsed = parser.parsePartialFrom(input);
      checkImmutable(parsed);
      result = "ok:" + parsed.isInitialized() + ":" + Arrays.toString(parsed.toByteArray());
    } catch (InvalidProtocolBufferException expected) {
      MessageLite unfinished = expected.getUnfinishedMessage();
      if (unfinished instanceof Message) checkImmutable((Message) unfinished);
      Throwable cause = expected.getCause();
      result = "invalid:" + expected.getMessage() + ":"
          + (cause == null ? "null" : cause.getClass().getName() + ":" + cause.getMessage())
          + ":" + (unfinished == null ? "null" : Arrays.toString(unfinished.toByteArray()));
    }
    if (candidate && forced && ChildParserCounts.raw != 0) {
      throw new AssertionError("Raw promotion bypassed forced field-builder initialization");
    }
    if (candidate && canonical && !forced) {
      if (ChildParserCounts.ordinary != 0 || ChildParserCounts.raw != 0) {
        throw new AssertionError("Builder on happy path: " + prototype.getDescriptorForType());
      }
      happy++;
    }
    results.add(prototype.getDescriptorForType().getName() + ":backend=" + backend
        + ":discard=" + discard + ":failure=" + failure + ":recursion=" + recursion + ":"
        + result + ":cursor=" + input.getTotalBytesRead() + ":tag=" + input.getLastTag());
  }

  private static Object member(Object owner, Class<?> type, String name) throws Exception {
    Field field = type.getDeclaredField(name);
    field.setAccessible(true);
    return field.get(owner);
  }

  private static void checkRestoration(Message message) throws Exception {
    Class<?> builderType = Class.forName(message.getClass().getName() + "$Builder");
    Method restore = builderType.getDeclaredMethod("restoreState", message.getClass(), boolean.class);
    restore.setAccessible(true);
    ChildParserCounts.reset();
    Message.Builder restored = (Message.Builder) restore.invoke(null, message, false);
    if (ChildParserCounts.raw != 1 || ChildParserCounts.ordinary != 0) {
      throw new AssertionError("Clean promotion ran ordinary default setup");
    }
    for (FieldDescriptor field : message.getDescriptorForType().getFields()) {
      if (field.isRepeated() && member(restored, builderType, field.getJsonName() + "_")
          != message.getField(field)) {
        throw new AssertionError("Copied published collection at promotion");
      }
      if (!field.isRepeated()
          && (field.getJavaType() == FieldDescriptor.JavaType.BYTE_STRING
              || field.getJavaType() == FieldDescriptor.JavaType.MESSAGE)
          && restored.getField(field) != message.getField(field)) {
        throw new AssertionError("Copied published bytes or child at promotion");
      }
    }
    if (member(restored, GeneratedMessage.Builder.class, "unknownFieldsOrBuilder")
        != message.getUnknownFields()) {
      throw new AssertionError("Copied published unknown fields at promotion");
    }
    if (!Arrays.equals(message.toByteArray(), restored.buildPartial().toByteArray())) {
      throw new AssertionError("Incorrect restored presence/default state");
    }
  }

  private static void checkHandoffAndSnapshots() throws Exception {
    Message parent = prototype("Parent");
    FieldDescriptor values = parent.getDescriptorForType().findFieldByName("values");
    FieldDescriptor child = parent.getDescriptorForType().findFieldByName("child");
    Message.Builder leaf = parent.newBuilderForType().newBuilderForField(child);
    FieldDescriptor value = leaf.getDescriptorForType().findFieldByName("value");
    leaf.setField(value, 7);
    Message prefixMessage = parent.newBuilderForType().addRepeatedField(values, 3)
        .setField(child, leaf.build()).build();
    byte[] prefix = concat(prefixMessage.toByteArray(), unknown(500));
    leaf.setField(value, 8);
    byte[] duplicate = parent.newBuilderForType().setField(child, leaf.build()).build().toByteArray();
    byte[] appended = parent.newBuilderForType().addRepeatedField(values, 4).build().toByteArray();
    byte[] wire = concat(prefix, duplicate, appended, unknown(501));
    ChildParserCounts.reset();
    Message parsed = parent.getParserForType().parseFrom(wire);
    if (candidate && !forced) {
      if (ChildParserCounts.ordinary != 0 || ChildParserCounts.raw != 2
          || ChildParserCounts.handoffs != 1 || ChildParserCounts.tag != 18
          || ChildParserCounts.offset != prefix.length + 1) {
        throw new AssertionError("Incorrect raw/pending-tag handoff: ordinary="
            + ChildParserCounts.ordinary + " raw=" + ChildParserCounts.raw
            + " handoffs=" + ChildParserCounts.handoffs + " offset=" + ChildParserCounts.offset);
      }
    }
    if (candidate && !forced) checkRestoration(parsed);
    Message.Builder builder = parsed.toBuilder();
    Message snapshot = builder.build();
    builder.getFieldBuilder(child).setField(value, 9);
    Message changed = builder.build();
    if (((Message) snapshot.getField(child)).getField(value).equals(9)
        || !((Message) changed.getField(child)).getField(value).equals(9)) {
      throw new AssertionError("Broken child invalidation or snapshot");
    }
    builder.getFieldBuilder(child).setField(value, 10);
    results.add("snapshots:" + Arrays.toString(snapshot.toByteArray()) + ":"
        + Arrays.toString(changed.toByteArray()) + ":" + Arrays.toString(builder.build().toByteArray()));
  }

  public static void main(String[] args) throws Exception {
    candidate = args[0].equals("candidate");
    forced = args.length > 2;
    Field flag = GeneratedMessage.class.getDeclaredField("alwaysUseFieldBuilders");
    flag.setAccessible(true);
    flag.setBoolean(null, forced);
    for (String type : new String[] {"Leaf", "Parent", "Recursive", "MutualA", "MutualB",
        "Wide", "Proto3Leaf", "Proto3Parent", "Proto3Recursive"}) {
      Message prototype = prototype(type);
      for (int seed = 0; seed < 8; seed++) {
        Message message = serialized(prototype, new Random(seed * 0x9e3779b97f4a7c15L), 3);
        byte[] canonical = message.toByteArray();
        byte[] wire = concat(unknown(500), canonical,
            serialized(prototype, new Random((seed + 1) * 0x9e3779b97f4a7c15L), 3).toByteArray(), unknown(501));
        for (int backend = 0; backend < 3; backend++) {
          for (boolean discard : new boolean[] {false, true}) {
            parse(prototype, canonical, backend, discard, -1, 100, true);
            parse(prototype, wire, backend, discard, -1, 100, false);
            if (seed == 0) {
              int stride = type.equals("Wide") ? 31 : 1;
              for (int end = 0; end < wire.length; end += stride) {
                parse(prototype, Arrays.copyOf(wire, end), backend, discard, -1, 100, false);
              }
              for (byte[] malformed : new byte[][] {
                  {0}, {8, -128}, {10, -1, -1, -1, -1, 15}, {18, 1, -128}, {11, 20}}) {
                parse(prototype, concat(wire, malformed), backend, discard, -1, 100, false);
              }
              for (int limit = 0; limit < 4; limit++) {
                parse(prototype, wire, backend, discard, -1, limit, false);
              }
            }
          }
        }
        if (candidate && !forced) checkRestoration(message);
      }
      byte[] wire = concat(unknown(500), serialized(prototype, new Random(0), 3).toByteArray());
      int stride = type.equals("Wide") ? 31 : 1;
      for (int failure = 0; failure <= wire.length; failure += stride) {
        parse(prototype, wire, 1, false, failure, 100, false);
      }
    }
    checkHandoffAndSnapshots();
    Path oracle = Paths.get(args[1]);
    if (candidate) {
      Files.write(oracle.resolveSibling(oracle.getFileName() + ".candidate.txt"),
          results, StandardCharsets.UTF_8);
      List<String> expected = Files.readAllLines(oracle, StandardCharsets.UTF_8);
      if (expected.size() != results.size()) throw new AssertionError("Wrong result count");
      for (int i = 0; i < results.size(); i++) {
        if (!expected.get(i).equals(results.get(i))) {
          throw new AssertionError("Case " + i + "\nexpected: " + expected.get(i)
              + "\nactual: " + results.get(i));
        }
      }
    } else {
      Files.write(oracle, results, StandardCharsets.UTF_8);
    }
    System.out.println("PASS: " + results.size() + " ordinary-child comparisons, "
        + happy + " zero-Builder happy parses, forced=" + forced);
  }
}
