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

#define FLARE_RPC_SERVER_CONTROLLER_SUPPRESS_INCLUDE_WARNING

#include "flare/rpc/protocol/protobuf/service.h"

#include <sys/signal.h>

#include <string_view>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/deferred.h"
#include "flare/fiber/async.h"
#include "flare/fiber/execution_context.h"
#include "flare/fiber/this_fiber.h"
#include "flare/init/override_flag.h"
#include "flare/rpc/internal/stream_io_adaptor.h"
#include "flare/rpc/protocol/protobuf/call_context_factory.h"
#include "flare/rpc/protocol/protobuf/message.h"
#include "flare/rpc/protocol/protobuf/rpc_server_controller.h"
#include "flare/rpc/protocol/stream_protocol.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/main.h"

using namespace std::literals;

FLARE_OVERRIDE_FLAG(
    flare_rpc_server_protocol_buffers_max_ongoing_requests_per_method,
    "flare.testing.EchoService.EchoWithMaxQueueingDelay:1000,flare.testing."
    "EchoService.EchoWithMaxOngoingRequests:2000");

namespace flare::protobuf {

static constexpr auto kStreamRequestTimes = 5;
static constexpr auto kStreamResponseTimes = 5;

class ServiceImpl : public testing::EchoService {
 public:
  void Echo(google::protobuf::RpcController* controller,
            const testing::EchoRequest* request,
            testing::EchoResponse* response,
            google::protobuf::Closure* done) override {
    ScopedDeferred _([&] { done->Run(); });
    response->set_body(request->body());
  }

  void EchoStreamRequest(google::protobuf::RpcController* controller,
                         const testing::EchoRequest* request,
                         testing::EchoResponse* response,
                         google::protobuf::Closure* done) override {
    ScopedDeferred _([&] { done->Run(); });
    auto ctlr = flare::down_cast<RpcServerController>(controller);
    auto&& is = ctlr->GetStreamReader<testing::EchoRequest>();
    std::string resp;
    while (auto v = is.Read()) {
      resp += v.value().body();
    }
    response->set_body(resp);
  }

  void EchoStreamResponse(google::protobuf::RpcController* controller,
                          const testing::EchoRequest* request,
                          testing::EchoResponse* response,
                          google::protobuf::Closure* done) override {
    ScopedDeferred _([&] { done->Run(); });
    auto ctlr = flare::down_cast<RpcServerController>(controller);
    auto&& os = ctlr->GetStreamWriter<testing::EchoResponse>();
    testing::EchoResponse resp;
    resp.set_body(request->body());
    for (int i = 0; i != kStreamResponseTimes; ++i) {
      if (i == kStreamResponseTimes - 1) {
        ASSERT_TRUE(os.WriteLast(resp));
      } else {
        ASSERT_TRUE(os.Write(resp));
      }
    }
  }

  void EchoStreamBoth(google::protobuf::RpcController* controller,
                      const testing::EchoRequest* request,
                      testing::EchoResponse* response,
                      google::protobuf::Closure* done) override {
    ScopedDeferred _([&] { done->Run(); });
    auto ctlr = flare::down_cast<RpcServerController>(controller);
    auto&& is = ctlr->GetStreamReader<testing::EchoRequest>();
    auto&& os = ctlr->GetStreamWriter<testing::EchoResponse>();
    while (auto v = is.Read()) {
      testing::EchoResponse resp;
      resp.set_body(v.value().body());
      ASSERT_TRUE(os.Write(resp));
    }
    os.Close();
  }
};

StreamProtocol* GetProtocol() {
  static auto p = server_side_stream_protocol_registry.New("flare");
  return p.get();
}

class ServiceFastCallTest : public ::testing::Test {
 public:
  void SetUp() override {
    svc_ = std::make_unique<Service>();
    svc_->AddService(std::make_unique<ServiceImpl>());

    auto meta = object_pool::Get<rpc::RpcMeta>();
    meta->set_method_type(rpc::METHOD_TYPE_SINGLE);
    meta->set_correlation_id(1);
    meta->mutable_request_meta()->set_method_name(
        "flare.testing.EchoService.Echo");
    testing::EchoRequest er;
    er.set_body(kEchoBody);
    req_msg_ = std::make_unique<ProtoMessage>(
        std::move(meta), std::make_unique<testing::EchoRequest>(er));
  }

