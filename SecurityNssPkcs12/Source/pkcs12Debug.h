/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */
/*
 * pkcs12Debug.h
 */
 
#ifndef	_PKCS12_DEBUG_H_
#define _PKCS12_DEBUG_H_

#include <Security/debugging.h>
#include <Security/cssmerrno.h>


#ifdef	NDEBUG
/* this actually compiles to nothing */
#define p12ErrorLog(args...)		debug("p12Error", ## args)
#define p12LogCssmError(op, err)
#else
#define p12ErrorLog(args...)		printf(args)
#define p12LogCssmError(op, err)	cssmPerror(op, err)
#endif

/* individual debug loggers */
#define p12DecodeLog(args...)		debug("p12Decode", ## args)
#define p12EncodeLog(args...)		debug("p12Encode", ## args)
#define p12CryptoLog(args...)		debug("p12Crypto", ## args)

#endif	/* _PKCS12_TEMPLATES_H_ */

