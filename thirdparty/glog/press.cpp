// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
//
// Author: Shixuan YU <shixuanyu@tencent.com>

#include <signal.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>

#include "gflags/gflags.h"
#include "glog/logging.h"
#include "glog/raw_logging.h"

DEFINE_int32(threads, 10, "number of threads");
DEFINE_int32(iterations, 50000, "each thread iterations");
DEFINE_bool(info, false, "log level info or warning");

using namespace std::literals;

template <class T>
inline T ReadClock(int type) {
  timespec ts;
  clock_gettime(type, &ts);
  return T((ts.tv_sec * 1'000'000'000LL + ts.tv_nsec) * 1ns);
}

std::chrono::steady_clock::time_point ReadSteadyClock() {
  return ReadClock<std::chrono::steady_clock::time_point>(CLOCK_MONOTONIC);
}

// counter[timeout in microseconds] = number of requests
std::atomic<std::size_t> counter[2000000];

void DumpStatistics() {
  thread_local std::vector<std::size_t> local_counter(std::size(counter));
  memcpy(local_counter.data(), counter, sizeof(counter));
  memset(reinterpret_cast<void*>(counter), 0, sizeof(counter));

  std::uint64_t avg = 0;
  for (int i = 0; i != local_counter.size(); ++i) {
    avg += local_counter[i] * i;
    if (i) {
      local_counter[i] += local_counter[i - 1];
    }
  }

#define UPDATE_STATISTIC(var, x, y)              \
  if (!var && local_counter[i] > reqs * x / y) { \
    var = i;                                     \
  }

  std::uint64_t p90 = 0, p95 = 0, p99 = 0, p995 = 0, p999 = 0, p9999 = 0,
                max = 0;
  std::size_t reqs = local_counter.back();
  avg /= reqs;
  for (int i = 0; i != std::size(local_counter); ++i) {
    UPDATE_STATISTIC(p90, 90, 100);
    UPDATE_STATISTIC(p95, 95, 100);
    UPDATE_STATISTIC(p99, 99, 100);
    UPDATE_STATISTIC(p995, 995, 1000);
    UPDATE_STATISTIC(p999, 999, 1000);
    UPDATE_STATISTIC(p9999, 9999, 10000);
    if (local_counter[i] == reqs) {
      max = i;
      break;
    }
  }
  std::cout << std::endl
            << "time in us " << std::endl
            << "avg: " << avg << std::endl
            << "p90: " << p90 << std::endl
            << "p95: " << p95 << std::endl
            << "p99: " << p99 << std::endl
            << "p995: " << p995 << std::endl
            << "p999: " << p999 << std::endl
            << "max: " << max << std::endl;
}

using namespace std::chrono_literals;

std::atomic<std::uint64_t> total_droped{0};
bool full_hooker(google::LogSeverity severity, time_t timestamp,
                 const std::string& message) {
  static std::atomic<bool> first(true);
  if (first.exchange(false)) {
    std::cerr << "Start droping, first message : "
              << "severity " << severity << " time_t " << timestamp
              << " message " << message << std::endl;
  }
  total_droped++;
  return false;
}

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  std::vector<std::thread> threads;
  for (int i = 0; i < FLAGS_threads; ++i) {
    threads.push_back(std::thread([&] {
      for (int i = 0; i < FLAGS_iterations; ++i) {
        if (i % 2 == 0) {
          std::this_thread::sleep_for(1ms);
        }
        auto start = ReadSteadyClock();
        if (FLAGS_info) {
          LOG(INFO) << std::string(100, 'i');
        } else {
          LOG(WARNING) << std::string(100, 'i');
        }
        int time_used = (ReadSteadyClock() - start) / 1us;
        time_used = std::min<std::size_t>(time_used, std::size(counter) - 1);
        ++counter[time_used];
      }
    }));
  }
  for (int i = 0; i < FLAGS_threads; ++i) {
    threads[i].join();
  }

  std::cout << "Iterations times " << FLAGS_iterations << std::endl
            << "with thread " << FLAGS_threads << std::endl
            << "Total droped message " << total_droped.load() << std::endl;

  DumpStatistics();

  return 0;
}
