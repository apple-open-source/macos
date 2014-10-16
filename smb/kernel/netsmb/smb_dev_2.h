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

#ifndef _NETSMB_SMB_DEV_2_H_
#define _NETSMB_SMB_DEV_2_H_

/* 
 * The SMB2IOC_CHECK_DIR ioctl is a read/write ioctl. So we use smb2ioc_check_dir
 * structure to pass information from the user land to the kernel and vis-versa  
 */
struct smb2ioc_check_dir {
	uint32_t	ioc_version;
	uint32_t	ioc_path_len;
	SMB_IOC_POINTER(const char *, path); /* local path to item */
    /* return values */
	uint32_t    ioc_ret_errno;
	uint32_t    ioc_ret_ntstatus;
};

/*
 * The SMB2IOC_CLOSE ioctl is a read/write ioctl. So we use smb2ioc_close
 * structure to pass information from the user land to the kernel and vis-versa  
 */
struct smb2ioc_close {
	uint32_t	ioc_version;
    uint32_t    ioc_flags;
    SMBFID     ioc_fid;
    /* return values */
	uint32_t    ioc_ret_ntstatus;
    uint32_t    ioc_ret_attributes;
	uint64_t    ioc_ret_create_time;
	uint64_t    ioc_ret_access_time;
	uint64_t    ioc_ret_write_time;
	uint64_t    ioc_ret_change_time;
    uint64_t    ioc_ret_alloc_size;
	uint64_t    ioc_ret_eof;
};

/* 
 * The SMB2IOC_CREATE ioctl is a read/write ioctl. So we use smb2ioc_create
 * structure to pass information from the user land to the kernel and vis-versa  
 */
struct smb2ioc_create {
	uint32_t	ioc_version;
	uint32_t	ioc_name_len;
	SMB_IOC_POINTER(const char *, name);
    uint8_t     ioc_oplock_level;
    uint8_t     pad[3];
	uint32_t    ioc_impersonate_level;
    uint32_t    ioc_desired_access;
    uint32_t    ioc_file_attributes;
    uint32_t    ioc_share_access;
	uint32_t    ioc_disposition;
    uint32_t    ioc_create_options;
    uint32_t    pad2;
    /* return values */
	uint32_t    ioc_ret_ntstatus;
	uint32_t    ioc_ret_attributes;
    uint8_t     ioc_ret_oplock_level;
    uint8_t     ioc_ret_pad[3];
    uint32_t    ioc_ret_create_action;
	uint64_t    ioc_ret_create_time;
	uint64_t    ioc_ret_access_time;
	uint64_t    ioc_ret_write_time;
	uint64_t    ioc_ret_change_time;
	uint64_t    ioc_ret_alloc_size;
	uint64_t    ioc_ret_eof;
    SMBFID      ioc_ret_fid;
    uint32_t    ioc_ret_max_access;
    /* %%% To Do - add contexts handling */
};

struct smb2ioc_get_dfs_referral {
	uint32_t	ioc_version;
	uint16_t	ioc_max_referral_level;
	uint16_t	pad;
	uint32_t	ioc_file_name_len;
	uint32_t	ioc_rcv_output_len;
	SMB_IOC_POINTER(void *, file_name);
	SMB_IOC_POINTER(void *, rcv_output);
    /* return values */
	uint32_t	ioc_ret_ntstatus;
	uint32_t    ioc_ret_output_len;
};

struct smb2ioc_ioctl {
	uint32_t	ioc_version;
	uint32_t	ioc_ctl_code;
	SMBFID		ioc_fid;
	uint32_t	ioc_snd_input_len;
	uint32_t	ioc_snd_output_len;
	uint32_t	ioc_rcv_input_len;
	uint32_t	ioc_rcv_output_len;
	SMB_IOC_POINTER(void *, snd_input);
	SMB_IOC_POINTER(void *, snd_output);
	SMB_IOC_POINTER(void *, rcv_input);
	SMB_IOC_POINTER(void *, rcv_output);
    /* return values */
	uint32_t	ioc_ret_ntstatus;
	uint32_t	ioc_ret_flags;
	uint32_t    ioc_ret_input_len;
	uint32_t    ioc_ret_output_len;
};

