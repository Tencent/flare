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

#include <chrono>

#include "gtest/gtest.h"

#include "flare/base/down_cast.h"
#include "flare/base/internal/cpu.h"
#include "flare/base/random.h"
#include "flare/fiber/async.h"
#include "flare/fiber/future.h"
#include "flare/fiber/this_fiber.h"
#include "flare/init/override_flag.h"
#include "flare/rpc/rpc_channel.h"
#include "flare/rpc/rpc_client_controller.h"
#include "flare/rpc/rpc_server_controller.h"
#include "flare/rpc/server.h"
#include "flare/rpc/server_group.h"
#include "flare/testing/echo_service.flare.pb.h"
#include "flare/testing/endpoint.h"
#include "flare/testing/main.h"
#include "flare/testing/relay_service.flare.pb.h"

using namespace std::literals;

DEFINE_bool(keep_running, false,
            "If set, this UT keeps running until killed. This flag is used for "
            "internal testing purpose.");
DEFINE_int32(concurrency, 0,
             "If set, overrides default RPC concurrency in this UT. Used for "
             "internal testing purpose.");

FLARE_FORCE_OVERRIDE_FLAG(flare_concurrency_hint, 32);

namespace flare {

class DummyEcho : public testing::SyncEchoService {
 public:
  void Echo(const testing::EchoRequest& request,
            testing::EchoResponse* response,
            RpcServerController* controller) override {
    this_fiber::SleepFor(Random(200) * 1ms);
    response->set_body(request.body());
  }

  void EchoWithMaxQueueingDelay(const testing::EchoRequest& request,
                                testing::EchoResponse* response,
                                RpcServerController* controller) override {
    // `flare.max_queueing_delay_ms` is applied by the framework automatically,
    // nothing special for us.
    return Echo(request, response, controller);
  }

  void EchoWithMaxOngoingRequests(const testing::EchoRequest& request,
                                  testing::EchoResponse* response,
                                  RpcServerController* controller) override {
    // `flare.max_ongoing_requests` is applied by the framework automatically,
    // nothing special for us.
    return Echo(request, response, controller);
  }
};

// Not using `Sync` stub on purpose, we need to test asynchronous
// implementation.
class DummyRelay : public testing::RelayService {
 public:
  explicit DummyRelay(std::vector<Endpoint> backends)
      : backends_(std::move(backends)) {
    for (auto&& e : backends_) {
      auto channel = std::make_unique<RpcChannel>();
      FLARE_CHECK(channel->Open("flare://" + e.ToString()));
      channels_.push_back(std::move(channel));
    }
  }

  void Relay(google::protobuf::RpcController* controller,
             const testing::RelayRequest* request,
             testing::RelayResponse* response,
             google::protobuf::Closure* done) override {
    testing::EchoService_Stub stub(
        channels_[Random(channels_.size() - 1)].get());
    testing::EchoRequest req;
    auto ctlr = std::make_shared<RpcClientController>();
    auto resp = std::make_shared<testing::EchoResponse>();
    ctlr->SetTimeout(100ms);
    req.set_body(request->body());

    auto my_done = NewCallback([=, this] {
      if (ctlr->Failed()) {
        ++failure_;
        flare::down_cast<RpcServerController>(controller)
            ->SetFailed(ctlr->ErrorCode(), ctlr->ErrorText());
      } else {
        ++success_;
        response->set_body(resp->body());
      }
      done->Run();
    });
    if (Random() % 5 <= 4) {
      stub.Echo(ctlr.get(), &req, resp.get(), my_done);
    } else {
      if (Random() % 2 == 0) {
        stub.EchoWithMaxQueueingDelay(ctlr.get(), &req, resp.get(), my_done);
      } else {
        stub.EchoWithMaxOngoingRequests(ctlr.get(), &req, resp.get(), my_done);
      }
    }
  }

  std::pair<std::size_t, std::size_t> GetCounters() const noexcept {
    return std::pair(success_.load(), failure_.load());
  }

  void ResetCounters() noexcept {
    success_ = 0;
    failure_ = 0;
  }

 private:
  std::vector<Endpoint> backends_;
  std::vector<std::unique_ptr<RpcChannel>> channels_;
  std::atomic<std::size_t> success_{}, failure_{};
};

TEST(IntegrationTest, RandomFailure) {
  static const auto kConcurrency =
      FLAGS_concurrency ? FLAGS_concurrency
                        : internal::GetNumberOfProcessorsAvailable() * 200;

  auto echo_ep = testing::PickAvailableEndpoint();
  auto relay_ep = testing::PickAvailableEndpoint();
  DummyRelay relay_svc({echo_ep, echo_ep, echo_ep, echo_ep, relay_ep,
                        EndpointFromIpv4("192.0.2.1", 56789) /* Timeout? */,
                        EndpointFromIpv4("127.0.0.1", 1) /* Error */});
  ServerGroup server_group;

  server_group.AddServer(echo_ep, {"flare"}, std::make_unique<DummyEcho>());
  server_group.AddServer(relay_ep, {"flare"},
                         MaybeOwning(non_owning, &relay_svc));
  server_group.Start();

  RpcChannel channels[3];
  FLARE_CHECK(channels[0].Open("flare://" + relay_ep.ToString()));
  FLARE_CHECK(channels[1].Open("flare://192.0.2.1:56789"));  // timeout?
  FLARE_CHECK(channels[2].Open("flare://127.0.0.1:1"));      // error.

  for (int i = 0; i != 5 || FLAGS_keep_running; ++i) {
    relay_svc.ResetCounters();

    // Call relay.
    std::atomic<std::size_t> failure{}, success{};
    for (int j = 0; j != 10; ++j) {
      std::vector<Future<>> fs;
      std::vector<RpcClientController> ctlr(kConcurrency);
      // User-defined conversion is allowed at most once in user-defined
      // conversion sequence, thus we cannot initialize stub with `RpcChannel*`.
      //
      // @sa: https://stackoverflow.com/a/15968967
      testing::RelayService_SyncStub stubs[3] = {
          {&channels[0]}, {&channels[1]}, {&channels[2]}};
      testing::RelayRequest req;
      req.set_body("1");
      for (int k = 0; k != kConcurrency; ++k) {
        fs.push_back(fiber::Async([&, k] {
          ctlr[k].SetTimeout(150ms);
          auto res = stubs[Random(2)].Relay(req, &ctlr[k]);
          if (!res) {
            ++failure;
          } else {
            ++success;
            EXPECT_EQ("1", res->body());
          }
        }));
      }

      // Wrong service.
      testing::EchoService_SyncStub echo_stub(&channels[0]);
      RpcClientController echo_ctlr;
      ASSERT_FALSE(echo_stub.Echo({}, &echo_ctlr));
      fiber::BlockingGet(WhenAll(&fs));
    }

    EXPECT_GT(failure, 0);
    EXPECT_GT(success, 0);

    auto&& [relay_succ, relay_fail] = relay_svc.GetCounters();
    EXPECT_GT(relay_succ, 0);
    EXPECT_GT(relay_fail, 0);

    FLARE_LOG_INFO("{} {} {} {}", failure, success, relay_succ, relay_fail);
  }

  server_group.Stop();
  server_group.Join();
}

}  // namespace flare

FLARE_TEST_MAIN
