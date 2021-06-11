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

#include "flare/base/compression/util.h"

#include <string>

#include "googletest/gtest/gtest.h"

#include "flare/base/buffer/compression_output_stream.h"

namespace flare::compression {

class TestCompressionOutputStream : public CompressionOutputStream {
 public:
  explicit TestCompressionOutputStream(std::string* s,
                                       std::size_t every_size = 2)
      : every_size_(every_size), buffer_{s} {}
  void Flush() { buffer_->resize(using_bytes_); }
  bool Next(void** data, std::size_t* size) noexcept override {
    if (buffer_->size() < using_bytes_ + every_size_) {
      buffer_->resize(using_bytes_ + every_size_);
    }
    *data = buffer_->data() + using_bytes_;
    *size = every_size_;
    using_bytes_ += every_size_;
    return true;
  }
  void BackUp(std::size_t count) noexcept override { using_bytes_ -= count; }

 private:
  std::size_t using_bytes_{};
  std::size_t every_size_;
  std::string* buffer_;
};

TEST(CopyDataToCompressionOutputStream, All) {
  std::string s;
  std::string a = "123456789+";

  TestCompressionOutputStream out1(&s, 2);
  CopyDataToCompressionOutputStream(&out1, a.data(), 1);
  CopyDataToCompressionOutputStream(&out1, a.data() + 1, 2);
  CopyDataToCompressionOutputStream(&out1, a.data() + 3, 3);
  CopyDataToCompressionOutputStream(&out1, a.data() + 6, 4);
  out1.Flush();

  ASSERT_EQ(s, a);
}

}  // namespace flare::compression
