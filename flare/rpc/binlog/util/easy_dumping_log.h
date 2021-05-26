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

#ifndef FLARE_RPC_BINLOG_UTIL_EASY_DUMPING_LOG_H_
#define FLARE_RPC_BINLOG_UTIL_EASY_DUMPING_LOG_H_

#include <list>
#include <mutex>

#include "flare/rpc/binlog/dumper.h"

namespace flare::binlog {

// This class helps you implementing `DumpingLog` in an easier way.
template <class TIncoming, class TOutgoing = TIncoming>
class EasyDumpingLog : public DumpingLog {
 public:
  DumpingCall* GetIncomingCall() override { return &incoming_call_; }
  DumpingCall* StartOutgoingCall() override {
    std::scoped_lock _(lock_);
    return &outgoing_calls_.emplace_back();
  }

 protected:
  TIncoming incoming_call_;
  std::mutex lock_;
  std::list<TOutgoing> outgoing_calls_;
};

}  // namespace flare::binlog

#endif  // FLARE_RPC_BINLOG_UTIL_EASY_DUMPING_LOG_H_
