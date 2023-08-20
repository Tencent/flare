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

#include "flare/rpc/name_resolver/util/domain_name_resolver.h"

#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/types.h>

#include "flare/base/logging.h"
#include "flare/base/string.h"

namespace flare::name_resolver::util {
namespace {

void SetErrorCode(int* error_code, int value) {
  if (error_code) *error_code = value;
}

void NormalizeDomain(std::string* domain) {
  if (EndsWith(*domain, ".")) {
    domain->pop_back();
  }  // remove the last '.' if exist.
  ToLower(domain);
}

bool ResolveDomainQuery(const std::string& hostname, std::uint16_t port,
                        std::vector<Endpoint>* addresses,
                        int* error_code = nullptr) {
  if (auto eps = flare::ResolveDomain(hostname, port); eps) {
    if (addresses->empty()) {
      addresses->swap(eps.value());
    } else {
      addresses->insert(addresses->end(), eps->begin(), eps->end());
    }
  } else {
    SetErrorCode(error_code, eps.error());
    return false;
  }

  SetErrorCode(error_code, 0);
  return true;
}

bool IsValidSegment(std::string_view segment) {
  if (segment.empty() || segment.size() > 63U) return false;
  // Must start and ends with alpha or number
  if (!isalnum(segment[0]) || !isalnum(segment[segment.size() - 1]))
    return false;
  for (int j = 1; j < static_cast<int>(segment.size()) - 1; ++j) {
    // '-' can appears in the middle part, but can't be double.
    if (!isalnum(segment[j])) {
      if (segment[j] != '-') return false;
      // Can't contains '--'
      if (segment[j + 1] == '-') return false;
    }
  }
  return true;
}

bool IsValidLastSegment(std::string_view last) {
  if (last.size() < 2 || last.size() > 6)  // Longest TLD: museum and travel
    return false;
  // The last part must be all lower latter, such as 'com', 'net'
  for (size_t i = 0; i < last.size(); ++i) {
    if (!islower(last[i])) return false;
  }
  return true;
}

bool IsValidDomain(const std::string& domain) {
  if (domain.empty() || !isalnum(domain.back())) return false;
  auto segments = Split(domain, ".");
  if (segments.empty()) {
    return false;
  }
  for (int i = 0; i < static_cast<int>(segments.size()) - 1; ++i) {
    std::string_view& segment = segments[i];
    if (!IsValidSegment(segment)) {
      return false;
    }
  }
  // Last part check only apply to real domain name, not hostname
  if (segments.size() == 1) return true;

  return IsValidLastSegment(segments.back());
}

/**
 * getaddrinfo() returns 0 if it succeeds, or one of the following nonzero error
 * codes:
 *
 *  Possible error codes, see netdb.h:
 *
 *  EAI_ADDRFAMILY
 *     The specified network host does not have any network addresses in the
 *     requested address family.
 *
 *  EAI_AGAIN
 *     The name server returned a temporary failure indication.  Try again
 *     later.
 *
 *  EAI_BADFLAGS
 *     hints.ai_flags contains invalid flags; or, hints.ai_flags included
 *     AI_CANONNAME and name was NULL.
 *
 *  EAI_FAIL
 *     The name server returned a permanent failure indication.
 *
 *  EAI_FAMILY
 *     The requested address family is not supported.
 *
 *  EAI_MEMORY
 *     Out of memory.
 *
 *  EAI_NODATA
 *     The specified network host exists, but does not have any network
 *     addresses defined.
 *
 *  EAI_NONAME
 *     The node or service is not known; or both node and service are NULL; or
 *     AI_NUMERICSERV was specified in hints.ai_flags and service was not a
 *     numeric port-number string.
 *
 *  EAI_SERVICE
 *     The  requested  service is not available for the requested socket type.
 *     It may be available through another socket type.  For example, this error
 *     could occur if service was "shell" (a service available only on stream
 *     sockets), and either hints.ai_protocol was IPPROTO_UDP, or
 *     hints.ai_socktype was SOCK_DGRAM; or the error could occur if service was
 *     not NULL, and hints.ai_socktype was SOCK_RAW (a socket type that  does
 *     not support the concept of services).
 *
 *  EAI_SOCKTYPE
 *     The requested socket type is not supported.  This could occur, for
 *     example, if hints.ai_socktype and hints.ai_protocol are inconsistent
 *     (e.g., SOCK_DGRAM and IPPROTO_TCP, respectively).
 *
 *  EAI_SYSTEM Other system error;
 *     errno is set to indicate the error.
 */
std::string ErrorString(int error_code) { return gai_strerror(error_code); }

}  // namespace

bool ResolveDomain(const std::string& domain, std::uint16_t port,
                   std::vector<Endpoint>* addresses) {
  std::string domain_normalized = domain;
  NormalizeDomain(&domain_normalized);
  if (!IsValidDomain(domain_normalized)) {
    FLARE_LOG_ERROR("Invalid domain: {}", domain_normalized);
    return false;
  }
  int error_code;
  bool res =
      ResolveDomainQuery(domain_normalized, port, addresses, &error_code);
  if (!res) {
    FLARE_LOG_ERROR("Fail to query domain {}", ErrorString(error_code));
  }
  return res;
}

}  // namespace flare::name_resolver::util
