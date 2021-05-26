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

#include "flare/base/exposed_var.h"

#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "flare/base/internal/logging.h"
#include "flare/base/string.h"

using namespace std::literals;

namespace flare {

namespace {

#define CHECK_PATH(path)                                   \
  FLARE_CHECK((path).size() <= 1 || (path).back() != '/',  \
              "Invalid path: [{}].", path);                \
  FLARE_CHECK((path).find("//") == std::string_view::npos, \
              "Invalid path: [{}].", path);

#define CHECK_RELATIVE_PATH(path)                                             \
  CHECK_PATH(path);                                                           \
  FLARE_CHECK((path).empty() || (path).front() != '/', "Invalid path: [{}].", \
              path)

#define CHECK_ABSOLUTE_PATH(path)                                             \
  CHECK_PATH(path);                                                           \
  FLARE_CHECK((path).empty() || (path).front() == '/', "Invalid path: [{}].", \
              path)

std::pair<std::string_view, std::string_view> SplitLastPart(
    std::string_view path) {
  auto pos = path.find_last_of('/');
  if (pos == std::string_view::npos) {
    return std::pair("", path);
  }
  return std::pair(path.substr(0, pos), path.substr(pos + 1));
}

std::pair<std::string_view, std::string_view> SplitFirstPart(
    std::string_view path) {
  auto pos = path.find_first_of('/');
  if (pos == std::string_view::npos) {
    return {path, ""};
  } else {
    return {path.substr(0, pos), path.substr(pos + 1)};
  }
}

std::string SubstituteEscapedSlashForZero(std::string_view path) {
  FLARE_CHECK(path.find('\0') == std::string_view::npos);
  return Replace(path, "\\/", "\0"s);
}

std::string SubstituteZeroForEscapedSlash(std::string_view path) {
  return Replace(path, "\0"s, "\\/");
}

std::string UnescapeZeroToPlainSlash(std::string_view path) {
  return Replace(path, "\0"s, "/");
}

std::string JoinPath(std::string_view a, std::string_view b) {
  if (EndsWith(b, "/")) {
    b.remove_suffix(1);
  }
  if (EndsWith(a, "/")) {
    a.remove_suffix(1);
  }
  if (b.empty()) {
    return std::string(a);
  }
  if (a.empty()) {
    return std::string(b);
  }
  return std::string(a) + "/" + std::string(b);
}

}  // namespace

ExposedVarGroup::Handle ExposedVarGroup::Add(std::string_view rel_path,
                                             Function<Json::Value()> value) {
  // "\\/" is stored as '\0' internally.
  auto real_path = SubstituteEscapedSlashForZero(rel_path);
  CHECK_RELATIVE_PATH(real_path);
  auto&& [path, name] = SplitLastPart(real_path);
  return CreateUpto(path)->AddDirect(
      name,
      [f = std::move(value), name = std::string(name)](
          std::string_view expected) -> std::optional<Json::Value> {
        auto jsv = f();
        if (expected.empty()) {
          return jsv;
        }
        auto real_path = SubstituteEscapedSlashForZero(expected);
        Json::Value* ptr = &jsv;
        auto pieces = Split(real_path, '/');
        for (auto&& e : pieces) {
          auto unescaped = UnescapeZeroToPlainSlash({e.data(), e.size()});
          if (ptr->isObject()) {
            if (ptr->isMember(unescaped)) {
              ptr = &(*ptr)[unescaped];  // Slow.
            } else {
              return {};
            }
          } else if (ptr->isArray()) {
            auto index = TryParse<std::size_t>(unescaped);
            if (index && *index < ptr->size()) {
              ptr = &(*ptr)[static_cast<Json::ArrayIndex>(*index)];
            } else {
              return {};
            }
          } else {
            return {};
          }
        }
        return *ptr;
      });
}

ExposedVarGroup::Handle ExposedVarGroup::Add(
    std::string_view rel_path, ExposedVarDynamicTree* dynamic_tree) {
  auto real_path = SubstituteEscapedSlashForZero(rel_path);
  CHECK_RELATIVE_PATH(real_path);
  auto&& [path, name] = SplitLastPart(real_path);
  return CreateUpto(path)->AddDirect(
      name, [dynamic_tree](std::string_view inner_path) {
        return dynamic_tree->TryGet(inner_path);
      });
}

ExposedVarGroup* ExposedVarGroup::FindOrCreate(std::string_view abs_path) {
  auto real_path = SubstituteEscapedSlashForZero(abs_path);
  CHECK_ABSOLUTE_PATH(real_path);
  // `substr(1) to remove '/'.
  return Root()->CreateUpto(real_path.substr(1));
}

std::optional<Json::Value> ExposedVarGroup::TryGet(std::string_view abs_path) {
  auto real_path = SubstituteEscapedSlashForZero(abs_path);
  FLARE_CHECK(!real_path.empty());
  CHECK_ABSOLUTE_PATH(real_path);
  if (real_path == "/") {  // Special case. Logic below cannot handle this.
    return Root()->Dump();
  }
  std::string_view left_path;
  // `substr(1)` to remove '/' at the beginning.
  auto rel_path = real_path.substr(1);
  auto parent = Root()->FindLowest(rel_path, &left_path);
  auto&& [name, rest] = SplitFirstPart(left_path);
  if (name.empty()) {
    return parent->Dump();
  }
  std::string s(name);
  std::shared_lock lk(parent->lock_);
  if (auto iter = parent->leaves_.find(s); iter != parent->leaves_.end()) {
    // It's a leaf. Dump it then.
    //
    // Before we pass the remaining path out, we have to restore escaped slashes
    // in it (we replaced them with '\0'.).
    return iter->second(SubstituteZeroForEscapedSlash(rest));
  } else {
    if (auto iter = parent->nodes_.find(s); iter != parent->nodes_.end()) {
      // It's a intermediate node, dump all its children.
      return iter->second->Dump();
    } else {
      return {};  // Not found.
    }
  }
}

ExposedVarGroup::ExposedVarGroup(std::string abs_path)
    : abs_path_(std::move(abs_path)) {
  CHECK_ABSOLUTE_PATH(abs_path);
}

ExposedVarGroup* ExposedVarGroup::Root() {
  static ExposedVarGroup evg("/");
  return &evg;
}

const std::string& ExposedVarGroup::AbsolutePath() const { return abs_path_; }

ExposedVarGroup* ExposedVarGroup::FindLowest(std::string_view rel_path,
                                             std::string_view* left) {
  CHECK_RELATIVE_PATH(rel_path);
  auto current = this;
  while (!rel_path.empty()) {
    std::scoped_lock lk(current->lock_);
    auto&& [name, rest] = SplitFirstPart(rel_path);
    auto iter = current->nodes_.find(std::string(name));
    if (iter == current->nodes_.end()) {
      break;
    } else {
      current = &*iter->second;
    }
    rel_path = rest;
  }
  if (left) {
    *left = rel_path;
  }
  return current;
}

ExposedVarGroup* ExposedVarGroup::CreateUpto(std::string_view rel_path) {
  CHECK_RELATIVE_PATH(rel_path);
  std::string_view left_path;
  auto current = FindLowest(rel_path, &left_path);

  // The rest pieces.
  auto pieces = Split(left_path, '/');

  for (auto&& e : pieces) {
    std::scoped_lock lk(current->lock_);
    auto s = std::string(e);
    auto p = JoinPath(current->AbsolutePath(), s);

    // Check if there's already a leaf with the same name. (Only meaningful
    // for the first loop round.)
    FLARE_CHECK(
        current->leaves_.find(s) == current->leaves_.end(),
        "Path [{}] has already been used: A value is registered at [{}].",
        rel_path, p);
    // Otherwise there's a bug in `FindLowest`.
    FLARE_CHECK(current->nodes_.find(s) == current->nodes_.end());
    // `std::make_unique` won't work here as the ctor is private.
    auto evg = std::unique_ptr<ExposedVarGroup>(new ExposedVarGroup(p));
    current = &*(current->nodes_[s] = std::move(evg));
  }
  FLARE_CHECK(EndsWith(current->AbsolutePath(), rel_path), "[{}] vs [{}]",
              current->AbsolutePath(), rel_path);
  return current;
}

ExposedVarGroup::Handle ExposedVarGroup::AddDirect(std::string_view name,
                                                   Getter value) {
  std::scoped_lock lk(lock_);
  std::string s(name);
  FLARE_CHECK(leaves_.find(s) == leaves_.end(),
              "Value [{}] has already been registered at [{}].", name,
              AbsolutePath());
  FLARE_CHECK(nodes_.find(s) == nodes_.end(),
              "Path [{}] has already been used.",
              JoinPath(AbsolutePath(), name));
  leaves_[s] = std::move(value);
  return Deferred([this, s] {
    std::scoped_lock lk(lock_);
    FLARE_CHECK_EQ(leaves_.erase(s), 1);
  });
}

Json::Value ExposedVarGroup::Dump() const {
  std::shared_lock lk(lock_);
  Json::Value jsv;
  for (auto&& [k, v] : nodes_) {
    jsv[UnescapeZeroToPlainSlash(k)] = v->Dump();
  }
  for (auto&& [k, v] : leaves_) {
    jsv[UnescapeZeroToPlainSlash(k)] = *v("");
  }
  return jsv;
}

ExposedVarDynamicTree::ExposedVarDynamicTree(std::string_view rel_path,
                                             Function<Json::Value()> getter,
                                             ExposedVarGroup* parent)
    : getter_(std::move(getter)) {
  handle_ = parent->Add(rel_path, this);
}

std::optional<Json::Value> ExposedVarDynamicTree::TryGet(
    std::string_view rel_path) const {
  auto real_path = SubstituteEscapedSlashForZero(rel_path);
  auto jsv = getter_();
  Json::Value* ptr = &jsv;
  auto pieces = Split(real_path, '/');
  for (auto&& e : pieces) {
    // We need to unescape "\0"s before traversing the tree.
    auto unescaped = UnescapeZeroToPlainSlash({e.data(), e.size()});
    ptr = &(*ptr)[unescaped];  // Slow.
  }
  if (ptr->isNull()) {
    return {};
  }
  return *ptr;
}

}  // namespace flare
