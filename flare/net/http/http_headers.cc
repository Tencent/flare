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

#include "flare/net/http/http_headers.h"

#include <algorithm>
#include <forward_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace flare {

HttpHeaders::HttpHeaders(const HttpHeaders& other) {
  for (auto&& [k, v] : other) {
    Append(std::string(k), std::string(v));
  }
}

HttpHeaders& HttpHeaders::operator=(const HttpHeaders& other) {
  if (FLARE_UNLIKELY(&other == this)) {
    return *this;
  }
  this->~HttpHeaders();
  new (this) HttpHeaders(other);
  return *this;
}

void HttpHeaders::clear() noexcept {
  header_block_.first.reset();
  header_block_.second = 0;
  owning_strs_2_.clear();
  header_idx_.clear();
  fields_.clear();
}

std::vector<std::string_view> HttpHeaders::TryGetMultiple(
    const std::string_view& key) const noexcept {
  std::vector<std::string_view> values;
  for (auto&& [k, v] : fields_) {
    // I really shouldn't have done this.
    if (internal::detail::hash_map::CaseInsensitiveEqualTo()(k, key)) {
      values.push_back(v);
    }
  }
  return values;
}

void HttpHeaders::Set(std::string key, std::string value) {
  FLARE_CHECK_EQ(Trim(key), key,
                 "Field key may not be surrounded by whitespaces.");
  // To be precise, surrounding `value` with whitespace is not an error, it
  // just does not make much sense.
  //
  // FIXME: Should we trim `value` automatically?
  FLARE_CHECK_EQ(Trim(value), value,
                 "There's hardly any point in surrounding field value with "
                 "whitespaces.");

  auto idx_opt = header_idx_.TryGet(key);
  if (!idx_opt) {
    Append(std::move(key), std::move(value));
  } else {
    // We don't remove the old value in owning_strs_2_ because it costs.
    fields_[*idx_opt].second = owning_strs_2_.emplace_back(std::move(value));
  }
}

void HttpHeaders::Append(std::string key, std::string value) {
  FLARE_CHECK_EQ(Trim(key), key,
                 "Field key may not be surrounded by whitespaces.");
  // To be precise, surrounding `value` with whitespace is not an error, it
  // just does not make much sense.
  //
  // FIXME: Should we trim `value` automatically?
  FLARE_CHECK_EQ(Trim(value), value,
                 "There's hardly any point in surrounding field value with "
                 "whitespaces.");
  auto&& k = owning_strs_2_.emplace_back(std::move(key));
  auto&& v = owning_strs_2_.emplace_back(std::move(value));
  if (!header_idx_.contains(k)) {
    header_idx_[k] = fields_.size();
  }
  fields_.emplace_back(k, v);
}

void HttpHeaders::Append(
    const std::initializer_list<std::pair<std::string_view, std::string_view>>&
        fields) {
  for (auto&& [k, v] : fields) {
    Append(std::string(k), std::string(v));
  }
}

bool HttpHeaders::Remove(const std::string_view& key) noexcept {
  if (!TryGet(key)) {
    return false;
  }

  header_idx_.clear();
  std::size_t i = 0;
  auto it = fields_.begin();
  while (it != fields_.end()) {
    if (internal::detail::hash_map::CaseInsensitiveEqualTo()(it->first, key)) {
      it = fields_.erase(it);
    } else {
      if (!header_idx_.contains(it->first)) {
        header_idx_[it->first] = i;
      }
      ++i;
      ++it;
    }
  }

  // We don't remove the key/value in owning_strs_2_ because it costs.
  return true;
}

std::string HttpHeaders::ToString() const {
  std::string result;

  // The implementation performs even worse than our own implementation (which
  // uses `StringAppend` to optimize). However, since we're providing this
  // method only for debugging purpose, this shouldn't hurt.
  for (auto&& [k, v] : fields_) {
    result += k;
    result += ": ";
    result += v;
    result += "\r\n";
  }
  return result;
}

std::string_view HttpHeaders::RetrieveHeaderStorage(
    std::pair<std::unique_ptr<char[]>, std::size_t>&& s) {
  owning_strs_2_.clear();
  header_idx_.clear();
  header_block_ = std::move(s);
  return std::string_view(header_block_.first.get(), header_block_.second);
}

void HttpHeaders::RetrieveFields(NonowningFields&& fields) {
  fields_ = std::move(fields);
  for (std::size_t index = 0; index != fields_.size(); ++index) {
    auto&& key = fields_[index].first;
    // For duplicate fields, only the first is indexed. The rest (along with the
    // first) can be read by `TryGetMultiple`.
    if (!header_idx_.contains(key)) {
      header_idx_[key] = index;
    }
  }
}

}  // namespace flare
