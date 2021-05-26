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

#include "flare/testing/hbase_controller.h"

#include <utility>

namespace flare::testing {

namespace detail {

void HbaseControllerMaster::SetResponseCellBlock(HbaseClientController* ctlr,
                                                 NoncontiguousBuffer buffer) {
  ctlr->SetResponseCellBlock(std::move(buffer));
}

void HbaseControllerMaster::SetRequestCellBlock(HbaseServerController* ctlr,
                                                NoncontiguousBuffer buffer) {
  ctlr->SetRequestCellBlock(std::move(buffer));
}

}  // namespace detail

void SetHbaseClientResponseCellBlock(HbaseClientController* ctlr,
                                     NoncontiguousBuffer buffer) {
  detail::HbaseControllerMaster::SetResponseCellBlock(ctlr, std::move(buffer));
}

void SetHbaseServerRequestCellBlock(HbaseServerController* ctlr,
                                    NoncontiguousBuffer buffer) {
  detail::HbaseControllerMaster::SetRequestCellBlock(ctlr, std::move(buffer));
}

}  // namespace flare::testing
