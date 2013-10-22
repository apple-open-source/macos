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

#ifndef _NETSMB_SMB_RQ_2_H_
#define	_NETSMB_SMB_RQ_2_H_


/* smb_rq sr_extflags values */
#define SMB2_REQUEST		0x0001	/* smb_rq is for SMB2 request */
#define SMB2_RESPONSE		0x0002	/* smb_rq received SMB2 response */
#define SMB2_REQ_SENT		0x0004	/* smb_rq is for SMB2 request */


/*
 * Note: Pad all structures to 8 byte boundaries
 */

/* 
 * smb2_create_rq flags 
 *
 * SMB2_CREATE_AAPL_RESOLVE_ID and SMB2_CREATE_DUR_HANDLE use the 
 * createp->create_contextp
 */
typedef enum _SMB2_CREATE_RQ_FLAGS
{
    SMB2_CREATE_DO_CREATE = 0x0001,
    SMB2_CREATE_IS_NAMED_STREAM = 0x0002,
    SMB2_CREATE_GET_MAX_ACCESS = 0x0004,
    SMB2_CREATE_NAME_IS_PATH = 0x0008,
    SMB2_CREATE_AAPL_QUERY = 0x0010,
    SMB2_CREATE_AAPL_RESOLVE_ID = 0x0020,
    SMB2_CREATE_DUR_HANDLE = 0x0040,
    SMB2_CREATE_DUR_HANDLE_RECONNECT = 0x0080
} _SMB2_CREATE_RQ_FLAGS;

/* smb2_cmpd_position flags */
typedef enum _SMB2_CMPD_POSITION_FLAGS
{
    SMB2_CMPD_FIRST = 0x0001,
    SMB2_CMPD_MIDDLE = 0x0002,
    SMB2_CMPD_LAST = 0x0004
} _SMB2_CMPD_POSITION_FLAGS;

struct smb2_change_notify_rq {
	uint32_t flags;
	uint32_t output_buffer_len;
	SMBFID fid;
 	uint32_t filter;
 	uint32_t pad;
    void *fn_callback;
    void *fn_callback_args;
    
    /* return values */
	uint32_t ret_ntstatus;
	uint32_t ret_buffer_len;
};

struct smb2_close_rq {
    struct smb_share *share;
    uint32_t flags;
    uint32_t pad;
    SMBFID fid;
    
    /* return values */
	uint32_t ret_ntstatus;
    uint32_t ret_attributes;
	uint64_t ret_create_time;
	uint64_t ret_access_time;
	uint64_t ret_write_time;
	uint64_t ret_change_time;
    uint64_t ret_alloc_size;
	uint64_t ret_eof;
};

struct smb2_create_ctx_resolve_id {
    uint64_t file_id;
    uint32_t *ret_errorp;
    char **ret_pathp;
};

struct smb2_create_rq {
    uint64_t flags;                 /* defined above */
    uint8_t oplock_level;
    uint8_t pad[3];
	uint32_t impersonate_level;
    uint32_t desired_access;
    uint32_t file_attributes;
    uint32_t share_access;
	uint32_t disposition;
    uint32_t create_options;
    uint32_t name_len;
    uint32_t strm_name_len;         /* stream name len */
    uint32_t pad2;
    struct smbnode *dnp;
    char *namep;
    char *strm_namep;               /* stream name */
    void *create_contextp;          /* used for various create contexts */
    
    /* return values */
	uint32_t ret_ntstatus;
	uint32_t ret_attributes;
    uint8_t ret_oplock_level;
    uint8_t ret_pad[3];
    uint32_t ret_create_action;
	uint64_t ret_create_time;
	uint64_t ret_access_time;
	uint64_t ret_write_time;
	uint64_t ret_change_time;
	uint64_t ret_alloc_size;
	uint64_t ret_eof;
    SMBFID ret_fid;
	uint32_t ret_max_access;
	uint32_t ret_pad2;
};

struct smb2_get_dfs_referral {
	uint16_t max_referral_level;
	uint16_t pad;
	uint32_t file_name_len;
	char *file_namep;
};

/*
 * The SRV_COPYCHUNK_COPY packet is sent in an SMB2 IOCTL Request
 * by the client to initiate a server-side copy of data. It is
 * set as the contents of the input data buffer.
 */
#define SMB2_COPYCHUNK_ARR_SIZE 16
#define SMB2_COPYCHUNK_MAX_CHUNK_LEN 1048576    // 1 MB
#define SMB2_RESUME_KEY_LEN 24

struct smb2_copychunk {
    uint8_t     source_key[SMB2_RESUME_KEY_LEN];
    uint32_t    chunk_count;
    uint32_t    reserved;
}__attribute__((__packed__));

/*
 * SRV_COPYCHUNK_COPY: struct to describe
 * an individual data range to copy.
 */
struct smb2_copychunk_chunk {
    uint64_t    source_offset;
    uint64_t    target_offset;
    uint32_t    length;
    uint32_t    reserved;
}__attribute__((__packed__));