  void TearDown() override {}

 protected:
  inline static const std::string kEchoBody = "hello 123";
  std::unique_ptr<ServiceImpl> impl_;
  std::unique_ptr<Service> svc_;
  std::unique_ptr<Message> req_msg_;
};

TEST_F(ServiceFastCallTest, FastCall) {
  fiber::ExecutionContext::Create()->Execute([&] {
    StreamService::Context context;
    auto validator = [&](auto&& m) {
      auto resp_body = flare::down_cast<testing::EchoResponse>(
                           *std::get<1>(cast<ProtoMessage>(m)->msg_or_buffer))
                           ->body();
      EXPECT_EQ(kEchoBody, resp_body);
      return 1;
    };
    auto rc = svc_->FastCall(&req_msg_, validator, &context);
    ASSERT_EQ(StreamService::ProcessingStatus::Processed, rc);
  });
}

TEST_F(ServiceFastCallTest, RejectedDelayedFastCall) {
  fiber::ExecutionContext::Create()->Execute([&] {
    StreamService::Context context;
    auto validator = [&](auto&& m) {
      EXPECT_EQ(rpc::STATUS_OVERLOADED,
                cast<ProtoMessage>(m)->meta->response_meta().status());
      return 1;
    };
    svc_->method_descs_["flare.testing.EchoService.Echo"sv].max_queueing_delay =
        1ns;
    context.received_tsc = context.dispatched_tsc = context.parsed_tsc = 0;
    auto rc = svc_->FastCall(&req_msg_, validator, &context);
    ASSERT_EQ(StreamService::ProcessingStatus::Overloaded, rc);
  });
}

TEST_F(ServiceFastCallTest, AcceptedDelayedFastCall) {
  fiber::ExecutionContext::Create()->Execute([&] {
    StreamService::Context context;
    auto validator = [&](auto&& m) {
      auto resp_body = flare::down_cast<testing::EchoResponse>(
                           *std::get<1>(cast<ProtoMessage>(m)->msg_or_buffer))
                           ->body();
      EXPECT_EQ(kEchoBody, resp_body);
      return 1;
    };
    svc_->method_descs_["flare.testing.EchoService.Echo"sv].max_queueing_delay =
        100ms;
    context.received_tsc = context.dispatched_tsc = context.parsed_tsc =
        ReadTsc();
    auto rc = svc_->FastCall(&req_msg_, validator, &context);
    ASSERT_EQ(StreamService::ProcessingStatus::Processed, rc);
  });
}

TEST_F(ServiceFastCallTest, RejectedMaxOngoingFastCall) {
  fiber::ExecutionContext::Create()->Execute([&] {
    StreamService::Context context;
    auto validator = [&](auto&& m) {
      EXPECT_EQ(rpc::STATUS_OVERLOADED,
                cast<ProtoMessage>(m)->meta->response_meta().status());
      return 1;
    };
    auto&& desc = svc_->method_descs_["flare.testing.EchoService.Echo"sv];
    desc.max_ongoing_requests = 10;
    desc.ongoing_requests = std::make_unique<Service::AlignedInt>();
    desc.ongoing_requests->value = 10;
    auto rc = svc_->FastCall(&req_msg_, validator, &context);
    ASSERT_EQ(StreamService::ProcessingStatus::Overloaded, rc);
  });
}

TEST_F(ServiceFastCallTest, AcceptedMaxOngoingFastCall) {
  fiber::ExecutionContext::Create()->Execute([&] {
    StreamService::Context context;
    auto validator = [&](auto&& m) {
      auto resp_body = flare::down_cast<testing::EchoResponse>(
                           *std::get<1>(cast<ProtoMessage>(m)->msg_or_buffer))
                           ->body();
      EXPECT_EQ(kEchoBody, resp_body);
      return 1;
    };
    auto&& desc = svc_->method_descs_["flare.testing.EchoService.Echo"sv];
    desc.max_ongoing_requests = 10;
    desc.ongoing_requests = std::make_unique<Service::AlignedInt>();
    desc.ongoing_requests->value = 9;
    auto rc = svc_->FastCall(&req_msg_, validator, &context);
    ASSERT_EQ(StreamService::ProcessingStatus::Processed, rc);
    rc = svc_->FastCall(&req_msg_, validator, &context);
    ASSERT_EQ(StreamService::ProcessingStatus::Processed, rc);
    rc = svc_->FastCall(&req_msg_, validator, &context);
    ASSERT_EQ(StreamService::ProcessingStatus::Processed, rc);
  });
}

TEST_F(ServiceFastCallTest, MaxOngoingFlag) {
  EXPECT_EQ(1000, svc_->method_descs_
                      ["flare.testing.EchoService.EchoWithMaxQueueingDelay"sv]
                          .max_ongoing_requests);
  EXPECT_EQ(1 /* Protocol Buffers method option. */,
            svc_->method_descs_
                ["flare.testing.EchoService.EchoWithMaxOngoingRequests"sv]
                    .max_ongoing_requests);
}

TEST_F(ServiceFastCallTest, ServiceNotFound) {
  fiber::ExecutionContext::Create()->Execute([&] {
    StreamService::Context context;

    {
      auto validator = [&](auto&& m) {
        EXPECT_EQ(rpc::STATUS_METHOD_NOT_FOUND,
                  cast<ProtoMessage>(m)->meta->response_meta().status());
        return 1;
      };
      cast<ProtoMessage>(*req_msg_)
          ->meta->mutable_request_meta()
          ->set_method_name("flare.testing.EchoService.MethodNotFound");
      auto rc = svc_->FastCall(&req_msg_, validator, &context);
      ASSERT_EQ(StreamService::ProcessingStatus::Processed, rc);
    }

    {
      auto validator = [&](auto&& m) {
        EXPECT_EQ(rpc::STATUS_SERVICE_NOT_FOUND,
                  cast<ProtoMessage>(m)->meta->response_meta().status());
        return 1;
      };
      cast<ProtoMessage>(*req_msg_)
          ->meta->mutable_request_meta()
          ->set_method_name("flare.testing.ErrorService.MethodNotFound");
      auto rc = svc_->FastCall(&req_msg_, validator, &context);
      ASSERT_EQ(StreamService::ProcessingStatus::Processed, rc);
    }
  });
}

class ServiceTest : public ::testing::Test {
  struct StreamContext {
    std::atomic<bool> cleaned_up = false;
    std::string resps;
    rpc::detail::StreamIoAdaptor* sio_ptr;
  };

