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

find_path(JSONCPP_INCLUDE_PATH json/json.h)

find_library(JSONCPP_LIBRARY NAMES jsoncpp)
if(JSONCPP_INCLUDE_PATH AND JSONCPP_LIBRARY)
  set(JSONCPP_FOUND TRUE)
endif(JSONCPP_INCLUDE_PATH AND JSONCPP_LIBRARY)
if(JSONCPP_FOUND)
  if(NOT JSONCPP_FIND_QUIETLY)
    message(STATUS "Found JSONCPP: ${JSONCPP_LIBRARY}")
  endif(NOT JSONCPP_FIND_QUIETLY)
else(JSONCPP_FOUND)
  if(JSONCPP_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find JSONCPP library.")
  endif(JSONCPP_FIND_REQUIRED)
endif(JSONCPP_FOUND)
