
#ifndef _S_NDRV_SOCKET_H
#define _S_NDRV_SOCKET_H

/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
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
 * ndrv_socket.h
 * - wrapper for allocating an NDRV socket
 */

/* 
 * Modification History
 *
 * October 26, 2001	Dieter Siegmund (dieter@apple)
 * - created
 */

#include <sys/types.h>

int	ndrv_socket(const char * ifname);
int	ndrv_socket_bind(int s, u_int32_t family,
			 const u_int16_t * ether_types, int ether_types_count);
int	ndrv_socket_add_multicast(int s, const struct sockaddr_dl * dl_p);
int	ndrv_socket_remove_multicast(int s, const struct sockaddr_dl * dl_p);

#endif _S_NDRV_SOCKET_H

