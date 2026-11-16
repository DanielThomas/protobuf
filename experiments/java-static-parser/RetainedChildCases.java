import builder_probe.Leaf;
import builder_probe.ManyChoices;
import builder_probe.Multi;
import builder_probe.MutualA;
import builder_probe.MutualB;
import builder_probe.RepeatedChoices;
import builder_probe.ScalarChoice;
import builder_probe.WideChoices;
import com.google.protobuf.ByteString;
import com.google.protobuf.CodedInputStream;
import com.google.protobuf.CodedOutputStream;
import com.google.protobuf.ExtensionRegistryLite;
import com.google.protobuf.Internal;
import com.google.protobuf.Message;
import com.google.protobuf.SingleFieldBuilder;
import java.io.ByteArrayOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.List;

public final class RetainedChildCases {
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

  static byte[][] wires(int type) throws Exception {
    byte[] leaf = wire(1, 7, 2, 11, 2, 13, 90, 17);
    byte[] history = wire(1, leaf, 3, 19, 1, wire(2, 23), 90, 31);
    switch (type) {
      case 0:
        return new byte[][] {wire(1, 7), wire(2, new byte[] {1, 2, 3}),
            wire(1, 7, 1, 9, 2, new byte[0], 1, 11)};
      case 1:
        return new byte[][] {wire(1, history, 3, history, 5, 7, 5, 11),
            wire(1, wire(1, leaf), 3, history, 2, 17, 1, wire(1, new byte[0])),
            wire(1, history, 3, history, 1, wire(1, wire(2, 37)), 3, wire(3, 41))};
      case 2:
        return new byte[][] {wire(1, leaf, 32, leaf),
            wire(1, leaf, 32, leaf, 1, wire(2, 19), 32, wire(2, 23)), wire(16, leaf, 16, new byte[0])};
      case 3:
        return new byte[][] {wire(33, leaf), wire(1, leaf, 34, 19, 33, leaf, 1, new byte[0]),
            wire(32, leaf, 33, leaf, 34, 19, 32, wire(2, 23))};
      case 4:
        return new byte[][] {wire(1, wire(1, wire(2, history))),
            wire(1, wire(1, wire(2, history)), 3, 7, 1, wire(1, wire(2, new byte[0])))};
      case 5:
        return new byte[][] {wire(1, wire(1, wire(1, wire(2, history)))),
            wire(1, wire(2, history), 2, 7, 1, wire(2, new byte[0]))};
      default:
        throw new AssertionError(type);
    }
  }

  static List<Message> happy(int seed) {
    Leaf leaf = Leaf.newBuilder().setValue(seed).addValues(seed + 1).build();
    RepeatedChoices choices = RepeatedChoices.newBuilder().setFirst(leaf).build();
    ScalarChoice scalar = seed % 2 == 0 ? ScalarChoice.newBuilder().setValue(seed).build()
        : ScalarChoice.newBuilder().setData(ByteString.copyFromUtf8("data-" + seed)).build();
    Multi multi = Multi.newBuilder().setFirst(choices).setSecond(choices).addValues(seed).build();
    ManyChoices.Builder many = ManyChoices.newBuilder();
    for (int i = 1; i <= 32; i++) many.setField(many.getDescriptorForType().findFieldByNumber(i), leaf);
    WideChoices.Builder wide = WideChoices.newBuilder();
    wide.setField(wide.getDescriptorForType().findFieldByNumber(seed % 34 + 1),
        seed % 34 == 33 ? Integer.valueOf(seed) : leaf);
    MutualA a = MutualA.newBuilder().setLeaf(choices).build();
    MutualB b = MutualB.newBuilder().setChild(a).build();
    for (int depth = 0; depth < seed % 8; depth++) {
      a = MutualA.newBuilder().setChild(b).build();
      b = MutualB.newBuilder().setChild(a).build();
    }
    return Arrays.<Message>asList(scalar, multi, many.build(), wide.build(), a, b);
  }

