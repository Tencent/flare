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

#include "flare/net/internal/http_engine.h"

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>

#include <algorithm>
#include <mutex>
#include <queue>
#include <vector>

#include "flare/base/internal/lazy_init.h"
#include "flare/base/object_pool.h"
#include "flare/base/thread/attribute.h"
#include "flare/fiber/detail/scheduling_group.h"
#include "flare/fiber/fiber.h"
#include "flare/fiber/runtime.h"

DEFINE_int32(flare_http_engine_workers_per_scheduling_group, 1,
             "http engine background workers per scheduling group");
DEFINE_int32(flare_http_engine_max_connections_per_host_per_worker, 50,
             "max connections per host per worker");
DEFINE_int32(flare_http_engine_max_total_connections_per_worker, 200,
             "max total connections per worker");
DEFINE_bool(flare_http_engine_use_epoll, false,
            "http client use epoll or poll");
DEFINE_bool(flare_http_engine_enable_debug, false,
            "If set, debugging output from libcurl is logged.");
DEFINE_bool(flare_http_engine_enable_debug_body, false,
            "If set, HTTP body is also logged.");

using namespace std::literals;

namespace flare {

namespace internal {

class Notifier {
 public:
  bool Init() {
    fd_ = eventfd(0, 0);
    if (fd_ < 0) return false;
    return SetupFd(fd_);
  }

  bool SetupFd(int fd) {
    int old_fl = fcntl(fd, F_GETFL, 0);
    if (old_fl < 0) {
      return false;
    }
    int f = fcntl(fd, F_SETFL, old_fl | O_NONBLOCK);
    if (f == -1) {
      return false;
    }
    int old_fd = fcntl(fd, F_GETFD, 0);
    if (old_fd < 0) return false;
    if (fcntl(fd, F_SETFD, old_fd | FD_CLOEXEC) == -1) return false;
    return true;
  }

  void Read() {
    eventfd_t u;
    while (eventfd_read(fd_, &u) == 0) {
      // Empty body
    }
  }

  bool Notify() {
    DCHECK_GE(fd_, 0);
    eventfd_t u = 1;
    return eventfd_write(fd_, u) == 0;
  }

  int Fd() { return fd_; }

 private:
  int fd_;
};

class CallContextQueue {
 public:
  void Push(PooledPtr<HttpTaskCallContext> ctx) {
    std::unique_lock _(pending_call_ctx_mutex_);
    pending_call_ctxs_.push(std::move(ctx));
  }

  // Out should ensure that the size is at least max_ctxs.
  // Returns the actual pop number.
  std::size_t Pop(PooledPtr<HttpTaskCallContext>* out, std::size_t max_ctxs) {
    if (max_ctxs == 0) return 0;
    std::unique_lock _(pending_call_ctx_mutex_);
    std::size_t i = 0;
    while (max_ctxs-- && !pending_call_ctxs_.empty()) {
      out[i++] = std::move(pending_call_ctxs_.front());
      pending_call_ctxs_.pop();
    }
    return i;
  }

 private:
  std::mutex pending_call_ctx_mutex_;
  std::queue<PooledPtr<HttpTaskCallContext>> pending_call_ctxs_;
};

int SockCallback(CURL* e, curl_socket_t s, int what, void* cbp, void* sockp);

int MultiTimerCallback(CURLM* multi, long timeout_ms, int* tfd) {  // NOLINT
  struct itimerspec its;
  if (timeout_ms > 0) {
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = timeout_ms / 1000;
    its.it_value.tv_nsec = (timeout_ms % 1000) * 1000 * 1000;
  } else if (timeout_ms == 0) {
    /* libcurl wants us to timeout now, however setting both fields of
     * new_value.it_value to zero disarms the timer. The closest we can
     * do is to schedule the timer to fire in 1 ns. */
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 1;
  } else {
    memset(&its, 0, sizeof(struct itimerspec));
  }

  timerfd_settime(*tfd, /*flags=*/0, &its, nullptr);
  return 0;
}

class CurlClient {
 public:
  CurlClient(std::size_t group, CallContextQueue* call_context_queue,
             Notifier* notifier) {
    multi_handle_ = curl_multi_init();
    FLARE_CHECK(multi_handle_, "Curl multi init failed");
    curl_multi_setopt(multi_handle_, CURLMOPT_MAXCONNECTS,
                      FLAGS_flare_http_engine_max_total_connections_per_worker);
    curl_multi_setopt(
        multi_handle_, CURLMOPT_MAX_HOST_CONNECTIONS,
        FLAGS_flare_http_engine_max_connections_per_host_per_worker);
    notifier_ = notifier;
    group_ = group;
    call_context_queue_ = call_context_queue;
    worker_ = std::thread([this, group] {
      flare::SetCurrentThreadAffinity(
          fiber::detail::GetSchedulingGroup(group)->Affinity());
      if (FLAGS_flare_http_engine_use_epoll) {
        InitEpoll();
        LoopEpoll();
      } else {
        LoopPoll();
      }
    });
  }

