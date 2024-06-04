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

#ifndef FLARE_RPC_SERVER_H_
#define FLARE_RPC_SERVER_H_

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "gflags/gflags.h"
#include "google/protobuf/service.h"

#include "flare/base/down_cast.h"
#include "flare/base/exposed_var.h"
#include "flare/base/handle.h"
#include "flare/base/internal/test_prod.h"
#include "flare/base/maybe_owning.h"
#include "flare/base/net/endpoint.h"
#include "flare/base/ref_ptr.h"
#include "flare/base/type_index.h"
#include "flare/fiber/timer.h"
#include "flare/io/util/rate_limiter.h"

DECLARE_int32(flare_rpc_server_max_ongoing_calls);
DECLARE_int32(flare_rpc_server_max_connections);
DECLARE_int32(flare_rpc_server_max_request_queueing_delay);
DECLARE_int32(flare_rpc_server_max_packet_size);
DECLARE_bool(flare_rpc_server_no_builtin_pages);

namespace flare {

namespace rpc::detail {
class ServerConnectionHandler;
class DryRunConnectionHandler;
class NormalConnectionHandler;
}  // namespace rpc::detail

class NativeAcceptor;
class StreamProtocol;
class StreamService;

class HttpFilter;
class HttpHandler;

// This is where the RPC server begins.
//
// Normally a user would:
//
// 1. Add one or more protocols (by name, see our documentation) the server
//    should support.
// 2. Add one or more services / handlers for handling the requests.
// 3. Call `ListenOn()` to start listening.
// 4. `Start()` the server.
class Server {
  template <class T>
  using Factory = Function<std::unique_ptr<T>()>;

 public:
  struct Options {
    // Specifies service name. It's used for exposition purpose.
    //
    // Even though basic functionality should still work, it's recommended to
    // provide this field so that more advanced features (e.g., distributed
    // tracing) would function correctly.
    std::string service_name;

    // If set, builtin HTTP pages (e.g. /inspect/...) are not exposed to
    // outside.
    bool no_builtin_pages = FLAGS_flare_rpc_server_no_builtin_pages;

    // Maximum size of a single RPC packet.
    std::size_t maximum_packet_size = FLAGS_flare_rpc_server_max_packet_size;

    ////////////////////////////////////////////////////////////////
    // Several factors controls how should request be proactively //
    // rejected. They help rejecting request early when we're     //
    // under heavy load.                                          //
    //                                                            //
    // Request is rejected if any of the conditions holds.        //
    ////////////////////////////////////////////////////////////////

    // If we're already busy handling so many requests, further requests are
    // rejected early.
    std::size_t max_concurrent_requests =
        FLAGS_flare_rpc_server_max_ongoing_calls;

    // If we've had so many connections, new connections are rejected.
    std::size_t max_concurrent_connections =
        FLAGS_flare_rpc_server_max_connections;

    // If a given request is delayed (in some sort of queue) for so long before
    // it's actually being scheduled for run, it's rejected.
    //
    // This check is done prior to parsing RPC request.
    //
    // No limitation is applied if 0 is specified.
    std::chrono::nanoseconds max_request_queueing_delay =
        FLAGS_flare_rpc_server_max_request_queueing_delay *
        std::chrono::milliseconds(1);

    // This callback allows you to block or allow certain IP to connect to your
    // server.
    //
    // Returns `true` if the connection should be allowed
    Function<bool(const Endpoint&)> conn_filter = [](auto&&) { return true; };
  };

  Server();  // Equivelent to `Server(Options())`;
  explicit Server(Options options);
  ~Server();

  ////////////////////////////////////
  // Protocol agnostic interfaces.  //
  ////////////////////////////////////

  // In most cases you should be adding protocol by name. Adding protocol by
  // its factory should only be used if you're adding ad hoc protocols which is
  // not registered (via our "class registration" mechanism) beforehand.
  void AddProtocol(const std::string& name);

  // Left here to catch unnoticed call to it. TODO(luobogao): Remove it later.
  void AddProtocol(std::initializer_list<std::string>) = delete;

  // For experts' use.
  void AddProtocol(Factory<StreamProtocol> factory);

  // Shorthand for adding multiple protocols at once.
  void AddProtocols(const std::vector<std::string>& names);

  // Supports `AF_INET` / `AF_INET6` / `AF_UNIX` (untested).
  //
  // Calling this method multiple times results in undefined behavior.
  void ListenOn(const Endpoint& addr, int backlog = 128);

  // TODO(luobogao): void ListenOn(..., TlsContext);

  // Once `Start()` is called, all of the above may no longer be called unless
  // otherwise stated.
  bool Start();

  void Stop();
  void Join();

  ///////////////////////////////
  // HTTP-related interfaces.  //
  ///////////////////////////////

  // Add HTTP filter. Filters are called unconditionally for *ALL* HTTP requests
  // **synchronously**. So make your implementation quick to be responsive.
  //
  // Special note: Be aware that, HTTP filters are only applicable to HTTP
  // requests. For non-HTTP requests (including HTTP-alike protocols such as
  // http+pb, poppy, ...), HTTP filters are not applied.
  void AddHttpFilter(MaybeOwningArgument<HttpFilter> filter);

  // Add HTTP handler.
  void AddHttpHandler(std::string path,
                      MaybeOwningArgument<HttpHandler> handler);

  // Add HTTP handler. This handler is called for all URIs matches `path`.
  void AddHttpHandler(std::regex path,
                      MaybeOwningArgument<HttpHandler> handler);

