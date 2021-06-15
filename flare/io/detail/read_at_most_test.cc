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

#include "flare/io/detail/read_at_most.h"

#include <fcntl.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <thread>

#include "gtest/gtest.h"

#include "flare/base/random.h"
#include "flare/io/util/socket.h"
#include "flare/testing/main.h"

using namespace std::literals;

namespace flare::io::detail {

class ReadAtMostTest : public ::testing::Test {
 public:
  void SetUp() override {
    PCHECK(pipe(fd_) == 0);
    PCHECK(write(fd_[1], "1234567", 7) == 7);
    util::SetNonBlocking(fd_[0]);
    util::SetNonBlocking(fd_[1]);
    io_ = std::make_unique<SystemStreamIo>(fd_[0]);
    buffer_.Clear();
  }
  void TearDown() override {
    // Failure is ignored. If the testcase itself closed the socket, call(s)
    // below can fail.
    close(fd_[0]);
    close(fd_[1]);
  }

 protected:
  int fd_[2];  // read fd, write fd.
  std::unique_ptr<SystemStreamIo> io_;
  NoncontiguousBuffer buffer_;
  std::size_t bytes_read_;
};

TEST_F(ReadAtMostTest, Drained) {
  ASSERT_EQ(ReadStatus::Drained,
            ReadAtMost(8, io_.get(), &buffer_, &bytes_read_));
  EXPECT_EQ("1234567", FlattenSlow(buffer_));
  EXPECT_EQ(7, bytes_read_);
}

TEST_F(ReadAtMostTest, Drained2) {
  buffer_ = CreateBufferSlow("0000");
  ASSERT_EQ(ReadStatus::Drained,
            ReadAtMost(8, io_.get(), &buffer_, &bytes_read_));
  EXPECT_EQ("00001234567", FlattenSlow(buffer_));
  EXPECT_EQ(7, bytes_read_);
}

TEST_F(ReadAtMostTest, MaxBytesRead) {
  ASSERT_EQ(ReadStatus::MaxBytesRead,
            ReadAtMost(7, io_.get(), &buffer_, &bytes_read_));
  EXPECT_EQ("1234567", FlattenSlow(buffer_));
  EXPECT_EQ(7, bytes_read_);
}

TEST_F(ReadAtMostTest, MaxBytesRead2) {
  ASSERT_EQ(ReadStatus::MaxBytesRead,
            ReadAtMost(5, io_.get(), &buffer_, &bytes_read_));
  EXPECT_EQ("12345", FlattenSlow(buffer_));
  EXPECT_EQ(5, bytes_read_);
}

TEST_F(ReadAtMostTest, PeerClosing) {
  FLARE_PCHECK(close(fd_[1]) == 0);
  ASSERT_EQ(ReadStatus::Drained,
            ReadAtMost(8, io_.get(), &buffer_, &bytes_read_));
  EXPECT_EQ(7, bytes_read_);
  // This is weird. The first call always succeeds even if it can tell the
  // remote side has closed the socket, yet we still need to issue another call
  // to `read` to see the situation.
  ASSERT_EQ(ReadStatus::PeerClosing,
            ReadAtMost(1, io_.get(), &buffer_, &bytes_read_));
  EXPECT_EQ(0, bytes_read_);
  EXPECT_EQ("1234567", FlattenSlow(buffer_));
}

TEST(ReadAtMost, LargeChunk) {
  // @sa: https://man7.org/linux/man-pages/man2/fcntl.2.html
  //
  // > Note that because of the way the pages of the pipe buffer are employed
  // > when data is written to the pipe, the number of bytes that can be written
  // > may be less than the nominal size, depending on the size of the writes.
  constexpr auto kMaxBytes = 1048576;  // @sa: `/proc/sys/fs/pipe-max-size`

  int fd[2];  // read fd, write fd.
  FLARE_PCHECK(pipe(fd) == 0);
  FLARE_PCHECK(fcntl(fd[0], F_SETPIPE_SZ, kMaxBytes) == kMaxBytes);
  util::SetNonBlocking(fd[0]);
  util::SetNonBlocking(fd[1]);
  auto io = std::make_unique<SystemStreamIo>(fd[0]);

  std::string source;
  for (int i = 0; i != kMaxBytes; ++i) {
    source.push_back(Random<char>());
  }

  for (int i = 1; i < kMaxBytes; i = i * 3 / 2 + 17) {
    FLARE_LOG_INFO("Testing chunk of size {}.", i);

    FLARE_PCHECK(write(fd[1], source.data(), i) == i);

    NoncontiguousBuffer buffer;
    std::size_t bytes_read;

    if (Random() % 2 == 0) {
      ASSERT_EQ(ReadStatus::Drained,
                ReadAtMost(i + 1, io.get(), &buffer, &bytes_read));
    } else {
      ASSERT_EQ(ReadStatus::MaxBytesRead,
                ReadAtMost(i, io.get(), &buffer, &bytes_read));
    }
    EXPECT_EQ(i, bytes_read);
    // Not using `EXPECT_EQ` as diagnostics on error is potentially large, so we
    // want to bail out on error ASAP.
    ASSERT_EQ(FlattenSlow(buffer), source.substr(0, i));
  }
}

}  // namespace flare::io::detail

FLARE_TEST_MAIN
