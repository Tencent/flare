// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of the
// License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#ifndef FLARE_BASE_MONITORING_FWD_H_
#define FLARE_BASE_MONITORING_FWD_H_

namespace flare::monitoring {

// Some monitor provider requires the operation to be performed on values to be
// reported along with the value. Albeit providing more opt. potential, IMO it's
// not a very well-defined interface (esp. when considering that their SDK is
// rather slow). So as to support such providers, we require our user to provide
// this information (i.e, how the values are eventually read.).
enum class Reading {
  // This enumerator is for internal use only. You shouldn't be using it.
  //
  // In case you believe the provider you're using should be able to infer
  // operation to be performed implicitly, call the overloaded version of
  // `Report` which does not take an `operation` argument.
  Inferred,

  Newest,  // Last write wins.
  Sum,     // Sum of all reported values is monitored.
  Mean,    // Average of all reported values is monitored.
  Max,     // Maximum value is monitored.
  Min,     // Minimum value is monitored.

  // Middle? ZhiYan supports this..
};

}  // namespace flare::monitoring

#endif  // FLARE_BASE_MONITORING_FWD_H_
