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
#include <stdint.h>
#include <sys/msfscc.h>

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_lib.h>
#include <netsmb/smbio.h>
#include <netsmb/smbio_2.h>
#include <netsmb/smb_dev_2.h>
#include <netsmb/smb_conn.h>
#include <smbclient/smbclient.h>
#include <smbclient/ntstatus.h>
#include <netsmb/upi_mbuf.h>
#include <sys/mchain.h>
#include "msdfs.h"
#include <parse_url.h>

/*
 * Note:  These are the user space calls that fill out the ioctl structs from
 * the args, makes the ioctl call into the kernel. Then takes the results 
 * returned in the ioctl structs and returns them in the args.
 */


int 
smb_is_smb2(struct smb_ctx *ctx)
{
    if (ctx->ct_vc_flags & SMBV_SMB2)
        return TRUE;
    else
        return FALSE;
}

int smb2io_check_directory(struct smb_ctx *ctx, const void *path,
                           uint32_t flags, uint32_t *nt_error)
{
#pragma unused(flags)
	int error;
	struct smb2ioc_check_dir rq;
	
    if (smb_is_smb2(ctx)) {
        /*
         * Using SMB2 
         */
        bzero(&rq, sizeof(rq));
        rq.ioc_version = SMB_IOC_STRUCT_VERSION;
        
        if (path == NULL) {
            return EINVAL;
        }
        
        rq.ioc_path = path;
        
        if (rq.ioc_path != NULL)
            rq.ioc_path_len = (uint32_t)strlen(rq.ioc_path) + 1;  /* end with null */
        else
            rq.ioc_path_len = 0;

        /* Call the kernel to make the Check Dir call */
        if (smb_ioctl_call(ctx->ct_fd, SMB2IOC_CHECK_DIR, &rq) == -1) {
            smb_log_info("%s: smb_ioctl_call, syserr = %s", 
                         ASL_LEVEL_DEBUG, 
                         __FUNCTION__, 
                         strerror(errno));
            error = errno;                  /* Some internal error happen? */
        }
        else {
            error = rq.ioc_ret_errno;
            
            if (rq.ioc_ret_ntstatus != 0) {
                /* error from server */
                *nt_error = rq.ioc_ret_ntstatus;
            }
            
            if (error) {
                smb_log_info("%s: smb_ioctl_call, error %d ntstatus = 0x%x", 
                             ASL_LEVEL_DEBUG, 
                             __FUNCTION__, 
                             error, *nt_error);
            }
        }
    }
    else {
        /*
         * Using SMB1 
         */
        error = smbio_check_directory(ctx, path, flags, nt_error);
    }
    
	return error;
}

int smb2io_close_file(void *smbctx, SMBFID fid)
{
	int error;
	int smb1_fid;
	struct smb2ioc_close rq;
    struct smb_ctx *ctx = smbctx;
	
    if (smb_is_smb2(ctx)) {
        /*
         * Using SMB2 
         */
        bzero(&rq, sizeof(rq));
        rq.ioc_version = SMB_IOC_STRUCT_VERSION;
        rq.ioc_flags = 0; /* do not want any returned attribute info */
        rq.ioc_fid = fid;
        
        /* Call the kernel to make the Close call */
        if (smb_ioctl_call(ctx->ct_fd, SMB2IOC_CLOSE, &rq) == -1) {
            smb_log_info("%s: smb_ioctl_call, syserr = %s", 
                         ASL_LEVEL_DEBUG, 
                         __FUNCTION__, 
                         strerror(errno));
            error = errno;                  /* Some internal error happen? */
        }
        else {
            error = rq.ioc_ret_ntstatus;	/* error from server */
            if (error) {
                smb_log_info("%s: smb_ioctl_call, ntstatus = 0x%x", 
                             ASL_LEVEL_DEBUG, 
                             __FUNCTION__, 
                             error);
            }
        }
    }
    else {
        /*
         * Using SMB1 
         */
        smb1_fid = (uint16_t) fid;  /* cast to smb1 fid */
        
        error = smbio_close_file(ctx, smb1_fid);
    }
    
	return error;
}

/* 
 * Perform a smb getDfsReferralDict call
 *
 * Return zero if no error or the appropriate errno.
 */
