/*
 * Copyright (c) 2012,2014 Apple Inc. All Rights Reserved.
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
 * tsaSupport.h -  ASN1 templates Time Stamping Authority requests and responses
 */

#ifndef libsecurity_smime_tsaSupport_priv_h
#define libsecurity_smime_tsaSupport_priv_h

#include <Security/SecCmsBase.h>
#include <CoreFoundation/CoreFoundation.h>

#include <Security/tsaTemplates.h>
#include <Security/SecAsn1Coder.h>

#if defined(__cplusplus)
extern "C" {
#endif

extern const CFStringRef kTSADebugContextKeyBadReq;         // CFURLRef
extern const CFStringRef kTSADebugContextKeyBadNonce;     // CFBooleanRef

OSStatus SecTSAResponseCopyDEREncoding(SecAsn1CoderRef coder, const CSSM_DATA *tsaResponse, SecAsn1TimeStampRespDER *respDER);
OSStatus decodeTimeStampToken(SecCmsSignerInfoRef signerinfo, CSSM_DATA_PTR inData, CSSM_DATA_PTR encDigest, uint64_t expectedNonce);
OSStatus decodeTimeStampTokenWithPolicy(SecCmsSignerInfoRef signerinfo, CFTypeRef timeStampPolicy, CSSM_DATA_PTR inData, CSSM_DATA_PTR encDigest, uint64_t expectedNonce);
OSStatus createTSAMessageImprint(SecCmsSignedDataRef signedData, CSSM_DATA_PTR encDigest, SecAsn1TSAMessageImprint *messageImprint);

#ifndef NDEBUG
int tsaWriteFileX(const char *fileName, const unsigned char *bytes, size_t numBytes);
#endif

char *cfStringToChar(CFStringRef inStr);
uint64_t tsaDER_ToInt(const CSSM_DATA *DER_Data);
void displayTSTInfo(SecAsn1TSATSTInfo *tstInfo);

#if defined(__cplusplus)
}
#endif

#endif  /* libsecurity_smime_tsaSupport_priv_h */

