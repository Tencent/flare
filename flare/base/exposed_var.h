// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_BASE_EXPOSED_VAR_H_
#define FLARE_BASE_EXPOSED_VAR_H_

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "thirdparty/jsoncpp/value.h"

#include "flare/base/deferred.h"
#include "flare/base/function.h"
#include "flare/base/internal/meta.h"
#include "flare/base/tsc.h"
#include "flare/base/write_mostly.h"

namespace flare {

namespace exposed_var {

template <class T>
Json::Value ToJsonValue(const T& t) {
  // Tests if `t.ToString()` is well-formed.
  auto has_to_string = FLARE_INTERNAL_IS_VALID(x.ToString());

  if constexpr (std::is_same_v<T, Json::Value>) {
    // `t` itself is already a JSON value.
    return t;
  } else if constexpr (std::is_integral_v<T>) {
    // JsonCpp does not handle integral types well (ambiguous overload set).
    if constexpr (std::is_unsigned_v<T>) {
      return Json::Value(static_cast<Json::UInt64>(t));
    } else {
      return Json::Value(static_cast<Json::Int64>(t));
    }
  } else if constexpr (std::is_floating_point_v<T> ||
                       // Does JsonCpp support this?
                       std::is_same_v<T, std::string_view> ||
                       std::is_same_v<T, std::string> ||
                       std::is_same_v<T, const char*> ||
                       std::is_same_v<T, bool>) {
    // Types handle-able by JsonCpp itself.
    return Json::Value(t);
  } else if constexpr (has_to_string(t)) {
    return Json::Value(t.ToString());
  } else {
    static_assert(sizeof(T) == 0,
                  "You need to specialize `flare::exposed_var::ToJsonValue` "
                  "before using `ExposedVar(Dynamic)<T>`.");
  }
}

template <class T>
Json::Value ToJsonValue(const std::atomic<T>& v) {
  return ToJsonValue(v.load(std::memory_order_relaxed));
}

// We can provide a method support all types who provide a `Read()` method, but
// I'm not sure if it's an overkill. So let's be safe here.
template <class T>
Json::Value ToJsonValue(const WriteMostlyCounter<T>& v) {
  return ToJsonValue(v.Read());
}

template <class T>
Json::Value ToJsonValue(const WriteMostlyGauge<T>& v) {
  return ToJsonValue(v.Read());
}

template <class T>
Json::Value ToJsonValue(const WriteMostlyMiner<T>& v) {
  return ToJsonValue(v.Read());
}

template <class T>
Json::Value ToJsonValue(const WriteMostlyMaxer<T>& v) {
  return ToJsonValue(v.Read());
}

template <class T>
Json::Value ToJsonValue(const WriteMostlyAverager<T>& v) {
  return ToJsonValue(v.Read());
}

}  // namespace exposed_var

class ExposedVarDynamicTree;

// Exposed variables forms a hierarchical (tree) structure, with root and
// internal nodes being `ExposedVarGroup`.
class ExposedVarGroup {
 public:
  using Handle = Deferred;

  // Adds value in `rel_path` to us.
  //
  // `\0` is not allowed in `rel_path`.
  //
  // Slashes in `rel_path`, if present, is treated as internal nodes' name
  // (i.e., `Add("a/b/c", ...)` results in a node named `c` residing in
  // `/x/y/z/a/b/`.)
  //
  // If there's no slash in `rel_path`, `rel_path` is treated as name of this
  // node.
  //
  // To use slashes in intermediate node name, escape it with backslash
  // (`Add("/a\/b", ...)` results in a node named `a/b`).
  //
  // Intermediate nodes are created along the way (if not present yet).
  // Duplicate names lead to crash.
  Handle Add(std::string_view rel_path, Function<Json::Value()> value);
  Handle Add(std::string_view rel_path, ExposedVarDynamicTree* dynamic_tree);

  // Internal nodes in `abs_path` are separated by backslashes.
  //
  // @sa: `Add` for usage of slashes in `abs_path`.
  static ExposedVarGroup* FindOrCreate(std::string_view abs_path);

