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

find_path(JEMALLOC_INCLUDE_PATH jemalloc/jemalloc.h)

find_library(JEMALLOC_LIBRARY NAMES jemalloc)
if(JEMALLOC_INCLUDE_PATH AND JEMALLOC_LIBRARY)
  set(JEMALLOC_FOUND TRUE)
endif(JEMALLOC_INCLUDE_PATH AND JEMALLOC_LIBRARY)
if(JEMALLOC_FOUND)
  if(NOT JEMALLOC_FIND_QUIETLY)
    message(STATUS "Found JEMALLOC: ${JEMALLOC_LIBRARY}")
  endif(NOT JEMALLOC_FIND_QUIETLY)
else(JEMALLOC_FOUND)
  if(JEMALLOC_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find JEMALLOC library.")
  endif(JEMALLOC_FIND_REQUIRED)
endif(JEMALLOC_FOUND)
