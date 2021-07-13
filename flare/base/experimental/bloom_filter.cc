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

#include "flare/base/experimental/bloom_filter.h"

#include "xxhash/xxhash.h"

namespace flare::experimental::bloom_filter::detail {

std::size_t Hash::operator()(std::string_view s) const noexcept {
  return XXH64(s.data(), s.size(), 0);
}

}  // namespace flare::experimental::bloom_filter::detail
