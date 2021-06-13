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

#include "flare/net/cos/signature.h"

#include "gtest/gtest.h"

namespace flare::cos {

// Sample input / output comes from COS's official documentation.
//
// @sa: https://cloud.tencent.com/document/product/436/7778

TEST(Signature, Upload) {
  auto auth_str = GenerateCosAuthString(
      "AKIDQjz3ltompVjBni5LitkWHFlFpwkn9U5q",
      "BQYIM75p8x0iWVFSIgqEKwFprpRSVHlz", HttpMethod::Put,
      "http://.../exampleobject(%E8%85%BE%E8%AE%AF%E4%BA%91)",
      {"Date: Thu, 16 May 2019 06:45:51 GMT",
       "Host: examplebucket-1250000000.cos.ap-beijing.myqcloud.com",
       "Content-Type: text/plain", "Content-Length: 13",
       "Content-MD5: mQ/fVh815F3k6TAUm8m0eg==", "x-cos-acl: private",
       "x-cos-grant-read: uin=\"100000000011\""},
      "1557989151;1557996351");
  EXPECT_EQ(
      "q-sign-algorithm=sha1&q-ak=AKIDQjz3ltompVjBni5LitkWHFlFpwkn9U5q&q-sign-"
      "time=1557989151;1557996351&q-key-time=1557989151;1557996351&q-header-"
      "list=content-length;content-md5;content-type;date;host;x-cos-acl;x-cos-"
      "grant-read&q-url-param-list=&q-signature="
      "3b8851a11a569213c17ba8fa7dcf2abec6935172",
      auth_str);
}

TEST(Signature, Download) {
  auto auth_str = GenerateCosAuthString(
      "AKIDQjz3ltompVjBni5LitkWHFlFpwkn9U5q",
      "BQYIM75p8x0iWVFSIgqEKwFprpRSVHlz", HttpMethod::Get,
      "http://.../"
      "exampleobject(%E8%85%BE%E8%AE%AF%E4%BA%91)?response-content-type="
      "application%2Foctet-stream&response-cache-control=max-age%3D600",
      {"Date: Thu, 16 May 2019 06:55:53 GMT",
       "Host: examplebucket-1250000000.cos.ap-beijing.myqcloud.com"},
      "1557989753;1557996953");
  EXPECT_EQ(
      "q-sign-algorithm=sha1&q-ak=AKIDQjz3ltompVjBni5LitkWHFlFpwkn9U5q&q-sign-"
      "time=1557989753;1557996953&q-key-time=1557989753;1557996953&q-header-"
      "list=date;host&q-url-param-list=response-cache-control;response-content-"
      "type&q-signature=01681b8c9d798a678e43b685a9f1bba0f6c0e012",
      auth_str);
}

}  // namespace flare::cos
