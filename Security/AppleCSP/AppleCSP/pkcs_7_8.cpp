/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// pkcs_7_8.cpp - encopde/decode key blobs in PKCS7 and 
//				  PKCS8 format.
//


#include "pkcs_7_8.h"
#include "AppleCSPUtils.h"
#include <Security/threading.h>

/*
 * NOTE: snacc-generated code is believed to be not thread safe. Thus we
 * use the Mutex snaccLock to single-thread access to snacc-generated code.
 */

static Mutex snaccLock;

// bring in a ton of snacc-related stuff
#include <Security/asn-incl.h>
#include <Security/sm_vdatypes.h>

// snacc-generated - snacc really should place these in pkcs[78].h
#include <Security/sm_x501ud.h>
#include <Security/sm_x411ub.h>
#include <Security/sm_x411mtsas.h>
#include <Security/sm_x501if.h>
#include <Security/sm_x520sa.h>
#include <Security/sm_x509cmn.h>
#include <Security/sm_x509af.h>
#include <Security/sm_x509ce.h>
#include <Security/pkcs1oids.h>
#include <Security/pkcs9oids.h>
#include <Security/sm_cms.h>
#include <Security/sm_ess.h>
#include <Security/pkcs7.h>
#include <Security/pkcs8.h>

static void algAndModeToOid(
	CSSM_ALGORITHMS		alg,
	CSSM_ENCRYPT_MODE	mode,
	AsnOid				&oid)		// to set
{
	switch(alg) {
		case CSSM_ALGID_DES:
			/* FIXME - plain old 56-bit DES doesn't have an OID! */
		case CSSM_ALGID_3DES_3KEY_EDE:
			oid.ReSet(des_ede3_cbc_arc);
			break;
		case CSSM_ALGID_RC2:
			switch(mode) {
				case CSSM_ALGMODE_CBCPadIV8:
				case CSSM_ALGMODE_CBC_IV8:
					oid.ReSet(rc2_cbc_arc);
					break;
				default:
					oid.ReSet(rc2_ecb_arc);
					break;
			}
			break;
		case CSSM_ALGID_RC4:
			oid.ReSet(rc4_arc);
			break;
		case CSSM_ALGID_RC5:
			if(mode == CSSM_ALGMODE_CBCPadIV8) {
				oid.ReSet(rc5_CBCPad_arc);
			}
			else {
				oid.ReSet(rc5CBC_arc);
			}
			break;
		case CSSM_ALGID_DESX:
			oid.ReSet(desx_CBC_arc);
			break;
		case CSSM_ALGID_RSA:
			oid.ReSet(rsaEncryption_arc);	// from pkcs1oids.h
			break;
		default:
			errorLog2("algAndModeToOid: Unknown alg %d mode %d\n", (int)alg, 
				(int)mode);
			CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	return;
}


/*
 * PKCS 7 format:
 *
 *	EncryptedData ::= SEQUENCE {
 *	 	version               INTEGER {edVer0(0)} (edVer0),
 *		encryptedContentInfo  EncryptedContentInfo
 *	}
 *
 *	EncryptedContentInfo ::= SEQUENCE {
 * 		contentType     ContentType,
 * 		contentEncryptionAlgorithm
 *                 ContentEncryptionAlgorithmIdentifier,
 * 		encryptedContent
 *                 [0] IMPLICIT EncryptedContent OPTIONAL
 * 	}
 *
 *  EncryptedContent ::= OCTET STRING
 */
 
#define PKCS7_BUFSIZE	128	/* plus sizeof encryptedContent */

/*
 * Given a symmetric CssmKey in raw format, and its encrypted blob,
 * cook up a PKCS-7 encoded blob.
 */
void cspEncodePkcs7(
	CSSM_ALGORITHMS		alg,			// encryption alg, used by PKCS7
	CSSM_ENCRYPT_MODE	mode,			// ditto
	const CssmData		&encryptedBlob,
	CssmData			&encodedBlob,	// mallocd and RETURNED
	CssmAllocator		&allocator)
{
	AsnBuf					buf;
	char					*b;
	unsigned				bLen;
	EncryptedData1			ed;
	EncryptedContentInfo1	*eci;
	AlgorithmIdentifier		*algId;
	AsnLen					len;
	StLock<Mutex>			_(snaccLock);
	
	// init some values
	ed.version.Set(EncryptedDataInt::edVer0);
	eci = ed.encryptedContentInfo = new EncryptedContentInfo1;
	eci->contentType = encryptedData;	// OID from pkcs7.h
	algId = eci->contentEncryptionAlgorithm = new AlgorithmIdentifier;
	
	/* 
	 * select an AsnOid based in key algorithm and mode.
	 * Note we support more alg/mode combos that there are
	 * assigned oids; no big deal - currently we don't even
	 * parse the OID on decode anyway.
	 */
	 algAndModeToOid(alg, mode, algId->algorithm);
	
	// unlike pkcs8, this one is a pointer - it gets deleted by
	// EncryptedContentInfo1's destructor
	eci->encryptedContent = new AsnOcts(
		(char *)encryptedBlob.Data, (size_t)encryptedBlob.Length);

	// cook up an AsnBuf to stash the encoded blob in
	bLen = PKCS7_BUFSIZE + encryptedBlob.Length;
	b = (char *)allocator.malloc(bLen);
	buf.Init(b, bLen);
	buf.ResetInWriteRvsMode();
	
	// pkcs7 encode
	len = ed.BEnc(buf);
	
	// malloc & copy back to encodedBlob
	setUpCssmData(encodedBlob, len, allocator);
	memmove(encodedBlob.Data, buf.DataPtr(), len);
	allocator.free(b);
}

	
/*
 * Given a symmetric key in (encrypted, encoded) PKCS-7 format, 
 * obtain its encrypted key blob.
 */
void cspDecodePkcs7(
	const CssmKey		&wrappedKey,	// for inferring format
	CssmData			&decodedBlob,	// mallocd and RETURNED
	CSSM_KEYBLOB_FORMAT	&format,		// RETURNED
	CssmAllocator		&allocator)
{
	const CssmData			&encodedBlob = 
		CssmData::overlay(wrappedKey.KeyData);
	ENV_TYPE				jbuf;
	EncryptedData1			ed;
	int						rtn;
	AsnBuf					buf;
	size_t					len = (size_t)encodedBlob.Length;
	StLock<Mutex>			_(snaccLock);
	
	buf.InstallData((char *)encodedBlob.Data, len);
	try {
		int i;
		EncryptedContentInfo1 *eci;
		
		ed.BDec(buf, len, jbuf);
		
		i = ed.version;
		if(i != EncryptedDataInt::edVer0) {
			errorLog1("cspDecodePkcs7: bad edDec.version (%d)\n", i);
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
		}
		eci = ed.encryptedContentInfo;
		if(!(eci->contentType == encryptedData)) {
			errorLog0("cspDecodePkcs7: bad contentType\n");
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
		}
		
		/* ignore encryption alg (for now) */

		/* eci->encryptedContent is decodedBlob */
		setUpCssmData(decodedBlob,
			eci->encryptedContent->Len(),
			allocator);
		memmove(decodedBlob.Data, 
			(char *)(*eci->encryptedContent),	
			eci->encryptedContent->Len());
	}
	catch(...) {
		errorLog1("cspDecodePkcs7: BDec threw %d\n", rtn);
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	}

	format = inferFormat(wrappedKey);
}

/*
 * PKCS-8 format
 *
 *	EncryptedPrivateKeyInfo ::= SEQUENCE {
 *   	encryptionAlgorithm AlgorithmIdentifier 
 *				{{KeyEncryptionAlgorithms}},
 *   	encryptedData EncryptedData 
 *	}
 *
 *	EncryptedData ::= OCTET STRING
 */
 
#define PKCS8_BUFSIZE	64	/* plus sizeof encryptedBlob */

/*
 * Given an asymmetric CssmKey in raw format, and its encrypted blob,
 * cook up a PKCS-8 encoded blob.
 */
void cspEncodePkcs8(
	CSSM_ALGORITHMS		alg,			// encryption alg, used by PKCS8
	CSSM_ENCRYPT_MODE	mode,			// ditto
	const CssmData		&encryptedBlob,
	CssmData			&encodedBlob,	// mallocd and RETURNED
	CssmAllocator		&allocator)
{
	AsnBuf					buf;
	char					*b;
	unsigned				bLen;
	EncryptedPrivateKeyInfo	epki;
	AsnLen					len;
	StLock<Mutex>			_(snaccLock);
	
	epki.encryptionAlgorithm = new AlgorithmIdentifier;
	algAndModeToOid(alg, mode, epki.encryptionAlgorithm->algorithm);
	epki.encryptedKey.Set((char *)encryptedBlob.Data, encryptedBlob.Length);

	// cook up an AsnBuf to stash the encoded blob in
	bLen = PKCS8_BUFSIZE + encryptedBlob.Length;
	b = (char *)allocator.malloc(bLen);
	buf.Init(b, bLen);
	buf.ResetInWriteRvsMode();
	
	// pkcs8 encode
	len = epki.BEnc(buf);
	
	// malloc & copy back to encodedBlob
	setUpCssmData(encodedBlob, len, allocator);
	memmove(encodedBlob.Data, buf.DataPtr(), len);
	allocator.free(b);
}

/*
 * Given a a private key in (encrypted, encoded) PKCS-8 format, 
 * obtain its encrypted key blob.
 */
void cspDecodePkcs8(
	const CssmKey		&wrappedKey,	// for inferring format
	CssmData			&decodedBlob,	// mallocd and RETURNED
	CSSM_KEYBLOB_FORMAT	&format,		// RETURNED
	CssmAllocator		&allocator)
{
	const CssmData			&encodedBlob = 
		CssmData::overlay(wrappedKey.KeyData);
	ENV_TYPE				jbuf;
	EncryptedData1			ed;
	int						rtn;
	AsnBuf					buf;
	size_t					len = (size_t)encodedBlob.Length;
	StLock<Mutex>			_(snaccLock);
	
	buf.InstallData((char *)encodedBlob.Data, len);
	try {
		EncryptedPrivateKeyInfo		epki;
		
		epki.BDec(buf, len, jbuf);
		
		/* skip algorithm - just snag encryptedKey */
		len = epki.encryptedKey.Len();
		setUpCssmData(decodedBlob, len,	allocator);
		memmove(decodedBlob.Data, 
			(char *)(epki.encryptedKey),	
			len);
	}
	catch(...) {
		errorLog1("cspDecodePkcs8: BDec threw %d\n", rtn);
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	}

	format = inferFormat(wrappedKey);
}
