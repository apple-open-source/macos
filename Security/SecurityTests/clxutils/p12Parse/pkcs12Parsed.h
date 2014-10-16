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
 * Parsed contents of a p12 blob
 */
 
#ifndef	_PKCS12_PARSED_H_
#define _PKCS12_PARSED_H_

#include <Security/cssmtype.h>
#include <string.h>
#include <security_asn1/SecNssCoder.h>


/*
 * A collection of CSSM_DATAs which are known to be {Cert, CRL, ...}
 */
class P12KnownBlobs {
public:
	P12KnownBlobs(SecNssCoder &coder);
	~P12KnownBlobs() { }
	void addBlob(const CSSM_DATA &blob);
	
	CSSM_DATA	*mBlobs;
	unsigned	mNumBlobs;
	SecNssCoder &mCoder;
};

/*
 * Unknown thingie.
 */
class P12UnknownBlob {
public:
	P12UnknownBlob(const CSSM_DATA &blob, const CSSM_OID &oid);
	P12UnknownBlob(const CSSM_DATA &blob, const char *descr);

	CSSM_DATA	mBlob;
	CSSM_OID	mOid;			// optional
	char		mDescr[200];	// optional
};

class P12UnknownBlobs {
public:
	P12UnknownBlobs(SecNssCoder &coder);
	~P12UnknownBlobs();
	void addBlob(P12UnknownBlob *blob);

	P12UnknownBlob	**mBlobs;
	unsigned		mNumBlobs;
	SecNssCoder 	&mCoder;
};

/*
 * The stuff we can get by parsing a p12 blob.
 * Currently highly incomplete. Add to it when we can 
 * parse more.
 */
typedef enum {
	PE_Cert,		// X509 only
	PE_CRL,			// ditto
	PE_Other		// expand here
} P12ElementType;

class P12Parsed {
public:
	P12Parsed(SecNssCoder &coder);
	~P12Parsed() { }
	
	SecNssCoder		&mCoder;
	/* the stuff */
	P12KnownBlobs	mCerts;
	P12KnownBlobs	mCrls;
	P12UnknownBlobs mUnknown;
};

#endif	/* _PKCS12_PARSED_H_ */
