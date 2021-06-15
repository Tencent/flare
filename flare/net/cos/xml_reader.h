// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FLARE_NET_COS_XML_READER_H_
#define FLARE_NET_COS_XML_READER_H_

#include <string>
#include <type_traits>

#include "rapidxml/rapidxml.h"

#include "flare/base/encoding/percent.h"
#include "flare/base/logging.h"
#include "flare/base/string.h"

// This macro simplifies copying field from XML to C++ type.
#define FLARE_COS_READ_XML_NODE(Variable, NodeName, To)                   \
  if (!::flare::cos::detail::CopyXmlNodeTo(Variable, NodeName, To, false, \
                                           false)) {                      \
    return false;                                                         \
  }

// This macro does nothing is the requested node is not present (but it would
// still raise an error if the node does present with a wrong type.).
#define FLARE_COS_READ_XML_NODE_OPT(Variable, NodeName, To)              \
  if (!::flare::cos::detail::CopyXmlNodeTo(Variable, NodeName, To, true, \
                                           false)) {                     \
    return false;                                                        \
  }

// This macro further decode value using pct-encoding.
#define FLARE_COS_READ_XML_NODE_PCT_ENCODED(Variable, NodeName, To)       \
  if (!::flare::cos::detail::CopyXmlNodeTo(Variable, NodeName, To, false, \
                                           true)) {                       \
    return false;                                                         \
  }

// Pct-encoded, with missing node ignored.
#define FLARE_COS_READ_XML_NODE_PCT_ENCODED_OPT(Variable, NodeName, To)  \
  if (!::flare::cos::detail::CopyXmlNodeTo(Variable, NodeName, To, true, \
                                           true)) {                      \
    return false;                                                        \
  }

namespace flare::cos::detail {

template <class T>
bool CopyXmlNodeTo(const rapidxml::xml_node<char>* node, const char* name,
                   T* to, bool ignore_missing,
                   [[maybe_unused]] bool pct_encoded) {
  FLARE_CHECK(node);
  if (auto p = node->first_node(name)) {
    if constexpr (std::is_same_v<T, std::string>) {
      // For string type there's nothing to cast.
      if (pct_encoded) {
        if (auto opt = DecodePercent(p->value())) {
          *to = *opt;
          return true;
        } else {
          FLARE_LOG_WARNING_EVERY_SECOND(
              "Failed to decode node [{}] with value [{}] using pct-encoding.",
              name, p->value());
          return false;
        }
      } else {
        *to = p->value();
        return true;
      }
    } else {
      // pct-encoding shouldn't make a difference here.
      if (auto opt = TryParse<T>(p->value())) {
        *to = *opt;
        return true;
      } else {
        FLARE_LOG_WARNING_EVERY_SECOND(
            "Failed to cast node [{}] with value [{}] to type [{}}.", name,
            p->value(), GetTypeName<T>());
        return false;
      }
    }
  } else {
    // Node is not present.
    if (ignore_missing) {
      return true;
    }
    FLARE_LOG_WARNING_EVERY_SECOND("Failed to find node [{}].", name);
    return false;
  }
}

}  // namespace flare::cos::detail

#endif  // FLARE_NET_COS_XML_READER_H_
