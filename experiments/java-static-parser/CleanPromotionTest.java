import builder_probe.Leaf;
import builder_probe.RepeatedChoices;
import com.google.protobuf.CodedInputStream;
import com.google.protobuf.CodedOutputStream;
import com.google.protobuf.ExtensionRegistryLite;
import com.google.protobuf.GeneratedMessage;
import com.google.protobuf.Internal;
import com.google.protobuf.Message;
import com.google.protobuf.SingleFieldBuilder;
import java.io.ByteArrayOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import promotion_probe.Envelope;
import promotion_probe.Ordinary;
import promotion_probe.Recursive;
import promotion_probe.Values;

public final class CleanPromotionTest {
  private static Path output;
  private static boolean oracle;
  private static int comparisons;

  private static void check(boolean condition, String message) {
    if (!condition) throw new AssertionError(message);
  }

  private static Object field(Object owner, String name) throws Exception {
    Field field = owner.getClass().getDeclaredField(name);
    field.setAccessible(true);
    return field.get(owner);
  }

  private static byte[] wire(Object... fields) throws Exception {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    for (int i = 0; i < fields.length; i += 2) {
      int number = (Integer) fields[i];
      Object value = fields[i + 1];
      if (value instanceof byte[]) output.writeByteArray(number, (byte[]) value);
      else output.writeInt32(number, (Integer) value);
    }
    output.flush();
    return bytes.toByteArray();
  }

  private static void compare(String name, Message message) throws Exception {
    Path path = output.resolve(name + ".bin");
    if (oracle) Files.write(path, message.toByteArray());
    else check(Arrays.equals(Files.readAllBytes(path), message.toByteArray()), name);
    comparisons++;
  }

  private static GeneratedMessage.Builder<?> restore(GeneratedMessage message, boolean retained)
      throws Exception {
    Class<?> builder = Class.forName(message.getClass().getName() + "$Builder");
    Method method = builder.getMethod(retained ? "restoreParsingState" : "restorePublishedState", message.getClass());
    return (GeneratedMessage.Builder<?>) method.invoke(null, message);
  }

  private static void retained() throws Exception {
    byte[] initial = wire(1, wire(1, wire(1, 7, 2, 11), 90, 8));
    byte[] switchAway = wire(1, wire(3, 9));
    byte[] reactivate = wire(1, wire(1, wire(2, 13)));
    Envelope clean = Envelope.parseFrom(initial);
    Envelope.Builder owner = oracle ? Envelope.newBuilder().mergeFrom(initial)
        : (Envelope.Builder) restore(clean, true);
    owner.mergeFrom(switchAway).mergeFrom(reactivate);
    check(owner.getChild().getFirst().getValue() == 7, "Retained child history disappeared");
    compare("retained", owner.buildPartial());
    Envelope snapshot = owner.buildPartial();
    byte[] snapshotBytes = snapshot.toByteArray();
    Leaf.Builder leaf = owner.getChildBuilder().getFirstBuilder();
    leaf.addValues(17);
    compare("retained-mutation", owner.buildPartial());
    check(Arrays.equals(snapshotBytes, snapshot.toByteArray()), "Mutation changed old snapshot");

    // Promotion through public Builder access must retain the same history as wire continuation.
    owner = oracle ? Envelope.newBuilder().mergeFrom(initial)
        : (Envelope.Builder) restore(clean, true);
    RepeatedChoices.Builder child = owner.getChildBuilder();
    owner.buildPartial();
    child.mergeFrom(wire(3, 9, 1, new byte[0]));
    compare("public-builder", owner.buildPartial());
    check(owner.getChild().getFirst().getValue() == 7, "Public access lost retained history");
  }

  private static void replacement() throws Exception {
    byte[] initial = wire(1, wire(1, wire(1, 7)));
    for (int operation = 0; operation < 4; operation++) {
      byte[] seed = operation == 3 ? wire(1, new byte[0]) : initial;
      Envelope.Builder owner = oracle ? Envelope.newBuilder().mergeFrom(seed)
          : (Envelope.Builder) restore(Envelope.parseFrom(seed), true);
      if (operation == 1) owner.getChildBuilder();
      if (operation == 2) owner.clearChild();
      RepeatedChoices replacement = RepeatedChoices.parseFrom(wire(1, wire(1, 23)));
      if (operation == 3) owner.mergeChild(replacement);
      else owner.setChild(replacement);
      owner.mergeFrom(wire(1, wire(3, 9), 1, wire(1, new byte[0])));
      check(owner.getChild().getFirst().getValue() == 0,
          "Replacement incorrectly inherited retained parsing provenance");
      compare("replacement-" + operation, owner.buildPartial());
    }
  }

  private static void released() throws Exception {
    byte[] initial = wire(1, wire(1, wire(1, 7)));
    Ordinary clean = Ordinary.parseFrom(initial);
    Ordinary.Builder owner = oracle ? Ordinary.newBuilder().mergeFrom(initial)
        : (Ordinary.Builder) restore(clean, true);
    owner.mergeFrom(wire(1, wire(3, 9), 1, wire(1, new byte[0])));
    compare("released", owner.buildPartial());
    check(owner.getChild().getFirst().getValue() == 0,
        "Promotion retained history across an ordinary publication boundary");
  }

