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

#ifndef FLARE_NET_HBASE_HBASE_CLIENT_CONTROLLER_H_
#define FLARE_NET_HBASE_HBASE_CLIENT_CONTROLLER_H_

#include "flare/net/hbase/call_context.h"
#include "flare/net/hbase/hbase_controller_common.h"

namespace flare {

namespace rpc::detail {

class StreamCallGateHandle;

}  // namespace rpc::detail

// RPC controller for HBase client.
class HbaseClientController : public hbase::HbaseControllerCommon {
 public:
  void SetPriority(int priority);
  int GetPriority() const;

  // Accessor for cell-block.
  using HbaseControllerCommon::GetRequestCellBlock;
  using HbaseControllerCommon::GetResponseCellBlock;
  using HbaseControllerCommon::SetRequestCellBlock;

  void Reset() override;

 private:
  friend class HbaseChannel;

  hbase::ProactiveCallContext* GetCallContext();

 private:
  hbase::ProactiveCallContext call_ctx_;

  int priority_ = 0;
};

}  // namespace flare

#endif  // FLARE_NET_HBASE_HBASE_CLIENT_CONTROLLER_H_
