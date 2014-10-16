/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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
 * tsaTemplates.c -  ASN1 templates Time Stamping Authority requests and responses
 */

#include <security_asn1/keyTemplates.h>     /* for kSecAsn1AlgorithmIDTemplate */
#include <security_asn1/SecAsn1Templates.h>
#include <stddef.h>
#include <assert.h>

#include "tsaTemplates.h"
#include "cmslocal.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"

// *** from CMSEncoder.cpp

typedef struct {
    CSSM_OID	contentType;
    CSSM_DATA	content;
} SimpleContentInfo;

// SecCmsContentInfoTemplate
static const SecAsn1Template cmsSimpleContentInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SimpleContentInfo) },
    { SEC_ASN1_OBJECT_ID, offsetof(SimpleContentInfo, contentType) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(SimpleContentInfo, content),
	  kSecAsn1AnyTemplate },
    { 0, }
};

#pragma mark ----- tsa -----

/*
Accuracy ::= SEQUENCE {
                seconds        INTEGER           OPTIONAL,
                millis     [0] INTEGER  (1..999) OPTIONAL,
                micros     [1] INTEGER  (1..999) OPTIONAL  }
*/

const SecAsn1Template kSecAsn1SignedIntegerTemplate[] = {
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1UnsignedIntegerTemplate[] = {
    { SEC_ASN1_INTEGER, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1TSAAccuracyTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1TSAAccuracy) },
    { SEC_ASN1_INTEGER,
        offsetof(SecAsn1TSAAccuracy, seconds) },
    { SEC_ASN1_CONTEXT_SPECIFIC | 0 | SEC_ASN1_OPTIONAL,
        offsetof(SecAsn1TSAAccuracy, millis), kSecAsn1UnsignedIntegerTemplate },
    { SEC_ASN1_CONTEXT_SPECIFIC | 1 | SEC_ASN1_OPTIONAL,
        offsetof(SecAsn1TSAAccuracy, micros), kSecAsn1UnsignedIntegerTemplate },
    { 0 }
};

/*
MessageImprint ::= SEQUENCE  {
     hashAlgorithm                AlgorithmIdentifier,
     hashedMessage                OCTET STRING  }
*/

const SecAsn1Template kSecAsn1TSAMessageImprintTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1TSAMessageImprint) },
    { SEC_ASN1_INLINE, offsetof(SecAsn1TSAMessageImprint,hashAlgorithm),
        kSecAsn1AlgorithmIDTemplate },
    { SEC_ASN1_OCTET_STRING,
        offsetof(SecAsn1TSAMessageImprint,hashedMessage) },
    { 0 }
};

/*
    TimeStampReq ::= SEQUENCE  {
       version                  INTEGER  { v1(1) },
       messageImprint           MessageImprint,
         --a hash algorithm OID and the hash value of the data to be
         --time-stamped
       reqPolicy                TSAPolicyId                OPTIONAL,
       nonce                    INTEGER                    OPTIONAL,
       certReq                  BOOLEAN                    DEFAULT FALSE,
       extensions               [0] IMPLICIT Extensions    OPTIONAL  }

    MessageImprint ::= SEQUENCE  {
         hashAlgorithm                AlgorithmIdentifier,
         hashedMessage                OCTET STRING  }

    TSAPolicyId ::= OBJECT IDENTIFIER
*/

const SecAsn1Template kSecAsn1TSATimeStampReqTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1TSATimeStampReq) },
    { SEC_ASN1_INTEGER,
        offsetof(SecAsn1TSATimeStampReq, version) },
    { SEC_ASN1_INLINE, offsetof(SecAsn1TSATimeStampReq,messageImprint),
        kSecAsn1TSAMessageImprintTemplate },
    { SEC_ASN1_OBJECT_ID | SEC_ASN1_OPTIONAL,
        offsetof(SecAsn1TSATimeStampReq,reqPolicy) },
    { SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL,
        offsetof(SecAsn1TSATimeStampReq, nonce) },
    { SEC_ASN1_BOOLEAN | SEC_ASN1_OPTIONAL,
        offsetof(SecAsn1TSATimeStampReq, certReq) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
        offsetof(SecAsn1TSATimeStampReq, extensions),
        kSecAsn1SequenceOfCertExtensionTemplate },
    { 0 }
};

/*
    PKIFreeText ::= SEQUENCE {
        SIZE (1..MAX) OF UTF8String
        -- text encoded as UTF-8 String (note: each UTF8String SHOULD
        -- include an RFC 1766 language tag to indicate the language -- of the contained text)
    }

    See e.g. kSecAsn1SequenceOfUTF8StringTemplate
*/

const SecAsn1Template kSecAsn1TSAPKIStatusInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1TSAPKIStatusInfo) },
    { SEC_ASN1_INTEGER,
        offsetof(SecAsn1TSAPKIStatusInfo, status) },
    {  SEC_ASN1_CONSTRUCTED | SEC_ASN1_SEQUENCE | SEC_ASN1_OPTIONAL,
        offsetof(SecAsn1TSAPKIStatusInfo, statusString) },
    { SEC_ASN1_BIT_STRING | SEC_ASN1_OPTIONAL,
        offsetof(SecAsn1TSAPKIStatusInfo,failInfo) },
    { 0 }
};