  void Stop() { exiting_.store(true, std::memory_order_relaxed); }

  ~CurlClient() {
    worker_.join();
    struct itimerspec its;
    timerfd_settime(tfd_, 0, &its, nullptr);
    curl_multi_cleanup(multi_handle_);
  }

  void InitEpoll() {
    epfd_ = epoll_create1(EPOLL_CLOEXEC);
    FLARE_CHECK(epfd_ != -1, "epoll_create1 failed");

    tfd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    FLARE_CHECK(tfd_ != -1, "timerfd_create failed");
    struct itimerspec its;
    memset(&its, 0, sizeof(struct itimerspec));
    its.it_interval.tv_sec = 0;
    its.it_value.tv_sec = 1;
    timerfd_settime(tfd_, 0, &its, nullptr);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = tfd_;
    epoll_ctl(epfd_, EPOLL_CTL_ADD, tfd_, &ev);

    curl_multi_setopt(multi_handle_, CURLMOPT_SOCKETFUNCTION, SockCallback);
    curl_multi_setopt(multi_handle_, CURLMOPT_SOCKETDATA, this);
    curl_multi_setopt(multi_handle_, CURLMOPT_TIMERFUNCTION,
                      MultiTimerCallback);
    curl_multi_setopt(multi_handle_, CURLMOPT_TIMERDATA, &tfd_);

    ev.events = EPOLLIN;
    ev.data.fd = notifier_->Fd();
    epoll_ctl(epfd_, EPOLL_CTL_ADD, notifier_->Fd(), &ev);
  }

  void TimerCallback(int revents) {
    uint64_t count = 0;
    ssize_t err = read(tfd_, &count, sizeof(uint64_t));
    if (err == -1) {
      /* Note that we may call the timer callback even if the timerfd isn't
       * readable. It's possible that there are multiple events stored in the
       * epoll buffer (i.e. the timer may have fired multiple times). The
       * event count is cleared after the first call so future events in the
       * epoll buffer will fail to read from the timer. */
      if (errno == EAGAIN) {
        FLARE_VLOG(100, "EAGAIN on tfd {}", tfd_);
        return;
      }
    }
    FLARE_CHECK(err == sizeof(uint64_t), "read(tfd) == {}", err);

    curl_multi_socket_action(multi_handle_, CURL_SOCKET_TIMEOUT, 0,
                             &still_running_);
    CheckMultiInfo();
  }

  void EventCallback(int fd, int revents) {
    int action = ((revents & EPOLLIN) ? CURL_CSELECT_IN : 0) |
                 ((revents & EPOLLOUT) ? CURL_CSELECT_OUT : 0);

    curl_multi_socket_action(multi_handle_, fd, action, &still_running_);

    CheckMultiInfo();
  }