int 
smb2io_get_dfs_referral(struct smb_ctx *smbctx, CFStringRef dfs_referral_str,
                        uint16_t max_referral_level,
                        CFMutableDictionaryRef *out_referral_dict)
{
	int error = 0;
    struct smb2ioc_get_dfs_referral get_dfs_refer_ioc;
	uint32_t rcv_output_len = 0;
	char *file_name = NULL;
    char *rcv_buffer = NULL;
    
    if (smb_is_smb2(smbctx)) {
        /*
         * Using SMB2 
         */
        
        /* Convert referral string to a C string */
        file_name = CStringCreateWithCFString(dfs_referral_str);
        if (!file_name) {
            return ENOMEM;
        }
        
        /* Malloc recv buffer */
        rcv_buffer = malloc(1024 * 64);
        if (rcv_buffer == NULL) {
            error = ENOMEM;
            goto bad;
        }
        bzero(rcv_buffer, sizeof(rcv_buffer));

        /* Setup for ioctl call */
        bzero(&get_dfs_refer_ioc, sizeof(get_dfs_refer_ioc));
        get_dfs_refer_ioc.ioc_version = SMB_IOC_STRUCT_VERSION;
        
        get_dfs_refer_ioc.ioc_max_referral_level = max_referral_level;
        
        get_dfs_refer_ioc.ioc_file_name = file_name;
        get_dfs_refer_ioc.ioc_file_name_len = (uint32_t) strlen(get_dfs_refer_ioc.ioc_file_name) + 1;  /* end with null */
        
        get_dfs_refer_ioc.ioc_rcv_output = rcv_buffer;
        get_dfs_refer_ioc.ioc_rcv_output_len = 1024 * 64;
        
        /* Call the kernel to make the Get DFS Referral call */
        if (smb_ioctl_call(smbctx->ct_fd, SMB2IOC_GET_DFS_REFERRAL, &get_dfs_refer_ioc) == -1) {
            smb_log_info("%s: smb_ioctl_call, syserr = %s", 
                         ASL_LEVEL_DEBUG, 
                         __FUNCTION__, 
                         strerror(errno));
            error = errno;                  /* Some internal error happen? */
        }
        else {
            error = get_dfs_refer_ioc.ioc_ret_ntstatus;	/* error from server */
            if (error) {
                smb_log_info("%s: smb_ioctl_call, ntstatus = 0x%x", 
                             ASL_LEVEL_DEBUG, 
                             __FUNCTION__, 
                             error);
            }
            
            /* if IOCTL worked, then return bytes read */
            if (NT_SUCCESS(error)) {
                rcv_output_len = get_dfs_refer_ioc.ioc_ret_output_len;
                error = decodeDfsReferral(smbctx, NULL,
                                          rcv_buffer, rcv_output_len,
                                          file_name,
                                          out_referral_dict);
            }
        }
    }
    else {
        /*
         * Using SMB1 
         */
        error = getDfsReferralDict(smbctx, dfs_referral_str, max_referral_level,
                                   out_referral_dict);
    }
    
bad:
    if (file_name) {
        free(file_name);
    }

    if (rcv_buffer) {
        free(rcv_buffer);
    }
    
	return error;
}

int 
smb2io_ntcreatex(void *smbctx, const char *path, const char *streamName, 
                 struct open_inparms *inparms, 
                 struct open_outparm_ex *outparms, SMBFID *fid)
{
    struct smb_ctx *ctx = smbctx;
	struct smb2ioc_create rq;
	int	error = 0;
    size_t len;
    char *full_name = NULL;
    int smb1_fid;
    struct open_outparm smb1_outparms;
    
