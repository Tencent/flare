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

#include "flare/base/option/option_service.h"

#include <optional>
#include <string>
#include <utility>

#include "thirdparty/gflags/gflags.h"

#include "flare/base/internal/time_keeper.h"
#include "flare/base/logging.h"
#include "flare/base/option/option_provider.h"

DEFINE_int32(
    flare_option_update_interval, 10,
    "Interval between updating values of `Option<T>`, in seconds. Do NOT set "
    "it too low, or you'll overload the server or your program (or both).");

using namespace std::literals;

namespace flare::option {

namespace {

template <class T>
Json::Value ToJsonValue(T value) {
  // JsonCpp is rather silly here..
  if constexpr (std::is_unsigned_v<T>) {
    return Json::Value(static_cast<Json::UInt64>(value));
  } else if constexpr (std::is_signed_v<T>) {
    return Json::Value(static_cast<Json::Int64>(value));
  } else {
    return Json::Value(value);
  }
}

}  // namespace

OptionService::OptionService() {}

OptionService::~OptionService() {}

OptionService* OptionService::Instance() {
  static NeverDestroyedSingleton<OptionService> os;
  return os.Get();
}

bool OptionService::ResolveAll() {
  bool all_success = true;
  std::scoped_lock _(lock_);
  for (auto&& [prov, options] : options_) {
    auto p = options.provider = option_passive_provider_registry.Get(prov);
    std::vector<const option::MultiKey*> keys;
    for (auto&& e : options.watchers) {
      if (!e->is_fixed && e->name_ref->Ready()) {
        keys.push_back(e->name_ref);
      }
    }
    // Called with lock. Perf. is not an issue here.
    if (!options.provider->GetAll(keys)) {
      FLARE_LOG_ERROR(
          "Initial try of reading options from [{}] failed. Failing back to "
          "the defaults.",
          prov);
      all_success = false;
      continue;
    }
    for (auto&& e : options.watchers) {
      if (!std::exchange(e->initial_resolution_done, true) &&
          e->name_ref->Ready()) {
        // Called with lock.
        if (!e->read_cb(p)) {
          FLARE_LOG_ERROR(
              "Initial try of reading [{}] from [{}] failed. "
              "Failing back to the defaults.",
              e->name_ref->ToString(), prov);
          all_success = false;
        }
        e->initial_resolution_done = true;  // Set even if the initial try
                                            // failed. We've warned the user.
      }
    }
  }
  resolve_all_done_ = true;  // Even if we didn't get a huge success, we're
                             // done.
  // Start a timer to refresh option values periodically.
  timer_id_ = internal::TimeKeeper::Instance()->AddTimer(
      {}, FLAGS_flare_option_update_interval * 1s,
      [this](auto) { UpdateOptions(); }, true);
  return all_success;
}

void OptionService::DeregisterOptionWatcher(std::uint64_t id) {
  std::scoped_lock _(lock_);
  for (auto&& [k, v] : options_) {
    for (auto iter = v.watchers.begin(); iter != v.watchers.end(); ++iter) {
      if ((*iter)->id == id) {
        v.watchers.erase(iter);
        return;
      }
    }
  }
  FLARE_CHECK(0, "Option #{} is not recognized.", id);
}

void OptionService::UpdateOptions() {
  std::scoped_lock _(lock_);
  // Let's see if it still works well if there are a lot of options to be read.
  for (auto&& [prov, options] : options_) {
    if (!options.provider) {
      // We run too early, even before the first call to `ResolveAll()` (for
      // this provider.).
      continue;
    }
    std::vector<const MultiKey*> keys;
    for (auto&& e : options.watchers) {
      if (!e->is_fixed && e->name_ref->Ready()) {
        keys.push_back(e->name_ref);
      }
    }
    // Called with lock. Perf. is not an issue here.
    if (!options.provider->GetAll(keys)) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Failed to read options from [{}]. Keep using current values.", prov);
      continue;
    }
    for (auto&& e : options.watchers) {
      if (e->is_fixed || !e->name_ref->Ready()) {
        continue;  // For "fixed" options, they're resolved only once, at the
                   // time when `ResolveAll()` is called.
      }
      // Called with lock.
      if (!e->read_cb(options.provider)) {
        FLARE_LOG_WARNING_EVERY_SECOND(
            "Failed to read option [{}] from [{}]. Keep using current "
            "value.",
            e->name_ref->ToString(), prov);
      }
    }
  }
}

Json::Value OptionService::Dump() const {
  Json::Value jsv;
  std::scoped_lock _(lock_);
  for (auto&& [prov, options] : options_) {
    auto&& opts = jsv[prov];
    for (auto&& e : options.watchers) {
      if (!e->name_ref->Ready()) {
        continue;
      }
      auto current_path = &opts;
      for (auto&& key : e->name_ref->GetKeys()) {
        current_path = &(*current_path)[key.Get()];
      }
      *current_path = e->current_value_as_json;
    }
  }
  return jsv;
}

void OptionService::Shutdown() {
  internal::TimeKeeper::Instance()->KillTimer(timer_id_);
}

#define FLARE_DETAIL_OPTION_DEFINE_CREATE_WATCHER_FOR(Type, TypeName)         \
  OptionService::ReadCallback OptionService::CreateReader(                    \
      const std::string& provider, const MultiKey* name,                      \
      Function<bool(Type)> cb, Json::Value* current_value) {                  \
    return [cb = std::move(cb), provider, name, current_value,                \
            last = std::optional<Type>()](OptionPassiveProvider* p) mutable { \
      Type value;                                                             \
      if (p->Get##TypeName(*name, &value)) {                                  \
        if (last && last == value) {                                          \
          return true; /* No change. */                                       \
        }                                                                     \
        last = value;                                                         \
        if (cb(value)) {                                                      \
          *current_value = ToJsonValue(value);                                \
          return true;                                                        \
        }                                                                     \
        return false;                                                         \
      } else {                                                                \
        return false;                                                         \
      }                                                                       \
    };                                                                        \
  }

FLARE_DETAIL_OPTION_DEFINE_CREATE_WATCHER_FOR(bool, Bool)
FLARE_DETAIL_OPTION_DEFINE_CREATE_WATCHER_FOR(std::int8_t, Int8)
FLARE_DETAIL_OPTION_DEFINE_CREATE_WATCHER_FOR(std::uint8_t, UInt8)
FLARE_DETAIL_OPTION_DEFINE_CREATE_WATCHER_FOR(std::int16_t, Int16)
FLARE_DETAIL_OPTION_DEFINE_CREATE_WATCHER_FOR(std::uint16_t, UInt16)
FLARE_DETAIL_OPTION_DEFINE_CREATE_WATCHER_FOR(std::int32_t, Int32)
FLARE_DETAIL_OPTION_DEFINE_CREATE_WATCHER_FOR(std::uint32_t, UInt32)
FLARE_DETAIL_OPTION_DEFINE_CREATE_WATCHER_FOR(std::int64_t, Int64)
FLARE_DETAIL_OPTION_DEFINE_CREATE_WATCHER_FOR(std::uint64_t, UInt64)
FLARE_DETAIL_OPTION_DEFINE_CREATE_WATCHER_FOR(float, Float)
FLARE_DETAIL_OPTION_DEFINE_CREATE_WATCHER_FOR(double, Double)
FLARE_DETAIL_OPTION_DEFINE_CREATE_WATCHER_FOR(std::string, String)

#undef FLARE_DETAIL_OPTION_DEFINE_CREATE_WATCHER_FOR

}  // namespace flare::option
