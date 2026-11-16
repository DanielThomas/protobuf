import builder_probe.Leaf;
import builder_probe.RepeatedChoices;
import com.google.protobuf.ByteString;
import com.google.protobuf.CodedInputStream;
import com.google.protobuf.CodedOutputStream;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.ExtensionRegistry;
import com.google.protobuf.ExtensionRegistryLite;
import com.google.protobuf.GeneratedMessage;
import com.google.protobuf.Internal;
import com.google.protobuf.Message;
import com.google.protobuf.UnknownFieldSet;
import edition_shapes.Delimited;
import full_shapes.AllFields;
import full_shapes.CompatibilityParent;
import full_shapes.Extensible;
import full_shapes.Groups;
import full_shapes.MessageSet;
import full_shapes.RequiredChild;
import full_shapes.Rich;
import java.io.ByteArrayOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.List;
import old_dependency.NewParent;
import open_shapes.OpenRich;
import promotion_probe.ExchangeCounts;
import promotion_probe.Values;

public final class CoverageCases {
  private static final List<Message> TYPES = Arrays.<Message>asList(Rich.getDefaultInstance(),
      Groups.getDefaultInstance(), RequiredChild.getDefaultInstance(), OpenRich.getDefaultInstance(),
      Delimited.getDefaultInstance(), Values.getDefaultInstance(), Extensible.getDefaultInstance(),
      MessageSet.getDefaultInstance(), CompatibilityParent.getDefaultInstance(), NewParent.getDefaultInstance());

  static ExtensionRegistry registry() {
    ExtensionRegistry registry = ExtensionRegistry.newInstance();
    AllFields.registerAllExtensions(registry);
    return registry;
  }

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

  private static byte[] group(int field, byte[] payload) throws Exception {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    CodedOutputStream output = CodedOutputStream.newInstance(bytes);
    output.writeTag(field, 3);
    output.writeRawBytes(payload);
    output.writeTag(field, 4);
    output.flush();
    return bytes.toByteArray();
  }

  private static Message sample(Message.Builder builder, int seed, int depth) {
    for (FieldDescriptor field : builder.getDescriptorForType().getFields()) {
      if (field.getContainingOneof() != null) {
        List<FieldDescriptor> members = field.getContainingOneof().getFields();
        if (members.get(Math.floorMod(seed, members.size())) != field) continue;
      }
      if (field.getJavaType() == FieldDescriptor.JavaType.MESSAGE && depth == 0) continue;
      for (int i = 0; i < (field.isRepeated() ? 2 : 1); i++) {
        Object value;
        int n = seed + i + field.getNumber();
        switch (field.getJavaType()) {
          case MESSAGE: value = sample(builder.newBuilderForField(field), n, depth - 1); break;
          case ENUM: value = field.getEnumType().getValues().get(Math.floorMod(n, field.getEnumType().getValues().size())); break;
          case STRING: value = "text-" + n; break;
          case BYTE_STRING: value = ByteString.copyFrom(new byte[] {(byte) n, (byte) 255}); break;
          case BOOLEAN: value = n % 2 == 0; break;
          case LONG: value = (long) n; break;
          case FLOAT: value = (float) n; break;
          case DOUBLE: value = (double) n; break;
          default: value = n;
        }
        if (field.isRepeated()) builder.addRepeatedField(field, value);
        else builder.setField(field, value);
      }
    }
    return builder.buildPartial();
  }

