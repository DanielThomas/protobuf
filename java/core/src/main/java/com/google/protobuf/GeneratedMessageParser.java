// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package com.google.protobuf;

import java.io.IOException;

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
  @Override
  public MessageType parseFrom(byte[] data) throws InvalidProtocolBufferException {
    // Skip forwarding overloads to leave more inline depth for the decoding loop.
    return checkMessageInitialized(parsePartialFrom(data, 0, data.length, EMPTY_REGISTRY));
  }

  /**
   * Continues parsing with a builder after a field tag has been read.
   *
   * <p>The builder must contain the fields parsed so far, with child builders for any active
   * message-valued oneofs. The pending field is merged before entering the generated builder's
   * parsing loop.
   *
   * @param <T> the generated message type
   * @param builder the builder containing the fields parsed so far
   * @param input the input positioned after the pending tag
   * @param registry the extension registry
   * @param tag the pending field tag
   * @param child the builder for the singular message or group identified by {@code tag}, or
   *     {@code null} to look up and merge the pending field
   * @return the parsed message, without checking required fields
   * @throws InvalidProtocolBufferException if parsing fails
   */
  @SuppressWarnings("unchecked")
  protected static <T extends GeneratedMessage> T mergeFrom(
      GeneratedMessage.Builder<?> builder,
      CodedInputStream input,
      ExtensionRegistryLite registry,
      int tag,
      Message.Builder child)
      throws InvalidProtocolBufferException {
    try {
      boolean more = true;
      if (child != null) {
        if (WireFormat.getTagWireType(tag) == WireFormat.WIRETYPE_START_GROUP) {
          input.readGroup(tag >>> 3, child, registry);
        } else {
          input.readMessage(child, registry);
        }
        if (input.isAtEnd()) {
          // A group leaves its closing tag in lastTag; finish with tag zero.
          input.readTag();
          more = false;
        }
      } else {
        Descriptors.Descriptor descriptor = builder.getDescriptorForType();
        Descriptors.FieldDescriptor field = descriptor.findFieldByNumber(tag >>> 3);
        if (field == null) {
          more = builder.parseUnknownField(input, registry, tag);
        } else {
          int wireType = WireFormat.getTagWireType(tag);
          boolean readingMessage =
              field.getJavaType() == Descriptors.FieldDescriptor.JavaType.MESSAGE
                  && wireType == FieldSet.getWireFormatForFieldType(field.getLiteType(), false);
          boolean readingClosedEnum =
              field.getJavaType() == Descriptors.FieldDescriptor.JavaType.ENUM
                  && field.legacyEnumFieldTreatedAsClosed()
                  && (wireType == WireFormat.WIRETYPE_VARINT
                      || (field.isPackable() && wireType == WireFormat.WIRETYPE_LENGTH_DELIMITED));
          more =
              MessageReflection.mergeFieldFrom(
                  input,
                  readingMessage || (input.shouldDiscardUnknownFields() && !readingClosedEnum)
                      ? null
                      : builder.getUnknownFieldSetBuilder(),
                  registry,
                  descriptor,
                  new MessageReflection.BuilderAdapter(builder),
                  tag);
        }
      }
      if (more) {
        builder.mergeFrom(input, registry);
      }
    } catch (IOException failure) {
      throwParseException(failure, builder.buildPartial());
    }
    return (T) builder.buildPartial();
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
   * Parses an unknown field of a non-extendable message.
   *
   * @param input the input stream
   * @param unknown the unknown-field builder, or {@code null} if unknown fields are discarded
   * @param tag the tag already read from {@code input}
   * @return {@code false} if {@code tag} is an end-group tag, otherwise {@code true}
   * @throws IOException if an I/O error occurs or the input is malformed
   */
  protected static boolean parseUnknownField(CodedInputStream input, Object unknown, int tag)
      throws IOException {
    return input.shouldDiscardUnknownFields()
        ? input.skipField(tag)
        : ((UnknownFieldSet.Builder) unknown).mergeFieldFrom(tag, input);
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
}
