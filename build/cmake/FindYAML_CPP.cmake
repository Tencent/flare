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

find_path(YAML_CPP_INCLUDE_PATH yaml-cpp/yaml.h)

find_library(YAML_CPP_LIBRARY NAMES yaml-cpp)
if(YAML_CPP_INCLUDE_PATH AND YAML_CPP_LIBRARY)
  set(YAML_CPP_FOUND TRUE)
endif(YAML_CPP_INCLUDE_PATH AND YAML_CPP_LIBRARY)
if(YAML_CPP_FOUND)
  if(NOT YAML_CPP_FIND_QUIETLY)
    message(STATUS "Found YAML_CPP: ${YAML_CPP_LIBRARY}")
  endif(NOT YAML_CPP_FIND_QUIETLY)
else(YAML_CPP_FOUND)
  if(YAML_CPP_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find YAML_CPP library.")
  endif(YAML_CPP_FIND_REQUIRED)
endif(YAML_CPP_FOUND)
