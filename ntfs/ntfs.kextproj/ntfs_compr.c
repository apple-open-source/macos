/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
/*	$NetBSD: ntfs_compr.c,v 1.3 1999/07/26 14:02:31 jdolecek Exp $	*/

/*-
 * Copyright (c) 1998, 1999 Semen Ustimenko
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/fs/ntfs/ntfs_compr.c,v 1.13 2001/11/26 23:45:12 jhb Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <sys/systm.h>
#include <string.h>
#include "ntfs.h"
#include "ntfs_compr.h"

#define GET_UINT16(addr)	(*((u_int16_t *)(addr)))

static int				/* Return: amount of compressed data consumed */
ntfs_uncompblock(
	u_int8_t * buf,		/* Destination: uncompressed */
	u_int8_t * cbuf)	/* Source: compressed */
{
	u_int32_t       ctag;
	int             len, dshift, lmask;
	int             blen, boff;
	int             i, j;
	int             pos, cpos;

	len = le16toh(GET_UINT16(cbuf)) & 0xFFF;
	dprintf(("ntfs_uncompblock: block length: %d + 3, 0x%x,0x%04x\n",
	    len, len, le16toh(GET_UINT16(cbuf))));

	if (!(le16toh(GET_UINT16(cbuf)) & 0x8000)) {
		if ((len + 1) != NTFS_COMPBLOCK_SIZE) {
			dprintf(("ntfs_uncompblock: len: %x instead of %d\n",
			    len, 0xfff));
		}
		memcpy(buf, cbuf + 2, len + 1);
		bzero(buf + len + 1, NTFS_COMPBLOCK_SIZE - 1 - len);
		return len + 3;
	}
	cpos = 2;
	pos = 0;
	while ((cpos < len + 3) && (pos < NTFS_COMPBLOCK_SIZE)) {
		ctag = cbuf[cpos++];
		for (i = 0; (i < 8) && (pos < NTFS_COMPBLOCK_SIZE); i++) {
			if (ctag & 1) {
				for (j = pos - 1, lmask = 0xFFF, dshift = 12;
				     j >= 0x10; j >>= 1) {
					dshift--;
					lmask >>= 1;
				}
				boff = -1 - (le16toh(GET_UINT16(cbuf + cpos)) >> dshift);
				blen = 3 + (le16toh(GET_UINT16(cbuf + cpos)) & lmask);
				for (j = 0; (j < blen) && (pos < NTFS_COMPBLOCK_SIZE); j++) {
					buf[pos] = buf[pos + boff];
					pos++;
				}
				cpos += 2;
			} else {
				buf[pos++] = cbuf[cpos++];
			}
			ctag >>= 1;
		}
	}
	return len + 3;
}

__private_extern__
int
ntfs_uncompunit(
	struct ntfsmount * ntmp,
	u_int8_t * uup,		/* Destination: uncompressed data */
	u_int8_t * cup,		/* Source: compressed data */
	size_t comp_unit_size)
{
	int             i;
	int             off = 0;
	int             new;

	for (i = 0; i * NTFS_COMPBLOCK_SIZE < comp_unit_size; i++) {
		new = ntfs_uncompblock(uup + i * NTFS_COMPBLOCK_SIZE, cup + off);
		if (new == 0)
			return (EINVAL);
		off += new;
	}
	return (0);
}
