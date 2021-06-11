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

#ifndef FLARE_NET_HBASE_HBASE_SERVICE_H_
#define FLARE_NET_HBASE_HBASE_SERVICE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "protobuf/service.h"

#include "flare/rpc/protocol/stream_service.h"

namespace flare {

// This class adapts (a collection of) Protocol Buffers service instances to
// flare framework.
class HbaseService : public StreamService {
 public:
  // This method must be called before **any** `Server` is started.
  void AddService(google::protobuf::Service* service);

  const experimental::Uuid& GetUuid() const noexcept override;

  bool Inspect(const Message& message, const Controller& controller,
               InspectionResult* result) override;

  bool ExtractCall(const std::string& serialized_pkt,
                   const std::vector<std::string>& serialized_pkt_ctxs,
                   ExtractedCall* extracted) override;

  ProcessingStatus FastCall(
      std::unique_ptr<Message>* request,
      const FunctionView<std::size_t(const Message&)>& writer,
      Context* context) override;

  ProcessingStatus StreamCall(
      AsyncStreamReader<std::unique_ptr<Message>>* input_stream,
      AsyncStreamWriter<std::unique_ptr<Message>>* output_stream,
      Context* context) override;

  void Stop() override;
  void Join() override;

 private:
  std::unordered_map<std::string, google::protobuf::Service*> services_;
};

}  // namespace flare

#endif  // FLARE_NET_HBASE_HBASE_SERVICE_H_
