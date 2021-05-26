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

#ifndef FLARE_TESTING_HOOKING_MOCK_H_
#define FLARE_TESTING_HOOKING_MOCK_H_

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>

#include "thirdparty/googletest/gmock/gmock.h"

#include "flare/base/internal/macro.h"
#include "flare/base/logging.h"
#include "flare/base/never_destroyed.h"
#include "flare/testing/detail/dirty_hook.h"

// Inspired by `https://github.com/gzc9047/CppFreeMock`.

// This macro helps you to mock non-virtual method. For obvious technical
// reasons, it does not _always_ work. However, for most cases, it should
// satisfy your needs.
//
// Internally this macro does some "inline hook" stuff to catch calls to
// `Method`. The hook is restored once you leave the scope where this macro is
// used.
//
// Example:
//
// TEST(..., ...) {  // Introduces a new scope.
//   FLARE_EXPECT_HOOKED_CALL(SomeNonVirtualOrGlobalMethod, ...).WillOnce(...);
// }  // The hook is restored.
//
// @sa: `hooking_mock_test.cc` for more examples.
#define FLARE_EXPECT_HOOKED_CALL(Method, ...)                                \
  FLARE_HOOKING_MOCK_DETAIL_INSTALL_HOOK_IN_CURRENT_SCOPE(Method);           \
  auto FLARE_INTERNAL_PP_CAT(flare_reserved_mock_object_, __LINE__) =        \
      ::flare::testing::detail::CreateOrReferenceMocker(#Method, Method);    \
  EXPECT_CALL(*FLARE_INTERNAL_PP_CAT(flare_reserved_mock_object_, __LINE__), \
              OnInvoke(__VA_ARGS__))

// In certain cases you might want hook be enabled during the lifetime of UT.
// This macro helps you accomplish that.
//
// Example:
//
// // In namespace scope.
// FLARE_INSTALL_PERSISTENT_HOOK(&SomeNonVirtualOrGlobalMethod);
#define FLARE_INSTALL_PERSISTENT_HOOK(Method)                             \
  FLARE_HOOKING_MOCK_DETAIL_INSTALL_HOOK_IN_CURRENT_SCOPE(Method);        \
  auto FLARE_INTERNAL_PP_CAT(flare_reserved_mock_object_, __LINE__) =     \
      ::flare::testing::detail::CreateOrReferenceMocker<                  \
          std::remove_pointer_t<decltype(Method)>>(#Method, __FUNCTION__, \
                                                   __LINE__);

////////////////////////////////////////
// Implementation goes below.         //
////////////////////////////////////////

namespace flare::testing::detail {

// Mocker object for plain old functions.
template <class R, class... Args>
class Mocker {
 public:
  explicit Mocker(const std::string& name) : name_(name) {}

  // The name `OnInvoke` is used by `FLARE_EXPECT_HOOKED_CALL`.
  R OnInvoke(Args... p) {
    impl_.SetOwnerAndName(this, name_.c_str());
    return impl_.Invoke(std::forward<Args>(p)...);
  }

  ::testing::MockSpec<R(Args...)> gmock_OnInvoke(
      const ::testing::Matcher<Args>&... p) {
    impl_.RegisterOwner(this);
    return impl_.With(p...);
  }

  // Non-copyable, non-movable.
  Mocker(const Mocker&) = delete;
  Mocker& operator=(const Mocker&) = delete;

 private:
  std::string name_;
  ::testing::FunctionMocker<R(Args...)> impl_;
};

// Registry for mockers ever created.
class MockerRegistry {
 public:
  static MockerRegistry* Instance();

  //   `fptr` helps us to know which method is notified in `NotifyMocker`.
  template <class R, class... Args>
  std::shared_ptr<Mocker<R, Args...>> CreateOrReferenceMocker(
      const std::string& name, const void* fptr) {
    auto registry = GetTypedRegistry<R, Args...>();
    std::scoped_lock _(registry->lock);
    auto&& existing = registry->map[fptr];
    if (auto ptr = existing.lock()) {
      return ptr;
    }
    auto fancy_mocker = std::make_shared<Mocker<R, Args...>>(name);
    existing = fancy_mocker;
    return fancy_mocker;
  }

  template <class R, class... Args>
  R NotifyMocker(const void* fptr, Args&&... args) {
    std::shared_ptr<Mocker<R, Args...>> mocker;

    {
      auto registry = GetTypedRegistry<R, Args...>();
      std::scoped_lock _(registry->lock);
      mocker = registry->map[fptr].lock();

      // The function is still hooked yet the mocker has gone?
      FLARE_CHECK(!!mocker);
    }
    return mocker->OnInvoke(std::forward<Args>(args)...);
  }

 private:
  template <class R, class... Args>
  struct TypedRegistry {
    std::mutex lock;
    std::map<const void*, std::weak_ptr<Mocker<R, Args...>>> map;
  };

  template <class R, class... Args>
  TypedRegistry<R, Args...>* GetTypedRegistry() {
    static NeverDestroyed<TypedRegistry<R, Args...>> registry;
    return registry.Get();
  }
};

void PrintCrashyImplementationErrorOnce();

// Conversion from member function pointer to `void*` is likely unsafe.
// Therefore we do all function pointer (including "normal" one) to `void*`
// here.
//
// Not for sure, but if my memory serves me well, member function pointer can be
// larger than `void*` on some platforms?
template <class T>
void* UnsafeCastToGenericPointer(T ptr) {
  if (sizeof(void*) != sizeof(ptr)) {
    PrintCrashyImplementationErrorOnce();
  }
  void* to;
  memcpy(&to, &ptr, sizeof(void*));
  return to;
}

template <class R, class... Args>
std::shared_ptr<Mocker<R, Args...>> CreateOrReferenceMocker(
    const std::string& name, R (*fptr)(Args...)) {
  return MockerRegistry::Instance()->CreateOrReferenceMocker<R, Args...>(
      name, UnsafeCastToGenericPointer(fptr));
}

template <class R, class C, class... Args>
std::shared_ptr<Mocker<R, C*, Args...>> CreateOrReferenceMocker(
    const std::string& name, R (C::*fptr)(Args...)) {
  // To provide the user with the ability to match `this`, `C` is kept.
  return MockerRegistry::Instance()->CreateOrReferenceMocker<R, C*, Args...>(
      name, UnsafeCastToGenericPointer(fptr));
}

template <class R, class C, class... Args>
std::shared_ptr<Mocker<R, const C*, Args...>> CreateOrReferenceMocker(
    const std::string& name, R (C::*fptr)(Args...) const) {
  return MockerRegistry::Instance()
      ->CreateOrReferenceMocker<R, const C*, Args...>(
          name, UnsafeCastToGenericPointer(fptr));
}

template <auto kMethodPtr, class R, class... Args>
R Trampoline(Args... args) {
  return MockerRegistry::Instance()->NotifyMocker<R, Args...>(
      UnsafeCastToGenericPointer(kMethodPtr), std::forward<Args>(args)...);
}

// Install a hook on `from` and redirect it to `to`.
//
// Multiple installation is explicitly allowed by this method. The hook is not
// uninstalled unless all installation is cancelled (by destroying handle
// returned from this method).
std::shared_ptr<void> ApplyHookOn(void* from, void* to);

template <auto kMethodPtr, class R, class... Args>
std::shared_ptr<void> SetOrReferenceHook(R (*ptr)(Args...)) {
  return ApplyHookOn(
      UnsafeCastToGenericPointer(ptr),
      UnsafeCastToGenericPointer(&Trampoline<kMethodPtr, R, Args...>));
}

template <auto kMethodPtr, class R, class C, class... Args>
std::shared_ptr<void> SetOrReferenceHook(R (C::*ptr)(Args...)) {
  // Here `ptr` and `Trampoline` is not of the same type. U.B.?
  //
  // It should work in practice, if ABI specifies `this` to be passed in the
  // same way as if it's the first argument.
  //
  // For all platform we support, this should be the case.
  return ApplyHookOn(
      UnsafeCastToGenericPointer(ptr),
      UnsafeCastToGenericPointer(&Trampoline<kMethodPtr, R, C*, Args...>));
}

template <auto kMethodPtr, class R, class C, class... Args>
std::shared_ptr<void> SetOrReferenceHook(R (C::*ptr)(Args...) const) {
  return ApplyHookOn(UnsafeCastToGenericPointer(ptr),
                     UnsafeCastToGenericPointer(
                         &Trampoline<kMethodPtr, R, const C*, Args...>));
}

}  // namespace flare::testing::detail

#define FLARE_HOOKING_MOCK_DETAIL_INSTALL_HOOK_IN_CURRENT_SCOPE(Method) \
  auto FLARE_INTERNAL_PP_CAT(flare_reserved_hook_handle_, __LINE__) =   \
      ::flare::testing::detail::SetOrReferenceHook<Method>(Method);

#endif  // FLARE_TESTING_HOOKING_MOCK_H_
