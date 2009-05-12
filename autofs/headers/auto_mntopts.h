/*
 * Copyright (c) 1999-2009 Apple Inc. All rights reserved.
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
 * Options of interest only in fstab entries.
 */
#define FSTAB_MNT_NET	0x00000001
#define MOPT_NET	{ "net",	0, FSTAB_MNT_NET, 1 }

/*
 * NFS-specific mount options.
 */
#define NFS_MNT_PORT	0x00000001
#define NFS_MNT_VERS	0x00000002
#define NFS_MNT_PROTO	0x00000004
#define NFS_MNT_TCP	0x00000008
#define NFS_MNT_UDP	0x00000010

#define MOPT_VERS		{ "vers",	0, NFS_MNT_VERS, 1 }
#define MOPT_NFS \
	{ "port",	0, NFS_MNT_PORT, 1 },	\
	MOPT_VERS,				\
	{ "proto",	0, NFS_MNT_PROTO, 1 },	\
	{ "tcp",	0, NFS_MNT_TCP, 1 },	\
	{ "udp",	0, NFS_MNT_UDP, 1 },	\
	{ NULL,		0, 0, 0 }
