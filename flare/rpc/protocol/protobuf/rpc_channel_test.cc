// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#define FLARE_RPC_CHANNEL_SUPPRESS_INCLUDE_WARNING

#include "flare/rpc/protocol/protobuf/rpc_channel.h"

#include <sys/signal.h>

#include <thread>
#include <vector>

#include "flare/fiber/async.h"
#include "flare/fiber/this_fiber.h"
#include "flare/rpc/protocol/protobuf/message.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/rpc_server_controller.h"
#include "flare/rpc/server.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"
#include "flare/testing/rpc_mock.h"

using namespace std::literals;

namespace flare {

class ServiceImpl : public testing::SyncEchoService {
 public:
  void Echo(const testing::EchoRequest& request,
            testing::EchoResponse* response,
            RpcServerController* controller) override {
    std::this_thread::sleep_for(10ms);
    if (++call_counter_ < 3) {
      controller->SetFailed(rpc::STATUS_OVERLOADED, "failed");
    } else {
      response->set_body(request.body());
      if (!controller->GetRequestAttachment().Empty()) {
        controller->SetResponseAttachment(CreateBufferSlow(
            "echoed: " + FlattenSlow(controller->GetRequestAttachment())));
      }
    }
  }

  void EchoStreamResponse(const testing::EchoRequest& request,
                          StreamWriter<testing::EchoResponse> writer,
                          RpcServerController* controller) override {
    testing::EchoResponse resp;
    resp.set_body("123");
    for (int i = 0; i != 5; ++i) {
      writer.Write(resp);
    }
    writer.Close();
  }

 public:
  std::atomic<std::size_t> call_counter_{};
};

class EchoServiceImpl : public testing::SyncEchoService {
 public:
  void Echo(const testing::EchoRequest& request,
            testing::EchoResponse* response,
            RpcServerController* controller) override {
    ++call_counter_;
  }

 public:
  std::atomic<std::size_t> call_counter_{};
};

class ChannelTest : public ::testing::Test {
 public:
  void SetUp() override {
    srs_.AddService(MaybeOwning(non_owning, &service_impl_));
    srs_.AddProtocols({"flare", "qzone-pb", "http+pb"});
    srs_.ListenOn(endpoint_);
    srs_.Start();
  }

  void TearDown() override {
    srs_.Stop();
    srs_.Join();
  }

