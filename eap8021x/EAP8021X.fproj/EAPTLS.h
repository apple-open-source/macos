/*
 * Copyright (c) 2002-2013 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_EAPTLS_H
#define _EAP8021X_EAPTLS_H

/*
 * EAPTLS.h
 * - definitions for EAP-TLS
 */

/* 
 * Modification History
 *
 * August 26, 2002	Dieter Siegmund (dieter@apple)
 * - created
 */

#include <stdint.h>

typedef struct {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire request/response */
    uint8_t		type;
    uint8_t		flags;
    uint8_t		tls_data[0];
} EAPTLSPacket, *EAPTLSPacketRef, EAPTLSFragment, *EAPTLSFragmentRef;

typedef struct {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire request/response */
    uint8_t		type;
    uint8_t		flags;
    uint8_t		tls_message_length[4]; /* if flags.L == 1 */
    uint8_t		tls_data[0];
} EAPTLSLengthIncludedPacket, *EAPTLSLengthIncludedPacketRef;

typedef enum {
    kEAPTLSPacketFlagsLengthIncluded	= 0x80,
    kEAPTLSPacketFlagsMoreFragments 	= 0x40,
    kEAPTLSPacketFlagsStart 		= 0x20,
} EAPTLSPacketFlags;

uint32_t
EAPTLSLengthIncludedPacketGetMessageLength(EAPTLSLengthIncludedPacketRef pkt);

void
EAPTLSLengthIncludedPacketSetMessageLength(EAPTLSLengthIncludedPacketRef pkt, 
					   uint32_t length);
#endif /* _EAP8021X_EAPTLS_H */
