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

#include "flare/rpc/tracing/tracing_ops_provider.h"

#include <memory>
#include <string>

FLARE_DEFINE_CLASS_DEPENDENCY_REGISTRY(
    flare_tracing_tracer_ops_provider_factory_registry,
    flare::tracing::TracingOpsProvider,
    flare::tracing::TracingOpsProviderOptions);

namespace flare::tracing {

std::unique_ptr<TracingOpsProvider> MakeTracingOpsProvider(
    std::string_view provider, const TracingOpsProviderOptions& options) {
  auto factory =
      flare_tracing_tracer_ops_provider_factory_registry.TryGetFactory(
          provider);
  FLARE_CHECK(factory,
              "Distributed tracing provider [{}] is not registered. Did you "
              "forget to link it in?",
              provider);
  return factory(options);
}

}  // namespace flare::tracing
