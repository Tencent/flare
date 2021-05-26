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

#include "flare/net/hbase/hbase_server_controller.h"

#include <string>

#include "flare/base/internal/early_init.h"

namespace flare {

const std::string& HbaseServerController::GetEffectiveUser() const {
  return conn_header_ ? conn_header_->user_info().effective_user()
                      : internal::EarlyInitConstant<std::string>();
}

const std::string& HbaseServerController::GetCellBlockCodec() const {
  return conn_header_ ? conn_header_->cell_block_codec_class()
                      : internal::EarlyInitConstant<std::string>();
}

const std::string& HbaseServerController::GetCellBlockCompressor() const {
  return conn_header_ ? conn_header_->cell_block_compressor_class()
                      : internal::EarlyInitConstant<std::string>();
}

void HbaseServerController::Reset() {
  HbaseControllerCommon::Reset();
  conn_header_ = nullptr;
}

void HbaseServerController::SetConnectionHeader(
    const hbase::ConnectionHeader* conn_header) {
  conn_header_ = conn_header;
}

}  // namespace flare
