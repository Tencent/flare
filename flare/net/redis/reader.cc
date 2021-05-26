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

#include "flare/net/redis/reader.h"

#include <cinttypes>
#include <string>
#include <utility>
#include <vector>

#include "flare/base/string.h"

namespace flare::redis {

int TryCutRedisObject(NoncontiguousBuffer* buffer, RedisObject* object) {
  if (buffer->Empty()) {
    return 0;  // What to cut then?
  }

  FLARE_CHECK_NE(buffer->FirstContiguous().size(), 0);  // Bug otherwise.
  auto type = buffer->FirstContiguous().data()[0];

  // https://redis.io/topics/protocol:
  //
  // > For Simple Strings the first byte of the reply is "+"
  // > For Errors the first byte of the reply is "-"
  // > For Integers the first byte of the reply is ":"
  // > For Bulk Strings the first byte of the reply is "$"
  // > For Arrays the first byte of the reply is "*"

  if (type == '+') {
    // > Simple Strings are encoded in the following way: a plus character,
    // > followed by a string that cannot contain a CR or LF character (no
    // > newlines are allowed), terminated by CRLF (that is "\r\n"). [...]
    // >
    // > "+OK\r\n"
    auto str = FlattenSlowUntil(*buffer, "\r\n");
    if (!EndsWith(str, "\r\n")) {
      return 0;  // More data needed.
    }

    buffer->Skip(str.size());
    *object = str.substr(1, str.size() - 3);
    return 1;
  } else if (type == '-') {
    // > The following are examples of error replies:
    // >
    // > -ERR unknown command 'foobar'
    // > -WRONGTYPE Operation against a key holding the wrong kind of value
    // >
    // > The first word after the "-", up to the first space or newline,
    // > represents the kind of error returned. This is just a convention used
    // > by Redis and is not part of the RESP Error format.
    auto str = FlattenSlowUntil(*buffer, "\r\n");
    if (!EndsWith(str, "\r\n")) {
      return 0;
    }

    buffer->Skip(str.size());
    str.erase(str.begin());  // type
    str.pop_back();          // CR
    str.pop_back();          // LF
    if (auto pos = str.find_first_of(' '); pos != std::string::npos) {
      *object = RedisError{str.substr(0, pos), str.substr(pos + 1)};
    } else {
      *object = RedisError{.message = str};
    }
    return 1;
  } else if (type == ':') {
    // > [...] However, the returned integer is guaranteed to be in the range of
    // > a signed 64 bit integer.
    auto str = FlattenSlowUntil(*buffer, "\r\n");
    if (!EndsWith(str, "\r\n")) {
      return 0;
    }

    buffer->Skip(str.size());
    if (auto opt = TryParse<std::int64_t>({str.data() + 1, str.size() - 3})) {
      *object = *opt;
      return 1;
    }
    FLARE_LOG_WARNING_EVERY_SECOND("Received invalid integer [{}] from Redis.",
                                   str);
    return -1;  // Invalid integer.
  } else if (type == '$') {
    // > Bulk Strings are used in order to represent a single binary safe string
    // > up to 512 MB in length.
    // >
    // > Bulk Strings are encoded in the following way:
    // >
    // > - A "$" byte followed by the number of bytes composing the string (a
    // >   prefixed length), terminated by CRLF.
    // >
    // > - The actual string data.
    // >
    // > - A final CRLF.
    auto str = FlattenSlowUntil(*buffer, "\r\n");
    if (!EndsWith(str, "\r\n")) {
      return 0;
    }
    auto size_opt = TryParse<int>({str.data() + 1, str.size() - 3});
    if (!size_opt) {
      FLARE_LOG_ERROR_EVERY_SECOND("Invalid Bulk String size [{]].", str);
      return -1;
    }
    if (*size_opt == -1) {  // Special case.
      buffer->Skip(str.size());
      *object = RedisNull{};
      return 1;
    }

    if (buffer->ByteSize() < str.size() + *size_opt + 2 /* CRLF */) {
      return 0;
    }
    buffer->Skip(str.size());
    auto body = buffer->Cut(*size_opt);
    buffer->Skip(2 /* CRLF */);
    *object = std::move(body);
    return 1;
  } else if (type == '*') {
    // > RESP Arrays are sent using the following format:
    // >
    // > - A * character as the first byte, followed by the number of elements
    // >   in the array as a decimal number, followed by CRLF.
    // >
    // > - An additional RESP type for every element of the Array.
    auto str = FlattenSlowUntil(*buffer, "\r\n");
    if (!EndsWith(str, "\r\n")) {
      return 0;
    }
    auto size_opt = TryParse<int>({str.data() + 1, str.size() - 3});
    if (!size_opt) {
      FLARE_LOG_ERROR_EVERY_SECOND("Invalid RESP Array size [{]].", str);
      return -1;
    }
    if (*size_opt == -1) {  // Special case.
      buffer->Skip(str.size());

      // > A client library API should return a null object and not an empty
      // > Array when Redis replies with a Null Array. This is necessary to
      // > distinguish between an empty list and a different condition (for
      // > instance the timeout condition of the BLPOP command).
      *object = RedisNull{};
      return 1;
    }

    // Here things go hard. We can't tell in advance how many bytes this array
    // occupies. However, without dropping this "element size" line, we can't
    // recurse to check the elements.
    //
    // To keep things simple, we make a copy of buffer and try to parse it. On
    // success we consume the real buffer (variable `buffer` we were given.).
    //
    // This shouldn't hurt perf. too much. Our buffer copies cheap.
    auto copy = *buffer;
    copy.Skip(str.size());
    std::vector<RedisObject> objects;

    for (int i = 0; i != *size_opt; ++i) {
      auto rc = TryCutRedisObject(&copy, &objects.emplace_back());
      if (rc <= 0) {
        return rc;
      }
      // Keep continue then.
    }

    // All elements were successfully parsed. Skip the array then.
    buffer->Skip(buffer->ByteSize() - copy.ByteSize());
    *object = std::move(objects);
    return 1;
  } else {
    FLARE_LOG_ERROR("Unexpected Redis object type [{}].", type);
    return -1;
  }
}

}  // namespace flare::redis
