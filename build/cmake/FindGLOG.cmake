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

find_path(GLOG_INCLUDE_PATH glog/logging.h)

find_library(GLOG_LIBRARY NAMES glog)
if(GLOG_INCLUDE_PATH AND GLOG_LIBRARY)
  set(GLOG_FOUND TRUE)
endif(GLOG_INCLUDE_PATH AND GLOG_LIBRARY)
if(GLOG_FOUND)
  if(NOT GLOG_FIND_QUIETLY)
    message(STATUS "Found GLOG: ${GLOG_LIBRARY}")
  endif(NOT GLOG_FIND_QUIETLY)
else(GLOG_FOUND)
  if(GLOG_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find GLOG library.")
  endif(GLOG_FIND_REQUIRED)
endif(GLOG_FOUND)
