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

#ifndef FLARE_NET_HBASE_HBASE_CHANNEL_H_
#define FLARE_NET_HBASE_HBASE_CHANNEL_H_

#include <string>

#include "protobuf/service.h"

#include "flare/base/net/endpoint.h"

namespace flare {

namespace rpc::internal {

class StreamCallGateHandle;

}  // namespace rpc::internal

// Channel for making RPCs to HBase server or cluster.
class HbaseChannel : public google::protobuf::RpcChannel {
 public:
  struct Options {
    // Fields used when connecting to server.
    //
    // @sa: rpc.proto
    std::string effective_user;  // Was called `ticket` in `gdt::HbaseChannel`.
    std::string service_name;    // Package names should NOT be included.
    std::string cell_block_compressor;
    std::string cell_block_codec;

    // Maximum packet size. Due to protocol overhead, this should be slightly
    // larger than maximum cell-block size.
    //
    // Don't worry, we won't allocate these bytes immediately, neither won't we
    // keep the buffer such large after it has been consumed. It's just an upper
    // limit to keep you safe in face of a malfunctioning server.
    std::size_t maximum_packet_size = 128 * 1024 * 1024;
  };

  // For the moment we only support URIs in syntax `hbase://host:port`. It's the
  // user's responsibility to locate master or region server beforehand
  // (presumably via requesting ZooKeeper.).
  //
  // `options` must be provided. Several fields in it (e.g. `effective_user`,
  // `service_name`) are required to establish a connection with the server.
  //
  // TODO(luobogao): Can we instead accept something like something like
  // `hbase-zk://zk-host:zk-port/hbase-dir` and locate the server for the user?
  bool Open(const std::string& address, const Options& options);

  // TODO(luobogao): HBase mock.
 private:
  void CallMethod(const google::protobuf::MethodDescriptor* method,
                  google::protobuf::RpcController* controller,
                  const google::protobuf::Message* request,
                  google::protobuf::Message* response,
                  google::protobuf::Closure* done) override;

  void CallMethodNonEmptyDone(const google::protobuf::MethodDescriptor* method,
                              google::protobuf::RpcController* controller,
                              const google::protobuf::Message* request,
                              google::protobuf::Message* response,
                              google::protobuf::Closure* done);

 private:
  rpc::internal::StreamCallGateHandle GetCallGate();

 private:
  Options options_;
  Endpoint server_addr_;
};

}  // namespace flare

#endif  // FLARE_NET_HBASE_HBASE_CHANNEL_H_
