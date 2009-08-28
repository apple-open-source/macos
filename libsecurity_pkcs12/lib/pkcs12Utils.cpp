/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
 * pkcs12Utils.cpp
 */

#include "pkcs12Utils.h"
#include <string.h>
#include "pkcs7Templates.h"
#include "pkcs12Templates.h"
#include "pkcs12Crypto.h"
#include "pkcs12Debug.h"
#include <security_asn1/nssUtils.h>
#include <Security/secasn1t.h>
#include <security_utilities/devrandom.h>
#include <security_utilities/errors.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <Security/oidsattr.h>
#include <Security/oidsalg.h>
#include <Security/cssmapple.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

/* malloc a NULL-ed array of pointers of size num+1 */
void **p12NssNullArray(
	uint32 num,
	SecNssCoder &coder)
{
	unsigned len = (num + 1) * sizeof(void *);
	void **p = (void **)coder.malloc(len);
	memset(p, 0, len);
	return p;
}

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

/* uint32 --> CSSM_DATA */
void p12IntToData(
	uint32 num,
	CSSM_DATA &cdata,
	SecNssCoder &coder)
{
	uint32 len = 0;
	
	if(num < 0x100) {
		len = 1;
	}
	else if(num < 0x10000) {
		len = 2;
	}
	else if(num < 0x1000000) {
		len = 3;
	}
	else {
		len = 4;
	}
	coder.allocItem(cdata, len);
	uint8 *cp = &cdata.Data[len - 1];
	for(unsigned i=0; i<len; i++) {
		*cp-- = num & 0xff;
		num >>= 8;
	}
}

/* CFDataRef <--> CSSM_DATA */
CFDataRef p12CssmDataToCf(
	const CSSM_DATA &c)
{
	return CFDataCreate(NULL, c.Data, c.Length);
}

void p12CfDataToCssm(
	CFDataRef cf,
	CSSM_DATA &c,
	SecNssCoder &coder)
{
	coder.allocCopyItem(CFDataGetBytePtr(cf),
		CFDataGetLength(cf), c);
}

/*
 * Attempt to convert a CFStringRef, which represents a SafeBag's
 * FriendlyName, to a UTF8-encoded CSSM_DATA. The CSSM_DATA and its
 * referent are allocated in the specified SecNssCoder's memory.
 * No guarantee that this conversion works. If it doesn't we return 
 * NULL and caller must be prepared to deal with that. 
 */
