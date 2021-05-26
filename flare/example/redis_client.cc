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

#include "thirdparty/gflags/gflags.h"

#include "flare/base/string.h"
#include "flare/init.h"
#include "flare/init/override_flag.h"
#include "flare/net/redis/redis_client.h"

using namespace std::literals;

FLARE_OVERRIDE_FLAG(logtostderr, true);

DEFINE_string(server, "redis://127.0.0.1:6379", "");
DEFINE_string(password, "", "Password for connecting Redis.");
DEFINE_string(cmd, "", "Command to send. Seperated by space");
DEFINE_int32(timeout, 1000, "Timeout in milliseconds.");

namespace example {

int Entry(int argc, char** argv) {
  flare::RedisChannel channel(
      FLAGS_server, flare::RedisChannel::Options{.password = FLAGS_password});
  flare::RedisClient client(&channel);

  auto cmds = flare::Split(FLAGS_cmd, " ");
  flare::RedisCommand command(
      cmds[0], std::vector<std::string_view>(cmds.begin() + 1, cmds.end()));
  auto result = client.Execute(command, FLAGS_timeout * 1ms);
  if (auto str = result.try_as<flare::RedisString>()) {
    FLARE_LOG_INFO("Received a string: {}", *str);
  } else if (auto integer = result.try_as<flare::RedisInteger>()) {
    FLARE_LOG_INFO("Received an integer: {}.", *integer);
  } else if (auto bytes = result.try_as<flare::RedisBytes>()) {
    FLARE_LOG_INFO("Received {} bytes.", bytes->ByteSize());
  } else if (auto array = result.try_as<flare::RedisArray>()) {
    FLARE_LOG_INFO("Received an array of {} elements.", array->size());
  } else if (auto error = result.try_as<flare::RedisError>()) {
    FLARE_LOG_INFO("Received an error of category [{}]: {}", error->category,
                   error->message);
  } else if (result.try_as<flare::RedisNull>()) {
    FLARE_LOG_INFO("Received a null.");
  } else {
    FLARE_LOG_ERROR("Unrecognized result type from Redis.");
  }
  return 0;
}

}  // namespace example

int main(int argc, char** argv) {
  return flare::Start(argc, argv, example::Entry);
}
