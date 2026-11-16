import builder_probe.ConstructorCounts;
import com.google.protobuf.ByteString;
import com.google.protobuf.CodedInputStream;
import com.google.protobuf.CodedOutputStream;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.DiscardUnknownFieldsParser;
import com.google.protobuf.Internal;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.Message;
import com.google.protobuf.MessageLite;
import com.google.protobuf.Parser;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.Random;

public final class GeneratedStaticParserTest {
  private static int checks;

  private static final class FailingInput extends InputStream {
    private final byte[] wire;
    private final int failure;
    private int position;

    FailingInput(byte[] wire, int failure) {
      this.wire = wire;
      this.failure = failure;
    }

    @Override
    public int read() throws IOException {
      if (position == failure) throw new IOException("injected failure");
      return position == wire.length ? -1 : wire[position++] & 255;
    }

    @Override
    public int read(byte[] target, int offset, int length) throws IOException {
      if (length == 0) return 0;
      if (position == failure) throw new IOException("injected failure");
      if (position == wire.length) return -1;
      int count = Math.min(length, Math.min(wire.length, failure) - position);
      System.arraycopy(wire, position, target, offset, count);
      position += count;
      return count;
    }
  }

  private static Message prototype(String prefix, String type) throws Exception {
    return (Message) Class.forName(prefix + "." + type)
        .getMethod("getDefaultInstance").invoke(null);
  }

  private static void checkDefaults(Message prototype) throws IOException {
    Message parsed = prototype.getParserForType().parsePartialFrom(new byte[0]);
    for (FieldDescriptor field : prototype.getDescriptorForType().getFields()) {
      Object expected = prototype.getField(field);
      Object actual = parsed.getField(field);
      if (!expected.equals(actual)) throw new AssertionError("Incorrect default: " + field);
      if (!field.isRepeated()
          && field.getJavaType() == FieldDescriptor.JavaType.BYTE_STRING && actual != expected) {
        throw new AssertionError("Copied bytes default while parsing: " + field);
      }
    }
  }

  private static void checkImmutable(Message message) {
    for (FieldDescriptor field : message.getDescriptorForType().getFields()) {
      if (field.isRepeated()) {
        Object values = message.getField(field);
        if (!(values instanceof Internal.ProtobufList)
            || ((Internal.ProtobufList<?>) values).isModifiable()) {
          throw new AssertionError("Published mutable repeated storage: " + field);
        }
      }
    }
  }

  private static byte[] serialized(Message prototype, int seed) {
    Random random = new Random(seed * 0x9e3779b97f4a7c15L);
    Message.Builder builder = prototype.newBuilderForType();
    for (FieldDescriptor field : prototype.getDescriptorForType().getFields()) {
      if (!field.isRequired() && random.nextBoolean()) continue;
      Object value;
      switch (field.getJavaType()) {
        case INT: value = random.nextInt(); break;
        case LONG: value = random.nextLong(); break;
        case BOOLEAN: value = random.nextBoolean(); break;
        case FLOAT: value = Float.intBitsToFloat(random.nextInt()); break;
        case DOUBLE: value = Double.longBitsToDouble(random.nextLong()); break;
        case BYTE_STRING: value = ByteString.copyFrom(new byte[] {0, 1, -1, (byte) seed}); break;
        default: throw new AssertionError(field);
      }
      if (field.isRepeated()) {
        for (int i = random.nextInt(5); i > 0; i--) builder.addRepeatedField(field, value);
      } else {
        builder.setField(field, value);
      }
    }
    return builder.build().toByteArray();
  }

  private static byte[] concat(byte[]... pieces) throws IOException {
    ByteArrayOutputStream output = new ByteArrayOutputStream();
    for (byte[] piece : pieces) output.write(piece);
    return output.toByteArray();
  }

