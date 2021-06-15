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

#include "flare/rpc/binlog/dry_runner.h"

#include <typeinfo>

#include "gflags/gflags.h"
#include "gtest/gtest.h"

using namespace std::literals;

DECLARE_string(flare_binlog_dry_runner);

namespace flare::binlog {

namespace {

class DummyDryRunner : public DryRunner {
 public:
  ByteStreamParseStatus ParseByteStream(
      NoncontiguousBuffer* buffer,
      std::unique_ptr<DryRunContext>* context) override {
    return ByteStreamParseStatus::Error;
  }
};

FLARE_RPC_BINLOG_REGISTER_DRY_RUNNER("dummy", [] {
  return std::make_unique<DummyDryRunner>();
});

}  // namespace

TEST(DryRunner, All) {
  google::FlagSaver _;
  FLAGS_flare_binlog_dry_runner = "dummy";

  // Not so much to test..
  auto&& runner = *GetDryRunner();
  EXPECT_EQ(typeid(DummyDryRunner), typeid(runner));
}

}  // namespace flare::binlog
