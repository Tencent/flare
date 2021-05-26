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

#ifndef FLARE_IO_UTIL_SSL_STREAM_IO_H_
#define FLARE_IO_UTIL_SSL_STREAM_IO_H_

#include <memory>

#include "thirdparty/openssl/ssl.h"

#include "flare/io/util/stream_io.h"

namespace flare {

class SslStreamIo : public AbstractStreamIo {
 public:
  SslStreamIo(std::unique_ptr<AbstractStreamIo> io,
              std::unique_ptr<SSL, decltype(&SSL_free)> ssl);
  ~SslStreamIo();

  HandshakingStatus Handshake() override;

  // Renegotiation is not supported. If the security protocol asked this, the
  // implementation generates an error.
  ssize_t ReadV(const iovec* iov, int iovcnt) override;

  ssize_t WriteV(const iovec* iov, int iovcnt) override;

 private:
  ssize_t DoReadV(const iovec* iov, int iovcnt);
  ssize_t DoWriteV(const iovec* iov, int iovcnt);

  int HandleSslError(const char* operation, int ret);

 private:
  std::unique_ptr<SSL, decltype(&SSL_free)> ssl_;
  std::unique_ptr<AbstractStreamIo> base_;
  bool base_handshake_done_{false};
};

}  // namespace flare

#endif  // FLARE_IO_UTIL_SSL_STREAM_IO_H_
