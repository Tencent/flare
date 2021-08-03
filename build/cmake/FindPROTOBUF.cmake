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

find_path(PROTOBUF_INCLUDE_PATH google/protobuf/message.h)

find_library(PROTOBUF_LIBRARY NAMES protobuf)
if(PROTOBUF_INCLUDE_PATH AND PROTOBUF_LIBRARY)
  set(PROTOBUF_FOUND TRUE)
endif(PROTOBUF_INCLUDE_PATH AND PROTOBUF_LIBRARY)
if(PROTOBUF_FOUND)
  if(NOT PROTOBUF_FIND_QUIETLY)
    message(STATUS "Found PROTOBUF: ${PROTOBUF_LIBRARY}")
  endif(NOT PROTOBUF_FIND_QUIETLY)
else(PROTOBUF_FOUND)
  if(PROTOBUF_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find PROTOBUF library.")
  endif(PROTOBUF_FIND_REQUIRED)
endif(PROTOBUF_FOUND)
