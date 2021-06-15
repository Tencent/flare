// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_PLUGIN_SYNC_DECL_GENERATOR_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_PLUGIN_SYNC_DECL_GENERATOR_H_

#include "google/protobuf/descriptor.h"

#include "flare/rpc/protocol/protobuf/plugin/code_writer.h"

namespace flare::protobuf::plugin {

// This class generates synchronous version of classes.
class SyncDeclGenerator {
 public:
  void GenerateService(const google::protobuf::FileDescriptor* file,
                       const google::protobuf::ServiceDescriptor* service,
                       CodeWriter* writer);

  void GenerateStub(const google::protobuf::FileDescriptor* file,
                    const google::protobuf::ServiceDescriptor* service,
                    CodeWriter* writer);
};

}  // namespace flare::protobuf::plugin

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_PLUGIN_SYNC_DECL_GENERATOR_H_
