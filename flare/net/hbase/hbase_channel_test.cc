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

#include "flare/net/hbase/hbase_channel.h"

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/net/hbase/hbase_client_controller.h"
#include "flare/net/hbase/hbase_server_controller.h"
#include "flare/net/hbase/hbase_service.h"
#include "flare/rpc/server.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"
#include "flare/testing/rpc_mock.h"

namespace flare::hbase {

constexpr auto kErrorRequest = "err req";

class ServiceImpl : public testing::EchoService {
 public:
  void Echo(google::protobuf::RpcController* controller,
            const testing::EchoRequest* request,
            testing::EchoResponse* response,
            google::protobuf::Closure* done) override {
    auto ctlr = flare::down_cast<HbaseServerController>(controller);
    ASSERT_EQ("root", ctlr->GetEffectiveUser());
    if (request->body() == kErrorRequest) {
      HbaseException xcpt;
      xcpt.set_exception_class_name("my xcpt");
      ctlr->SetException(xcpt);
    } else {
      ctlr->SetResponseCellBlock(CreateBufferSlow("my cells"));
      response->set_body("hey there.");
    }
    done->Run();
  }
};

class ChannelTest : public ::testing::Test {
 public:
  void SetUp() override {
    srs_.GetBuiltinNativeService<HbaseService>()->AddService(&service_impl_);
    srs_.AddProtocol("hbase");
    srs_.ListenOn(endpoint_);
    srs_.Start();

    CHECK(channel_.Open("hbase://" + endpoint_.ToString(),
                        HbaseChannel::Options{.effective_user = "root",
                                              .service_name = "EchoService"}));
  }

  void TearDown() override {
    srs_.Stop();
    srs_.Join();
  }

 protected:
  ServiceImpl service_impl_;
  Server srs_;
  Endpoint endpoint_ = testing::PickAvailableEndpoint();
  HbaseChannel channel_;
};

TEST_F(ChannelTest, NormalRpc) {
  testing::EchoService_Stub stub(&channel_);
  testing::EchoRequest req;
  testing::EchoResponse resp;
  HbaseClientController ctlr;

  req.set_body("body");
  stub.Echo(&ctlr, &req, &resp, nullptr);
  ASSERT_FALSE(ctlr.Failed());
  ASSERT_EQ("hey there.", resp.body());
  ASSERT_EQ("my cells", FlattenSlow(ctlr.GetResponseCellBlock()));
}

TEST_F(ChannelTest, Error) {
  testing::EchoService_Stub stub(&channel_);
  testing::EchoRequest req;
  testing::EchoResponse resp;
  HbaseClientController ctlr;

  req.set_body(kErrorRequest);
  stub.Echo(&ctlr, &req, &resp, nullptr);
  ASSERT_TRUE(ctlr.Failed());
  ASSERT_EQ("my xcpt", ctlr.ErrorText());
  ASSERT_EQ("my xcpt", ctlr.GetException().exception_class_name());
}

}  // namespace flare::hbase

FLARE_TEST_MAIN
