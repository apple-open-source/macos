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
	File:		sslDigests.h

	Contains:	HashReference declarations

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#ifndef	_SSL_DIGESTS_H_
#define _SSL_DIGESTS_H_	1

#include "cryptType.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * These numbers show up all over the place...might as well hard code 'em once.
 */
#define SSL_MD5_DIGEST_LEN	16
#define SSL_SHA1_DIGEST_LEN	20
#define SSL_MAX_DIGEST_LEN	20

extern const UInt8 SSLMACPad1[], SSLMACPad2[];

extern const HashReference SSLHashNull;
extern const HashReference SSLHashMD5;
extern const HashReference SSLHashSHA1;

extern OSStatus CloneHashState(
	const HashReference *ref, 
	const SSLBuffer *state, 
	SSLBuffer *newState, 
	SSLContext *ctx);
extern OSStatus ReadyHash(
	const HashReference *ref, 
	SSLBuffer *state, 
	SSLContext *ctx);
extern OSStatus CloseHash(
	const HashReference *ref, 
	SSLBuffer *state, 
	SSLContext *ctx);

#ifdef __cplusplus
}
#endif

#endif	/* _SSL_DIGESTS_H_ */
