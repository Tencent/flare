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

#ifndef FLARE_NET_REDIS_REDIS_OBJECT_H_
#define FLARE_NET_REDIS_REDIS_OBJECT_H_

#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "flare/base/buffer.h"

namespace flare {

class RedisObject;

///////////////////////////////////////
// All possible Redis object types.  //
///////////////////////////////////////

using RedisString = std::string;

// https://redis.io/topics/protocol:
//
// > However, the returned integer is guaranteed to be in the range of a
// > signed 64 bit integer.
using RedisInteger = std::int64_t;

// Called as "Bulk String" in Redis' documentation.
using RedisBytes = NoncontiguousBuffer;

// Array is NOT guaranteed to be heterogeneous. You need to check type of each
// element.
using RedisArray = std::vector<RedisObject>;

// Describes an error return.
struct RedisError {
  std::string category;
  std::string message;
};

// Describes a Redis "null object".
struct RedisNull {};

// Represents a Redis object. This is ususally a Redis response.
class RedisObject {
 public:
  RedisObject() = default;

  // Constructs a Redis Object with the value.
  /* implicit */ RedisObject(RedisString value) : value_(std::move(value)) {}
  /* implicit */ RedisObject(RedisInteger value) : value_(value) {}
  /* implicit */ RedisObject(RedisBytes value) : value_(std::move(value)) {}
  /* implicit */ RedisObject(RedisArray value) : value_(std::move(value)) {}
  /* implicit */ RedisObject(RedisError value) : value_(std::move(value)) {}
  /* implicit */ RedisObject(RedisNull value) : value_(value) {}

  // Tests if this object is of the given type.
  template <class T>
  bool is() const noexcept {
    return !!try_as<T>();
  }

  // Casts `*this` to the given type. If `*this` does not hold an object of the
  // given type, `nullptr` is returned.
  //
  // Returning `T*` seems weird, but returing reference does not conform to our
  // coding convention..
  template <class T>
  T* try_as() noexcept {
    return std::get_if<T>(&value_);
  }
  template <class T>
  const T* try_as() const noexcept {
    return std::get_if<T>(&value_);
  }

  // Same as `try_as` except we crash the program if conversion fails.
  template <class T>
  T* as() noexcept {
    auto result = try_as<T>();
    FLARE_CHECK(result, "This Redis object does not contain the given type.");
    return result;
  }
  template <class T>
  const T* as() const noexcept {
    return const_cast<RedisObject*>(this)->as<T>();
  }

 private:
  std::variant<std::monostate, RedisString, RedisInteger, RedisBytes,
               RedisArray, RedisError, RedisNull>
      value_;
};

}  // namespace flare

#endif  // FLARE_NET_REDIS_REDIS_OBJECT_H_
