//
//  PSAssetConstants.h
//  CertificateTool
//
//  Copyright (c) 2013-2015 Apple Inc. All Rights Reserved.
//

#ifndef _PSAssetConstants_h
#define _PSAssetConstants_h

#include <CoreFoundation/CoreFoundation.h>

enum
{
    isAnchor = (1UL << 0),
    isBlocked = (1UL << 1),
    isGrayListed = (1UL << 2),
    hasFullCert = (1UL << 3),
    hasCertHash = (1UL << 4),
    isAllowListed = (1UL << 5)
};

typedef unsigned long PSAssetFlags;

extern const CFStringRef kPSAssertCertificatesKey;
extern const CFStringRef kPSAssertVersionNumberKey;
extern const CFStringRef kPSAssetCertDataKey;
extern const CFStringRef kPSAssetCertHashKey;
extern const CFStringRef kPSAssetCertEVOIDSKey;
extern const CFStringRef kPSAssetCertFlagsKey;
extern const CFStringRef kPSAssertAdditionalDataKey;


#endif
