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

#include "flare/rpc/protocol/stream_protocol.h"

namespace flare {

FLARE_DEFINE_CLASS_DEPENDENCY_REGISTRY(client_side_stream_protocol_registry,
                                       StreamProtocol);
FLARE_DEFINE_CLASS_DEPENDENCY_REGISTRY(server_side_stream_protocol_registry,
                                       StreamProtocol);

}  // namespace flare
