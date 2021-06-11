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

#include "flare/base/buffer/compression_output_stream.h"

#include <string>

#include "googletest/gtest/gtest.h"

namespace flare {

TEST(NoncontiguousBufferCompressionOutputStream, All) {
  NoncontiguousBufferBuilder nbb;
  NoncontiguousBufferCompressionOutputStream nbcos(&nbb);
  std::string src = "short str";
  void* data;
  std::size_t size;
  nbcos.Next(&data, &size);
  memcpy(data, src.data(), src.size());
  nbcos.BackUp(size - src.size());
  nbcos.Flush();
  EXPECT_EQ(src, FlattenSlow(nbb.DestructiveGet()));
}

}  // namespace flare
