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

find_path(ZSTD_INCLUDE_PATH zstd/zstd.h)

find_library(ZSTD_LIBRARY NAMES zstd)
if(ZSTD_INCLUDE_PATH AND ZSTD_LIBRARY)
  set(ZSTD_FOUND TRUE)
endif(ZSTD_INCLUDE_PATH AND ZSTD_LIBRARY)
if(ZSTD_FOUND)
  if(NOT ZSTD_FIND_QUIETLY)
    message(STATUS "Found ZSTD: ${ZSTD_LIBRARY}")
  endif(NOT ZSTD_FIND_QUIETLY)
else(ZSTD_FOUND)
  if(ZSTD_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find ZSTD library.")
  endif(ZSTD_FIND_REQUIRED)
endif(ZSTD_FOUND)
