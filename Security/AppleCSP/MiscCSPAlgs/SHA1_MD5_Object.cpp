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


/*
 * DigestObject.cpp - generic C++ implementations of SHA1 and MD5. 
 *
 * Created 2/19/2001 by dmitch.
 */

#include "SHA1_MD5_Object.h"
#include <stdexcept>
#include <string.h>

/***
 *** MD5
 ***/
void MD5Object::digestInit()
{
	mIsDone = false;
	MD5Init(&mCtx);
}

void MD5Object::digestUpdate(
	const void 	*data, 
	size_t 		len)
{
	if(mIsDone) {
		throw std::runtime_error("MD5 digestUpdate after final");
	}
	MD5Update(&mCtx, (unsigned char *)data, len);
}

void MD5Object::digestFinal(
	void 		*digest)
{
	if(mIsDone) {
		throw std::runtime_error("MD5 digestFinal after final");
	}
	MD5Final(&mCtx, (unsigned char *)digest);
	mIsDone = true;
}

/* use default memberwise init */
DigestObject *MD5Object::digestClone() const
{
	return new MD5Object(*this);
}

UInt32 MD5Object::digestSizeInBytes() const
{
	return MD5_DIGEST_SIZE;
}

/***
 *** SHA1
 ***/
void SHA1Object::digestInit()
{
	mIsDone = false;
	shsInit(&mCtx);
	mBufferCount = 0;
}

void SHA1Object::digestUpdate(
	const void 	*data, 
	size_t 		len)
{
	size_t cnt;
	uint8 *uData = (uint8 *)data;
	
	if(mIsDone) { 
		throw std::runtime_error("SHA1 digestUpdate after final");
	}

	// deal with miniscule input leaving still less than one block
	if (mBufferCount + len < SHS_BLOCKSIZE) {
		memcpy(mBuffer + mBufferCount, uData, len);
		mBufferCount += len;
		return;
	}
	
	// fill possible partial existing buffer and process
	if (mBufferCount > 0) {	
		cnt = SHS_BLOCKSIZE - mBufferCount;
		memcpy(mBuffer + mBufferCount, uData, cnt);
		shsUpdate(&mCtx, mBuffer, SHS_BLOCKSIZE);
		uData += cnt;
		len   -= cnt;
	}
	
	// process remaining whole buffer multiples
	UInt32 blocks = len / SHS_BLOCKSIZE;
	if(blocks) {
		cnt = blocks * SHS_BLOCKSIZE;
		shsUpdate(&mCtx, uData, cnt);
		uData += cnt;
		len   -= cnt;
	}
	
	// keep remainder
	mBufferCount = len;
	if (len > 0) {
		memcpy(mBuffer, uData, len);
	}
}

void SHA1Object::digestFinal(
	void 		*digest)
{
	if(mIsDone) {
		throw std::runtime_error("SHA1 digestFinal after final");
	}
	if (mBufferCount > 0) {
		shsUpdate(&mCtx, mBuffer, mBufferCount);
	}
	shsFinal(&mCtx);
	memcpy(digest, mCtx.digest, SHS_DIGESTSIZE);
	mIsDone = true;
}

/* use default memberwise init */
DigestObject *SHA1Object::digestClone() const
{
	return new SHA1Object(*this);
}

UInt32 SHA1Object::digestSizeInBytes() const
{
	return SHS_DIGESTSIZE;
}