  private static Object field(Object owner, String name) throws Exception {
    Field field = owner.getClass().getDeclaredField(name);
    field.setAccessible(true);
    return field.get(owner);
  }

  private static void check(boolean condition, String message) {
    if (!condition) throw new AssertionError(message);
  }

  static void continuation() throws Exception {
    byte[] leaf = wire(1, 7);
    promotion_probe.ExchangeCounts.reset();
    ManyChoices clean = ManyChoices.parseFrom(wire(1, leaf, 1, 9, 90, new byte[0], 32, leaf));
    check(clean.getChild1().getValue() == 7 && clean.getChild32().getValue() == 7,
        "Shared oneof guard consumed an unrelated field");
    check(clean.getUnknownFields().getField(1).getVarintList().equals(Arrays.asList(9L))
        && clean.getUnknownFields().getField(90).getLengthDelimitedList().size() == 1,
        "Shared oneof guard lost wrong-wire or unknown fields");
    check(promotion_probe.ExchangeCounts.builders == 0
        && promotion_probe.ExchangeCounts.wrappers == 0
        && promotion_probe.ExchangeCounts.handoffs == 0,
        "Shared oneof guard handed off for wrong-wire, unknown or unrelated fields");
    byte[] history = wire(1, wire(2, 7), 3, 9, 1, wire(2, 11), 90, 13);
    CodedInputStream input = CodedInputStream.newInstance(wire(1, history, 3, history, 5, 17));
    Method parse = Multi.Builder.class.getDeclaredMethod("parseState_", CodedInputStream.class,
        ExtensionRegistryLite.class);
    parse.setAccessible(true);
    Object state = parse.invoke(null, input, ExtensionRegistryLite.getEmptyRegistry());
    check(state instanceof Multi.Builder, "EOF published two live child continuations");
    Multi.Builder owner = (Multi.Builder) state;
    check(input.isAtEnd(), "EOF handoff did not finish the input");
    SingleFieldBuilder<?, ?, ?> first = (SingleFieldBuilder<?, ?, ?>) field(owner, "firstBuilder_");
    SingleFieldBuilder<?, ?, ?> second = (SingleFieldBuilder<?, ?, ?>) field(owner, "secondBuilder_");
    Leaf.Builder left = ((RepeatedChoices.Builder) first.getBuilder()).getFirstBuilder();
    Leaf.Builder right = ((RepeatedChoices.Builder) second.getBuilder()).getFirstBuilder();
    Object leftValues = field(left, "values_");
    Object rightValues = field(right, "values_");
    Object parentValues = field(owner, "values_");
    for (Object values : new Object[] {leftValues, rightValues, parentValues}) {
      check(((Internal.ProtobufList<?>) values).isModifiable(), "EOF handoff froze live repeated storage");
    }
    owner.mergeFrom(wire(1, wire(1, wire(2, 19)), 3, wire(1, wire(2, 23)), 5, 29));
    check(field(left, "values_") == leftValues && field(right, "values_") == rightValues
        && field(owner, "values_") == parentValues, "Later merge copied live repeated storage");
    check(left.getValuesCount() == 3 && right.getValuesCount() == 3 && owner.getValuesCount() == 2,
        "Multiple child handoff lost values");
    Multi snapshot = owner.buildPartial();
    byte[] before = snapshot.toByteArray();
    left.addValues(31);
    right.addValues(37);
    check(owner.buildPartial().getFirst().getFirst().getValuesCount() == 4
        && owner.buildPartial().getSecond().getFirst().getValuesCount() == 4,
        "A retained child failed to invalidate the enclosing snapshot");
    check(Arrays.equals(before, snapshot.toByteArray()), "Later child mutation changed an old snapshot");
    System.out.println("PASS: multiple live children at EOF, identity transfer and ancestor invalidation");
  }
}