  private static byte[][] wires(int type) throws Exception {
    byte[] first = wire(1, wire(1, 7, 2, 11));
    byte[] history = wire(1, wire(1, 7, 2, 11), 3, 9, 1, wire(2, 13));
    byte[] canonical = sample(TYPES.get(type).newBuilderForType(), type + 3, 1).toByteArray();
    if (type == 0) return new byte[][] {canonical,
        wire(11, first, 12, 999, 11, wire(3, 9), 11, wire(1, new byte[0])),
        wire(11, first, 12, 1, 11, wire(3, 9), 11, wire(1, new byte[0])),
        wire(6, wire(1, new byte[] {65}, 2, first, 2, wire(3, 9), 2, wire(1, new byte[0])),
            6, wire(1, new byte[] {66}, 2, history), 6, wire(1, new byte[] {65}, 2, wire(1, new byte[0]))),
        wire(4, new byte[] {0, (byte) 0xe7, 7, 1}, 7, wire(1, 7, 2, 999),
            7, wire(1, 7, 2, 1), 1, new byte[] {(byte) 255}, 2, new byte[] {(byte) 254})};
    if (type == 1) return new byte[][] {canonical,
        concat(group(1, wire(2, first)), group(1, wire(3, 9, 2, wire(1, new byte[0])))),
        concat(group(1, wire(2, history)), group(2, wire(1, first)), new byte[] {11, 20})};
    if (type == 2) return new byte[][] {canonical, new byte[0], wire(1, 7, 1, 11)};
    if (type == 3) return new byte[][] {canonical,
        wire(3, 999, 4, new byte[] {0, (byte) 0xe7, 7, 1}, 5, wire(1, new byte[] {65}, 2, 999)),
        wire(1, new byte[] {(byte) 255}, 10, new byte[] {(byte) 254})};
    if (type == 4) return new byte[][] {canonical,
        concat(group(4, group(2, wire(11, first))), wire(5, 9), group(4, group(2, wire(11, new byte[0])))),
        concat(group(1, group(2, wire(11, history))), new byte[] {11, 20})};
    if (type == 6) return new byte[][] {canonical, wire(100, 7, 101, first, 101, history, 102, new byte[] {0, 1, 5}),
        wire(101, first, 101, wire(3, 9), 101, wire(1, new byte[0]))};
    if (type == 7) return new byte[][] {group(1, wire(2, 100, 3, history)),
        concat(group(1, wire(2, 100, 3, first)), group(1, wire(3, wire(3, 9), 2, 100)))};
    if (type == 8) return new byte[][] {canonical,
        wire(2, wire(101, first), 3, 9, 2, wire(101, wire(3, 9)), 2, wire(101, wire(1, new byte[0])))};
    if (type == 9) return new byte[][] {canonical,
        wire(1, first, 2, 9, 1, wire(1, new byte[0])),
        wire(4, wire(1, new byte[] {65}, 2, first, 2, wire(3, 9), 2, wire(1, new byte[0]))),
        wire(3, history, 3, wire(1, new byte[0]), 5, history, 5, first)};
    return new byte[][] {canonical, wire(1, new byte[] {(byte) 255}, 2, new byte[] {(byte) 254},
        3, wire(1, 7, 2, 11), 3, wire(1, 7, 2, 13), 5, 19)};
  }

  static void corpus() throws Exception {
    for (int type = 0; type < TYPES.size(); type++) {
      byte[][] samples = wires(type);
      for (int sample = 0; sample < samples.length; sample++) {
        byte[] bytes = samples[sample];
        String name = "coverage-" + type + "-" + sample;
        for (int mode = 0; mode < 3; mode++) {
          for (int length = 0; length <= bytes.length; length++) {
            ChildExchangeTest.compare(name + "-" + mode + "-prefix-" + length, TYPES.get(type),
                Arrays.copyOf(bytes, length), mode, 100, -1, false);
          }
          for (int limit : new int[] {0, 1, 2, 4, 8}) {
            ChildExchangeTest.compare(name + "-" + mode + "-limit-" + limit,
                TYPES.get(type), bytes, mode, limit, -1, false);
          }
          ChildExchangeTest.compare(name + "-" + mode + "-discard", TYPES.get(type), bytes, mode, 100, -1, true);
          int tail = 0;
          for (byte[] suffix : new byte[][] {{0}, {12}, {(byte) 128}, {10, 3, 8}, {10, 1, 12}}) {
            ChildExchangeTest.compare(name + "-" + mode + "-tail-" + tail++, TYPES.get(type),
                concat(bytes, suffix), mode, 100, -1, false);
          }
        }
        for (int at = 0; at <= bytes.length; at++) {
          ChildExchangeTest.compare(name + "-io-" + at, TYPES.get(type), bytes, 1, 100, at, false);
        }
      }
    }
  }