 public:
  void SetUp() override {
    svc = std::make_unique<Service>();
    svc->AddService(std::make_unique<ServiceImpl>());

    auto write_cb = [this](auto&& m) {
      auto payload = std::get<1>(cast<ProtoMessage>(&m)->msg_or_buffer).Get();
      if (payload) {
        sctx->resps += flare::down_cast<testing::EchoResponse>(payload)->body();
      }
      sctx->sio_ptr->NotifyWriteCompletion();
      return true;
    };
    sctx = std::make_shared<StreamContext>();
    rpc::detail::StreamIoAdaptor::Operations ops = {
        .try_parse = [](auto&&) { return true; },
        .write = write_cb,
        .restart_read = [] {},
        .on_close = [] {},
        .on_cleanup = [this] { sctx->cleaned_up = true; }};
    sio_adaptor =
        std::make_unique<rpc::detail::StreamIoAdaptor>(1, std::move(ops));
    sctx->sio_ptr = sio_adaptor.get();
    ctlr_ctx = passive_call_context_factory.Create(true);
  }

  void TearDown() override {
    while (!sctx->cleaned_up) {
      this_fiber::Yield();
    }
    sio_adaptor->FlushPendingCalls();
  }

 protected:
  std::unique_ptr<Message> CreateMessageFor(
      std::string name, rpc::MessageFlags extra_flags = {}) {
    auto req_msg = std::make_unique<testing::EchoRequest>();
    req_msg->set_body(kEchoBody);

    meta = object_pool::Get<rpc::RpcMeta>();
    meta->set_method_type(rpc::METHOD_TYPE_STREAM);
    meta->set_correlation_id(1);
    meta->set_flags(extra_flags);
    meta->mutable_request_meta()->set_method_name(std::move(name));
    std::unique_ptr<Message> msg =
        std::make_unique<ProtoMessage>(std::move(meta), std::move(req_msg));
    return msg;
  }

