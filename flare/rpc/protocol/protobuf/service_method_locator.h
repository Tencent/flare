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

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_SERVICE_METHOD_LOCATOR_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_SERVICE_METHOD_LOCATOR_H_

#include <atomic>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "thirdparty/protobuf/descriptor.h"
#include "thirdparty/protobuf/message.h"
#include "thirdparty/protobuf/service.h"

#include "flare/base/erased_ptr.h"
#include "flare/base/function.h"
#include "flare/base/internal/annotation.h"
#include "flare/base/internal/hash_map.h"
#include "flare/base/logging.h"
#include "flare/base/never_destroyed.h"
#include "flare/base/type_index.h"
#include "flare/init/on_init.h"

namespace flare::protobuf {

namespace protocol_ids {

// Concept `ProtocolId`:
//
// struct Xxx {
//   using MethodKey = std::tuple<...>;  // Information required for identifying
//                                       // a method by the protocol.
// };

// In order to use standard protocol, linking with `std_protocol`.
inline constexpr struct Standard {
  using MethodKey = std::string;  // method.full_name()
} standard;

// Link with `qzone_protocol` to use this one.
inline constexpr struct QZone {
  using MethodKey = std::tuple<std::int32_t, std::int32_t>;  // (version, cmd)
} qzone;

// Link with `svrkit_protocol` to use this one.
inline constexpr struct Svrkit {
  using MethodKey = std::tuple<std::int16_t, std::int16_t>;  // (magic, cmd)
} svrkit;

// Link with `trpc_protocol` to use this one.
inline constexpr struct Trpc {
  using MethodKey = std::string;  // "/{service}/{method}".
} trpc;

}  // namespace protocol_ids

// This class maps various IDs (or keys) used in a given protocol (FlareStd,
// QZone, Svrkit, ...) to its detailed information.
//
// Unless otherwise stated, methods of this class are thread-safe.
//
// Note that only server-side methods are registered here. For client side, use
// `ProactiveCallContext` to pass information instead.
class ServiceMethodLocator {
  template <class T>
  using MethodKey = typename T::MethodKey;

 public:
  // `ServiceDescriptor` can be inferred from `method->service()`.
  using LocatorProviderCallback =
      Function<void(const google::protobuf::MethodDescriptor* method)>;

  // All information required by `Service` and various `XxxProtocol` to
  // implement their functionality.
  template <class T>
  struct MethodDesc {
    std::string normalized_method_name;
    MethodKey<T> method_key;  // Need not to be unique.
    const google::protobuf::ServiceDescriptor* service_desc;
    const google::protobuf::MethodDescriptor* method_desc;
    const google::protobuf::Message* request_prototype;
    const google::protobuf::Message* response_prototype;
  };

  static ServiceMethodLocator* Instance() {
    static NeverDestroyedSingleton<ServiceMethodLocator> mp;
    return mp.Get();
  }

  // Register a method.
  //
  // Duplicates are NOT allowed among `key`s, as the `key` is used for finding
  // message prototype of requests. Were there any duplicates, it's ambiguous in
  // how to deserialize the requests.
  template <class T>
  void RegisterMethod(T protocol,
                      const google::protobuf::MethodDescriptor* method,
                      const MethodKey<T>& key) {
    std::scoped_lock lk(lock_);
    UnsafeTryInitializeControlBlock<T>();
    auto&& cb = UnsafeGetControlBlock<T>();
    // For the moment we don't handle duplicate registration well.
    FLARE_CHECK(!cb->key_desc_map.contains(key));
    FLARE_CHECK(!cb->name_key_map.contains(method->full_name()));
    cb->key_desc_map[key] = CreateMethodDesc<T>(method, key);
    cb->name_key_map[method->full_name()] = key;
    version_.fetch_add(1, std::memory_order_relaxed);
  }

  // Find method with `key`.
  template <class T>
  const MethodDesc<T>* TryGetMethodDesc(T protocol,
                                        const MethodKey<T>& key) const {
    auto&& cb = GetCachedControlBlock<T>();
    if (auto p = cb->key_desc_map.TryGet(key); FLARE_LIKELY(p)) {
      return p;
    }
    return nullptr;
  }

  // Deregister a method.
  template <class T>
  void DeregisterMethod(T protocol,
                        const google::protobuf::MethodDescriptor* method) {
    std::scoped_lock lk(lock_);
    auto&& cb = UnsafeGetControlBlock<T>();
    auto key = cb->name_key_map.at(method->full_name());
    FLARE_CHECK_EQ(cb->key_desc_map.erase(key), 1);
    FLARE_CHECK_EQ(cb->name_key_map.erase(method->full_name()), 1);
    version_.fetch_add(1, std::memory_order_relaxed);
  }

  // Called by `FLARE_RPC_PROTOCOL_PROTOBUF_REGISTER_METHOD_PROVIDER`.
  //
  // NOT thread-safe. It should be called in initialization phase anyway.
  void RegisterMethodProvider(LocatorProviderCallback init,
                              LocatorProviderCallback fini);