  void CheckMultiInfo() {
    CURLMsg* msg;
    int msgs_left;
    while ((msg = curl_multi_info_read(multi_handle_, &msgs_left))) {
      if (msg->msg == CURLMSG_DONE) {
        CURL* e = msg->easy_handle;
        curl_multi_remove_handle(multi_handle_, e);
        fiber::internal::StartFiberDetached(
            Fiber::Attributes{.scheduling_group = group_},
            [this, e, msg]() mutable {
              void* pointer;
              curl_easy_getinfo(e, CURLINFO_PRIVATE, &pointer);
              EasyHandlerDone(e, msg->data.result,
                              reinterpret_cast<HttpTaskCallContext*>(pointer));
            });
      }
    }
  }

  void AddHandlers() {
    static constexpr std::size_t kMaxStilRunning = 50;
    static constexpr std::size_t kLocalPopArraySize = 30;
    PooledPtr<HttpTaskCallContext> local_pop_array[kLocalPopArraySize];
    std::size_t max_to_pop = (still_running_ >= kMaxStilRunning)
                                 ? 1
                                 : kMaxStilRunning - still_running_;
    PooledPtr<HttpTaskCallContext>* out;
    std::vector<PooledPtr<HttpTaskCallContext>> large_pop_array;
    if (max_to_pop <= kLocalPopArraySize) {
      out = local_pop_array;
    } else {
      large_pop_array.resize(max_to_pop);
      out = large_pop_array.data();
    }
    auto n = call_context_queue_->Pop(out, max_to_pop);
    for (std::size_t i = 0; i < n; ++i) {
      curl_multi_add_handle(multi_handle_, out[i]->curl_handler);
      HttpTaskCallContext* ctx = out[i].Leak();
      FLARE_CHECK_EQ(CURLE_OK,
                     curl_easy_setopt(ctx->curl_handler, CURLOPT_PRIVATE, ctx));
    }
  }

  void LoopEpoll() {
    struct epoll_event events[128];
    while (!exiting_.load(std::memory_order_relaxed)) {
      AddHandlers();
      int err = epoll_wait(epfd_, events,
                           sizeof(events) / sizeof(struct epoll_event), 1000);
      if (err == -1) {
        if (errno == EINTR) {
          FLARE_LOG_INFO_EVERY_SECOND("Note: wait interrupted");
          continue;
        } else {
          FLARE_CHECK(0, "Epoll wait error");
        }
      }
      for (int idx = 0; idx < err; ++idx) {
        if (events[idx].data.fd == tfd_) {
          TimerCallback(events[idx].events);
        } else if (events[idx].data.fd == notifier_->Fd()) {
          notifier_->Read();
        } else {
          EventCallback(events[idx].data.fd, events[idx].events);
        }
      }
    }
  }

  void LoopPoll() {
    int numfds;
    struct curl_waitfd extra_fds[1];
    extra_fds[0].fd = notifier_->Fd();
    extra_fds[0].events = CURL_WAIT_POLLIN;
    while (!exiting_.load(std::memory_order_relaxed)) {
      [[maybe_unused]] CURLMcode mc =
          curl_multi_perform(multi_handle_, &still_running_);
      CheckMultiInfo();
      AddHandlers();

      long curl_timeo = -1;  // NOLINT
      curl_multi_timeout(multi_handle_, &curl_timeo);
      if (curl_timeo < 0) {
        curl_timeo = 5;
      }
      mc = curl_multi_poll(multi_handle_, extra_fds, 1, curl_timeo, &numfds);
      notifier_->Read();
    }
  }

  void EasyHandlerDone(CURL* easy_handler, CURLcode result_code,
                       HttpTaskCallContext* ctx) {
    // `done` is kept alive until user's callback returns. This is necessary if
    // the user frees `HttpTaskCompletion` in its callback. If we keep `done` in
    // `HttpTaskCompletion`, user's callback might get freed even before it
    // completes.
    auto done = std::move(ctx->done);
    if (result_code != CURLE_OK) {
      done(Status(result_code, curl_easy_strerror(result_code)));
      object_pool::Put<HttpTaskCallContext>(ctx);
    } else {
      done(HttpTaskCompletion(ctx));
    }
  }

  CURLM* multi_handle_;
  int epfd_;

 private:
  std::atomic<bool> exiting_{false};
  std::thread worker_;

