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

#ifndef FLARE_NET_COS_COS_STATUS_H_
#define FLARE_NET_COS_COS_STATUS_H_

#include <optional>
#include <string>

#include "flare/base/status.h"
#include "flare/net/http/types.h"

namespace flare {

enum class CosStatus {
  Success = 0,  // Hardly used.

  // Using this status code is discouraged. Consider use a more specific one
  // instead.
  Unsuccessful = 1,

  Timeout = 2,
  HttpError = 3,          // General HTTP failure.
  InvalidArguments = 4,   // `PrepareTask` failed.
  MalformedResponse = 5,  // `ParseResult` failed.
  UnknownCosStatus = 6,
  AddressResolutionFailure = 7,  // Polaris address resolution failure.
  NotOpened = 8,  // COS client has not yet been opened successfully.

  // Defined by COS.
  //
  // @sa: https://cloud.tencent.com/document/product/436/7730
  ActionAccelerateNotSupported = 1000,
  AttachmentFull,
  BadDigest,
  BadRequest,
  BucketAccelerateNotEnabled,
  BucketNameTooLong,
  BucketVersionNotOpen,
  DNSRecordVerifyFailed,
  EntitySizeNotMatch,
  EntityTooLarge,
  EntityTooSmall,
  ExpiredToken,
  ImageResolutionExceed,
  ImageTooLarge,
  IncompleteBody,
  IncorrectNumberOfFilesInPostRequest,
  InvalidArgument,
  InvalidBucketName,
  InvalidCopySource,
  InvalidDelimiter,
  InvalidDigest,
  InvalidImageFormat,
  InvalidImageSource,
  InvalidLocationConstraint,
  InvalidObjectName,
  InvalidPart,
  InvalidPartOrder,
  InvalidPicOperations,
  InvalidPolicyDocument,
  InvalidRegionName,
  InvalidRequest,
  InvalidSHA1Digest,
  InvalidTag,
  InvalidTargetBucketForLogging,
  InvalidUploadStatus,
  InvalidURI,
  InventoryFull,
  JsonAPINotSupportOnMAZBucket,
  KeyTooLong,
  KmsException,
  KmsKeyDisabled,
  KmsKeyNotExist,
  ListPartUploadIdIsEmpty,
  LoggingConfExists,
  LoggingPrefixInvalid,
  MalformedPolicy,
  MalformedPOSTRequest,
  MalformedXML,
  MAZOperationNotSupportOnOAZBucket,
  MissingRequestBodyError,
  MultiAZFeatureNotSupport,
  MultiBucketNotSupport,
  NotifyRuleEventConflict,
  NotifyRulePrefixConflict,
  NotifyRuleSuffixConflict,
  NotSupportedStorageClass,
  OAZOperationNotSupportOnMAZBucket,
  PolicyFull,
  PolicyVersionFull,
  RequestTimeout,
  SsecDecryptHeaderInvalid,
  SSEContentNotSupported,
  SSEHeaderNotAllowed,
  TargetBucketNameInvalid,
  TooManyBuckets,
  UnexpectedContent,
  UserCnameInvalid,
  UserNetworkTooSlow,
  VerifyAlgorithmNotSupported,
  WebsiteURLInvalid,
  XMLSizeLimit,
  PaymentRequired,
  AccessDenied,
  AccessForbidden,
  InvalidAccessKeyId,
  InvalidObjectState_403,  // Under HTTP 403 category.
  NoProcessAuthority,
  RequestTimeTooSkewed,
  RequestHasExpired,
  SignatureDoesNotMatch,
  UserNotSourceBucketOwner,
  UserNotTargetBucketOwner,
  InventoryConfigurationNotFoundError,
  NoBucketQuotaPolicy,
  NoSuchBucket,
  NoSuchCopySource,
  NoSuchCORSConfiguration,
  NoSuchEncryptionConfiguration,
  NoSuchJob,
  NoSuchKey,
  NoSuchLifecycleConfiguration,
  NoSuchObjectLockConfiguration,
  NoSuchPolicyVersion,
  NoSuchTagSet,
  NoSuchUpload,
  NoSuchVersion,
  NoSuchWebsiteConfiguration,
  OriginConfigurationNotFoundError,
  ReplicationConfigurationNotFoundError,
  MethodNotAllowed,
  RestoreNonArchiveObject,
  UploadIdNotSupported,
  AppendPositionErr,
  BucketAlreadyExists,
  BucketAlreadyOwnedByYou,
  BucketLocked,
  BucketNotEmpty,
  DomainConfigConflict,
  InvalidBucketState,
  InvalidLockedTime,
  ObjectLocked,
  InvalidObjectState_409,  // HTTP 409.
  PathConflict,
  QuotaConflict,
  QuotaOperationConflict,
  RecordAlreadyExist,
  RestoreAlreadyInProgress,
  UploadConflict,
  ObjectNotAppendable,
  MissingContentLength,
  PreconditionFailed,
  InvalidRange,
  DomainAuditFailed,
  UnavailableForLegalReasons,
  InternalError,
  KmsInternalException,
  NotImplemented,
  KmsFreqControl,
  ServiceUnavailable,
  SlowDown,

  // TODO(luobogao): Translate more HTTP 3xx codes.
  NotModified = 2000,  // HTTP 304.
};

namespace cos {

// Parses XML response from COS server. For internal use only.
//
// `MalformedResponse` or `UnknownCosStatus` is returned on failure.
Status ParseCosStatus(HttpStatus status, const std::string& resp);

}  // namespace cos

}  // namespace flare

#endif  // FLARE_NET_COS_COS_STATUS_H_
