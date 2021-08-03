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

find_path(RAPIDXML_INCLUDE_PATH rapidxml/rapidxml.h)

find_library(RAPIDXML_LIBRARY NAMES rapidxml)
if(RAPIDXML_INCLUDE_PATH AND RAPIDXML_LIBRARY)
  set(RAPIDXML_FOUND TRUE)
endif(RAPIDXML_INCLUDE_PATH AND RAPIDXML_LIBRARY)
if(RAPIDXML_FOUND)
  if(NOT RAPIDXML_FIND_QUIETLY)
    message(STATUS "Found RAPIDXML: ${RAPIDXML_LIBRARY}")
  endif(NOT RAPIDXML_FIND_QUIETLY)
else(RAPIDXML_FOUND)
  if(RAPIDXML_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find RAPIDXML library.")
  endif(RAPIDXML_FIND_REQUIRED)
endif(RAPIDXML_FOUND)
