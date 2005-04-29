#ifndef _S_HOST_IDENTIFIER_H
#define _S_HOST_IDENTIFIER_H
/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * host_identifier.h
 * - the identifier string format is:
 *   		%x,%x:%x...:%x
 * - the byte before the slash '/' is the arp hardware type (or 0),
 *   the bytes following the slash are the hardware address bytes, or
 *   the opaque client identifier bytes
 * - each returns NULL if a failure occurred, a non-NULL pointer otherwise;
 *   the non-NULL pointer may be freed using free()
 * - arp hardware types are defined in the "Assigned Numbers RFC",
 *   currently this is RFC 1700 (look for "hrd")
 *   
 */

/*
 * Modification History
 *
 * June 4, 1998 Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

u_char *
identifierToString(const u_char type, const void * identifier, int len);

u_char *
identifierToStringWithBuffer(u_char type, const void * identifier, int len,
			     u_char * buf, int buf_len);

void *
identifierFromString(const u_char * str, u_char * type, int * len);

#endif _S_HOST_IDENTIFIER_H
