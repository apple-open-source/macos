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

#ifndef _NETSMB_SMB_FID_H_
#define	_NETSMB_SMB_FID_H_

#include <netsmb/smb_2.h>

#define	smb_fid_table_lock(share)	(lck_mtx_lock(&(share)->ss_fid_lock))
#define	smb_fid_table_unlock(share)	(lck_mtx_unlock(&(share)->ss_fid_lock))

LIST_HEAD(fid_list_head, fid_node_t);

// An element in the global fid hash table
typedef struct fid_node_t
{
	SMBFID  fid;
	SMB2FID smb2_fid;
	LIST_ENTRY(fid_node_t) link;
	
} SMB_FID_NODE;

typedef struct fid_hash_table_slot
{
	struct fid_list_head fid_list;
	
} FID_HASH_TABLE_SLOT;

// Global fid hash table
#define SMB_FID_TABLE_SIZE 4096
#define SMB_FID_TABLE_MASK 0x0000000000000fff

void smb_fid_delete_all(struct smb_share *share);
int smb_fid_get_kernel_fid(struct smb_share *share, SMBFID fid, int remove_fid,
                           SMB2FID *smb2_fid);
int smb_fid_get_user_fid(struct smb_share *share, SMB2FID smb2_fid, 
                         SMBFID *fid);
uint64_t smb_fid_count_all(struct smb_share *share);

#endif
