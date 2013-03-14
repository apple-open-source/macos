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

//  <rdar://problem/12435992> Message tracing for kext loads
#include <asl.h>
#include <Security/SecCode.h>
#include <Security/SecCodeSigner.h>
#include <Security/SecStaticCode.h>
#include <Security/SecRequirement.h>
#include <Security/SecRequirementPriv.h>
#include <Security/SecCodePriv.h>

#define kMessageTracerDomainKey     "com.apple.message.domain"
#define kMessageTracerHashKey       "com.apple.message.hash"
#define kMessageTracerBundleIDKey   "com.apple.message.bundleID"
#define kMessageTracerVersionKey    "com.apple.message.version"
#define kMessageTracerKextNameKey   "com.apple.message.kextname"
#define kMTKextLoadingDomain        "com.apple.libkext.kext.loading"

void    logMTMessage(OSKextRef aKext);
Boolean isDebugSetInBootargs(void);

#endif // _SECURITY_H
