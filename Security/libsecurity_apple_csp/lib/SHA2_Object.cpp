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
 * SHA2_Object.cpp - SHA2 digest objects 
 * Created 8/12/2004 by dmitch.
 * Created 2/19/2001 by dmitch.
 */

#include "SHA2_Object.h"
#include <stdexcept>
#include <string.h>

/***
 *** SHA224
 ***/
void SHA224Object::digestInit()
{
	mIsDone = false;
	CC_SHA224_Init(&mCtx);
}

void SHA224Object::digestUpdate(
	const void 	*data, 
	size_t 		len)
{
	CC_SHA224_Update(&mCtx, (const unsigned char *)data, (CC_LONG)len);
}

void SHA224Object::digestFinal(
	void 		*digest)
{
	CC_SHA224_Final((unsigned char *)digest, &mCtx);
	mIsDone = true;
}

/* use default memberwise init */
DigestObject *SHA224Object::digestClone() const
{
	return new SHA224Object(*this);
}

size_t SHA224Object::digestSizeInBytes() const
{
	return CC_SHA224_DIGEST_LENGTH;
}

/***
 *** SHA256
 ***/
void SHA256Object::digestInit()
{
	mIsDone = false;
	CC_SHA256_Init(&mCtx);
}

void SHA256Object::digestUpdate(
	const void 	*data, 
	size_t 		len)
{
	CC_SHA256_Update(&mCtx, (const unsigned char *)data, (CC_LONG)len);
}

void SHA256Object::digestFinal(
	void 		*digest)
{
	CC_SHA256_Final((unsigned char *)digest, &mCtx);
	mIsDone = true;
}

/* use default memberwise init */
DigestObject *SHA256Object::digestClone() const
{
	return new SHA256Object(*this);
}

size_t SHA256Object::digestSizeInBytes() const
{
	return CC_SHA256_DIGEST_LENGTH;
}

/***
 *** SHA384
 ***/
void SHA384Object::digestInit()
{
	mIsDone = false;
	CC_SHA384_Init(&mCtx);
}

void SHA384Object::digestUpdate(
	const void 	*data, 
	size_t 		len)
{
	CC_SHA384_Update(&mCtx, (const unsigned char *)data, (CC_LONG)len);
}

void SHA384Object::digestFinal(
	void 		*digest)
{
	CC_SHA384_Final((unsigned char *)digest, &mCtx);
	mIsDone = true;
}

/* use default memberwise init */
DigestObject *SHA384Object::digestClone() const
{
	return new SHA384Object(*this);
}

size_t SHA384Object::digestSizeInBytes() const
{
	return CC_SHA384_DIGEST_LENGTH;
}

/***
 *** SHA512
 ***/
void SHA512Object::digestInit()
{
	mIsDone = false;
	CC_SHA512_Init(&mCtx);
}

void SHA512Object::digestUpdate(
	const void 	*data, 
	size_t 		len)
{
	CC_SHA512_Update(&mCtx, (const unsigned char *)data, (CC_LONG)len);
}

void SHA512Object::digestFinal(
	void 		*digest)
{
	CC_SHA512_Final((unsigned char *)digest, &mCtx);
	mIsDone = true;
}

/* use default memberwise init */
DigestObject *SHA512Object::digestClone() const
{
	return new SHA512Object(*this);
}

size_t SHA512Object::digestSizeInBytes() const
{
	return CC_SHA512_DIGEST_LENGTH;
}

