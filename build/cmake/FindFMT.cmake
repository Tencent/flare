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

find_path(FMT_INCLUDE_PATH fmt/format.h)

find_library(FMT_LIBRARY NAMES fmt)
if(FMT_INCLUDE_PATH AND FMT_LIBRARY)
  set(FMT_FOUND TRUE)
endif(FMT_INCLUDE_PATH AND FMT_LIBRARY)
if(FMT_FOUND)
  if(NOT FMT_FIND_QUIETLY)
    message(STATUS "Found FMT: ${FMT_LIBRARY}")
  endif(NOT FMT_FIND_QUIETLY)
else(FMT_FOUND)
  if(FMT_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find FMT library.")
  endif(FMT_FIND_REQUIRED)
endif(FMT_FOUND)
