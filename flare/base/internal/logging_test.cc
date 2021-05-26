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

#include "flare/base/internal/logging.h"

#include <string>
#include <vector>

#include "thirdparty/googletest/gmock/gmock-matchers.h"
#include "thirdparty/googletest/gtest/gtest.h"

namespace foreign_ns {

struct AwesomeLogSink : public google::LogSink {
  void send(google::LogSeverity severity, const char* full_filename,
            const char* base_filename, int line, const struct ::tm* tm_time,
            const char* message, size_t message_len) override {
    msgs.emplace_back(message, message_len);
  }
  std::vector<std::string> msgs;
};

std::string my_prefix, my_prefix2;

void WriteLoggingPrefix(std::string* s) { *s += my_prefix; }
void WriteLoggingPrefix2(std::string* s) { *s += my_prefix2; }

TEST(Logging, Prefix) {
  AwesomeLogSink sink;
  google::AddLogSink(&sink);

  FLARE_LOG_INFO("something");

  my_prefix = "[prefix]";
  FLARE_LOG_INFO("something");

  my_prefix = "[prefix1]";
  FLARE_LOG_INFO("something");

  my_prefix2 = "[prefix2]";
  FLARE_LOG_INFO("something");

  ASSERT_THAT(sink.msgs,
              ::testing::ElementsAre("something", "[prefix] something",
                                     "[prefix1] something",
                                     "[prefix1] [prefix2] something"));
  google::RemoveLogSink(&sink);
}

}  // namespace foreign_ns

FLARE_INTERNAL_LOGGING_REGISTER_PREFIX_PROVIDER(0,
                                                foreign_ns::WriteLoggingPrefix)
FLARE_INTERNAL_LOGGING_REGISTER_PREFIX_PROVIDER(1,
                                                foreign_ns::WriteLoggingPrefix2)
