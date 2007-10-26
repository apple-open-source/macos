/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
 * SecNetscapeTemplates.cpp - Structs and templates for DER encoding and 
 *						 decoding of Netscape-style certificate requests 
 *						 and certificate sequences.
 */
 
#include "SecNetscapeTemplates.h"
#include <Security/SecAsn1Templates.h>
#include <Security/secasn1t.h>
#include <stddef.h>

const SecAsn1Template NetscapeCertSequenceTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NetscapeCertSequence) },
	{ SEC_ASN1_OBJECT_ID,
	  offsetof(NetscapeCertSequence, contentType), 0},
   { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | 
		SEC_ASN1_CONTEXT_SPECIFIC | 0 , 
	    offsetof(NetscapeCertSequence, certs),
	    kSecAsn1SequenceOfAnyTemplate },
	{ 0 }
};
const SecAsn1Template PublicKeyAndChallengeTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(PublicKeyAndChallenge) },
    { SEC_ASN1_INLINE,
	  offsetof(PublicKeyAndChallenge, spki),
	  kSecAsn1SubjectPublicKeyInfoTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(PublicKeyAndChallenge, challenge),
	  kSecAsn1IA5StringTemplate },
	{ 0 }
};

extern const SecAsn1Template SignedPublicKeyAndChallengeTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(SignedPublicKeyAndChallenge) },
    { SEC_ASN1_INLINE,
	  offsetof(SignedPublicKeyAndChallenge, pubKeyAndChallenge),
	  PublicKeyAndChallengeTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(SignedPublicKeyAndChallenge, algId),
	  kSecAsn1AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING,
	  offsetof(SignedPublicKeyAndChallenge, signature) },
	{ 0 }
};

