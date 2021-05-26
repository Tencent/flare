# Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
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

cc_library(
  name = 'atomic_ptr',
  hdrs = 'atomic_ptr.h',
  deps = [
    '//flare/base:align',
    '//flare/base:ref_ptr',
    '//flare/base/internal:copyable_atomic',
  ],
  visibility = [
    '//flare/base/thread:thread_local',
    '//flare/base/thread/thread_local:ref_counted',
  ],
)
