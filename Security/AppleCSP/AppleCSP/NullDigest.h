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
 * NULLDigest.h - nop digest for use with raw signature algorithms.
 *				   NullDigest(someData) = someData.
 */
 
#ifndef	_NULL_DIGEST_H_
#define _NULL_DIGEST_H_

#include <AppleCSP/DigestObject.h>
#include <Security/cssmalloc.h>

class NullDigest : public DigestObject
{
public:
	NullDigest() : mInBuf(NULL), mInBufSize(0) 
	{ 
	}
	
	~NullDigest()
	{
		CssmAllocator::standard().free(mInBuf);
	}
	
	void digestInit() 
	{ 
		/* reusable - reset */
		CssmAllocator::standard().free(mInBuf);
		mInBufSize = 0;
		mInBuf = NULL;
	}
	
	void digestUpdate(
		const void *data, 
		size_t 		len) 
	{
		mInBuf = CssmAllocator::standard().realloc(mInBuf, mInBufSize + len);
		memmove((uint8 *)mInBuf + mInBufSize, data, len);
		mInBufSize += len;
	}
	
	virtual void digestFinal(
		void 		*digest)
	{
		memmove(digest, mInBuf, mInBufSize);
	}
										
	size_t digestSizeInBytes() const
	{ 
		return mInBufSize;
	}

private:
	void		*mInBuf;
	size_t		mInBufSize;
};

#endif	/* _NULL_DIGEST_H_ */