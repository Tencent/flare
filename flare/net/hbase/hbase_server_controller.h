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

#ifndef FLARE_NET_HBASE_HBASE_SERVER_CONTROLLER_H_
#define FLARE_NET_HBASE_HBASE_SERVER_CONTROLLER_H_

#include <string>

#include "thirdparty/googletest/gtest/gtest_prod.h"

#include "flare/net/hbase/hbase_controller_common.h"

namespace flare {

namespace hbase {

class HbaseServerProtocol;

}  // namespace hbase

// RPC controller for HBase server.
class HbaseServerController : public hbase::HbaseControllerCommon {
 public:
  // Some facts about the connection.
  const std::string& GetEffectiveUser() const;
  const std::string& GetCellBlockCodec() const;
  const std::string& GetCellBlockCompressor() const;

  // `GetConnectionHeader()`?

  // Accessor for cell-block.
  using HbaseControllerCommon::GetRequestCellBlock;
  using HbaseControllerCommon::GetResponseCellBlock;
  using HbaseControllerCommon::SetResponseCellBlock;

  // Reset the controller.
  void Reset() override;

 private:
  FRIEND_TEST(HbaseServerController, ConnectionHeader);
  friend class HbaseService;

  // Only pointer (not value) is saved.
  void SetConnectionHeader(const hbase::ConnectionHeader* conn_header);

 private:
  const hbase::ConnectionHeader* conn_header_ = nullptr;
};

}  // namespace flare

#endif  // FLARE_NET_HBASE_HBASE_SERVER_CONTROLLER_H_
