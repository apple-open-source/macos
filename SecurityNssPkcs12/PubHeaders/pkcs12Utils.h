/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
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
 * pkcs12Utils.h
 */
 
#ifndef	_PKCS12_UTILS_H_
#define _PKCS12_UTILS_H_

#include <Security/cssmtype.h>
#include <SecurityNssAsn1/SecNssCoder.h>
#include <SecurityNssPkcs12/pkcs7Templates.h>
#include <SecurityNssPkcs12/pkcs12Templates.h>
#include <Security/cssmerr.h>
#include <Security/utilities.h>
#include <Security/Security.h>

#ifdef __cplusplus
extern "C" {
#endif

/* malloc a NULL-ed array of pointers of size num+1 */
void **p12NssNullArray(
	uint32 num,
	SecNssCoder &coder);

/* CSSM_DATA --> uint32. Returns true if OK. */
bool p12DataToInt(
	const CSSM_DATA &cdata,
	uint32 &u);

/* uint32 --> CSSM_DATA */
void p12IntToData(
	uint32 num,
	CSSM_DATA &cdata,
	SecNssCoder &coder);

/* CFDataRef <--> CSSM_DATA */
CFDataRef p12CssmDataToCf(
	const CSSM_DATA &c);
void p12CfDataToCssm(
	CFDataRef cf,
	CSSM_DATA &c,
	SecNssCoder &coder);

CSSM_DATA_PTR p12StringToUtf8(
	CFStringRef cfStr,
	SecNssCoder &coder);

const char *p12BagTypeStr(
	NSS_P12_SB_Type type);
const char *p7ContentInfoTypeStr(
	NSS_P7_CI_Type type);

/* map an OID to the components */
/* returns false if OID not found */
bool pkcsOidToParams(
	const CSSM_OID 		*oid,
	CSSM_ALGORITHMS		&keyAlg,		// e.g., CSSM_ALGID_DES
	CSSM_ALGORITHMS		&encrAlg,		// e.g., CSSM_ALGID_3DES_3KEY_EDE
	CSSM_ALGORITHMS		&pbeHashAlg,	// SHA1 or MD5
	uint32				&keySizeInBits,
	uint32				&blockSizeInBytes,	// for IV, optional
	CSSM_PADDING		&padding,		// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	&mode);			// CSSM_ALGMODE_CBCPadIV8, etc.

CSSM_RETURN p12VerifyMac(
	const NSS_P12_DecodedPFX 	&pfx,
	CSSM_CSP_HANDLE				cspHand,
	const CSSM_DATA				&pwd,	// unicode, double null terminated
	SecNssCoder					&coder);// for temp mallocs

void p12GenSalt(
	CSSM_DATA 	&salt,
	SecNssCoder &coder);

void p12GenLabel(
	CSSM_DATA &label,
	SecNssCoder &coder);

void p12NullAlgParams(
	CSSM_X509_ALGORITHM_IDENTIFIER &algId);

/*
 * Free memory via specified plugin's app-level allocator
 */
void freeCssmMemory(
	CSSM_HANDLE	hand,
	void 		*p);

/*
 * Though it pains me to do this, I must. We "happen to know" the 
 * names (in string form) of two of a key's attributes. These
 * have not been published anywhere, they are hard-coded into 
 * the script (KeySchema.m4) which generates the KeySchema
 * tables. 
 */

/*
 * This one is initially the same as the "label" argument passed 
 * in to the CSP when creating or importing keys; it eventually
 * gets munged into the hash of the associated public key (
 * in our case, by p12SetPubKeyHash()).
 */
#define P12_KEY_ATTR_LABEL_AND_HASH		"Label"

/*
 * This one is the user-friendly name.
 */
#define P12_KEY_ATTR_PRINT_NAME			"PrintName"

/*
 * Find private key by label, modify its Label attr to be the
 * hash of the associated cert's public key. 
 */
CSSM_RETURN p12SetPubKeyHash(
	CSSM_CSP_HANDLE 	cspHand,		// where the key lives
	CSSM_DL_DB_HANDLE 	dlDbHand,		// ditto
	CSSM_CSP_HANDLE		rawCspHand,		// for hash calculation
	CSSM_CL_HANDLE		clHand,			// for key/cert extraction
	const CSSM_DATA		&cert,		
	CSSM_DATA			&keyLabel,
	CSSM_DATA_PTR		newPrintName,	// optional
	SecNssCoder			&coder,			// for mallocing newLabel
	CSSM_DATA			&newLabel);		// RETURNED with label as hash

CSSM_RETURN p12AddContextAttribute(CSSM_CC_HANDLE CCHandle,
	uint32 				AttributeType,
	uint32 				AttributeLength,
	const void 			*AttributePtr);

/*
 * Find private key by specified label, delete it.
 */
CSSM_RETURN p12DeleteKey(
	CSSM_DL_DB_HANDLE 	dlDbHand, 
	const CSSM_DATA		&keyLabel);

/*
 * Standard error throwMes.
 */
#define P12_DECODE_ERR		CSSMERR_CL_INVALID_FIELD_POINTER
#define P12_ENCODE_ERR		CSSMERR_CL_INTERNAL_ERROR
#define P12_THROW_DECODE	CssmError::throwMe(P12_DECODE_ERR)
#define P12_THROW_ENCODE	CssmError::throwMe(P12_ENCODE_ERR)

#ifdef __cplusplus
}
#endif

#endif	/* _PKCS12_UTILS_H_ */

