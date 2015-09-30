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
 * SHA2_Object.h - SHA2 digest objects 
 *
 */

#ifndef	_SHA2_OBJECT_H_
#define _SHA2_OBJECT_H_

#include <security_cdsa_utilities/digestobject.h>
#include <CommonCrypto/CommonDigest.h>

class SHA224Object : public DigestObject
{
public:
	SHA224Object() { }
	virtual ~SHA224Object() { };
	virtual void digestInit();
	virtual void digestUpdate(
		const void 	*data, 
		size_t 		len);
	virtual void digestFinal(
		void 		*digest);
	virtual DigestObject *digestClone() const;
	virtual size_t digestSizeInBytes() const;
private:
	CC_SHA256_CTX		mCtx;
};

class SHA256Object : public DigestObject
{
public:
	SHA256Object() { }
	virtual ~SHA256Object() { };
	virtual void digestInit();
	virtual void digestUpdate(
		const void 	*data, 
		size_t 		len);
	virtual void digestFinal(
		void 		*digest);
	virtual DigestObject *digestClone() const;
	virtual size_t digestSizeInBytes() const;
private:
	CC_SHA256_CTX		mCtx;
};

class SHA384Object : public DigestObject
{
public:
	SHA384Object() { }
	virtual ~SHA384Object() { };
	virtual void digestInit();
	virtual void digestUpdate(
		const void 	*data, 
		size_t 		len);
	virtual void digestFinal(
		void 		*digest);
	virtual DigestObject *digestClone() const;
	virtual size_t digestSizeInBytes() const;
private:
	CC_SHA512_CTX		mCtx;
};

class SHA512Object : public DigestObject
{
public:
	SHA512Object() { }
	virtual ~SHA512Object() { };
	virtual void digestInit();
	virtual void digestUpdate(
		const void 	*data, 
		size_t 		len);
	virtual void digestFinal(
		void 		*digest);
	virtual DigestObject *digestClone() const;
	virtual size_t digestSizeInBytes() const;
private:
	CC_SHA512_CTX		mCtx;
};

#endif	/* _SHA2_OBJECT_H_ */