    if (smb_is_smb2(ctx)) {
        /*
         * Using SMB2 
         */
        bzero(&rq, sizeof(rq));
        rq.ioc_version = SMB_IOC_STRUCT_VERSION;
        
        /*
         * Check to see if need to concat stream name onto end of path 
         */
        if (path == NULL) {
            return EINVAL;
        }
        
        if ((streamName != NULL) && (strlen(streamName) != 0)) {
            len = strlen(path) + strlen(streamName) + 1; /* null at end */
            full_name = malloc(len);
            if (full_name == NULL) {
                return ENOMEM;
            }
            bzero(full_name, len);
            
            strncpy(full_name, path, len);
            
            if ( (strlen(streamName) + 1) > (len - strlen(full_name)) ) {
                /* paranoid check */
                error = ENOMEM;
                goto bad;
            }
            strncat(full_name, streamName, len - strlen(full_name) - 1);
        }
	    
        if (full_name != NULL) {
            /* must have a stream name too */
            rq.ioc_name = full_name;
        }
        else {
            /* no stream name found */
            rq.ioc_name = path;
        }
        
        if (rq.ioc_name != NULL)
            rq.ioc_name_len = (uint32_t)strlen(rq.ioc_name) + 1;  /* end with null */
        else
            rq.ioc_name_len = 0;
        
        rq.ioc_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
        rq.ioc_impersonate_level = SMB2_IMPERSONATION_IMPERSONATION;
        rq.ioc_desired_access = inparms->rights;
        rq.ioc_file_attributes = inparms->attrs;
        rq.ioc_share_access = inparms->shareMode;
        rq.ioc_disposition = inparms->disp;
        rq.ioc_create_options = inparms->createOptions;
        
        /* Call the kernel to make the Create call */
        if (smb_ioctl_call(ctx->ct_fd, SMB2IOC_CREATE, &rq) == -1) {
            smb_log_info("%s: smb_ioctl_call, syserr = %s", 
                         ASL_LEVEL_DEBUG, 
                         __FUNCTION__, 
                         strerror(errno));
            error = errno;                  /* Some internal error happened? */
            goto bad;
        }
        else {
            error = rq.ioc_ret_ntstatus;	/* error from server */
            if (error) {
                smb_log_info("%s: smb_ioctl_call, ntstatus = 0x%x", 
                             ASL_LEVEL_DEBUG, 
                             __FUNCTION__, 
                             error);
                goto bad;
            }
        }
        
        if (outparms) {
            outparms->createTime = rq.ioc_ret_create_time;
            outparms->accessTime = rq.ioc_ret_access_time;
            outparms->writeTime = rq.ioc_ret_write_time;
            outparms->changeTime = rq.ioc_ret_change_time;
            outparms->attributes = rq.ioc_ret_attributes;
            outparms->allocationSize = rq.ioc_ret_alloc_size;
            outparms->fileSize = rq.ioc_ret_eof;
            outparms->fid = rq.ioc_ret_fid;
            outparms->maxAccessRights = rq.ioc_ret_max_access;
            outparms->fileInode = 0;
            
            /*
             * %%% To Do - implement these fields
             * SMB 2.x cant get the File ID from just a create call so set
             * outparms->fileInode to be 0. Not sure if anyone uses this field.
             */
        }
        
        /* return the SMB2 fid */
        *fid = rq.ioc_ret_fid;
        
    bad:  
        if (full_name != NULL) {
            free(full_name);
        }
    }
    else {
        /*
         * Using SMB1 
         */
        if (inparms == NULL) {
            return (EINVAL);
        }
        
        bzero(&smb1_outparms, sizeof(struct open_outparm));

        error = smbio_ntcreatex(ctx, 
                                path, 
                                streamName, 
                                inparms, 
                                &smb1_outparms, 
                                &smb1_fid);
        if (!error) {
            /* copy data from old SMB1 struct to newer SMB2 struct */
            *fid = smb1_fid;   /* save smb1 fid */

            if (outparms != NULL) {
                outparms->createTime = smb1_outparms.createTime;
                outparms->accessTime = smb1_outparms.accessTime;
                outparms->writeTime = smb1_outparms.writeTime;
                outparms->changeTime = smb1_outparms.changeTime;
                outparms->attributes = smb1_outparms.attributes;
                outparms->allocationSize = smb1_outparms.allocationSize;
                outparms->fileSize = smb1_outparms.fileSize;
                outparms->fileInode = smb1_outparms.fileInode;
                outparms->maxAccessRights = smb1_outparms.maxAccessRights;
            }
        }
    }
    
    return error;
}

