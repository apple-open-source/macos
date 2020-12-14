/*
 * Copyright (c) 2009 - 2010 Apple Inc. All rights reserved.
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

int is_memberd_tempuuid(const guid_t *uuidp);
void smbfs_clear_acl_cache(struct smbnode *np);
int smbfs_getsecurity(struct smb_share	*share, struct smbnode *np, 
					  struct vnode_attr *vap, vfs_context_t context);
int smbfs_setsecurity(struct smb_share *share, vnode_t vp, struct vnode_attr *vap, 
					  vfs_context_t context);
void smb_get_sid_list(struct smb_share *share, struct smbmount *smp, struct mdchain *mdp, 
					  uint32_t ntwrk_sids_cnt, uint32_t ntwrk_sid_size);
uint32_t smbfs_get_maximum_access(struct smb_share *share, vnode_t vp, vfs_context_t context);
int smbfs_compose_create_acl(struct vnode_attr *vap, struct vnode_attr *svrva, 
							 kauth_acl_t *savedacl);
int smbfs_is_sid_known(ntsid_t *sid);
int smbfs_set_ace_modes(struct smb_share *share, struct smbnode *np, uint64_t vamode, vfs_context_t context);