  private static void sharing() throws Exception {
    Leaf clean = Leaf.parseFrom(wire(1, 7, 2, 11, 90, 8));
    Leaf.Builder builder = oracle ? Leaf.newBuilder().mergeFrom(clean)
        : (Leaf.Builder) restore(clean, true);
    if (!oracle) {
      check(field(builder, "values_") == field(clean, "values_"), "Promotion copied immutable list");
      check(!((Internal.ProtobufList<?>) field(builder, "values_")).isModifiable(),
          "Promotion made published storage mutable");
    }
    builder.mergeFrom(wire(2, 13));
    compare("shared-list", builder.buildPartial());
    check(clean.getValuesList().equals(Arrays.asList(11)), "Continuation mutated published list");

    byte[] raw = new byte[] {(byte) 0xff, (byte) 0xfe};
    Values values = Values.parseFrom(wire(1, raw, 2, raw, 3, wire(1, 7, 2, 11), 90, 8));
    Values.Builder copy = oracle ? Values.newBuilder().mergeFrom(values)
        : (Values.Builder) restore(values, true);
    if (!oracle) {
      for (String name : new String[] {"text_", "texts_", "numbers_", "data_"}) {
        check(field(copy, name) == field(values, name), "Promotion converted or copied " + name);
      }
      check(copy.getUnknownFields() == values.getUnknownFields(), "Promotion copied unknown fields");
    }
    copy.mergeFrom(wire(2, new byte[] {65}, 3, wire(1, 13, 2, 17)));
    compare("shared-values", copy.buildPartial());
    check(values.getNumbersCount() == 1 && values.getTextsCount() == 1,
        "Continuation mutated published map or strings");
    compare("shared-original", values);
  }

  private static void recursive() throws Exception {
    byte[] initial = wire(2, wire(1, wire(1, 7, 2, 11)));
    for (int i = 0; i < 20; i++) initial = wire(1, initial);
    Recursive clean = Recursive.parseFrom(initial);
    Recursive.Builder owner = oracle ? Recursive.newBuilder().mergeFrom(initial)
        : (Recursive.Builder) restore(clean, true);
    if (!oracle) {
      SingleFieldBuilder<?, ?, ?> wrapper = (SingleFieldBuilder<?, ?, ?>) field(owner, "childBuilder_");
      check(field(wrapper, "builder") == null, "Promotion eagerly constructed a descendant Builder");
      check(field(wrapper, "message") == clean.getChild(), "Promotion rebuilt descendant message");
    }
    Recursive.Builder cursor = owner;
    for (int i = 0; i < 20; i++) cursor = cursor.getChildBuilder();
    Recursive snapshot = owner.buildPartial();
    byte[] before = snapshot.toByteArray();
    Leaf.Builder leaf = cursor.getLeafBuilder().getFirstBuilder();
    leaf.addValues(13);
    compare("recursive", owner.buildPartial());
    check(Arrays.equals(before, snapshot.toByteArray()), "Recursive snapshot changed");
  }

  private static void failures() throws Exception {
    byte[] initial = wire(1, wire(1, wire(1, 7, 2, 11)));
    byte[] continuation = wire(1, wire(3, 9, 1, wire(2, 13), 90, 8));
    for (int length = 0; length <= continuation.length; length++) {
      Envelope.Builder builder = oracle ? Envelope.newBuilder().mergeFrom(initial)
          : (Envelope.Builder) restore(Envelope.parseFrom(initial), true);
      CodedInputStream input = CodedInputStream.newInstance(Arrays.copyOf(continuation, length));
      String status = "ok";
      try {
        builder.mergeFrom(input, ExtensionRegistryLite.getEmptyRegistry());
      } catch (java.io.IOException expected) {
        status = expected.getClass().getName();
      }
      String name = "prefix-" + length;
      if (oracle) Files.write(output.resolve(name + ".status"), status.getBytes("US-ASCII"));
      else check(status.equals(new String(Files.readAllBytes(output.resolve(name + ".status")), "US-ASCII")),
          "Different failure at " + length);
      compare(name, builder.buildPartial());
      Envelope snapshot = builder.buildPartial();
      byte[] snapshotBytes = snapshot.toByteArray();
      builder.mergeFrom(wire(1, wire(1, wire(2, 17))));
      compare(name + "-continued", builder.buildPartial());
      check(Arrays.equals(snapshotBytes, snapshot.toByteArray()),
          "Failed continuation changed an old snapshot at " + length);
    }
  }

  public static void main(String[] args) throws Exception {
    oracle = args[0].equals("oracle");
    output = Paths.get(args[1]);
    Files.createDirectories(output);
    retained();
    replacement();
    released();
    sharing();
    recursive();
    failures();
    System.out.println("PASS: " + comparisons + " clean-promotion comparisons; retained/released history, "
        + "sharing, recursive public access, invalidation, snapshots and failed continuation");
  }
}
