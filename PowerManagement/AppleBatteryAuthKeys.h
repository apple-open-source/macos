#include <TargetConditionals.h>

#if TARGET_OS_IOS || TARGET_OS_WATCH || (TARGET_OS_OSX && TARGET_CPU_ARM64)
#pragma once

#define APPLE_BATTERYAUTH_RETRY_CERTSN_KEY      "GetCertSnRetryCnt"
#define APPLE_BATTERYAUTH_RETRY_INFO_KEY        "GetInfoRetryCnt"
#define APPLE_BATTERYAUTH_RETRY_SIGNATURE_KEY   "GetSignatureRetryCnt"
#define APPLE_BATTERYAUTH_RETRY_CERT_KEY        "GetCertificateRetryCnt"
#define APPLE_BATTERYAUTH_RETRY_TRUST_KEY       "SetTrustStatusRetryCnt"
#define APPLE_BATTERYAUTH_RETRY_GGRESET_KEY     "RetryWithGGResetCnt"

#endif /* TARGET_OS_IOS || TARGET_OS_WATCH || (TARGET_OS_OSX && TARGET_CPU_ARM64)*/
