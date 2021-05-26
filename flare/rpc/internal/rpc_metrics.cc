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

#include "flare/rpc/internal/rpc_metrics.h"

#include <algorithm>
#include <map>
#include <shared_mutex>
#include <string>

namespace flare::rpc::detail {
namespace {

Json::Value DumpMetricsCnt(const WriteMostlyMetrics<std::uint64_t>& metrics) {
  Json::Value j;
  j["last_hour"] = static_cast<Json::Value::UInt64>(metrics.Get(3600).cnt);
  j["last_minute"] = static_cast<Json::Value::UInt64>(metrics.Get(60).cnt);
  j["last_second"] = static_cast<Json::Value::UInt64>(metrics.Get(1).cnt);
  j["total"] = static_cast<Json::Value::UInt64>(metrics.GetAll().cnt);
  return j;
}

Json::Value DumpMetricsLatency(const WriteMostlyMetrics<std::uint64_t>& m1,
                               const WriteMostlyMetrics<std::uint64_t>& m2) {
  auto combine_and_to_json = [](auto&& jsv, auto&& r1, auto&& r2) {
    jsv["average"] = static_cast<Json::Value::UInt64>(
        (r1.cnt + r2.cnt != 0)
            ? (r1.average * r1.cnt + r2.average * r2.cnt) / (r1.cnt + r2.cnt)
            : 0);
    jsv["max"] = static_cast<Json::Value::UInt64>(std::max(r1.max, r2.max));
    jsv["min"] = static_cast<Json::Value::UInt64>(
        (r1.cnt == 0 || r2.cnt == 0) ? std::max(r1.min, r2.min)
                                     : std::min(r1.min, r2.min));
  };
  Json::Value j;
  for (auto&& [k, v] : std::map<int, std::string>(
           {{3600, "last_hour"}, {60, "last_minute"}, {1, "last_second"}})) {
    auto&& r1 = m1.Get(k);
    auto&& r2 = m2.Get(k);
    combine_and_to_json(j[v], r1, r2);
  }
  combine_and_to_json(j["total"], m1.GetAll(), m2.GetAll());
  return j;
}

Json::Value DumpPacketSize(const WriteMostlyMetrics<std::size_t>& m) {
  Json::Value j;
  for (auto&& [k, v] : std::map<int, std::string>(
           {{3600, "last_hour"}, {60, "last_minute"}, {1, "last_second"}})) {
    auto&& r = m.Get(k);
    j[v]["average"] = static_cast<Json::UInt64>(r.average);
    j[v]["min"] = static_cast<Json::UInt64>(r.min);
    j[v]["max"] = static_cast<Json::UInt64>(r.max);
  }
  auto&& r = m.GetAll();
  j["total"]["average"] = static_cast<Json::UInt64>(r.average);
  j["total"]["min"] = static_cast<Json::UInt64>(r.min);
  j["total"]["max"] = static_cast<Json::UInt64>(r.max);
  return j;
}

void MergeGlobal(Json::Value* global, const Json::Value& method_stat) {
  // Latency should calc first, else g_cnt will change.
  for (auto&& k : {"last_hour", "last_minute", "last_second", "total"}) {
    auto&& g_latency = (*global)["latency"][k]["average"].asUInt64();
    auto&& m_latency = method_stat["latency"][k]["average"].asUInt64();
    auto&& g_cnt = (*global)["counter"]["total"][k].asUInt64();
    auto&& m_cnt = method_stat["counter"]["total"][k].asUInt64();
    (*global)["latency"][k]["average"] =
        (g_cnt + m_cnt != 0)
            ? (g_latency * g_cnt + m_latency * m_cnt) / (g_cnt + m_cnt)
            : 0;
    (*global)["latency"][k]["max"] = std::max((*global)["latency"][k]["max"],
                                              method_stat["latency"][k]["max"]);
    (*global)["latency"][k]["min"] =
        (g_cnt == 0 || m_cnt == 0) ? std::max((*global)["latency"][k]["min"],
                                              method_stat["latency"][k]["min"])
                                   : std::min((*global)["latency"][k]["min"],
                                              method_stat["latency"][k]["min"]);
  }
  for (auto&& pkt_size : {"packet_size_in", "packet_size_out"}) {
    for (auto&& k : {"last_hour", "last_minute", "last_second", "total"}) {
      auto&& g_pkt_size = (*global)[pkt_size][k]["average"].asUInt64();
      auto&& m_pkg_size = method_stat[pkt_size][k]["average"].asUInt64();
      auto&& g_cnt = (*global)["counter"]["total"][k].asUInt64();
      auto&& m_cnt = method_stat["counter"]["total"][k].asUInt64();
      (*global)[pkt_size][k]["average"] =
          (g_cnt + m_cnt != 0)
              ? (g_pkt_size * g_cnt + m_pkg_size * m_cnt) / (g_cnt + m_cnt)
              : 0;
      (*global)[pkt_size][k]["max"] = std::max((*global)[pkt_size][k]["max"],
                                               method_stat[pkt_size][k]["max"]);
      (*global)[pkt_size][k]["min"] =
          (g_cnt == 0 || m_cnt == 0)
              ? std::max((*global)[pkt_size][k]["min"],
                         method_stat[pkt_size][k]["min"])
              : std::min((*global)[pkt_size][k]["min"],
                         method_stat[pkt_size][k]["min"]);
    }
  }
  for (auto&& k1 : {"failure", "success", "total"}) {
    for (auto&& k2 : {"last_hour", "last_minute", "last_second", "total"}) {
      (*global)["counter"][k1][k2] = (*global)["counter"][k1][k2].asUInt64() +
                                     method_stat["counter"][k1][k2].asUInt64();
    }
  }
}

}  // namespace

void RpcMetrics::RegisterMethod(MethodDescriptorPtr method) {
  std::unique_lock lk(lock_);
  auto&& it = std::lower_bound(method_map_.begin(), method_map_.end(), method,
                               CmpMethodStats());
  if (it == method_map_.end() || it->first != method) {
    method_map_.insert(it, {method, std::make_unique<MethodStats>()});
  }  // Nothing to do otherwise.
}

void RpcMetrics::Dump(Json::Value* json_stat) const {
  std::shared_lock locker(lock_);
  Json::Value global;
  for (auto&& it = method_map_.begin(); it != method_map_.end(); ++it) {
    auto&& method_stat = DumpMethodStats(it->second.get());
    MergeGlobal(&global, method_stat);
    (*json_stat)[it->first->full_name()] = method_stat;
  }
  (*json_stat)["global"] = global;
}

Json::Value RpcMetrics::DumpMethodStats(const MethodStats* method_stats) const {
  Json::Value j;
  Json::Value& count_stat = j["counter"];
  count_stat["failure"] = DumpMetricsCnt(method_stats->failure);
  count_stat["success"] = DumpMetricsCnt(method_stats->success);
  for (auto&& key : {"last_hour", "last_minute", "last_second", "total"}) {
    count_stat["total"][key] = count_stat["failure"][key].asUInt64() +
                               count_stat["success"][key].asUInt64();
  }

  j["latency"] =
      DumpMetricsLatency(method_stats->success, method_stats->failure);
  j["packet_size_in"] = DumpPacketSize(method_stats->pkt_size_in);
  j["packet_size_out"] = DumpPacketSize(method_stats->pkt_size_out);
  return j;
}

void RpcMetrics::RegisterMethod(MethodDescriptorPtr method,
                                MethodCacheMap* cache,
                                MethodCacheMap::iterator* cache_it) {
  std::unique_lock lk(lock_);
  auto&& it = std::lower_bound(method_map_.begin(), method_map_.end(), method,
                               CmpMethodStats());
  if (it == method_map_.end() || it->first != method) {
    it = method_map_.insert(it, {method, std::make_unique<MethodStats>()});
  }
  *cache_it = cache->insert(*cache_it, {method, it->second.get()});
}

}  // namespace flare::rpc::detail
