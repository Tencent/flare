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

#ifndef FLARE_BASE_OPTION_OPTION_IMPL_H_
#define FLARE_BASE_OPTION_OPTION_IMPL_H_

#include <optional>
#include <string>
#include <utility>

#include "flare/base/logging.h"
#include "flare/base/option/dynamically_changed.h"
#include "flare/base/option/key.h"
#include "flare/base/option/option_service.h"

// Internal implementation.

namespace flare::option {

namespace detail {

template <class T>
struct IdentityParser {
  static std::optional<T> TryParse(T value) { return value; }
};

template <class T>
struct remove_optional {
  using type = T;
};

template <class T>
struct remove_optional<std::optional<T>> {
  using type = T;
};

template <class T>
using remove_optional_t = typename remove_optional<T>::type;

}  // namespace detail

// Implementation detail. Use `Option<T>` instead.
//
// If needed, `Parser` can be used to apply user-defined parsing on `T`.
template <class T, class Parser>
class OptionImpl {
 public:
  using parsed_t =
      typename decltype(Parser::TryParse(std::declval<T>()))::value_type;

  // Or `value_or_ref_t`?
  using value_or_ref =
      decltype(std::declval<detail::DynamicallyChanged<parsed_t>>().Get());

  OptionImpl(const std::string& provider, MultiKey name, T default_value,
             Function<bool(T)> validator, bool fixed)
      : provider_(provider),
        name_(std::move(name)),
        value_(ParseDefaultValue(std::move(default_value))) {
    option_id_ = option::OptionService::Instance()->RegisterOptionWatcher<T>(
        provider, &name_, fixed,
        [=](std::optional<T> value) { return OnChanged(std::move(value)); });
  }
  ~OptionImpl() {
    option::OptionService::Instance()->DeregisterOptionWatcher(option_id_);
  }

  // Movable but not copyable.
  OptionImpl(OptionImpl&&) = default;
  OptionImpl& operator=(OptionImpl&&) = default;

  value_or_ref Get() const noexcept { return value_.Get(); }
  /* implicit */ operator value_or_ref() const noexcept { return Get(); }

 private:
  bool OnChanged(std::optional<T> value) {
    if (!value) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Key [{}] is not recognized by provider [{}].", name_.ToString(),
          provider_);
      return false;
    }
    if (validator_ && !validator_(*value)) {
      // FIXME: Do not print `value` if it can't be stringified. The currently
      // implementation would cause a hard compilation error, this should be
      // avoided.
      FLARE_LOG_WARNING_EVERY_SECOND(
          "New value [{}] of option [{}] from provider [{}]) didn't pass "
          "validation. Keep using the old value.",
          *value, name_.ToString(), provider_);
      return false;  // Ignore the new one.
    }
    auto parsed = Parser{}.TryParse(std::move(*value));
    if (!parsed) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "User-supplied parser failed. Keep using the old value.");
      return false;
    }
    value_.Emplace(std::move(*parsed));
    return true;
  }

  // Use `Parser` to parse the default value. This cannot fail, otherwise we
  // won't be able to initialize in a sane way.
  static parsed_t ParseDefaultValue(T&& default_value) {
    auto parsed = Parser{}.TryParse(std::move(default_value));
    FLARE_CHECK(parsed, "Value parser failed to parse the default value.");
    return *parsed;
  }

 private:
  std::uint64_t option_id_;
  std::string provider_;
  MultiKey name_;
  detail::DynamicallyChanged<parsed_t> value_;
  Function<bool(T)> validator_;
};

template <class T, class Parser>
class OptionImpl<std::optional<T>, Parser> {
 public:
  using parsed_t =
      typename decltype(Parser::TryParse(std::declval<T>()))::value_type;

  // Or `value_or_ref_t`?
  using value_or_ref =
      decltype(std::declval<
                   detail::DynamicallyChanged<std::optional<parsed_t>>>()
                   .Get());

  OptionImpl(const std::string& provider, MultiKey name,
             std::optional<T> default_value, Function<bool(T)> validator,
             bool fixed)
      : provider_(provider),
        name_(std::move(name)),
        value_(ParseDefaultValue(std::move(default_value))) {
    option_id_ = option::OptionService::Instance()->RegisterOptionWatcher<T>(
        provider, &name_, fixed,
        [=](std::optional<T> value) { return OnChanged(std::move(value)); });
  }
  ~OptionImpl() {
    option::OptionService::Instance()->DeregisterOptionWatcher(option_id_);
  }

  // Movable but not copyable.
  OptionImpl(OptionImpl&&) = default;
  OptionImpl& operator=(OptionImpl&&) = default;

  value_or_ref Get() const noexcept { return value_.Get(); }
  /* implicit */ operator value_or_ref() const noexcept { return Get(); }

 private:
  bool OnChanged(std::optional<T> value) {
    if (!value) {
      value_.Emplace(std::nullopt);
      return true;
    }

    if (validator_ && !validator_(*value)) {
      // FIXME: Do not print `value` if it can't be stringified. The currently
      // implementation would cause a hard compilation error, this should be
      // avoided.
      FLARE_LOG_WARNING_EVERY_SECOND(
          "New value [{}] of option [{}] from provider [{}]) didn't pass "
          "validation. Keep using the old value.",
          *value, name_.ToString(), provider_);
      return false;  // Ignore the new one.
    }
    auto parsed = Parser{}.TryParse(std::move(*value));
    if (!parsed) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "User-supplied parser failed. Keep using the old value.");
      return false;
    }
    value_.Emplace(std::move(*parsed));
    return true;
  }

  // Use `Parser` to parse the default value. This cannot fail, otherwise we
  // won't be able to initialize in a sane way.
  static std::optional<parsed_t> ParseDefaultValue(
      std::optional<T> default_value) {
    if (!default_value) {
      return std::nullopt;
    }
    auto parsed = Parser{}.TryParse(*default_value);
    FLARE_CHECK(parsed, "Value parser failed to parse the default value.");
    return *parsed;
  }

 private:
  std::uint64_t option_id_;
  std::string provider_;
  MultiKey name_;
  detail::DynamicallyChanged<std::optional<parsed_t>> value_;
  Function<bool(T)> validator_;
};

}  // namespace flare::option

#endif  // FLARE_BASE_OPTION_OPTION_IMPL_H_