  // Read value at `abs_path` if it's present.
  //
  // @sa: `Add` for usage of slashes in `abs_path`.
  static std::optional<Json::Value> TryGet(std::string_view abs_path);

 private:
  // Unified getter for normal value and dynamic tree.
  //
  // Relative path to the value is given (in case of normal value, this argument
  // is meaningless.).
  using Getter = Function<std::optional<Json::Value>(std::string_view)>;

  explicit ExposedVarGroup(std::string abs_path);

  // Get aboluste path of this node.
  const std::string& AbsolutePath() const;

  // Find node nearest to `rel_path`. `left` is updated to reflect the remaining
  // part of path.
  ExposedVarGroup* FindLowest(std::string_view rel_path,
                              std::string_view* left);

  // Create (if not present yet) nodes upto `rel_path`.
  ExposedVarGroup* CreateUpto(std::string_view rel_path);

  // Add leave with name `name`, it's value is available at `value`.
  Handle AddDirect(std::string_view name, Getter value);

  // Dump this group as JSON.
  Json::Value Dump() const;

  // Get root group.
  static ExposedVarGroup* Root();

 private:
  std::string abs_path_;
  mutable std::shared_mutex lock_;  // Slow.

  // `\0`, if present, in keys, is a substitution of slash.
  std::unordered_map<std::string, std::unique_ptr<ExposedVarGroup>> nodes_;
  std::unordered_map<std::string, Getter> leaves_;
};

// Leaves in exported variable tree could be `ExposedVar`. The value of this
// type of leaf should be updated proactively by the user.
//
// Note that `T` itself must be thread-safe since it might be read by the
// library at any time.
template <class T>
class ExposedVar {
 public:
  // @sa `ExposedVarGroup::Add` for usage of slashes in `rel_path`.
  explicit ExposedVar(std::string_view rel_path) {
    LinkToParent(rel_path, ExposedVarGroup::FindOrCreate("/"));
  }

  // We cannot default `initial_value` to `T()`, otherwise non-movable types
  // won't work.
  //
  // @sa: `ExposedVarGroup::Add` for usage of slashes in `rel_path`.
  template <class U>
  ExposedVar(std::string_view rel_path, U&& initial_value,
             ExposedVarGroup* parent = ExposedVarGroup::FindOrCreate("/"))
      : obj_(std::forward<U>(initial_value)) {
    LinkToParent(rel_path, parent);
  }

  // Getter. Users could update value in this object here.
  T* operator->() noexcept { return &obj_; }
  T& operator*() noexcept { return obj_; }

 private:
  void LinkToParent(std::string_view rel_path, ExposedVarGroup* parent) {
    handle_ = parent->Add(rel_path,
                          [this] { return exposed_var::ToJsonValue(obj_); });
  }

 private:
  T obj_{};  // Value initialized.

  // Must be the last member. We need to remove ourselves from the tree before
  // destroying other state to prevent potential races.
  ExposedVarGroup::Handle handle_;
};

// Leaves in exported variable tree could also be `ExposedVarDynamic`. Value of
// this type of leaf is queried by calling `getter` provided by the user.
template <class T>
class ExposedVarDynamic {
 public:
  // `getter` must be thread-safe.
  //
  // @sa: `ExposedVarGroup::Add` for usage of slashes in `rel_path`.
  ExposedVarDynamic(
      std::string_view rel_path, Function<T()> getter,
      ExposedVarGroup* parent = ExposedVarGroup::FindOrCreate("/"))
      : getter_(std::move(getter)) {
    handle_ = parent->Add(
        rel_path, [this] { return exposed_var::ToJsonValue(getter_()); });
  }