/* smb2ioc_query_dir is ONLY used for test tools */
struct smb2ioc_query_dir {
	uint32_t	ioc_version;
    uint8_t     ioc_file_info_class;
	uint8_t     ioc_flags;
    uint16_t    pad;
	uint32_t    ioc_file_index;
    uint32_t    ioc_rcv_output_len;
	SMBFID      ioc_fid;
    uint32_t    ioc_name_len;
    uint32_t    ioc_name_flags;    /* use UTF_SFM_CONVERSIONS or not */
	SMB_IOC_POINTER(const char *, name);
	SMB_IOC_POINTER(void *, rcv_output);
    /* return values */
	uint32_t	ioc_ret_ntstatus;
	uint32_t    ioc_ret_output_len;
};

/*
 * The SMB2IOC_READ/SMB2IOC_WRITE ioctl is a read/write ioctl. So we use 
 * smb2ioc_rw structure to pass information from the user land to the kernel 
 * and vis-versa  
 */
struct smb2ioc_rw {
	uint32_t	ioc_version;
	uint32_t	ioc_len;
	off_t		ioc_offset;
	uint32_t	ioc_remaining;
	uint32_t	ioc_write_flags;
	SMBFID		ioc_fid;
	SMB_IOC_POINTER(void *, base);
    /* return values */
	uint32_t    ioc_ret_ntstatus;
	uint32_t    ioc_ret_len;
};

/* SMBIOC_SHARE_PROPERTIES to pass information in struct smb_share to userland */
struct smbioc_share_properties {
	uint32_t    ioc_version;
    uint32_t    ioc_reserved;
	uint32_t    share_caps;
	uint32_t    share_flags;
    uint32_t    share_type;
	uint32_t    attributes;
};

/*
 * Device IOCTLs
 */
#define	SMB2IOC_CHECK_DIR		_IOWR('n', 118, struct smb2ioc_check_dir)
#define	SMB2IOC_CLOSE			_IOWR('n', 119, struct smb2ioc_close)
#define	SMB2IOC_CREATE			_IOWR('n', 120, struct smb2ioc_create)
#define	SMB2IOC_IOCTL			_IOWR('n', 121, struct smb2ioc_ioctl)
#define	SMB2IOC_READ			_IOWR('n', 122, struct smb2ioc_rw)
#define	SMB2IOC_WRITE			_IOWR('n', 123, struct smb2ioc_rw)
#define	SMB2IOC_GET_DFS_REFERRAL    _IOWR('n', 124, struct smb2ioc_get_dfs_referral)
#define SMBIOC_SHARE_PROPERTIES	_IOWR('n', 125, struct smbioc_share_properties)
#define	SMB2IOC_QUERY_DIR       _IOWR('n', 126, struct smb2ioc_query_dir)


#ifdef _KERNEL

int smb_usr_check_dir(struct smb_share *share, struct smb_vc *vcp,
                      struct smb2ioc_check_dir *check_dir_rq,
                      vfs_context_t context);
int smb_usr_close(struct smb_share *share,
                  struct smb2ioc_close *close_rq, 
                  vfs_context_t context);
int smb_usr_create(struct smb_share *share, 
                   struct smb2ioc_create *create_rq, 
                   vfs_context_t context);
int smb_usr_get_dfs_referral(struct smb_share *share, struct smb_vc *vcp,
                             struct smb2ioc_get_dfs_referral *get_dfs_refer_ioc,
                             vfs_context_t context);
int smb_usr_ioctl(struct smb_share *share, struct smb_vc *vcp,
                  struct smb2ioc_ioctl *ioctl_ioc, vfs_context_t context);
int smb_usr_query_dir(struct smb_share *share,
                      struct smb2ioc_query_dir *query_dir_ioc,
                      vfs_context_t context);
int smb_usr_read_write(struct smb_share *share,
                       u_long cmd, 
                       struct smb2ioc_rw *rw_ioc, 
                       vfs_context_t context);

#endif /* _KERNEL */

#endif
