// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of the
// License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "flare/net/hbase/message.h"

#include <initializer_list>
#include <utility>

#include "thirdparty/protobuf/util/delimited_message_util.h"

#include "flare/base/buffer.h"
#include "flare/base/buffer/zero_copy_stream.h"
#include "flare/base/endian.h"

namespace flare::hbase {

namespace {

// It's explicitly allowed some elements in `msgs` to be null.
void WriteTo(std::initializer_list<const google::protobuf::Message*> msgs,
             NoncontiguousBuffer cell_block,
             NoncontiguousBufferBuilder* builder) {
  auto size_ptr = builder->Reserve(4);  // Filled later.
  std::size_t size_was = builder->ByteSize();

  {
    NoncontiguousBufferOutputStream nbos(builder);
    for (auto&& m : msgs) {
      if (m) {
        FLARE_LOG_FATAL_IF(
            !google::protobuf::util::SerializeDelimitedToZeroCopyStream(*m,
                                                                        &nbos),
            "Cannot serialize message.");
      }
    }
  }

  builder->Append(std::move(cell_block));

  // Now the serialized size is known.
  std::uint32_t size =
      ToBigEndian<std::uint32_t>(builder->ByteSize() - size_was);
  memcpy(size_ptr, &size, sizeof(size));  // `memcpy` to suppress "unaligned
                                          // write" warning.
}

// Cut off a message buffer.
//
// Returns: Same as `HbaseXxx::TryCut`.
template <class T>
std::optional<bool> ParseHbaseBuffer(NoncontiguousBuffer* buffer, T* message,
                                     NoncontiguousBuffer* rest) {
  std::uint32_t size;
  if (buffer->ByteSize() < sizeof(size)) {
    return std::nullopt;
  }
  FlattenToSlow(*buffer, &size, sizeof(size));
  FromBigEndian<std::uint32_t>(&size);
  if (buffer->ByteSize() < size) {
    return std::nullopt;
  }
  buffer->Skip(sizeof(size));

  auto msg_cut = buffer->Cut(size);
  {
    NoncontiguousBufferInputStream nbis(&msg_cut);
    if (!google::protobuf::util::ParseDelimitedFromZeroCopyStream(
            message, &nbis, nullptr)) {
      FLARE_LOG_WARNING_EVERY_SECOND("Cannot parse message header.");
      return false;
    }
  }
  *rest = std::move(msg_cut);
  return true;
}

bool ParseHbaseBody(NoncontiguousBuffer* buffer,
                    std::size_t expected_cell_block_size, MessageIoBuffer* body,
                    NoncontiguousBuffer* cell_block) {
  FLARE_CHECK_EQ(body->index(), 0);  // Input buffer.
  {
    NoncontiguousBufferInputStream nbis(buffer);
    if (!google::protobuf::util::ParseDelimitedFromZeroCopyStream(
            std::get<0>(*body).Get(), &nbis, nullptr)) {
      FLARE_LOG_WARNING_EVERY_SECOND("Cannot parse message body.");
      return false;
    }
  }
  if (buffer->ByteSize() != expected_cell_block_size) {
    FLARE_LOG_WARNING_EVERY_SECOND("Cell-block size mismatch.");
    return false;
  }
  *cell_block = std::move(*buffer);
  return true;
}

}  // namespace

std::optional<bool> HbaseRequest::TryCut(NoncontiguousBuffer* buffer) {
  return ParseHbaseBuffer(buffer, &header, &rest_bytes_);
}

bool HbaseRequest::TryParse() {
  auto cell_block_size = header.cell_block_meta().length();
  if (!header.request_param()) {
    cell_block = std::move(rest_bytes_);
    if (cell_block.ByteSize() != cell_block_size) {
      FLARE_LOG_WARNING_EVERY_SECOND("Cell-block size mismatch.");
      return false;
    }
  }
  return ParseHbaseBody(&rest_bytes_, cell_block_size, &body, &cell_block);
}

void HbaseRequest::WriteTo(NoncontiguousBufferBuilder* builder) const {
  FLARE_CHECK_EQ(body.index(), 1);  // Output buffer.
  // FIXME: Can we move `cell_block` into `WriteTo`?
  hbase::WriteTo({&header, std::get<1>(body)}, cell_block, builder);
}

std::optional<bool> HbaseResponse::TryCut(NoncontiguousBuffer* buffer) {
  return ParseHbaseBuffer(buffer, &header, &rest_bytes_);
}

bool HbaseResponse::TryParse() {
  FLARE_CHECK_EQ(body.index(), 0);
  if (header.has_exception()) {
    // For error response, there's nothing more to parse.
    if (!rest_bytes_.Empty()) {
      // If not, there's a protocol error.
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Unexpected: Data follows an exception response.");
      return false;
    }
    if (header.exception().exception_class_name().empty()) {
      FLARE_LOG_WARNING_EVERY_SECOND("Unexpected: Empty exception class name.");
    }
    return true;  // Nothing more to parse.
  }
  return ParseHbaseBody(&rest_bytes_, header.cell_block_meta().length(), &body,
                        &cell_block);
}

void HbaseResponse::WriteTo(NoncontiguousBufferBuilder* builder) const {
  const google::protobuf::Message* resp_body = nullptr;
  if (body.index() == 0) {
    FLARE_CHECK(!std::get<0>(body));  // Not initialized at all.
  } else {
    FLARE_CHECK_EQ(body.index(), 1);  // It must be an output buffer otherwise.
    resp_body = std::get<1>(body);
  }
  hbase::WriteTo({&header, resp_body}, cell_block, builder);
}

}  // namespace flare::hbase