/*
 * SRV_COPYCHUNK_COPY: struct to describe the results of a SRC_COPYCHUNK_COPY
 * request.
 *
 * chunks_written: number of chunks successfully written
 *     (only valid if status != STATUS_INVALID_PARAMETER).
 *
 * chunk_bytes_written (depends on status from server):
 *     status != STATUS_INVALID_PARAMETER: number of bytes written
 *     the last chunk that did not successfully process (if a partial
 *     write occurred).
 *
 *     status == STATUS_INVALID_PARAMETER: indicates maximum number of
 *     bytes the server will allow to be written in a single chunk.
 *
 * total_bytes_written (depends on status from server):
 *     status != STATUS_INVALID_PARAMETER: the total number of bytes
 *     written in the server-side copy operation.
 *
 *     status == STATUS_INVALID_PARAMATER: maximum number of bytes the
 *     server will accept to copy in a single request.
 */
struct smb2_copychunk_result {
    uint32_t    chunks_written;
    uint32_t    chunk_bytes_written;
    uint32_t    total_bytes_written;
}__attribute__((__packed__));

struct smb2_ioctl_rq {
    struct smb_share *share;
    uint32_t ctl_code;
	uint32_t pad;
	SMBFID fid;
	uint32_t snd_input_len;
	uint32_t snd_output_len;
	uint32_t rcv_input_len;
	uint32_t rcv_output_len;
    
    /* uio buffers used for ioctls from user space */
    uio_t snd_input_uio;
    uio_t snd_output_uio;
    uio_t rcv_input_uio;
    uio_t rcv_output_uio;
    
    /* data ptrs used for ioctls from kernel space */
    uint8_t *snd_input_buffer;
    uint8_t *rcv_output_buffer;
    
    /* return values */
	uint32_t ret_ntstatus;
	uint32_t ret_flags;
	uint32_t ret_input_len;
	uint32_t ret_output_len;
};

struct smb2_query_dir_rq {
	uint8_t file_info_class;
	uint8_t flags;
    uint8_t pad[6];
	uint32_t file_index;
    uint32_t output_buffer_len;
	SMBFID fid;
    uint32_t name_len;
    uint32_t name_flags;    /* use UTF_SFM_CONVERSIONS or not */
    struct smbnode *dnp;
    char *namep;
    
    /* return values */
	struct smb_rq *ret_rqp;
	uint32_t ret_ntstatus;
	uint32_t ret_buffer_len;
};

struct smb2_query_info_rq {
    uint8_t info_type; 
    uint8_t file_info_class;
    uint8_t pad[6];
	uint32_t add_info;
	uint32_t flags;
	uint32_t output_buffer_len;
	uint32_t input_buffer_len;
    uint8_t *output_buffer;
    uint8_t *input_buffer;
	SMBFID fid;
    
    /* return values */
	uint32_t ret_ntstatus;
	uint32_t ret_buffer_len;
};

struct smb2_rw_rq {
	uint32_t remaining;
	uint32_t write_flags;
	SMBFID fid;
    uio_t auio;
    user_ssize_t io_len;
    
    /* return values */
	uint32_t ret_ntstatus;
	uint32_t ret_len;
};

struct smb2_set_info_file_basic_info {
    uint64_t create_time; 
    uint64_t access_time; 
    uint64_t write_time; 
    uint64_t change_time; 
	uint32_t attributes;
    uint32_t pad[4];
};

struct smb2_set_info_file_rename_info {
    uint8_t replace_if_exists;
    uint8_t pad[3];
    uint32_t tname_len;
    struct smbnode *tdnp;
    char *tnamep;
};

struct smb2_set_info_security {
    uint32_t security_attrs;
    uint16_t control_flags;
    uint16_t pad;
    struct ntsid *owner;
    struct ntsid *group;
    struct ntacl *sacl;
    struct ntacl *dacl;
};

struct smb2_set_info_rq {
    uint8_t info_type; 
    uint8_t file_info_class;
    uint8_t pad[2];
	uint32_t add_info;
	SMBFID fid;
    uint8_t *input_buffer;
    
    /* return values */
	uint32_t ret_ntstatus;
    uint8_t ret_pad[4];
};


int smb2_rq_alloc(struct smb_connobj *obj, u_char cmd, uint32_t *rq_len, 
                  vfs_context_t context, struct smb_rq **rqpp);
void smb_rq_bend32(struct smb_rq *rqp);
void smb2_rq_bstart(struct smb_rq *rqp, uint16_t *len_ptr);
void smb2_rq_bstart32(struct smb_rq *rqp, uint32_t *len_ptr);
void smb2_rq_align8(struct smb_rq *rqp);
int smb2_rq_credit_increment(struct smb_rq *rqp);
uint32_t smb2_rq_credit_check(struct smb_rq *rqp, uint32_t len);
void smb2_rq_credit_start(struct smb_vc *vcp, uint16_t credits);
int smb2_rq_message_id_increment(struct smb_rq *rqp);
int smb2_rq_next_command(struct smb_rq *rqp, size_t *next_cmd_offset,
                         struct mdchain *mdp);
uint32_t smb2_rq_length(struct smb_rq *rqp);
uint32_t smb2_rq_parse_header(struct smb_rq *rqp, struct mdchain **mdp);
int smb_rq_getenv(struct smb_connobj *obj, struct smb_vc **vcp, 
                  struct smb_share **share);
int smb2_rq_update_cmpd_hdr(struct smb_rq *rqp, uint32_t position_flag);


#endif
