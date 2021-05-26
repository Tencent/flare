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

#ifndef FLARE_NET_INTERNAL_HTTP_ENGINE_H_
#define FLARE_NET_INTERNAL_HTTP_ENGINE_H_

#include <chrono>
#include <string>

#include "thirdparty/curl/curl.h"

#include "flare/base/expected.h"
#include "flare/base/function.h"
#include "flare/base/never_destroyed.h"
#include "flare/base/status.h"
#include "flare/net/internal/http_task.h"

namespace flare::internal {

// Call in fiber context.
class HttpEngine {
 public:
  static HttpEngine* Instance();

  void StartTask(HttpTask task,
                 Function<void(Expected<HttpTaskCompletion, Status>)> done);

  // This is called by `flare::Start()` (as the
  // initialization cannot be done before flare runtime start) and may not be
  // called by users.
  static void Stop();
  static void Join();

 private:
  friend class NeverDestroyedSingleton<HttpEngine>;
  HttpEngine();
};

}  // namespace flare::internal

#endif  // FLARE_NET_INTERNAL_HTTP_ENGINE_H_
