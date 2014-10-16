/*
 * Copyright (c) 2003,2005 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */

/*
 * pkcs12Utils.cpp - standalone copies of utility functions from libsecurity_pkcs12
 */

#include "pkcs12Utils.h"
#include <string.h>
#include <Security/oidsalg.h>
#include <security_asn1/nssUtils.h>

/* CSSM_DATA --> uint32. Returns true if OK. */
bool p12DataToInt(
	const CSSM_DATA &cdata,
	uint32 &u)
{
	if((cdata.Length == 0) || (cdata.Data == NULL)) {
		/* default/not present */
		u = 0;
		return true;
	}
	uint32 len = cdata.Length;
	if(len > sizeof(uint32)) {
		return false;
	}
	
	uint32 rtn = 0;
	uint8 *cp = cdata.Data;
	for(uint32 i=0; i<len; i++) {
		rtn = (rtn << 8) | *cp++;
	}
	u = rtn;
	return true;
}

/*
 * OIDS for P12 and PKCS5 v1.5 (PBES1) encrypt and decrypt map to the following
 * attributes.
 */
typedef struct {
	const CSSM_OID		*oid;
	CSSM_ALGORITHMS		keyAlg;		// e.g., CSSM_ALGID_DES
	CSSM_ALGORITHMS		encrAlg;	// e.g., CSSM_ALGID_3DES_3KEY_EDE
	CSSM_ALGORITHMS		pbeHashAlg;	// SHA1 or MD5
	uint32				keySizeInBits;
	uint32				blockSizeInBytes;	// for IV, optional
	CSSM_PADDING		padding;	// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	mode;		// CSSM_ALGMODE_CBCPadIV8, etc.
	PKCS_Which			pkcs;		// PW_PKCS12 (for this module) or PW_PKCS5_v1_5
} PKCSOidInfo;

static const PKCSOidInfo pkcsOidInfos[] = {
	/* PKCS12 first, the ones this module uses */
	{ 
		&CSSMOID_PKCS12_pbeWithSHAAnd128BitRC4,
		CSSM_ALGID_RC4,
		CSSM_ALGID_RC4,
		CSSM_ALGID_SHA1,
		128,
		0,					// RC4 is a stream cipher
		CSSM_PADDING_NONE,
		CSSM_ALGMODE_NONE,
		PW_PKCS12
	},
	{ 
		&CSSMOID_PKCS12_pbeWithSHAAnd40BitRC4,
		CSSM_ALGID_RC4,
		CSSM_ALGID_RC4,
		CSSM_ALGID_SHA1,
		40,
		0,					// RC4 is a stream cipher
		CSSM_PADDING_NONE,
		CSSM_ALGMODE_NONE,
		PW_PKCS12
	},
	{ 
		&CSSMOID_PKCS12_pbeWithSHAAnd3Key3DESCBC,
		CSSM_ALGID_3DES_3KEY,
		CSSM_ALGID_3DES_3KEY_EDE,
		CSSM_ALGID_SHA1,
		64 * 3,
		8,	
		CSSM_PADDING_PKCS7,
		CSSM_ALGMODE_CBCPadIV8,
		PW_PKCS12
	},
	{ 
		&CSSMOID_PKCS12_pbeWithSHAAnd2Key3DESCBC,
		CSSM_ALGID_3DES_2KEY,
		CSSM_ALGID_3DES_2KEY_EDE,
		CSSM_ALGID_SHA1,
		64 * 2,
		8,	
		CSSM_PADDING_PKCS7,
		CSSM_ALGMODE_CBCPadIV8,
		PW_PKCS12
	},
	{ 
		&CSSMOID_PKCS12_pbeWithSHAAnd128BitRC2CBC,
		CSSM_ALGID_RC2,
		CSSM_ALGID_RC2,
		CSSM_ALGID_SHA1,
		128,
		8,	
		CSSM_PADDING_PKCS7,
		CSSM_ALGMODE_CBCPadIV8,
		PW_PKCS12
	},
	{ 
		&CSSMOID_PKCS12_pbewithSHAAnd40BitRC2CBC,
		CSSM_ALGID_RC2,
		CSSM_ALGID_RC2,
		CSSM_ALGID_SHA1,
		40,
		8,	
		CSSM_PADDING_PKCS7,
		CSSM_ALGMODE_CBCPadIV8,
		PW_PKCS12
	},
	
	/* PKCS5 v1.5, used for SecImportExport module */
	{
		&CSSMOID_PKCS5_pbeWithMD2AndDES,
		CSSM_ALGID_DES,
		CSSM_ALGID_DES,
		CSSM_ALGID_MD2,
		64,
		8,
		CSSM_PADDING_PKCS7,
		CSSM_ALGMODE_CBCPadIV8,
		PW_PKCS5_v1_5
	},
	{
		&CSSMOID_PKCS5_pbeWithMD2AndRC2,
		CSSM_ALGID_RC2,
		CSSM_ALGID_RC2,
		CSSM_ALGID_MD2,
		64,
		8,
		CSSM_PADDING_PKCS7,
		CSSM_ALGMODE_CBCPadIV8,
		PW_PKCS5_v1_5
	},
	{
		&CSSMOID_PKCS5_pbeWithMD5AndDES,
		CSSM_ALGID_DES,
		CSSM_ALGID_DES,
		CSSM_ALGID_MD5,
		64,
		8,
		CSSM_PADDING_PKCS7,
		CSSM_ALGMODE_CBCPadIV8,
		PW_PKCS5_v1_5
	},
	{
		&CSSMOID_PKCS5_pbeWithMD5AndRC2,
		CSSM_ALGID_RC2,
		CSSM_ALGID_RC2,
		CSSM_ALGID_MD5,
		64,
		8,
		CSSM_PADDING_PKCS7,
		CSSM_ALGMODE_CBCPadIV8,
		PW_PKCS5_v1_5
	},
	{
		&CSSMOID_PKCS5_pbeWithSHA1AndDES,
		CSSM_ALGID_DES,
		CSSM_ALGID_DES,
		CSSM_ALGID_SHA1,
		64,
		8,
		CSSM_PADDING_PKCS7,
		CSSM_ALGMODE_CBCPadIV8,
		PW_PKCS5_v1_5
	},
	{
		&CSSMOID_PKCS5_pbeWithSHA1AndRC2,
		CSSM_ALGID_RC2,
		CSSM_ALGID_RC2,
		CSSM_ALGID_SHA1,
		64,
		8,
		CSSM_PADDING_PKCS7,
		CSSM_ALGMODE_CBCPadIV8,
		PW_PKCS5_v1_5
	},
	
	/* finally one for PKCS5 v2.0, which has its own means of 
	 * cooking up all the parameters */
	{
		&CSSMOID_PKCS5_PBES2,
		CSSM_ALGID_NONE,
		CSSM_ALGID_NONE,
		CSSM_ALGID_NONE,
		0, 0, 0, 0, 
		PW_PKCS5_v2
	}
};

