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

#include "flare/rpc/internal/error_stream_provider.h"

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/maybe_owning.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare::rpc::detail {

TEST(ErrorStreamReaderProvider, All) {
  auto srp = MakeRefCounted<ErrorStreamReaderProvider<int>>();
  StreamReader<int> sr1(srp);
  ASSERT_EQ(StreamError::IoError, sr1.Peek()->error());
  StreamReader<int> sr2(srp);
  ASSERT_EQ(StreamError::IoError, sr1.Read().error());
  StreamReader<int> sr3(srp);
  sr3.SetExpiration(ReadSteadyClock() + 1s);
  sr3.Close();  // Shouldn't hang.
}

TEST(ErrorStreamWriterProvider, All) {
  auto swp = MakeRefCounted<ErrorStreamWriterProvider<int>>();
  StreamWriter<int> sw(swp);
  ASSERT_FALSE(sw.Write(1));
  ASSERT_FALSE(sw.WriteLast(2));
  sw.Close();
}

}  // namespace flare::rpc::detail

FLARE_TEST_MAIN