  static void happy() throws Exception {
    int parses = 0;
    for (int seed = 0; seed < 48; seed++) {
      for (Message type : TYPES.subList(0, 6)) {
        Message message = sample(type.newBuilderForType(), seed, 2);
        byte[] bytes = message.toByteArray();
        for (int mode = 0; mode < 3; mode++) {
          ExchangeCounts.reset();
          Message parsed = (Message) type.getParserForType().parsePartialFrom(
              ChildExchangeTest.input(bytes, mode, -1), registry());
          check(ExchangeCounts.builders == 0 && ExchangeCounts.wrappers == 0,
              type.getDescriptorForType().getFullName() + " happy parse allocated "
                  + ExchangeCounts.builders + " Builders and " + ExchangeCounts.wrappers + " field wrappers");
          check(Arrays.equals(bytes, parsed.toByteArray()), "Happy field coverage changed bytes");
          parses++;
        }
      }
    }
    RepeatedChoices child = RepeatedChoices.newBuilder().setFirst(Leaf.newBuilder().setValue(7)).build();
    Rich unknownEnum = Rich.newBuilder().setSelected(child).setUnknownFields(
        UnknownFieldSet.newBuilder().addField(12, UnknownFieldSet.Field.newBuilder().addVarint(999).build()).build()).build();
    ExchangeCounts.reset();
    Rich parsed = Rich.parseFrom(unknownEnum.toByteArray());
    check(parsed.hasSelected() && parsed.getUnknownFields().hasField(12), "Unknown enum switched the active case");
    check(ExchangeCounts.builders == 0 && ExchangeCounts.wrappers == 0, "Unknown closed enum caused cold handoff");
    for (Message value : Arrays.<Message>asList(
        OpenRich.newBuilder().setKindValue(999).addKindsValue(999).putEnumsValue("A", 999)
            .setSelectedKindValue(999).build(),
        Rich.newBuilder().setTextBytes(ByteString.copyFrom(new byte[] {(byte) 255}))
            .addTextsBytes(ByteString.copyFrom(new byte[] {(byte) 254}))
            .setNameBytes(ByteString.copyFrom(new byte[] {(byte) 253})).build())) {
      byte[] bytes = value.toByteArray();
      ExchangeCounts.reset();
      Message decoded = (Message) value.getParserForType().parseFrom(bytes);
      check(ExchangeCounts.builders == 0 && ExchangeCounts.wrappers == 0,
          "Unknown open enum or raw proto2 string allocated a Builder/wrapper");
      check(Arrays.equals(bytes, decoded.toByteArray()), "Raw value happy parse changed bytes");
    }
    System.out.println("PASS: " + parses + " all-shape happy parses with zero Builders/wrappers; enum/raw-string witnesses");
  }

  private static void initializationAndCompatibility() throws Exception {
    for (Message defaults : Arrays.<Message>asList(
        RequiredChild.getDefaultInstance(), Rich.getDefaultInstance())) {
      byte[] bytes = defaults instanceof Rich ? wire(16, new byte[0]) : new byte[0];
      Message partial = (Message) defaults.getParserForType().parsePartialFrom(bytes);
      check(!partial.isInitialized(), "Partial parse hid missing required field");
      try {
        defaults.getParserForType().parseFrom(bytes);
        throw new AssertionError("Initialized parse accepted missing required field");
      } catch (com.google.protobuf.InvalidProtocolBufferException expected) {
        check(expected.getUnfinishedMessage().equals(partial), "Initialization failure changed partial message");
      }
    }
    byte[] bytes = wire(100, 7, 101, wire(1, wire(1, 11)), 102, new byte[] {0, 1, 5});
    Extensible known = Extensible.parseFrom(bytes, registry());
    Extensible unknown = Extensible.parseFrom(bytes);
    check(known.getExtension(AllFields.number) == 7 && known.getExtensionCount(AllFields.extensionKinds) == 3,
        "Registered extensions were not decoded");
    check(unknown.getAllFields().isEmpty() && unknown.getUnknownFields().hasField(101),
        "Unregistered extensions were not retained as unknowns");
    check(Extensible.parseFrom(unknown.toByteArray(), registry()).equals(known),
        "Unknown extension round trip changed registered values");
    byte[] item = group(1, wire(2, 100, 3, wire(1, wire(1, 17))));
    check(MessageSet.parseFrom(item, registry()).getExtension(AllFields.messageSetChild)
        .getFirst().getValue() == 17, "Registered MessageSet extension was not decoded");
    check(!MessageSet.parseFrom(item).getUnknownFields().asMap().isEmpty(),
        "Unregistered MessageSet item was lost");
    check(CompatibilityParent.parseFrom(wire(4, wire(1, 19))).getLegacy().getValue() == 19,
        "CODE_SIZE child fallback changed values");
  }

  private static Object field(Object owner, String name) throws Exception {
    Class<?> type = name.equals("unknownFieldsOrBuilder") ? GeneratedMessage.Builder.class : owner.getClass();
    Field field = type.getDeclaredField(name);
    field.setAccessible(true);
    return field.get(owner);
  }

