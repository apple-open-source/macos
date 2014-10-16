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
 * pkcs12Parsed.h - Parsed contents of a p12 blob
 */
 
#include "pkcs12Parsed.h"
#include <string.h>

P12KnownBlobs::P12KnownBlobs(
	SecNssCoder &coder)
		: mBlobs(NULL), mNumBlobs(0), mCoder(coder)
{

}

void P12KnownBlobs::addBlob(
	const CSSM_DATA &blob)
{
	/* coder doesn't have a realloc */
	CSSM_DATA *newBlobs = 
		(CSSM_DATA *)mCoder.malloc((mNumBlobs + 1) * sizeof(CSSM_DATA));
	memmove(newBlobs, mBlobs, mNumBlobs * sizeof(CSSM_DATA));
	newBlobs[mNumBlobs++] = blob;
	mBlobs = newBlobs;
}

P12UnknownBlob::P12UnknownBlob(
	const CSSM_DATA &blob, 
	const char *descr)
{
	mBlob = blob;
	mOid.Data = NULL;
	mOid.Length = 0;
	strcpy(mDescr, descr);
}

P12UnknownBlob::P12UnknownBlob(
	const CSSM_DATA &blob, 
	const CSSM_OID &oid)
{
	mBlob = blob;
	mOid = oid;
	mDescr[0] = '\0';
}

P12UnknownBlobs::P12UnknownBlobs(
	SecNssCoder &coder)
		: mBlobs(NULL), mNumBlobs(0), mCoder(coder)
{

}

P12UnknownBlobs::~P12UnknownBlobs()
{
	for(unsigned dex=0; dex<mNumBlobs; dex++) {
		delete mBlobs[dex];
	}
}

void P12UnknownBlobs::addBlob(
	P12UnknownBlob *blob)
{
	/* coder doesn't have a realloc */
	P12UnknownBlob **newBlobs = 
		(P12UnknownBlob **)mCoder.malloc((mNumBlobs + 1) * 
			sizeof(P12UnknownBlob *));
	memmove(newBlobs, mBlobs, mNumBlobs * sizeof(P12UnknownBlob *));
	newBlobs[mNumBlobs++] = blob;
	mBlobs = newBlobs;
}

P12Parsed::P12Parsed(SecNssCoder &coder)
	: mCoder(coder), mCerts(coder), mCrls(coder), mUnknown(coder)
{

}
