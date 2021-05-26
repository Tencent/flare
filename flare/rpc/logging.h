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

#ifndef FLARE_RPC_LOGGING_H_
#define FLARE_RPC_LOGGING_H_

#include <string>
#include <string_view>

#include "flare/fiber/logging.h"

namespace flare {

// Add a prefix prepended to _every_ log written during handling this RPC.
//
// This method may only be called during handling RPC. Calling it outside is
// undefined.
//
// Usage:
//
// void FancyService::SaveTheWorld(const SaveRequest& req, ...) {
//   flare::AddLoggingItemToRpc("item");
//   flare::AddLoggingTagToRpc("world_id", 123);
//
//   // Writes:
//   // Ixxxx hh:mm:ss XXXXX path/to/file.cc] [item] [world_id: 123] hi there.
//   FLARE_LOG_INFO("hi there.");
// }
//
inline void AddLoggingItemToRpc(const std::string& s) {
  return fiber::AddLoggingItemToExecution(s);
}

template <class T>
void AddLoggingTagToRpc(const std::string_view& key, const T& value) {
  return fiber::AddLoggingTagToExecution(key, value);
}

}  // namespace flare

#endif  // FLARE_RPC_LOGGING_H_
