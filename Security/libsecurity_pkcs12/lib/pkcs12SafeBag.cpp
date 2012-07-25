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
 * pkcs12SafeBag.cpp : internal representation of various kinds
 *                   of P12 SafeBags
 */
 
#include "pkcs12SafeBag.h"
#include "pkcs12Debug.h"
#include "pkcs12Utils.h"
#include <string.h>
#include <assert.h>
#include <Security/Security.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecAsn1Templates.h>
#include <security_asn1/nssUtils.h>

/*
 * P12SafeBag, abstract superclass of all safe bags.
 *
 * Constructor for decoding. Attr may include friendlyName and localKeyId;
 * It may also be empty or NULL.
 */
P12SafeBag::P12SafeBag(
	NSS_Attribute 		**attrs, // including friendlyName, etc.
	SecNssCoder 		&coder)
	: mBagAttrs(coder),
	  mCoder(coder)
{
	mFriendlyName.Data = mLocalKeyId.Data = NULL;
	mFriendlyName.Length = mLocalKeyId.Length = 0;
	
	/* parse attrs into friendlyName, localKeyId, and generic attrs */
	unsigned numAttrs = nssArraySize((const void **)attrs);
	for(unsigned dex=0; dex<numAttrs; dex++) {
		NSS_Attribute *attr = attrs[dex];
		unsigned numValues = nssArraySize((const void**)attr->attrValue);
		
		if(nssCompareCssmData(&attr->attrType, 
				&CSSMOID_PKCS9_FriendlyName)) {
			/* 
			 * BMP string (UniCode). Spec says only one legal value.
			 */
			if(numValues != 1) {
				p12ErrorLog("FriendlyName with %u values\n", numValues);
				/* but let's keep going if we can */
				if(numValues == 0) {
					P12_THROW_DECODE;
				}
			}
			if(mCoder.decodeItem(*attr->attrValue[0],
					kSecAsn1BMPStringTemplate, &mFriendlyName)) {
				p12ErrorLog("***Error decoding FriendlyName string\n");
				P12_THROW_DECODE;
			}
		}
		else if(nssCompareCssmData(&attr->attrType, 
					&CSSMOID_PKCS9_LocalKeyId)) {
			/* 
			 * Octet string. Spec says only one legal value.
			 */
			if(numValues != 1) {
				p12ErrorLog("LocalKeyId with %u values\n", numValues);
				/* but let's keep going if we can */
				if(numValues == 0) {
					P12_THROW_DECODE;
				}
			}
			if(mCoder.decodeItem(*attr->attrValue[0],
					kSecAsn1OctetStringTemplate, &mLocalKeyId)) {
				p12ErrorLog("***Error decoding LocalKeyId\n");
				P12_THROW_DECODE;
			}
		}
		else {
			/* others */
			mBagAttrs.addAttr(*attr);
		}
	}
}

/*
 * Constructor for encoding. All arguments except for the coder 
 * are optional.
 */
P12SafeBag::P12SafeBag(
	CFStringRef			fname,
	CFDataRef			keyId,
	P12BagAttrs			*otherAttrs,		// optional
	SecNssCoder 		&coder)
	: mBagAttrs(otherAttrs, coder),
	  mCoder(coder)
{
	friendlyName(fname);
	localKeyId(keyId);
}

P12SafeBag::~P12SafeBag()
{
	/* nothing if everything we allocd is via mCoder */
}

/* 
 * Setters in CF terms, used when constructing prior
 * to encoding.
 */
void P12SafeBag::friendlyName(
	CFStringRef 		fname)
{
	CFIndex len = 0;
	
	if(fname != NULL) {
		len = CFStringGetLength(fname);
	}
	if(len == 0) {
		mFriendlyName.Data = NULL;
		mFriendlyName.Length = 0;
		return;
	}
	
	/* convert unicode to byte array */
	unsigned flen = len * sizeof(UniChar);
	mCoder.allocItem(mFriendlyName, flen);
	unsigned char *cp = mFriendlyName.Data;
	for(CFIndex dex=0; dex<len; dex++) {
		UniChar uc = CFStringGetCharacterAtIndex(fname, dex);
		*cp++ = uc >> 8;
		*cp++ = uc & 0xff;
	}
}

