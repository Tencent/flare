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

#include "flare/base/casting.h"

#include "thirdparty/benchmark/benchmark.h"

// Run on (76 X 2494.14 MHz CPU s)
// 2020-02-11 15:50:54
// Benchmark                                    Time           CPU Iterations
// --------------------------------------------------------------------------
// Benchmark_BuiltinDynamicCast                22 ns         22 ns   31094959
// Benchmark_DynCast                            2 ns          2 ns  419860704
// Benchmark_ExactMatchCastableDynCast          2 ns          2 ns  349577035

namespace flare {

struct Base {
  virtual ~Base() = default;
  enum { kA, kB, kC [[maybe_unused]] } type;
};

struct A : Base {
  A() { type = kA; }

  static bool classof(const Base& val) {
    return val.type == kA || val.type == kB;
  }
};

struct B : A {
  B() { type = kB; }

  static bool classof(const Base& val) { return val.type == kB; }
};

auto pb = std::make_unique<B>();
Base* ptr = pb.get();
volatile A* converted_ptr;

struct C1 : ExactMatchCastable {};

struct C2 : C1 {
  C2() { SetRuntimeType(this, kRuntimeType<C2>); }
};

struct C3 : C1 {
  C3() { SetRuntimeType(this, kRuntimeType<C3>); }
};

auto pc2 = std::make_unique<C2>();
C1* pc1 = pc2.get();
volatile C3* pc3;

void Benchmark_BuiltinDynamicCast(benchmark::State& state) {
  while (state.KeepRunning()) {
    // ... <+34>: lea    0x10783f(%rip),%rax        # <_ZN5flare3ptrE>
    // ... <+41>: mov    (%rax),%rdi
    // ... <+44>: xor    %eax,%eax
    // ... <+46>: test   %rdi,%rdi
    // ... <+49>: je     0x40a1e8 <flare::Benchmark_BuiltinDynamicCast(benchmark::State&)+72>
    // ... <+51>: lea    0x1003d6(%rip),%rdx        # <_ZTIN5flare1AE>
    // ... <+58>: lea    0x1003bf(%rip),%rsi        # <_ZTIN5flare4BaseE>
    // ... <+65>: xor    %ecx,%ecx
    // ... <+67>: callq  0x431130 <__dynamic_cast>
    // ... <+72>: lea    0x107811(%rip),%rdx        # <_ZN5flare13converted_ptrE>
    // ... <+79>: mov    %rax,(%rdx)
    converted_ptr = dynamic_cast<A*>(ptr);
  }
}

BENCHMARK(Benchmark_BuiltinDynamicCast);

void Benchmark_DynCast(benchmark::State& state) {
  while (state.KeepRunning()) {
    // ... <+34>: lea    0x1077bf(%rip),%rax        # <_ZN5flare3ptrE>
    // ... <+41>: lea    0x1077b0(%rip),%rdx        # <_ZN5flare13converted_ptrE>
    // ... <+48>: mov    (%rax),%rax
    // ... <+51>: cmpl   $0x2,0x8(%rax)
    // ... <+55>: cmovae %r12,%rax
    // ... <+59>: mov    %rax,(%rdx)
    converted_ptr = dyn_cast<A>(ptr);
  }
}

BENCHMARK(Benchmark_DynCast);

void Benchmark_ExactMatchCastableDynCast(benchmark::State& state) {
  while (state.KeepRunning()) {
    // ... <+34>: lea    0x107747(%rip),%rax        # <_ZN5flare3pc1E>
    // ... <+41>: lea    0x1077d8(%rip),%rdx        # <_ZN5flare8Castable12kRuntimeTypeINS_2C3EEE>
    // ... <+48>: mov    (%rax),%rax
    // ... <+51>: mov    (%rax),%ecx
    // ... <+53>: cmp    %ecx,(%rdx)
    // ... <+55>: cmovne %r12,%rax
    // ... <+59>: lea    0x107726(%rip),%rdx        # <_ZN5flare3pc3E>
    // ... <+66>: mov    %rax,(%rdx)
    pc3 = dyn_cast<C3>(pc1);
  }
}

BENCHMARK(Benchmark_ExactMatchCastableDynCast);

}  // namespace flare
