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
 * pkcs12Templates.h
 *
 *******************************************************************
 *
 * In a probably vain attempt to clarify the structure of a PKCS12
 * PFX, here is a high-level summary.
 *
 * The top level item in P12 is a PFX.
 *
 * PFX = {
 *  	int version;
 *		ContentInfo authSafe;	-- from PKCS7
 *		MacData mac;			-- optional, password integrity version
 * }
 *
 * The authSafe in a PFX has two legal contentTypes in the P12
 * world, CT_Data (password integrity mode) or CT_SignedData 
 * (public key integrity mode). The current version of this library
 * only supports password integrity mode. Thus the integrity of 
 * the whole authSafe item is protected by a MAC in the PFX. 
 *
 * The authSafe.content field is a BER-encoded AuthenticatedSafe.
 *
 * AuthenticatedSafe = {
 *		SEQUENCE OF ContentInfo;
 * }
 *
 * OK. Each ContentInfo in an AuthenticatedSafe can either be type
 * CT_Data, CT_EnvData, or CT_EncryptedData. In the latter cases the
 * content is decrypted to produce an encoded SafeContents; in the 
 * former case the content *is* an encoded SafeContents.
 *
 * A SafeContents is a sequence of SafeBags. 
 *
 * Each SafeBag can be of several types:
 *
 *		BT_KeyBag
 *		BT_ShroudedKeyBag
 *		BT_CertBag
 *		BT_CrlBag
 *		BT_SecretBag
 *		BT_SafeContentsBag
 *
 */
 
#ifndef	_PKCS12_TEMPLATES_H_
#define _PKCS12_TEMPLATES_H_

#include <SecurityNssAsn1/secasn1t.h>
#include <SecurityNssAsn1/keyTemplates.h>	/* for NSS_Attribute */
#include <Security/cssmtype.h>
#include "pkcs7Templates.h"	/* will be lib-specific place */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MacData ::= SEQUENCE {
 * 		mac 		DigestInfo,
 *		macSalt	    OCTET STRING,
 *		iterations	INTEGER DEFAULT 1
 * }
 */
typedef struct {
	NSS_P7_DigestInfo	mac;
	CSSM_DATA			macSalt;
	CSSM_DATA			iterations;	// optional
} NSS_P12_MacData;

extern const SEC_ASN1Template NSS_P12_MacDataTemplate[];

/*
 * PFX ::= SEQUENCE {
 *   	version		INTEGER {v3(3)}(v3,...),
 *   	authSafe	ContentInfo,
 *   	macData    	MacData OPTIONAL
 * }
 */
 
/* 
 * First the top level PFX with unparsed ContentInfo.content.
 */
typedef struct {
	CSSM_DATA				version;
	NSS_P7_RawContentInfo	authSafe;
	NSS_P12_MacData			*macData;
} NSS_P12_RawPFX;

extern const SEC_ASN1Template NSS_P12_RawPFXTemplate[];

/*
 * And a PFX with a decoded ContentInfo.content.
 */
typedef struct {
	CSSM_DATA					version;
	NSS_P7_DecodedContentInfo	authSafe;
	NSS_P12_MacData				*macData;
} NSS_P12_DecodedPFX;

extern const SEC_ASN1Template NSS_P12_DecodedPFXTemplate[];

/*
 * The CSSMOID_PKCS7_Data-style ContentInfo.content of a PFX 
 * contains an encoded AuthenticatedSafe.
 *
 * AuthenticatedSafe ::= SEQUENCE OF ContentInfo
 * 		-- Data if unencrypted
 * 		-- EncryptedData if password-encrypted
 * 		-- EnvelopedData if public key-encrypted
 */
typedef struct {
	NSS_P7_DecodedContentInfo		**info;
} NSS_P12_AuthenticatedSafe;

extern const SEC_ASN1Template NSS_P12_AuthenticatedSafeTemplate[];

/* 
 * Individual BagTypes. 
 * Code on demand. 
 */
typedef CSSM_DATA	NSS_P12_KeyBag;
typedef NSS_EncryptedPrivateKeyInfo	NSS_P12_ShroudedKeyBag;
typedef CSSM_DATA	NSS_P12_SecretBag;
typedef CSSM_DATA	NSS_P12_SafeContentsBag;

/* 
 * CertBag
 *
 * CertBag ::= SEQUENCE {
 * 		certId BAG-TYPE.&id ({CertTypes}),
 * 		certValue [0] EXPLICIT BAG-TYPE.&Type ({CertTypes}{@certId})
 * }
 *
 * x509Certificate BAG-TYPE ::=
 * 		{OCTET STRING IDENTIFIED BY {certTypes 1}}
 * 			-- DER-encoded X.509 certificate stored in OCTET STRING
 * sdsiCertificate BAG-TYPE ::=
 * 		{IA5String IDENTIFIED BY {certTypes 2}}
 * 			-- Base64-encoded SDSI certificate stored in IA5String
 */
