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

#ifndef FLARE_BASE_DEPENDENCY_REGISTRY_H_
#define FLARE_BASE_DEPENDENCY_REGISTRY_H_

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include "flare/base/demangle.h"
#include "flare/base/function.h"
#include "flare/base/internal/hash_map.h"
#include "flare/base/internal/lazy_init.h"
#include "flare/base/internal/logging.h"
#include "flare/base/internal/macro.h"
#include "flare/base/maybe_owning.h"
#include "flare/base/never_destroyed.h"

//////////////////////
// Class registry.  //
//////////////////////

// See comments on corresponding macro for:
//
// 1) How to declare / define class registry,
// 2) Register class to the registry.
//
// To use (an already declared) class registry with name `world_destroyer`:
//
// ```cpp
// // Create a destroyer.
// std::unique_ptr<Destroyer> destroyer = world_destroyer.TryNew(
//     "fast-destroyer");
//
// // Get a factory for create one or more destroyer.
// Function<std::unique_ptr<Destroyer>> factory = world_destroyer.TryGetFactory(
//     "fast-destroyer");
// FLARE_CHECK(factory, "Destroyer is not found.");
//
// // May Peace Prevail On Earth (tm).
// ```

// Declare class registry. This is usually used in header.
//
// Usage: `FLARE_DECLARE_CLASS_DEPENDENCY_REGISTRY(world_destroyer, Destroyer);`
#define FLARE_DECLARE_CLASS_DEPENDENCY_REGISTRY(                         \
    Registry, Interface, ... /* Types of arguments to class factory. */) \
  extern ::flare::detail::dependency_registry::ClassRegistry<            \
      struct Registry, Interface, ##__VA_ARGS__>& Registry

// Define class registry. This should be used in source file.
//
// Usage: `FLARE_DEFINE_CLASS_DEPENDENCY_REGISTRY(world_destroyer, Destroyer);`
#define FLARE_DEFINE_CLASS_DEPENDENCY_REGISTRY(                          \
    Registry, Interface, ... /* Types of arguments to class factory. */) \
  ::flare::detail::dependency_registry::ClassRegistry<                   \
      struct Registry, Interface, ##__VA_ARGS__>& Registry =             \
      **::flare::internal::LazyInit<::flare::NeverDestroyed<             \
          ::flare::detail::dependency_registry::ClassRegistry<           \
              struct Registry, Interface, ##__VA_ARGS__>>>()

// `FLARE_INLINE_DEFINE_CLASS_DEPENDENCY_REGISTRY`?

// Register a class (by its type) to the corresponding class registry.
//
// `ImplementationClassName` must implements the interface that was used to
// define `Registry`.
//
// Usage:
//
// ```cpp
// FLARE_REGISTER_CLASS_DEPENDENCY(world_destroyer, "fast-destroyer",
//                                 FastDestroyer);
// ```
#define FLARE_REGISTER_CLASS_DEPENDENCY(Registry, ImplementationName,     \
                                        ImplementationClassName)          \
  FLARE_REGISTER_CLASS_DEPENDENCY_FACTORY(                                \
      Registry, ImplementationName, []<class... Args>(Args&&... args) {   \
        return std::make_unique<ImplementationClassName>(                 \
            std::forward<Args>(args)...);                                 \
      })

// Register a class by its factory.
//
// Usage:
//
// ```cpp
// FLARE_REGISTER_CLASS_DEPENDENCY_FACTORY(
//     world_destroyer, "fast_destroyer", [] {
//   return std::make_unique<FastDestroyer>();
// });
// ```
#define FLARE_REGISTER_CLASS_DEPENDENCY_FACTORY(Registry, Name, Factory)   \
  [[gnu::constructor]] static void FLARE_INTERNAL_PP_CAT(                  \
      flare_reserved_register_dependency_class_register_, __COUNTER__)() { \
    /* Make sure the registry is initialized. */                           \
    auto registry =                                                        \
        ::flare::internal::LazyInit<                                       \
            ::flare::NeverDestroyed<std::decay_t<decltype(Registry)>>>()   \
            ->Get();                                                       \
    ::flare::detail::dependency_registry::AddToRegistry(registry, Name,    \
                                                        Factory);          \
  }

