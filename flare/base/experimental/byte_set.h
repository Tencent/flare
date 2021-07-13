// Copyright (C) 2011 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_BASE_EXPERIMENTAL_BYTE_SET_H_
#define FLARE_BASE_EXPERIMENTAL_BYTE_SET_H_

#include <cctype>
#include <climits>
#include <string_view>

namespace flare::experimental {

// Implements a set of byte.
class ByteSet {
 public:
  using value_type = std::uint8_t;  // `unsigned char`.

  constexpr ByteSet() : rep_{} {}
  constexpr explicit ByteSet(const char* str) : rep_(FromString(str)) {}

  constexpr explicit ByteSet(std::string_view bytes) : ByteSet() {
    insert(bytes);
  }

  // fill with pred
  template <typename Pred>
  explicit ByteSet(const Pred& pred) : ByteSet() {
    insert_if(pred);
  }

  constexpr ByteSet operator|(const ByteSet& bs) const {
    return ByteSet{rep_ | bs.rep_};
  }
  constexpr ByteSet operator|(const char* s) const {
    return *this | ByteSet(s);
  }
  constexpr ByteSet operator&(const ByteSet& bs) const {
    return ByteSet{rep_ & bs.rep_};
  }
  constexpr bool operator==(const ByteSet& bs) const { return rep_ == bs.rep_; }

  // insert one byte
  void insert(value_type n) { rep_.u[n / 64] |= (1ULL << (n % 64)); }

  // remove one byte
  void erase(value_type n) { rep_.u[n / 64] &= ~(1ULL << (n % 64)); }

  // clear all bytes
  void clear() { rep_ = {}; }

  // insert bytes by pred
  // bool pred(value_type byte)
  template <typename Pred>
  void insert_if(const Pred& pred) {
    for (int c = 0; c <= UCHAR_MAX; ++c) {
      if (pred(c)) insert(c);
    }
  }

  // insert any bytes in str into set
  void insert(std::string_view bytes) {
    for (auto&& e : bytes) {
      insert(e);
    }
  }

  // remove all bytes which satisfy the pred
  template <typename Pred>
  void erase_if(const Pred& pred) {
    for (int c = 0; c <= UCHAR_MAX; ++c) {
      if (pred(c)) erase(c);
    }
  }

  // remove all bytes in str from the set
  void erase(std::string_view bytes) {
    for (auto&& e : bytes) {
      erase(e);
    }
  }

  // return whether exist
  constexpr bool contains(value_type c) const {
    return (rep_.u[c / 64] >> (c % 64)) & 1;
  }

  // OR the set with rhs
  ByteSet& operator|=(const ByteSet& rhs) {
    rep_ |= rhs.rep_;
    return *this;
  }

  // AND the set with rhs
  ByteSet& operator&=(const ByteSet& rhs) {
    rep_ &= rhs.rep_;
    return *this;
  }

  // OR the set with string
  ByteSet& operator|=(const char* rhs) {
    insert(rhs);
    return *this;
  }

 public:  // predefined singletons
  // return the set of all bytes satisfy isspace
  static const ByteSet& spaces() {
    static const ByteSet cs(isspace);
    return cs;
  }

  // return the set of all bytes satisfy isblank
  static const ByteSet& blanks() {
    static const ByteSet cs(isblank);
    return cs;
  }

  // return the set of all bytes satisfy isspace
  static const ByteSet& alphas() {
    static const ByteSet cs(isalpha);
    return cs;
  }

  // return the set of all bytes satisfy isalnum
  static const ByteSet& alpha_nums() {
    static const ByteSet cs(isalnum);
    return cs;
  }

  // return the set of all bytes satisfy isascii
  static const ByteSet& asciis() {
    static const ByteSet cs(isascii);
    return cs;
  }

  // return the set of all bytes satisfy isxdigit
  static const ByteSet& hex() {
    static const ByteSet cs(isxdigit);
    return cs;
  }

  // return the set of all bytes satisfy isdigit
  static const ByteSet& digits() {
    static const ByteSet cs(isdigit);
    return cs;
  }

  // return the set of all bytes satisfy isupper
  static const ByteSet& uppercase() {
    static const ByteSet cs(isupper);
    return cs;
  }

  // return the set of all bytes satisfy islower
  static const ByteSet& lowercase() {
    static const ByteSet cs(islower);
    return cs;
  }

  // return the set of all bytes satisfy isprint
  static const ByteSet& printables() {
    static const ByteSet cs(isprint);
    return cs;
  }

 private:
  struct Rep {
    uint64_t u[4];
    constexpr Rep operator|(const Rep& rep) const {
      return {
          {u[0] | rep.u[0], u[1] | rep.u[1], u[2] | rep.u[2], u[3] | rep.u[3]}};
    }
    constexpr Rep operator&(const Rep& rep) const {
      return {
          {u[0] & rep.u[0], u[1] & rep.u[1], u[2] & rep.u[2], u[3] & rep.u[3]}};
    }
    Rep& operator|=(const Rep& rep) {
      u[0] |= rep.u[0];
      u[1] |= rep.u[1];
      u[2] |= rep.u[2];
      u[3] |= rep.u[3];
      return *this;
    }
    Rep& operator&=(const Rep& rep) {
      u[0] &= rep.u[0];
      u[1] &= rep.u[1];
      u[2] &= rep.u[2];
      u[3] &= rep.u[3];
      return *this;
    }
    constexpr bool operator==(const Rep& rep) const {
      return u[0] == rep.u[0] && u[1] == rep.u[1] && u[2] == rep.u[2] &&
             u[3] == rep.u[3];
    }
  };
  static constexpr Rep FromChar(value_type c) {
    return c < 64 ? Rep{{(1ULL << c), 0, 0, 0}}
                  : c < 128 ? Rep{{0, (1ULL << (c - 64)), 0, 0}}
                            : c < 192 ? Rep{{0, 0, (1ULL << (c - 128)), 0}}
                                      : Rep{{0, 0, 0, (1ULL << (c - 192))}};
  }
  static constexpr Rep FromString(const char* s) {
    return *s == '\0' ? Rep() : FromChar(*s) | FromString(s + 1);
  }
  explicit constexpr ByteSet(Rep rep) : rep_(rep) {}

 private:
  Rep rep_;
};

}  // namespace flare::experimental

#endif  // FLARE_BASE_EXPERIMENTAL_BYTE_SET_H_