  int tfd_;
  Notifier* notifier_;
  std::size_t group_;
  CallContextQueue* call_context_queue_;
  int still_running_ = 0;
};

class CurlClientGroup {
 public:
  CurlClientGroup(std::size_t sz, std::size_t group) {
    FLARE_CHECK(notifier_.Init(), "Failed to init notifier");
    for (std::size_t i = 0; i < sz; ++i) {
      clients_.push_back(
          std::make_unique<CurlClient>(group, &queue_, &notifier_));
    }
  }

  void PushContext(PooledPtr<HttpTaskCallContext> ctx) {
    queue_.Push(std::move(ctx));
    notifier_.Notify();
  }

  void Stop() {
    for (auto&& c : clients_) {
      c->Stop();
    }
  }

 private:
  std::vector<std::unique_ptr<CurlClient>> clients_;
  CallContextQueue queue_;
  Notifier notifier_;
};

std::vector<std::unique_ptr<CurlClientGroup>> curl_client_groups;

struct SockInfo {
  curl_socket_t sockfd;
  CURL* easy;
  int action;
  long timeout;  // NOLINT
};

void SetSock(SockInfo* f, curl_socket_t s, CURL* e, int act, CurlClient* g) {
  struct epoll_event ev;
  int kind = ((act & CURL_POLL_IN) ? static_cast<int>(EPOLLIN) : 0) |
             ((act & CURL_POLL_OUT) ? static_cast<int>(EPOLLOUT) : 0);

  if (f->sockfd) {
    if (epoll_ctl(g->epfd_, EPOLL_CTL_DEL, f->sockfd, nullptr))
      FLARE_LOG_ERROR_EVERY_SECOND("EPOLL_CTL_DEL failed for fd: {} : {}\n",
                                   f->sockfd, strerror(errno));
  }

  f->sockfd = s;
  f->action = act;
  f->easy = e;

  ev.events = kind;
  ev.data.fd = s;
  if (epoll_ctl(g->epfd_, EPOLL_CTL_ADD, s, &ev))
    FLARE_LOG_ERROR_EVERY_SECOND("EPOLL_CTL_ADD failed for fd: {} : {}\n", s,
                                 strerror(errno));
}

void RemoveSock(SockInfo* f, CurlClient* g) {
  if (f) {
    if (f->sockfd) {
      if (epoll_ctl(g->epfd_, EPOLL_CTL_DEL, f->sockfd, nullptr))
        FLARE_LOG_ERROR_EVERY_SECOND("EPOLL_CTL_DEL failed for fd: %d : %s\n",
                                     f->sockfd, strerror(errno));
    }
    delete f;
  }
}

int SockCallback(CURL* e, curl_socket_t s, int what, void* cbp, void* sockp) {
  CurlClient* g = reinterpret_cast<CurlClient*>(cbp);
  SockInfo* fdp = reinterpret_cast<SockInfo*>(sockp);
  const char* whatstr[] = {"none", "IN", "OUT", "INOUT", "REMOVE"};
  FLARE_VLOG(100, "socket callback: s={} e={} what={} ", s, e, whatstr[what]);
  if (what == CURL_POLL_REMOVE) {
    RemoveSock(fdp, g);
  } else {
    if (!fdp) {
      SockInfo* fdp = new SockInfo;
      fdp->sockfd = 0;
      SetSock(fdp, s, e, what, g);
      curl_multi_assign(g->multi_handle_, s, fdp);
    } else {
      SetSock(fdp, s, e, what, g);
    }
  }
  return 0;
}

size_t HttpWriteCallback(char* ptr, size_t size, size_t nmemb, void* pstr) {
  auto bytes = size * nmemb;
  static_cast<flare::NoncontiguousBufferBuilder*>(pstr)->Append(ptr, bytes);
  return bytes;
}

// The header callback will be called once for each header and
// only complete header lines are passed on to the callback.
// Parsing headers is very easy using this.
size_t HttpHeaderCallback(char* ptr, size_t size, size_t nmemb, void* pstr) {
  std::size_t bytes = size * nmemb;
  std::string_view s(ptr, bytes);
  if (s.find_first_of(':') == std::string_view::npos) {  // Status-Line.
    return bytes;
  }
  if (s.size() > 2 && s[bytes - 2] == '\r' && s[bytes - 1] == '\n') {
    s.remove_suffix(2);
  }
  static_cast<std::vector<std::string>*>(pstr)->emplace_back(s);
  return bytes;
}

int HttpDebugCallback(CURL* handle, curl_infotype type, char* data, size_t size,
                      void* userptr) {
  std::string_view data_view(data, size);
  if (type == CURLINFO_TEXT || type == CURLINFO_HEADER_IN ||
      type == CURLINFO_HEADER_OUT) {
    FLARE_LOG_INFO("[{}] {}", type, data_view);
  } else if (type == CURLINFO_DATA_IN || type == CURLINFO_DATA_OUT) {
    if (FLAGS_flare_http_engine_enable_debug_body) {
      FLARE_LOG_INFO("[{}] {}", type, data_view);
    }  // Ignored otherwise.
  }    // Everything else is ignored.
  return 0;
}

HttpEngine* HttpEngine::Instance() {
  static NeverDestroyedSingleton<HttpEngine> engine;
  return engine.Get();
}

void HttpEngine::StartTask(
    HttpTask task, Function<void(Expected<HttpTaskCompletion, Status>)> done) {
  auto ctx = task.ctx_.Get();
  ctx->hdrs = std::move(task.hdrs_);
  if (FLAGS_flare_http_engine_enable_debug) {
    FLARE_CHECK_EQ(CURLE_OK,
                   curl_easy_setopt(ctx->curl_handler, CURLOPT_DEBUGFUNCTION,
                                    HttpDebugCallback));
    FLARE_CHECK_EQ(CURLE_OK,
                   curl_easy_setopt(ctx->curl_handler, CURLOPT_VERBOSE, 1));
  }
  FLARE_CHECK_EQ(CURLE_OK,
                 curl_easy_setopt(ctx->curl_handler, CURLOPT_NOSIGNAL, 1));
  FLARE_CHECK_EQ(CURLE_OK,
                 curl_easy_setopt(ctx->curl_handler, CURLOPT_WRITEFUNCTION,
                                  HttpWriteCallback));
  ctx->body = std::make_unique<NoncontiguousBufferBuilder>();
  FLARE_CHECK_EQ(
      CURLE_OK,
      curl_easy_setopt(ctx->curl_handler, CURLOPT_WRITEDATA, ctx->body.get()));
  FLARE_CHECK_EQ(CURLE_OK,
                 curl_easy_setopt(ctx->curl_handler, CURLOPT_HEADERFUNCTION,
                                  HttpHeaderCallback));
  FLARE_CHECK_EQ(CURLE_OK, curl_easy_setopt(ctx->curl_handler,
                                            CURLOPT_HEADERDATA, &ctx->headers));
  FLARE_CHECK_EQ(
      CURLE_OK,
      curl_easy_setopt(ctx->curl_handler, CURLOPT_HTTPHEADER, ctx->hdrs.get()));
  ctx->done = std::move(done);
  auto c = curl_client_groups[fiber::GetCurrentSchedulingGroupIndex()].get();
  c->PushContext(std::move(task.ctx_));
}

void HttpEngine::Stop() {
  for (auto&& c : curl_client_groups) {
    c->Stop();
  }
}

void HttpEngine::Join() {
  for (auto&& c : curl_client_groups) {
    c.reset();
  }
  curl_global_cleanup();
}

HttpEngine::HttpEngine() {
  auto ret = curl_global_init(CURL_GLOBAL_DEFAULT);
  FLARE_CHECK(!ret, "Curl Init failed {}", ret);
  for (auto i = 0; i < fiber::GetSchedulingGroupCount(); ++i) {
    curl_client_groups.push_back(std::make_unique<CurlClientGroup>(
        FLAGS_flare_http_engine_workers_per_scheduling_group, i));
  }
}

}  // namespace internal
}  // namespace flare
