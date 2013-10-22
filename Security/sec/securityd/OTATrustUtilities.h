/*
 * Copyright (c) 2003-2004,2006-2010 Apple Inc. All Rights Reserved.
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
 *
 * OTATrustUtilities.h
 */

#ifndef _OTATRUSTUTILITIES_H_
#define _OTATRUSTUTILITIES_H_  1

#include <CoreFoundation/CoreFoundation.h>
#include <sys/types.h>
#include <stdio.h>

__BEGIN_DECLS

// Opawue type that holds the data for a specific version of the OTA PKI assets
typedef struct _OpaqueSecOTAPKI *SecOTAPKIRef;

// Get a reference to the current OTA PKI asset data
// Caller is responsible for releasing the returned SecOTAPKIRef
CF_EXPORT
SecOTAPKIRef SecOTAPKICopyCurrentOTAPKIRef(void);

// Accessor to retrieve a copy of the current black listed key.  
// Caller is responsible for releasing the returned CFSetRef
CF_EXPORT
CFSetRef SecOTAPKICopyBlackListSet(SecOTAPKIRef otapkiRef);

// Accessor to retrieve a copy of the current gray listed key.  
// Caller is responsible for releasing the returned CFSetRef
CF_EXPORT
CFSetRef SecOTAPKICopyGrayList(SecOTAPKIRef otapkiRef);

// Accessor to retrieve the array of Escrow certificates
// Caller is responsible for releasing the returned CFArrayRef
CF_EXPORT
CFArrayRef SecOTAPKICopyEscrowCertificates(SecOTAPKIRef otapkiRef);

// Accessor to retrieve the dictionary of EV Policy OIDs to Anchor digest
// Caller is responsible for releasing the returned CFDictionaryRef
CF_EXPORT
CFDictionaryRef SecOTAPKICopyEVPolicyToAnchorMapping(SecOTAPKIRef otapkiRef);

// Accessor to retrieve the dictionary of anchor digest to file offest
// Caller is responsible for releasing the returned CFDictionaryRef
CF_EXPORT
CFDictionaryRef SecOTAPKICopyAnchorLookupTable(SecOTAPKIRef otapkiRef);

// Accessor to retrieve the ponter to the top of the anchor certs file
// Caller should NOT free the returned pointer.  The caller should hold
// a reference to the SecOTAPKIRef object until finishing processing with
// the returned const char*
CF_EXPORT
const char*	SecOTAPKIGetAnchorTable(SecOTAPKIRef otapkiRef);

// Accessor to retrieve the current OTA PKI asset version number
CF_EXPORT
int SecOTAPKIGetAssetVersion(SecOTAPKIRef otapkiRef);

// Signal that a new OTA PKI asset version is available. This call
// will update the current SecOTAPKIRef to now reference the latest
// asset data
CF_EXPORT
void SecOTAPKIRefreshData(void);

// SPI to return the array of currently trusted Escrow certificates
CF_EXPORT
CFArrayRef SecOTAPKICopyCurrentEscrowCertificates(CFErrorRef* error);

// SPI to return the current OTA PKI asset version
CF_EXPORT
int SecOTAPKIGetCurrentAssetVersion(CFErrorRef* error); 

// SPI to signal securityd to get a new set of trust data
CF_EXPORT
int SecOTAPKISignalNewAsset(CFErrorRef* error);

__END_DECLS

#endif /* _OTATRUSTUTILITIES_H_ */
