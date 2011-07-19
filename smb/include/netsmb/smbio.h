/*
 * Copyright (C) 2008 - 2010 Apple Inc. All rights reserved.
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

struct open_inparms {
	uint32_t rights;
	uint32_t shareMode;
	uint32_t disp;
	uint32_t attrs;
	uint64_t allocSize;
	uint32_t createOptions;
};

struct open_outparm {
	uint64_t createTime;
	uint64_t accessTime;
	uint64_t writeTime;
	uint64_t changeTime;
	uint32_t attributes;
	uint64_t allocationSize;
	uint64_t fileSize;
	uint8_t volumeGID[16];
	uint64_t fileInode;
	uint32_t maxAccessRights;
	uint32_t maxGuessAccessRights;
};

/*  Return value is -errno if < 0, otherwise the received byte count. */
ssize_t smbio_read(void *smbctx, int fid, uint8_t *buf, size_t bufSize);

/* 
 * Perform a smb transaction call
 *
 * Return zero if no error or the appropriate errno.
 */
int smbio_transact(void *smbctx, uint16_t *setup, int setupCnt, const char *name, 
				   const uint8_t *sndPData, size_t sndPDataLen, 
				   const uint8_t *sndData, size_t sndDataLen, 
				   uint8_t *rcvPData, size_t *rcvPDataLen, 
				   uint8_t *rcvdData, size_t *rcvDataLen);

/* Open a SMB names pipe. Returns 0 on success, otherwise -errno. */
int smbio_open_pipe(void * smbctx, const char *	pipename, int *fid);
int smbio_close_file(void *ctx, int fid);
int smbio_check_directory(struct smb_ctx *ctx, const void *path, 
						  uint32_t /* flags2 */, uint32_t */* nt_error */);
int smbio_ntcreatex(void *smbctx, const char *path, const char *streamName, 
					struct open_inparms *inparms, struct open_outparm *outparms, 
					int *fid);
#endif /* __NETSMB_SMBIO_H_INCLUDED__ */

