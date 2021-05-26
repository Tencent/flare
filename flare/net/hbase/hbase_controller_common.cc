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

#include "flare/net/hbase/hbase_controller_common.h"

#include <chrono>
#include <string>
#include <utility>

#include "flare/net/hbase/proto/constants.h"

using namespace std::literals;

namespace flare::hbase {

constexpr auto kDefaultTimeout = 2s;

HbaseControllerCommon::HbaseControllerCommon() { Reset(); }

// Set / get exception.
void HbaseControllerCommon::SetException(HbaseException exception) {
  FLARE_CHECK(!exception.exception_class_name().empty(),
              "`HbaseException.exception_class_name` must be set.");
  exception_ = std::move(exception);
}

const HbaseException& HbaseControllerCommon::GetException() const {
  return exception_;
}

bool HbaseControllerCommon::Failed() const {
  return exception_.has_exception_class_name();
}

std::string HbaseControllerCommon::ErrorText() const {
  return exception_.exception_class_name();
}

void HbaseControllerCommon::SetTimeout(internal::SteadyClockView timeout) {
  timeout_ = timeout.Get();
}

std::chrono::steady_clock::time_point HbaseControllerCommon::GetTimeout()
    const {
  return timeout_;
}

const Endpoint& HbaseControllerCommon::GetRemotePeer() const {
  return remote_peer_;
}

std::chrono::nanoseconds HbaseControllerCommon::GetElapsedTime() const {
  return ReadSteadyClock() - last_reset_;
}

void HbaseControllerCommon::Reset() {
  timeout_ = ReadSteadyClock() + kDefaultTimeout;
  last_reset_ = ReadSteadyClock();
  exception_.Clear();
  request_cell_block_.Clear();
  response_cell_block_.Clear();
  remote_peer_ = Endpoint();
}

void HbaseControllerCommon::SetRemotePeer(const Endpoint& remote_peer) {
  remote_peer_ = remote_peer;
}

void HbaseControllerCommon::SetRequestCellBlock(
    NoncontiguousBuffer cell_block) {
  request_cell_block_ = std::move(cell_block);
}

const NoncontiguousBuffer& HbaseControllerCommon::GetRequestCellBlock() const {
  return request_cell_block_;
}

void HbaseControllerCommon::SetResponseCellBlock(
    NoncontiguousBuffer cell_block) {
  response_cell_block_ = std::move(cell_block);
}

const NoncontiguousBuffer& HbaseControllerCommon::GetResponseCellBlock() const {
  return response_cell_block_;
}

void HbaseControllerCommon::SetFailed(const std::string& what) {
  FLARE_LOG_ERROR_EVERY_SECOND(
      "DEPRECATED: `SetFailed` is deprecated, use `SetException` instead.");
  HbaseException xcpt;
  xcpt.set_exception_class_name(constants::kUnknownServiceException);
  SetException(std::move(xcpt));
}

void HbaseControllerCommon::StartCancel() {
  FLARE_CHECK(0, "HBase RPC cancellation is not supported yet.");
}

bool HbaseControllerCommon::IsCanceled() const {
  FLARE_CHECK(0, "HBase RPC cancellation is not supported yet.");
}

void HbaseControllerCommon::NotifyOnCancel(
    google::protobuf::Closure* callback) {
  FLARE_CHECK(0, "HBase RPC cancellation is not supported yet.");
}

}  // namespace flare::hbase
