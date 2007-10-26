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
 * MD2Object.cpp
 */
#include "MD2Object.h"
#include <stdexcept>
#include <string.h>

void MD2Object::digestInit()
{
	setIsDone(false);
	CC_MD2_Init(&mCtx);
}

void MD2Object::digestUpdate(
	const void 	*data, 
	size_t 		len)
{
	if(isDone()) {
		throw std::runtime_error("MD2 digestUpdate after final");
	}
	CC_MD2_Update(&mCtx, (unsigned char *)data, len);
}

void MD2Object::digestFinal(
	void 		*digest)
{
	if(isDone()) {
		throw std::runtime_error("MD2 digestFinal after final");
	}
	CC_MD2_Final((unsigned char *)digest, &mCtx);
	setIsDone(true);
}

/* use default memberwise init */
DigestObject *MD2Object::digestClone() const
{
	return new MD2Object(*this);
}

CSSM_SIZE MD2Object::digestSizeInBytes() const
{
	return CC_MD2_DIGEST_LENGTH;
}

