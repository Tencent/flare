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

#ifndef FLARE_RPC_BINLOG_TEXT_ONLY_DUMPER_H_
#define FLARE_RPC_BINLOG_TEXT_ONLY_DUMPER_H_

#include <fstream>
#include <memory>
#include <mutex>
#include <string>

#include "flare/rpc/binlog/dumper.h"

namespace flare::binlog {

// This dumper dumps RPCs as plain-text. It's provided primarily for debugging
// purpose.
//
// Note that it does NOT perform well, and its output can change over time to
// time, you shouldn't use it in production.
class TextOnlyDumper : public Dumper {
 public:
  struct Options {
    // No, auto-spliting is not supported, as this dumper is for debugging
    // purpose anyway.
    std::string filename;
  };
  explicit TextOnlyDumper(const Options& options);

  std::unique_ptr<DumpingLog> StartDumping() override;

  void Write(const std::string& entry);

 private:
  Options options_;
  std::mutex dump_lock_;
  std::ofstream dumping_to_;
};

}  // namespace flare::binlog

#endif  // FLARE_RPC_BINLOG_TEXT_ONLY_DUMPER_H_