 private:
  Function<T()> getter_;
  ExposedVarGroup::Handle handle_;
};

// This class is special in that it allows the user to create a tree of
// variables, in the same way what would be created using `ExposedVarGroup` +
// `ExposedVar(Dynamic)`, albeit in a inefficient way.
//
// Users is expected to return a JSON object that represents the tree.
//
// Note that even if what is wanted is only a part of the tree, the entire tree
// is generated (via `getter`).
//
// Consider this case:
//
// - The tree is registered via `ExposedVarDynamicTree("d/e", "a/b/c")`
// - `ExposedVarDynamicTree` generates: '{"x": {"y": {"z": 1}}}'
// - Value at `/a/b/c/d/e/x/y/z` is wanted.
//
// Although only value of `z` is required, since there's no way to only evaluate
// `z` (from the library's perspective), the entire tree at `/a/b/c/d/e/` is
// generated, and the library picks `x/y/z` and output it afterwards.
//
// The same WON'T happen if you registered `x/y/z` in `/a/b/c/d/e/` via
// `ExposedVar(Dynamic)`, and only `z` will be evaluated.
class ExposedVarDynamicTree {
 public:
  // `getter` must be thread-safe.
  //
  // @sa: `ExposedVarGroup::Add` for usage of slashes in `rel_path`.
  ExposedVarDynamicTree(
      std::string_view rel_path, Function<Json::Value()> getter,
      ExposedVarGroup* parent = ExposedVarGroup::FindOrCreate("/"));

  // Get value of a specific node.
  //
  // If `rel_path` refers to a JSON (sub-)object, that sub-object is returned.
  std::optional<Json::Value> TryGet(std::string_view rel_path) const;

 private:
  Function<Json::Value()> getter_;
  ExposedVarGroup::Handle handle_;
};

// FOR INTERNAL USE ONLY.
namespace detail {

template <class T>
struct IdentityTime {
  T operator()(const T& val) const { return val; }
};

template <class T>
struct TscToDuration {
  std::uint64_t operator()(const T& val) const {
    return DurationFromTsc(0, val) / std::chrono::nanoseconds(1);
  }
};

}  // namespace detail

template <class T, class F = detail::IdentityTime<T>>
class ExposedMetrics {
 public:
  // @sa `ExposedVarGroup::Add` for usage of slashes in `rel_path`.
  explicit ExposedMetrics(std::string_view rel_path) {
    LinkToParent(rel_path, ExposedVarGroup::FindOrCreate("/"));
  }

  // Getter. Users could update value in this object here.
  WriteMostlyMetrics<T>* operator->() noexcept { return &obj_; }
  WriteMostlyMetrics<T>& operator*() noexcept { return obj_; }

 private:
  Json::Value ToJsonValue(const WriteMostlyMetrics<T>& v) {
    Json::Value result;
    std::unordered_map<std::string, typename WriteMostlyMetrics<T>::Result> m =
        {{"1s", v.Get(1)},       {"1min", v.Get(60)}, {"10min", v.Get(600)},
         {"30min", v.Get(1800)}, {"1h", v.Get(3600)}, {"total", v.GetAll()}};
    for (auto&& [k, r] : m) {
      result[k]["avg"] = exposed_var::ToJsonValue(F()(r.average));
      result[k]["min"] = exposed_var::ToJsonValue(F()(r.min));
      result[k]["max"] = exposed_var::ToJsonValue(F()(r.max));
      result[k]["cnt"] = static_cast<Json::UInt64>(r.cnt);
    }
    return result;
  }

  void LinkToParent(std::string_view rel_path, ExposedVarGroup* parent) {
    handle_ = parent->Add(rel_path, [this] { return ToJsonValue(obj_); });
  }

 private:
  WriteMostlyMetrics<T> obj_{};  // Value initialized.

  // Must be the last member. We need to remove ourselves from the tree before
  // destroying other state to prevent potential races.
  ExposedVarGroup::Handle handle_;
};

template <class T>
using ExposedCounter = ExposedVar<WriteMostlyCounter<T>>;
template <class T>
using ExposedGauge = ExposedVar<WriteMostlyGauge<T>>;
template <class T>
using ExposedMaxer = ExposedVar<WriteMostlyMaxer<T>>;
template <class T>
using ExposedMiner = ExposedVar<WriteMostlyMiner<T>>;
template <class T>
using ExposedAverager = ExposedVar<WriteMostlyAverager<T>>;

namespace internal {

using ExposedMetricsInTsc =
    ExposedMetrics<std::uint64_t, flare::detail::TscToDuration<std::uint64_t>>;

}

}  // namespace flare

#endif  // FLARE_BASE_EXPOSED_VAR_H_
