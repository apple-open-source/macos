/*
 * Copyright (c) 2000-2004,2011,2014 Apple Inc. All Rights Reserved.
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
 
/*
 * SecPkcs8Templates.cpp - ASN1 templates for private keys in PKCS8 format.  
 */

#include "SecPkcs8Templates.h"
#include <Security/keyTemplates.h>
#include <Security/secasn1t.h>
#include <security_asn1/prtypes.h>  
#include <stddef.h>

const SecAsn1Template impExpPKCS5_PBE_ParametersTemplate[] = {
	{ SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(impExpPKCS5_PBE_Parameters) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(impExpPKCS5_PBE_Parameters,salt) },
	/* iterations is unsigned - right? */
	{ SEC_ASN1_INTEGER,
	  offsetof(impExpPKCS5_PBE_Parameters,iterations) },
	{ 0 }
};

const SecAsn1Template impExpPKCS5_PBKDF2_ParamsTemplate[] = {
	{ SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(impExpPKCS5_PBKDF2_Params) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(impExpPKCS5_PBKDF2_Params,salt) },
	/* iterations is unsigned - right? */
	{ SEC_ASN1_INTEGER,
	  offsetof(impExpPKCS5_PBKDF2_Params,iterationCount) },
	{ SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL,
	  offsetof(impExpPKCS5_PBKDF2_Params,keyLengthInBytes) },
	{ SEC_ASN1_OBJECT_ID | SEC_ASN1_OPTIONAL,
	  offsetof(impExpPKCS5_PBKDF2_Params,prf) },
	{ 0 }
};

const SecAsn1Template impExpPKCS5_RC2ParamsTemplate[] = {
	{ SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(impExpPKCS5_RC2Params) },
	{ SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL,
	  offsetof(impExpPKCS5_RC2Params,version) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(impExpPKCS5_RC2Params,iv) },
	{ 0 }
};

const SecAsn1Template impExpPKCS5_RC5ParamsTemplate[] = {
	{ SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(impExpPKCS5_RC5Params) },
	{ SEC_ASN1_INTEGER,
	  offsetof(impExpPKCS5_RC5Params,version) },
	{ SEC_ASN1_INTEGER,
	  offsetof(impExpPKCS5_RC5Params,rounds) },
	{ SEC_ASN1_INTEGER,
	  offsetof(impExpPKCS5_RC5Params,blockSizeInBits) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(impExpPKCS5_RC5Params,iv) },
	{ 0 }
};

const SecAsn1Template impExpPKCS5_PBES2_ParamsTemplate[] = {
	{ SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(impExpPKCS5_PBES2_Params) },
    { SEC_ASN1_INLINE,
	  offsetof(impExpPKCS5_PBES2_Params,keyDerivationFunc),
	  kSecAsn1AlgorithmIDTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(impExpPKCS5_PBES2_Params,encryptionScheme),
	  kSecAsn1AlgorithmIDTemplate },
	{ 0 }
};