  @SuppressWarnings("unchecked")
  private static String parse(Message prototype, byte[] wire, boolean stream,
      boolean discard, int failure, boolean requireNoBuilder) throws IOException {
    Parser<Message> parser = (Parser<Message>) prototype.getParserForType();
    if (discard) parser = DiscardUnknownFieldsParser.wrap(parser);
    InputStream source = failure < 0
        ? new ByteArrayInputStream(wire) : new FailingInput(wire, failure);
    CodedInputStream input = stream
        ? CodedInputStream.newInstance(source) : CodedInputStream.newInstance(wire);
    ConstructorCounts.builders = 0;
    try {
      Message message = parser.parsePartialFrom(input);
      checkImmutable(message);
      return "ok:" + message.isInitialized() + ":" + Arrays.toString(message.toByteArray());
    } catch (InvalidProtocolBufferException expected) {
      MessageLite unfinished = expected.getUnfinishedMessage();
      if (unfinished instanceof Message) checkImmutable((Message) unfinished);
      return "invalid:" + expected.getMessage() + ":"
          + (unfinished == null ? "null" : Arrays.toString(unfinished.toByteArray()));
    } finally {
      if (requireNoBuilder && ConstructorCounts.builders != 0) {
        throw new AssertionError("Constructed " + ConstructorCounts.builders + " builders");
      }
    }
  }

  private static void compare(Message control, Message candidate, byte[] wire,
      boolean stream, boolean discard, int failure) throws IOException {
    String expected = parse(control, wire, stream, discard, failure, false);
    String actual = parse(candidate, wire, stream, discard, failure, true);
    if (!expected.equals(actual)) {
      throw new AssertionError(candidate.getDescriptorForType().getName()
          + " stream=" + stream + " discard=" + discard + " failure=" + failure
          + "\nwire: " + Arrays.toString(wire)
          + "\nexpected: " + expected + "\nactual: " + actual);
    }
    checks++;
  }

  public static void main(String[] args) throws Exception {
    ByteArrayOutputStream unknownBytes = new ByteArrayOutputStream();
    CodedOutputStream unknown = CodedOutputStream.newInstance(unknownBytes);
    unknown.writeUInt64(500, -1);
    unknown.writeBytes(501, ByteString.copyFromUtf8("unknown"));
    unknown.flush();
    byte[] tail = unknownBytes.toByteArray();
    for (String type : new String[] {"Scalars", "Empty", "Wide", "Repeated"}) {
      Message control = prototype("static_control", type);
      Message candidate = prototype("static_parser", type);
      checkDefaults(control);
      checkDefaults(candidate);
      for (int seed = 0; seed < 16; seed++) {
        byte[] canonical = serialized(control, seed);
        byte[] wire = concat(tail, canonical, serialized(control, seed + 1), tail);
        for (boolean stream : new boolean[] {false, true}) {
          for (boolean discard : new boolean[] {false, true}) {
            compare(control, candidate, canonical, stream, discard, -1);
            compare(control, candidate, wire, stream, discard, -1);
            if (seed == 0) {
              for (int end = 0; end < wire.length; end++) {
                compare(control, candidate, Arrays.copyOf(wire, end), stream, discard, -1);
              }
              for (byte[] malformed : new byte[][] {
                  {0}, {8, -128}, {10, -1, -1, -1, -1, 15}, {14}, {11, 20}}) {
                compare(control, candidate, concat(wire, malformed), stream, discard, -1);
              }
            }
          }
        }
        Message parsed = candidate.getParserForType().parseFrom(canonical);
        Message.Builder builder = parsed.toBuilder();
        Message snapshot = builder.buildPartial();
        builder.mergeFrom(wire);
        if (!Arrays.equals(canonical, snapshot.toByteArray())) {
          throw new AssertionError("Published snapshot changed");
        }
        if (!Arrays.equals(control.newBuilderForType().mergeFrom(canonical).mergeFrom(wire)
            .build().toByteArray(), builder.build().toByteArray())) {
          throw new AssertionError("Stateful builder changed");
        }
        checks++;
      }
      byte[] wire = concat(tail, serialized(control, 0));
      for (boolean discard : new boolean[] {false, true}) {
        for (int failure = 0; failure <= wire.length; failure++) {
          compare(control, candidate, wire, true, discard, failure);
        }
      }
    }
    System.out.println("PASS: " + checks + " generated primitive/repeated/empty/wide comparisons; "
        + "zero parser builder constructions before warmup");
  }
}