 protected:
  ServiceImpl service_impl_;
  Server srs_{Server::Options{.maximum_packet_size = 1048576 * 64}};
  Endpoint endpoint_ = testing::PickAvailableEndpoint();
};

TEST_F(ChannelTest, NormalRpc) {
  static const auto kBody = "test body"s;
  RpcChannel channel;
  channel.Open("flare://" + endpoint_.ToString(),
               RpcChannel::Options{.override_nslb = "list+rr"});
  testing::EchoService_Stub stub(&channel);
  testing::EchoRequest req;
  testing::EchoResponse resp;
  req.set_body(kBody);

  RpcClientController ctlr;
  service_impl_.call_counter_ = 0;
  stub.Echo(&ctlr, &req, &resp, nullptr);
  ASSERT_TRUE(ctlr.Failed());
  ctlr.Reset();
  stub.Echo(&ctlr, &req, &resp, nullptr);
  ASSERT_TRUE(ctlr.Failed());
  ctlr.Reset();
  stub.Echo(&ctlr, &req, &resp, nullptr);
  ASSERT_FALSE(ctlr.Failed());
  ASSERT_EQ(kBody, resp.body());

  ctlr.Reset();
  ctlr.SetMaxRetries(2);
  service_impl_.call_counter_ = 0;
  stub.Echo(&ctlr, &req, &resp, nullptr);
  ASSERT_TRUE(ctlr.Failed());

  ctlr.Reset();
  ctlr.SetMaxRetries(3);
  service_impl_.call_counter_ = 0;
  stub.Echo(&ctlr, &req, &resp, nullptr);
  ASSERT_FALSE(ctlr.Failed());
  ASSERT_EQ(kBody, resp.body());
  ASSERT_EQ(3, service_impl_.call_counter_.load());
}

TEST_F(ChannelTest, Timeout) {
  testing::EchoService_SyncStub stub("flare://" + endpoint_.ToString());
  RpcClientController ctlr;
  ctlr.SetTimeout(1ns);
  EXPECT_EQ(rpc::STATUS_TIMEOUT,
            stub.Echo(testing::EchoRequest(), &ctlr).error().code());
}

TEST_F(ChannelTest, UriNormalization) {
  for (auto scheme : {"qzone"s, "http"s}) {
    RpcChannel channel;
    CHECK(channel.Open(scheme + "://" + endpoint_.ToString()));
    testing::EchoService_SyncStub stub(&channel);
    testing::EchoRequest req;
    req.set_body("asdf");

    RpcClientController ctlr;
    service_impl_.call_counter_ = 10;  // @sa: ServiceImpl::Echo, must be
                                       // greater than for it to return success.
    ASSERT_EQ("asdf", stub.Echo(req, &ctlr)->body());
  }
}

TEST_F(ChannelTest, Attachment) {
  static const auto kAttachment = std::string(10000000, 'A');
  // At the moment only `flare` protocol supports attachment.
  RpcChannel channel;
  CHECK(channel.Open("flare://" + endpoint_.ToString(),
                     RpcChannel::Options{.maximum_packet_size = 64 * 1048576}));
  testing::EchoService_SyncStub stub(&channel);
  testing::EchoRequest req;
  req.set_body("asdf");

  RpcClientController ctlr;
  ctlr.SetRequestAttachment(CreateBufferSlow(kAttachment));
  service_impl_.call_counter_ = 10;  // @sa: ServiceImpl::Echo, must be
                                     // greater than for it to return success.
  ASSERT_EQ("asdf", stub.Echo(req, &ctlr)->body());
  ASSERT_EQ("echoed: " + kAttachment,
            FlattenSlow(ctlr.GetResponseAttachment()));
}

TEST_F(ChannelTest, ImplicitOpen) {
  RpcChannel channel("flare://" + endpoint_.ToString());
  testing::EchoService_SyncStub stub(&channel);
  testing::EchoRequest req;
  req.set_body("asdf");
  RpcClientController ctlr;
  service_impl_.call_counter_ = 10;  // @sa: ServiceImpl::Echo, must be
                                     // greater than for it to return success.
  ASSERT_EQ("asdf", stub.Echo(req, &ctlr)->body());
}

TEST_F(ChannelTest, StreamingRpc) {
  testing::EchoService_SyncStub stub("flare://" + endpoint_.ToString());
  testing::EchoRequest req;
  req.set_body("aaa");
  RpcClientController ctlr;
  auto reader = stub.EchoStreamResponse(req, &ctlr);
  int resps = 0;
  while (auto&& body = reader.Read()) {
    EXPECT_EQ("123", body->body());
    ++resps;
  }
  EXPECT_EQ(5, resps);
}

TEST_F(ChannelTest, StreamingRpcFailure) {
  testing::EchoService_SyncStub stub("flare://127.0.0.1:1");
  testing::EchoRequest req;
  req.set_body("aaa");
  RpcClientController ctlr;
  ctlr.SetTimeout(1s);
  auto reader = stub.EchoStreamResponse(req, &ctlr);
  EXPECT_FALSE(reader.Read());
}

TEST_F(ChannelTest, StreamingRpcFailure2) {
  // Hopefully it's a blakehole.
  testing::EchoService_SyncStub stub("flare://192.0.2.1:1");
  testing::EchoRequest req;
  req.set_body("aaa");
  RpcClientController ctlr;
  ctlr.SetTimeout(1s);
  auto reader = stub.EchoStreamResponse(req, &ctlr);
  EXPECT_FALSE(reader.Read());
}

TEST(Channel, ImplicitOpenInvalid) {
  RpcChannel channel("flare://l5:something-nobody-recognizes");
  testing::EchoService_SyncStub stub(&channel);
  testing::EchoRequest req;
  RpcClientController ctlr;
  ASSERT_EQ(rpc::STATUS_INVALID_CHANNEL, stub.Echo(req, &ctlr).error().code());
}

class NonmultiplexableTestService : public testing::SyncEchoService {
 public:
  void Echo(const testing::EchoRequest& request,
            testing::EchoResponse* response,
            RpcServerController* controller) override {
    {
      std::scoped_lock lk(lock);
      eps.push_back(controller->GetRemotePeer());
    }

    // Sleep for sometime so that the second request comes before this one
    // finishes (force the caller to use a second connection.).
    this_fiber::SleepFor(1s);
  }
  std::mutex lock;
  std::vector<Endpoint> eps;
};

TEST(Channel, NonmultiplexableTalk) {
  static const std::string kReply = "Bad weather.";
  // Start a service.
  auto ep = testing::PickAvailableEndpoint();
  NonmultiplexableTestService echo_svc;
  Server rs;

  rs.AddProtocol("svrkit");
  rs.AddService(MaybeOwning(non_owning, &echo_svc));
  rs.ListenOn(ep);
  CHECK(rs.Start());

  auto talk = [&] {
    RpcChannel channel;
    CHECK(channel.Open("svrkit://" + ep.ToString()));
    testing::EchoRequest req;
    testing::EchoService_SyncStub stub(&channel);
    RpcClientController ctlr;

    (void)stub.Echo(req, &ctlr);
  };

  // Make two RPCs.
  auto f1 = fiber::Async(talk);
  auto f2 = fiber::Async(talk);

  fiber::BlockingGet(WhenAll(&f1, &f2));
  ASSERT_EQ(2, echo_svc.eps.size());  // Two connections were made.

  rs.Stop();
  rs.Join();
}

TEST(Channel, MockRpc) {
  static const auto kResponse = "test body"s;
  RpcChannel channel;
  channel.Open("mock://what-ever-it-wants-to-be.");
  testing::EchoService_Stub stub(&channel);
  testing::EchoRequest req;
  testing::EchoResponse resp, resp_data;
  req.set_body("not cared");
  resp_data.set_body(kResponse);

  FLARE_EXPECT_RPC(testing::EchoService::Echo, ::testing::_)
      .WillRepeatedly(testing::Respond(resp_data));

  RpcClientController ctlr;
  stub.Echo(&ctlr, &req, &resp, nullptr);
  ASSERT_FALSE(ctlr.Failed());
  ASSERT_EQ(kResponse, resp.body());
}

}  // namespace flare

int main(int argc, char** argv) {
  return flare::testing::InitAndRunAllTests(&argc, argv);
}
