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

#ifndef FLARE_BASE_HAZPTR_HAZPTR_DOMAIN_H_
#define FLARE_BASE_HAZPTR_HAZPTR_DOMAIN_H_

#include <atomic>
#include <cstdint>
#include <unordered_set>

#include "flare/base/never_destroyed.h"

namespace flare {

namespace hazptr {

struct Entry;
class Object;

}  // namespace hazptr

// For resource reclamation, different domains are handled differently. That
// is, if you hold a `Hazptr` belonging to domain 1, it adds no overhead to
// resource reclamation occurring in domain 2.
//
// Note that, however, for default domain, a dedicated thread-local cache is
// provided for allocating internal data structures to speed things up.
// Therefore, IF YOU INSIST USING A NON-DEFAULT DOMAIN, YOU'LL LIKELY SEE
// PERFORMANCE DEGRADATION RATHER THAN PERFORMANCE BOOST.
//
// Note that for hazard pointer to work correctly, all reader and the writer (at
// most one at a time) must be working with the same domain, otherwise the
// behavior is undefined.
//
// ALL METHODS (EXCEPT FOR CTOR. / DTOR.) ARE FOR INTERNAL USE ONLY.
class HazptrDomain {
 public:
  HazptrDomain();
  ~HazptrDomain();

  // Acquire / release hazard pointer.
  hazptr::Entry* GetEntry();
  void PutEntry(hazptr::Entry* entry);

  // Retire object for reclamation.
  void Retire(hazptr::Object* object);

 private:
  hazptr::Entry* GetEntrySlow();

  // Chain `object` into `retired_` without triggering reclamation.
  void PushRetired(hazptr::Object* object);

  // Reclaim all retired objects that is not referenced anywhere.
  void ReclaimBestEffort();

  // Collect all pointers kept alive by some `Hazptr`.
  std::unordered_set<const hazptr::Object*> GetKeptPointers();

 private:
  // We set a timer so as to periodically call `ReclaimBestEffort()`, otherwise
  // retired list is only checked on `Retire`, if it's not called often enough,
  // we may end up with some stale objects alive.
  std::uint64_t timer_id_;

  // Entries in `hazptrs_` are never freed. This simplified list traversal.
  std::atomic<hazptr::Entry*> hazptrs_{nullptr};
  // Objects to be reclaimed.
  std::atomic<hazptr::Object*> retired_{nullptr};
};

// For `Hazptr`s that are constructedi without specifying a domain explicitly,
// they're associated with the default one.
inline HazptrDomain* GetDefaultHazptrDomain() {
  static NeverDestroyed<HazptrDomain> domain;
  return domain.Get();
}

}  // namespace flare

#endif  // FLARE_BASE_HAZPTR_HAZPTR_DOMAIN_H_
