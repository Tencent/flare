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

#include <sys/signal.h>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/compression.h"
#include "flare/base/string.h"
#include "flare/base/thread/latch.h"
#include "flare/init/override_flag.h"
#include "flare/rpc/load_balancer/round_robin.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/rpc_server_controller.h"
#include "flare/rpc/server.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"

FLARE_OVERRIDE_FLAG(flare_buffer_block_size, BUFFER_BLOCK_SIZE);

using namespace std::literals;

namespace flare::protobuf {

static const auto kEchoPrefix = "I'd like to have a prefix. You sent: "s;

static const auto kServerProtocols = {
    "flare",        "qzone-pb", "svrkit", "http+gdt-json", "http+proto3-json",
    "http+pb-text", "http+pb",  "trpc",   "baidu-std",     "poppy",
};

static const auto kProtocolsForBasic = {
    "flare",        "qzone",   "svrkit", "http+gdt-json", "http+proto3-json",
    "http+pb-text", "http+pb", "trpc",   "baidu-std",     "poppy"};
static const auto kProtocolsForStreamingResponse = {"flare", "qzone"};
static const auto kProtocolsForStreamingRpc = {"flare"};
static const auto kProtocolsForBytes = {"flare",   "qzone", "svrkit",
                                        "http+pb", "trpc",  "baidu-std"};
// `COMPRESSION_ALGORITHM_NONE` is implied.
static const std::pair<const char*, std::vector<rpc::CompressionAlgorithm>>
    kProtocolsForCompression[] = {
        {"flare",
         {rpc::COMPRESSION_ALGORITHM_GZIP, rpc::COMPRESSION_ALGORITHM_LZ4_FRAME,
          rpc::COMPRESSION_ALGORITHM_SNAPPY}},
        {"trpc",
         {rpc::COMPRESSION_ALGORITHM_SNAPPY, rpc::COMPRESSION_ALGORITHM_GZIP}},
        {"baidu-std",
         {rpc::COMPRESSION_ALGORITHM_SNAPPY, rpc::COMPRESSION_ALGORITHM_GZIP}},
        {"svrkit", {rpc::COMPRESSION_ALGORITHM_SNAPPY}}};

static constexpr auto kUserErrorStatus = 12345;
static const auto kUserErrorDesc = "a great failure."s;

class EchoServiceImpl : public testing::SyncEchoService {
 public:
  void Echo(const testing::EchoRequest& request,
            testing::EchoResponse* response,
            RpcServerController* controller) override {
    if (failure) {
      controller->SetFailed(kUserErrorStatus, kUserErrorDesc);
      return;
    }
    if (enable_compression) {
      controller->SetCompressionAlgorithm(
          controller->GetPreferredCompressionAlgorithm());
    }
    if (write_resp_in_bytes) {
      testing::EchoResponse resp;
      resp.set_body(kEchoPrefix + request.body());
      controller->SetResponseRawBytes(
          CreateBufferSlow(resp.SerializeAsString()));
    } else {
      response->set_body(kEchoPrefix + request.body());
    }
  }

  void EchoStreamRequest(StreamReader<testing::EchoRequest> reader,
                         testing::EchoResponse* response,
                         RpcServerController* controller) override {
    std::string s;
    while (auto n = reader.Read()) {
      s += n->body();
    }
    response->set_body(s);
  }

  void EchoStreamResponse(const testing::EchoRequest& request,
                          StreamWriter<testing::EchoResponse> writer,
                          RpcServerController* controller) override {
    auto n = TryParse<int>(request.body());
    FLARE_CHECK(n);
    for (int i = 0; i != *n; ++i) {
      testing::EchoResponse resp;
      resp.set_body(request.body());
      writer.Write(resp);
    }
    writer.Close();
  }

  void EchoStreamBoth(StreamReader<testing::EchoRequest> reader,
                      StreamWriter<testing::EchoResponse> writer,
                      RpcServerController* controller) override {
    while (auto n = reader.Read()) {
      testing::EchoResponse resp;
      resp.set_body(n->body());
      writer.Write(resp);
    }
    writer.Close();
  }

  std::atomic<bool> write_resp_in_bytes{false};
  std::atomic<bool> enable_compression{false};
  std::atomic<bool> failure{false};
} service_impl;

class IntegrationTest : public ::testing::Test {
 public:
  void SetUp() override {
    for (auto&& e : kServerProtocols) {
      server_.AddProtocol(e);
    }
    server_.AddService(MaybeOwning(non_owning, &service_impl));
    server_.ListenOn(endpoint_);
    server_.Start();
  }

  void TearDown() override {
    server_.Stop();
    server_.Join();
    service_impl.write_resp_in_bytes = false;
  }

