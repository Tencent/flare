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

#include "flare/net/cos/cos_channel.h"

#include <chrono>
#include <string>

#include "flare/base/enum.h"
#include "flare/base/status.h"
#include "flare/base/string.h"
#include "flare/net/cos/cos_status.h"
#include "flare/net/internal/http_engine.h"
#include "flare/net/internal/http_task.h"

using namespace std::literals;

namespace flare::cos {

bool CosChannel::OpenPolaris(const std::string& polaris_addr) {
  dispatcher_ = message_dispatcher_registry.New("polaris");
  return dispatcher_->Open(polaris_addr);
}

void CosChannel::Perform(const cos::Channel* self, const CosOperation& op,
                         CosOperationResult* result,
                         const CosTask::Options& options,
                         std::chrono::nanoseconds timeout,
                         Function<void(Status)>* done) {
  struct LowLevelContext {
    CosOperationResult* result;
    Function<void(Status)> done;
    ErasedPtr context;
    std::uintptr_t nslb_ctx;
    Endpoint access_point;
    std::chrono::steady_clock::time_point start = flare::ReadSteadyClock();
  };

  auto ctx = std::make_unique<LowLevelContext>();
  ctx->result = result;
  ctx->done = std::move(*done);

  cos::CosTask task(&options);
  if (!op.PrepareTask(&task, &ctx->context)) {
    ctx->done(Status(CosStatus::InvalidArguments));
    return;
  }
  if (dispatcher_) {
    if (!dispatcher_->GetPeer(0, &ctx->access_point, &ctx->nslb_ctx)) {
      ctx->done(Status(CosStatus::AddressResolutionFailure));
      return;
    }
    task.OverrideAccessPoint(ctx->access_point);
  }

  auto cb = [this, ctx = std::move(ctx)](auto completion) mutable {
    if (dispatcher_) {
      dispatcher_->Report(ctx->access_point,
                          completion ? MessageDispatcher::Status::Success
                                     : MessageDispatcher::Status::Failed,
                          flare::ReadSteadyClock() - ctx->start, ctx->nslb_ctx);
    }
    if (!completion) {
      ctx->done(
          Status(completion.error().code() == CURLE_OPERATION_TIMEDOUT
                     ? CosStatus::Timeout
                     : CosStatus::HttpError,
                 Format("Http error: {}", completion.error().ToString())));
      return;
    }
    if (underlying_value(completion->status()) < 200 ||
        underlying_value(completion->status()) >= 300) {
      ctx->done(ParseCosStatus(completion->status(),
                               flare::FlattenSlow(*completion->body())));
      return;
    }
    if (!ctx->result->ParseResult(CosTaskCompletion(std::move(*completion)),
                                  std::move(ctx->context))) {
      ctx->done(Status(CosStatus::MalformedResponse));
      return;
    }
    ctx->done(Status());
  };
  auto http_task = task.BuildTask();
  http_task.SetTimeout(timeout);
  internal::HttpEngine::Instance()->StartTask(std::move(http_task),
                                              std::move(cb));
}

}  // namespace flare::cos
