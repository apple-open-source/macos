/*
 * Copyright (c) 2001-2013 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_EAPUTIL_H
#define _EAP8021X_EAPUTIL_H


/*
 * EAPUtil.h
 * - functions to return string values, and validate values for EAP
 */

#include <EAP8021X/EAP.h>
#include <stdbool.h>
#include <stdio.h>
#include <CoreFoundation/CFString.h>

int
EAPCodeValid(EAPCode code);

const char *
EAPCodeStr(EAPCode code);

const char *
EAPTypeStr(EAPType type);

bool
EAPPacketValid(EAPPacketRef eap_p, uint16_t pkt_length, FILE * f);

bool
EAPPacketIsValid(EAPPacketRef eap_p, uint16_t pkt_length,
		 CFMutableStringRef str);

/*
 * Function: EAPPacketCreate
 *
 * Purpose:
 *   Create an EAP packet, filling in the header information, and optionally,
 *   the type and its associated data.
 *
 *   If type is kEAPTypeInvalid, the packet size will be sizeof(EAPPacket),
 *   data and data_len are ignored.
 *
 *   If type is not kEAPTypeInvalid, the packet size will be 
 *   (sizeof(EAPRequestPacket) + data_len).  If data is not NULL, data_len
 *   bytes are copied into the type_data field.  If data is NULL, the caller
 *   fills in the data.
 *   
 *   If buf is not NULL, use it if it's big enough, otherwise
 *   malloc() a buffer that's large enough.
 *
 * Returns:
 *   A pointer to buf, if the buffer was used, otherwise a newly allocated
 *   buffer that must be released by calling free(), and also, the total
 *   length of the packet, in ret_size_p.
 *
 * Code example:
 *
 *   char 		buf[20];
 *   int 		identifier = 123;
 *   const char * 	identity = "user@domain";
 *   EAPPacketRef 	pkt;
 *   int		size;
 *
 *   pkt = EAPPacketCreate(buf, sizeof(buf), kEAPCodeResponse,
 *                         identifier, kEAPTypeIdentity, identity,
 *			   sizeof(identity), &size);
 *   send_packet(pkt);
 *   if (pkt != buf) {
 *       free(pkt);
 *   }
 */
EAPPacketRef
EAPPacketCreate(void * buf, int buf_size, 
		uint8_t code, int identifier, int type,
		const void * data, int data_len,
		int * ret_size_p);

#endif /* _EAP8021X_EAPUTIL_H */

