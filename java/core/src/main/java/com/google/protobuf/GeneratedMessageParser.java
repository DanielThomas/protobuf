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
   * Creates a builder by directly transferring the fields of a parsed message.
   *
   * @param message the parsed message whose decoding will continue
   * @return a builder initialized by the generated state constructor
   */
  protected abstract GeneratedMessage.Builder<?> newBuilderForParsing(MessageType message);

  @SuppressWarnings("unchecked")
  static MessageLite.Builder toBuilderForParsing(MessageLiteOrBuilder value) {
    if (value instanceof MessageLite.Builder) {
      return (MessageLite.Builder) value;
    }
    MessageLite message = (MessageLite) value;
    Parser<?> parser = message.getParserForType();
    if (parser instanceof GeneratedMessageParser) {
      return ((GeneratedMessageParser<GeneratedMessage>) parser)
          .newBuilderForParsing((GeneratedMessage) message);
    }
    return message.toBuilder();
  }

  protected final Internal.IntList readPackedInt32(CodedInputStream input) throws IOException {
    return input.readPackedInt32();
  }

  protected final Internal.LongList readPackedInt64(CodedInputStream input) throws IOException {
    return input.readPackedInt64();
  }

  protected final Internal.BooleanList readPackedBool(CodedInputStream input) throws IOException {
    return input.readPackedBool();
  }

  protected final Internal.FloatList readPackedFloat(CodedInputStream input) throws IOException {
    return input.readPackedFloat();
  }

  protected final Internal.DoubleList readPackedDouble(CodedInputStream input) throws IOException {
    return input.readPackedDouble();
  }

  private static UnknownFieldSet.Builder getUnknownFieldSetBuilder(UnknownFieldSet.Builder unknown) {
    return unknown == null ? UnknownFieldSet.newBuilder() : unknown;
  }

  /**
   * Prepares a builder for reading unknown fields.
   *
   * @param unknown the unknown-field builder, or {@code null} if none was created
   * @param input the input stream whose unknown-field discard setting is used
   * @return an unknown-field builder, or {@code unknown} if unknown fields are being discarded
   */
  protected static UnknownFieldSet.Builder prepareUnknownFields(
      UnknownFieldSet.Builder unknown, CodedInputStream input) {
    return input.shouldDiscardUnknownFields() ? unknown : getUnknownFieldSetBuilder(unknown);
  }

  /**
   * Adds a varint to the unknown fields.
   *
   * @param unknown the unknown-field builder, or {@code null} if none was created
   * @param number the field number
   * @param value the varint value
   * @return the builder containing the added value
   */
  protected static UnknownFieldSet.Builder mergeUnknownVarintField(
      UnknownFieldSet.Builder unknown, int number, int value) {
    UnknownFieldSet.Builder builder = getUnknownFieldSetBuilder(unknown);
    builder.mergeVarintField(number, value);
    return builder;
  }

  /**
   * Adds a length-delimited value to the unknown fields.
   *
   * @param unknown the unknown-field builder, or {@code null} if none was created
   * @param number the field number
   * @param value the field's bytes
   * @return the builder containing the added value
   */
  protected static UnknownFieldSet.Builder mergeUnknownLengthDelimitedField(
      UnknownFieldSet.Builder unknown, int number, ByteString value) {
    UnknownFieldSet.Builder builder = getUnknownFieldSetBuilder(unknown);
    builder.mergeLengthDelimitedField(number, value);
    return builder;
  }

  /**
   * Parses an unknown field of a non-extendable message.
   *
   * @param input the input stream
   * @param unknown the prepared unknown-field builder; may be {@code null} when discarding
   * @param tag the tag already read from {@code input}
   * @return {@code false} if {@code tag} is an end-group tag, otherwise {@code true}
   * @throws IOException if an I/O error occurs or the input is malformed
   */
  protected static boolean parseUnknownField(
      CodedInputStream input, UnknownFieldSet.Builder unknown, int tag) throws IOException {
    return GeneratedMessage.parseUnknownField(input, unknown, tag);
  }

  /**
   * Stores the parsed unknown fields in the message.
   *
   * @param instance the message being parsed
   * @param unknown the unknown-field builder, or {@code null} if none was created
   */
  protected static void finishUnknownFields(
      GeneratedMessage instance, UnknownFieldSet.Builder unknown) {
    if (unknown != null) {
      instance.unknownFields = unknown.build();
    }
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
