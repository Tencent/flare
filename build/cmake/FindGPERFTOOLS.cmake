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

find_path(GPERFTOOLS_INCLUDE_PATH gperftools/tcmalloc.h)

find_library(GPERFTOOLS_LIBRARY NAMES tcmalloc_and_profiler)
if(GPERFTOOLS_INCLUDE_PATH AND GPERFTOOLS_LIBRARY)
  set(GPERFTOOLS_FOUND TRUE)
endif(GPERFTOOLS_INCLUDE_PATH AND GPERFTOOLS_LIBRARY)
if(GPERFTOOLS_FOUND)
  if(NOT GPERFTOOLS_FIND_QUIETLY)
    message(STATUS "Found GPERFTOOLS: ${GPERFTOOLS_LIBRARY}")
  endif(NOT GPERFTOOLS_FIND_QUIETLY)
else(GPERFTOOLS_FOUND)
  if(GPERFTOOLS_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find GPERFTOOLS library.")
  endif(GPERFTOOLS_FIND_REQUIRED)
endif(GPERFTOOLS_FOUND)
