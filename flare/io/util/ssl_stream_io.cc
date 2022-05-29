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

#include "flare/io/util/ssl_stream_io.h"

#include <algorithm>
#include <utility>

#include "flare/base/logging.h"
#include "flare/io/util/http_proxy_stream_io.h"

namespace flare {

SslStreamIo::SslStreamIo(std::unique_ptr<AbstractStreamIo> base,
                         std::unique_ptr<SSL, decltype(&SSL_free)> ssl)
    : ssl_(std::move(ssl)), base_(std::move(base)) {
  int fd = -1;
  SystemStreamIo* system_io = dynamic_cast<SystemStreamIo*>(base_.get());
  if (system_io) {
    fd = system_io->GetFd();
  } else {
    HttpProxyStreamIo* proxy_io = dynamic_cast<HttpProxyStreamIo*>(base_.get());
    FLARE_CHECK(proxy_io, "Ssl should have underlying system or proxy io");
    fd = proxy_io->GetFd();
  }

  SSL_set_fd(ssl_.get(), fd);
  SSL_set_connect_state(ssl_.get());
}

SslStreamIo::~SslStreamIo() {}

SslStreamIo::HandshakingStatus SslStreamIo::Handshake() {
  if (!base_handshake_done_) {
    auto status = base_->Handshake();
    if (status != AbstractStreamIo::HandshakingStatus::Success) {
      return status;
    }
    base_handshake_done_ = true;
  }

  int ret = SSL_do_handshake(ssl_.get());
  if (ret != 1) {
    int err = HandleSslError("Handshake", ret);
    if (err == SSL_ERROR_WANT_WRITE) {
      return SslStreamIo::HandshakingStatus::WannaWrite;
    } else if (err == SSL_ERROR_WANT_READ) {
      return SslStreamIo::HandshakingStatus::WannaRead;
    } else {
      // The detailed SSL error is already logged and particularly
      // for the underlying non-blocking sockets, SSL_ERROR_WANT_READ
      // and SSL_ERROR_WANT_WRITE are handled above.
      return SslStreamIo::HandshakingStatus::Error;
    }
  }
  return SslStreamIo::HandshakingStatus::Success;
}

ssize_t SslStreamIo::ReadV(const iovec* iov, int iovcnt) {
  ssize_t ret = DoReadV(iov, iovcnt);
  if (ret <= 0) {
    HandleSslError("Read", ret);
  }
  return ret;
}

// Does copy internally. Slow but works.
//
// TODO(luobogao): Optimize this.
ssize_t SslStreamIo::DoReadV(const iovec* iov, int iovcnt) {
  std::size_t bytes_to_read = 0;
  for (int i = 0; i != iovcnt; ++i) {
    bytes_to_read += iov[i].iov_len;
  }

  // `std::make_unique<char[]>` initializes buffer, so don't use that.
  std::unique_ptr<char[]> buffer(new char[bytes_to_read]);

  // Read from OpenSSL.
  auto bytes_read = SSL_read(ssl_.get(), buffer.get(), bytes_to_read);
  if (FLARE_UNLIKELY(bytes_read <= 0)) {
    return bytes_read;
  }

  // Copy data back to caller's buffer segments.
  int current_iov = 0;
  std::size_t bytes_copied = 0;
  while (bytes_copied != bytes_read) {
    auto&& iove = iov[current_iov++];
    auto len = std::min(iove.iov_len, bytes_read - bytes_copied);
    memcpy(iove.iov_base, buffer.get() + bytes_copied, len);
    bytes_copied += len;
  }
  FLARE_CHECK_LE(current_iov, iovcnt);
  FLARE_CHECK_LE(bytes_copied, bytes_to_read);
  return bytes_copied;
}

// copied from
// https://github.com/httperf/httperf/blob/master/src/lib/ssl_writev.c
ssize_t SslStreamIo::DoWriteV(const iovec* vector, int count) {
  static constexpr std::size_t kMaxLocalSize = 128 * 1024;
  thread_local auto local_buffer = std::make_unique<char[]>(kMaxLocalSize);
  char* buffer = local_buffer.get();
  char* bp;
  size_t bytes, to_copy;
  int i;

  /* Find the total number of bytes to be written.  */
  bytes = 0;
  for (i = 0; i < count; ++i) bytes += vector[i].iov_len;

  /* Allocate dynamically a temporary buffer to hold the data.  */
  if (bytes > kMaxLocalSize) {
    buffer = reinterpret_cast<char*>(malloc(bytes));
    FLARE_CHECK(buffer);
  }

  /* Copy the data into BUFFER.  */
  to_copy = bytes;
  bp = buffer;
  for (i = 0; i < count; ++i) {
    size_t copy = std::min(vector[i].iov_len, to_copy);

    memcpy(reinterpret_cast<void*>(bp),
           reinterpret_cast<void*>(vector[i].iov_base), copy);
    bp += copy;

    to_copy -= copy;
    if (to_copy == 0) break;
  }

  auto ret = SSL_write(ssl_.get(), buffer, bytes);
  if (bytes > kMaxLocalSize) {
    free(buffer);
  }
  return ret;
}

ssize_t SslStreamIo::WriteV(const iovec* iov, int iovcnt) {
  ssize_t ret = DoWriteV(iov, iovcnt);
  if (ret <= 0) {
    HandleSslError("Write", ret);
  }
  return ret;
}

int SslStreamIo::HandleSslError(const char* operation, int ret) {
  int ssle = SSL_get_error(ssl_.get(), ret);
  if (ssle == SSL_ERROR_WANT_READ || ssle == SSL_ERROR_WANT_WRITE) {
    fiber::SetLastError(EAGAIN);
  } else if (ssle == SSL_ERROR_ZERO_RETURN) {
    // ret should be 0 here.
    FLARE_LOG_WARNING_EVERY_SECOND("SSL error {} errno {} ssle {} ret {}",
                                   operation, fiber::GetLastError(), ssle, ret);
  } else {
    FLARE_LOG_WARNING_EVERY_SECOND("SSL error {} errno {} ssle {} ret {}",
                                   operation, fiber::GetLastError(), ssle, ret);
  }
  return ssle;
}

}  // namespace flare
