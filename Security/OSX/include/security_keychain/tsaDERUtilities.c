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
 * tsaDERUtilities.c -  ASN1 templates Time Stamping Authority requests and responses.
 * see rfc3161.asn1 for ASN.1 and other comments
 */

#include <libDER/asn1Types.h>
#include <libDER/DER_Decode.h>
#include <AssertMacros.h>
#include <Security/cssmtype.h>
#include <stdlib.h>
#include "tsaDERUtilities.h"

#ifndef DER_MULTIBYTE_TAGS
#error We expect DER_MULTIBYTE_TAGS
#endif

/* PKIStatusInfo */
typedef struct {
    DERItem     status;         // INTEGER
	DERItem     statusString;      // UTF8_STRING | SEC_ASN1_OPTIONAL
    DERItem     failInfo;          // BIT_STRING | SEC_ASN1_OPTIONAL
} DERPKIStatusInfo;

/* xx */
typedef struct {
	DERItem     statusString;      // UTF8_STRING | SEC_ASN1_OPTIONAL
} DERPKIStatusStringInner;

/* TimeStampResp */
typedef struct
{
    DERItem status;             /* PKIStatusInfo */
    DERItem timeStampToken;     /* TimeStampToken */
} DERTimeStampResp;

/* TimeStampResp */
const DERItemSpec DERTimeStampRespItemSpecs[] = 
{
    { DER_OFFSET(DERTimeStampResp, status),
        ASN1_CONSTR_SEQUENCE, DER_DEC_NO_OPTS },
    { DER_OFFSET(DERTimeStampResp, timeStampToken),
        ASN1_CONSTR_SEQUENCE, DER_DEC_NO_OPTS | DER_DEC_OPTIONAL | DER_DEC_SAVE_DER}
};
const DERSize DERNumTimeStampRespItemSpecs = sizeof(DERTimeStampRespItemSpecs) / sizeof(DERItemSpec);

/*
    This code is here rather than in libsecurity_smime because
    libsecurity_smime doesn't know about libDER
*/

int DERDecodeTimeStampResponse(
	const CSSM_DATA *contents,
    CSSM_DATA *derStatus,
    CSSM_DATA *derTimeStampToken,
	size_t			*numUsedBytes)      /* RETURNED */
{
    DERReturn drtn = DR_ParamErr;
    DERDecodedInfo decodedPackage;

    if (contents)
    {
        DERItem derContents = {.data = contents->Data, .length = contents->Length };
        DERTimeStampResp derResponse = {{0,},{0,}};
        DERReturn rx;
        require_noerr(DERDecodeItem(&derContents, &decodedPackage), badResponse);

        rx = DERParseSequenceContent(&decodedPackage.content,
            DERNumTimeStampRespItemSpecs, DERTimeStampRespItemSpecs, 
            &derResponse, 0);
        if (rx != DR_Success)
            goto badResponse;
/*
        require_noerr(DERParseSequenceContent(&decodedPackage.content,
            DERNumTimeStampRespItemSpecs, DERTimeStampRespItemSpecs, 
            &derResponse, 0), badResponse);
*/
        if (derStatus && derResponse.status.data)
        {
            derStatus->Data = malloc(derResponse.status.length);
            derStatus->Length = derResponse.status.length;
            memcpy(derStatus->Data, derResponse.status.data, derStatus->Length);
        }
        if (derTimeStampToken && derResponse.timeStampToken.data)
        {
            derTimeStampToken->Data = malloc(derResponse.timeStampToken.length);
            derTimeStampToken->Length = derResponse.timeStampToken.length;
            memcpy(derTimeStampToken->Data, derResponse.timeStampToken.data, derTimeStampToken->Length);
        }
    }

    drtn = DR_Success;
    
badResponse:
    if (numUsedBytes)
        *numUsedBytes = decodedPackage.content.length +
            decodedPackage.content.data - contents->Data;

    return drtn;
}

