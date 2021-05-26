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

namespace flare::compression {

bool CopyDataToCompressionOutputStream(CompressionOutputStream* out,
                                       const void* data, std::size_t size) {
  if (size == 0) {
    return true;
  }
  std::size_t current_pos = 0;
  std::size_t left_to_copy = size;
  while (true) {
    void* next_data;
    std::size_t next_size;
    if (FLARE_UNLIKELY(!out->Next(&next_data, &next_size))) {
      return false;
    }

    if (left_to_copy <= next_size) {
      // Get more space than we need to copy, finish copying.
      memcpy(next_data, reinterpret_cast<const char*>(data) + current_pos,
             left_to_copy);
      out->BackUp(next_size - left_to_copy);
      return true;
    } else {
      // Next data is not big enough, we copy as much as possible,
      // and continue next turn.
      memcpy(next_data, reinterpret_cast<const char*>(data) + current_pos,
             next_size);
      current_pos += next_size;
      left_to_copy -= next_size;
    }
  }
  FLARE_UNREACHABLE();
}

}  // namespace flare::compression