int
smb2io_read(struct smb_ctx *smbctx, SMBFID fid, off_t offset, uint32_t count, 
            char *dst, uint32_t *bytes_read)
{
	int error = 0;
	struct smb2ioc_rw rwrq2;
	struct smbioc_rw rwrq;
	uint16_t smb1_fid;
    
    if (smb_is_smb2(smbctx)) {
        /*
         * Using SMB2 
         */
        bzero(&rwrq2, sizeof(rwrq2));
        rwrq2.ioc_version = SMB_IOC_STRUCT_VERSION;
        rwrq2.ioc_len = count;
        rwrq2.ioc_offset = offset;
        /* leaving "remaining" at 0 since really dont know whats next */
        rwrq2.ioc_fid = fid;
        rwrq2.ioc_base = dst;
        
        /* Call the kernel to make the Read call */
        if (smb_ioctl_call(smbctx->ct_fd, SMB2IOC_READ, &rwrq2) == -1) {
            smb_log_info("%s: smb_ioctl_call, syserr = %s", 
                         ASL_LEVEL_DEBUG, 
                         __FUNCTION__, 
                         strerror(errno));
            error = errno;                  /* Some internal error happen? */
        }
        else {
            error = rwrq2.ioc_ret_ntstatus;	/* error from server */
            if (error) {
                smb_log_info("%s: smb_ioctl_call, ntstatus = 0x%x", 
                             ASL_LEVEL_DEBUG, 
                             __FUNCTION__, 
                             error);
            }
            
            /*
             * if read worked, or status is STATUS_BUFFER_OVERFLOW,
             * then return bytes_read.
             */
            if (NT_SUCCESS(error) ||
                (rwrq2.ioc_ret_ntstatus == STATUS_BUFFER_OVERFLOW)) {
                *bytes_read = rwrq2.ioc_ret_len;
                
                if (rwrq2.ioc_ret_ntstatus == STATUS_BUFFER_OVERFLOW)
                    error = EOVERFLOW;
            }
        }
    }
    else {
        /*
         * Using SMB1 
         * Dont call smb_read() because I want to return bytesRead
         */        
        smb1_fid = (uint16_t) fid;  /* cast to smb1 fid */
        
        bzero(&rwrq, sizeof(rwrq));
        rwrq.ioc_version = SMB_IOC_STRUCT_VERSION;
        rwrq.ioc_fh = smb1_fid;
        rwrq.ioc_base = dst;
        rwrq.ioc_cnt = count;
        rwrq.ioc_offset = offset;
        if (smb_ioctl_call(smbctx->ct_fd, SMBIOC_READ, &rwrq) == -1) {
            smb_log_info("%s: smb1_ioctl_call, syserr = %s", 
                         ASL_LEVEL_DEBUG, 
                         __FUNCTION__, 
                         strerror(errno));
            error = errno;                  /* Some internal error happen? */
        }
        *bytes_read = rwrq.ioc_cnt;
    }
	return error;
}

/* 
 * Perform a smb transaction call
 *
 * Return zero if no error or the appropriate errno.
 */
int 
smb2io_transact(struct smb_ctx *smbctx, uint64_t *setup, int setupCnt, 
                const char *pipeName, 
                const uint8_t *sndPData, size_t sndPDataLen, 
                const uint8_t *sndData, size_t sndDataLen, 
                uint8_t *rcvPData, size_t *rcvPDataLen, 
                uint8_t *rcvdData, size_t *rcvDataLen)
{
	int error = 0;
    uint16_t smb1_setup[2];
    struct smb2ioc_ioctl ioctl_rq;
	uint32_t rcv_max_output_len = 0;
    
    if (smb_is_smb2(smbctx)) {
        /*
         * Using SMB2 
         */

        /*
         * SMB2 only handles named pipe transactions where
         * pipe data to send is in sndData
         * pipe data to receive is in rcvdData
         */
        if ((setup[0] != TRANS_TRANSACT_NAMED_PIPE) || (setupCnt != 2)) {
            return EINVAL;
        }

        /*
         * SMB ioctl uses 32 bit field, never let the calling process send more 
         * than will fit in this field.
         */
        if (sndDataLen > UINT32_MAX) {
            return EINVAL;
        }	
        
        /*
         * SMB2 ioctl uses 32 bit field, never let the calling process request 
         * more than will fit in this field.
         */
        if (rcvDataLen != NULL) {
            if (*rcvDataLen > UINT32_MAX) {
                rcv_max_output_len = UINT32_MAX;
            } else {
                rcv_max_output_len = (uint32_t) *rcvDataLen;
            }
        }
        
        bzero(&ioctl_rq, sizeof(ioctl_rq));
        ioctl_rq.ioc_version = SMB_IOC_STRUCT_VERSION;
        ioctl_rq.ioc_ctl_code = FSCTL_PIPE_TRANSCEIVE;

        ioctl_rq.ioc_fid = setup[1];
        
        ioctl_rq.ioc_snd_input_len = (uint32_t) sndDataLen;
        ioctl_rq.ioc_snd_input = (void *) sndData;
        
        ioctl_rq.ioc_snd_output_len = (uint32_t) 0;
        ioctl_rq.ioc_snd_output = (void *) NULL;
        
        ioctl_rq.ioc_rcv_input_len = 0;
        ioctl_rq.ioc_rcv_input = (void *) NULL;
        
        ioctl_rq.ioc_rcv_output_len = rcv_max_output_len;
        ioctl_rq.ioc_rcv_output = (void *) rcvdData;
        
        /* Call the kernel to make the Ioctl call */
        if (smb_ioctl_call(smbctx->ct_fd, SMB2IOC_IOCTL, &ioctl_rq) == -1) {
            smb_log_info("%s: smb_ioctl_call, syserr = %s", 
                         ASL_LEVEL_DEBUG, 
                         __FUNCTION__, 
                         strerror(errno));
            error = errno;                  /* Some internal error happened? */
            goto bad;
        }
        else {
            error = ioctl_rq.ioc_ret_ntstatus;	/* error from server */
            if ((error) &&
                (ioctl_rq.ioc_ret_ntstatus != STATUS_BUFFER_OVERFLOW)) {
                smb_log_info("%s: smb_ioctl_call, ntstatus = 0x%x", 
                             ASL_LEVEL_DEBUG, 
                             __FUNCTION__, 
                             error);
                goto bad;
            }
        }

        if (rcvDataLen != NULL) {
            *rcvDataLen = ioctl_rq.ioc_ret_output_len;
        }
        
        if (ioctl_rq.ioc_ret_ntstatus == STATUS_BUFFER_OVERFLOW) {
            return EOVERFLOW;
        }
        
        error = 0;
    }
    else {
        /*
         * Using SMB1 
         */
        if (setup != NULL) {
            smb1_setup[0] = (uint16_t) setup[0];
            smb1_setup[1] = (uint16_t) setup[1]; /* cast to smb1 fid */
            
            error = smbio_transact(smbctx, smb1_setup, 2, pipeName, 
                                   sndPData, sndPDataLen,
                                   sndData, sndDataLen,
                                   rcvPData, rcvPDataLen,
                                   rcvdData, rcvDataLen);
        }
        else {
            error = smbio_transact(smbctx, NULL, 0, pipeName, 
                                   sndPData, sndPDataLen,
                                   sndData, sndDataLen,
                                   rcvPData, rcvPDataLen,
                                   rcvdData, rcvDataLen);
        }
    }
    
bad:
	return error;
}

