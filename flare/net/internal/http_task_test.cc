// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/net/internal/http_task.h"

#include <memory>
#include <utility>

#include "gtest/gtest.h"

#include "flare/testing/main.h"

namespace flare::internal {

TEST(HttpTask, All) {
  HttpTask task;
  task.AddHeader("a:b");
  auto task2 = std::move(task);

  // Nothing to check. This UT indirectly tests there's no leak (via
  // heap-checker), no crash would occur.
}

TEST(HttpTaskCompletion, All) {
  auto ptr = object_pool::Get<HttpTaskCallContext>();
  ptr->body = std::make_unique<flare::NoncontiguousBufferBuilder>();
  HttpTaskCompletion comp(ptr.Leak());
  auto comp2 = std::move(comp);
  // No leak, no crash.
}

}  // namespace flare::internal

FLARE_TEST_MAIN
