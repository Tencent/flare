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

#include "flare/net/hbase/hbase_server_controller.h"

#include "gtest/gtest.h"

#include "flare/testing/main.h"

namespace flare {

TEST(HbaseServerController, ConnectionHeader) {
  hbase::ConnectionHeader conn_header;

  conn_header.set_cell_block_codec_class("my codec");
  conn_header.set_cell_block_compressor_class("compressor 2");
  conn_header.mutable_user_info()->set_effective_user("super user");

  HbaseServerController ctlr;

  ASSERT_EQ("", ctlr.GetCellBlockCodec());
  ASSERT_EQ("", ctlr.GetCellBlockCompressor());
  ASSERT_EQ("", ctlr.GetEffectiveUser());

  ctlr.SetConnectionHeader(&conn_header);
  ASSERT_EQ("my codec", ctlr.GetCellBlockCodec());
  ASSERT_EQ("compressor 2", ctlr.GetCellBlockCompressor());
  ASSERT_EQ("super user", ctlr.GetEffectiveUser());

  ctlr.Reset();
  ASSERT_EQ("", ctlr.GetCellBlockCodec());
  ASSERT_EQ("", ctlr.GetCellBlockCompressor());
  ASSERT_EQ("", ctlr.GetEffectiveUser());
}

}  // namespace flare

FLARE_TEST_MAIN
