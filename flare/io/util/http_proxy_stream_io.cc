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

#include "flare/io/util/http_proxy_stream_io.h"

#include <memory>
#include <utility>

#include "flare/base/logging.h"
#include "flare/base/string.h"

using namespace std::literals;

namespace flare {

HttpProxyStreamIo::HttpProxyStreamIo(std::unique_ptr<AbstractStreamIo> base,
                                     const std::string& addr)
    : addr_(addr), base_(std::move(base)) {
  FLARE_CHECK(base_, "Proxy should have underlying io");
  handshake_message_ = "CONNECT " + addr + " HTTP/1.1\r\n\r\n";
}

HttpProxyStreamIo::~HttpProxyStreamIo() {}

AbstractStreamIo::HandshakingStatus HttpProxyStreamIo::Handshake() {
  if (!base_handshake_done_) {
    auto status = base_->Handshake();
    if (status != AbstractStreamIo::HandshakingStatus::Success) {
      return status;
    }
    base_handshake_done_ = true;
  }

  if (written_ < handshake_message_.size()) {
    return DoHandshakeWrite();
  }
  return DoHandshakeRead();
}

int HttpProxyStreamIo::GetFd() {
  SystemStreamIo* system_io = dynamic_cast<SystemStreamIo*>(base_.get());
  FLARE_CHECK(system_io, "Now support only system stream io.");
  return system_io->GetFd();
}

AbstractStreamIo::HandshakingStatus HttpProxyStreamIo::DoHandshakeWrite() {
  iovec write_buf;
  write_buf.iov_base = handshake_message_.data() + written_;
  write_buf.iov_len = handshake_message_.size() - written_;
  auto n = WriteV(&write_buf, 1);
  if (FLARE_UNLIKELY(n == 0)) {
    return AbstractStreamIo::HandshakingStatus::Error;
  }
  if (FLARE_UNLIKELY(n < 0)) {
    auto err = fiber::GetLastError();
    if (err == EAGAIN || err == EWOULDBLOCK) {
      return AbstractStreamIo::HandshakingStatus::WannaWrite;
    } else {
      return AbstractStreamIo::HandshakingStatus::Error;
    }
  }
  written_ += n;
  if (FLARE_LIKELY(written_ == handshake_message_.size())) {
    return AbstractStreamIo::HandshakingStatus::WannaRead;
  }
  return AbstractStreamIo::HandshakingStatus::WannaWrite;
}

AbstractStreamIo::HandshakingStatus HttpProxyStreamIo::DoHandshakeRead() {
  // Response just contains a start-line normally, 256 should be enough.
  constexpr auto kBufLength = 256;
  char read_buf[kBufLength];
  iovec iov[1] = {{.iov_base = read_buf, .iov_len = kBufLength}};
  auto n = ReadV(iov, 1);
  if (FLARE_UNLIKELY(n == 0)) {
    return AbstractStreamIo::HandshakingStatus::Error;
  }
  if (FLARE_UNLIKELY(n < 0)) {
    auto err = fiber::GetLastError();
    if (err == EAGAIN || err == EWOULDBLOCK) {
      return AbstractStreamIo::HandshakingStatus::WannaRead;
    } else {
      return AbstractStreamIo::HandshakingStatus::Error;
    }
  }
  read_.append(read_buf, n);
  if (FLARE_LIKELY(EndsWith(read_, "\r\n\r\n"sv))) {
    std::string start_line = read_.substr(0, read_.find_first_of("\r\n"sv));
    auto splited = Split(start_line, " "sv);
    if (splited.size() < 2) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Proxy handshake with addr {} response format error {}", addr_,
          start_line);
      return AbstractStreamIo::HandshakingStatus::Error;
    }
    if (splited[1] != "200"sv) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Proxy handshake fail with addr {} code {}", addr_, splited[1]);
      return AbstractStreamIo::HandshakingStatus::Error;
    }
    return AbstractStreamIo::HandshakingStatus::Success;
  } else {
    if (read_.size() >= kBufLength) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Proxy handshake response too long with addr {} {}", addr_, read_);
      return AbstractStreamIo::HandshakingStatus::Error;
    }
    return AbstractStreamIo::HandshakingStatus::WannaRead;
  }
}

ssize_t HttpProxyStreamIo::ReadV(const iovec* iov, int iovcnt) {
  return base_->ReadV(iov, iovcnt);
}

ssize_t HttpProxyStreamIo::WriteV(const iovec* iov, int iovcnt) {
  return base_->WriteV(iov, iovcnt);
}

}  // namespace flare