void P12SafeBag::localKeyId(
	CFDataRef 			keyId)
{
	CFIndex len = 0;
	
	if(keyId != NULL) {
		len = CFDataGetLength(keyId);
	}
	if(len == 0) {
		mLocalKeyId.Data = NULL;
		mLocalKeyId.Length = 0;
		return;
	}
	mCoder.allocItem(mLocalKeyId, len);
	const UInt8 *cp = CFDataGetBytePtr(keyId);
	memmove(mLocalKeyId.Data, cp, len);
}

/* 
 * Copy out all attrs in API form. All incoming ptrs
 * are optional.
 */
void P12SafeBag::copyAllAttrs(
	CFStringRef 		*fName,
	CFDataRef			*keyId,
	P12BagAttrs			**bagAttrs)
{
	if(fName) {
		*fName = friendlyName();
	}
	if(keyId) {
		*keyId = localKeyId();
	}
	if(bagAttrs) {
		if(mBagAttrs.numAttrs()) {
			/* make a copy of our bag attrs */
			P12BagAttrs *attrs = new P12BagAttrs(&mBagAttrs, mCoder);
			*bagAttrs = attrs;
		}
		else {
			*bagAttrs = NULL;
		}
	}
}


/* getters in CF terms - result is created and returned */
CFStringRef P12SafeBag::friendlyName()
{
	if(mFriendlyName.Data == NULL) {
		/* not present, no error */
		return NULL;
	}
	/* shouldn't have stored this if it's an odd number of bytes */
	assert((mFriendlyName.Length & 1) == 0);
	
	/* convert byte array to unicode */
	unsigned strLen = mFriendlyName.Length / 2;
	UniChar *uc = (UniChar *)malloc(strLen * sizeof(UniChar));
	const uint8 *inp = mFriendlyName.Data;
	UniChar *outp = uc;
	while(inp < (mFriendlyName.Data + mFriendlyName.Length)) {
		*outp = (((unsigned)inp[0]) << 8) | inp[1];
		outp++;
		inp += 2;
	}
	CFStringRef cstr = CFStringCreateWithCharacters(NULL, uc, strLen);
	free(uc);
	return cstr;
}

CFDataRef P12SafeBag::localKeyId()
{
	if(mLocalKeyId.Data == NULL) {
		/* not present, no error */
		return NULL;
	}
	return CFDataCreate(NULL, (const UInt8 *)mLocalKeyId.Data,
		mLocalKeyId.Length);
}

/* 
 * Get all attrs, including friendlyName and localKeyId,
 * in preparation for encoding.
 */
NSS_Attribute **P12SafeBag::getAllAttrs()
{
	unsigned numAttrs = mBagAttrs.numAttrs();
	if(mFriendlyName.Data) {
		numAttrs++;
	}
	if(mLocalKeyId.Data) {
		numAttrs++;
	}
	NSS_Attribute **attrs = 
		(NSS_Attribute **)p12NssNullArray(numAttrs, mCoder);
	unsigned outDex=0;
	for(unsigned i=0; i<mBagAttrs.numAttrs(); i++) {
		attrs[outDex++] = mBagAttrs.getAttr(i);
	}
	if(mFriendlyName.Data) {
		CSSM_DATA berName = {0, NULL};
		if(mCoder.encodeItem(&mFriendlyName, kSecAsn1BMPStringTemplate,
				berName)) {
			p12ErrorLog("***Error encoding FriendlyName string\n");
			P12_THROW_ENCODE;
		}
		attrs[outDex++] = makeAttr(CSSMOID_PKCS9_FriendlyName,
			berName);
	}
	if(mLocalKeyId.Data) {
		CSSM_DATA berName = {0, NULL};
		if(mCoder.encodeItem(&mLocalKeyId, kSecAsn1OctetStringTemplate,
				berName)) {
			p12ErrorLog("***Error encoding LocalKeyId string\n");
			P12_THROW_ENCODE;
		}
		attrs[outDex++] = makeAttr(CSSMOID_PKCS9_LocalKeyId,
			berName);
	}
	assert(outDex == numAttrs);
	return attrs;
}

