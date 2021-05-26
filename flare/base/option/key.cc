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

#include "flare/base/option/key.h"

#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

#include "flare/base/internal/hash_map.h"
#include "flare/base/string.h"

namespace flare::option {

namespace {

std::shared_mutex dynamic_key_store_lock;
internal::HashMap<std::string, std::string> dynamic_key_store;

}  // namespace

Key::Key(std::string name) : Key(FixedKey(std::move(name))) {}

std::string Key::Get() const { return impl_->Get(); }

bool Key::Ready() const noexcept { return impl_->Ready(); }

MultiKey::MultiKey(MultiKey&& first, Key&& second)
    : keys_(std::move(first.keys_)) {
  keys_.push_back(std::move(second));
}

MultiKey::MultiKey(MultiKey&& first, MultiKey&& second)
    : keys_(std::move(first.keys_)) {
  for (auto&& e : second.keys_) {
    keys_.push_back(std::move(e));
  }
  second.keys_.clear();
}

MultiKey::MultiKey(std::string name) : MultiKey(Key(std::move(name))) {}

MultiKey::MultiKey(const char* name) : MultiKey(std::string(name)) {}

const std::vector<Key>& MultiKey::GetKeys() const noexcept { return keys_; }

bool MultiKey::Ready() const noexcept {
  return std::all_of(keys_.begin(), keys_.end(),
                     [](auto&& e) { return e.Ready(); });
}

std::string MultiKey::ToString() const {
  std::vector<std::string> strs;
  for (auto&& e : keys_) {
    strs.push_back(e.Get());
  }
  return Join(strs, "/");
}

std::string DynamicKey::Get() const {
  std::shared_lock _(dynamic_key_store_lock);
  if (auto p = dynamic_key_store.TryGet(name_)) {
    return *p;
  } else {
    return "(not ready yet.)";
  }
}

bool DynamicKey::Ready() const noexcept {
  std::shared_lock _(dynamic_key_store_lock);
  return !!dynamic_key_store.TryGet(name_);
}

void SetDynamicKey(const std::string& name, std::string value) {
  std::scoped_lock _(dynamic_key_store_lock);
  dynamic_key_store[name] = value;
}

}  // namespace flare::option
