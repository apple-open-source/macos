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
/* NKEMgrvar.h created by justin on Fri 19-Mar-1999 */
#ifndef _NKEMGRVAR_H_
#define _NKEMGRVAR_H_

struct skn_pcb
{	struct socket *sp_so;
	struct sockaddr_nke sp_sa;
	struct NFDescriptor *sp_nfd;
};

struct dln_pcb
{	struct socket *dp_so;
	struct sockaddr_nke dp_sa;
	unsigned long dp_id;
	struct NFDescriptor *dp_nfd;
};

#define NO_FILTER (0xffffffff)

#ifdef KERNEL
#endif

#endif /* _NKEMGRVAR_H_ */
