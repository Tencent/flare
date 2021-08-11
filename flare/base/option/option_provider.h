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

#ifndef FLARE_BASE_OPTION_OPTION_PROVIDER_H_
#define FLARE_BASE_OPTION_OPTION_PROVIDER_H_

#include <cstdint>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "flare/base/dependency_registry.h"
#include "flare/base/option/key.h"
#include "flare/base/status.h"

namespace flare {

// This is the interface each `Option` provider should implement.
//
// Class template `Option` internally uses the provider to query (possibly
// periodically) option values.
//
// The implementation is NOT required (although encouraged) to be efficient,
// `Option` itself does caching. (CAUTION: BEING TOOOO SLOW CAN CAUSE PROBLEMS.)
//
// It's the framework's responsibility to make sure the same provider is not
// called concurrently.
class OptionPassiveProvider {
 public:
  virtual ~OptionPassiveProvider() = default;

  // If your provider benefit from fetching all options in advance (and cache
  // the result) and return values from `GetXxx` below directly (from the
  // cache), you can implement this method. Otherwise `true` should be returned
  // without performing actual operations.
  virtual bool GetAll(const std::vector<const option::MultiKey*>& names) = 0;

  virtual Status GetBool(const option::MultiKey& name,
                         std::optional<bool>* value) = 0;
  virtual Status GetInt8(const option::MultiKey& name,
                         std::optional<std::int8_t>* value) = 0;
  virtual Status GetUInt8(const option::MultiKey& name,
                          std::optional<std::uint8_t>* value) = 0;
  virtual Status GetInt16(const option::MultiKey& name,
                          std::optional<std::int16_t>* value) = 0;
  virtual Status GetUInt16(const option::MultiKey& name,
                           std::optional<std::uint16_t>* value) = 0;
  virtual Status GetInt32(const option::MultiKey& name,
                          std::optional<std::int32_t>* value) = 0;
  virtual Status GetUInt32(const option::MultiKey& name,
                           std::optional<std::uint32_t>* value) = 0;
  virtual Status GetInt64(const option::MultiKey& name,
                          std::optional<std::int64_t>* value) = 0;
  virtual Status GetUInt64(const option::MultiKey& name,
                           std::optional<std::uint64_t>* value) = 0;
  virtual Status GetFloat(const option::MultiKey& name,
                          std::optional<float>* value) = 0;
  virtual Status GetDouble(const option::MultiKey& name,
                           std::optional<double>* value) = 0;
  virtual Status GetString(const option::MultiKey& name,
                           std::optional<std::string>* value) = 0;
  // `(unsigned) char`?
  // `long double`?

  // If the provider supports UDF, `GetAny` should be able to provide support
  // for it.
  //
  // virtual bool GetAny(const option::MultiKey& name, void* value, TypeIndex
  //     type);
};

// Provider that proactively notifies `OptionService` about value changes.
//
// TODO(luobogao): Implement this.
// class OptionProactiveProvider {};

// If we want to provide paramter to providers, we can use class registry
// instead, and pass parameter-string passed to `Option<T>` all the way to
// `XxxProvider::Init(...)`. This might be necessary if multiple credentials are
// needed to access different configuration repository.
FLARE_DECLARE_OBJECT_DEPENDENCY_REGISTRY(option_passive_provider_registry,
                                         OptionPassiveProvider);

}  // namespace flare

#define FLARE_OPTION_REGISTER_OPTION_PROVIDER(Name, Implementation) \
  FLARE_REGISTER_OBJECT_DEPENDENCY(                                 \
      flare::option_passive_provider_registry, Name,                \
      [] { return std::make_unique<Implementation>(); })

#endif  // FLARE_BASE_OPTION_OPTION_PROVIDER_H_
