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

find_path(SNAPPY_INCLUDE_PATH snappy/snappy.h)

find_library(SNAPPY_LIBRARY NAMES snappy)
if(SNAPPY_INCLUDE_PATH AND SNAPPY_LIBRARY)
  set(SNAPPY_FOUND TRUE)
endif(SNAPPY_INCLUDE_PATH AND SNAPPY_LIBRARY)
if(SNAPPY_FOUND)
  if(NOT SNAPPY_FIND_QUIETLY)
    message(STATUS "Found SNAPPY: ${SNAPPY_LIBRARY}")
  endif(NOT SNAPPY_FIND_QUIETLY)
else(SNAPPY_FOUND)
  if(SNAPPY_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find SNAPPY library.")
  endif(SNAPPY_FIND_REQUIRED)
endif(SNAPPY_FOUND)
