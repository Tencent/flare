// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "flare/base/demangle.h"

#include "thirdparty/googletest/gtest/gtest.h"

namespace flare {

struct C {
  struct D {
    struct E {};
  };
};

TEST(Demangle, All) {
  ASSERT_EQ("flare::C::D::E", GetTypeName<C::D::E>());
  ASSERT_NE(GetTypeName<C::D::E>(), typeid(C::D::E).name());
  ASSERT_EQ("invalid function name !@#$",
            Demangle("invalid function name !@#$"));
}

}  // namespace flare
