/*
 * Copyright (c) 2011 - 2012 Apple Inc. All rights reserved.
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

#ifndef _NETSMB_SMBIO_2_H_
#define _NETSMB_SMBIO_2_H_

#include <netsmb/smbio.h>

struct open_outparm_ex {
	uint64_t createTime;
	uint64_t accessTime;
	uint64_t writeTime;
	uint64_t changeTime;
	uint32_t attributes;
	uint64_t allocationSize;
	uint64_t fileSize;
	uint64_t fileInode;
	uint32_t maxAccessRights;
    SMBFID fid;
};

int smb_is_smb2(struct smb_ctx *ctx);

int smb2io_check_directory(struct smb_ctx *ctx, const void *path,
                           uint32_t flags, uint32_t *nt_error);
int smb2io_close_file(void *smbctx, SMBFID fid);
int smb2io_get_dfs_referral(struct smb_ctx *smbctx, CFStringRef dfs_referral_str,
                            uint16_t max_referral_level,
                            CFMutableDictionaryRef *out_referral_dict);
int smb2io_ntcreatex(void *smbctx, const char *path, const char *streamName,
                     struct open_inparms *inparms, 
                     struct open_outparm_ex *outparms, SMBFID *fid);
int smb2io_query_dir(void *smbctx, uint8_t file_info_class, uint8_t flags,
                     uint32_t file_index, SMBFID fid,
                     const char *name, uint32_t name_len,
                     char *rcv_output_buffer, uint32_t rcv_max_output_len,
                     uint32_t *rcv_output_len, uint32_t *query_dir_reply_len);
int smb2io_read(struct smb_ctx *smbctx, SMBFID fid, off_t offset, uint32_t count,
                char *dst, uint32_t *bytes_read);
int smb2io_transact(struct smb_ctx *smbctx, uint64_t *setup, int setupCnt, 
                    const char *pipeName, 
                    const uint8_t *sndPData, size_t sndPDataLen, 
                    const uint8_t *sndData, size_t sndDataLen, 
                    uint8_t *rcvPData, size_t *rcvPDataLen, 
                    uint8_t *rcvdData, size_t *rcvDataLen);
int smb2io_write(struct smb_ctx *smbctx, SMBFID fid, off_t offset, uint32_t count,
                 const char *src, uint32_t *bytes_written);

#endif
