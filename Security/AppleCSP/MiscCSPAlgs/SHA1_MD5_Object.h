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
 * SHA1_MD5_Object.h - SHA1, MD5 digest objects 
 *
 * Created 2/19/2001 by dmitch.
 */

#ifndef	_SHA1_MD5_OBJECT_H_
#define _SHA1_MD5_OBJECT_H_

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>
#include <AppleCSP/DigestObject.h>
#include <MiscCSPAlgs/MD5.h>
#include <CryptKit/SHA1_priv.h>

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
	virtual size_t digestSizeInBytes() const;
private:
	SHS_INFO 		mCtx;
	UInt8 			mBuffer[SHS_BLOCKSIZE];
	size_t 			mBufferCount;

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
	virtual size_t digestSizeInBytes() const;
private:
	MD5Context mCtx;
};

#endif	/* _SHA1_MD5_OBJECT_H_ */