/*
 * Create an NSS_Attribute * for friendlyName or keyId
 */
NSS_Attribute *P12SafeBag::makeAttr(
	const CSSM_OID		&attrId,
	const CSSM_DATA		&attrValue)
{
	NSS_Attribute *attr = mCoder.mallocn<NSS_Attribute>();
	mCoder.allocCopyItem(attrId, attr->attrType);
	attr->attrValue = mCoder.mallocn<CSSM_DATA *>(2);
	attr->attrValue[0] = mCoder.mallocn<CSSM_DATA>();
	attr->attrValue[1] = NULL;
	mCoder.allocCopyItem(attrValue, *attr->attrValue[0]);
	return attr;
}

/*
 * Individual bag types 
 */
 
/* decode */
P12CertBag::P12CertBag(
	NSS_P12_CertBagType	certType,	// CT_X509, CT_SDSI
	CSSM_DATA			&certData,
	NSS_Attribute		**attrs,	// optional
	SecNssCoder			&coder)
		: P12SafeBag(attrs, coder),
		  mCertType(certType),
		  mCertRef(NULL)
{
	coder.allocCopyItem(certData, mCertData);
}

/* encode */
P12CertBag::P12CertBag(
	NSS_P12_CertBagType	certType,	// CT_X509, CT_SDSI
	CSSM_DATA			&certData,
	CFStringRef			fname,
	CFDataRef			keyId,
	P12BagAttrs			*otherAttrs,
	SecNssCoder			&coder)
		: P12SafeBag(fname, keyId, otherAttrs, coder),
		  mCertType(certType),
		  mCertRef(NULL)
{
	coder.allocCopyItem(certData, mCertData);
}

P12CertBag::~P12CertBag()
{
	if(mCertRef) {
		CFRelease(mCertRef);
	}
	/* everything else we allocd is via mCoder */
}

/* convert to P12CertBag to SecCertificateRef */
SecCertificateRef P12CertBag::getSecCert()
{
	if(mCertRef) {
		CFRetain(mCertRef);		/* a ref count for the caller */
		return mCertRef;
	}
	
	/* lazy creation... */
	CSSM_CERT_TYPE certType;
	CSSM_CERT_ENCODING certEncoding;
	switch(mCertType) {
		case CT_X509:
			certType = CSSM_CERT_X_509v3;
			certEncoding = CSSM_CERT_ENCODING_DER;
			break;
		case CT_SDSI:
			certType = CSSM_CERT_SDSIv1;
			/* it's base64 encoded - no value for that in this enum */
			certEncoding = CSSM_CERT_ENCODING_UNKNOWN;
			break;
		default:
			/* shouldn't currently happen, but... */
			certType = CSSM_CERT_UNKNOWN;
			certEncoding = CSSM_CERT_ENCODING_UNKNOWN;
			break;
	}
	OSStatus ortn = SecCertificateCreateFromData(
		&mCertData,
		certType,
		certEncoding,
		&mCertRef);
	if(ortn) {
		MacOSError::throwMe(ortn);
	}
	
	/* One ref count for us, one for the caller */
	CFRetain(mCertRef);
	return mCertRef;
}

P12CrlBag::P12CrlBag(
	NSS_P12_CrlBagType	crlType,	// CRT_X509, only for now
	CSSM_DATA			&crlData,
	NSS_Attribute		**attrs,	// optional
	SecNssCoder			&coder)
		: P12SafeBag(attrs, coder), 
		  mCrlType(crlType)
{
	coder.allocCopyItem(crlData, mCrlData);
}

P12CrlBag::P12CrlBag(
	NSS_P12_CrlBagType	crlType,	// CRT_X509, only for now
	CFDataRef			crlData,
	CFStringRef			fname,
	CFDataRef			keyId,
	P12BagAttrs			*otherAttrs,
	SecNssCoder			&coder)
		: P12SafeBag(fname, keyId, otherAttrs, coder),
		  mCrlType(crlType)
{
	coder.allocCopyItem(CFDataGetBytePtr(crlData), 
		CFDataGetLength(crlData), mCrlData);
}

