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

#include "flare/io/native/acceptor.h"

#include <utility>

#include "flare/base/logging.h"
#include "flare/io/detail/eintr_safe.h"

namespace flare {

NativeAcceptor::NativeAcceptor(Handle fd, Options options)
    : Descriptor(std::move(fd), Event::Read, "NativeAcceptor"),
      options_(std::move(options)) {}

void NativeAcceptor::Stop() { Kill(CleanupReason::UserInitiated); }

void NativeAcceptor::Join() { WaitForCleanup(); }

Descriptor::EventAction NativeAcceptor::OnReadable() {
  while (true) {
    EndpointRetriever er;
    Handle new_fd(io::detail::EIntrSafeAccept(fd(), er.RetrieveAddr(),
                                              er.RetrieveLength()));
    if (new_fd.Get() >= 0) {
      auto ep = er.Build();
      FLARE_VLOG(10, "Accepted connection from [{}].", ep.ToString());
      // `addr` does not support move, actually.
      options_.connection_handler(std::move(new_fd), std::move(ep));
    } else {
      // @sa: http://man7.org/linux/man-pages/man2/accept.2.html
      if (errno == ECONNABORTED || errno == EPERM || errno == EMFILE ||
          errno == ENFILE || errno == ENOBUFS || errno == ENOMEM) {
        FLARE_LOG_WARNING_EVERY_SECOND(
            "Failed in accepting connection (fd #{}): [{}] {}", fd(), errno,
            strerror(errno));
        continue;
      } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return EventAction::Ready;
      } else {
        FLARE_LOG_FATAL(
            "Unexpected error when accepting connection (fd #{}): [{}] {}",
            fd(), errno, strerror(errno));
      }
    }
  }
}

Descriptor::EventAction NativeAcceptor::OnWritable() {
  FLARE_CHECK(0, "Unexpected: NativeAcceptor::OnWritable.");
}

void NativeAcceptor::OnCleanup(CleanupReason reason) {
  // NOTHING.
}

void NativeAcceptor::OnError(int err) {
  FLARE_LOG_FATAL(
      "Error occurred on acceptor {}, no more connection can be accepted: {}",
      fmt::ptr(this), strerror(err));
}

}  // namespace flare