  static void continuation() throws Exception {
    initializationAndCompatibility();
    byte[] first = wire(1, wire(1, 7, 2, 11));
    byte[] entry = wire(1, new byte[] {65}, 2, first, 2, wire(3, 9), 2, wire(1, new byte[0]));
    Rich switchedEnum = Rich.parseFrom(wire(11, first, 12, 1, 11, wire(3, 9), 11, wire(1, new byte[0])));
    check(switchedEnum.getSelected().getFirst().getValue() == 7,
        "Known closed-enum switch lost the previous child lifetime");
    NewParent legacy = NewParent.parseFrom(wire(1, first, 2, 9, 1, wire(1, new byte[0])));
    check(legacy.getSelected().getFirst().getValue() == 7,
        "Old generated child's actual continuation was not retained");
    legacy = NewParent.parseFrom(wire(4, wire(1, new byte[] {65}, 2, first,
        2, wire(3, 9), 2, wire(1, new byte[0]))));
    check(legacy.getChildrenOrThrow("A").getFirst().getValue() == 7,
        "Old generated map value lost intra-entry history");
    Rich map = Rich.parseFrom(wire(6, entry));
    check(map.getMessagesOrThrow("A").getFirst().getValue() == 7, "Map value lost intra-entry history");
    map = Rich.parseFrom(wire(6, entry, 6, wire(1, new byte[] {65}, 2, wire(1, new byte[0]))));
    check(map.getMessagesOrThrow("A").getFirst().getValue() == 0, "Separate map entries merged child history");
    Groups groups = Groups.parseFrom(concat(group(1, wire(2, first)),
        group(1, wire(3, 9, 2, wire(1, new byte[0])))));
    check(groups.getLegacy().getSelected().getFirst().getValue() == 0, "Ordinary group retained history across its public Parser boundary");

    byte[] prefix = wire(1, new byte[] {(byte) 255}, 2, new byte[] {(byte) 254}, 4, new byte[] {1},
        6, wire(1, new byte[] {65}, 2, first), 7, wire(1, 7, 2, 1), 9, first, 90, 17);
    Method parse = Rich.Builder.class.getDeclaredMethod("parseState_", CodedInputStream.class, ExtensionRegistryLite.class);
    parse.setAccessible(true);
    Rich.Builder owner = (Rich.Builder) parse.invoke(null,
        CodedInputStream.newInstance(concat(prefix, wire(9, new byte[0]))), registry());
    Object texts = field(owner, "texts_");
    Object kinds = field(owner, "kinds_");
    Object messages = field(owner, "messages_");
    Object enums = field(owner, "enums_");
    Object unknown = field(owner, "unknownFieldsOrBuilder");
    check(((Internal.ProtobufList<?>) texts).isModifiable() && ((Internal.ProtobufList<?>) kinds).isModifiable(),
        "Handoff froze repeated string/enum state");
    check(unknown instanceof UnknownFieldSet.Builder, "Handoff finalized unknowns");
    owner.mergeFrom(wire(2, new byte[] {65}, 4, new byte[] {0}, 6, wire(1, new byte[] {66}, 2, first),
        7, wire(1, 8, 2, 5), 91, 19));
    check(field(owner, "texts_") == texts && field(owner, "kinds_") == kinds
        && field(owner, "messages_") == messages && field(owner, "enums_") == enums
        && field(owner, "unknownFieldsOrBuilder") == unknown, "Continuation copied live collection/unknown state");
    Rich snapshot = owner.buildPartial();
    byte[] before = snapshot.toByteArray();
    Method restore = Rich.Builder.class.getDeclaredMethod("restoreState", Rich.class, boolean.class);
    restore.setAccessible(true);
    Rich.Builder restored = (Rich.Builder) restore.invoke(null, snapshot, false);
    Object restoredMap = field(restored, "messages_");
    check(field(restoredMap, "builderMap") == null, "Snapshot restoration eagerly copied message map");
    check(restored.getMessagesMap().get("A") == snapshot.getMessagesOrThrow("A"),
        "Snapshot restoration rebuilt message map values");
    check(field(restoredMap, "builderMap") == null, "Reading the immutable map view copied its backing state");
    restored.putMessages("later", RepeatedChoices.getDefaultInstance()).putEnums(99, full_shapes.Kind.ONE);
    check(field(restoredMap, "messageMap") == null && !snapshot.containsMessages("later")
        && !snapshot.containsEnums(99), "Restored maps did not copy on first mutation");
    check(Arrays.equals(before, snapshot.toByteArray()), "Restored map mutation changed old snapshot");
    owner.addTexts("later").addKinds(full_shapes.Kind.ONE).putStrings(3, "later");
    check(Arrays.equals(before, snapshot.toByteArray()), "Later mutation changed old snapshot");
    check(!snapshot.getTextBytes().isValidUtf8(), "Restoration converted invalid UTF-8");
    System.out.println("PASS: map entry lifetimes, group publication boundary, live strings/enums/maps/unknowns and snapshots");
  }
}
