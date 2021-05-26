// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_EXAMPLE_CUSTOM_PROTOCOL_SERVICE_H_
#define FLARE_EXAMPLE_CUSTOM_PROTOCOL_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "flare/rpc/protocol/stream_service.h"

namespace example::naive_proto {

class Service : public flare::StreamService {
 public:
  const flare::experimental::Uuid& GetUuid() const noexcept override;

  bool Inspect(const flare::Message&, const flare::Controller&,
               InspectionResult*) override;

  bool ExtractCall(const std::string& serialized,
                   const std::vector<std::string>& serialized_pkt_ctxs,
                   ExtractedCall* extracted) override;

  ProcessingStatus FastCall(
      std::unique_ptr<flare::Message>* request,
      const flare::FunctionView<std::size_t(const flare::Message&)>& writer,
      Context* context) override;

  ProcessingStatus StreamCall(
      flare::AsyncStreamReader<std::unique_ptr<flare::Message>>* input_stream,
      flare::AsyncStreamWriter<std::unique_ptr<flare::Message>>* output_stream,
      Context* context) override;

  void Stop() override {}
  void Join() override {}
};

}  // namespace example::naive_proto

#endif  // FLARE_EXAMPLE_CUSTOM_PROTOCOL_SERVICE_H_
