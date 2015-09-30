/*
 * Copyright (c) 2000-2004,2011,2014 Apple Inc. All Rights Reserved.
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
 * SHA1_MD5_Object.h - SHA1, MD5 digest objects 
 *
 */

#ifndef	_SHA1_MD5_OBJECT_H_
#define _SHA1_MD5_OBJECT_H_

#include <security_cdsa_utilities/digestobject.h>
#include <CommonCrypto/CommonDigest.h>

class SHA1Object : public DigestObject
{
public:
	SHA1Object() { }
	virtual ~SHA1Object() { };
	virtual void digestInit();
	virtual void digestUpdate(
		const void 	*data, 
		size_t 		len);
	virtual void digestFinal(
		void 		*digest);
	virtual DigestObject *digestClone() const;
	virtual size_t digestSizeInBytes() const;
private:
	CC_SHA1_CTX		mCtx;
};

class MD5Object : public DigestObject
{
public:
	MD5Object() { }
	virtual ~MD5Object() { }
	virtual void digestInit();
	virtual void digestUpdate(
		const void 	*data, 
		size_t 		len);
	virtual void digestFinal(
		void 		*digest);
	virtual DigestObject *digestClone() const;
	virtual size_t digestSizeInBytes() const;
private:
	CC_MD5_CTX mCtx;
};

#endif	/* _SHA1_MD5_OBJECT_H_ */
