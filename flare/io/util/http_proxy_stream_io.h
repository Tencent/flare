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

#ifndef FLARE_IO_UTIL_HTTP_PROXY_STREAM_IO_H_
#define FLARE_IO_UTIL_HTTP_PROXY_STREAM_IO_H_

#include <memory>
#include <string>

#include "flare/io/util/stream_io.h"

namespace flare {

class HttpProxyStreamIo : public AbstractStreamIo {
 public:
  HttpProxyStreamIo(std::unique_ptr<AbstractStreamIo> base,
                    const std::string& addr);
  ~HttpProxyStreamIo();

  HandshakingStatus Handshake() override;

  ssize_t ReadV(const iovec* iov, int iovcnt) override;
  ssize_t WriteV(const iovec* iov, int iovcnt) override;

  int GetFd();

 private:
  HandshakingStatus DoHandshakeWrite();
  HandshakingStatus DoHandshakeRead();

  int written_ = 0;
  bool base_handshake_done_ = false;

  std::string read_;
  std::string handshake_message_;
  std::string addr_;
  std::unique_ptr<AbstractStreamIo> base_;
};

}  // namespace flare

#endif  // FLARE_IO_UTIL_HTTP_PROXY_STREAM_IO_H_