 protected:
  Server server_;
  Endpoint endpoint_ = testing::PickAvailableEndpoint();
};

TEST_F(IntegrationTest, Basic) {
  static const auto kBody = "this is my body.";
  for (auto&& prot : kProtocolsForBasic) {
    FLARE_LOG_INFO("Testing protocol [{}].", prot);
    RpcChannel channel;
    CHECK(channel.Open(prot + "://"s + endpoint_.ToString()));

    testing::EchoService_Stub stub(&channel);
    testing::EchoRequest req;
    testing::EchoResponse resp;
    RpcClientController rpc_ctlr;
    req.set_body(kBody);
    stub.Echo(&rpc_ctlr, &req, &resp, nullptr);
    EXPECT_EQ(kEchoPrefix + kBody, resp.body());
  }
}

TEST_F(IntegrationTest, BasicError) {
  ScopedDeferred _([] { service_impl.failure = false; });
  service_impl.failure = true;

  for (auto&& prot : kProtocolsForBasic) {
    FLARE_LOG_INFO("Testing protocol [{}].", prot);
    RpcChannel channel;
    CHECK(channel.Open(prot + "://"s + endpoint_.ToString()));

    testing::EchoService_SyncStub stub(&channel);
    testing::EchoRequest req;
    RpcClientController rpc_ctlr;
    req.set_body("...");
    auto result = stub.Echo(req, &rpc_ctlr);
    EXPECT_FALSE(result);
    EXPECT_EQ(kUserErrorStatus, result.error().code());

    // These two protocols does not support passing error message around.
    if (prot != "qzone"s && prot != "svrkit"s) {
      EXPECT_EQ(kUserErrorDesc, result.error().message());
    }
  }
}

TEST_F(IntegrationTest, StreamingRequests) {
  static const auto kBody = "this is my body."s;
  for (auto&& prot : kProtocolsForStreamingRpc) {
    FLARE_LOG_INFO("Testing protocol [{}].", prot);
    RpcChannel channel;
    CHECK(channel.Open(prot + "://"s + endpoint_.ToString()));

    testing::EchoService_SyncStub stub(&channel);
    testing::EchoRequest req;
    RpcClientController rpc_ctlr;
    req.set_body(kBody);
    auto&& [is, os] = stub.EchoStreamRequest(&rpc_ctlr);
    for (int i = 0; i != 5; ++i) {
      os.Write(req);
    }
    os.Close();
    EXPECT_EQ(kBody + kBody + kBody + kBody + kBody, is.Read()->body());
    EXPECT_FALSE(is.Read());
    EXPECT_FALSE(rpc_ctlr.Failed());
  }
}

TEST_F(IntegrationTest, StreamingResponse) {
  for (auto&& prot : kProtocolsForStreamingResponse) {
    FLARE_LOG_INFO("Testing protocol [{}].", prot);
    RpcChannel channel;
    CHECK(channel.Open(prot + "://"s + endpoint_.ToString()));

    testing::EchoService_SyncStub stub(&channel);
    testing::EchoRequest req;
    RpcClientController rpc_ctlr;
    req.set_body("5");
    auto is = stub.EchoStreamResponse(req, &rpc_ctlr);
    for (int i = 0; i != 5; ++i) {
      ASSERT_EQ("5", is.Read()->body());
    }
    if (prot == "qzone"s) {
      is.Close();  // No end-of-stream marker.
    } else {
      ASSERT_FALSE(is.Read());
    }
    EXPECT_FALSE(rpc_ctlr.Failed());
  }
}

TEST_F(IntegrationTest, StreamingBoth) {
  static const auto kBody = "this is my body."s;
  for (auto&& prot : kProtocolsForStreamingRpc) {
    FLARE_LOG_INFO("Testing protocol [{}].", prot);
    RpcChannel channel;
    CHECK(channel.Open(prot + "://"s + endpoint_.ToString()));

    testing::EchoService_SyncStub stub(&channel);
    testing::EchoRequest req;
    RpcClientController rpc_ctlr;
    req.set_body(kBody);
    auto [is, os] = stub.EchoStreamBoth(&rpc_ctlr);
    for (int i = 0; i != 5; ++i) {
      os.Write(req);
      ASSERT_EQ(kBody, is.Read()->body());
    }
    os.Close();
    ASSERT_FALSE(is.Read());
    EXPECT_FALSE(rpc_ctlr.Failed());
  }
}

TEST_F(IntegrationTest, ClientInBytes) {
  static const auto kBody = "this is my body." + std::string(123456, 'a');
  for (auto&& prot : kProtocolsForBytes) {
    FLARE_LOG_INFO("Testing protocol [{}].", prot);
    RpcChannel channel;
    FLARE_CHECK(channel.Open(prot + "://"s + endpoint_.ToString()));

    testing::EchoService_Stub stub(&channel);
    testing::EchoRequest req;
    testing::EchoResponse resp;
    RpcClientController rpc_ctlr;
    req.set_body(kBody);
    rpc_ctlr.SetAcceptResponseRawBytes(true);
    rpc_ctlr.SetRequestRawBytes(CreateBufferSlow(req.SerializeAsString()));
    stub.Echo(&rpc_ctlr, nullptr, nullptr, nullptr);
    ASSERT_TRUE(!rpc_ctlr.Failed());
    ASSERT_TRUE(
        resp.ParseFromString(FlattenSlow(rpc_ctlr.GetResponseRawBytes())));
    EXPECT_EQ(kEchoPrefix + kBody, resp.body());
  }
}

TEST_F(IntegrationTest, ServerInBytes) {
  ScopedDeferred _([] { service_impl.write_resp_in_bytes = false; });
  service_impl.write_resp_in_bytes = true;

  static const auto kBody = "this is my body." + std::string(123456, 'a');
  for (auto&& prot : kProtocolsForBytes) {
    FLARE_LOG_INFO("Testing protocol [{}].", prot);
    RpcChannel channel;
    FLARE_CHECK(channel.Open(prot + "://"s + endpoint_.ToString()));

    testing::EchoService_SyncStub stub(&channel);
    testing::EchoRequest req;
    req.set_body(kBody);
    RpcClientController rpc_ctlr;
    auto result = stub.Echo(req, &rpc_ctlr);
    ASSERT_TRUE(result);
    EXPECT_EQ(kEchoPrefix + kBody, result->body());
  }
}

TEST_F(IntegrationTest, ServerCompression) {
  ScopedDeferred _([] { service_impl.enable_compression = false; });
  service_impl.enable_compression = true;

  static const auto kBody = "this is my body.";
  for (auto&& [prot, algos] : kProtocolsForCompression) {
    auto algos_plus_no_compression = algos;
    algos_plus_no_compression.push_back(rpc::COMPRESSION_ALGORITHM_NONE);

    for (auto&& algo : algos_plus_no_compression) {
      FLARE_LOG_INFO("Testing protocol [{}] with [{}].", prot,
                     rpc::CompressionAlgorithm_Name(algo));
      RpcChannel channel;
      CHECK(channel.Open(prot + "://"s + endpoint_.ToString()));

      testing::EchoService_Stub stub(&channel);
      testing::EchoRequest req;
      testing::EchoResponse resp;
      RpcClientController rpc_ctlr;
      rpc_ctlr.SetCompressionAlgorithm(algo);
      req.set_body(kBody);
      stub.Echo(&rpc_ctlr, &req, &resp, nullptr);
      EXPECT_EQ(kEchoPrefix + kBody, resp.body());
    }
  }
}

TEST_F(IntegrationTest, NotSupportedCompression) {
  ScopedDeferred _([] { service_impl.enable_compression = false; });
  service_impl.enable_compression = true;

  static const auto kBody = "this is my body.";
  RpcChannel channel;
  CHECK(channel.Open("svrkit://"s + endpoint_.ToString()));

  testing::EchoService_Stub stub(&channel);
  testing::EchoRequest req;
  testing::EchoResponse resp;
  RpcClientController rpc_ctlr;
  rpc_ctlr.SetCompressionAlgorithm(rpc::COMPRESSION_ALGORITHM_GZIP);
  req.set_body(kBody);
  stub.Echo(&rpc_ctlr, &req, &resp, nullptr);
  EXPECT_EQ(kEchoPrefix + kBody, resp.body());
}

class CompressedEchoService : public testing::SyncEchoService {
 public:
  void Echo(const testing::EchoRequest& request,
            testing::EchoResponse* response,
            RpcServerController* controller) override {
    controller->SetCompressionAlgorithm(rpc::COMPRESSION_ALGORITHM_ZSTD);
    if (request.body() == "precompressed") {
      controller->SetResponseAttachment(*Compress(
          MakeCompressor("zstd").get(), CreateBufferSlow("attachment")));
      controller->SetResponseAttachmentPrecompressed(true);
    } else {
      controller->SetResponseAttachment(CreateBufferSlow("attachment"));
    }
  }
};

TEST(Integration, Compression) {
  CompressedEchoService service_impl;

  auto endpoint = testing::PickAvailableEndpoint();
  Server server;
  server.AddProtocol("flare");
  server.AddService(&service_impl);
  server.ListenOn(endpoint);
  server.Start();

  RpcChannel channel;
  CHECK(channel.Open("flare://"s + endpoint.ToString()));

  testing::EchoService_Stub stub(&channel);
  testing::EchoRequest req;
  testing::EchoResponse resp;

  {
    RpcClientController rpc_ctlr;
    stub.Echo(&rpc_ctlr, &req, &resp, nullptr);
    EXPECT_EQ("attachment", FlattenSlow(rpc_ctlr.GetResponseAttachment()));
  }
  {
    req.set_body("precompressed");
    RpcClientController rpc_ctlr;
    stub.Echo(&rpc_ctlr, &req, &resp, nullptr);
    EXPECT_EQ("attachment", FlattenSlow(rpc_ctlr.GetResponseAttachment()));
  }
}

}  // namespace flare::protobuf

FLARE_TEST_MAIN
