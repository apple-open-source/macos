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
 * pkcs12SafeBag.cpp : internal representation of various kinds
 *                   of P12 SafeBags
 *
 * Created 3/5/03 by Doug Mitchell. 
 */
 
#include "pkcs12SafeBag.h"
#include "pkcs12Debug.h"
#include "pkcs12Utils.h"
#include <string.h>
#include <assert.h>
#include <Security/utilities.h>
#include <Security/SecCertificate.h>
#include <Security/SecKeyPriv.h>
#include <SecurityNssAsn1/nssUtils.h>
#include <Security/oidsattr.h>

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
					SEC_BMPStringTemplate, &mFriendlyName)) {
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
					SEC_OctetStringTemplate, &mLocalKeyId)) {
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
	if(fname == NULL) {
		mFriendlyName.Data = NULL;
		mFriendlyName.Length = 0;
		return;
	}
	
	/* convert unicode to byte array */
	CFIndex len = CFStringGetLength(fname);
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
	if(keyId == NULL) {
		mLocalKeyId.Data = NULL;
		mLocalKeyId.Length = 0;
		return;
	}
	CFIndex len = CFDataGetLength(keyId);
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
		if(mCoder.encodeItem(&mFriendlyName, SEC_BMPStringTemplate,
				berName)) {
			p12ErrorLog("***Error encoding FriendlyName string\n");
			P12_THROW_ENCODE;
		}
		attrs[outDex++] = makeAttr(CSSMOID_PKCS9_FriendlyName,
			berName);
	}
	if(mLocalKeyId.Data) {
		CSSM_DATA berName = {0, NULL};
		if(mCoder.encodeItem(&mLocalKeyId, SEC_OctetStringTemplate,
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
		  mCertType(certType)
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
		  mCertType(certType)
{
	coder.allocCopyItem(certData, mCertData);
}

P12CertBag::~P12CertBag()
{
	/* nothing if everything we allocd is via mCoder */
}

/* convert to P12CertBag to SecCertificateRef */
SecCertificateRef P12CertBag::getSecCert()
{
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
	SecCertificateRef secCert;
	OSStatus ortn = SecCertificateCreateFromData(
		&mCertData,
		certType,
		certEncoding,
		&secCert);
	if(ortn) {
		MacOSError::throwMe(ortn);
	}
	return secCert;
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
		  mPrivKeyCreds(NULL)
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
		  mPrivKeyCreds(NULL)
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
	if(mWeOwnKey) {
		assert(mKey != NULL);
		assert(mCspHand != 0);
		CSSM_FreeKey(mCspHand, NULL, mKey, CSSM_FALSE);
	}
	if(mKeyRef) {
		CFRelease(mKeyRef);
	}
}

void P12KeyBag::setLabel(
	const CSSM_DATA &newLabel)
{
	mCoder.allocCopyItem(newLabel, mLabel);
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