CSSM_DATA_PTR p12StringToUtf8(
	CFStringRef cfStr,
	SecNssCoder &coder)
{
	if(cfStr == NULL) {
		return NULL;
	}
	CFIndex strLen = CFStringGetLength(cfStr);
	if(strLen == 0) {
		return NULL;
	}
	CSSM_DATA_PTR rtn = coder.mallocn<CSSM_DATA>();
	coder.allocItem(*rtn, strLen + 1);
	if(!CFStringGetCString(cfStr, (char *)rtn->Data,strLen + 1,
			kCFStringEncodingUTF8)) {
		/* not convertible from native Unicode to UTF8 */
		return NULL;
	}
	return rtn;
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
 * Verify MAC on an existing PFX.  
 */
CSSM_RETURN p12VerifyMac(
	const NSS_P12_DecodedPFX 	&pfx,
	CSSM_CSP_HANDLE				cspHand,
	const CSSM_DATA				*pwd,	// unicode, double null terminated
	const CSSM_KEY				*passKey,
	SecNssCoder					&coder)	// for temp mallocs
{
	if(pfx.macData == NULL) {
		return CSSMERR_CSP_INVALID_SIGNATURE;
	}
	NSS_P12_MacData &macData = *pfx.macData;
	NSS_P7_DigestInfo &digestInfo  = macData.mac;
	CSSM_OID &algOid = digestInfo.digestAlgorithm.algorithm;
	CSSM_ALGORITHMS macAlg;
	if(!cssmOidToAlg(&algOid, &macAlg)) {
		return CSSMERR_CSP_INVALID_ALGORITHM;
	}
	uint32 iterCount = 0;
	CSSM_DATA &citer = macData.iterations;
	if(!p12DataToInt(citer, iterCount)) {
		return CSSMERR_CSP_INVALID_ATTR_ROUNDS;
	}
	if(iterCount == 0) {
		/* optional, default 1 */
		iterCount = 1;
	}

	/*
	 * In classic fashion, the PKCS12 spec now says:
	 *
	 *      When password integrity mode is used to secure a PFX PDU, 
	 *      an SHA-1 HMAC is computed on the BER-encoding of the contents 
	 *      of the content field of the authSafe field in the PFX PDU.
	 *
	 * So here we go.
	 */
	CSSM_DATA genMac;
	CSSM_RETURN crtn = p12GenMac(cspHand, *pfx.authSafe.content.data, 
		macAlg, iterCount, macData.macSalt, pwd, passKey, coder, genMac);
	if(crtn) {
		return crtn;
	}
	if(nssCompareCssmData(&genMac, &digestInfo.digest)) {
		return CSSM_OK;
	}
	else {
		return CSSMERR_CSP_VERIFY_FAILED;
	}
}

/* we generate 8 random bytes of salt */
#define P12_SALT_LEN		8

void p12GenSalt(
	CSSM_DATA &salt,
	SecNssCoder &coder)
{
	DevRandomGenerator rng;
	coder.allocItem(salt, P12_SALT_LEN);
	rng.random(salt.Data, P12_SALT_LEN);
}

/* 
 * Generate random label string to allow associating an imported private
 * key with a cert.
 */
void p12GenLabel(
	CSSM_DATA &label,
	SecNssCoder &coder)
{
	/* first a random uint32 */
	uint8 d[4];
	DevRandomGenerator rng;
	rng.random(d, 4);
	CSSM_DATA cd = {4, d};
	uint32 i;
	p12DataToInt(cd, i);
	
	/* sprintf that into a real string */
	coder.allocItem(label, 9);
	memset(label.Data, 0, 9);
	sprintf((char *)label.Data, "%08X", (unsigned)i);
}

/* NULL algorithm parameters */

static const uint8 nullAlg[2] = {SEC_ASN1_NULL, 0};

void p12NullAlgParams(
	CSSM_X509_ALGORITHM_IDENTIFIER &algId)
{
	CSSM_DATA &p = algId.parameters;
	p.Data = (uint8 *)nullAlg;
	p.Length = 2;
}

/*
 * Free memory via specified plugin's app-level allocator
 */
void freeCssmMemory(
	CSSM_HANDLE	hand,
	void 			*p)
{
	CSSM_API_MEMORY_FUNCS memFuncs;
	CSSM_RETURN crtn = CSSM_GetAPIMemoryFunctions(hand, &memFuncs);
	if(crtn) {
		p12LogCssmError("CSSM_GetAPIMemoryFunctions", crtn);
		/* oh well, leak and continue */
		return;
	}
	memFuncs.free_func(p, memFuncs.AllocRef);
}

/*
 * Find private key by label, modify its Label attr to be the
 * hash of the associated public key. 
 * Also optionally re-sets the key's PrintName attribute; used to reset
 * this attr from the random label we create when first unwrap it 
 * to the friendly name we find later after parsing attributes.
 * Detection of a duplicate key when updating the key's attributes
 * results in a lookup of the original key and returning it in
 * foundKey.
 */
CSSM_RETURN p12SetPubKeyHash(
	CSSM_CSP_HANDLE 	cspHand,		// where the key lives
	CSSM_DL_DB_HANDLE 	dlDbHand,		// ditto
	CSSM_DATA			&keyLabel,		// for DB lookup
	CSSM_DATA_PTR		newPrintName,	// optional
	SecNssCoder			&coder,			// for mallocing newLabel
	CSSM_DATA			&newLabel,		// RETURNED with label as hash
	CSSM_KEY_PTR		&foundKey)		// RETURNED
{
	CSSM_QUERY						query;
	CSSM_SELECTION_PREDICATE		predicate;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_RETURN						crtn;
	CSSM_HANDLE						resultHand = 0;
	CSSM_DATA						keyData = {0, NULL};
	CSSM_CC_HANDLE					ccHand = 0;
	CSSM_KEY_PTR					privKey = NULL;
	CSSM_DATA_PTR					keyDigest = NULL;
	
	assert(cspHand != 0);
	query.RecordType = CSSM_DL_DB_RECORD_PRIVATE_KEY;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 1;
	predicate.DbOperator = CSSM_DB_EQUAL;
	
	predicate.Attribute.Info.AttributeNameFormat = 
		CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	predicate.Attribute.Info.Label.AttributeName = 
		(char*) P12_KEY_ATTR_LABEL_AND_HASH;
	predicate.Attribute.Info.AttributeFormat = 
		CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	/* hope this cast is OK */
	predicate.Attribute.Value = &keyLabel;
	query.SelectionPredicate = &predicate;
	
	query.QueryLimits.TimeLimit = 0;	// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;	// FIXME - meaningful?
	query.QueryFlags = CSSM_QUERY_RETURN_DATA;	

	/* build Record attribute with one or two attrs */
	CSSM_DB_RECORD_ATTRIBUTE_DATA recordAttrs;
	CSSM_DB_ATTRIBUTE_DATA attr[2];
	attr[0].Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr[0].Info.Label.AttributeName = (char*) P12_KEY_ATTR_LABEL_AND_HASH;
	attr[0].Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	if(newPrintName) {
		attr[1].Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
		attr[1].Info.Label.AttributeName = (char*) P12_KEY_ATTR_PRINT_NAME;
		attr[1].Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	}
	recordAttrs.DataRecordType = CSSM_DL_DB_RECORD_PRIVATE_KEY;
	recordAttrs.NumberOfAttributes = newPrintName ? 2 : 1;
	recordAttrs.AttributeData = attr;
	
	crtn = CSSM_DL_DataGetFirst(dlDbHand,
		&query,
		&resultHand,
		&recordAttrs,
		&keyData,			// theData
		&record);
	/* abort only on success */
	if(crtn != CSSM_OK) {
		p12LogCssmError("CSSM_DL_DataGetFirst", crtn);
		p12ErrorLog("***p12SetPubKeyHash: can't find private key\n");
		return crtn;
	}
	/* subsequent errors to errOut: */
	if(keyData.Data == NULL) {
		p12ErrorLog("***p12SetPubKeyHash: private key lookup failure\n");
		crtn = CSSMERR_CSSM_INTERNAL_ERROR;
		goto errOut;
	}
	privKey = (CSSM_KEY_PTR)keyData.Data;
	
	/* public key hash via passthrough - works on any key, any CSP/CSPDL.... */
	/*
	 * Warning! This relies on the current default ACL meaning "allow this
	 * current app to access this private key" since we created the key. 
	 */
	crtn = CSSM_CSP_CreatePassThroughContext(cspHand, privKey, &ccHand);
	if(crtn) {
		p12LogCssmError("CSSM_CSP_CreatePassThroughContext", crtn);
		goto errOut;
	}
	crtn = CSSM_CSP_PassThrough(ccHand,
		CSSM_APPLECSP_KEYDIGEST,
		NULL,
		(void **)&keyDigest);
	if(crtn) {
		p12LogCssmError("CSSM_CSP_PassThrough", crtn);
		goto errOut;
	}

	/* 
	 * Replace Label attr data with hash.
	 * NOTE: the module which allocated this attribute data - a DL -
	 * was loaded and attached by out client layer, not by us. Thus 
	 * we can't use the memory allocator functions *we* used when 
	 * attaching to the CSP - we have to use the ones
	 * which the client registered with the DL.
	 */
	freeCssmMemory(dlDbHand.DLHandle, attr[0].Value->Data);
	freeCssmMemory(dlDbHand.DLHandle, attr[0].Value);
	if(newPrintName) {
		freeCssmMemory(dlDbHand.DLHandle, attr[1].Value->Data);
		freeCssmMemory(dlDbHand.DLHandle, attr[1].Value);
	}
	/* modify key attributes */
	attr[0].Value = keyDigest;
	if(newPrintName) {
		attr[1].Value = newPrintName;
	}
	crtn = CSSM_DL_DataModify(dlDbHand,
			CSSM_DL_DB_RECORD_PRIVATE_KEY,
			record,
			&recordAttrs,
            NULL,				// DataToBeModified
			CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);
	switch(crtn) {
		case CSSM_OK:
			/* give caller the key's new label */
			coder.allocCopyItem(*keyDigest, newLabel);
			break;
		default:
			p12LogCssmError("CSSM_DL_DataModify", crtn);
			break;
		case CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA:
		{
			/* 
			 * Special case: dup private key. The label we just tried to modify is 
			 * the public key hash so we can be confident that this really is a dup. 
			 * Delete it, look up the original, and return the original to caller. 
			 */ 
			CSSM_RETURN drtn = CSSM_DL_DataDelete(dlDbHand, record);
			if(drtn) {
				p12LogCssmError("CSSM_DL_DataDelete on dup key", drtn);
				crtn = drtn;
				break;
			}

			/* Free items created in last search */
			CSSM_DL_DataAbortQuery(dlDbHand, resultHand);
			resultHand = 0;
			CSSM_DL_FreeUniqueRecord(dlDbHand, record);
			record = NULL;
			
			/* lookup by label as public key hash this time */
			predicate.Attribute.Value = keyDigest;
			drtn = CSSM_DL_DataGetFirst(dlDbHand,
				&query,
				&resultHand,
				NULL,				// no attrs this time
				&keyData,		
				&record);
			if(drtn) {
				p12LogCssmError("CSSM_DL_DataGetFirst on original key", crtn);
				crtn = drtn;
				break;
			}
			foundKey = (CSSM_KEY_PTR)keyData.Data;
			/* give caller the key's actual label */
			coder.allocCopyItem(*keyDigest, newLabel);
			break;
		}
	}
	
errOut:
	/* free resources */
	if(resultHand) {
		CSSM_DL_DataAbortQuery(dlDbHand, resultHand);
	}
	if(record) {
		CSSM_DL_FreeUniqueRecord(dlDbHand, record);
	}
	if(ccHand) {
		CSSM_DeleteContext(ccHand);
	}
	if(privKey) {
		/* key created by the CSPDL */
		CSSM_FreeKey(cspHand, NULL, privKey, CSSM_FALSE);
		freeCssmMemory(dlDbHand.DLHandle, privKey);
	}
	if(keyDigest)  {
		/* mallocd by someone else's CSP */
		freeCssmMemory(cspHand, keyDigest->Data);
		freeCssmMemory(cspHand, keyDigest);
	}
	return crtn;
}

/*
 * Given a context specified via a CSSM_CC_HANDLE, add a new
 * CSSM_CONTEXT_ATTRIBUTE to the context as specified by AttributeType,
 * AttributeLength, and an untyped pointer.
 */
CSSM_RETURN p12AddContextAttribute(CSSM_CC_HANDLE CCHandle,
	uint32 AttributeType,
	uint32 AttributeLength,
	const void *AttributePtr)
{
	CSSM_CONTEXT_ATTRIBUTE		newAttr;	
	CSSM_RETURN					crtn;
	
	newAttr.AttributeType     = AttributeType;
	newAttr.AttributeLength   = AttributeLength;
	newAttr.Attribute.Data    = (CSSM_DATA_PTR)AttributePtr;
	crtn = CSSM_UpdateContextAttributes(CCHandle, 1, &newAttr);
	if(crtn) {
		p12LogCssmError("CSSM_UpdateContextAttributes", crtn);
	}
	return crtn;
}

/*
 * Find private key by specified label, delete it.
 */
CSSM_RETURN p12DeleteKey(
	CSSM_DL_DB_HANDLE dlDbHand, 
	const CSSM_DATA	&keyLabel)
{
	CSSM_QUERY						query;
	CSSM_SELECTION_PREDICATE		predicate;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_RETURN						crtn;
	CSSM_HANDLE						resultHand = 0;
	
	query.RecordType = CSSM_DL_DB_RECORD_PRIVATE_KEY;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 1;
	predicate.DbOperator = CSSM_DB_EQUAL;
	
	predicate.Attribute.Info.AttributeNameFormat = 
		CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	predicate.Attribute.Info.Label.AttributeName = 
		(char*) P12_KEY_ATTR_LABEL_AND_HASH;
	predicate.Attribute.Info.AttributeFormat = 
		CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	predicate.Attribute.Value = const_cast<CSSM_DATA_PTR>(&keyLabel);
	
	query.SelectionPredicate = &predicate;
	query.QueryLimits.TimeLimit = 0;
	query.QueryLimits.SizeLimit = 1;
	query.QueryFlags = 0;

	crtn = CSSM_DL_DataGetFirst(dlDbHand,
		&query,
		&resultHand,
		NULL,			// attrs - don't need 'em
		NULL, 			// theData - don't need it
		&record);
	/* abort only on success */
	if(crtn) {
		p12LogCssmError("CSSM_DL_DataGetFirst", crtn);
		p12ErrorLog("***p12DeleteKey: can't find private key\n");
		return crtn;
	}

	crtn = CSSM_DL_DataDelete(dlDbHand, record);
	if(crtn) {
		p12LogCssmError("CSSM_DL_DataDelete", crtn);
		p12ErrorLog("***p12DeleteKey: can't delete private key\n");
	}
	
	CSSM_DL_DataAbortQuery(dlDbHand, resultHand);
	CSSM_DL_FreeUniqueRecord(dlDbHand, record);
	return crtn;
}

/* convert App passphrase to array of chars used in P12 PBE */
void p12ImportPassPhrase(
	CFStringRef		inPhrase,
	SecNssCoder		&coder,
	CSSM_DATA		&outPhrase)
{
	CFDataRef cfData = CFStringCreateExternalRepresentation(NULL,
		inPhrase, kCFStringEncodingUTF8, 0);
	if(cfData == NULL) {
		p12ErrorLog("***p12ImportPassPhrase: can't convert passphrase to UTF8\n");
		MacOSError::throwMe(paramErr);
	}
	unsigned keyLen = CFDataGetLength(cfData);
	coder.allocItem(outPhrase, keyLen);
	memmove(outPhrase.Data, CFDataGetBytePtr(cfData), keyLen);
	CFRelease(cfData);
}
