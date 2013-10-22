/*
 * Copyright (c) 2000-2001,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
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
 * sslUtils.h - - Misc. OS independant SSL utility functions
 */

#ifndef _SSLUTILS_H_
#define _SSLUTILS_H_ 1

#include "sslTypes.h"

#ifdef	__cplusplus
extern "C" {
#endif

uint32_t SSLDecodeInt(
	const uint8_t *     p,
	size_t              length);
uint8_t *SSLEncodeInt(
	uint8_t             *p,
	size_t              value,
	size_t length);

/* Same, but the value to encode is a size_t */
size_t SSLDecodeSize(
      const uint8_t *     p,
      size_t              length);
uint8_t *SSLEncodeSize(
      uint8_t             *p,
      size_t              value,
      size_t              length);

/* Same but for 64bits int */
uint8_t* SSLEncodeUInt64(
	uint8_t             *p,
	sslUint64           value);
void IncrementUInt64(
	sslUint64 			*v);
void SSLDecodeUInt64(
    const uint8_t *p,
    size_t length,
    sslUint64 *v);

static inline
int SSLHandshakeHeaderSize(SSLRecord *rec)
{
    if(rec->protocolVersion==DTLS_Version_1_0)
        return 12;
    else
        return 4;
}

#ifndef NDEBUG
extern const char *protocolVersStr(SSLProtocolVersion prot);
#endif
 
#ifdef	__cplusplus
}
#endif

#endif
