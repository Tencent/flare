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

#ifndef FLARE_BASE_OPTION_KEY_H_
#define FLARE_BASE_OPTION_KEY_H_

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace flare::option {

// To deal with a variety of option keys, we provide this class as a generic way
// to handle them.
class Key {
 public:
  // Construct `Key` with a key implementation.
  template <class T, class = decltype(std::declval<T>().Get())>
  /* implicit */ Key(T&& impl)
      : impl_(CreateImpl<std::decay_t<T>>(std::forward<T>(impl))) {}

  // For `std::string`, we provide a shortcut for easier use.
  /* implicit */ Key(std::string name);
  /* implicit */ Key(const char* name) : Key(std::string(name)) {}

  // Returns `std::nullopt` if the key is not available yet. This can be the
  // case for `DynamicKey` if `SetDynamicKey` has not been called yet.
  std::string Get() const;

  // For certain type of keys, the key is not ready until some condition happens
  // (e.g., first call ot `SetDynamicKey(...)`). This method tests if the key is
  // ready.
  bool Ready() const noexcept;

 private:
  class AbstractKey;
  template <class T>
  class KeyImpl;

  template <class T, class U>
  static std::unique_ptr<AbstractKey> CreateImpl(U&& impl);

  std::unique_ptr<AbstractKey> impl_;
};

// Multi-part key.
class MultiKey {
 public:
  // `std::initializer_list` does not support move-only types, so we use
  // variadic template here. This does not feel quite C++-ish.
  template <class... Ts,
            class = std::enable_if_t<(std::is_convertible_v<Ts, Key> && ...)>>
  /* implicit */ MultiKey(Ts&&... keys);

  // Concatenate keys.
  MultiKey(MultiKey&& first, Key&& second);
  MultiKey(MultiKey&& first, MultiKey&& second);

  // For `std::string`, we make the user's life easier.
  /* implicit */ MultiKey(std::string name);
  /* implicit */ MultiKey(const char* name);

  const std::vector<Key>& GetKeys() const noexcept;
  bool Ready() const noexcept;

  std::string ToString() const;

 private:
  std::vector<Key> keys_;
};

// Represents a key that is a plain string (i.e., it won't change).
class FixedKey {
 public:
  explicit FixedKey(std::string s) : str_(std::move(s)) {}

  // Not very performant but we don't expect `Key`s` to be access too often.
  std::string Get() const { return str_; }
  bool Ready() const noexcept { return true; }

 private:
  std::string str_;
};

// Represents a key that, each time it's read, query its value from a global
// mapping (@sa: `SetDynamicKey`).
class DynamicKey {
 public:
  explicit DynamicKey(std::string name) : name_(std::move(name)) {}

  std::string Get() const;
  bool Ready() const noexcept;

 private:
  std::string name_;
};

// Represents a key whose value is read from an external string.
//
// Note that the external string is NOT expected to change (once referenced by
// this class) during its whole life. The reason is that we can be reading the
// string at any time, and modifying it concurrently to read is a race. If you
// want to update the string dynamically, use `DynamicKey` instead.
class ReferencingKey {
 public:
  // It's your responsibility to make sure `ref` exists during the lifetime of
  // the program.
  explicit ReferencingKey(const std::string& ref) : ref_(&ref) {}

  // If this overload is chosen, you're likely passing a temporary to us, and
  // this is an error.
  explicit ReferencingKey(std::string&& ref) = delete;

  std::string Get() const { return *ref_; }
  bool Ready() const noexcept { return true; }

 private:
  const std::string* ref_;
};

// Set value for a dynamic key.
void SetDynamicKey(const std::string& name, std::string value);

///////////////////////////////////////
// Implementation goes below.        //
///////////////////////////////////////

class Key::AbstractKey {
 public:
  virtual ~AbstractKey() = default;
  virtual std::string Get() const = 0;
  virtual bool Ready() const noexcept = 0;
};

template <class T>
class Key::KeyImpl : public AbstractKey {
 public:
  template <class U>
  explicit KeyImpl(U&& object) : impl_(std::forward<U>(object)) {}
  std::string Get() const override { return impl_.Get(); }
  bool Ready() const noexcept override { return impl_.Ready(); }

 private:
  T impl_;
};

template <class T, class U>
std::unique_ptr<Key::AbstractKey> Key::CreateImpl(U&& impl) {
  return std::make_unique<KeyImpl<T>>(std::forward<U>(impl));
}

template <class... Ts, class>
MultiKey::MultiKey(Ts&&... keys) {
  [[maybe_unused]] int dummy[] = {
      (keys_.push_back(std::forward<Ts>(keys)), 0)...};
}

}  // namespace flare::option

#endif  // FLARE_BASE_OPTION_KEY_H_