  // Called by `Service` for registering services.
  void AddService(const google::protobuf::ServiceDescriptor* service_desc);
  void DeleteService(const google::protobuf::ServiceDescriptor* service_desc);

  std::vector<const google::protobuf::ServiceDescriptor*> GetAllServices()
      const;

 private:
  friend class NeverDestroyedSingleton<ServiceMethodLocator>;

  template <class T>
  struct ControlBlock {
    internal::HashMap<MethodKey<T>, MethodDesc<T>> key_desc_map;
    internal::HashMap<std::string, MethodKey<T>> name_key_map;
  };

  ServiceMethodLocator();

  template <class T>
  void UnsafeTryInitializeControlBlock() {
    if (auto p = &control_blocks_[GetTypeIndex<T>()]; !*p) {
      *p = MakeErased<ControlBlock<T>>();
    }
  }

  template <class T>
  ControlBlock<T>* UnsafeGetControlBlock() {
    auto iter = control_blocks_.find(GetTypeIndex<T>());
    if (iter == control_blocks_.end()) {
      // Either there's a programming error or the user has enabled the
      // calling-protocol without having any of method that enabled
      // protocol-specific `option`. In later case we shouldn't crash the
      // service (otherwise it's a DoS.).
      return nullptr;
    }
    return static_cast<ControlBlock<T>*>(iter->second.Get());
  }

  template <class T>
  [[gnu::noinline, gnu::cold]] void FillControlBlockCache(
      std::uint64_t* version, std::unique_ptr<ControlBlock<T>>* cache) const {
    auto v = version_.load(std::memory_order_relaxed);
    FLARE_CHECK_GT(v, *version);
    std::shared_lock lk(lock_);
    if (auto ptr = const_cast<ServiceMethodLocator*>(this)
                       ->UnsafeGetControlBlock<T>()) {
      *cache = std::make_unique<ControlBlock<T>>(*ptr);
    }  // Left cache empty otherwise.
    *version = v;
  }

  template <class T>
  ControlBlock<T>* GetCachedControlBlock() const {
    FLARE_INTERNAL_TLS_MODEL thread_local std::uint64_t version = 0;
    // `std::unique_ptr<T>` here saves a call to `tls_init` as `std::unique_ptr`
    // can be initalized statically.
    FLARE_INTERNAL_TLS_MODEL thread_local std::unique_ptr<ControlBlock<T>>
        cache;

    if (auto v = version_.load(std::memory_order_relaxed);
        FLARE_UNLIKELY(v != version)) {
      FillControlBlockCache(&version, &cache);
    }
    return cache.get();
  }

  // Initialize `MethodDesc` from method's descriptor.
  template <class T>
  static MethodDesc<T> CreateMethodDesc(
      const google::protobuf::MethodDescriptor* method_desc,
      const MethodKey<T>& key) {
    MethodDesc<T> rc;

    rc.normalized_method_name = method_desc->full_name();
    rc.method_key = key;
    rc.service_desc = method_desc->service();
    rc.method_desc = method_desc;
    rc.request_prototype =
        google::protobuf::MessageFactory::generated_factory()->GetPrototype(
            method_desc->input_type());
    rc.response_prototype =
        google::protobuf::MessageFactory::generated_factory()->GetPrototype(
            method_desc->output_type());
    return rc;
  }

 private:
  // For detecting multiple addition of the same service (to simplify our code,
  // adding a service more than once is not treated as an error).
  mutable std::mutex services_lock_;
  std::unordered_map<const google::protobuf::ServiceDescriptor*, std::size_t>
      services_;
  std::vector<std::pair<LocatorProviderCallback, LocatorProviderCallback>>
      providers_;

  // Increased each time anything protected by `lock_` changes.
  //
  // Starts from 1, since 0 is used for initializing thread local cache's
  // version (which must be less than this one in order to trigger a cache fill
  // on first call).
  std::atomic<std::uint64_t> version_{1};

  // Protects all fields below.
  //
  // Indeed this can be slow (even if it's a shared lock, it IS slow even in
  // read-only cases). However, we optimize read-only methods (which is in
  // critical path) by employing a thread-local cache, which is only updated
  // on vesion (`version_` above) change.
  mutable std::shared_mutex lock_;

  // Type -> ControlBlock<T>
  std::map<TypeIndex, ErasedPtr> control_blocks_;
};

}  // namespace flare::protobuf

#define FLARE_RPC_PROTOCOL_PROTOBUF_REGISTER_METHOD_PROVIDER(init, fini) \
  FLARE_ON_INIT(0, [] {                                                  \
    ::flare::protobuf::ServiceMethodLocator::Instance()                  \
        ->RegisterMethodProvider(init, fini);                            \
  })

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_SERVICE_METHOD_LOCATOR_H_
