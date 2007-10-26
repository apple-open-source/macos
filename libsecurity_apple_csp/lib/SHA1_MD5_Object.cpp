/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
	CC_MD5_Init(&mCtx);
}

void MD5Object::digestUpdate(
	const void 	*data, 
	size_t 		len)
{
	if(mIsDone) {
		throw std::runtime_error("MD5 digestUpdate after final");
	}
	CC_MD5_Update(&mCtx, data, len);
}

void MD5Object::digestFinal(
	void 		*digest)
{
	if(mIsDone) {
		throw std::runtime_error("MD5 digestFinal after final");
	}
	CC_MD5_Final((unsigned char *)digest, &mCtx);
	mIsDone = true;
}

/* use default memberwise init */
DigestObject *MD5Object::digestClone() const
{
	return new MD5Object(*this);
}

size_t MD5Object::digestSizeInBytes() const
{
	return CC_MD5_DIGEST_LENGTH;
}

/***
 *** SHA1
 ***/
void SHA1Object::digestInit()
{
	mIsDone = false;
	CC_SHA1_Init(&mCtx);
}

void SHA1Object::digestUpdate(
	const void 	*data, 
	size_t 		len)
{
	CC_SHA1_Update(&mCtx, (const unsigned char *)data, len);
}

void SHA1Object::digestFinal(
	void 		*digest)
{
	CC_SHA1_Final((unsigned char *)digest, &mCtx);
	mIsDone = true;
}

/* use default memberwise init */
DigestObject *SHA1Object::digestClone() const
{
	return new SHA1Object(*this);
}

size_t SHA1Object::digestSizeInBytes() const
{
	return CC_SHA1_DIGEST_LENGTH;
}

