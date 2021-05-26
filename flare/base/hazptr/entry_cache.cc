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

#include "flare/base/hazptr/entry_cache.h"

#include <array>

#include "flare/base/internal/annotation.h"
#include "flare/base/internal/logging.h"

namespace flare::hazptr {

inline constexpr std::size_t kEntryCacheSize = 8;

struct EntryCacheSlow {
  std::array<Entry*, kEntryCacheSize> cache;
  Entry** current = cache.data();
  Entry** top = cache.data() + cache.size();

  ~EntryCacheSlow() {
    while (current != cache.data()) {
      PutEntryOfDefaultDomain(*--current);
    }
  }
};

EntryCacheSlow* GetEntryCacheSlow() {
  FLARE_INTERNAL_TLS_MODEL thread_local EntryCacheSlow cache;
  return &cache;
}

void EntryCache::InitializeOnceCheck() {
  if (current == nullptr) {
    auto&& cache = GetEntryCacheSlow();
    FLARE_CHECK(top == nullptr);
    FLARE_CHECK(bottom == nullptr);
    current = cache->current;
    bottom = cache->cache.data();
    top = cache->cache.data() + cache->cache.size();
  }
}

Entry* EntryCache::GetSlow() noexcept {
  InitializeOnceCheck();
  return GetEntryOfDefaultDomain();
}

void EntryCache::PutSlow(Entry* entry) noexcept {
  InitializeOnceCheck();
  return PutEntryOfDefaultDomain(entry);
}

Entry* GetEntryOfDefaultDomain() noexcept {
  return GetDefaultHazptrDomain()->GetEntry();
}

void PutEntryOfDefaultDomain(Entry* entry) noexcept {
  GetDefaultHazptrDomain()->PutEntry(entry);
}

}  // namespace flare::hazptr