///////////////////////
// Object registry.  //
///////////////////////

// See comments on corresponding macros about registry creation and object
// registration.
//
// Object registries are mostly used for singletons.
//
// To use an (already defined) object registry with name `world_destroyer`:
//
// ```cpp
// // Get destroyer with name "fast-destroyer".
// Destroyer* destroyer = world_destroyer.TryGet("fast-destroyer");
// FLARE_CHECK(destroyer, "Destroyer is not found.");
// ```

// Declare a registry for objects implementing `Interface`.
//
// Usage: `FLARE_DECLARE_OBJECT_DEPENDENCY_REGISTRY(world_destroyer, Destroyer)`
#define FLARE_DECLARE_OBJECT_DEPENDENCY_REGISTRY(Registry, Interface) \
  extern ::flare::detail::dependency_registry::ObjectRegistry<        \
      struct Registry, Interface>& Registry

// Define a registry for objects implementing `Interface`.
//
// Usage: `FLARE_DEFINE_OBJECT_DEPENDENCY_REGISTRY(world_destroyer, Destroyer)`
#define FLARE_DEFINE_OBJECT_DEPENDENCY_REGISTRY(Registry, Interface)          \
  ::flare::detail::dependency_registry::ObjectRegistry<struct Registry,       \
                                                       Interface>& Registry = \
      **::flare::internal::LazyInit<::flare::NeverDestroyed<                  \
          ::flare::detail::dependency_registry::ObjectRegistry<               \
              struct Registry, Interface>>>()

// Register an object to the given registry.
//
// If factory is used for registration, it's lazily evaluated (not evaluated
// until first use).
//
// Usage:
//
// ```cpp
// // Register an existing object with its pointer.
// GentleDestroyer gentle_destroyer;
// FLARE_REGISTER_OBJECT_DEPENDENCY(world_destroyer, "gentle-destroyer",
//                                  &gentle_destroyer);
//
// // Register an object with a factory.
// FLARE_REGISTER_OBJECT_DEPENDENCY(world_destroyer, "gentle-destroyer", [] {
//   return std::make_unique<GentleDestroyer>();
// });
// ```
#define FLARE_REGISTER_OBJECT_DEPENDENCY(Registry, ObjectName,                \
                                         PointerOrFactory)                    \
  [[gnu::constructor]] static void FLARE_INTERNAL_PP_CAT(                     \
      flare_reserved_register_dependency_object_register_, __COUNTER__)() {   \
    /* Make sure the registry is initialized. */                              \
    auto registry =                                                           \
        ::flare::internal::LazyInit<                                          \
            ::flare::NeverDestroyed<std::decay_t<decltype(Registry)>>>()      \
            ->Get();                                                          \
    ::flare::detail::dependency_registry::AddToRegistry(registry, ObjectName, \
                                                        PointerOrFactory);    \
  }

/////////////////////////////////
// Implementation goes below.  //
/////////////////////////////////

namespace flare::detail::dependency_registry {

template <class T, class... Args>
void AddToRegistry(T* registry, Args&&... args) {
  registry->Register(std::forward<Args>(args)...);
}

// Registry holding different classes implementing the same interface.
template <class Tag, class Interface, class... FactoryArgs>
class ClassRegistry {
  using Factory = Function<std::unique_ptr<Interface>(FactoryArgs...)>;

 public:
  // Returns factory for instantiating the class of the given name.
  //
  // An empty factory is returned if no class with the requested name is found.
  Factory TryGetFactory(const std::string_view& name) const noexcept {
    auto ptr = factories_.TryGet(name);
    if (ptr) {
      // We don't support deregistration, therefore holding a pointer to the
      // factory should be safe.
      return [ptr = ptr->get()]<class... Args>(Args&&... args) {
        return (*ptr)(std::forward<Args>(args)...);
      };
    }
    return nullptr;
  }

