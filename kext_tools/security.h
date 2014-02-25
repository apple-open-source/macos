/*
 *  security.h
 *  kext_tools
 *
 *  Copyright 20012 Apple Inc. All rights reserved.
 *
 */
#ifndef _SECURITY_H
#define _SECURITY_H

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/kext/OSKext.h>
#include <mach/mach_error.h>

//  <rdar://problem/12435992>
#include <asl.h>
#include <Security/SecCode.h>
#include <Security/SecCodeSigner.h>
#include <Security/SecStaticCode.h>
#include <Security/SecRequirement.h>
#include <Security/SecRequirementPriv.h>
#include <Security/SecCodePriv.h>
#include <Security/cssmerr.h>

#define kMessageTracerDomainKey     "com.apple.message.domain"
#define kMessageTracerHashKey       "com.apple.message.hash"
#define kMessageTracerBundleIDKey   "com.apple.message.bundleID"
#define kMessageTracerVersionKey    "com.apple.message.version"
#define kMessageTracerKextNameKey   "com.apple.message.kextname"
#define kMessageTracerFatKey        "com.apple.message.fat"
#define kMessageTracerArchKey       "com.apple.message.architecture"

#define kMessageTracerTeamIdKey     "com.apple.message.teamid"
#define kMessageTracerSubjectCNKey  "com.apple.message.subjectcn"
#define kMessageTracerIssuerCNKey   "com.apple.message.issuercn"

#define kMessageTracerSignatureTypeKey "com.apple.message.signaturetype"
#define kMessageTracerPathKey       "com.apple.message.kextpath"

#define kAppleKextWithAppleRoot \
"Apple kext with Apple root"
#define k3rdPartyKextWithAppleRoot \
"3rd-party kext with Apple root"
#define k3rdPartyKextWithoutAppleRoot \
"3rd-party kext without Apple root"
#define k3rdPartyKextWithDevIdPlus \
"3rd-party kext with devid+ certificate"
#define k3rdPartyKextWithRevokedDevIdPlus \
"3rd-party kext with revoked devid+ certificate"
#define kUnsignedKext \
"Unsigned kext"

/* "com.apple.libkext.kext.loading" was used in 10.8
 * "com.apple.libkext.kext.loading.v3"  is used in 10.9 */
#define kMTKextLoadingDomain        "com.apple.libkext.kext.loading.v3"
#define kMTKextBlockedDomain        "com.apple.libkext.kext.blocked"

void    messageTraceExcludedKext(OSKextRef aKext);
void    recordKextLoadListForMT(CFArrayRef kextList);
void    recordKextLoadForMT(OSKextRef aKext);

Boolean isDebugSetInBootargs(void);
OSStatus checkKextSignature(OSKextRef aKext, Boolean checkExceptionList);
Boolean isInExceptionList(OSKextRef aKext, Boolean useCache);
Boolean isInLibraryExtensionsFolder(OSKextRef theKext);

#endif // _SECURITY_H
