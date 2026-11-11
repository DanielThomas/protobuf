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
 * <p>Static helpers are called by parsing loops generated in the message classes.
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

  public static Internal.IntList readPackedInt32(CodedInputStream input) throws IOException {
    return input.readPackedInt32();
  }

  public static Internal.LongList readPackedInt64(CodedInputStream input) throws IOException {
    return input.readPackedInt64();
  }

  public static Internal.BooleanList readPackedBool(CodedInputStream input) throws IOException {
    return input.readPackedBool();
  }

  public static Internal.FloatList readPackedFloat(CodedInputStream input) throws IOException {
    return input.readPackedFloat();
  }

  public static Internal.DoubleList readPackedDouble(CodedInputStream input) throws IOException {
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
  public static UnknownFieldSet.Builder prepareUnknownFields(
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
  public static UnknownFieldSet.Builder mergeUnknownVarintField(
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
  public static UnknownFieldSet.Builder mergeUnknownLengthDelimitedField(
      UnknownFieldSet.Builder unknown, int number, ByteString value) {
    UnknownFieldSet.Builder builder = getUnknownFieldSetBuilder(unknown);
    builder.mergeLengthDelimitedField(number, value);
    return builder;
  }
}
