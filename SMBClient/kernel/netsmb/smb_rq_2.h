/*
 * Copyright (c) 2011 - 2023 Apple Inc. All rights reserved.
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


typedef enum _SMB2_CMD_FLAGS
{
    SMB2_CMD_NO_BLOCK      = 0x80000000  /* Dont wait for credits */
} _SMB2_CMD_FLAGS;


/* smb_rq sr_extflags values */
#define SMB2_REQUEST		        0x0001	/* smb_rq is for SMB 2/3 request */
#define SMB2_RESPONSE		        0x0002	/* smb_rq received SMB 2/3 response */
#define SMB2_REQ_SENT		        0x0004	/* smb_rq is for SMB 2/3 request */
#define SMB2_NON_IDEMPOTENT         0x0008  /* smb_rq is a non replayable request */
#define SMB2_NO_COMPRESS_WRITE      0x0010  /* Compressed writes not allowed */
#define SMB2_FAILED_COMPRESS_WRITE  0x0020  /* Failed to compress this write */
#define SMB2_HDR_PREPARSED          0x0040  /* SMB2 header has been parsed already */
#define SMB2_REQ_NO_BLOCK	    0x80000000	/* dont block waiting for credits */


/*
 * Note: Pad all structures to 8 byte boundaries
 */

/* 
 * smb2_create_rq flags 
 *
 * SMB2_CREATE_AAPL_RESOLVE_ID,
 * SMB2_CREATE_DUR_HANDLE, SMB2_CREATE_DUR_HANDLE_RECONNECT use the
 * createp->create_contextp
 */
typedef enum _SMB2_CREATE_RQ_FLAGS
{
    SMB2_CREATE_DO_CREATE            = 0x0001, /* We are creating a new item */
    SMB2_CREATE_IS_NAMED_STREAM      = 0x0002, /* Dealing with a named stream */
    SMB2_CREATE_GET_MAX_ACCESS       = 0x0004, /* Add Max Access Create Context */
    SMB2_CREATE_NAME_IS_PATH         = 0x0008, /* Name field is already converted to a path format */
    SMB2_CREATE_AAPL_QUERY           = 0x0010, /* Add AAPL Create Context */
    SMB2_CREATE_AAPL_RESOLVE_ID      = 0x0020, /* Add AAPL Resolve ID Create Context */
    SMB2_CREATE_DUR_HANDLE           = 0x0040, /* Get a durable handle */
    SMB2_CREATE_DUR_HANDLE_RECONNECT = 0x0080, /* Reopen file that has durable handle */
    SMB2_CREATE_ASSUME_DELETE        = 0x0100, /* Assume delete access in Max Access */
    SMB2_CREATE_HANDLE_RECONNECT     = 0x0200, /* Reopen file that does not have a durable handle */
    SMB2_CREATE_DIR_LEASE            = 0x0400, /* Request a Dir Lease */
    SMB2_CREATE_SET_DFS_FLAG         = 0x0800, /* Set the DFS Flag in the header */
    SMB2_CREATE_REPLAY_FLAG          = 0x1000, /* This is a retransmit, ie, set the SMB2_FLAGS_REPLAY_OPERATIONS flag */
    SMB2_CREATE_ADD_TIME_WARP        = 0x2000, /* Add Time Warp Context */
    SMB2_CREATE_QUERY_DISK_ID        = 0x4000, /* Add Query on Disk ID Create Context */
    SMB2_CREATE_FILE_LEASE           = 0x8000  /* Request a file Lease */
} _SMB2_CREATE_RQ_FLAGS;

/* smb2_cmpd_position flags */
typedef enum _SMB2_CMPD_POSITION_FLAGS
{
    SMB2_CMPD_FIRST  = 0x0001,
    SMB2_CMPD_MIDDLE = 0x0002,
    SMB2_CMPD_LAST   = 0x0004
} _SMB2_CMPD_POSITION_FLAGS;

typedef enum smb_mc_control {
    SMB2_MC_REPLAY_FLAG         = SMB2_CREATE_REPLAY_FLAG, /* This is a retransmit, ie, set the SMB2_FLAGS_REPLAY_OPERATIONS flag */
} _SMB2_MC_CONTROL;

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
    uint32_t add_flush;
    SMBFID fid;
    enum smb_mc_control mc_flags;
    
    /* return values */
    uint32_t ret_ntstatus;
    uint32_t ret_attributes;
	uint64_t ret_create_time;
	uint64_t ret_access_time;
	uint64_t ret_write_time;
	uint64_t ret_change_time;
    uint64_t ret_alloc_size;
	uint64_t ret_eof;
    uint16_t ret_flags;
    uint8_t pad2[6];
};