int 
smb2io_write(struct smb_ctx *smbctx, SMBFID fid, off_t offset, uint32_t count, 
             const char *src, uint32_t *bytes_written)
{
	int error = 0;
	struct smb2ioc_rw rwrq2;
	struct smbioc_rw rwrq;
	uint16_t smb1_fid;
    
    if (smb_is_smb2(smbctx)) {
        /*
         * Using SMB2 
         */
        bzero(&rwrq2, sizeof(rwrq2));
        rwrq2.ioc_version = SMB_IOC_STRUCT_VERSION;
        rwrq2.ioc_len = count;
        rwrq2.ioc_offset = offset;
        /* leaving "remaining" at 0 since really dont know whats next */
        rwrq2.ioc_fid = fid;
        rwrq2.ioc_base = (char *) src;
        
        /* Call the kernel to make the Write call */
        if (smb_ioctl_call(smbctx->ct_fd, SMB2IOC_WRITE, &rwrq2) == -1) {
            smb_log_info("%s: smb_ioctl_call, syserr = %s", 
                         ASL_LEVEL_DEBUG, 
                         __FUNCTION__, 
                         strerror(errno));
            error = errno;                  /* Some internal error happen? */
        }
        else {
            error = rwrq2.ioc_ret_ntstatus;	/* error from server */
            if (error) {
                smb_log_info("%s: smb_ioctl_call, ntstatus = 0x%x", 
                             ASL_LEVEL_DEBUG, 
                             __FUNCTION__, 
                             error);
            }
            
            /* if write worked, then return bytes written */
            if (NT_SUCCESS(error)) {
                *bytes_written = rwrq2.ioc_ret_len;
            }
        }
    }
    else {
        /*
         * Using SMB1 
         * Dont call smb_write() because I want to return bytesRead
         */        
        smb1_fid = (uint16_t) fid;  /* cast to smb1 fid */
        
        bzero(&rwrq, sizeof(rwrq));
        rwrq.ioc_version = SMB_IOC_STRUCT_VERSION;
        rwrq.ioc_fh = smb1_fid;
        rwrq.ioc_base = (char *)src;
        rwrq.ioc_cnt = count;
        rwrq.ioc_offset = offset;
        /* 
         * Curretly we don't support Write Modes from user land. We do support paasing
         * it down, but until we see a requirement lets leave it zero out.
         */
        if (smb_ioctl_call(smbctx->ct_fd, SMBIOC_WRITE, &rwrq) == -1) {
            smb_log_info("%s: smb1_ioctl_call, syserr = %s", 
                         ASL_LEVEL_DEBUG, 
                         __FUNCTION__, 
                         strerror(errno));
            error = errno;                  /* Some internal error happen? */
        }
        *bytes_written = rwrq.ioc_cnt;
    }
    
	return error;
}


