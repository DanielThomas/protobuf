package builder_probe;

public final class ConstructorCounts {
  public static long builders;

  private ConstructorCounts() {}

  public static void created() {
    builders++;
  }
}
