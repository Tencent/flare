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

#include "flare/base/logging.h"

#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "flare/base/chrono.h"

namespace foreign_ns {

struct AwesomeLogSink : public google::LogSink {
  void send(google::LogSeverity severity, const char* full_filename,
            const char* base_filename, int line, const struct ::tm* tm_time,
            const char* message, size_t message_len) override {
    ++count;
  }
  std::atomic<int> count{};
};

TEST(Logging, LogEverySecond) {
  AwesomeLogSink sink;
  google::AddLogSink(&sink);

  std::vector<std::thread> ts(100);
  for (auto&& t : ts) {
    t = std::thread([] {
      auto start = flare::ReadSteadyClock();
      while (start + std::chrono::seconds(10) > flare::ReadSteadyClock()) {
        FLARE_LOG_WARNING_EVERY_SECOND("Some warning.");
      }
    });
  }
  for (auto&& t : ts) {
    t.join();
  }
  google::RemoveLogSink(&sink);
  ASSERT_NEAR(11 /* Plus the initial one. */, sink.count, 1);
}

}  // namespace foreign_ns