const SecAsn1Template kSecAsn1TSAPKIStatusInfoTemplateRFC3161[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1TSAPKIStatusInfo) },
    { SEC_ASN1_INTEGER,
        offsetof(SecAsn1TSAPKIStatusInfo, status) },
    { SEC_ASN1_UTF8_STRING | SEC_ASN1_OPTIONAL,
        offsetof(SecAsn1TSAPKIStatusInfo, statusString) },
    { SEC_ASN1_BIT_STRING | SEC_ASN1_OPTIONAL,
        offsetof(SecAsn1TSAPKIStatusInfo,failInfo) },
    { 0 }
};

const SecAsn1Template kSecAsn1TSATimeStampRespTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1TimeStampResp) },
    { SEC_ASN1_INLINE, offsetof(SecAsn1TimeStampResp,status),
        kSecAsn1TSAPKIStatusInfoTemplate },
    { SEC_ASN1_INLINE | SEC_ASN1_OPTIONAL, offsetof(SecAsn1TimeStampResp,timeStampToken),
        SecCmsContentInfoTemplate },
    { 0 }
};

// Decode the status but not the TimeStampToken
const SecAsn1Template kSecAsn1TSATimeStampRespTemplateDER[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1TimeStampRespDER) },
    { SEC_ASN1_INLINE, offsetof(SecAsn1TimeStampRespDER,status),
        kSecAsn1TSAPKIStatusInfoTemplate },
    { SEC_ASN1_ANY | SEC_ASN1_OPTIONAL ,//| SEC_ASN1_SAVE,
	  offsetof(SecAsn1TimeStampRespDER, timeStampTokenDER), kSecAsn1AnyTemplate  },
    { 0 }
};

/*
    RFC 3161               Time-Stamp Protocol (TSP)             August 2001

    TimeStampToken ::= ContentInfo

     -- contentType is id-signedData as defined in [CMS]
     -- content is SignedData as defined in([CMS])
     -- eContentType within SignedData is id-ct-TSTInfo
     -- eContent within SignedData is TSTInfo

    TSTInfo ::= SEQUENCE  {
        version                      INTEGER  { v1(1) },
        policy                       TSAPolicyId,
        messageImprint               MessageImprint,
          -- MUST have the same value as the similar field in
          -- TimeStampReq
        serialNumber                 INTEGER,
         -- Time-Stamping users MUST be ready to accommodate integers
         -- up to 160 bits.
        genTime                      GeneralizedTime,
        accuracy                     Accuracy                 OPTIONAL,
        ordering                     BOOLEAN             DEFAULT FALSE,
        nonce                        INTEGER                  OPTIONAL,
          -- MUST be present if the similar field was present
          -- in TimeStampReq.  In that case it MUST have the same value.
        tsa                          [0] GeneralName          OPTIONAL,
        extensions                   [1] IMPLICIT Extensions  OPTIONAL   }

    Accuracy ::= SEQUENCE {
                    seconds        INTEGER           OPTIONAL,
                    millis     [0] INTEGER  (1..999) OPTIONAL,
                    micros     [1] INTEGER  (1..999) OPTIONAL  }
*/

const SecAsn1Template kSecAsn1TSATSTInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1TSATSTInfo) },
    { SEC_ASN1_INTEGER,
        offsetof(SecAsn1TSATSTInfo, version) },
    { SEC_ASN1_OBJECT_ID,
        offsetof(SecAsn1TSATSTInfo,reqPolicy) },
   { SEC_ASN1_INLINE, offsetof(SecAsn1TSATSTInfo,messageImprint),
        kSecAsn1TSAMessageImprintTemplate },
    { SEC_ASN1_INTEGER,
        offsetof(SecAsn1TSATSTInfo, serialNumber) },
    { SEC_ASN1_GENERALIZED_TIME | SEC_ASN1_MAY_STREAM,
        offsetof(SecAsn1TSATSTInfo,genTime) },
    { SEC_ASN1_INLINE | SEC_ASN1_OPTIONAL,
	  offsetof(SecAsn1TSATSTInfo,accuracy),
	  kSecAsn1TSAAccuracyTemplate },
    { SEC_ASN1_BOOLEAN | SEC_ASN1_OPTIONAL,
        offsetof(SecAsn1TSATSTInfo, ordering) },
    { SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL,
        offsetof(SecAsn1TSATSTInfo, nonce) },
    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0 | SEC_ASN1_OPTIONAL,
        offsetof(SecAsn1TSATSTInfo, tsa),
        kSecAsn1GenNameOtherNameTemplate},

    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1 | SEC_ASN1_OPTIONAL,
        offsetof(SecAsn1TSATSTInfo, extensions),
        kSecAsn1CertExtensionTemplate },
    { 0 }
};

#pragma clang diagnostic pop
