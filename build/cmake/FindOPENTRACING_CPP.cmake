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

find_path(OPENTRACING_CPP_INCLUDE_PATH opentracing/span.h)

find_library(OPENTRACING_CPP_LIBRARY NAMES opentracing)
if(OPENTRACING_CPP_INCLUDE_PATH AND OPENTRACING_CPP_LIBRARY)
  set(OPENTRACING_CPP_FOUND TRUE)
endif(OPENTRACING_CPP_INCLUDE_PATH AND OPENTRACING_CPP_LIBRARY)
if(OPENTRACING_CPP_FOUND)
  if(NOT OPENTRACING_CPP_FIND_QUIETLY)
    message(STATUS "Found OPENTRACING_CPP: ${OPENTRACING_CPP_LIBRARY}")
  endif(NOT OPENTRACING_CPP_FIND_QUIETLY)
else(OPENTRACING_CPP_FOUND)
  if(OPENTRACING_CPP_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find OPENTRACING_CPP library.")
  endif(OPENTRACING_CPP_FIND_REQUIRED)
endif(OPENTRACING_CPP_FOUND)
