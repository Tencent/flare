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

#ifndef FLARE_BASE_INTERNAL_CPU_H_
#define FLARE_BASE_INTERNAL_CPU_H_

#include <cinttypes>
#include <optional>
#include <string>
#include <vector>

// For the moment I'm not very satisfied about interfaces here, so I placed this
// header in `internal/`.

namespace flare::internal {

namespace numa {

struct Node {
  int id;
  std::vector<int> logical_cpus;
  // TODO(luobogao): For the moment we do not need distances between node, but
  // providing it would be better.
};

std::vector<Node> GetAvailableNodes();

// Note that node IDs are NOT necessarily contiguous. If you want need index of
// current node in vector returned by `GetAvailableNodes()`. call
// `GetNodeIndexOf` or `GetCurrentNodeIndex()` instead.
int GetCurrentNode();

// Get current node index.
std::size_t GetCurrentNodeIndex();

// Maps from node index to node ID.
int GetNodeId(std::size_t index);

// Maps from node ID to index.
std::size_t GetNodeIndex(int node_id);

// Upperbound (exclusive) of indices.
std::size_t GetNumberOfNodesAvailable();

// Get NUMA node ID of processor #cpu. `-1` is returned if `cpu` is not
// accessible to us.
int GetNodeOfProcessor(int cpu);

}  // namespace numa

// FIXME: Naming is not consistent here, replace `CPU`s below with `Processor`.
int GetCurrentProcessorId();

// CAUTION: If some processors are disabled, we may find processors whose ID is
// greater than number of processors.
std::size_t GetNumberOfProcessorsAvailable();
std::size_t GetNumberOfProcessorsConfigured();
bool IsInaccessibleProcessorPresent();
bool IsProcessorAccessible(int cpu);

// Parse processor list. e.g., "1-10,21,-1".
//
// Not sure if this is the right place to declare it though..
std::optional<std::vector<int>> TryParseProcesserList(const std::string& s);

}  // namespace flare::internal

#endif  // FLARE_BASE_INTERNAL_CPU_H_
