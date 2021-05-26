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

#ifndef FLARE_BASE_HAZPTR_ENTRY_CACHE_H_
#define FLARE_BASE_HAZPTR_ENTRY_CACHE_H_

#include "flare/base/hazptr/entry.h"
#include "flare/base/hazptr/hazptr_domain.h"
#include "flare/base/internal/annotation.h"
#include "flare/base/internal/logging.h"
#include "flare/base/likely.h"

namespace flare::hazptr {

Entry* GetEntryOfDefaultDomain() noexcept;
void PutEntryOfDefaultDomain(Entry* entry) noexcept;

// Placeholder to mark the default domain. Recognized by this file only.
//
// The reason why we do not use `GetDefaultHazptrDomain()` is for perf. reasons,
// calling that method incurs some unnecessary overhead in fast path.
inline HazptrDomain* const kDefaultDomainPlaceholder =
    reinterpret_cast<HazptrDomain*>(1);

// Allocating `Entry` from `HazptrDomain` incurs some overhead, so we keep some
// `Entry` locally to speed up allocation (in trade of slower reclamation path.)
//
// Here we initialize all fields to `nullptr`, which would cause both `Get()`
// and `Put()` to fall to slow path (as desired.). Besides, using `nullptr` here
// permits static initialization of TLS.
struct EntryCache {
  // Note that `nullptr` would cause both `Get()` and `Put()` to fall to slow
  // path, which is desired.
  Entry** current = nullptr;
  Entry** bottom = nullptr;
  Entry** top = nullptr;

  Entry* Get() noexcept {
    if (FLARE_LIKELY(current > bottom)) {
      return *--current;
    }
    return GetSlow();
  }

  void Put(Entry* entry) noexcept {
    if (FLARE_LIKELY(current < top)) {
      *current++ = entry;
      return;
    }
    PutSlow(entry);
  }

 private:
  void InitializeOnceCheck();

  Entry* GetSlow() noexcept;
  void PutSlow(Entry* entry) noexcept;
};

inline EntryCache* GetEntryCacheOfDefaultDomain() noexcept {
  FLARE_INTERNAL_TLS_MODEL thread_local EntryCache entry_cache;
  return &entry_cache;
}

// For default domain, this method tries thread-local cache first, and fallback
// to allocating from the `domain` if either the requested domain is not the
// default one, or the cache is empty.
inline Entry* GetEntryOf(HazptrDomain* domain) noexcept {
  if (FLARE_LIKELY(domain == kDefaultDomainPlaceholder)) {
    return GetEntryCacheOfDefaultDomain()->Get();
  }
  return domain->GetEntry();
}

inline void PutEntryOf(HazptrDomain* domain, Entry* entry) noexcept {
  FLARE_DCHECK(entry->TryGetPtr() == nullptr);
  if (FLARE_LIKELY(domain == kDefaultDomainPlaceholder)) {
    return GetEntryCacheOfDefaultDomain()->Put(entry);
  }
  return domain->PutEntry(entry);
}

}  // namespace flare::hazptr

#endif  // FLARE_BASE_HAZPTR_ENTRY_CACHE_H_
