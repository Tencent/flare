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

#include "flare/base/maybe_owning.h"

#include "gtest/gtest.h"

using namespace std::literals;

namespace flare {

int dtor_called = 0;

struct C {
  ~C() { ++dtor_called; }
};

struct Base {
  virtual ~Base() = default;
};

struct Derived : Base {};

void AcceptMaybeOwningArgument(MaybeOwningArgument<int> ptr) {
  // NOTHING.
}

void AcceptMaybeOwningArgumentBase(MaybeOwningArgument<Base> ptr) {}

TEST(MaybeOwning, Owning) {
  dtor_called = 0;
  C* ptr = new C();
  {
    MaybeOwning<C> ppp(ptr, true);
    ASSERT_EQ(0, dtor_called);
  }
  ASSERT_EQ(1, dtor_called);
}

TEST(MaybeOwning, Owning2) {
  dtor_called = 0;
  C* ptr = new C();
  {
    MaybeOwning<C> ppp(owning, ptr);
    ASSERT_EQ(0, dtor_called);
  }
  ASSERT_EQ(1, dtor_called);
}

TEST(MaybeOwning, NonOwning) {
  dtor_called = 0;
  C* ptr = new C();
  {
    MaybeOwning<C> ppp(ptr, false);
    ASSERT_EQ(0, dtor_called);
  }
  ASSERT_EQ(0, dtor_called);
  delete ptr;
  ASSERT_EQ(1, dtor_called);
}

TEST(MaybeOwning, NonOwning2) {
  dtor_called = 0;
  C* ptr = new C();
  {
    MaybeOwning<C> ppp(non_owning, ptr);
    ASSERT_EQ(0, dtor_called);
  }
  ASSERT_EQ(0, dtor_called);
  delete ptr;
  ASSERT_EQ(1, dtor_called);
}

TEST(MaybeOwning, FromUniquePtr) {
  dtor_called = 0;
  auto ptr = std::make_unique<C>();
  {
    MaybeOwning<C> ppp(std::move(ptr));
    ASSERT_EQ(0, dtor_called);
  }
  ASSERT_EQ(1, dtor_called);
}

// This UT shouldn't crash.
TEST(MaybeOwning, FromEmptyUniquePtr) {
  dtor_called = 0;
  std::unique_ptr<C> p;
  {
    MaybeOwning<C> ppp(std::move(p));
    ASSERT_EQ(0, dtor_called);
  }
  ASSERT_EQ(0, dtor_called);
}

TEST(MaybeOwning, Move) {
  dtor_called = 0;
  {
    MaybeOwning<C> ppp(new C(), true);
    ASSERT_EQ(0, dtor_called);
    MaybeOwning<C> ppp2(std::move(ppp));
    ASSERT_FALSE(!!ppp);
    ASSERT_TRUE(!!ppp2);
    ASSERT_EQ(0, dtor_called);
    MaybeOwning<C> ppp3;
    ASSERT_FALSE(!!ppp3);
    ppp3 = std::move(ppp);
    ASSERT_FALSE(!!ppp3);
    ppp3 = std::move(ppp2);
    ASSERT_FALSE(!!ppp2);
    ASSERT_TRUE(!!ppp3);
    ASSERT_EQ(0, dtor_called);
  }
  ASSERT_EQ(1, dtor_called);
}

TEST(MaybeOwning, Reset) {
  dtor_called = 0;
  MaybeOwning<C> ppp(std::make_unique<C>());
  ASSERT_EQ(0, dtor_called);
  ppp.Reset();
  ASSERT_EQ(1, dtor_called);
}

TEST(MaybeOwning, TransferringOwnership) {
  dtor_called = 0;
  MaybeOwning<C> ppp(std::make_unique<C>());
  ASSERT_EQ(0, dtor_called);
  ppp = std::make_unique<C>();
  ASSERT_EQ(1, dtor_called);
}

// Shouldn't leak.
TEST(MaybeOwning, MoveIntoNonNull) {
  dtor_called = 0;
  {
    MaybeOwning<C> ppp(std::make_unique<C>());
    MaybeOwning<C> ppp2(std::make_unique<C>());
    ASSERT_EQ(0, dtor_called);
    ppp = std::move(ppp2);
    ASSERT_EQ(1, dtor_called);
  }
}

TEST(MaybeOwning, SelfMove) {
  dtor_called = 0;
  {
    MaybeOwning<C> ppp(std::make_unique<C>());
    ASSERT_EQ(0, dtor_called);
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-move"
#endif
    ppp = std::move(ppp);
#ifdef __clang__
#pragma clang diagnostic pop
#endif
    ASSERT_EQ(0, dtor_called);
  }
  ASSERT_EQ(1, dtor_called);
}

TEST(MaybeOwning, Conversion) {
  MaybeOwning<C> ppp(new C(), true);
  MaybeOwning<const C> ppp2(std::move(ppp));
  ASSERT_TRUE(!!ppp2);
  ASSERT_FALSE(!!ppp);
}

TEST(MaybeOwning, Conversion2) {
  MaybeOwning<C> ppp(new C(), true);
  MaybeOwning<const C> ppp2;
  ppp2 = std::move(ppp);
  ASSERT_TRUE(!!ppp2);
  ASSERT_FALSE(!!ppp);
}

TEST(MaybeOwning, ConversionUniquePtr) {
  auto ppp = std::make_unique<C>();
  MaybeOwning<const C> ppp2(std::move(ppp));
  ASSERT_TRUE(!!ppp2);
  ASSERT_FALSE(!!ppp);
}

TEST(MaybeOwning, ConversionUniquePtr2) {
  auto ppp = std::make_unique<C>();
  MaybeOwning<const C> ppp2;
  ppp2 = std::move(ppp);
  ASSERT_TRUE(!!ppp2);
  ASSERT_FALSE(!!ppp);
}

TEST(MaybeOwning, DeductionGuides) {
  MaybeOwning ppp(new C(), true);
  ASSERT_TRUE(!!ppp);
}

TEST(MaybeOwningArgument, All) {
  int x = 0;
  AcceptMaybeOwningArgument(&x);
  AcceptMaybeOwningArgument(std::make_unique<int>());
  AcceptMaybeOwningArgument(nullptr);

  Derived derived;
  AcceptMaybeOwningArgumentBase(&derived);

  static_assert(std::is_convertible_v<Derived*, MaybeOwningArgument<Base>>);
}

}  // namespace flare
