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

#include "flare/base/dependency_registry.h"

#include "googletest/gtest/gtest.h"

namespace flare {

struct Destroyer {
  virtual ~Destroyer() {}
};

struct GentleDestroyer : Destroyer {
  GentleDestroyer() { ++instances; }
  ~GentleDestroyer() { --instances; }
  inline static int instances = 0;
};

struct FastDestroyer : Destroyer {
  FastDestroyer() { ++instances; }
  ~FastDestroyer() { --instances; }
  inline static int instances = 0;
};

struct SpeedDestroyer : Destroyer {
  explicit SpeedDestroyer(int) {}
};

struct SpeedDestroyer2 : Destroyer {};

// Declaration is not required as we can see definition already.
FLARE_DECLARE_CLASS_DEPENDENCY_REGISTRY(world_destroyer, Destroyer);
FLARE_DEFINE_CLASS_DEPENDENCY_REGISTRY(world_destroyer, Destroyer);
FLARE_REGISTER_CLASS_DEPENDENCY(world_destroyer, "fast-destroyer",
                                FastDestroyer);
FLARE_REGISTER_CLASS_DEPENDENCY_FACTORY(
    world_destroyer, "gentle-destroyer",
    [] { return std::make_unique<GentleDestroyer>(); });

GentleDestroyer gentle_destroyer;
FLARE_DECLARE_OBJECT_DEPENDENCY_REGISTRY(singleton_destroyer, Destroyer);
FLARE_DEFINE_OBJECT_DEPENDENCY_REGISTRY(singleton_destroyer, Destroyer);
FLARE_REGISTER_OBJECT_DEPENDENCY(singleton_destroyer, "gentle-destroyer",
                                 &gentle_destroyer);
FLARE_REGISTER_OBJECT_DEPENDENCY(singleton_destroyer, "fast-destroyer", [] {
  return std::make_unique<FastDestroyer>();
});

FLARE_DECLARE_CLASS_DEPENDENCY_REGISTRY(with_arg_destroyer, Destroyer, int);
FLARE_DEFINE_CLASS_DEPENDENCY_REGISTRY(with_arg_destroyer, Destroyer, int);
FLARE_REGISTER_CLASS_DEPENDENCY(with_arg_destroyer, "speed-destroyer",
                                SpeedDestroyer);
FLARE_REGISTER_CLASS_DEPENDENCY_FACTORY(
    with_arg_destroyer, "speed-destroyer-2",
    [](int speed) { return std::make_unique<SpeedDestroyer2>(); });

TEST(DependencyRegistry, Class) {
  EXPECT_TRUE(world_destroyer.TryGetFactory("gentle-destroyer"));
  EXPECT_TRUE(world_destroyer.TryGetFactory("fast-destroyer"));
  EXPECT_FALSE(world_destroyer.TryGetFactory("404-destroyer"));
  EXPECT_TRUE(world_destroyer.TryNew("gentle-destroyer"));
  EXPECT_TRUE(world_destroyer.TryNew("fast-destroyer"));
  EXPECT_FALSE(world_destroyer.TryGetFactory("404-destroyer"));

  EXPECT_EQ(1, GentleDestroyer::instances);  // The global one.
  EXPECT_EQ(0, FastDestroyer::instances);

  {
    auto gentle = world_destroyer.TryNew("gentle-destroyer");
    EXPECT_EQ(2, GentleDestroyer::instances);
    EXPECT_EQ(0, FastDestroyer::instances);
    auto fast = world_destroyer.TryNew("fast-destroyer");
    EXPECT_EQ(2, GentleDestroyer::instances);
    EXPECT_EQ(1, FastDestroyer::instances);
  }
  EXPECT_EQ(1, GentleDestroyer::instances);
  EXPECT_EQ(0, FastDestroyer::instances);
}

TEST(DependencyRegistry, ClassWithArgs) {
  EXPECT_TRUE(with_arg_destroyer.TryGetFactory("speed-destroyer"));
  EXPECT_TRUE(with_arg_destroyer.TryGetFactory("speed-destroyer-2"));
  EXPECT_FALSE(with_arg_destroyer.TryGetFactory("speed-destroyer-3"));
  EXPECT_TRUE(with_arg_destroyer.TryNew("speed-destroyer", 456));
  EXPECT_TRUE(with_arg_destroyer.TryNew("speed-destroyer-2", 456));
  EXPECT_FALSE(with_arg_destroyer.TryNew("speed-destroyer-3", 456));
}

TEST(DependencyRegistry, Object) {
  EXPECT_EQ(1, GentleDestroyer::instances);
  EXPECT_EQ(0, FastDestroyer::instances);

  {
    EXPECT_TRUE(singleton_destroyer.TryGet("gentle-destroyer"));
    EXPECT_EQ(1, GentleDestroyer::instances);
    EXPECT_EQ(0, FastDestroyer::instances);

    EXPECT_TRUE(singleton_destroyer.TryGet("fast-destroyer"));
    EXPECT_EQ(1, GentleDestroyer::instances);
    EXPECT_EQ(1, FastDestroyer::instances);  // Instantiated now.

    EXPECT_FALSE(singleton_destroyer.TryGet("404-destroyer"));
    EXPECT_EQ(1, GentleDestroyer::instances);
    EXPECT_EQ(1, FastDestroyer::instances);
  }

  EXPECT_EQ(1, GentleDestroyer::instances);
  EXPECT_EQ(1, FastDestroyer::instances);  // It's still there.
}

}  // namespace flare