  std::string ReadAllResponses() {
    return fiber::ExecutionContext::Create()->Execute([&] {
      StreamService::Context ctx;
      auto rc = svc->StreamCall(&sio_adaptor->GetStreamReader(),
                                &sio_adaptor->GetStreamWriter(), &ctx);
      EXPECT_EQ(StreamService::ProcessingStatus::Processed, rc);
      return sctx->resps;
    });
  }

 protected:
  inline static const std::string kEchoBody = "hello 456";
  std::unique_ptr<Service> svc;
  PooledPtr<rpc::RpcMeta> meta;
  std::shared_ptr<StreamContext> sctx;
  std::unique_ptr<rpc::detail::StreamIoAdaptor> sio_adaptor;
  std::unique_ptr<Controller> ctlr_ctx;
};

TEST_F(ServiceTest, StreamCallStreamingRequest) {
  auto msg = CreateMessageFor("flare.testing.EchoService.EchoStreamRequest");

  // Prepare data.
  for (int i = 0; i != kStreamRequestTimes; ++i) {
    rpc::MessageFlags flag = {};
    if (i == 0) {
      flag = rpc::MESSAGE_FLAGS_START_OF_STREAM;
    } else if (i == kStreamRequestTimes - 1) {
      flag = rpc::MESSAGE_FLAGS_END_OF_STREAM;
    }
    auto msg =
        CreateMessageFor("flare.testing.EchoService.EchoStreamRequest", flag);
    sio_adaptor->NotifyRead(std::move(msg));
  }

  std::string expected;
  for (int i = 0; i != kStreamRequestTimes; ++i) {
    expected += kEchoBody;
  }
  ASSERT_EQ(expected, ReadAllResponses());
}

TEST_F(ServiceTest, StreamCallStreamingResponse) {
  auto msg = CreateMessageFor("flare.testing.EchoService.EchoStreamResponse");
  sio_adaptor->NotifyRead(std::move(msg));

  std::string expected;
  for (int i = 0; i != kStreamResponseTimes; ++i) {
    expected += kEchoBody;
  }
  ASSERT_EQ(expected, ReadAllResponses());
}

TEST_F(ServiceTest, StreamCallStreamingBoth) {
  auto msg = CreateMessageFor("flare.testing.EchoService.EchoStreamBoth");

  // Prepare data.
  for (int i = 0; i != kStreamRequestTimes; ++i) {
    rpc::MessageFlags flag = {};
    if (i == 0) {
      flag = rpc::MESSAGE_FLAGS_START_OF_STREAM;
    } else if (i == kStreamRequestTimes - 1) {
      flag = rpc::MESSAGE_FLAGS_END_OF_STREAM;
    }
    auto msg =
        CreateMessageFor("flare.testing.EchoService.EchoStreamBoth", flag);
    sio_adaptor->NotifyRead(std::move(msg));
  }
  // Closed 100ms later.
  auto f = fiber::Async([this] {
    this_fiber::SleepFor(100ms);
    sio_adaptor->NotifyError(StreamError::EndOfStream);
  });

  std::string expected;
  for (int i = 0; i != kStreamRequestTimes; ++i) {
    expected += kEchoBody;
  }
  ASSERT_EQ(expected, ReadAllResponses());
  fiber::BlockingGet(&f);
}

}  // namespace flare::protobuf

FLARE_TEST_MAIN
