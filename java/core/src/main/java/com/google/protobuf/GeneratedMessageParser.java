// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/**
 * Parsing support for generated full-runtime messages.
 *
 * <p>Helpers are inherited by generated parser singletons so javac does not need to emit protected
 * access bridges in each generated message class.
 *
 * @param <MessageType> the type of message parsed
 */
public abstract class GeneratedMessageParser<MessageType extends GeneratedMessage>
    extends AbstractParser<MessageType> {
  /**
   * Returns the map used to cache child builders, creating it if necessary.
   *
   * <p>The caller must store the returned map before calling {@link #readChild} so that partially
   * merged children can be recovered if parsing fails.
   *
   * @param children the current map, or {@code null} if none has been created
   * @return {@code children} if non-null, otherwise a new empty map
   */
  @SuppressWarnings("rawtypes")
  protected static Map<Integer, SingleFieldBuilder> childBuilders(
      Map<Integer, SingleFieldBuilder> children) {
    return children == null ? new HashMap<>() : children;
  }

  /**
   * Returns the cached builder for a field, creating it from the field value if necessary.
   *
   * @param children the map of child builders
   * @param field the field number
   * @param value the message used to initialize a new builder
   * @return the builder associated with {@code field}
   */
  @SuppressWarnings({"rawtypes", "unchecked"})
  protected static SingleFieldBuilder childBuilder(
      Map<Integer, SingleFieldBuilder> children, int field, GeneratedMessage value) {
    SingleFieldBuilder child = children.get(field);
    if (child == null) {
      child = new SingleFieldBuilder(value, null, false);
      children.put(field, child);
    }
    return child;
  }

  /**
   * Reads a singular message field and merges it with any previous value.
   *
   * <p>Duplicate occurrences are merged in a builder stored in {@code children}. Call {@link
   * #buildChild} after parsing the enclosing message to obtain the merged child.
   *
   * @param children the child builders, non-null when {@code value} is non-null
   * @param field the field number
   * @param value the previous field value, or {@code null} if absent
   * @param defaultValue the default instance of the field's message type
   * @param input the input stream
   * @param registry the extension registry
   * @param group {@code true} if the field uses the group wire format
   * @return the parsed message, or {@code value} if a builder was used
   * @throws IOException if an I/O error occurs or the input is malformed
   */
  @SuppressWarnings("rawtypes")
  protected static GeneratedMessage readChild(
      Map<Integer, SingleFieldBuilder> children,
      int field,
      GeneratedMessage value,
      GeneratedMessage defaultValue,
      CodedInputStream input,
      ExtensionRegistryLite registry,
      boolean group)
      throws IOException {
    SingleFieldBuilder child;
    if (value == null) {
      child = children == null ? null : children.get(field);
    } else {
      child = childBuilder(children, field, value);
    }
    if (child == null) {
      return group
          ? input.readGroup(field, defaultValue.getParserForType(), registry)
          : input.readMessage(defaultValue.getParserForType(), registry);
    }
    if (group) {
      input.readGroup(field, child.getBuilder(), registry);
    } else {
      input.readMessage(child.getBuilder(), registry);
    }
    return value;
  }

  /**
   * Returns the child builder's message, if a builder exists for the field.
   *
   * @param children the child builders, or {@code null} if none were created
   * @param field the field number
   * @param value the current field value
   * @return the built child message, or {@code value} if no builder exists
   */
  @SuppressWarnings("rawtypes")
  protected static Object buildChild(
      Map<Integer, SingleFieldBuilder> children, int field, Object value) {
    SingleFieldBuilder child = children == null ? null : children.get(field);
    return child == null ? value : child.build();
  }

  private static UnknownFieldSet.Builder getUnknownFieldSetBuilder(Object unknown) {
    if (unknown instanceof UnknownFieldSet.Builder) {
      return (UnknownFieldSet.Builder) unknown;
    }
    return unknown == null
        ? UnknownFieldSet.newBuilder()
        : ((UnknownFieldSet) unknown).toBuilder();
  }

  /**
   * Prepares a builder for reading unknown fields.
   *
   * @param unknown the current {@link UnknownFieldSet}, its builder, or {@code null}
   * @param input the input stream whose unknown-field discard setting is used
   * @return an unknown-field builder, or {@code unknown} if unknown fields are being discarded
   */
  protected static Object prepareUnknownFields(Object unknown, CodedInputStream input) {
    return input.shouldDiscardUnknownFields() ? unknown : getUnknownFieldSetBuilder(unknown);
  }

  /**
   * Adds a varint to the unknown fields.
   *
   * @param unknown the current {@link UnknownFieldSet}, its builder, or {@code null}
   * @param number the field number
   * @param value the varint value
   * @return the builder containing the added value
   */
  protected static UnknownFieldSet.Builder mergeUnknownVarintField(
      Object unknown, int number, int value) {
    UnknownFieldSet.Builder builder = getUnknownFieldSetBuilder(unknown);
    builder.mergeVarintField(number, value);
    return builder;
  }

  /**
   * Adds a length-delimited value to the unknown fields.
   *
   * @param unknown the current {@link UnknownFieldSet}, its builder, or {@code null}
   * @param number the field number
   * @param value the field's bytes
   * @return the builder containing the added value
   */
  protected static UnknownFieldSet.Builder mergeUnknownLengthDelimitedField(
      Object unknown, int number, ByteString value) {
    UnknownFieldSet.Builder builder = getUnknownFieldSetBuilder(unknown);
    builder.mergeLengthDelimitedField(number, value);
    return builder;
  }

  /**
   * Parses an unknown field.
   *
   * @param instance the message being parsed
   * @param input the input stream
   * @param unknown the unknown-field builder, or {@code null} if unknown fields are discarded
   * @param extensionRegistry the extension registry
   * @param tag the tag already read from {@code input}
   * @return {@code false} if {@code tag} is an end-group tag, otherwise {@code true}
   * @throws IOException if an I/O error occurs or the input is malformed
   */
  protected static boolean parseUnknownField(
      GeneratedMessage instance,
      CodedInputStream input,
      Object unknown,
      ExtensionRegistryLite extensionRegistry,
      int tag)
      throws IOException {
    return instance.parseUnknownField(
        input,
        input.shouldDiscardUnknownFields() ? null : (UnknownFieldSet.Builder) unknown,
        extensionRegistry,
        tag);
  }

  /**
   * Returns the builder used to parse extensions, creating it if necessary.
   *
   * @param instance the message being parsed
   * @param extensions the current extension builder, or {@code null} if none has been created
   * @return {@code extensions} if non-null, otherwise a new extension builder for {@code instance}
   */
  protected static Object prepareExtensions(
      GeneratedMessage.ExtendableMessage<?> instance, Object extensions) {
    return extensions == null ? instance.newExtensionBuilderForParsing() : extensions;
  }

  /**
   * Parses an extension or unknown field.
   *
   * @param instance the message being parsed
   * @param input the input stream
   * @param unknown the unknown-field builder, or {@code null} if unknown fields are discarded
   * @param extensionRegistry the extension registry
   * @param tag the tag already read from {@code input}
   * @param extensions the extension builder returned by {@link #prepareExtensions}
   * @return {@code false} if {@code tag} is an end-group tag, otherwise {@code true}
   * @throws IOException if an I/O error occurs or the input is malformed
   */
  @SuppressWarnings("unchecked")
  protected static boolean parseUnknownField(
      GeneratedMessage.ExtendableMessage<?> instance,
      CodedInputStream input,
      Object unknown,
      ExtensionRegistryLite extensionRegistry,
      int tag,
      Object extensions)
      throws IOException {
    return MessageReflection.mergeFieldFrom(
        input,
        input.shouldDiscardUnknownFields() ? null : (UnknownFieldSet.Builder) unknown,
        extensionRegistry,
        instance.getDescriptorForType(),
        new MessageReflection.ExtensionBuilderAdapter(
            (FieldSet.Builder<Descriptors.FieldDescriptor>) extensions),
        tag);
  }

  /**
   * Stores the parsed unknown fields in the message.
   *
   * @param instance the message being parsed
   * @param unknown the unknown-field builder, or {@code null} if none was created
   */
  protected static void finishUnknownFields(GeneratedMessage instance, Object unknown) {
    if (unknown != null) {
      instance.unknownFields = ((UnknownFieldSet.Builder) unknown).build();
    }
  }

  /**
   * Throws a parse exception containing the unfinished message.
   *
   * @param e the parsing failure
   * @param message the unfinished message
   * @throws InvalidProtocolBufferException the supplied exception if it is an {@code
   *     InvalidProtocolBufferException}, otherwise a new exception wrapping {@code e}
   */
  protected static void throwParseException(IOException e, MessageLite message)
      throws InvalidProtocolBufferException {
    if (e instanceof InvalidProtocolBufferException) {
      throw ((InvalidProtocolBufferException) e).setUnfinishedMessage(message);
    }
    throw new InvalidProtocolBufferException(e).setUnfinishedMessage(message);
  }

  /**
   * Returns a mutable copy of a protobuf list.
   *
   * @param <ListT> the list type
   * @param list the list to copy
   * @return a mutable list containing the same elements
   */
  protected static <ListT extends Internal.ProtobufList<?>> ListT makeMutableCopy(ListT list) {
    return GeneratedMessage.makeMutableCopy(list);
  }

  /**
   * Returns a mutable copy of a protobuf list with at least the requested capacity.
   *
   * @param <ListT> the list type
   * @param list the list to copy
   * @param minCapacity the minimum capacity of the copy
   * @return a mutable list containing the same elements
   */
  protected static <ListT extends Internal.ProtobufList<?>> ListT makeMutableCopy(
      ListT list, int minCapacity) {
    return GeneratedMessage.makeMutableCopy(list, minCapacity);
  }

  /**
   * Stores the parsed extensions in the message and makes them immutable.
   *
   * @param instance the message being parsed
   * @param extensions the extension builder, or {@code null} if no extensions were parsed
   */
  @SuppressWarnings("unchecked")
  protected static void finishParsingExtensions(
      GeneratedMessage.ExtendableMessage<?> instance, Object extensions) {
    instance.finishParsingExtensions((FieldSet.Builder<Descriptors.FieldDescriptor>) extensions);
  }
}
