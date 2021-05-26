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

#include "flare/testing/detail/dirty_hook.h"

#include <sys/mman.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "flare/base/logging.h"

namespace flare::testing::detail {

namespace {

#if defined(__x86_64__)

// Using `rax` as scratch register.
//
// `rax` is caller-saved. It should be safe for us to overwrite it.
//
// Note that we don't have to use so many bytes if the target can be reached
// within 4GB from the original function. In that case we can use near jump
// (`0xe9`) instead, which only costs 5 bytes.
constexpr char kOpcodes[] = {
    // mov rax, 0x1234567890abcdef
    '\x48', '\xB8', '\xEF', '\xCD', '\xAB', '\x90', '\x78', '\x56', '\x34',
    '\x12',
    // jmp rax
    '\xFF', '\xE0'};
constexpr auto kJumpTargetOffset = 2;
constexpr auto kOpcodeSize = sizeof(kOpcodes);

#elif defined(__aarch64__)

// Using `x9` as scratch register.
//
// `x9` is caller-saved and should be safe for us to use.
constexpr char kOpcodes[] = {
    // ldr x9, target
    '\x49', '\x00', '\x00', '\x58',
    // br x9
    '\x20', '\x01', '\x1f', '\xd6',
    // target:
    '\xef', '\xcd', '\xab', '\x90', '\x78', '\x56', '\x34', '\x12'};
constexpr auto kOpcodeSize = sizeof(kOpcodes);
constexpr auto kJumpTargetOffset = kOpcodeSize - 8;

#elif defined(__powerpc64__)

// Using `r0` and `r12` as scratch registers.
//
// This is horrible, to say the least. Not sure if we can load an imm64 on
// ppc64le in a more compact way.
constexpr char kOpcodes[] = {
    // The two `nop` below covers (possible?) TOC pointer setup instructions.
    //
    // GCC jumps to `fptr + 8` in case TOC need not to be re-set up. If we don't
    // take this into consideration and always overwrite starting from the first
    // byte, jumping to `fptr + 8` leads to disaster.

    '\x00', '\x00', '\x00', '\x60',  // nop
    '\x00', '\x00', '\x00', '\x60',  // nop

    // start:
    '\xa6', '\x02', '\x08', '\x7c',  // mflr r0  ; save LR
    '\x11', '\x00', '\x00', '\x48',  // bl load_target
    '\xa6', '\x03', '\x08', '\x7c',  // mtlr r0  ; restore LR.

    // jump_away:
    '\xa6', '\x03', '\x89', '\x7d',  // mtctr r12
    '\x20', '\x04', '\x80', '\x4e',  // bctr

    // Load imm64 is done in a way similar to how PIC is emulated on
    // not-supported ISAs.
    //
    // load_target:
    '\xa6', '\x02', '\x88', '\x7d',  // mflr r12
    '\x18', '\x00', '\x8c', '\xe9',  // ld r12, 24(r12)  ; 24 = offset of imm64
    '\x20', '\x00', '\x80', '\x4e',  // blr

    // target:
    '\xef', '\xcd', '\xab', '\x90', '\x78', '\x56', '\x34', '\x12'};
constexpr auto kOpcodeSize = sizeof(kOpcodes);
constexpr auto kJumpTargetOffset = kOpcodeSize - 8;

#endif

struct Handle {
  void* fptr;
  std::string original;
};

// For the moment all ISAs share the same `GenJump`. However, some ISAs allows
// more compact code to be used in certain condition. If we want to optimize
// such ISAs, we can implement different `GenJump` for those ISAs.
std::string GenJump(void* from, void* to) {
  static_assert(sizeof(void*) == 8);  // Only 64-bit is supported.

  std::string s(kOpcodes, kOpcodeSize);
  memcpy(s.data() + kJumpTargetOffset, &to, 8);
  return s;
}

void SetPageProtection(void* ptr, std::size_t size, int to) {
  static const auto kPageSize = sysconf(_SC_PAGE_SIZE);

  auto iptr = reinterpret_cast<std::uintptr_t>(ptr);
  auto lowest = iptr / kPageSize * kPageSize;
  auto highest = ((iptr + size + kPageSize - 1) / kPageSize) * kPageSize;
  FLARE_PCHECK(
      mprotect(reinterpret_cast<void*>(lowest), highest - lowest, to) == 0);
}

void FlushInstructionCache(void* ptr, std::size_t size) {
  // This should only compile to actual instructions on ISAs whose I-cache and
  // D-cache are not conherent.
  __builtin___clear_cache(reinterpret_cast<char*>(ptr),
                          reinterpret_cast<char*>(ptr) + size);
}

}  // namespace

void* InstallHook(void* fptr, void* to) {
  static_assert(sizeof(void*) == 8);  // Only 64-bit is supported.

  auto opcodes = GenJump(fptr, to);
  auto handle = std::make_unique<Handle>();
  handle->fptr = fptr;
  handle->original.assign(reinterpret_cast<char*>(fptr), opcodes.size());

  // Well I'm not gonna handle N^X stuff here..
  SetPageProtection(fptr, opcodes.size(), PROT_READ | PROT_WRITE | PROT_EXEC);
  memcpy(fptr, opcodes.data(), opcodes.size());
  FlushInstructionCache(fptr, opcodes.size());

  return handle.release();
}

void UninstallHook(void* handle) {
  std::unique_ptr<Handle> ptr{static_cast<Handle*>(handle)};

  memcpy(ptr->fptr, ptr->original.data(), ptr->original.size());
  FlushInstructionCache(ptr->fptr, kOpcodeSize);
}

}  // namespace flare::testing::detail
