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

#include "flare/fiber/execution_context.h"

#include "flare/base/object_pool/ref_counted.h"

namespace flare::fiber {

FiberLocal<ExecutionContext*> ExecutionContext::current_;

void ExecutionContext::Clear() {
  FLARE_CHECK_EQ(
      UnsafeRefCount(), 1,
      "Unexpected: `ExecutionContext` is using by others when `Clear()`-ed.");

  for (auto&& e : inline_els_) {
    // This is ugly TBH.
    e.~ElsEntry();
    new (&e) ElsEntry();
  }
  external_els_.clear();
}

RefPtr<ExecutionContext> ExecutionContext::Capture() {
  return RefPtr(ref_ptr, *current_);
}

RefPtr<ExecutionContext> ExecutionContext::Create() {
  return object_pool::GetRefCounted<ExecutionContext>();
}

ExecutionContext::ElsEntry* ExecutionContext::GetElsEntrySlow(
    std::size_t slot_index) {
  FLARE_LOG_WARNING_ONCE(
      "Excessive ELS usage. Performance will likely degrade.");
  // TODO(luobogao): Optimize this.
  std::scoped_lock _(external_els_lock_);
  return &external_els_[slot_index];
}

}  // namespace flare::fiber
