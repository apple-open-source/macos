/*
 * Copyright (c) 2003-2004,2011,2014 Apple Inc. All Rights Reserved.
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
 * pkcs12Debug.h
 */
 
#ifndef	_PKCS12_DEBUG_H_
#define _PKCS12_DEBUG_H_

#include <security_utilities/debugging.h>
#include <Security/cssmapple.h>

#ifdef	NDEBUG
/* this actually compiles to nothing */
#define p12ErrorLog(args...)		secdebug("p12Error", ## args)
#define p12LogCssmError(op, err)
#else
#define p12ErrorLog(args...)		printf(args)
#define p12LogCssmError(op, err)	cssmPerror(op, err)
#endif

/* individual debug loggers */
#define p12DecodeLog(args...)		secdebug("p12Decode", ## args)
#define p12EncodeLog(args...)		secdebug("p12Encode", ## args)
#define p12CryptoLog(args...)		secdebug("p12Crypto", ## args)

#endif	/* _PKCS12_TEMPLATES_H_ */