  // Add HTTP prefix handler.
  void AddHttpPrefixHandler(std::string prefix,
                            MaybeOwningArgument<HttpHandler> handler);

  // If set, HTTP requests that are not otherwised handled by handlers / filters
  // registered above are handed to this handler.
  void SetDefaultHttpHandler(MaybeOwningArgument<HttpHandler> handler);

  ///////////////////////////////////////////
  // Protocol Buffers related interfaces.  //
  ///////////////////////////////////////////

  // Add services generated by Protocol Buffers.
  void AddService(MaybeOwningArgument<google::protobuf::Service> service);

  ///////////////////////////////
  // Experts-only interfaces.  //
  ///////////////////////////////

  // This method allows you to add "native" services (class implementing
  // `StreamService`). Note that, however, in most cases, adding two native
  // services of the same type (in terms of C++ class) is an error. Because each
  // native service should be able to individually determine if there's no
  // handler for a given message (and send a response accordingly), two native
  // services being able to handle the same type of message will likely
  // complicate this logic. Meanwhile, shortcuts above should likely satisfy
  // your need, use them instead. This method is for experts' use.
  void AddNativeService(MaybeOwningArgument<StreamService> service);

  // If not enabled yet, `T` is enabled by this call internally.
  //
  // It's permitted to call this method after `Start()` is called under the
  // condition that the caller knows `T` has already been enabled before.
  template <class T>
  T* GetBuiltinNativeService();

 private:
  FLARE_FRIEND_TEST(Server, RemoveIdleConnection);

  friend class rpc::detail::NormalConnectionHandler;
  friend class rpc::detail::DryRunConnectionHandler;

  struct ConnectionContext;

  enum class ServerState { Initialized, Running, Stopped, Joined };

  Json::Value DumpInternals();

 private:
  void OnConnection(Handle fd, Endpoint peer);

  // Called when a new call come. (Note that for stream calls, only the first
  // message triggers this callback.).
  //
  // Returns false if the new call should be dropped.
  bool OnNewCall();

  // Called when a call permitted by `OnNewCall` has completed.
  void OnCallCompletion();

  // Caller is responsible for removing connection from the event loop.
  void OnConnectionClosed(std::uint64_t id);

  // Called periodically to remove idle connections.
  void OnConnectionCleanupTimer();

  // Create a new connection handler for normal request processing.
  std::unique_ptr<rpc::detail::ServerConnectionHandler>
  CreateNormalConnectionHandler(std::uint64_t id, Endpoint peer);

  // Create a new connection handler for dry-run environment.
  std::unique_ptr<rpc::detail::ServerConnectionHandler>
  CreateDryRunConnectionHandler(std::uint64_t id, Endpoint peer);

  // Start a background job.
  void StartBackgroundJob(Function<void()> cb);

 private:
  Options options_;

  ServerState state_{ServerState::Initialized};

  // Timer responsible for closing idle connections.
  std::uint64_t idle_conn_cleaner_;

  // Set by `ListenOn`.
  Endpoint listening_on_;
  Function<void()> listen_cb_;  // Called by `Start`.

  // Number of alive connections. This is used by `Join()` to wait for all
  // connections to be fully closed.
  std::atomic<std::size_t> alive_conns_{0};

  // Builtin HTTP handlers. Automatically registered in `Start`.
  std::vector<std::pair<std::unique_ptr<HttpHandler>, std::vector<std::string>>>
      builtin_http_handlers_;  // Handler -> Paths
  std::vector<std::pair<std::unique_ptr<HttpHandler>, std::string>>
      builtin_http_prefix_handlers_;  // Handler -> Paths

  // Adding a protocol twice is not an error, so we check for duplicate here.
  std::unordered_set<std::string> known_protocols_;

  std::vector<Factory<StreamProtocol>> protocol_factories_;
  RefPtr<NativeAcceptor> acceptor_;

  //  Contains pointers into `services_`. It's used by shortcut methods for
  //  adding services / HTTP handlers / ...
  //
  // `TypeIndex` -> `protobuf::Service*`.
  std::unordered_map<TypeIndex, StreamService*> builtin_services_;
  std::deque<MaybeOwning<StreamService>> services_;

  // Number of on-going calls.
  std::atomic<std::size_t> ongoing_calls_{0};

  std::mutex conns_lock_;  // Likely to content for short-lived connections.
  // Using map here for easier removal (on connection close). ctx.id -> ctx*.
  std::unordered_map<std::uint64_t, std::unique_ptr<ConnectionContext>> conns_;

  // Number of outstanding `StartBackgroundJob`.
  std::atomic<std::size_t> outstanding_jobs_{};

  // Exposes some internal state.
  ExposedVarDynamic<Json::Value> internal_exposer_;
};

template <class T>
T* Server::GetBuiltinNativeService() {
  auto key = GetTypeIndex<T>();
  auto iter = builtin_services_.find(key);
  if (iter == builtin_services_.end()) {
    FLARE_CHECK(!acceptor_,
                "GetBuiltinNativeService() is only usable for finding services "
                "that has been enabled once `Start()` is called.");
    services_.push_back(std::make_unique<T>());
    builtin_services_[key] = services_.back().Get();
    iter = builtin_services_.find(key);
  }
  return down_cast<T>(iter->second);
}

}  // namespace flare

#endif  // FLARE_RPC_SERVER_H_
