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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_PLUGIN_CODE_WRITER_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_PLUGIN_CODE_WRITER_H_

#include <string>

namespace flare::protobuf::plugin {

// This interface defines interface for accepting generated code and write it to
// destinated files. If desired, class implementing this interface may create
// its own files.
class CodeWriter {
 public:
  virtual ~CodeWriter() = default;

  enum class File { Header, Source };

  // Returns a buffer for inserting code, the code will be inserted into header
  // file if `header` is set, or source file otherwise.
  //
  // We only uses insertion points defined as `kInsertionPointXxx`.
  virtual std::string* NewInsertionToHeader(
      const std::string& insertion_point) = 0;
  virtual std::string* NewInsertionToSource(
      const std::string& insertion_point) = 0;
};

}  // namespace flare::protobuf::plugin

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_PLUGIN_CODE_WRITER_H_
