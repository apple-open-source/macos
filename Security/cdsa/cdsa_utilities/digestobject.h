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
 * digestobject.h - generic virtual Digest base class 
 */

#ifndef	_DIGEST_OBJECT_H_
#define _DIGEST_OBJECT_H_

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>
#include <Security/cssmalloc.h>

/* common virtual digest class */
class DigestObject {
public:
	DigestObject() : mInitFlag(false), mIsDone(false) { }
	virtual ~DigestObject() { }
	
	/* 
	 * The remaining functions must be implemented by subclass. 
	 */
	/* init is reusable */
	virtual void digestInit() = 0;

	/* add some data */
	virtual void digestUpdate(
		const void *data, 
		size_t 		len) = 0;
	
	/* obtain digest (once only per init, update, ... cycle) */
	virtual void digestFinal(
		void 		*digest) = 0;  	/* RETURNED, alloc'd by caller */
	
	/* sublass-specific copy */
	virtual DigestObject *digestClone() const = 0;
	
	virtual size_t digestSizeInBytes() const = 0;

protected:
	bool			mInitFlag;
	bool			mIsDone;	
			
	bool			initFlag() 				{ return mInitFlag; }
	void			setInitFlag(bool flag) 	{ mInitFlag = flag; }
	bool			isDone() 				{ return mIsDone; }
	void			setIsDone(bool done) 	{ mIsDone = done; }
};

/*
 * NullDigest.h - nop digest for use with raw signature algorithms.
 *				  NullDigest(someData) = someData.
 */
class NullDigest : public DigestObject
{
public:
	NullDigest() : mInBuf(NULL), mInBufSize(0) 
	{ 
	}

	void digestInit() 
	{ 
		/* reusable - reset */
		if(mInBufSize) {
			assert(mInBuf != NULL);
			memset(mInBuf, 0, mInBufSize);
			CssmAllocator::standard().free(mInBuf);
			mInBufSize = 0;
			mInBuf = NULL;
		}
	}

	~NullDigest()
	{
		digestInit();
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
										
	virtual DigestObject *digestClone() const
	{
		NullDigest *cloned = new NullDigest;
		cloned->digestUpdate(mInBuf, mInBufSize);
		return cloned;
	}
	
	/* unique to NullDigest - just obtain current data ptr, no copy */
	virtual const void *digestPtr() { return mInBuf; }
	
	size_t digestSizeInBytes() const
	{ 
		return mInBufSize;
	}

private:
	void		*mInBuf;
	size_t		mInBufSize;
};

#endif	/* _DIGEST_OBJECT_H_ */
