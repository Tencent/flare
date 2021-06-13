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

#ifndef FLARE_NET_HBASE_HBASE_CONTROLLER_COMMON_H_
#define FLARE_NET_HBASE_HBASE_CONTROLLER_COMMON_H_

#include <chrono>
#include <string>

#include "googletest/gtest/gtest_prod.h"
#include "google/protobuf/service.h"

#include "flare/base/buffer.h"
#include "flare/base/internal/time_view.h"
#include "flare/base/net/endpoint.h"
#include "flare/net/hbase/proto/rpc.pb.h"

namespace flare {

using HbaseException = hbase::ExceptionResponse;

// FIXME: How should we introduce HBase exception class names into `flare::`?

}  // namespace flare

namespace flare::testing::detail {

struct HbaseControllerMaster;

}

namespace flare::hbase {

// Implements some common facilities shared by `HbaseServerController` and
// `HbaseClientController`.
//
// TODO(luobogao): This class shares a lot common with `RpcControllerCommon`, we
// might want to refactor them.
class HbaseControllerCommon : public google::protobuf::RpcController {
 public:
  HbaseControllerCommon();

  // Set / get exception.
  void SetException(HbaseException exception);
  const HbaseException& GetException() const;

  // Get what's set by `SetException`.
  bool Failed() const override;
  std::string ErrorText() const override;

  // Set & get timeout.
  //
  // For server side, this method returns the timeout specified in the request,
  // for client-side, it's specified by the user.
  //
  // Both time point (of whatever clock type) and duration are accepted by
  // `SetTimeout`.
  void SetTimeout(internal::SteadyClockView timeout);
  std::chrono::steady_clock::time_point GetTimeout() const;

  // Get remote peer's address.
  const Endpoint& GetRemotePeer() const;

  // Get time elapsed since construction or last `Reset()` of this controller.
  std::chrono::nanoseconds GetElapsedTime() const;

  // Reset the controller.
  void Reset() override;

 protected:
  FRIEND_TEST(HbaseControllerCommon, RemotePeer);
  FRIEND_TEST(HbaseControllerCommon, RequestCellBlock);
  FRIEND_TEST(HbaseControllerCommon, ResponseCellBlock);
  friend struct testing::detail::HbaseControllerMaster;

  // Set local & remote peer address.
  void SetRemotePeer(const Endpoint& remote_peer);
  // void SetLocalPeer(Endpoint local_peer);

  // Cell block sent by the client.
  void SetRequestCellBlock(NoncontiguousBuffer cell_block);
  const NoncontiguousBuffer& GetRequestCellBlock() const;
  // NoncontiguousBuffer& GetRequestCellBlock(); ?

  // Cell block returned by the server.
  void SetResponseCellBlock(NoncontiguousBuffer cell_block);
  const NoncontiguousBuffer& GetResponseCellBlock() const;

 private:
  // DEPRECATED. Use `SetException` instead.
  void SetFailed(const std::string& what) override;

  // Cancellation is not implemented yet.
  void StartCancel() override;
  bool IsCanceled() const override;
  void NotifyOnCancel(google::protobuf::Closure* callback) override;

 private:
  std::chrono::steady_clock::time_point timeout_;
  std::chrono::steady_clock::time_point last_reset_;  // @sa: `GetElapsedTime()`
  HbaseException exception_;
  NoncontiguousBuffer request_cell_block_;
  NoncontiguousBuffer response_cell_block_;
  Endpoint remote_peer_;
};

}  // namespace flare::hbase

#endif  // FLARE_NET_HBASE_HBASE_CONTROLLER_COMMON_H_
