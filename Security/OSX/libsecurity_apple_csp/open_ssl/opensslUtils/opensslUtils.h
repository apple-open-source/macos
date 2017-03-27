/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
 * appleUtils.h - Support for ssleay-derived crypto modules
 */
 
#ifndef	_OPENSSL_UTILS_H_
#define _OPENSSL_UTILS_H_

#include <openssl/opensslerr.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Trivial exception class associated with an openssl error.
 */
class openSslException
{
public:
	openSslException(
		int irtn,
		const char *op = NULL); 	
	~openSslException() 				{ }
	int irtn()	{ return mIrtn; }
private:
	int mIrtn;
};

/* Clear openssl error stack. */
void clearOpensslErrors();

unsigned long logSslErrInfo(const char *op);

void throwRsaDsa(
	const char *op);
	
/*
 * given an openssl-style error, throw appropriate CssmError.
 */
void throwOpensslErr(
	int irtn);


#ifdef	__cplusplus
}
#endif

#endif	/* _OPENSSL_UTILS_H_ */