  // Same as `TryGetFactory` except that this method aborts if the requested
  // class is not found.
  Factory GetFactory(const std::string& name) const noexcept {
    auto result = TryGetFactory(name);
    FLARE_CHECK(result,
                "Class [{}] implementing interface [{}] is not found. You need "
                "to link against that class to use it.",
                name, GetTypeName<Interface>());
    return result;
  }

  // Instantiate an instance of the class with the given name.
  //
  // `nullptr` is returned if the name given is not recognized.
  std::unique_ptr<Interface> TryNew(const std::string_view& name,
                                    FactoryArgs... args) const {
    auto ptr = factories_.TryGet(name);
    if (ptr) {
      return (**ptr)(std::forward<FactoryArgs>(args)...);
    }
    return nullptr;
  }

  // Same as `TryNew` except that this method aborts if the requested class is
  // not found.
  std::unique_ptr<Interface> New(const std::string_view& name,
                                 FactoryArgs... args) const {
    auto result = TryNew(name, std::forward<FactoryArgs>(args)...);
    FLARE_CHECK(result,
                "Class dependency [{}] implementing interface [{}] is not "
                "found. You need to link against that class to use it.",
                name, GetTypeName<Interface>());
    return result;
  }

 private:
  // To avoid our user (mistakenly) calling `Register` by hand, `Register` is
  // marked as `private`. This helper function allows ourselves to call
  // `Register` when necessary.
  template <class T, class... Args>
  friend void AddToRegistry(T* registry, Args&&... args);

  // Only usable before `main` is called.
  void Register(const std::string& name, Factory factory) {
    FLARE_CHECK(!factories_.contains(name),
                "Double registration of class dependency [{}].", name);
    factories_[name] = std::make_unique<Factory>(std::move(factory));
  }

 private:
  internal::HashMap<std::string, std::unique_ptr<Factory>> factories_;
};

// Registry holding different objects implementing the same interface.
template <class Tag, class Interface>
class ObjectRegistry {
 public:
  // Get object with the specified name.
  Interface* TryGet(const std::string_view& name) const {
    auto ptr = objects_.TryGet(name);
    if (ptr) {
      auto&& e = **ptr;
      std::call_once(e.flag, [&] { e.object = e.initializer(); });
      return e.object.Get();
    }
    return nullptr;
  }

  // Same as `TryGet` except that this method aborts if the requested object is
  // not found.
  Interface* Get(const std::string_view& name) const {
    auto result = TryGet(name);
    FLARE_CHECK(result,
                "Object dependency [{}] implementing interface [{}] is not "
                "found. You need to link agains that object to use it.",
                name, GetTypeName<Interface>());
    return result;
  }

 private:
  template <class T, class... Args>
  friend void AddToRegistry(T* registry, Args&&... args);

  void Register(const std::string& name, Interface* object) {
    FLARE_CHECK(!objects_.contains(name));
    auto&& e = objects_[name];
    e = std::make_unique<LazilyInstantiatedObject>();
    std::call_once(e->flag, [&] {
      // Eagerly initialized, as there's hardly anything for us to delay.
      e->object = MaybeOwning(non_owning, object);
    });
  }

  void Register(const std::string& name,
                Function<MaybeOwning<Interface>()> initializer) {
    FLARE_CHECK(!objects_.contains(name),
                "Double registration of object dependency [{}].", name);
    objects_[name] = std::make_unique<LazilyInstantiatedObject>();
    objects_[name]->initializer = std::move(initializer);
  }

 private:
  struct LazilyInstantiatedObject {
    std::once_flag flag;
    MaybeOwning<Interface> object;
    Function<MaybeOwning<Interface>()> initializer;
  };

  internal::HashMap<std::string, std::unique_ptr<LazilyInstantiatedObject>>
      objects_;
};

}  // namespace flare::detail::dependency_registry

#endif  // FLARE_BASE_DEPENDENCY_REGISTRY_H_
