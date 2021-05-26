// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_TESTING_NAKED_SERVER_H_
#define FLARE_TESTING_NAKED_SERVER_H_

#include <mutex>
#include <vector>

#include "flare/base/buffer.h"
#include "flare/base/function.h"
#include "flare/base/handle.h"
#include "flare/base/net/endpoint.h"
#include "flare/base/ref_ptr.h"
#include "flare/io/native/acceptor.h"
#include "flare/io/native/stream_connection.h"

namespace flare::testing {

// This class provides a "naked" server. It provides user with raw bytes without
// parsing it. This allows us to write "mock" server whose protocol is not
// otherwise supported by Flare.
//
// FOR TESTING PURPOSE ONLY.
class NakedServer {
 public:
  ~NakedServer();

  // Set a handler for handling incoming bytes. If `false` is returned, the
  // connection is closed.
  void SetHandler(
      Function<bool(StreamConnection*, NoncontiguousBuffer*)> handler);

  // Listen on the given address and start serving.
  void ListenOn(const Endpoint& addr);
  void Start();

  // Shutdown the server.
  void Stop();
  void Join();

 private:
  class ConnectionHandler;

  void OnConnection(Handle fd, Endpoint peer);

 private:
  bool stopped_ = false;
  Function<bool(StreamConnection*, NoncontiguousBuffer*)> handler_;
  Endpoint listening_on_;

  RefPtr<NativeAcceptor> acceptor_;
  std::mutex conns_lock_;
  std::vector<RefPtr<NativeStreamConnection>> conns_;
};

}  // namespace flare::testing

#endif  // FLARE_TESTING_NAKED_SERVER_H_