struct smb2_create_ctx_resolve_id {
    uint64_t file_id;
    uint32_t *ret_errorp;
    char **ret_pathp;
    size_t ret_pathp_allocsize; /* *ret_pathp alloc size, required when freeing *ret_pathp */
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
    size_t strm_name_allocsize;     /* strm_namep alloc size, required when freeing strm_namep */
    void *create_contextp;          /* used for various create contexts */
	struct timespec req_time;       /* time this create was done */
    enum smb_mc_control mc_flags;

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
    uint64_t ret_disk_file_id;
    uint64_t ret_volume_id;
};

struct smb2_get_dfs_referral {
	uint16_t max_referral_level;
	uint16_t pad;
	uint32_t file_name_len;
	char *file_namep;
};

/*
 * The SRV_COPYCHUNK_COPY packet is sent in an SMB 2/3 IOCTL Request
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

struct smb2_flush_rq {
    struct smb_share *share;
    uint64_t full_sync;
    SMBFID fid;
    enum smb_mc_control mc_flags;
    
    /* return values */
    uint32_t ret_ntstatus;
    uint32_t pad;
};

struct smb2_ioctl_rq {
    struct smb_share *share;
    uint32_t ctl_code;
	uint32_t pad;
	SMBFID fid;
	uint32_t snd_input_len;
	uint32_t snd_output_len;
	uint32_t rcv_input_len;
	uint32_t rcv_output_len;
    size_t   rcv_output_allocsize;
    enum smb_mc_control mc_flags;

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
    size_t name_allocsize;  /* namep alloc size, required when freeing namep */
    enum smb_mc_control mc_flags;

    /* uio buffers used for ioctls from user space */
    uio_t rcv_output_uio;
    
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
    enum smb_mc_control mc_flags;

    /* return values */
	uint32_t ret_ntstatus;
	uint32_t ret_buffer_len;
};

/* smb2_rw_rq flags */
typedef enum _SMB2_RW_RQ_FLAGS
{
    SMB2_SYNC_IO = 0x0001
} _SMB2_RW_RQ_FLAGS;

struct smb2_rw_rq {
    uint64_t flags;
	uint32_t remaining;
	uint32_t write_flags;
	SMBFID fid;
    uio_t auio;
    user_ssize_t io_len;
    enum smb_mc_control mc_flags;
    
    /* return values */
	uint32_t ret_ntstatus;
	uint32_t ret_len;
};

struct smb2_secure_neg_info {
    uint32_t capabilities;
    uint8_t guid[16];
    uint16_t security_mode;
    uint16_t dialect_count;
    uint16_t dialects[8];
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
    enum smb_mc_control mc_flags;
    
    /* return values */
	uint32_t ret_ntstatus;
    uint8_t ret_pad[4];
};

#define QUERY_NETWORK_INTERFACE_MAX_REPLY (64*1024)  // 64K bytes
struct smb2_network_info_reply {
    uint32_t buff_size;
    void     *buff;
};

int smb2_rq_alloc(struct smb_connobj *obj, u_char cmd, uint32_t *rq_len, 
                  vfs_context_t context, struct smbiod *iod, struct smb_rq **rqpp);
void smb_rq_bend32(struct smb_rq *rqp);
void smb2_rq_bstart(struct smb_rq *rqp, uint16_t *len_ptr);
void smb2_rq_bstart32(struct smb_rq *rqp, uint32_t *len_ptr);
void smb2_rq_align8(struct smb_rq *rqp);

uint32_t smb2_rq_credit_check(struct smb_rq *rqp, uint32_t len);
int smb2_rq_credit_increment(struct smb_rq *rqp);
void smb2_rq_credit_preprocess_reply(struct smb_rq *rqp);
void smb2_rq_credit_start(struct smbiod *iod, uint16_t credits);

int smb2_rq_message_id_increment(struct smb_rq *rqp);
int smb2_rq_next_command(struct smb_rq *rqp, size_t *next_cmd_offset,
                         struct mdchain *mdp);
uint32_t smb2_rq_length(struct smb_rq *rqp);
uint32_t smb2_rq_parse_header(struct smb_rq *rqp, struct mdchain **mdp, uint32_t parse_for_credits);
int smb_rq_getenv(struct smb_connobj *obj, struct smb_session **sessionp, 
                  struct smb_share **share);
int smb2_rq_update_cmpd_hdr(struct smb_rq *rqp, uint32_t position_flag);


#endif
