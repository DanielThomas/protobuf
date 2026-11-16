package builder_probe;

import com.google.protobuf.CodedInputStream;
import com.google.protobuf.GeneratedMessage;
import com.google.protobuf.Internal;
import com.google.protobuf.UnknownFieldSet;
import java.lang.reflect.Field;

public final class ChildParserCounts {
  public static int ordinary;
  public static int raw;
  public static int handoffs;
  public static int offset;
  public static int tag;

  public static void reset() {
    ordinary = raw = handoffs = offset = tag = 0;
  }

  private static Object field(Object owner, Class<?> type, String name)
      throws ReflectiveOperationException {
    Field field = type.getDeclaredField(name);
    field.setAccessible(true);
    return field.get(owner);
  }

  public static boolean handoff(Object builder, CodedInputStream input, int pendingTag,
      Object values, UnknownFieldSet.Builder unknownFields) {
    handoffs++;
    offset = input.getTotalBytesRead();
    tag = pendingTag;
    checkState(builder, values, unknownFields, true);
    return values != null && ((Internal.ProtobufList<?>) values).isModifiable();
  }

  public static void checkState(Object builder, Object values, UnknownFieldSet.Builder unknownFields,
      boolean retainValues) {
    try {
      if (values != null && retainValues) {
        Internal.ProtobufList<?> list = (Internal.ProtobufList<?>) values;
        if (field(builder, builder.getClass(), "values_") != values
            || (!list.isEmpty() && !list.isModifiable())) {
          throw new AssertionError("Copied or froze live repeated storage at handoff");
        }
      }
      if (unknownFields != null && field(builder, GeneratedMessage.Builder.class,
          "unknownFieldsOrBuilder") != unknownFields) {
        throw new AssertionError("Published or copied live unknown fields at handoff");
      }
    } catch (ReflectiveOperationException failure) {
      throw new AssertionError(failure);
    }
  }
}