#define NUM_PKCS_OID_INFOS (sizeof(pkcsOidInfos) / sizeof(pkcsOidInfos[1]))

/* map an OID to the components */
/* returns false if OID not found */

/* 
 * NOTE: as of March 8 2004 this is also used by the SecImportExport
 * module...not just PKCS12!
 */
bool pkcsOidToParams(
	const CSSM_OID 		*oid,
	CSSM_ALGORITHMS		&keyAlg,		// e.g., CSSM_ALGID_DES
	CSSM_ALGORITHMS		&encrAlg,		// e.g., CSSM_ALGID_3DES_3KEY_EDE
	CSSM_ALGORITHMS		&pbeHashAlg,	// SHA1 or MD5
	uint32				&keySizeInBits,
	uint32				&blockSizeInBytes,	// for IV, optional
	CSSM_PADDING		&padding,		// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	&mode,			// CSSM_ALGMODE_CBCPadIV8, etc.
	PKCS_Which			&pkcs)			// PW_PKCS5_v1_5 or PW_PKCS12
{
	const PKCSOidInfo *info = pkcsOidInfos;
	pkcs = PW_None;
	
	for(unsigned dex=0; dex<NUM_PKCS_OID_INFOS; dex++) {
		if(nssCompareCssmData(oid, info->oid)) {
			keyAlg 			 = info->keyAlg;
			encrAlg 		 = info->encrAlg;
			pbeHashAlg 		 = info->pbeHashAlg;
			keySizeInBits 	 = info->keySizeInBits;
			blockSizeInBytes = info->blockSizeInBytes;
			padding			 = info->padding;
			mode 			 = info->mode;
			pkcs			 = info->pkcs;
			return true;
		}
		info++;
	}
	return false;
}

/*
 * Enum to string mappper.
 * Maybe DEBUG only.
 */
/*
 * Each type of attribute has a name/value pair in a table of these:
 */
typedef struct {
	unsigned		value;
	const char 		*name;
} p12NameValuePair;

/* declare one entry in a table of p12NameValuePair */
#define NVP(attr)		{attr, #attr}

/* the NULL entry which terminates all p12NameValuePair tables */
#define NVP_END		{0, NULL}

static const p12NameValuePair p7CITypeNames[] = 
{
	NVP(CT_None),
	NVP(CT_Data),
	NVP(CT_SignedData),
	NVP(CT_EnvData),
	NVP(CT_SignedEnvData),
	NVP(CT_DigestData),
	NVP(CT_EncryptedData),
	NVP_END
};

static const p12NameValuePair p12BagTypeNames[] = 
{
	NVP(BT_None),
	NVP(BT_KeyBag),
	NVP(BT_ShroudedKeyBag),
	NVP(BT_CertBag),
	NVP(BT_CrlBag),
	NVP(BT_SecretBag),
	NVP(BT_SafeContentsBag),
	NVP_END
};

static const char *typeToStr(
	unsigned type,
	const p12NameValuePair *table)
{
	while(table->name) {
		if(table->value == type) {
			return table->name;
		}
		table++;
	}
	return "Unknown";
}

const char *p12BagTypeStr(
	NSS_P12_SB_Type type)
{
	return typeToStr(type, p12BagTypeNames);
}

const char *p7ContentInfoTypeStr(
	NSS_P7_CI_Type type)
{
	return typeToStr(type, p7CITypeNames);
}
