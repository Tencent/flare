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

#include <chrono>
#include <unordered_map>

#include "gflags/gflags.h"

#include "flare/init.h"
#include "flare/init/override_flag.h"
#include "flare/net/cos/cos_client.h"
#include "flare/net/cos/ops/bucket/get_bucket.h"
#include "flare/net/cos/ops/object/delete_multiple_objects.h"
#include "flare/net/cos/ops/object/delete_object.h"
#include "flare/net/cos/ops/object/get_object.h"
#include "flare/net/cos/ops/object/put_object.h"

DEFINE_string(uri, "", "");
DEFINE_string(secret_id, "", "");
DEFINE_string(secret_key, "", "");
DEFINE_string(bucket, "", "");
DEFINE_string(key, "", "");
DEFINE_string(key2, "", "");
DEFINE_string(op, "", "");
DEFINE_string(bytes, "", "");

FLARE_OVERRIDE_FLAG(logtostderr, true);

namespace example {

void GetBucket(flare::CosClient* client) {
  flare::CosGetBucketRequest req;
  if (auto result = client->Execute(req)) {
    FLARE_LOG_INFO("Got {} entries:", result->contents.size());
    for (auto&& e : result->contents) {
      FLARE_LOG_INFO("{}\t{}\t{}", e.last_modified, e.size, e.key);
    }
  } else {
    FLARE_LOG_WARNING("Failed to list bucket: {}", result.error().ToString());
  }
}

void PutObject(flare::CosClient* client) {
  flare::CosPutObjectRequest req;
  req.key = FLAGS_key;
  req.bytes = flare::CreateBufferSlow(FLAGS_bytes);
  if (auto result = client->Execute(req)) {
    FLARE_LOG_INFO("Upload file successfully.");
  } else {
    FLARE_LOG_WARNING("Failed to upload file: {}", result.error().ToString());
  }
}

void GetObject(flare::CosClient* client) {
  flare::CosGetObjectRequest req;
  req.key = FLAGS_key;
  if (auto result = client->Execute(req)) {
    FLARE_LOG_INFO("Read {} bytes from file.", result->bytes.ByteSize());
  } else {
    FLARE_LOG_WARNING("Failed to download file: {}", result.error().ToString());
  }
}

void DeleteObject(flare::CosClient* client) {
  flare::CosDeleteObjectRequest req;
  req.key = FLAGS_key;
  if (auto result = client->Execute(req)) {
    FLARE_LOG_INFO("Deleted [{}] from COS.", FLAGS_key);
  } else {
    FLARE_LOG_WARNING("Failed to delete [{}] from COS: {}", FLAGS_key,
                      result.error().ToString());
  }
}

void DeleteMultipleObjects(flare::CosClient* client) {
  flare::CosDeleteMultipleObjectsRequest req;
  req.objects.emplace_back().key = FLAGS_key;
  req.objects.emplace_back().key = FLAGS_key2;
  if (auto result = client->Execute(req)) {
    FLARE_LOG_INFO("Deleted files from COS.");
  } else {
    FLARE_LOG_WARNING("Failed to delete files from COS: {}",
                      result.error().ToString());
  }
}

int Entry(int argc, char** argv) {
  flare::CosClient::Options options = {.secret_id = FLAGS_secret_id,
                                       .secret_key = FLAGS_secret_key,
                                       .bucket = FLAGS_bucket};
  flare::CosClient client;
  FLARE_CHECK(client.Open(FLAGS_uri, options));

  static const std::unordered_map<std::string, void (*)(flare::CosClient*)>
      kOperations = {{"get_bucket", GetBucket},
                     {"put_object", PutObject},
                     {"get_object", GetObject},
                     {"delete_object", DeleteObject},
                     {"delete_multiple_objects", DeleteMultipleObjects}};
  kOperations.at(FLAGS_op)(&client);
  return 0;
}

}  // namespace example

int main(int argc, char** argv) {
  return flare::Start(argc, argv, example::Entry);
}
