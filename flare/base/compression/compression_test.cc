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

#include <functional>
#include <string>

#include "thirdparty/googletest/gtest/gtest.h"

#include "flare/base/buffer/compression_output_stream.h"
#include "flare/base/compression.h"
#include "flare/testing/main.h"

namespace flare {

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

// Param test
class CompressorTest : public ::testing::TestWithParam<const char*> {
 protected:
  void SetUp() override {
    std::string param = GetParam();
    auto&& pos = param.find("_");
    if (pos == std::string::npos) {
      method_ = param;
      with_test_output_stream_ = false;
    } else {
      method_ = param.substr(0, pos);
      FLARE_CHECK_EQ("with_test_output_stream", param.substr(pos + 1));
      with_test_output_stream_ = true;
    }
    std::cout << "Testing " << method_ << "\n";
  }
  std::string CompressBuffer(const NoncontiguousBuffer& nb) {
    return Compress([nb = std::move(nb)](auto&& c, auto&& out) {
      return c->Compress(nb, out);
    });
  }
  std::string CompressString(const std::string& str) {
    return Compress([&](auto&& c, auto&& out) {
      return c->Compress(str.data(), str.size(), out);
    });
  }
  std::string Compress(
      std::function<bool(Compressor*, CompressionOutputStream* out)> f) {
    if (!with_test_output_stream_) {
      NoncontiguousBufferBuilder builder;
      NoncontiguousBufferCompressionOutputStream out(&builder);
      auto&& c = MakeCompressor(method_);
      EXPECT_TRUE(c);
      EXPECT_TRUE(f(c.get(), &out));
      out.Flush();
      return FlattenSlow(builder.DestructiveGet());
    } else {
      std::string builder;
      TestCompressionOutputStream out(&builder);
      auto&& c = MakeCompressor(method_);
      EXPECT_TRUE(c);
      EXPECT_TRUE(f(c.get(), &out));
      out.Flush();
      return builder;
    }
  }
  std::string DecompressBuffer(const NoncontiguousBuffer& nb) {
    return Decompress([nb = std::move(nb)](auto&& c, auto&& out) {
      return c->Decompress(nb, out);
    });
  }
  std::string DecompressString(const std::string& str) {
    return Decompress([&](auto&& c, auto&& out) {
      return c->Decompress(str.data(), str.size(), out);
    });
  }
  std::string Decompress(
      std::function<bool(Decompressor*, CompressionOutputStream* out)> f) {
    if (!with_test_output_stream_) {
      NoncontiguousBufferBuilder builder;
      NoncontiguousBufferCompressionOutputStream out(&builder);
      auto&& c = MakeDecompressor(method_);
      EXPECT_TRUE(c);
      EXPECT_TRUE(f(c.get(), &out));
      out.Flush();
      return FlattenSlow(builder.DestructiveGet());
    } else {
      std::string builder;
      TestCompressionOutputStream out(&builder);
      auto&& c = MakeDecompressor(method_);
      EXPECT_TRUE(c);
      EXPECT_TRUE(f(c.get(), &out));
      out.Flush();
      return builder;
    }
  }
  std::string method_;
  bool with_test_output_stream_ = false;
};

TEST_P(CompressorTest, Empty) {
  std::string original;
  EXPECT_EQ(original, DecompressString(CompressString(original)));
}

TEST_P(CompressorTest, SmallSize) {
  std::string original(1024, 'a');
  EXPECT_EQ(original, DecompressString(CompressString(original)));
}

TEST_P(CompressorTest, LargeSize) {
  std::string original(10 * 1024 * 1024, 'a');
  EXPECT_EQ(original, DecompressString(CompressString(original)));
}

TEST_P(CompressorTest, Stream) {
  std::string original1(16, 'a');
  std::string original2(16, 'b');
  std::string original3(16, 'c');
  NoncontiguousBufferBuilder builder;
  builder.Append(original1.data(), original1.size());
  builder.Append(original2.data(), original2.size());
  builder.Append(original3.data(), original3.size());
  auto compressed = CompressBuffer(builder.DestructiveGet());
  NoncontiguousBufferBuilder builder2;
  builder2.Append(compressed.data(), compressed.size() / 3);
  builder2.Append(compressed.data() + compressed.size() / 3,
                  compressed.size() / 3);
  builder2.Append(compressed.data() + 2 * (compressed.size() / 3),
                  compressed.size() - 2 * (compressed.size() / 3));
  // builder2.Append(compressed.data(), compressed.size());
  auto decompressed = DecompressBuffer(builder2.DestructiveGet());
  EXPECT_EQ(decompressed, original1 + original2 + original3);
}

TEST_P(CompressorTest, Reuse) {
  std::string original(1024, 'a');
  auto&& c = MakeCompressor(method_);
  EXPECT_TRUE(c);
  auto&& d = MakeDecompressor(method_);
  EXPECT_TRUE(d);
  for (auto i = 0; i < 10; ++i) {
    NoncontiguousBufferBuilder builder;
    NoncontiguousBufferCompressionOutputStream out(&builder);
    c->Compress(original.data(), original.size(), &out);
    out.Flush();
    NoncontiguousBufferBuilder builder2;
    NoncontiguousBufferCompressionOutputStream out2(&builder2);
    d->Decompress(builder.DestructiveGet(), &out2);
    out2.Flush();
    EXPECT_EQ(original, FlattenSlow(builder2.DestructiveGet()));
  }
}

TEST_P(CompressorTest, LargeChunk) {
  const size_t kLargeSize = 2 * 1024 * 1024ULL;
  std::string original(kLargeSize, 'a');
  CompressString(original);
}

TEST_P(CompressorTest, LargeChunk2) {
  const size_t kLargeSize = 2 * 1024 * 1024ULL;
  CompressBuffer(CreateBufferSlow(std::string(kLargeSize, 'a')));
}

TEST_P(CompressorTest, VariantSize) {
  // Surprisingly, this UT is slow for Zstd. It seems that Zstd is good at
  // compressing a bunch of byte stream, but not as good at frequently resetting
  // its internal state?
  for (int i = 1; i <= 123450; i += 13) {
    CompressBuffer(CreateBufferSlow(std::string(i, 'a')));
  }
}

// NOTE: Huge size tests are very time consuming, so we disable them by default.
// Please make sure to run them manually before commit if you modify this
// module. command:
//   blade test -- --gtest_also_run_disabled_tests

TEST_P(CompressorTest, DISABLED_HugeSize) {
  const size_t kHugeTestSize = 2 * 1024 * 1024 * 1024ULL;
  std::string original(kHugeTestSize, 'a');
  CompressString(original);
}

INSTANTIATE_TEST_SUITE_P(
    CompressorTest, CompressorTest,
    ::testing::Values("gzip", "gzip_with_test_output_stream", "lz4-frame",
                      "lz4-frame_with_test_output_stream", "snappy",
                      "snappy_with_test_output_stream", "zstd"));

}  // namespace flare

FLARE_TEST_MAIN
