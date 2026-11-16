package promotion_probe;

import com.google.protobuf.CodedInputStream;

public final class ExchangeCounts {
  public static int builders;
  public static int wrappers;
  public static int handoffs;
  public static int pendingTag;
  public static int pendingOffset;
  public static Object coldChild;

  public static void reset() {
    builders = wrappers = handoffs = pendingTag = pendingOffset = 0;
    coldChild = null;
  }

  public static void handoff(CodedInputStream input, int tag) {
    handoffs++;
    pendingTag = tag;
    pendingOffset = input.getTotalBytesRead();
  }
}
