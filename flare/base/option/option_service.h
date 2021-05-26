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

#ifndef FLARE_BASE_OPTION_OPTION_SERVICE_H_
#define FLARE_BASE_OPTION_OPTION_SERVICE_H_

#include <any>
#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "thirdparty/jsoncpp/value.h"

#include "flare/base/function.h"
#include "flare/base/logging.h"
#include "flare/base/never_destroyed.h"
#include "flare/base/option/dynamically_changed.h"
#include "flare/base/option/key.h"

namespace flare {

class OptionPassiveProvider;

}  // namespace flare

namespace flare::option {

// `OptionService` is responsible for periodically query options' values and
// update `Option<T>` instance accordingly.
//
// THIS CLASS IS NOT INTENDED FOR PUBLIC USE. Consider using standalone methods
// in `flare::option::` instead.
class OptionService {
 public:
  OptionService();
  ~OptionService();

  static OptionService* Instance();

  // Resolve all options being watched, and call their callbacks (for
  // initialization `Option<...>` instances.)
  bool ResolveAll();

  // Register a watcher on option `*name_ref` provided by `provider`.
  //
  // `cb` might be called even if the value has not changed.
  template <class T>
  [[gnu::noinline]] std::uint64_t RegisterOptionWatcher(
      const std::string& provider, const MultiKey* name_ref, bool is_fixed,
      Function<bool(T)> cb) {
    auto id = ++option_id_;
    {
      std::scoped_lock _(lock_);
      auto&& opt = options_[provider];

      // Make sure there's no duplicate.
      for (auto&& w : opt.watchers) {
        // FIXME: Use `std::source_location` to dump more information once it's
        // available.
        FLARE_LOG_FATAL_IF(w->name_ref->ToString() == name_ref->ToString(),
                           "Option [{}] from [{}] has already been registered.",
                           name_ref->ToString(), provider);
      }

      // Add a new entry.
      auto watcher = std::make_unique<WatchedOption>();
      watcher->id = id;
      watcher->name_ref = name_ref;
      watcher->is_fixed = is_fixed;
      watcher->read_cb = CreateReader(provider, name_ref, std::move(cb),
                                      &watcher->current_value_as_json);
      options_[provider].watchers.push_back(std::move(watcher));
    }

    if (resolve_all_done_) {
      ResolveAll();
    }

    return id;
  }

  void DeregisterOptionWatcher(std::uint64_t id);

  // Called periodically. Check if option's value has changed.
  void UpdateOptions();

  // Dump all registered options.
  Json::Value Dump() const;

  void Shutdown();

 private:
  friend class NeverDestroyedSingleton<OptionService>;
  using ReadCallback = Function<bool(OptionPassiveProvider*)>;
  struct WatchedOption {
    bool initial_resolution_done{false};
    std::uint64_t id;
    const MultiKey* name_ref;
    bool is_fixed;
    ReadCallback read_cb;
    Json::Value current_value_as_json;
  };
  struct Options {
    OptionPassiveProvider* provider = nullptr;
    std::vector<std::unique_ptr<WatchedOption>> watchers;
  };

#define FLARE_DETAIL_OPTION_DECLARE_CREATE_WATCHER_FOR(Type)                   \
  ReadCallback CreateReader(const std::string& provider, const MultiKey* name, \
                            Function<bool(Type)> cb,                           \
                            Json::Value* current_value);

  FLARE_DETAIL_OPTION_DECLARE_CREATE_WATCHER_FOR(bool)
  FLARE_DETAIL_OPTION_DECLARE_CREATE_WATCHER_FOR(std::int8_t)
  FLARE_DETAIL_OPTION_DECLARE_CREATE_WATCHER_FOR(std::uint8_t)
  FLARE_DETAIL_OPTION_DECLARE_CREATE_WATCHER_FOR(std::int16_t)
  FLARE_DETAIL_OPTION_DECLARE_CREATE_WATCHER_FOR(std::uint16_t)
  FLARE_DETAIL_OPTION_DECLARE_CREATE_WATCHER_FOR(std::int32_t)
  FLARE_DETAIL_OPTION_DECLARE_CREATE_WATCHER_FOR(std::uint32_t)
  FLARE_DETAIL_OPTION_DECLARE_CREATE_WATCHER_FOR(std::int64_t)
  FLARE_DETAIL_OPTION_DECLARE_CREATE_WATCHER_FOR(std::uint64_t)
  FLARE_DETAIL_OPTION_DECLARE_CREATE_WATCHER_FOR(float)
  FLARE_DETAIL_OPTION_DECLARE_CREATE_WATCHER_FOR(double)
  FLARE_DETAIL_OPTION_DECLARE_CREATE_WATCHER_FOR(std::string)

#undef FLARE_DETAIL_OPTION_DECLARE_CREATE_WATCHER_FOR

 private:
  std::uint64_t timer_id_{};

  // To provide `Option<T>` with fresh value, we need to resolve the option
  // immediately if `ResolveAll()` has already been called. (Were it not called
  // yet, the whole program has not already finished initialization, so no
  // hurry.)
  std::atomic<bool> resolve_all_done_{false};
  std::atomic<std::uint64_t> option_id_{};

  mutable std::mutex lock_;  // Being slow is not a problem here.
  std::unordered_map<std::string, Options> options_;
};

}  // namespace flare::option

#endif  // FLARE_BASE_OPTION_OPTION_SERVICE_H_
