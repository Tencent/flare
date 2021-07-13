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

#include "flare/net/cos/cos_status.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "rapidxml/rapidxml.h"

#include "flare/base/enum.h"
#include "flare/base/logging.h"
#include "flare/base/string.h"

using namespace std::literals;

namespace flare::cos {

namespace {

// Typo in code strings are kept, as we need an exact match on these strings.
constexpr std::tuple<HttpStatus, std::string_view, CosStatus> kCosStatusDescs[]{
    {HttpStatus::BadRequest, "ActionAccelerateNotSupported"sv,
     CosStatus::ActionAccelerateNotSupported},
    {HttpStatus::BadRequest, "AttachmentFull"sv, CosStatus::AttachmentFull},
    {HttpStatus::BadRequest, "BadDigest"sv, CosStatus::BadDigest},
    {HttpStatus::BadRequest, "BadRquest"sv, CosStatus::BadRequest},
    {HttpStatus::BadRequest, "BucketAccelerateNotEnabled"sv,
     CosStatus::BucketAccelerateNotEnabled},
    {HttpStatus::BadRequest, "BucketNameTooLong"sv,
     CosStatus::BucketNameTooLong},
    {HttpStatus::BadRequest, "BucketVersionNotOpen"sv,
     CosStatus::BucketVersionNotOpen},
    {HttpStatus::BadRequest, "DNSRecordVerifyFailed"sv,
     CosStatus::DNSRecordVerifyFailed},
    {HttpStatus::BadRequest, "EntitySizeNotMatch"sv,
     CosStatus::EntitySizeNotMatch},
    {HttpStatus::BadRequest, "EntityTooLarge"sv, CosStatus::EntityTooLarge},
    {HttpStatus::BadRequest, "EntityTooSmall"sv, CosStatus::EntityTooSmall},
    {HttpStatus::BadRequest, "ExpiredToken"sv, CosStatus::ExpiredToken},
    {HttpStatus::BadRequest, "ImageResolutionExceed"sv,
     CosStatus::ImageResolutionExceed},
    {HttpStatus::BadRequest, "ImageTooLarge"sv, CosStatus::ImageTooLarge},
    {HttpStatus::BadRequest, "IncompleteBody"sv, CosStatus::IncompleteBody},
    {HttpStatus::BadRequest, "IncorrectNumberOfFilesInPostRequest"sv,
     CosStatus::IncorrectNumberOfFilesInPostRequest},
    {HttpStatus::BadRequest, "InvalidArgument"sv, CosStatus::InvalidArgument},
    {HttpStatus::BadRequest, "InvalidBucketName"sv,
     CosStatus::InvalidBucketName},
    {HttpStatus::BadRequest, "InvalidCopySource"sv,
     CosStatus::InvalidCopySource},
    {HttpStatus::BadRequest, "InvalidDelimiter"sv, CosStatus::InvalidDelimiter},
    {HttpStatus::BadRequest, "InvalidDigest"sv, CosStatus::InvalidDigest},
    {HttpStatus::BadRequest, "InvalidImageFormat"sv,
     CosStatus::InvalidImageFormat},
    {HttpStatus::BadRequest, "InvalidImageSource"sv,
     CosStatus::InvalidImageSource},
    {HttpStatus::BadRequest, "InvalidLocationConstraint"sv,
     CosStatus::InvalidLocationConstraint},
    {HttpStatus::BadRequest, "InvalidObjectName"sv,
     CosStatus::InvalidObjectName},
    {HttpStatus::BadRequest, "InvalidPart"sv, CosStatus::InvalidPart},
    {HttpStatus::BadRequest, "InvalidPartOrder"sv, CosStatus::InvalidPartOrder},
    {HttpStatus::BadRequest, "InvalidPicOperations"sv,
     CosStatus::InvalidPicOperations},
    {HttpStatus::BadRequest, "InvalidPolicyDocument"sv,
     CosStatus::InvalidPolicyDocument},
    {HttpStatus::BadRequest, "InvalidRegionName"sv,
     CosStatus::InvalidRegionName},
    {HttpStatus::BadRequest, "InvalidRequest"sv, CosStatus::InvalidRequest},
    {HttpStatus::BadRequest, "InvalidSHA1Digest"sv,
     CosStatus::InvalidSHA1Digest},
    {HttpStatus::BadRequest, "InvalidTag"sv, CosStatus::InvalidTag},
    {HttpStatus::BadRequest, "InvalidTargetBucketForLogging"sv,
     CosStatus::InvalidTargetBucketForLogging},
    {HttpStatus::BadRequest, "InvalidUploadStatus"sv,
     CosStatus::InvalidUploadStatus},
    {HttpStatus::BadRequest, "InvalidURI"sv, CosStatus::InvalidURI},
    {HttpStatus::BadRequest, "InventoryFull"sv, CosStatus::InventoryFull},
    {HttpStatus::BadRequest, "JsonAPINotSupportOnMAZBucket"sv,
     CosStatus::JsonAPINotSupportOnMAZBucket},
    {HttpStatus::BadRequest, "KeyTooLong"sv, CosStatus::KeyTooLong},
    {HttpStatus::BadRequest, "KmsException"sv, CosStatus::KmsException},
    {HttpStatus::BadRequest, "KmsKeyDisabled"sv, CosStatus::KmsKeyDisabled},
    {HttpStatus::BadRequest, "KmsKeyNotExist"sv, CosStatus::KmsKeyNotExist},
    {HttpStatus::BadRequest, "ListPartUploadIdIsEmpty"sv,
     CosStatus::ListPartUploadIdIsEmpty},
    {HttpStatus::BadRequest, "LoggingConfExists"sv,
     CosStatus::LoggingConfExists},
    {HttpStatus::BadRequest, "LoggingPrefixInvalid"sv,
     CosStatus::LoggingPrefixInvalid},
    {HttpStatus::BadRequest, "MalformedPolicy"sv, CosStatus::MalformedPolicy},
    {HttpStatus::BadRequest, "MalformedPOSTRequest"sv,
     CosStatus::MalformedPOSTRequest},
    {HttpStatus::BadRequest, "MalformedXML"sv, CosStatus::MalformedXML},
    {HttpStatus::BadRequest, "MAZOperationNotSupportOnOAZBucket"sv,
     CosStatus::MAZOperationNotSupportOnOAZBucket},
    {HttpStatus::BadRequest, "MissingRequestBodyError"sv,
     CosStatus::MissingRequestBodyError},
    {HttpStatus::BadRequest, "MultiAZFeatureNotSupport"sv,
     CosStatus::MultiAZFeatureNotSupport},
    {HttpStatus::BadRequest, "MultiBucketNotSupport"sv,
     CosStatus::MultiBucketNotSupport},
    {HttpStatus::BadRequest, "NotifyRuleEventConflict"sv,
     CosStatus::NotifyRuleEventConflict},
    {HttpStatus::BadRequest, "NotifyRulePrefixConflict"sv,
     CosStatus::NotifyRulePrefixConflict},
    {HttpStatus::BadRequest, "NotifyRuleSuffixConflict"sv,
     CosStatus::NotifyRuleSuffixConflict},
    {HttpStatus::BadRequest, "NotSupportedStorageClass"sv,
     CosStatus::NotSupportedStorageClass},
    {HttpStatus::BadRequest, "OAZOperationNotSupportOnMAZBucket"sv,
     CosStatus::OAZOperationNotSupportOnMAZBucket},
    {HttpStatus::BadRequest, "PolicyFull"sv, CosStatus::PolicyFull},
    {HttpStatus::BadRequest, "PolicyVersionFull"sv,
     CosStatus::PolicyVersionFull},
    {HttpStatus::BadRequest, "RequestTimeout"sv, CosStatus::RequestTimeout},
    {HttpStatus::BadRequest, "SsecDecryptHeaderInvalid"sv,
     CosStatus::SsecDecryptHeaderInvalid},
    {HttpStatus::BadRequest, "SSEContentNotSupported"sv,
     CosStatus::SSEContentNotSupported},
    {HttpStatus::BadRequest, "SSEHeaderNotAllowed"sv,
     CosStatus::SSEHeaderNotAllowed},
    {HttpStatus::BadRequest, "TargetBucketNameInvalid"sv,
     CosStatus::TargetBucketNameInvalid},
    {HttpStatus::BadRequest, "TooManyBuckets"sv, CosStatus::TooManyBuckets},
    {HttpStatus::BadRequest, "UnexpectedContent"sv,
     CosStatus::UnexpectedContent},
    {HttpStatus::BadRequest, "UserCnameInvalid"sv, CosStatus::UserCnameInvalid},
    {HttpStatus::BadRequest, "UserNetworkTooSlow"sv,
     CosStatus::UserNetworkTooSlow},
    {HttpStatus::BadRequest, "VerifyAlgorithmNotSupported"sv,
     CosStatus::VerifyAlgorithmNotSupported},
    {HttpStatus::BadRequest, "WebsiteURLInvalid"sv,
     CosStatus::WebsiteURLInvalid},
    {HttpStatus::BadRequest, "XMLSizeLimit"sv, CosStatus::XMLSizeLimit},
    {HttpStatus::PaymentRequired, "PaymentRequired"sv,
     CosStatus::PaymentRequired},
    {HttpStatus::Forbidden, "AccessDenied"sv, CosStatus::AccessDenied},
    {HttpStatus::Forbidden, "AccessForbidden"sv, CosStatus::AccessForbidden},
    {HttpStatus::Forbidden, "InvalidAccessKeyId"sv,
     CosStatus::InvalidAccessKeyId},
    {HttpStatus::Forbidden, "InvalidObjectState"sv,
     CosStatus::InvalidObjectState_403},
    {HttpStatus::Forbidden, "NoProcessAuthority"sv,
     CosStatus::NoProcessAuthority},
    {HttpStatus::Forbidden, "RequestTimeTooSkewed"sv,
     CosStatus::RequestTimeTooSkewed},
    {HttpStatus::Forbidden, "Request has expired"sv,
     CosStatus::RequestHasExpired},
    {HttpStatus::Forbidden, "SignatureDoesNotMatch"sv,
     CosStatus::SignatureDoesNotMatch},
    {HttpStatus::Forbidden, "UserNotSourceBucketOwner"sv,
     CosStatus::UserNotSourceBucketOwner},
    {HttpStatus::Forbidden, "UserNotTargetBucketOwner"sv,
     CosStatus::UserNotTargetBucketOwner},
    {HttpStatus::NotFound, "InventoryConfigurationNotFoundError"sv,
     CosStatus::InventoryConfigurationNotFoundError},
    {HttpStatus::NotFound, "NoBucketQuotaPolicy"sv,
     CosStatus::NoBucketQuotaPolicy},
    {HttpStatus::NotFound, "NoSuchBucket"sv, CosStatus::NoSuchBucket},
    {HttpStatus::NotFound, "NoSuchCopySource"sv, CosStatus::NoSuchCopySource},
    {HttpStatus::NotFound, "NoSuchCORSConfiguration"sv,
     CosStatus::NoSuchCORSConfiguration},
    {HttpStatus::NotFound, "NoSuchEncryptionConfiguration"sv,
     CosStatus::NoSuchEncryptionConfiguration},
    {HttpStatus::NotFound, "NoSuchJob"sv, CosStatus::NoSuchJob},
    {HttpStatus::NotFound, "NoSuchKey"sv, CosStatus::NoSuchKey},
    {HttpStatus::NotFound, "NoSuchLifecycleConfiguration"sv,
     CosStatus::NoSuchLifecycleConfiguration},
    {HttpStatus::NotFound, "NoSuchObjectLockConfiguration"sv,
     CosStatus::NoSuchObjectLockConfiguration},
    {HttpStatus::NotFound, "NoSuchPolicyVersion"sv,
     CosStatus::NoSuchPolicyVersion},
    {HttpStatus::NotFound, "NoSuchTagSet"sv, CosStatus::NoSuchTagSet},
    {HttpStatus::NotFound, "NoSuchUpload"sv, CosStatus::NoSuchUpload},
    {HttpStatus::NotFound, "NoSuchVersion"sv, CosStatus::NoSuchVersion},
    {HttpStatus::NotFound, "NoSuchWebsiteConfiguration"sv,
     CosStatus::NoSuchWebsiteConfiguration},
    {HttpStatus::NotFound, "OriginConfigurationNotFoundError"sv,
     CosStatus::OriginConfigurationNotFoundError},
    {HttpStatus::NotFound, "ReplicationConfigurationNotFoundError"sv,
     CosStatus::ReplicationConfigurationNotFoundError},
    {HttpStatus::MethodNotAllowed, "MethodNotAllowed"sv,
     CosStatus::MethodNotAllowed},
    {HttpStatus::MethodNotAllowed, "RestoreNonArchiveObject"sv,
     CosStatus::RestoreNonArchiveObject},
    {HttpStatus::MethodNotAllowed, "UploadIdNotSupported"sv,
     CosStatus::UploadIdNotSupported},
    {HttpStatus::Conflict, "AppendPositionErr"sv, CosStatus::AppendPositionErr},
    {HttpStatus::Conflict, "BucketAlreadyExists"sv,
     CosStatus::BucketAlreadyExists},
    {HttpStatus::Conflict, "BucketAlreadyOwnedByYou"sv,
     CosStatus::BucketAlreadyOwnedByYou},
    {HttpStatus::Conflict, "BucketLocked"sv, CosStatus::BucketLocked},
    {HttpStatus::Conflict, "BucketNotEmpty"sv, CosStatus::BucketNotEmpty},
    {HttpStatus::Conflict, "DomainConfigConflict"sv,
     CosStatus::DomainConfigConflict},
    {HttpStatus::Conflict, "InvalidBucketState"sv,
     CosStatus::InvalidBucketState},
    {HttpStatus::Conflict, "InvalidLockedTime"sv, CosStatus::InvalidLockedTime},
    {HttpStatus::Conflict, "ObjectLocked"sv, CosStatus::ObjectLocked},
    {HttpStatus::Conflict, "InvalidObjectState"sv,
     CosStatus::InvalidObjectState_409},
    {HttpStatus::Conflict, "PathConflict"sv, CosStatus::PathConflict},
    {HttpStatus::Conflict, "QuotaConflict"sv, CosStatus::QuotaConflict},
    {HttpStatus::Conflict, "QuotaOperationConfilct"sv,
     CosStatus::QuotaOperationConflict},
    {HttpStatus::Conflict, "RecordAlreadyExist"sv,
     CosStatus::RecordAlreadyExist},
    {HttpStatus::Conflict, "RestoreAlreadyInProgress"sv,
     CosStatus::RestoreAlreadyInProgress},
    {HttpStatus::Conflict, "UploadConflict"sv, CosStatus::UploadConflict},
    {HttpStatus::Conflict, "ObjectNotAppendable"sv,
     CosStatus::ObjectNotAppendable},
    {HttpStatus::LengthRequired, "MissingContentLength"sv,
     CosStatus::MissingContentLength},
    {HttpStatus::PreconditionFailed, "PreconditionFailed"sv,
     CosStatus::PreconditionFailed},
    {HttpStatus::RangeNotSatisfiable, "InvalidRange"sv,
     CosStatus::InvalidRange},
    {HttpStatus::UnavailableForLegalReasons, "DomainAuditFailed"sv,
     CosStatus::DomainAuditFailed},
    {HttpStatus::UnavailableForLegalReasons, "UnavailableForLegalReasons"sv,
     CosStatus::UnavailableForLegalReasons},
    {HttpStatus::InternalServerError, "InternalError"sv,
     CosStatus::InternalError},
    {HttpStatus::InternalServerError, "KmsInternalException"sv,
     CosStatus::KmsInternalException},
    {HttpStatus::NotImplemented, "NotImplemented"sv, CosStatus::NotImplemented},
    {HttpStatus::ServiceUnavailable, "KmsFreqControl"sv,
     CosStatus::KmsFreqControl},
    {HttpStatus::ServiceUnavailable, "ServiceUnavailable"sv,
     CosStatus::ServiceUnavailable},
    {HttpStatus::ServiceUnavailable, "SlowDown"sv, CosStatus::SlowDown},
};

const std::map<std::pair<HttpStatus, std::string_view>, CosStatus>&
GetStringToStatusCodeMapping() {
  static const auto result = [] {
    std::map<std::pair<HttpStatus, std::string_view>, CosStatus> result;
    for (auto&& [x, y, z] : kCosStatusDescs) {
      result[std::pair(x, y)] = z;
    }
    return result;
  }();
  return result;
}

std::optional<CosStatus> TryMapCodeErrorCode(HttpStatus status,
                                             std::string_view str) {
  if (auto iter = GetStringToStatusCodeMapping().find(std::pair(status, str));
      iter != GetStringToStatusCodeMapping().end()) {
    return iter->second;
  }
  return std::nullopt;
}

}  // namespace

Status ParseCosStatus(HttpStatus status, const std::string& resp) {
  if (status == HttpStatus::NotModified) {
    return Status(CosStatus::NotModified);
  }
  try {
    rapidxml::xml_document doc;
    doc.parse<0>(const_cast<char*>(resp.c_str()));
    auto error = doc.first_node("Error");
    if (!error) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Error node is not present in COS's error text.");
      return flare::Status(CosStatus::MalformedResponse, resp);
    }
    auto code = error->first_node("Code");
    auto message = error->first_node("Message");
    auto resource = error->first_node("Resource");
    auto request_id = error->first_node("RequestId");
    auto trace_id = error->first_node("TraceId");
    if (!code || !message || !resource || !request_id || !trace_id) {
      FLARE_LOG_WARNING_EVERY_SECOND(
          "Missing critical fields in error response?");
      return flare::Status(CosStatus::MalformedResponse, resp);
    }
    auto desc =
        Format("[{}] {} (resource = {}, request_id = {}, trace_id = {})",
               code->value(), message->value(), resource->value(),
               request_id->value(), trace_id->value());
    if (auto result = TryMapCodeErrorCode(status, code->value())) {
      return Status(*result, desc);
    }
    return Status(CosStatus::UnknownCosStatus, desc);
  } catch (const rapidxml::parse_error& xcpt) {
    FLARE_LOG_WARNING_EVERY_SECOND("Failed to parse COS response: {}",
                                   xcpt.what());
    return flare::Status(CosStatus::MalformedResponse, resp);
  }
}

}  // namespace flare::cos