P12CrlBag::~P12CrlBag()
{
	/* nothing if everything we allocd is via mCoder */
}

/* 
 * For decode - both shrouded and plain.
 * On decode, we own the key and will do the CSSM_FreeKey in
 * our destructor. Caller owns the actual CSSM_KEY memory. 
 */
P12KeyBag::P12KeyBag(
	CSSM_KEY_PTR		key,	
	CSSM_CSP_HANDLE		cspHand,
	NSS_Attribute		**attrs,	// optional
	CSSM_DATA			&labelData,
	SecNssCoder			&coder)
		: P12SafeBag(attrs, coder),
		  mKey(key),
		  mCspHand(cspHand),
		  mKeyRef(NULL),
		  mWeOwnKey(true),
		  mPrivKeyCreds(NULL),
		  mDupKey(false)
{
	setLabel(labelData);
}

/* for encode - app owns CSSM_KEY */
P12KeyBag::P12KeyBag(
	const CSSM_KEY		*key,	
	CSSM_CSP_HANDLE		cspHand,
	CFStringRef			fname,
	CFDataRef			keyId,
	P12BagAttrs			*otherAttrs,
	SecNssCoder			&coder,
	SecKeyRef			keyRef /* = NULL */)

		: P12SafeBag(fname, keyId, otherAttrs, coder),
		  mKey((CSSM_KEY_PTR)key),
		  mCspHand(cspHand),
		  mKeyRef(keyRef),
		  mWeOwnKey(false),		// app giveth, app taketh away
		  mPrivKeyCreds(NULL),
		  mDupKey(false)
{
	if(mKeyRef) {
		CFRetain(mKeyRef);
		/* 
	 	 * Get creds associated with this key 
		 */
		OSStatus ortn = SecKeyGetCredentials(mKeyRef,
			CSSM_ACL_AUTHORIZATION_EXPORT_WRAPPED,
			kSecCredentialTypeDefault,
			&mPrivKeyCreds);
		if(ortn) {
			p12LogCssmError("SecKeyGetCredentials", ortn);
			MacOSError::throwMe(ortn);
		}
	}
	mLabel.Data = NULL;
	mLabel.Length = 0;
}


P12KeyBag::~P12KeyBag()
{
	freeKey();
}

void P12KeyBag::setLabel(
	const CSSM_DATA &newLabel)
{
	mCoder.allocCopyItem(newLabel, mLabel);
}

/* reusable key setter */
void P12KeyBag::setKey(
	CSSM_KEY_PTR cssmKey)
{
	freeKey();
	mKey = cssmKey;
}

void P12KeyBag::freeKey()
{
	if(mWeOwnKey) {
		assert(mKey != NULL);
		assert(mCspHand != 0);
		CSSM_FreeKey(mCspHand, NULL, mKey, CSSM_FALSE);
	}
	mKey = NULL;
	if(mKeyRef) {
		CFRelease(mKeyRef);
		mKeyRef = NULL;
	}
}

/*
 * Others we don't implement 
 */
P12OpaqueBag::P12OpaqueBag(
	const CSSM_OID		&oid,
	const CSSM_DATA		&blob,
	NSS_Attribute		**attrs,	// optional
	SecNssCoder			&coder)
		: P12SafeBag(attrs, coder)
{
	coder.allocCopyItem(oid, mOid);
	coder.allocCopyItem(blob, mBlob);
}

P12OpaqueBag::P12OpaqueBag(
	CFDataRef			oid,
	CFDataRef			blob,
	CFStringRef			fname,
	CFDataRef			keyId,
	P12BagAttrs			*otherAttrs,
	SecNssCoder			&coder)
		: P12SafeBag(fname, keyId, otherAttrs, coder)
{
	coder.allocCopyItem(CFDataGetBytePtr(oid), 
		CFDataGetLength(oid), mOid);
	coder.allocCopyItem(CFDataGetBytePtr(blob), 
		CFDataGetLength(blob), mBlob);
}

P12OpaqueBag::~P12OpaqueBag()
{
	/* nothing if everything we allocd is via mCoder */
}

