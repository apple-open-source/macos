/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/* NKENKEMgr.h created by justin on Fri 19-Mar-1999 */

#ifndef _NKEMGR_H_
#define _NKEMGR_H_

#define PF_NKE 253	/* TEMP - move to socket.h */
#define AF_NKE PF_NKE	/* If it ain't broke... */

/*
 * sn_handle is a 32-bit "signature", obtained by NKE developers from
 *  Apple Computer.
 */
struct sockaddr_nke
{	u_char	sn_len;
	u_char	sn_family;	/* AF_NKE */
	unsigned int sn_handle;
	char pad[10];
};

#define NKEPROTO_SOCKET	1	/* Access to 'socket' NKE */
#define NKEPROTO_DLINK	2	/* Access to Data Link NKE */

#endif /* _NKEMGR_H_ */
