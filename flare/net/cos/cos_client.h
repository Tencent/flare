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

#ifndef FLARE_NET_COS_COS_CLIENT_H_
#define FLARE_NET_COS_COS_CLIENT_H_

#include <chrono>
#include <string>

#include "gflags/gflags_declare.h"

#include "flare/base/expected.h"
#include "flare/base/future.h"
#include "flare/base/internal/time_view.h"
#include "flare/base/maybe_owning.h"
#include "flare/base/status.h"
#include "flare/fiber/future.h"
#include "flare/net/cos/channel.h"
#include "flare/net/cos/cos_status.h"
#include "flare/net/cos/ops/operation.h"

DECLARE_int32(flare_cos_client_default_timeout_ms);

namespace flare {

// This class helps you interacting with COS provided by Tencent Cloud.
class CosClient {
 public:
  struct Options {
    // Credential for accessing COS.
    std::string secret_id, secret_key;

    // Default bucket name. This can be overridden by setting it explicit on the
    // operation you perform.
    //
    // e.g., `examplebucket-1250000000` (appid included.).
    std::string bucket;

    // Effective only if no timeout is set explicitly when performing operation.
    std::chrono::nanoseconds timeout =
        std::chrono::milliseconds(FLAGS_flare_cos_client_default_timeout_ms);
  };

  // Initialize this client.
  //
  // Acceptable `uri`:
  //
  // - cos://ap-guangzhou: Using COS server in region `ap-guangzhou`.
  // - mock://...: Mostly used in UT, for mocking COS result.
  bool Open(const std::string& uri, const Options& options);

  // Perform `op` and wait for its completion.
  //
  // @sa: `flare/net/cos/ops/...` for supported operations and their
  // corresponding resulting type.
  template <class T>
  Expected<cos::cos_result_t<T>, Status> Execute(
      const T& op, internal::NanosecondsView timeout = {}) const;

  // Perform `op` asynchronously.
  template <class T>
  Future<Expected<cos::cos_result_t<T>, Status>> AsyncExecute(
      const T& op, internal::NanosecondsView timeout = {}) const;

  // FOR INTERNAL USE ONLY.
  static void RegisterMockChannel(cos::Channel* channel);

 private:
  Options options_;
  cos::CosTask::Options task_opts_;
  MaybeOwning<cos::Channel> channel_;
};

////////////////////////////////////////
// Implementation goes below.         //
////////////////////////////////////////

template <class T>
Expected<cos::cos_result_t<T>, Status> CosClient::Execute(
    const T& op, internal::NanosecondsView timeout) const {
  return fiber::BlockingGet(AsyncExecute(op, timeout));
}

template <class T>
Future<Expected<cos::cos_result_t<T>, Status>> CosClient::AsyncExecute(
    const T& op, internal::NanosecondsView timeout) const {
  if (!channel_) {
    return Status(CosStatus::NotOpened, "COS client has not been opened yet.");
  }
  Promise<Expected<cos::cos_result_t<T>, Status>> promise;
  auto result = std::make_unique<cos::cos_result_t<T>>();
  auto result_ptr = result.get();
  auto future = promise.GetFuture();
  Function<void(Status)> done =
      [result = std::move(result),
       promise = std::move(promise)](Status status) mutable {
        if (status.ok()) {
          promise.SetValue(std::move(*result));
        } else {
          promise.SetValue(std::move(status));
        }
      };
  channel_->Perform(nullptr, op, result_ptr, task_opts_,
                    timeout.Get().count() ? timeout.Get() : options_.timeout,
                    &done);
  return future;
}

}  // namespace flare

#endif  // FLARE_NET_COS_COS_CLIENT_H_