typedef enum {
	CT_Unknown,			// --> ASN_ANY
	CT_X509,
	CT_SDSI,
} NSS_P12_CertBagType;

typedef struct {
	CSSM_OID			bagType;
	NSS_P12_CertBagType	type;
	CSSM_DATA			certValue;
} NSS_P12_CertBag;

extern const SEC_ASN1Template NSS_P12_CertBagTemplate[];

/* 
 * CRLBag
 *
 * CRLBag ::= SEQUENCE {
 * 		certId BAG-TYPE.&id ({CertTypes}),
 * 		certValue [0] EXPLICIT BAG-TYPE.&Type ({CertTypes}{@certId})
 * }
 *
 * x509Certificate BAG-TYPE ::=
 * 		{OCTET STRING IDENTIFIED BY {certTypes 1}}
 * 			-- DER-encoded X.509 certificate stored in OCTET STRING
 * sdsiCertificate BAG-TYPE ::=
 * 		{IA5String IDENTIFIED BY {certTypes 2}}
 * 			-- Base64-encoded SDSI certificate stored in IA5String
 */
typedef enum {
	CRT_Unknown,			// --> ASN_ANY
	CRT_X509,
} NSS_P12_CrlBagType;

typedef struct {
	CSSM_OID			bagType;
	NSS_P12_CrlBagType	type;
	CSSM_DATA			crlValue;
} NSS_P12_CrlBag;

extern const SEC_ASN1Template NSS_P12_CrlBagTemplate[];

/*
 * BagId OIDs map to one of these for convenience. Our dynamic 
 * template chooser drops one of these into NSS_P12_SafeBag.type
 * on decode. 
 */
typedef enum {
	BT_None = 0,
	BT_KeyBag,
	BT_ShroudedKeyBag,
	BT_CertBag,
	BT_CrlBag,
	BT_SecretBag,
	BT_SafeContentsBag
} NSS_P12_SB_Type;

/*
 * The ContentInfo.content values of each element in 
 * an AuthenticatedSafe map to a sequence of these - either directly
 * (contentType CSSMOID_PKCS7_Data, octet string contents are
 * the DER encoding of this) or indirectly (encrypted or 
 * shrouded, the decrypted content is the DER encoding of this).
 */
typedef struct {
	CSSM_OID					bagId;
	NSS_P12_SB_Type				type;
	union {
		NSS_P12_KeyBag			*keyBag;
		NSS_P12_ShroudedKeyBag	*shroudedKeyBag;
		NSS_P12_CertBag			*certBag;
		NSS_P12_CrlBag			*crlBag;
		NSS_P12_SecretBag		*secretBag;
		NSS_P12_SafeContentsBag	*safeContentsBag;
	} bagValue;
	NSS_Attribute				**bagAttrs;		// optional
} NSS_P12_SafeBag;

extern const SEC_ASN1Template NSS_P12_SafeBagTemplate[];

/* 
 * SafeContents, the contents of an element in an AuthenticatedSafe.
 */
typedef struct {
	NSS_P12_SafeBag				**bags;
}
NSS_P12_SafeContents;

extern const SEC_ASN1Template NSS_P12_SafeContentsTemplate[];

/*
 * PKCS12-specific algorithm parameters.
 * A DER encoded version of this is the parameters value of 
 * a CSSM_X509_ALGORITHM_IDENTIFIER used in a 
 * NSS_P7_EncrContentInfo.encrAlg in P12 password privacy mode. 
 *
 * pkcs-12PbeParams ::= SEQUENCE {
 *		salt OCTET STRING,
 *		iterations INTEGER
 * }
 *
 * NOTE the P12 spec does place a limit on the value of iterations.
 * I guess we have to assume in actual usage that it's 
 * restricted to (0..MAX), i.e., uint32-sized. 
 *
 * We're also assuming that it is explicitly an unsigned value,
 * so that the value bytes in the encoding of 0xff would be
 * (0, 255). 
 */
typedef struct {
	CSSM_DATA		salt;
	CSSM_DATA		iterations;
} NSS_P12_PBE_Params;

extern const SEC_ASN1Template NSS_P12_PBE_ParamsTemplate[];

#ifdef __cplusplus
}
#endif

#endif	/* _PKCS12_TEMPLATES_H_ */

