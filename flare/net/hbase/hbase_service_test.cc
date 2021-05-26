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

#include "flare/net/hbase/hbase_service.h"

#include <utility>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/down_cast.h"
#include "flare/net/hbase/call_context.h"
#include "flare/net/hbase/hbase_server_controller.h"
#include "flare/net/hbase/message.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/main.h"

namespace flare {

constexpr auto kEchoBody = "echo";
constexpr auto kErrorBody = "error body";
constexpr auto kCellBlockBody = "return cellblock";
constexpr auto kExceptionName = "flare.hbase.xcpt.RuntimeException";

class DummyService : public testing::EchoService {
 public:
  void Echo(google::protobuf::RpcController* controller,
            const flare::testing::EchoRequest* request,
            flare::testing::EchoResponse* response,
            google::protobuf::Closure* done) override {
    auto ctlr = flare::down_cast<HbaseServerController>(controller);
    ASSERT_EQ("cell-codec", ctlr->GetCellBlockCodec());

    if (request->body() == kEchoBody) {
      response->set_body(request->body());
    } else if (request->body() == kErrorBody) {
      HbaseException xcpt;
      xcpt.set_exception_class_name(kExceptionName);
      ctlr->SetException(std::move(xcpt));
    } else if (request->body() == kCellBlockBody) {
      ASSERT_EQ("my cell req", FlattenSlow(ctlr->GetRequestCellBlock()));
      response->set_body(kCellBlockBody);
      ctlr->SetResponseCellBlock(CreateBufferSlow("cell resp"));
    }
    done->Run();
  }
} dummy;

class HbaseServiceTest : public ::testing::Test {
 public:
  void SetUp() override {
    service_.AddService(&dummy);
    conn_header_.set_cell_block_codec_class("cell-codec");

    call_ctx_.conn_header = &conn_header_;
    call_ctx_.service = dummy.GetDescriptor();
    call_ctx_.method = dummy.GetDescriptor()->FindMethodByName("Echo");
    call_ctx_.response = std::make_unique<testing::EchoResponse>();

    ctx_.controller = &call_ctx_;
    auto req = std::make_unique<hbase::HbaseRequest>();
    req->body = MaybeOwning(non_owning, &request_);
    req_msg_ = std::move(req);
  }

 protected:
  hbase::ConnectionHeader conn_header_;
  HbaseService::Context ctx_;
  HbaseService service_;
  std::unique_ptr<Message> req_msg_;
  testing::EchoRequest request_;
  hbase::PassiveCallContext call_ctx_;
};

TEST_F(HbaseServiceTest, Echo) {
  request_.set_body(kEchoBody);

  auto validator = [&](auto&& m) {
    auto&& resp = *cast<hbase::HbaseResponse>(m);
    auto&& body =
        flare::down_cast<testing::EchoResponse>(std::get<1>(resp.body));
    EXPECT_FALSE(resp.header.has_exception());
    EXPECT_EQ(kEchoBody, body->body());
    return 1;
  };
  ASSERT_EQ(StreamService::ProcessingStatus::Processed,
            service_.FastCall(&req_msg_, validator, &ctx_));
}

TEST_F(HbaseServiceTest, Error) {
  request_.set_body(kErrorBody);

  auto validator = [&](auto&& m) {
    auto&& resp = *cast<hbase::HbaseResponse>(m);
    EXPECT_EQ(kExceptionName, resp.header.exception().exception_class_name());
    // Not initialized at all.
    EXPECT_EQ(0, resp.body.index());
    EXPECT_EQ(nullptr, std::get<0>(resp.body).Get());
    return 1;
  };
  ASSERT_EQ(StreamService::ProcessingStatus::Processed,
            service_.FastCall(&req_msg_, validator, &ctx_));
}

TEST_F(HbaseServiceTest, PassCellBlockBackAndForth) {
  request_.set_body(kCellBlockBody);
  auto&& hbase_req = down_cast<hbase::HbaseRequest>(*req_msg_);
  hbase_req->cell_block = CreateBufferSlow("my cell req");
  hbase_req->header.mutable_cell_block_meta()->set_length(
      hbase_req->cell_block.ByteSize());

  auto validator = [&](auto&& m) {
    auto&& resp = *cast<hbase::HbaseResponse>(m);
    auto&& body =
        flare::down_cast<testing::EchoResponse>(std::get<1>(resp.body));
    EXPECT_FALSE(resp.header.has_exception());
    EXPECT_EQ("cell resp", FlattenSlow(resp.cell_block));
    EXPECT_EQ(kCellBlockBody, body->body());
    EXPECT_EQ(resp.header.cell_block_meta().length(),
              resp.cell_block.ByteSize());
    return 1;
  };
  ASSERT_EQ(StreamService::ProcessingStatus::Processed,
            service_.FastCall(&req_msg_, validator, &ctx_));
}

}  // namespace flare

FLARE_TEST_MAIN
