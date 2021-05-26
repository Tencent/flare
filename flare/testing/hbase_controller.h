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

#ifndef FLARE_TESTING_HBASE_CONTROLLER_H_
#define FLARE_TESTING_HBASE_CONTROLLER_H_

// When writing UTs, sometimes you might want to access several private fields
// in `HbaseXxxController`. Here we provide some helper methods for you to do
// these.

#include "flare/net/hbase/hbase_client_controller.h"
#include "flare/net/hbase/hbase_server_controller.h"

namespace flare::testing {

namespace detail {

struct HbaseControllerMaster {
  static void SetResponseCellBlock(HbaseClientController* ctlr,
                                   NoncontiguousBuffer buffer);

  static void SetRequestCellBlock(HbaseServerController* ctlr,
                                  NoncontiguousBuffer buffer);
};

}  // namespace detail

void SetHbaseClientResponseCellBlock(HbaseClientController* ctlr,
                                     NoncontiguousBuffer buffer);

void SetHbaseServerRequestCellBlock(HbaseServerController* ctlr,
                                    NoncontiguousBuffer buffer);
}  // namespace flare::testing

#endif  // FLARE_TESTING_HBASE_CONTROLLER_H_
