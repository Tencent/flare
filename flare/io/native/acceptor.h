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

#ifndef FLARE_IO_NATIVE_ACCEPTOR_H_
#define FLARE_IO_NATIVE_ACCEPTOR_H_

#include "flare/base/function.h"
#include "flare/base/net/endpoint.h"
#include "flare/io/acceptor.h"
#include "flare/io/descriptor.h"

namespace flare {

// `NativeAcceptor` listens on a TCP port for incoming connections.
class NativeAcceptor final : public Descriptor, public Acceptor {
 public:
  struct Options {
    // Called when new connection is accepted.
    //
    // The handler is responsible for setting `FD_CLOEXEC` / `O_NONBLOCK` and
    // whatever it sees need.
    //
    // CAVEAT: Due to technical limitations, it's likely that
    // `connection_handler` is not called in a balanced fashion if same `fd`
    // (see above) is associated with `NativeAcceptor`s bound with different
    // `EventLoop`. You may have to write your own logic to balance work loads.
    // This imbalance is probably significant if you're trying to use multiple
    // `NativeAcceptor`s to balance workloads between several program-defined
    // worker domains. A possible choice would be dispatch the requests into
    // each NUMA domain in a round-robin fashion in `connection_handler`, for
    // example.
    Function<void(Handle fd, Endpoint peer)> connection_handler;
  };

  // O_NONBLOCK must be set on `fd`. Ownership is taken.
  //
  // The caller is responsible for bind / listen / etc.. `NativeAcceptor` is
  // only responsible for "accept"ing connections from `fd`.
  explicit NativeAcceptor(Handle fd, Options options);

  void Stop() override;
  void Join() override;

 private:
  EventAction OnReadable() override;
  EventAction OnWritable() override;  // Abort on call.
  void OnError(int err) override;
  void OnCleanup(CleanupReason reason) override;

 private:
  Options options_;
};

}  // namespace flare

#endif  // FLARE_IO_NATIVE_ACCEPTOR_H_
