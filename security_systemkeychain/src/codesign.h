/*
 * Copyright (c) 2006-2010 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

//
// codesign - Swiss Army Knife tool for Code Signing operations
//
#ifndef _H_CODESIGN
#define _H_CODESIGN

#include "cs_utils.h"
#include <Security/Security.h>
#include <Security/CodeSigning.h>


//
// Main functions
//

void prepareToSign();
void sign(const char *target);
void prepareToVerify();
void verify(const char *target);
void dump(const char *target);
void hostinginfo(const char *target);
void procinfo(const char *target);
void procaction(const char *target);


//
// Program arguments
//
static const int pagesizeUnspecified = -1;
extern int pagesize;					// signing page size
extern SecIdentityRef signer;			// signer identity
extern SecKeychainRef keychain;			// source keychain for signer identity
extern const char *internalReq;			// internal requirement (raw optarg)
extern const char *testReq;				// external requirement (raw optarg)
extern const char *detached;			// detached signature path (to explicit file)
extern const char *detachedDb;			// reference to detached signature database
extern const char *entitlements;		// path to entitlement configuration input
extern const char *resourceRules;		// explicit resource rules template
extern const char *uniqueIdentifier;	// unique ident hash
extern const char *identifierPrefix;	// prefix for un-dotted default identifiers
extern const char *modifiedFiles;		// file to receive list of modified files
extern const char *extractCerts;		// location for extracting signing chain certificates
extern const char *sdkRoot;				// alternate root for looking up sub-components
extern SecCSFlags staticVerifyOptions;	// option flags to static verifications
extern SecCSFlags dynamicVerifyOptions;	// option flags to dynamic verifications
extern uint32_t digestAlgorithm;		// digest algorithm to be used when signing
extern CFDateRef signingTime;			// explicit signing time option
extern size_t signatureSize;			// override CMS signature size estimate
extern uint32_t cdFlags;				// CodeDirectory flags requested
extern const char *procAction;			// action-on-process(es) requested
extern Architecture architecture;		// specific binary architecture to process (from a universal file)
extern const char *bundleVersion;		// specific version string requested (from a versioned bundle)
extern bool noMachO;					// force non-MachO operation
extern bool dryrun;						// do not actually change anything
extern bool allArchitectures;			// process all architectures in a universal (aka fat) code file

enum {
    kPreserveIdentifier = 1 << 0,		// preserve signing identifier
    kPreserveRequirements = 1 << 1,		// preserve internal requirements (including DR)
    kPreserveEntitlements = 1 << 2,		// preserve entitlements
    kPreserveResourceRules = 1 << 3,	// preserve resource rules (and thus resources)
};
extern int preserveMetadata;			// keep metadata from previous signature (if any)


#endif //_H_CODESIGN
