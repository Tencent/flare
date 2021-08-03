# Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
#
# Licensed under the BSD 3-Clause License (the "License"); you may not use this
# file except in compliance with the License. You may obtain a copy of the
# License at
#
# https://opensource.org/licenses/BSD-3-Clause
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

find_path(LZ4_INCLUDE_PATH lz4.h)

find_library(LZ4_LIBRARY NAMES lz4)
if(LZ4_INCLUDE_PATH AND LZ4_LIBRARY)
  set(LZ4_FOUND TRUE)
endif(LZ4_INCLUDE_PATH AND LZ4_LIBRARY)
if(LZ4_FOUND)
  if(NOT LZ4_FIND_QUIETLY)
    message(STATUS "Found LZ4: ${LZ4_LIBRARY}")
  endif(NOT LZ4_FIND_QUIETLY)
else(LZ4_FOUND)
  if(LZ4_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find LZ4 library.")
  endif(LZ4_FIND_REQUIRED)
endif(LZ4_FOUND)
