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
  struct hostent* he = nullptr;
  int error = 0;
#ifdef _GNU_SOURCE
  char buf[4096];
  struct hostent he_buf;
  gethostbyname_r(hostname.c_str(), &he_buf, buf, sizeof(buf), &he, &error);
#else
  he = gethostbyname(hostname.c_str());
  if (!he) error = h_errno;
#endif
  if (!he) {
    SetErrorCode(error_code, error);
    return false;
  }

  char** address = he->h_addr_list;
  if (address) {
    while (*address) {
      if (he->h_addrtype == AF_INET) {
        char buffer[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, reinterpret_cast<in_addr*>(*address), buffer,
                      INET_ADDRSTRLEN)) {
          addresses->push_back(EndpointFromIpv4(buffer, port));
        }
      } else if (he->h_addrtype == AF_INET6) {
        char buffer[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, reinterpret_cast<in6_addr*>(*address), buffer,
                      INET6_ADDRSTRLEN)) {
          addresses->push_back(EndpointFromIpv6(buffer, port));
        }
      }
      ++address;
    }
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
 * convert error code to human readable string.
 *
 *  Possible error codes, see netdb.h:
 *
 *  NETDB_SUCCESS
 *     No problem.
 *
 *  HOST_NOT_FOUND
 *     The specified host is unknown.
 *
 *  NO_ADDRESS or NO_DATA
 *     The requested name is valid but does not have an IP address.
 *
 *  NO_RECOVERY
 *     A non-recoverable name server error occurred.
 *
 *  TRY_AGAIN
 *     A temporary error occurred on an authoritative name server.  Try again
 * later.
 */
std::string ErrorString(int error_code) { return hstrerror(error_code); }

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
