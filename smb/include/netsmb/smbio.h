/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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

#ifndef __NETSMB_SMBIO_H_INCLUDED__
#define __NETSMB_SMBIO_H_INCLUDED__

#include <stdint.h>
#include <sys/types.h>
#include <netsmb/smb_lib.h>

/* Perform a SMB_READ or SMB_READX.
 *
 * Return value is -errno if < 0, otherwise the received byte count.
 */
ssize_t
smbio_read(
	void *		smbctx,
	int		fid,
	uint8_t *	buf,
	size_t		buflen);

/* Perform a TRANSACT_NAMES_PIPE.
 *
 * Return value is -errno if < 0, otherwise the received byte count.
 */
ssize_t
smbio_transact(
	void *		smbctx,
	int		fid,	    /* pipe file ID */
	const uint8_t *	out_buf,    /* data to send */
	size_t		out_len,
	uint8_t *	in_buf,	    /* buffer for response */
	size_t		in_len);

/* Open a SMB names pipe. Returns 0 on success, otherwise -errno. */
int
smbio_open_pipe(
	void *		smbctx,
	const char *	pipename, /* without the leading \\ */
	int *		fid);

#endif /* __NETSMB_SMBIO_H_INCLUDED__ */

/* vim: set ts=4 et tw=79 */
