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

#include <sys/smb_apple.h>
#include <sys/syslog.h>

#include <sys/msfscc.h>
#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_conn_2.h>

#include <smbfs/smbfs.h>
#include <smbfs/smbfs_node.h>
#include <smbfs/smbfs_subr.h>
#include <smbfs/smbfs_subr_2.h>
#include <smbfs/smbfs_notify_change.h>
#include <smbfs/smb_tran.h>
#include <smbclient/ntstatus.h>
#include <netsmb/smb_converter.h>
#include <smbclient/smbclient_internal.h>
#include <netsmb/smb2_mc_support.h>

extern lck_mtx_t global_Lease_hash_lock;

static int
smb2fs_smb_copyfile_mac(struct smb_share *share, struct smbnode *src_np,
                        struct smbnode *tdnp, const char *tnamep, size_t tname_len,
                        vfs_context_t context);
static int
smb2fs_smb_delete(struct smb_share *share, struct smbnode *np, enum vtype vnode_type,
                  const char *namep, size_t name_len,
                  int xattr, vfs_context_t context);

static uint32_t
smb2fs_smb_fillchunk_arr(struct smb2_copychunk_chunk *chunk_arr,
                         uint32_t chunk_arr_size,
                         uint64_t total_len, uint32_t max_chunk_len,
                         uint64_t src_offset, uint64_t trg_offset,
                         uint32_t *chunk_count_out, uint64_t *total_len_out);

static uint32_t
smb2fs_smb_get_create_options(struct smb_share *share, struct smbnode *np,
                              const char *namep, const char *strm_namep,
                              enum vtype vnode_type, uint32_t check_reparse);

static int
smb2fs_smb_listxattrs(struct smb_share *share, struct smbnode *np, char **xattrlist,
                      size_t *xattrlist_len, vfs_context_t context);
int
smb2fs_smb_ntcreatex(struct smb_share *share, struct smbnode *np,
                     const char *namep, size_t in_nmlen,
                     const char *strm_namep, size_t in_strm_nmlen,
                     uint32_t desired_access, enum vtype vnode_type,
                     uint32_t share_access, uint32_t disposition,
                     uint64_t create_flags, uint32_t create_options,
                     SMBFID *fidp, struct smbfattr *fap,
                     struct smb_rq **compound_rqp, struct smb2_create_rq **in_createp,
                     void *create_contextp, vfs_context_t context);
static int
smb2fs_smb_parse_ntcreatex(struct smb_share *share, struct smbnode *np, 
                           struct smb2_create_rq *createp,
                           SMBFID *fidp, struct smbfattr *fap,
                           vfs_context_t context);
static int
smb2fs_smb_qstreaminfo(struct smb_share *share, struct smbnode *np, enum vtype vnode_type,
                       const char *namep, size_t name_len,
                       const char *stream_namep,
                       uio_t uio, size_t *sizep,
                       uint64_t *stream_sizep, uint64_t *stream_alloc_sizep,
                       uint32_t *stream_flagsp, uint32_t *max_accessp,
                       vfs_context_t context);
static int
smb2fs_smb_request_resume_key(struct smb_share *share, SMBFID fid, u_char *resume_key,
                              vfs_context_t context);

static int
smb2fs_smb_set_eof(struct smb_share *share, SMBFID fid, uint64_t newsize,
                   vfs_context_t context);


/*
 * Note:  The _smbfs_smb_ in the function name indicates that these functions 
 * are called from the smbfs kext and set up the structs necessary to call the
 * _smb_ functions.  Then copy the results back out to smbfs.
 *
 *
 * SMB 1 implementation remains in original files
 *
 * Implementations listed in alphabetical order
 * 1) SMB 2/3 implementation listed first,
 * 2) SMB 1, SMB 2/3 generic implementation next
 *
 */

/*
 * Compound Request Chain notes:
 * 1) A compound req is identified by rqp->sr_next_rqp != NULL
 * 2) The first rqp in the chain has the response data for ALL the 
 * responses. Its header data has already been parsed out and saved in
 * the first rqp. Subsequent headers will have to be manually parsed out.
 * 3) When parsing the response chain, even if an earlier response gets an
 * error, continuing trying to parse the rest of the replies to get the
 * credits granted in the response header.
 * 4) If the first response got an error, only print that error message and
 * skip printing any more errors for the rest of the responses in the chain.
 */

int
smb2fs_smb_cmpd_check_copyfile(struct smb_share *share,
                               struct smbnode *create_np,
                               vfs_context_t context)
{
	int error, tmp_error;
    SMBFID fid = 0;
    struct smb2_create_rq *createp = NULL;
    struct smb2_ioctl_rq *ioctlp = NULL;
    struct smb2_close_rq *closep = NULL;
	struct smb_rq *create_rqp = NULL;
	struct smb_rq *ioctl_rqp = NULL;
	struct smb_rq *close_rqp = NULL;
	struct mdchain *mdp;
    size_t next_cmd_offset = 0;
    uint32_t need_delete_fid = 0;
    uint32_t create_desired_access = SMB2_FILE_READ_DATA |
                                     SMB2_FILE_READ_ATTRIBUTES |
                                     SMB2_FILE_READ_EA;
    enum vtype vnode_type = VREG;
    uint32_t share_access = NTCREATEX_SHARE_ACCESS_READ;
    uint64_t create_flags = 0;
    uint32_t create_options = 0;
    uint32_t disposition = FILE_OPEN;
    
    /*
     * For this function, vnode_type is the item's type. The item will be
     * Opened and the Ioctl done on the item.
     */

    SMB_MALLOC(ioctlp,
               struct smb2_ioctl_rq *,
               sizeof(struct smb2_ioctl_rq),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (ioctlp == NULL) {
		SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    if (create_np) {
        vnode_type = vnode_isdir(create_np->n_vnode) ? VDIR : VREG;
    }

resend:
    /*
     * Build the Create call
     */
    create_options = smb2fs_smb_get_create_options(share, create_np,
                                                   NULL, NULL,
                                                   vnode_type, 0);
    error = smb2fs_smb_ntcreatex(share, create_np,
                                 NULL, 0,
                                 NULL, 0,
                                 create_desired_access, vnode_type,
                                 share_access, disposition,
                                 create_flags, create_options,
                                 &fid, NULL,
                                 &create_rqp, &createp,
                                 NULL, context);
    
    if (error) {
        SMBDEBUG("smb2fs_smb_ntcreatex failed %d\n", error);
        goto bad;
    }
    
    /* Update Create hdr */
    error = smb2_rq_update_cmpd_hdr(create_rqp, SMB2_CMPD_FIRST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /*
     * Build the IOCTL request
     */
    ioctlp->share = share;
    ioctlp->ctl_code = FSCTL_SRV_REQUEST_RESUME_KEY;
    fid = 0xffffffffffffffff;   /* fid is -1 for compound requests */
    ioctlp->fid = fid;

	ioctlp->snd_input_len = 0;
	ioctlp->snd_output_len = 0;
	ioctlp->rcv_input_len = 0;
	ioctlp->rcv_output_len = 0x20;
    ioctlp->mc_flags = (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_MC_REPLAY_FLAG):0;

    error = smb2_smb_ioctl(share, create_rqp->sr_iod, ioctlp, &ioctl_rqp, context);
    if (error) {
        SMBERROR("smb2_smb_ioctl failed %d\n", error);
        goto bad;
    }
    
    /* Update IOCTL hdr */
    error = smb2_rq_update_cmpd_hdr(ioctl_rqp, SMB2_CMPD_MIDDLE);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /* Chain IOCTL to the Create */
    create_rqp->sr_next_rqp = ioctl_rqp;

    /*
     * Build the Close request
     */
    error = smb2_smb_close_fid(share, fid, &close_rqp, &closep, create_rqp->sr_iod, context);
    if (error) {
        SMBERROR("smb2_smb_close_fid failed %d\n", error);
        goto bad;
    }
    *close_rqp->sr_flagsp |= (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_FLAGS_REPLAY_OPERATIONS):0;

    /* Update Close hdr */
    error = smb2_rq_update_cmpd_hdr(close_rqp, SMB2_CMPD_LAST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /* Chain Close to the IOCTL */
    ioctl_rqp->sr_next_rqp = close_rqp;
    
    /*
     * Send the compound request of Create/IOCTL/Close
     */
    error = smb_rq_simple(create_rqp);
    
    if ((error) && (create_rqp->sr_flags & SMBR_RECONNECTED)) {
        SMB_LOG_MC("resending messageid %llu cmd %u (sr_flags: 0x%x).\n", create_rqp->sr_messageid, create_rqp->sr_command, create_rqp->sr_flags);
        if (create_rqp->sr_flags & SMBR_ALT_CH_DISCON) {
            /* An alternate channel got disconnected. Resend with the REPLAY flag set */
            create_flags |= SMB2_CREATE_REPLAY_FLAG;
        }

        /* Rebuild and try sending again */
        smb_rq_done(create_rqp);
        create_rqp = NULL;
        
        smb_rq_done(ioctl_rqp);
        ioctl_rqp = NULL;
        
        smb_rq_done(close_rqp);
        close_rqp = NULL;
        
        SMB_FREE(createp, M_SMBTEMP);
        createp = NULL;
        
        SMB_FREE(closep, M_SMBTEMP);
        closep = NULL;
        
        goto resend;
    }
    
    createp->ret_ntstatus = create_rqp->sr_ntstatus;
    
    /* Get pointer to response data */
    smb_rq_getreply(create_rqp, &mdp);
    
    if (error) {
        /* Create failed, try parsing the IOCTL */
        SMBDEBUG("smb_rq_simple failed %d id %lld\n",
                 error, create_rqp->sr_messageid);
        goto parse_ioctl;
    }
    
    /*
     * Parse the Create response.
     */
    error = smb2_smb_parse_create(share, mdp, createp);
    if (error) {
        /* Create parsing failed, try parsing the IOCTL */
        SMBERROR("smb2_smb_parse_create failed %d id %lld\n",
                 error, create_rqp->sr_messageid);
        goto parse_ioctl;
    }
    
    /* At this point, fid has been entered into fid table */
    need_delete_fid = 1;
    
    /*
     * No need to call smb2fs_smb_parse_ntcreatex since just need a FID.
     */

parse_ioctl:
    /* Consume any pad bytes */
    tmp_error = smb2_rq_next_command(create_rqp, &next_cmd_offset, mdp);
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("create smb2_rq_next_command failed %d id %lld\n",
                 tmp_error, create_rqp->sr_messageid);
        goto bad;
    }
    
    /*
     * Parse IOCTL SMB 2/3 header
     */
    tmp_error = smb2_rq_parse_header(ioctl_rqp, &mdp);
    ioctlp->ret_ntstatus = ioctl_rqp->sr_ntstatus;
    if (tmp_error) {
        /* IOCTL got an error, try parsing the Close */
        if (!error) {
            SMBDEBUG("ioctl smb2_rq_parse_header failed %d, id %lld\n",
                     tmp_error, ioctl_rqp->sr_messageid);
            error = tmp_error;
        }
        goto parse_close;
    }
    
    /* Parse the IOCTL response */
    tmp_error = smb2_smb_parse_ioctl(mdp, ioctlp);
    if (tmp_error) {
        /* IOCTL parsing got an error, try parsing the Close */
        if (!error) {
            SMBERROR("smb2_smb_parse_ioctl failed %d id %lld\n",
                     tmp_error, ioctl_rqp->sr_messageid);
            error = tmp_error;
        }
        goto parse_close;
    }
    
    /*
     * At this point, we know the IOCTL worked. Thats all we need to 
     * know is whether this compound request worked or not.
     */
    
parse_close:
    /* Update closep fid so it gets freed from FID table */
    closep->fid = createp->ret_fid;
    
    /* Consume any pad bytes */
    tmp_error = smb2_rq_next_command(ioctl_rqp, &next_cmd_offset, mdp);
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("ioctl smb2_rq_next_command failed %d\n", tmp_error);
        error = error ? error : tmp_error;
        goto bad;
    }
    
    /*
     * Parse Close SMB 2/3 header
     */
    tmp_error = smb2_rq_parse_header(close_rqp, &mdp);
    closep->ret_ntstatus = close_rqp->sr_ntstatus;
    if (tmp_error) {
        if (!error) {
            SMBDEBUG("close smb2_rq_parse_header failed %d id %lld\n",
                     tmp_error, close_rqp->sr_messageid);
            error = tmp_error;
        }
        goto bad;
    }
    
    /* Parse the Close response */
    tmp_error = smb2_smb_parse_close(mdp, closep);
    if (tmp_error) {
        if (!error) {
            SMBERROR("smb2_smb_parse_close failed %d id %lld\n",
                     tmp_error, close_rqp->sr_messageid);
            error = tmp_error;
        }
        goto bad;
    }
    
    /* At this point, fid has been removed from fid table */
    need_delete_fid = 0;
    
bad:
    if (need_delete_fid == 1) {
        /*
         * Close failed but the Create worked and was successfully parsed.
         * Try issuing the Close request again.
         */
        tmp_error = smb2_smb_close_fid(share, createp->ret_fid,
                                       NULL, NULL, NULL, context);
        if (tmp_error) {
            SMBERROR("Second close failed %d\n", tmp_error);
        }
    }

    if (create_rqp != NULL) {
        smb_rq_done(create_rqp);
    }
    if (ioctl_rqp != NULL) {
        smb_rq_done(ioctl_rqp);
    }
    if (close_rqp != NULL) {
        smb_rq_done(close_rqp);
    }
    
    if (createp != NULL) {
        SMB_FREE(createp, M_SMBTEMP);
    }
    if (ioctlp != NULL) {
        SMB_FREE(ioctlp, M_SMBTEMP);
    }
    if (closep != NULL) {
        SMB_FREE(closep, M_SMBTEMP);
    }

    return (error);
}

int
smb2fs_smb_cmpd_create(struct smb_share *share, struct smbnode *np,
                       const char *namep, size_t name_len,
                       const char *strm_namep, size_t strm_name_len,
                       uint32_t desired_access, enum vtype vnode_type,
                       uint32_t share_access, uint32_t disposition,
                       uint64_t create_flags, uint32_t *ntstatus,
                       SMBFID *fidp, struct smbfattr *fap,
                       void *create_contextp, vfs_context_t context)
{
	int error, tmp_error;
    struct smb2_create_rq *createp = NULL;
    struct smb2_query_info_rq *queryp = NULL;
    struct smb2_close_rq *closep = NULL;
	struct smb_rq *create_rqp = NULL;
	struct smb_rq *query_rqp = NULL;
	struct smb_rq *close_rqp = NULL;
	struct mdchain *mdp;
    size_t next_cmd_offset = 0;
    uint32_t need_delete_fid = 0;
    SMBFID fid = 0;
    int add_query = 0;
    int add_close = 0;
    uint64_t inode_number = 0;
    uint32_t inode_number_len;
    uint32_t create_options = 0;
    
    /*
     * For this function, vnode_type is the item's type. The item will be
     * Opened and the Query Info will be done on the item.
     */

    /*
     * This function can do Create/Close, Create/Query, or Create/Query/Close
     * If SMB2_CREATE_DO_CREATE is set in create_flags, then the Query is
     * added to get the file ID. If fidp is NULL, then the Close is added.
     *
     * ntstatus is needed for DFS code.  smb_usr_check_dir needs to return the
     * ntstatus up to isReferralPathNotCovered() in user space.
     */

    if ((create_flags & SMB2_CREATE_DO_CREATE) &&
        (SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS)) {
        /* Need the file id, so add a query into the compound request */
        add_query = 1;
    }
    
    if (fidp == NULL) {
        /* If fidp is null, add the close into the compound request */
        add_close = 1;
    }

    if ((add_query == 0) && (add_close == 0)) {
        /* Just doing a simple create */
        create_options = smb2fs_smb_get_create_options(share, np,
                                                       namep, strm_namep,
                                                       vnode_type, 1);
        error = smb2fs_smb_ntcreatex(share, np,
                                     namep, name_len,
                                     strm_namep, strm_name_len,
                                     desired_access, vnode_type,
                                     share_access, disposition,
                                     create_flags, create_options,
                                     fidp, fap,
                                     NULL, NULL,
                                     create_contextp, context);
        return error;
    }
    
    if (add_query) {
        SMB_MALLOC(queryp,
                   struct smb2_query_info_rq *,
                   sizeof(struct smb2_query_info_rq),
                   M_SMBTEMP,
                   M_WAITOK | M_ZERO);
        if (queryp == NULL) {
            SMBERROR("SMB_MALLOC failed\n");
            error = ENOMEM;
            goto bad;
        }
    }

resend:
    /*
     * Build the Create call 
     */
    create_options = smb2fs_smb_get_create_options(share, np,
                                                   namep, strm_namep,
                                                   vnode_type, 1);
    error = smb2fs_smb_ntcreatex(share, np,
                                 namep, name_len,
                                 strm_namep, strm_name_len,
                                 desired_access, vnode_type,
                                 share_access, disposition,
                                 create_flags, create_options,
                                 &fid, fap,
                                 &create_rqp, &createp, 
                                 create_contextp, context);
    if (error) {
        SMBERROR("smb2fs_smb_ntcreatex failed %d\n", error);
        goto bad;
    }
    
    /* Update Create hdr */
    error = smb2_rq_update_cmpd_hdr(create_rqp, SMB2_CMPD_FIRST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }

    fid = 0xffffffffffffffff;   /* fid is -1 for compound requests */
    
    if (add_query) {
        /*
         * Build the Query Info request (to get File ID)
         */
        inode_number_len = (uint32_t) sizeof(inode_number);

        queryp->info_type = SMB2_0_INFO_FILE;
        queryp->file_info_class = FileInternalInformation;
        queryp->add_info = 0;
        queryp->flags = 0;
        queryp->output_buffer_len = inode_number_len;
        queryp->output_buffer = (uint8_t *) &inode_number;
        queryp->input_buffer_len = 0;
        queryp->input_buffer = NULL;
        queryp->ret_buffer_len = 0;
        queryp->fid = fid;
        queryp->mc_flags = (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_MC_REPLAY_FLAG):0;
        
        error = smb2_smb_query_info(share, queryp, &query_rqp, create_rqp->sr_iod, context);
        if (error) {
            SMBERROR("smb2_smb_query_info failed %d\n", error);
            goto bad;
        }
        
        /* Update Query hdr */
        if (add_close) {
            error = smb2_rq_update_cmpd_hdr(query_rqp, SMB2_CMPD_MIDDLE);
        }
        else {
            error = smb2_rq_update_cmpd_hdr(query_rqp, SMB2_CMPD_LAST);
        }
        if (error) {
            SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
            goto bad;
        }
        
        /* Chain Query Info to the Create */
        create_rqp->sr_next_rqp = query_rqp;
    }
    
    if (add_close) {
        /*
         * Build the Close request 
         */
        error = smb2_smb_close_fid(share, fid, &close_rqp, &closep, create_rqp->sr_iod, context);
        if (error) {
            SMBERROR("smb2_smb_close_fid failed %d\n", error);
            goto bad;
        }
        *close_rqp->sr_flagsp |= (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_FLAGS_REPLAY_OPERATIONS):0;

        /* Update Close hdr */
        error = smb2_rq_update_cmpd_hdr(close_rqp, SMB2_CMPD_LAST);
        if (error) {
            SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
            goto bad;
        }
        
        if (add_query) {
            /* Chain Close to the Query */
            query_rqp->sr_next_rqp = close_rqp;
        }
        else {
            /* Chain Close to the Create */
            create_rqp->sr_next_rqp = close_rqp;
        }
    }
    
    /* 
     * Send the compound request of Create/Query/Close 
     */
    error = smb_rq_simple(create_rqp);

    if ((error) && (create_rqp->sr_flags & SMBR_RECONNECTED)) {
        SMB_LOG_MC("resending messageid %llu cmd %u (sr_flags: 0x%x).\n", create_rqp->sr_messageid, create_rqp->sr_command, create_rqp->sr_flags);
        if (create_rqp->sr_flags & SMBR_ALT_CH_DISCON) {
            /* An alternate channel got disconnected. Resend with the REPLAY flag set */
            create_flags |= SMB2_CREATE_REPLAY_FLAG;
        }

        /* Rebuild and try sending again */
        smb_rq_done(create_rqp);
        create_rqp = NULL;
        
        if (query_rqp != NULL) {
            smb_rq_done(query_rqp);
            query_rqp = NULL;
        }

        if (close_rqp != NULL) {
            smb_rq_done(close_rqp);
            close_rqp = NULL;
        }
        
        SMB_FREE(createp, M_SMBTEMP);
        createp = NULL;

        if (closep != NULL) {
            SMB_FREE(closep, M_SMBTEMP);
            closep = NULL;
        }

        goto resend;
    }

    createp->ret_ntstatus = create_rqp->sr_ntstatus;
    if (ntstatus) {
        /* Return NT Status error if they want it */
        *ntstatus = createp->ret_ntstatus;
    }

    /* Get pointer to response data */
    smb_rq_getreply(create_rqp, &mdp);
    
    if (error) {
        /* Create failed, try parsing the Query */
        if (error != ENOENT) {
            SMBDEBUG("smb_rq_simple failed %d id %lld\n",
                     error, create_rqp->sr_messageid);
        }
        goto parse_query;
    }
    
    /* 
     * Parse the Create response.
     */
    error = smb2_smb_parse_create(share, mdp, createp);
    if (error) {
        /* Create parsing failed, try parsing the Query */
        SMBERROR("smb2_smb_parse_create failed %d id %lld\n", 
                 error, create_rqp->sr_messageid);
        goto parse_query;
    }
    
    /* At this point, fid has been entered into fid table */
    need_delete_fid = 1;
    
    /*
     * Fill in fap and possibly update vnode's meta data caches
     */
    error = smb2fs_smb_parse_ntcreatex(share, np, createp, 
                                       fidp, fap, context);
    if (error) {
        /* Updating meta data cache failed, try parsing the Close */
        SMBERROR("smb2fs_smb_parse_ntcreatex failed %d id %lld\n", 
                 error, create_rqp->sr_messageid);
    }

parse_query:
    if (query_rqp != NULL) {
        /* Consume any pad bytes */
        tmp_error = smb2_rq_next_command(create_rqp, &next_cmd_offset, mdp);
        if (tmp_error) {
            /* Failed to find next command, so can't parse rest of the responses */
            SMBERROR("create smb2_rq_next_command failed %d id %lld\n",
                     tmp_error, create_rqp->sr_messageid);
            error = error ? error : tmp_error;
            goto bad;
        }
        
        /*
         * Parse Query Info SMB 2/3 header
         */
        tmp_error = smb2_rq_parse_header(query_rqp, &mdp);
        queryp->ret_ntstatus = query_rqp->sr_ntstatus;
        if (tmp_error) {
            /* Query Info got an error, try parsing the Close */
            if (!error) {
                SMBDEBUG("query smb2_rq_parse_header failed %d, id %lld\n",
                         tmp_error, query_rqp->sr_messageid);
                error = tmp_error;
            }
            goto parse_close;
        }
        
        /* Parse the Query Info response */
        tmp_error = smb2_smb_parse_query_info(mdp, queryp);
        if (tmp_error) {
            /* Query Info parsing got an error, try parsing the Close */
            if (!error) {
                if (tmp_error != ENOATTR) {
                    SMBERROR("smb2_smb_parse_query_info failed %d id %lld\n",
                             tmp_error, query_rqp->sr_messageid);
                }
                error = tmp_error;
            }
            goto parse_close;
        }
        else {
            /* Query worked, so get the inode number */
            if (fap) {
                fap->fa_ino = inode_number;
                smb2fs_smb_file_id_check(share, fap->fa_ino, NULL, 0);
            }
        }
    }

parse_close:
    if (close_rqp != NULL) {
        /* Update closep fid so it gets freed from FID table */
        closep->fid = createp->ret_fid;
        
        /* Consume any pad bytes */
        tmp_error = smb2_rq_next_command(query_rqp != NULL ? query_rqp : create_rqp,
                                         &next_cmd_offset, mdp);
        if (tmp_error) {
            /* Failed to find next command, so can't parse rest of the responses */
            SMBERROR("create smb2_rq_next_command failed %d id %lld\n",
                     tmp_error, create_rqp->sr_messageid);
            error = error ? error : tmp_error;
            goto bad;
        }
        
        /*
         * Parse Close SMB 2/3 header
         */
        tmp_error = smb2_rq_parse_header(close_rqp, &mdp);
        closep->ret_ntstatus = close_rqp->sr_ntstatus;
        if (tmp_error) {
            if (!error) {
                SMBDEBUG("close smb2_rq_parse_header failed %d id %lld\n",
                         tmp_error, close_rqp->sr_messageid);
                error = tmp_error;
                
                if (ntstatus) {
                    /* Return NT Status error if they want it */
                    *ntstatus = closep->ret_ntstatus;
                }
            }
            goto bad;
        }
        
        /* Parse the Close response */
        tmp_error = smb2_smb_parse_close(mdp, closep);
        if (tmp_error) {
            if (!error) {
                SMBERROR("smb2_smb_parse_close failed %d id %lld\n",
                         tmp_error, close_rqp->sr_messageid);
                error = tmp_error;
            }
            goto bad;    
        }

        /* At this point, fid has been removed from fid table */
        need_delete_fid = 0;
    }
    else {
        /* Not doing a close, so leave it open */
        need_delete_fid = 0;
    }
    
bad:
    if (need_delete_fid == 1) {
        /*
         * Close failed but the Create worked and was successfully parsed.
         * Try issuing the Close request again.
         */
        tmp_error = smb2_smb_close_fid(share, createp->ret_fid,
                                       NULL, NULL, NULL, context);
        if (tmp_error) {
            SMBERROR("Second close failed %d\n", tmp_error);
        }
    }
    
    if (create_rqp != NULL) {
        smb_rq_done(create_rqp);
    }
    if (query_rqp != NULL) {
        smb_rq_done(query_rqp);
    }
    if (close_rqp != NULL) {
        smb_rq_done(close_rqp);
    }
    
    if (createp != NULL) {
        SMB_FREE(createp, M_SMBTEMP);
    }
    if (queryp != NULL) {
        SMB_FREE(queryp, M_SMBTEMP);
    }
    if (closep != NULL) {
        SMB_FREE(closep, M_SMBTEMP);
    }
    
	return error;
}

static int
smb2fs_smb_cmpd_create_read(struct smb_share *share, struct smbnode *dnp,
                            const char *namep, size_t name_len,
                            const char *snamep, size_t sname_len,
                            uint32_t desired_access, uio_t uio,
                            size_t *sizep, uint32_t *max_accessp,
                            SMBFID *fidp, struct timespec *mtime,
                            vfs_context_t context)
{
	int error, tmp_error;
    uint32_t disposition;
    uint32_t share_access;
    SMBFID fid = 0;
    struct smbfattr *fap = NULL;
    struct smb2_create_rq *createp = NULL;
    struct smb2_rw_rq *readp = NULL;
    struct smb2_close_rq *closep = NULL;
	struct smb_rq *create_rqp = NULL;
	struct smb_rq *read_rqp = NULL;
	struct smb_rq *close_rqp = NULL;
	struct mdchain *mdp;
    size_t next_cmd_offset = 0;
	user_ssize_t len, resid = 0;
    user_ssize_t rresid = 0;
    uint64_t create_flags = SMB2_CREATE_GET_MAX_ACCESS;
    uint32_t need_delete_fid = 0;
    int add_close = 0;
    uint32_t create_options = 0;
    enum vtype vnode_type = VREG;

    /*
     * We can only read files, so vnode_type is always VREG
     * Named streams are always files.
     */
    
    /*
     * This function can do Create/Read or Create/Read/Close
     * If fidp is NULL, then the Close is added.
     */

    /*
     * (1) This is used when dealing with readdirattr for Finder Info 
     * It has a parent dnp, child of namep and stream name of snamep.
     * (2) This is used for dealing with Finder Info which is an xattr
     * (ie stream) and assumes the read data will all fit in one read request
     */
    
    if (fidp == NULL) {
        /* If fidp is null, add the close into the compound request */
        add_close = 1;
    }
    else {
        *fidp = 0;  /* indicates create failed */
    }

    SMB_MALLOC(fap,
               struct smbfattr *, 
               sizeof(struct smbfattr), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (fap == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    SMB_MALLOC(readp,
               struct smb2_rw_rq *,
               sizeof(struct smb2_rw_rq),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (readp == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    /*
     * Be careful here. The vnode_type is for the actual item that the
     * Read is being done on. The path to the item is built from
     * create_np and namep, but only the calling function knows what the
     * item type is.
     */

    /*
     * Set up for the Create call
     */
    if (snamep) {
        create_flags |= SMB2_CREATE_IS_NAMED_STREAM;
        /* Named streams are always files */
        vnode_type = VREG;
		
		if ((namep == NULL) && (vnode_vtype(SMBTOV(dnp)) == VLNK)) {
			/* dnp is actual item being opened, see if its a symlink */
			vnode_type = VLNK;
		}
    }
    
    if (desired_access & SMB2_FILE_WRITE_DATA) {
		share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_DELETE;
		disposition = FILE_OPEN_IF;
	}
    else {
		share_access = NTCREATEX_SHARE_ACCESS_ALL;
		disposition = FILE_OPEN;
	}

resend:
    /*
     * Build the Create call
     */
    create_options = smb2fs_smb_get_create_options(share, dnp,
                                                   namep, snamep,
                                                   vnode_type, 1);
    error = smb2fs_smb_ntcreatex(share, dnp,
                                 namep, name_len,
                                 snamep, sname_len,
                                 desired_access, vnode_type,
                                 share_access, disposition,
                                 create_flags, create_options,
                                 &fid, fap,
                                 &create_rqp, &createp, 
                                 NULL, context);
    if (error) {
        SMBERROR("smb2fs_smb_ntcreatex failed %d\n", error);
        goto bad;
    }
    
    /* Update Create hdr */
    error = smb2_rq_update_cmpd_hdr(create_rqp, SMB2_CMPD_FIRST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /* 
     * Build the Read request 
     */
    readp->flags = 0;
    readp->remaining = 0;
    readp->write_flags = 0;
    fid = 0xffffffffffffffff;   /* fid is -1 for compound requests */
    readp->fid = fid;
    readp->auio = uio;
    readp->mc_flags = (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_MC_REPLAY_FLAG):0;
    
    /* assume we can read it all in one request */
	len = uio_resid(readp->auio);
    
    error = smb2_smb_read_one(share, readp, &len, &resid, &read_rqp, create_rqp->sr_iod, context);
    if (error) {
        SMBERROR("smb2_smb_read_one failed %d\n", error);
        goto bad;
    }

    /* Update Read hdr */
    if (add_close) {
        error = smb2_rq_update_cmpd_hdr(read_rqp, SMB2_CMPD_MIDDLE);
    }
    else {
        error = smb2_rq_update_cmpd_hdr(read_rqp, SMB2_CMPD_LAST);
    }
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /* Chain Read to the Create */
    create_rqp->sr_next_rqp = read_rqp;
    
    if (add_close) {
        /*
         * Build the Close request 
         */
        error = smb2_smb_close_fid(share, fid, &close_rqp, &closep, create_rqp->sr_iod, context);
        if (error) {
            SMBERROR("smb2_smb_close_fid failed %d\n", error);
            goto bad;
        }
        *close_rqp->sr_flagsp |= (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_FLAGS_REPLAY_OPERATIONS):0;

        /* Update Close hdr */
        error = smb2_rq_update_cmpd_hdr(close_rqp, SMB2_CMPD_LAST);
        if (error) {
            SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
            goto bad;
        }
        
        /* Chain Close to the Read */
        read_rqp->sr_next_rqp = close_rqp;
    }

    /*
     * Send the compound request of Create/Read/Close
     */
    error = smb_rq_simple(create_rqp);
    
    if ((error) && (create_rqp->sr_flags & SMBR_RECONNECTED)) {
        SMB_LOG_MC("resending messageid %llu cmd %u (sr_flags: 0x%x).\n", create_rqp->sr_messageid, create_rqp->sr_command, create_rqp->sr_flags);
        if (create_rqp->sr_flags & SMBR_ALT_CH_DISCON) {
            /* An alternate channel got disconnected. Resend with the REPLAY flag set */
            create_flags |= SMB2_CREATE_REPLAY_FLAG;
        }

        /* Rebuild and try sending again */
        smb_rq_done(create_rqp);
        create_rqp = NULL;
        
        smb_rq_done(read_rqp);
        read_rqp = NULL;

        if (close_rqp != NULL) {
            smb_rq_done(close_rqp);
            close_rqp = NULL;
        }
        
        SMB_FREE(createp, M_SMBTEMP);
        createp = NULL;
        
        if (closep != NULL) {
            SMB_FREE(closep, M_SMBTEMP);
            closep = NULL;
        }
        
        goto resend;
    }

    createp->ret_ntstatus = create_rqp->sr_ntstatus;
    
    /* Get pointer to response data */
    smb_rq_getreply(create_rqp, &mdp);
    
    if (error) {
        /* Create failed, try parsing the Read */
        if (error != ENOENT) {
            SMBDEBUG("smb_rq_simple failed %d id %lld\n",
                     error, create_rqp->sr_messageid);
        }
        
        if (fidp) {
            *fidp = 0;  /* indicate Create failed */
        }
        goto parse_read;
    }
    
    /* 
     * Parse the Create response.
     */
    error = smb2_smb_parse_create(share, mdp, createp);
    if (error) {
        /* Create parsing failed, try parsing the Read */
        SMBERROR("smb2_smb_parse_create failed %d id %lld\n", 
                 error, create_rqp->sr_messageid);
        
        if (fidp) {
            *fidp = 0;  /* indicate Create failed */
        }
        goto parse_read;
    }

    /* At this point, fid has been entered into fid table */
    need_delete_fid = 1;
    
    /*
     * Fill in fap and possibly update vnode's meta data caches
     */
    error = smb2fs_smb_parse_ntcreatex(share, dnp, createp, 
                                       fidp, fap, context);
    if (error) {
        /* Updating meta data cache failed, try parsing the Read */
        SMBERROR("smb2fs_smb_parse_ntcreatex failed %d id %lld\n", 
                 error, create_rqp->sr_messageid);
    }
    else {
        /* Return max access if they want it */
        if (max_accessp) {
            DBG_ASSERT(fap->fa_valid_mask & FA_MAX_ACCESS_VALID)
            *max_accessp = fap->fa_max_access;
        }
        if (mtime) {
            *mtime = fap->fa_mtime;
        }
    }
    
parse_read:
    /* Consume any pad bytes */
    tmp_error = smb2_rq_next_command(create_rqp, &next_cmd_offset, mdp);
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("create smb2_rq_next_command failed %d id %lld\n", 
                 tmp_error, create_rqp->sr_messageid);
        error = error ? error : tmp_error;
        goto bad;
    }
    
    /* 
     * Parse Read SMB 2/3 header 
     */
    tmp_error = smb2_rq_parse_header(read_rqp, &mdp);
    readp->ret_ntstatus = read_rqp->sr_ntstatus;
    
    if (tmp_error == ENODATA) {
        /*
         * Ignore EOF and leave error alone as it could contain an error from
         * the create, but skip trying to parse the read data
         */
        goto parse_close;
    }
    
    if (tmp_error) {
        /* Read got an error, try parsing the Close */
        if (!error) {
            SMBDEBUG("read smb2_rq_parse_header failed %d, id %lld\n",
                     tmp_error, read_rqp->sr_messageid);
            error = tmp_error;
        }
        goto parse_close;
    }
    
    /* Parse the Read response */
    tmp_error = smb2_smb_parse_read_one(mdp, &rresid, readp);
    if (tmp_error) {
        /* Read parsing got an error, try parsing the Close */
        if (!error) {
            SMBERROR("smb2_smb_parse_read_one failed %d id %lld\n", 
                     tmp_error, read_rqp->sr_messageid);
            error = tmp_error;
        }
        goto parse_close;
    }
    
    if (rresid != len) {
        /*
         * Not doing a vnop_read, so probably ok to get a short read
         */
        SMBWARNING("IO Mismatched. Requested %lld but got %lld\n",
                   len, rresid);
    }

    if (sizep) {
        /* 
         * *sizep contains the logical size of the stream
         * The calling routines can only handle size_t 
         */
        *sizep = fap->fa_size;
    }

parse_close:
    if (close_rqp != NULL) {
        /* Update closep fid so it gets freed from FID table */
        closep->fid = createp->ret_fid;

        /* Consume any pad bytes */
        tmp_error = smb2_rq_next_command(read_rqp, &next_cmd_offset, mdp);
        if (tmp_error) {
            /* Failed to find next command, so can't parse rest of the responses */
            SMBERROR("read smb2_rq_next_command failed %d id %lld\n", 
                     tmp_error, read_rqp->sr_messageid);
            error = error ? error : tmp_error;
            goto bad;
        }
        
        /* 
         * Parse Close SMB 2/3 header
         */
        tmp_error = smb2_rq_parse_header(close_rqp, &mdp);
        closep->ret_ntstatus = close_rqp->sr_ntstatus;
        if (tmp_error) {
            if (!error) {
                SMBDEBUG("close smb2_rq_parse_header failed %d id %lld\n",
                         tmp_error, close_rqp->sr_messageid);
                error = tmp_error;
            }
            goto bad;    
        }
        
        /* Parse the Close response */
        tmp_error = smb2_smb_parse_close(mdp, closep);
        if (tmp_error) {
            if (!error) {
                SMBERROR("smb2_smb_parse_close failed %d id %lld\n", 
                         tmp_error, close_rqp->sr_messageid);
                error = tmp_error;
            }
            goto bad;    
        }
        
        /* At this point, fid has been removed from fid table */
        need_delete_fid = 0;
    }
    else {
        /* Not doing a close, so leave it open */
        need_delete_fid = 0;
    }

bad:
    if (need_delete_fid == 1) {
        /*
         * Close failed but the Create worked and was successfully parsed.
         * Try issuing the Close request again.
         */
        tmp_error = smb2_smb_close_fid(share, createp->ret_fid,
                                       NULL, NULL, NULL, context);
        if (tmp_error) {
            SMBERROR("Second close failed %d\n", tmp_error);
        }
    }

    if (create_rqp != NULL) {
        smb_rq_done(create_rqp);
    }
    if (read_rqp != NULL) {
        smb_rq_done(read_rqp);
    }
    if (close_rqp != NULL) {
        smb_rq_done(close_rqp);
    }
    
    if (createp != NULL) {
        SMB_FREE(createp, M_SMBTEMP);
    }
    if (readp != NULL) {
        SMB_FREE(readp, M_SMBTEMP);
    }
    if (closep != NULL) {
        SMB_FREE(closep, M_SMBTEMP);
    }
    
    if (fap != NULL) {
        SMB_FREE(fap, M_SMBTEMP);
    }
    
	return error;
}

int smbfs_smb_cmpd_create_read_close(struct smb_share *share, struct smbnode *dnp,
                                     const char *namep, size_t name_len,
                                     const char *snamep, size_t sname_len,
                                     uio_t uio, size_t *sizep,
                                     uint32_t *max_accessp,
                                     vfs_context_t context)
{
    int error;
    SMBFID fid = 0;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        /* Do a Create/Read/Close */
        error = smb2fs_smb_cmpd_create_read(share, dnp,
                                            namep, name_len,
                                            snamep, sname_len,
                                            SMB2_FILE_READ_DATA, uio,
                                            sizep, max_accessp,
                                            NULL, NULL,
                                            context);
    }
    else {
        error = smb1fs_smb_open_read(share, dnp,
                                     namep, name_len,
                                     snamep, sname_len,
                                     &fid, uio, sizep,
                                     max_accessp,
                                     context);
        if (fid) {
            (void)smbfs_smb_close(share, fid, context);
        }
    }
    
    return error;
}

int
smb2fs_smb_cmpd_create_write(struct smb_share *share, struct smbnode *dnp,
                             const char *namep, size_t name_len,
                             const char *snamep, size_t sname_len,
                             uint32_t desired_access, struct smbfattr *fap,
                             uint64_t create_flags, uio_t uio,
                             SMBFID *fidp, int ioflag,
                             vfs_context_t context)
{
#pragma unused(ioflag)
    int error, tmp_error;
    uint32_t disposition;
    uint32_t share_access;
    SMBFID fid = 0;
    struct smb2_create_rq *createp = NULL;
    struct smb2_query_info_rq *queryp = NULL;
    struct smb2_rw_rq *writep = NULL;
    struct smb2_close_rq *closep = NULL;
    struct smb_rq *create_rqp = NULL;
    struct smb_rq *query_rqp = NULL;
    struct smb_rq *write_rqp = NULL;
    struct smb_rq *close_rqp = NULL;
    struct mdchain *mdp;
    size_t next_cmd_offset = 0;
    user_ssize_t len, resid = 0;
    user_ssize_t rresid = 0;
    uint32_t need_delete_fid = 0;
    int add_query = 0;
    int add_close = 0;
    uint32_t create_options = 0;
    uint32_t write_mode = 0;    /* Never supports write through */
    uint64_t inode_number = 0;
    uint32_t inode_number_len;
    enum vtype vnode_type = VREG;

    /*
     * This function can do Create/Write, Create/Query/Write, 
     * Create/Write/Close or Create/Query/Write/Close.
     *
     * If SMB2_CREATE_DO_CREATE is set in create_flags, then the Query is
     * added to get the file ID
     *
     * If fidp is NULL, then the Close is added.
     */
    
    if ((create_flags & SMB2_CREATE_DO_CREATE) &&
        (SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS)) {
        /* Need the file id, so add a query into the compound request */
        add_query = 1;
    }

    if (fidp == NULL) {
        /* If fidp is null, add the close into the compound request */
        add_close = 1;
    }
    else {
        *fidp = 0;  /* indicates create failed */
    }
    
    if (add_query) {
        SMB_MALLOC(queryp,
                   struct smb2_query_info_rq *,
                   sizeof(struct smb2_query_info_rq),
                   M_SMBTEMP,
                   M_WAITOK | M_ZERO);
        if (queryp == NULL) {
            SMBERROR("SMB_MALLOC failed\n");
            error = ENOMEM;
            goto bad;
        }
    }

    SMB_MALLOC(writep,
               struct smb2_rw_rq *,
               sizeof(struct smb2_rw_rq),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (writep == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    /*
     * Set up for the Create call
     */
    if (snamep) {
        create_flags |= SMB2_CREATE_IS_NAMED_STREAM;
        /* Named streams are always files */
        vnode_type = VREG;

        if ((namep == NULL) && (vnode_vtype(SMBTOV(dnp)) == VLNK)) {
            /* dnp is actual item being opened, see if its a symlink */
            vnode_type = VLNK;
        }
    }

    if (desired_access & SMB2_FILE_WRITE_DATA) {
        share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_DELETE;
        if (create_flags & SMB2_CREATE_DO_CREATE) {
            disposition = FILE_OPEN_IF;
        }
        else {
            disposition = FILE_OPEN;
        }
    }
    else {
        share_access = NTCREATEX_SHARE_ACCESS_ALL;
        disposition = FILE_OPEN;
    }
    
resend:
    /*
     * Build the Create call
     */
    create_options = smb2fs_smb_get_create_options(share, dnp,
                                                   namep, snamep,
                                                   vnode_type, 1);
    error = smb2fs_smb_ntcreatex(share, dnp,
                                 namep, name_len,
                                 snamep, sname_len,
                                 desired_access, vnode_type,
                                 share_access, disposition,
                                 create_flags, create_options,
                                 &fid, fap,
                                 &create_rqp, &createp,
                                 NULL, context);
    if (error) {
        SMBERROR("smb2fs_smb_ntcreatex failed %d\n", error);
        goto bad;
    }
    
    /* Update Create hdr */
    error = smb2_rq_update_cmpd_hdr(create_rqp, SMB2_CMPD_FIRST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    fid = 0xffffffffffffffff;   /* fid is -1 for compound requests */

    if (add_query) {
        /*
         * Build the Query Info request (to get File ID)
         */
        inode_number_len = (uint32_t) sizeof(inode_number);
        
        queryp->info_type = SMB2_0_INFO_FILE;
        queryp->file_info_class = FileInternalInformation;
        queryp->add_info = 0;
        queryp->flags = 0;
        queryp->output_buffer_len = inode_number_len;
        queryp->output_buffer = (uint8_t *) &inode_number;
        queryp->input_buffer_len = 0;
        queryp->input_buffer = NULL;
        queryp->ret_buffer_len = 0;
        queryp->fid = fid;
        queryp->mc_flags = (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_MC_REPLAY_FLAG):0;

        error = smb2_smb_query_info(share, queryp, &query_rqp, create_rqp->sr_iod, context);
        if (error) {
            SMBERROR("smb2_smb_query_info failed %d\n", error);
            goto bad;
        }
        
        /* Update Query hdr */
        error = smb2_rq_update_cmpd_hdr(query_rqp, SMB2_CMPD_MIDDLE);
        if (error) {
            SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
            goto bad;
        }
        
        /* Chain Query Info to the Create */
        create_rqp->sr_next_rqp = query_rqp;
    }

    /*
     * Build the Write request
     */
    writep->flags = 0;
    writep->remaining = 0;
    writep->write_flags = write_mode;
    writep->fid = fid;
    writep->auio = uio;
    writep->mc_flags = (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_MC_REPLAY_FLAG):0;
    
    /* assume we can write it all in one request */
    len = uio_resid(writep->auio);
    
    error = smb2_smb_write_one(share, writep, &len, &resid, &write_rqp, create_rqp->sr_iod, context);
    if (error) {
        SMBERROR("smb2_smb_write_one failed %d\n", error);
        goto bad;
    }
    
    /* Manually compound align the write here */
    smb2_rq_align8(write_rqp);
    
    /* Update Write hdr */
    if (add_close) {
        error = smb2_rq_update_cmpd_hdr(write_rqp, SMB2_CMPD_MIDDLE);
    }
    else {
        error = smb2_rq_update_cmpd_hdr(write_rqp, SMB2_CMPD_LAST);
    }
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    if (add_query) {
        /* Chain Write to the Query */
        query_rqp->sr_next_rqp = write_rqp;
    }
    else {
        /* Chain Write to the Create */
        create_rqp->sr_next_rqp = write_rqp;
    }
    
    if (add_close) {
        /*
         * Build the Close request
         */
        error = smb2_smb_close_fid(share, fid, &close_rqp, &closep, create_rqp->sr_iod, context);
        if (error) {
            SMBERROR("smb2_smb_close_fid failed %d\n", error);
            goto bad;
        }
        *close_rqp->sr_flagsp |= (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_FLAGS_REPLAY_OPERATIONS):0;

        /* Update Close hdr */
        error = smb2_rq_update_cmpd_hdr(close_rqp, SMB2_CMPD_LAST);
        if (error) {
            SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
            goto bad;
        }
        
        /* Chain Close to the Write */
        write_rqp->sr_next_rqp = close_rqp;
    }
    
    /*
     * Send the compound request of Create/Query/Write/Close
     */
    error = smb_rq_simple(create_rqp);
    
    if ((error) && (create_rqp->sr_flags & SMBR_RECONNECTED)) {
        SMB_LOG_MC("resending messageid %llu cmd %u (sr_flags: 0x%x).\n", create_rqp->sr_messageid, create_rqp->sr_command, create_rqp->sr_flags);
        if (create_rqp->sr_flags & SMBR_ALT_CH_DISCON) {
            /* An alternate channel got disconnected. Resend with the REPLAY flag set */
            create_flags |= SMB2_CREATE_REPLAY_FLAG;
        }

        /* Rebuild and try sending again */
        smb_rq_done(create_rqp);
        create_rqp = NULL;
        
        if (query_rqp != NULL) {
            smb_rq_done(query_rqp);
            query_rqp = NULL;
        }

        smb_rq_done(write_rqp);
        write_rqp = NULL;
        
        if (close_rqp != NULL) {
            smb_rq_done(close_rqp);
            close_rqp = NULL;
        }
        
        SMB_FREE(createp, M_SMBTEMP);
        createp = NULL;
        
        if (closep != NULL) {
            SMB_FREE(closep, M_SMBTEMP);
            closep = NULL;
        }
        
        goto resend;
    }
    
    createp->ret_ntstatus = create_rqp->sr_ntstatus;
    
    /* Get pointer to response data */
    smb_rq_getreply(create_rqp, &mdp);
    
    if (error) {
        /* Create failed, try parsing the Query */
        if (error != ENOENT) {
            SMBDEBUG("smb_rq_simple failed %d id %lld\n",
                     error, create_rqp->sr_messageid);
        }
        
        if (fidp) {
            *fidp = 0;  /* indicate Create failed */
        }
        goto parse_query;
    }
    
    /*
     * Parse the Create response.
     */
    error = smb2_smb_parse_create(share, mdp, createp);
    if (error) {
        /* Create parsing failed, try parsing the Query */
        SMBERROR("smb2_smb_parse_create failed %d id %lld\n",
                 error, create_rqp->sr_messageid);
        
        if (fidp) {
            *fidp = 0;  /* indicate Create failed */
        }
        goto parse_query;
    }
    
    /* At this point, fid has been entered into fid table */
    need_delete_fid = 1;

    if (fap != NULL) {
        /*
         * Fill in fap and possibly update vnode's meta data caches
         */
        error = smb2fs_smb_parse_ntcreatex(share, dnp, createp,
                                           fidp, fap, context);
        if (error) {
            /* Updating meta data cache failed, try parsing the Query */
            SMBERROR("smb2fs_smb_parse_ntcreatex failed %d id %lld\n",
                     error, create_rqp->sr_messageid);
        }
    }

parse_query:
    if (query_rqp != NULL) {
        /* Consume any pad bytes */
        tmp_error = smb2_rq_next_command(create_rqp, &next_cmd_offset, mdp);
        if (tmp_error) {
            /* Failed to find next command, so can't parse rest of the responses */
            SMBERROR("create smb2_rq_next_command failed %d id %lld\n",
                     tmp_error, create_rqp->sr_messageid);
            error = error ? error : tmp_error;
            goto bad;
        }
        
        /*
         * Parse Query Info SMB 2/3 header
         */
        tmp_error = smb2_rq_parse_header(query_rqp, &mdp);
        queryp->ret_ntstatus = query_rqp->sr_ntstatus;
        if (tmp_error) {
            /* Query Info got an error, try parsing the Close */
            if (!error) {
                SMBDEBUG("query smb2_rq_parse_header failed %d, id %lld\n",
                         tmp_error, query_rqp->sr_messageid);
                error = tmp_error;
            }
            goto parse_write;
        }
        
        /* Parse the Query Info response */
        tmp_error = smb2_smb_parse_query_info(mdp, queryp);
        if (tmp_error) {
            /* Query Info parsing got an error, try parsing the Close */
            if (!error) {
                if (tmp_error != ENOATTR) {
                    SMBERROR("smb2_smb_parse_query_info failed %d id %lld\n",
                             tmp_error, query_rqp->sr_messageid);
                }
                error = tmp_error;
            }
            goto parse_write;
        }
        else {
            /* Query worked, so get the inode number */
            if (fap) {
                fap->fa_ino = inode_number;
                smb2fs_smb_file_id_check(share, fap->fa_ino, NULL, 0);
            }
        }
    }

parse_write:
    /* Consume any pad bytes */
    tmp_error = smb2_rq_next_command(query_rqp != NULL ? query_rqp : create_rqp,
                                     &next_cmd_offset, mdp);
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("create smb2_rq_next_command failed %d id %lld\n",
                 tmp_error, create_rqp->sr_messageid);
        error = error ? error : tmp_error;
        goto bad;
    }
    
    /*
     * Parse Write SMB 2/3 header
     */
    tmp_error = smb2_rq_parse_header(write_rqp, &mdp);
    writep->ret_ntstatus = write_rqp->sr_ntstatus;
    
    if (tmp_error) {
        /* Write got an error, try parsing the Close */
        if (!error) {
            SMBDEBUG("write smb2_rq_parse_header failed %d, id %lld\n",
                     tmp_error, write_rqp->sr_messageid);
            error = tmp_error;
        }
        goto parse_close;
    }
    
    /* Parse the Write response */
    tmp_error = smb2_smb_parse_write_one(mdp, &rresid, writep);
    if (tmp_error) {
        /* Write parsing got an error, try parsing the Close */
        if (!error) {
            SMBERROR("smb2_smb_parse_write_one failed %d id %lld\n",
                     tmp_error, write_rqp->sr_messageid);
            error = tmp_error;
        }
        goto parse_close;
    }
    
parse_close:
    if (close_rqp != NULL) {
        /* Update closep fid so it gets freed from FID table */
        closep->fid = createp->ret_fid;
        
        /* Consume any pad bytes */
        tmp_error = smb2_rq_next_command(write_rqp, &next_cmd_offset, mdp);
        if (tmp_error) {
            /* Failed to find next command, so can't parse rest of the responses */
            SMBERROR("write smb2_rq_next_command failed %d id %lld\n",
                     tmp_error, write_rqp->sr_messageid);
            error = error ? error : tmp_error;
            goto bad;
        }
        
        /*
         * Parse Close SMB 2/3 header
         */
        tmp_error = smb2_rq_parse_header(close_rqp, &mdp);
        closep->ret_ntstatus = close_rqp->sr_ntstatus;
        if (tmp_error) {
            if (!error) {
                SMBDEBUG("close smb2_rq_parse_header failed %d id %lld\n",
                         tmp_error, close_rqp->sr_messageid);
                error = tmp_error;
            }
            goto bad;
        }
        
        /* Parse the Close response */
        tmp_error = smb2_smb_parse_close(mdp, closep);
        if (tmp_error) {
            if (!error) {
                SMBERROR("smb2_smb_parse_close failed %d id %lld\n",
                         tmp_error, close_rqp->sr_messageid);
                error = tmp_error;
            }
            goto bad;
        }
        
        /* At this point, fid has been removed from fid table */
        need_delete_fid = 0;
    }
    else {
        /* Not doing a close, so leave it open */
        need_delete_fid = 0;
    }
    
bad:
    if (need_delete_fid == 1) {
        /*
         * Close failed but the Create worked and was successfully parsed.
         * Try issuing the Close request again.
         */
        tmp_error = smb2_smb_close_fid(share, createp->ret_fid,
                                       NULL, NULL, NULL, context);
        if (tmp_error) {
            SMBERROR("Second close failed %d\n", tmp_error);
        }
    }
    
    if (create_rqp != NULL) {
        smb_rq_done(create_rqp);
    }
    if (query_rqp != NULL) {
        smb_rq_done(query_rqp);
    }
    if (write_rqp != NULL) {
        smb_rq_done(write_rqp);
    }
    if (close_rqp != NULL) {
        smb_rq_done(close_rqp);
    }
    
    if (createp != NULL) {
        SMB_FREE(createp, M_SMBTEMP);
    }
    if (queryp != NULL) {
        SMB_FREE(queryp, M_SMBTEMP);
    }
    if (writep != NULL) {
        SMB_FREE(writep, M_SMBTEMP);
    }
    if (closep != NULL) {
        SMB_FREE(closep, M_SMBTEMP);
    }
    
    return error;
}

static int
smb2fs_smb_cmpd_query(struct smb_share *share, struct smbnode *create_np, enum vtype vnode_type,
                      const char *create_namep, size_t create_name_len,
                      uint32_t create_xattr, uint32_t create_desired_access,
                      uint8_t query_info_type, uint8_t query_file_info_class,
                      uint32_t query_add_info, uint32_t *max_accessp,
                      uint32_t *query_output_buffer_len, uint8_t *query_output_buffer,
                      vfs_context_t context)
{
	int error, tmp_error;
    SMBFID fid = 0;
    struct smbfattr *fap = NULL;
    struct smb2_create_rq *createp = NULL;
    struct smb2_query_info_rq *queryp = NULL;
    struct smb2_close_rq *closep = NULL;
	struct smb_rq *create_rqp = NULL;
	struct smb_rq *query_rqp = NULL;
	struct smb_rq *close_rqp = NULL;
	struct mdchain *mdp;
    size_t next_cmd_offset = 0;
    uint32_t need_delete_fid = 0;
    uint32_t share_access = NTCREATEX_SHARE_ACCESS_ALL;
    uint64_t create_flags = create_xattr ? SMB2_CREATE_IS_NAMED_STREAM : 0;
    char *file_namep = NULL, *stream_namep = NULL;
    size_t file_name_len = 0, stream_name_len = 0;
    int add_submount_path = 0;
    uint32_t create_options = 0;
    uint32_t disposition = FILE_OPEN;
    struct FILE_ALL_INFORMATION *all_infop = NULL;
    
    /*
     * For this function, vnode_type is the item's type. The item will be
     * Opened and the Query Info will be done on the item.
     */

    /*
     * Note: Be careful as 
     * (1) share->ss_mount can be null
     * (2) create_np can be null
     */
    
    SMB_MALLOC(fap,
               struct smbfattr *, 
               sizeof(struct smbfattr), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (fap == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }

    SMB_MALLOC(queryp,
               struct smb2_query_info_rq *,
               sizeof(struct smb2_query_info_rq),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (queryp == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    if ((create_np == NULL) && (create_namep != NULL) &&
        ((query_file_info_class == FileFsAttributeInformation) ||
         (query_file_info_class == FileFsSizeInformation))) {
            /*
             * Must be during mount time which means we do not have the root
             * vnode yet, but we do have a submount path. Submount path is
             * inside the create_namep and already in network form.
             */
            add_submount_path = 1;
        }

resend:
    /*
     * Build the Create call 
     */
    create_flags |= SMB2_CREATE_GET_MAX_ACCESS;
    
    if (add_submount_path == 1) {
        create_flags |= SMB2_CREATE_NAME_IS_PATH;
    }
    
    /*
     * Always check to see if the server is OS X based for
     * FileFsAttributeInformation which is only done at mount time. If the
     * last share is unmounted but the session remains, the macOS server will
     * forget that the session is to a macOS client, but the SMBClient will
     * remember and the mismatch causes all folders to show as access
     * denied because the QueryDir Response is formated for WinOS clients.
     * You can tell because the EASize is zero from the server and that
     * should never be zero for macOS sessions.
     */
    if ((query_info_type == SMB2_0_INFO_FILESYSTEM) &&
        (query_file_info_class == FileFsAttributeInformation)) {
        SMBDEBUG("Checking for OS X server \n");
        create_flags |= SMB2_CREATE_AAPL_QUERY;
    }
    
    if (!(create_flags & SMB2_CREATE_IS_NAMED_STREAM)) {
        file_namep = (char *) create_namep;
        file_name_len = create_name_len;
    }
    else {
        /* create_namep is actually the stream name */
        stream_namep = (char *) create_namep;
        stream_name_len = create_name_len;
    }

    /*
     * Be careful here. The vnode_type is for the actual item that the
     * Query Info is being done on. The path to the item is built from
     * create_np and file_namep, but only the calling function knows what the
     * item type is.
     */
    create_options = smb2fs_smb_get_create_options(share, create_np,
                                                   file_namep, stream_namep,
                                                   vnode_type, 1);
    error = smb2fs_smb_ntcreatex(share, create_np,
                                 file_namep, file_name_len,
                                 stream_namep, stream_name_len,
                                 create_desired_access, vnode_type,
                                 share_access, disposition,
                                 create_flags, create_options,
                                 &fid, fap,
                                 &create_rqp, &createp, 
                                 NULL, context);
    if (error) {
        SMBERROR("smb2fs_smb_ntcreatex failed %d\n", error);
        goto bad;
    }
    
    /* Update Create hdr */
    error = smb2_rq_update_cmpd_hdr(create_rqp, SMB2_CMPD_FIRST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /* 
     * Build the Query Info request 
     */
    queryp->info_type = query_info_type;
    queryp->file_info_class = query_file_info_class;
    queryp->add_info = query_add_info;
    queryp->flags = 0;
    queryp->output_buffer_len = *query_output_buffer_len;
    queryp->output_buffer = query_output_buffer;
    queryp->input_buffer_len = 0;
    queryp->input_buffer = NULL;
    queryp->ret_buffer_len = 0;
    fid = 0xffffffffffffffff;   /* fid is -1 for compound requests */
    queryp->fid = fid;
    queryp->mc_flags = (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_MC_REPLAY_FLAG):0;

    error = smb2_smb_query_info(share, queryp, &query_rqp, create_rqp->sr_iod, context);
    if (error) {
        SMBERROR("smb2_smb_query_info failed %d\n", error);
        goto bad;
    }
    
    /* Update Query hdr */
    error = smb2_rq_update_cmpd_hdr(query_rqp, SMB2_CMPD_MIDDLE);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }

    /* Chain Query Info to the Create */
    create_rqp->sr_next_rqp = query_rqp;
    
    /* 
     * Build the Close request 
     */
    error = smb2_smb_close_fid(share, fid, &close_rqp, &closep, create_rqp->sr_iod, context);
    if (error) {
        SMBERROR("smb2_smb_close_fid failed %d\n", error);
        goto bad;
    }
    *close_rqp->sr_flagsp |= (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_FLAGS_REPLAY_OPERATIONS):0;
    
    /* Update Close hdr */
    error = smb2_rq_update_cmpd_hdr(close_rqp, SMB2_CMPD_LAST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /* Chain Close to the Query Info */
    query_rqp->sr_next_rqp = close_rqp;
    
    /* 
     * Send the compound request of Create/Query/Close 
     */
    error = smb_rq_simple(create_rqp);
    
    if ((error) && (create_rqp->sr_flags & SMBR_RECONNECTED)) {
        SMB_LOG_MC("resending messageid %llu cmd %u (sr_flags: 0x%x).\n", create_rqp->sr_messageid, create_rqp->sr_command, create_rqp->sr_flags);
        if (create_rqp->sr_flags & SMBR_ALT_CH_DISCON) {
            /* An alternate channel got disconnected. Resend with the REPLAY flag set */
            create_flags |= SMB2_CREATE_REPLAY_FLAG;
        }

        /* Rebuild and try sending again */
        smb_rq_done(create_rqp);
        create_rqp = NULL;
        
        smb_rq_done(query_rqp);
        query_rqp = NULL;
        
        smb_rq_done(close_rqp);
        close_rqp = NULL;
        
        SMB_FREE(createp, M_SMBTEMP);
        createp = NULL;
        
        SMB_FREE(closep, M_SMBTEMP);
        closep = NULL;
        
        goto resend;
    }

    createp->ret_ntstatus = create_rqp->sr_ntstatus;

    /* Get pointer to response data */
    smb_rq_getreply(create_rqp, &mdp);
    
    if (error) {
        /* Create failed, try parsing the Query Info */
        if ((error != ENOENT) && (error != EACCES)) {
            SMBDEBUG("smb_rq_simple failed %d id %lld\n",
                     error, create_rqp->sr_messageid);
        }
        
        /* 
         * Some servers return an error with the AAPL Create context instead 
         * of just ignoring the context like it says in the MS-SMB doc.
         * Obviously must be a non OS X Server.
         */
        if (createp->flags & SMB2_CREATE_AAPL_QUERY) {
            SMBDEBUG("Found a NON OS X server\n");
            SS_TO_SESSION(share)->session_misc_flags |= SMBV_OTHER_SERVER;
        }
        
        goto parse_query;
    }
    
    /* 
     * Parse the Create response.
     */
    error = smb2_smb_parse_create(share, mdp, createp);
    if (error) {
        /* Create parsing failed, try parsing the Query Info */
        SMBERROR("smb2_smb_parse_create failed %d id %lld\n", 
                 error, create_rqp->sr_messageid);
        goto parse_query;
    }
    
    /* At this point, fid has been entered into fid table */
    need_delete_fid = 1;
    
    /*
     * Fill in fap and possibly update vnode's meta data caches
     */
    error = smb2fs_smb_parse_ntcreatex(share, create_np, createp, 
                                       &fid, fap, context);
    if (error) {
        /* Updating meta data cache failed, try parsing the Query Info */
        SMBERROR("smb2fs_smb_parse_ntcreatex failed %d id %lld\n", 
                 error, create_rqp->sr_messageid);
    }
    else {
        /* Return max access if they want it */
        if (max_accessp) {
            DBG_ASSERT(fap->fa_valid_mask & FA_MAX_ACCESS_VALID)
            *max_accessp = fap->fa_max_access;
        }
    }
    
parse_query:
    /* Consume any pad bytes */
    tmp_error = smb2_rq_next_command(create_rqp, &next_cmd_offset, mdp);
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("create smb2_rq_next_command failed %d id %lld\n", 
                 tmp_error, create_rqp->sr_messageid);
        error = error ? error : tmp_error;
        goto bad;
    }
    
    /* 
     * Parse Query Info SMB 2/3 header 
     */
    tmp_error = smb2_rq_parse_header(query_rqp, &mdp);
    queryp->ret_ntstatus = query_rqp->sr_ntstatus;
    if (tmp_error) {
        /* Query Info got an error, try parsing the Close */
        if (!error) {
            SMBDEBUG("query smb2_rq_parse_header failed %d, id %lld\n",
                     tmp_error, query_rqp->sr_messageid);
            error = tmp_error;
        }
        goto parse_close;
    }
    
    /* Parse the Query Info response */
    tmp_error = smb2_smb_parse_query_info(mdp, queryp);
    if (tmp_error) {
        /* Query Info parsing got an error, try parsing the Close */
        if (!error) {
            if (tmp_error != ENOATTR) {
                SMBERROR("smb2_smb_parse_query_info failed %d id %lld\n", 
                         tmp_error, query_rqp->sr_messageid);
            }
            error = tmp_error;
        }
        goto parse_close;
    }
    else {
        *query_output_buffer_len = queryp->ret_buffer_len;
        
        if (queryp->file_info_class == FileAllInformation) {
            /*
             * This compound call mallocs its own local fap. Need to copy the
             * fa_reqtime from the local fap, to the returned all_infop->fap
             */
            all_infop = (struct FILE_ALL_INFORMATION *) queryp->output_buffer;
            all_infop->fap->fa_reqtime = fap->fa_reqtime;
        }
    }
    
parse_close:
    /* Update closep fid so it gets freed from FID table */
    closep->fid = createp->ret_fid;
    
    /* Consume any pad bytes */
    tmp_error = smb2_rq_next_command(query_rqp, &next_cmd_offset, mdp);
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("query smb2_rq_next_command failed %d\n", tmp_error);
        error = error ? error : tmp_error;
        goto bad;    
    }
    
    /* 
     * Parse Close SMB 2/3 header
     */
    tmp_error = smb2_rq_parse_header(close_rqp, &mdp);
    closep->ret_ntstatus = close_rqp->sr_ntstatus;
    if (tmp_error) {
        /* Close got an error */
        if (!error) {
            SMBDEBUG("close smb2_rq_parse_header failed %d id %lld\n",
                     tmp_error, close_rqp->sr_messageid);
            error = tmp_error;
        }
        goto bad;    
    }
    
    /* Parse the Close response */
    tmp_error = smb2_smb_parse_close(mdp, closep);
    if (tmp_error) {
        /* Close parsing got an error */
        if (!error) {
            SMBERROR("smb2_smb_parse_close failed %d id %lld\n", 
                     tmp_error, close_rqp->sr_messageid);
            error = tmp_error;
        }
        goto bad;    
    }
    
    /* At this point, fid has been removed from fid table */
    need_delete_fid = 0;
    
bad:
    if (need_delete_fid == 1) {
        /*
         * Close failed but the Create worked and was successfully parsed.
         * Try issuing the Close request again.
         */
        tmp_error = smb2_smb_close_fid(share, createp->ret_fid,
                                       NULL, NULL, NULL, context);
        if (tmp_error) {
            SMBERROR("Second close failed %d\n", tmp_error);
        }
    }
    
    if (create_rqp != NULL) {
        smb_rq_done(create_rqp);
    }
    if (query_rqp != NULL) {
        smb_rq_done(query_rqp);
    }
    if (close_rqp != NULL) {
        smb_rq_done(close_rqp);
    }
    
    if (createp != NULL) {
        SMB_FREE(createp, M_SMBTEMP);
    }
    if (queryp != NULL) {
        SMB_FREE(queryp, M_SMBTEMP);
    }
    if (closep != NULL) {
        SMB_FREE(closep, M_SMBTEMP);
    }
    
    if (fap != NULL) {
        SMB_FREE(fap, M_SMBTEMP);
    }

	return error;
}

int
smb2fs_smb_cmpd_query_async(struct smb_share *share, vnode_t dvp,
                            void *in_cachep, int32_t flags,
                            vfs_context_t context)
{
    struct smb_enum_cache *cachep = in_cachep;
    struct smbnode *dnp = NULL;
    int error = 0, tmp_error;
    int32_t nbr_to_update = 0;
    int i, j;
    struct cached_dir_entry *currp = NULL;
    struct cached_dir_entry *entryp = NULL;
    int32_t count = 0;
    struct mdchain *mdp;
    struct compound_pb *pb = NULL;
    int done = 0;
    uint32_t ret_ntstatus = 0;
    struct smbmount *smp = NULL;
    int32_t dir_cache_async_cnt = 10;
    
    if ((flags & kDirCacheGetStreamInfo) && (flags & kDirCacheGetFinderInfo)) {
        /* Both can not be set at same time */
        SMBERROR("Illegal flags 0x%x \n", flags);
        return (EINVAL);
    }
    
    if (dvp == NULL) {
        SMBERROR("dvp is null \n");
        return (EINVAL);
    }
    dnp = VTOSMB(dvp);
    
    smp = VFSTOSMBFS(vnode_mount(dvp));
    if (smp != NULL) {
        if ((smp->sm_args.dir_cache_async_cnt != 0) &&
            (smp->sm_args.dir_cache_async_cnt <= 100)){
            dir_cache_async_cnt = smp->sm_args.dir_cache_async_cnt;
        }
    }
    SMB_LOG_DIR_CACHE("async cnt %d flags 0x%x\n", dir_cache_async_cnt, flags);
    
    /*
     * Find first entry that needs Meta Data or Finder Info.
     * A linear search is not that efficient, but its simple and should be
     * reliable. Plus I do not expect the dir cache to get too big.
     */
    for (currp = cachep->list; currp; currp = currp->next) {
        if (((flags & kDirCacheGetStreamInfo) && (currp->flags & kCacheEntryNeedsMetaData)) ||
            ((flags & kDirCacheGetFinderInfo) && (currp->flags & kCacheEntryNeedsFinderInfo))) {
            break;
        }
    }
    
    if (currp == NULL) {
        error = 0;
        goto bad;
    }
    
    SMB_MALLOC(pb,
               struct compound_pb *,
               sizeof(struct compound_pb) * dir_cache_async_cnt,
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    
    /* Zero out param blocks */
    for (i = 0; i < dir_cache_async_cnt; i++) {
        pb[i].dnp = NULL;
        pb[i].entryp = NULL;
        
        pb[i].createp = NULL;
        pb[i].queryp = NULL;
        pb[i].closep = NULL;
        pb[i].stream_infop = NULL;
        
        pb[i].create_rqp = NULL;
        pb[i].query_rqp = NULL;
        pb[i].close_rqp = NULL;
        
        bzero(&pb[i].fattr, sizeof(pb[i].fattr));
        pb[i].fid = 0;
        pb[i].rsrc_size = 0;
        pb[i].alloc_rsrc_size = 0;
        pb[i].stream_flags = 0;
        
        bzero(&pb[i].finfo, sizeof(pb[i].finfo));
        pb[i].finfo_uio = NULL;
        
        pb[i].pending = 0;
    }
    
    /*
     * Count how many entries need Meta Data or Finder Info.
     * All entries will need their Meta Data fetched, but only those items that
     * have Finder Info will need the Finder Info to be read.
     */
    SMB_LOG_DIR_CACHE("Starting fetch data at <%s> \n", currp->name);
    for (entryp = currp; entryp; entryp = entryp->next) {
        if ((flags & kDirCacheGetStreamInfo) ||
            ((flags & kDirCacheGetFinderInfo) && (entryp->flags & kCacheEntryNeedsFinderInfo))) {
            SMB_LOG_DIR_CACHE2("counting <%s> \n", entryp->name);
            nbr_to_update += 1;
        }
    }
    SMB_LOG_DIR_CACHE("nbr_to_update %d \n", nbr_to_update);
    
    /* Dont exceed max async request count */
    count = nbr_to_update;
    if (nbr_to_update > dir_cache_async_cnt) {
        count = dir_cache_async_cnt;
    }
    
    /* Fill in initial requests */
    for (i = 0; i < count; i++) {
        if (currp == NULL) {
            SMBERROR("curr is NULL? \n");
            error = EINVAL;
            goto bad;
        }
        
        SMB_MALLOC(pb[i].queryp,
                   struct smb2_query_info_rq *,
                   sizeof(struct smb2_query_info_rq),
                   M_SMBTEMP,
                   M_WAITOK | M_ZERO);
        
        SMB_MALLOC(pb[i].readp,
                   struct smb2_rw_rq *,
                   sizeof(struct smb2_rw_rq),
                   M_SMBTEMP,
                   M_WAITOK | M_ZERO);
        
        SMB_MALLOC(pb[i].stream_infop,
                   struct FILE_STREAM_INFORMATION *,
                   sizeof(struct FILE_STREAM_INFORMATION),
                   M_SMBTEMP,
                   M_WAITOK | M_ZERO);
        
        SMB_MALLOC(pb[i].closep,
                   struct smb2_close_rq *,
                   sizeof(struct smb2_close_rq),
                   M_SMBTEMP,
                   M_WAITOK | M_ZERO);

        /*
         * Fill in the Create/GetInfo/Close requests
         */
        error = smb2fs_smb_cmpd_query_async_fill(share, dnp, currp, &pb[i],
                                                 flags, context);
        
        if (error) {
            if (error == ENOBUFS) {
                /* Running out of credits, clear error and send what we have */
                SMBWARNING("low on credits %d\n", error);
                error = 0;
                
                /* In this case, if the fill failed, dont increment i */
                break;
            }
            else {
                SMBWARNING("smb2fs_smb_cmpd_query_async_fill failed %d\n", error);
                goto bad;
            }
        }
        
        /* On to next dir cache entry */
        if (flags & kDirCacheGetStreamInfo) {
            currp = currp->next;
        }
        else {
            do {
                currp = currp->next;
            } while ((currp != NULL) &&
                     !(currp->flags & kCacheEntryNeedsFinderInfo));
        }
    }
    SMB_LOG_DIR_CACHE("Inital requests %d \n", i);
    
    nbr_to_update -= i;
    
    /*
     * Send initial requests
     */
    for (j = 0; j < i; j++) {
        error = smb_iod_rq_enqueue(pb[j].create_rqp);
        if (error) {
			if (error != ETIMEDOUT) {
				SMBWARNING("smb_iod_rq_enqueue failed %d\n", error);
			}
            goto bad;
        }
        pb[j].pending = 1;
    }
    
    /* Wait for replies and refill requests as needed */
    while (!done) {
        /* Assume we are done */
        done = 1;
        
        for (j = 0; j < i; j++) {
            if (pb[j].pending == 1) {
                error = smb_rq_reply(pb[j].create_rqp);
                
                pb[j].pending = 0;
                if (error) {
                    /*
                     * If its due to reconnect, then exit as we will have
                     * to refill the dir cache anyways since we dont know the
                     * state of that dir on the server anymore.
                     */
                    if ((error == ETIMEDOUT) ||
						(pb[j].create_rqp->sr_flags & SMBR_RECONNECTED)) {
                        SMBWARNING("reconnected on Create/GetInfo/Close[%d] on <%s>\n",
								   j, pb[j].createp->namep);
                        goto bad;
                    }
                    else {
                        /* Go ahead and attempt to parse and refill */
                        SMBWARNING("smb_rq_reply failed %d ntstatus 0x%x on <%s> \n",
								   error, pb[j].create_rqp->sr_ntstatus,
								   pb[j].createp->namep);
                    }
                }
                
                if (!ret_ntstatus) {
                    ret_ntstatus = pb[j].create_rqp->sr_ntstatus;
                }
                
                /* Now get pointer to response data */
                smb_rq_getreply(pb[j].create_rqp, &mdp);
                
                error = smb2fs_smb_cmpd_query_async_parse(share, mdp, &pb[j],
                                                          flags, context);
                if (error) {
                    SMBWARNING("smb2fs_smb_cmpd_query_async_parse failed %d on <%s>\n",
							   error, pb[j].createp->namep);
                    goto bad;
                }
                
                if (nbr_to_update > 0) {
                    /* More meta data to retrieve */
                    error = smb2fs_smb_cmpd_query_async_fill(share, dnp, currp, &pb[j],
                                                             flags, context);
                    
                    if (error) {
                        if (error == ENOBUFS) {
                            done = 0;   /* Not done yet */
                            continue;   /* Wait for more credits to arrive */
                        }
                        
                        SMBWARNING("smb2fs_smb_cmpd_query_async_fill2 failed %d on <%s> \n",
								   error, pb[j].createp->namep);
                        goto bad;
                    }
                    
                    /* On to next dir cache entry */
                    if (flags & kDirCacheGetStreamInfo) {
                        currp = currp->next;
                    }
                    else {
                        do {
                            currp = currp->next;
                        } while ((currp != NULL) &&
                                 !(currp->flags & kCacheEntryNeedsFinderInfo));
                    }
                    
                    nbr_to_update -= 1;
                    
                    error = smb_iod_rq_enqueue(pb[j].create_rqp);
                    if (error) {
                        SMBWARNING("smb_iod_rq_enqueue failed %d on <%s> \n",
								   error, pb[j].createp->namep);
                        goto bad;
                    }
                    pb[j].pending = 1;
                    
                    /* Not done yet */
                    done = 0;
                }
            }
        }
    }
    
bad:
    if (pb != NULL) {
        /* Cleanup time */
        for (i = 0; i < dir_cache_async_cnt; i++) {
            /* If it has not finished, then wait for it to finish */
            if (pb[i].pending == 1) {
                error = smb_rq_reply(pb[i].create_rqp);
                pb[i].pending = 0;
            }
            
            if (pb[i].fid != 0) {
                /*
                 * Close failed but the Create worked and was successfully parsed.
                 * Try issuing the Close request again.
                 */
                tmp_error = smb2_smb_close_fid(share, pb[i].fid,
                                               NULL, NULL, NULL, context);
                if (tmp_error) {
                    SMBWARNING("Second close failed %d\n", tmp_error);
                }
                pb[i].fid = 0;
            }
            
            if (pb[i].create_rqp != NULL) {
                smb_rq_done(pb[i].create_rqp);
                pb[i].create_rqp = NULL;
            }
            
            if (pb[i].query_rqp != NULL) {
                smb_rq_done(pb[i].query_rqp);
                pb[i].query_rqp = NULL;
            }
            
            if (pb[i].read_rqp != NULL) {
                smb_rq_done(pb[i].read_rqp);
                pb[i].read_rqp = NULL;
            }

            if (pb[i].close_rqp != NULL) {
                smb_rq_done(pb[i].close_rqp);
                pb[i].close_rqp = NULL;
            }
            
            if (pb[i].createp != NULL) {
                SMB_FREE(pb[i].createp, M_SMBTEMP);
                pb[i].createp = NULL;
            }
            
            if (pb[i].queryp != NULL) {
                SMB_FREE(pb[i].queryp, M_SMBTEMP);
                pb[i].queryp = NULL;
            }
            
            if (pb[i].closep != NULL) {
                SMB_FREE(pb[i].closep, M_SMBTEMP);
                pb[i].closep = NULL;
            }
            
            if (pb[i].stream_infop != NULL) {
                SMB_FREE(pb[i].stream_infop, M_SMBTEMP);
                pb[i].stream_infop = NULL;
            }
        }
        
        SMB_FREE(pb, M_SMBTEMP);
        pb = NULL;
    }
    
    return error;
}

static void
smb2fs_smb_cmpd_query_async_clear(struct compound_pb *pb)
{
    /* Clear out any previous values */
    if (pb->create_rqp != NULL) {
        smb_rq_done(pb->create_rqp);
        pb->create_rqp = NULL;
    }
    
    if (pb->query_rqp != NULL) {
        smb_rq_done(pb->query_rqp);
        pb->query_rqp = NULL;
    }
    
    if (pb->read_rqp != NULL) {
        smb_rq_done(pb->read_rqp);
        pb->read_rqp = NULL;
    }
    
    if (pb->close_rqp != NULL) {
        smb_rq_done(pb->close_rqp);
        pb->close_rqp = NULL;
    }
    
    if (pb->createp != NULL) {
        SMB_FREE(pb->createp, M_SMBTEMP);
        pb->createp = NULL;
    }
    
    if (pb->finfo_uio != NULL) {
        uio_free(pb->finfo_uio);
        pb->finfo_uio = NULL;
    }
    
    pb->dnp = NULL;
    pb->entryp = NULL;
    bzero(pb->queryp, sizeof(struct smb2_query_info_rq));
    bzero(pb->readp, sizeof(struct smb2_rw_rq));
    bzero(pb->stream_infop, sizeof(struct FILE_STREAM_INFORMATION));
    bzero(pb->closep, sizeof(struct smb2_close_rq));
    bzero(&pb->fattr, sizeof(struct smbfattr));
    bzero(&pb->fid, sizeof(SMBFID));
    pb->rsrc_size = 0;
    pb->alloc_rsrc_size = 0;
    pb->stream_flags = 0;
    bzero(&pb->finfo, sizeof(pb->finfo));
    
    pb->pending = 0;
}


int
smb2fs_smb_cmpd_query_async_fill(struct smb_share *share, struct smbnode *dnp,
                                 struct cached_dir_entry *currp, struct compound_pb *pb,
                                 int32_t flags, vfs_context_t context)
{
    int error = 0;
    uint32_t create_desired_access = SMB2_FILE_READ_ATTRIBUTES |
                                     SMB2_SYNCHRONIZE;
    uint32_t share_access = NTCREATEX_SHARE_ACCESS_ALL;
    uint32_t disposition = FILE_OPEN;
    uint64_t create_flags = SMB2_CREATE_GET_MAX_ACCESS;
    uint32_t create_options = 0;
    uint32_t query_output_buffer_len = 32 * 1024;
    user_ssize_t len, resid = 0;
    const char *snamep = NULL;
    size_t sname_len = 0;
    enum vtype vnode_type = currp->fattr.fa_vtype;
    
    /* Clear out any previous values */
    smb2fs_smb_cmpd_query_async_clear(pb);

    /* Start filling pb in */
    pb->dnp = dnp;
    pb->entryp = currp;
    
    /* Fill in Finder Info pb if needed */
    if (flags & kDirCacheGetFinderInfo) {
        pb->finfo_uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
        if (pb->finfo_uio == NULL) {
            SMBERROR("uio_create failed for %s \n", currp->name);
            error = ENOMEM;
            goto bad;
        }
        
        error = uio_addiov(pb->finfo_uio, CAST_USER_ADDR_T(pb->finfo),
                           sizeof(pb->finfo));
        if (error) {
            SMBERROR("uio_addiov failed for %s \n", currp->name);
            goto bad;
        }
        
        uio_setoffset(pb->finfo_uio, 0);
        
        create_desired_access = SMB2_FILE_READ_DATA;
        create_flags = 0;
        
        snamep = SFM_FINDERINFO_NAME;
        sname_len = strlen(SFM_FINDERINFO_NAME);
        vnode_type = VREG; /* streams are always files */
    }

    create_options = smb2fs_smb_get_create_options(share, dnp,
                                                   currp->name, NULL,
                                                   vnode_type, 1);

    create_flags |= SMB2_CMD_NO_BLOCK;
    error = smb2fs_smb_ntcreatex(share, dnp,
                                 currp->name, currp->name_len,
                                 snamep, sname_len,
                                 create_desired_access, vnode_type,
                                 share_access, disposition,
                                 create_flags, create_options,
                                 &pb->fid, &pb->fattr,
                                 &pb->create_rqp, &pb->createp,
                                 NULL, context);
    if (error) {
		if (error != ENOBUFS) {
			SMBERROR("smb2fs_smb_ntcreatex failed %d\n", error);
		}
        goto bad;
    }

    error = smb2_rq_update_cmpd_hdr(pb->create_rqp, SMB2_CMPD_FIRST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    if (flags & kDirCacheGetStreamInfo) {
        /*
         * Build the Query Info request
         */
        pb->stream_infop->share = share;
        pb->stream_infop->np = dnp;
        pb->stream_infop->namep = currp->name;
        pb->stream_infop->name_len = currp->name_len;
        pb->stream_infop->uio = NULL;
        pb->stream_infop->stream_buf_sizep = NULL;
        pb->stream_infop->stream_namep = SFM_RESOURCEFORK_NAME;
        pb->stream_infop->stream_sizep = &pb->rsrc_size;
        pb->stream_infop->stream_alloc_sizep = &pb->alloc_rsrc_size;
        pb->stream_infop->stream_flagsp = &pb->stream_flags;
        
        pb->queryp->info_type = SMB2_0_INFO_FILE;
        pb->queryp->file_info_class = FileStreamInformation;
        pb->queryp->add_info = 0;
        pb->queryp->flags = SMB2_CMD_NO_BLOCK;
        pb->queryp->output_buffer_len = query_output_buffer_len;
        pb->queryp->output_buffer = (uint8_t *) pb->stream_infop;
        pb->queryp->input_buffer_len = 0;
        pb->queryp->input_buffer = NULL;
        pb->queryp->ret_buffer_len = 0;
        pb->queryp->fid = 0xffffffffffffffff;
        pb->queryp->mc_flags = 0;

        error = smb2_smb_query_info(share, pb->queryp, &pb->query_rqp, pb->create_rqp->sr_iod, context);
        if (error) {
			if (error != ENOBUFS) {
				SMBERROR("smb2_smb_query_info failed %d\n", error);
			}
            goto bad;
        }
        
        /* Update Query hdr */
        error = smb2_rq_update_cmpd_hdr(pb->query_rqp, SMB2_CMPD_MIDDLE);
        if (error) {
            SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
            goto bad;
        }
        
        /* Chain Query Info to the Create */
        pb->create_rqp->sr_next_rqp = pb->query_rqp;
    }
    else {
        /*
         * Build the Read request
         */
        pb->readp->flags = SMB2_CMD_NO_BLOCK;
        pb->readp->remaining = 0;
        pb->readp->write_flags = 0;
        pb->readp->fid = 0xffffffffffffffff;
        pb->readp->auio = pb->finfo_uio;
        pb->readp->mc_flags = 0;
        
        /* assume we can read it all in one request */
        len = uio_resid(pb->readp->auio);
        
        error = smb2_smb_read_one(share, pb->readp, &len, &resid,
                                  &pb->read_rqp, pb->create_rqp->sr_iod,
                                  context);
        if (error) {
			if (error != ENOBUFS) {
				SMBERROR("smb2_smb_read_one failed %d\n", error);
			}
            goto bad;
        }
        
        /* Update Read hdr */
        error = smb2_rq_update_cmpd_hdr(pb->read_rqp, SMB2_CMPD_MIDDLE);
        if (error) {
            SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
            goto bad;
        }
        
        /* Chain Read to the Create */
        pb->create_rqp->sr_next_rqp = pb->read_rqp;
    }

    /*
     * Build the Close request
     */
    pb->closep->share = share;
    pb->closep->flags = SMB2_CMD_NO_BLOCK;
    pb->closep->fid = 0xffffffffffffffff;
    pb->closep->mc_flags = 0;

    error = smb2_smb_close(share, pb->closep, &pb->close_rqp, pb->create_rqp->sr_iod, context);

    if (error) {
		if (error != ENOBUFS) {
			SMBERROR("smb2_smb_close_fid failed %d\n", error);
		}
        goto bad;
    }
    
    /* Update Close hdr */
    error = smb2_rq_update_cmpd_hdr(pb->close_rqp, SMB2_CMPD_LAST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    if (flags & kDirCacheGetStreamInfo) {
        /* Chain Close to the Query Info */
        pb->query_rqp->sr_next_rqp = pb->close_rqp;
    }
    else {
        /* Chain Close to the Read */
        pb->read_rqp->sr_next_rqp = pb->close_rqp;
    }
    
    pb->create_rqp->sr_timo = pb->create_rqp->sr_session->session_timo;
    pb->create_rqp->sr_state = SMBRQ_NOTSENT;

bad:
    
    if (error) {
        /* Clear out any previous values */
        smb2fs_smb_cmpd_query_async_clear(pb);
    }
    
    return (error);
}

int
smb2fs_smb_cmpd_query_async_parse(struct smb_share *share, struct mdchain *mdp, struct compound_pb *pb,
                                  int32_t flags, vfs_context_t context)
{
    int error = 0, tmp_error = 0;
    size_t next_cmd_offset = 0;
    user_ssize_t rresid = 0;
    
    if (pb->entryp == NULL) {
        /* Should never happen */
        SMBERROR("pb->entryp is NULL? \n");
        return(EINVAL);
    }
    
    /* Save any errors */
    pb->createp->ret_ntstatus = pb->create_rqp->sr_ntstatus;
    pb->entryp->create_ntstatus = pb->create_rqp->sr_ntstatus;
	
	/* 
	 * If the create got EACCES, then nothing to parse in the create.
	 * Still have to parse the query and close to get back the credits
	 */
	tmp_error = smb_ntstatus_to_errno(pb->createp->ret_ntstatus);
	if (tmp_error == EACCES) {
		error = EACCES;
		goto parse_query;
	}
	
	/*
     * Parse the Create response.
     */
    error = smb2_smb_parse_create(share, mdp, pb->createp);
    if (error) {
        /* Create parsing failed, try parsing the Query Info */
        SMBERROR("smb2_smb_parse_create failed %d id %lld\n",
                 error, pb->create_rqp->sr_messageid);
        goto parse_query;
    }
    
    /* At this point, fid has been entered into fid table */
    
    /*
     * Fill in fap and possibly update vnode's meta data caches
     *
     * fid != 0 means that the fid needs to be closed.
     */
    error = smb2fs_smb_parse_ntcreatex(share, pb->dnp, pb->createp,
                                       &pb->fid, &pb->fattr, context);
    if (error) {
        /* Updating meta data cache failed, try parsing the Query Info */
        SMBERROR("smb2fs_smb_parse_ntcreatex failed %d id %lld\n",
                 error, pb->create_rqp->sr_messageid);
    }
    
parse_query:
    /* Consume any pad bytes */
    tmp_error = smb2_rq_next_command(pb->create_rqp, &next_cmd_offset, mdp);
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("create smb2_rq_next_command failed %d id %lld\n",
                 tmp_error, pb->create_rqp->sr_messageid);
        error = error ? error : tmp_error;
        goto bad;
    }
    
    if (flags & kDirCacheGetStreamInfo) {
        /*
         * Parse Query Info SMB 2/3 header
         */
        tmp_error = smb2_rq_parse_header(pb->query_rqp, &mdp);
        pb->queryp->ret_ntstatus = pb->query_rqp->sr_ntstatus;
        pb->entryp->query_ntstatus = pb->queryp->ret_ntstatus;
        if (tmp_error) {
            /* Query Info got an error, try parsing the Close */
            if (!error) {
                SMBDEBUG("query smb2_rq_parse_header failed %d, id %lld\n",
                         tmp_error, pb->query_rqp->sr_messageid);
                error = tmp_error;
            }
            goto parse_close;
        }
        
        /* Parse the Query Info response */
        tmp_error = smb2_smb_parse_query_info(mdp, pb->queryp);
        if (tmp_error) {
            /* Query Info parsing got an error, try parsing the Close */
            if (!error) {
                if (tmp_error != ENOATTR) {
                    SMBERROR("smb2_smb_parse_query_info failed %d id %lld\n",
                             tmp_error, pb->query_rqp->sr_messageid);
                }
                error = tmp_error;
            }
            goto parse_close;
        }
    }
    else {
        /*
         * Parse Read SMB 2/3 header
         */
        tmp_error = smb2_rq_parse_header(pb->read_rqp, &mdp);
        pb->readp->ret_ntstatus = pb->read_rqp->sr_ntstatus;
        pb->entryp->read_ntstatus = pb->readp->ret_ntstatus;
        if (tmp_error) {
            if (tmp_error == ENODATA) {
                /*
                 * Ignore EOF and leave error alone as it could contain an error from
                 * the create, but skip trying to parse the read data
                 */
            }
            else {
                /* Read got an error, try parsing the Close */
                if (!error) {
                    SMBDEBUG("read smb2_rq_parse_header failed %d, id %lld\n",
                             tmp_error, pb->read_rqp->sr_messageid);
                    error = tmp_error;
                }
            }
            goto parse_close;
        }
        
        /* Parse the Read response */
        tmp_error = smb2_smb_parse_read_one(mdp, &rresid, pb->readp);
        if (tmp_error) {
            /* Read parsing got an error, try parsing the Close */
            if (!error) {
                SMBERROR("smb2_smb_parse_read_one failed %d id %lld\n",
                         tmp_error, pb->read_rqp->sr_messageid);
                error = tmp_error;
            }
            goto parse_close;
        }

        if (rresid != pb->readp->io_len) {
            /*
             * Not doing a vnop_read, so probably ok to get a short read
             */
            SMBWARNING("IO Mismatched. Requested %lld but got %lld\n",
                       pb->readp->io_len, rresid);
        }

    }
    
parse_close:
    /* Update closep fid so it gets freed from FID table */
    pb->closep->fid = pb->createp->ret_fid;
    
    /* Consume any pad bytes */
    if (flags & kDirCacheGetStreamInfo) {
        tmp_error = smb2_rq_next_command(pb->query_rqp, &next_cmd_offset, mdp);
    }
    else {
        tmp_error = smb2_rq_next_command(pb->read_rqp, &next_cmd_offset, mdp);
    }
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("query/read smb2_rq_next_command failed %d\n", tmp_error);
        error = error ? error : tmp_error;
        goto bad;
    }
    
    /*
     * Parse Close SMB 2/3 header
     */
    tmp_error = smb2_rq_parse_header(pb->close_rqp, &mdp);
    pb->closep->ret_ntstatus = pb->close_rqp->sr_ntstatus;
    if (tmp_error) {
        /* Close got an error */
        if (!error) {
            SMBDEBUG("close smb2_rq_parse_header failed %d id %lld\n",
                     tmp_error, pb->close_rqp->sr_messageid);
            error = tmp_error;
        }
        goto update_dir_cache;
    }
    
    /* Parse the Close response */
    tmp_error = smb2_smb_parse_close(mdp, pb->closep);
    if (tmp_error) {
        /* Close parsing got an error */
        if (!error) {
            SMBERROR("smb2_smb_parse_close failed %d id %lld\n",
                     tmp_error, pb->close_rqp->sr_messageid);
            error = tmp_error;
        }
        goto update_dir_cache;
    }
    
    /* At this point, fid has been removed from fid table */
    pb->fid = 0;
    
update_dir_cache:
    /*
     * Save this entry's data into the dir cache
     */
    
    /*
     * If its Create/QueryStream/Close, then ENOATTR is fine.
     */
    if ((error == ENOATTR) && (flags & kDirCacheGetStreamInfo)) {
        error = 0;
    }

    if (error) {
        /* 
		 * Got some sort of error. This shouldn't happen
		 * Dont log EACCES as it is a common error
		 */
		if (error != EACCES) {
			SMBERROR("smb2_smb_parse_query_one failed %d for %s \n",
					 error, pb->entryp->name);
		}
        goto bad;
    }
    
    if (flags & kDirCacheGetStreamInfo) {
        /* 
         * smbfs_smb_qstreaminfo worked so we now have
         * 1) Max Access
         * 2) If its a file, we know if it has a resource fork exists or not.
         *    If it does, we have its alloc and logical size.
         * 3) We know if it has Finder Info, but it has not been read in yet.
         */
        pb->entryp->flags &= ~kCacheEntryNeedsMetaData;
        
        if (pb->stream_flags & SMB_NO_SUBSTREAMS) {
            /* No named streams at all on item */
            pb->entryp->fattr.fa_valid_mask |= FA_FSTATUS_VALID;
            pb->entryp->fattr.fa_fstatus = kNO_SUBSTREAMS;
            //SMB_LOG_DIR_CACHE("No streams for <%s> \n", pb->entryp->name);
        }
        else {
            /* At least one named stream on item */
            pb->entryp->fattr.fa_valid_mask |= FA_FSTATUS_VALID;
            pb->entryp->fattr.fa_fstatus = 0;
            SMB_LOG_DIR_CACHE("Named streams for <%s> \n", pb->entryp->name);
        }
        
        if (pb->entryp->fattr.fa_vtype == VREG) {
            /*
             * Successfully got Resource Fork Info. Its either already in
             * f_attr or no Resource Fork was found
             */
            pb->entryp->fattr.fa_valid_mask |= FA_RSRC_FORK_VALID;
            
            if (pb->stream_flags & SMB_NO_RESOURCE_FORK) {
                /* No Resource Fork, so set resource fork lengths to zero */
                pb->entryp->fattr.fa_rsrc_size = 0;
                pb->entryp->fattr.fa_rsrc_alloc = 0;
                pb->entryp->fattr.fa_fstatus |= kNO_RSRCFORK;
                //SMB_LOG_DIR_CACHE("No rsrc fork for <%s> \n", pb->entryp->name);
            }
            else {
                pb->entryp->fattr.fa_rsrc_size = pb->fattr.fa_rsrc_size;
                pb->entryp->fattr.fa_rsrc_alloc = pb->fattr.fa_rsrc_alloc;
                SMB_LOG_DIR_CACHE("Rsrc fork %lld/%lld for <%s> \n",
                                  pb->entryp->fattr.fa_rsrc_size,
                                  pb->entryp->fattr.fa_rsrc_alloc,
                                  pb->entryp->name);
            }
        }
        
        /*
         * Now we know if there is Finder Info or not on the item
         */
        if (pb->stream_flags & SMB_NO_FINDER_INFO) {
            /* No Finder Info, so set Finder Info to all zeros */
            pb->entryp->fattr.fa_valid_mask |= FA_FINDERINFO_VALID;
            bzero(pb->entryp->fattr.fa_finder_info, sizeof(u_int8_t) * 32);
            pb->entryp->fattr.fa_fstatus |= kNO_FINDERINFO;
            //SMB_LOG_DIR_CACHE("No Finder Info for <%s> \n", pb->entryp->name);
        }
        else {
            /* Still need to read Finder Info for this entry */
            pb->entryp->flags |= kCacheEntryNeedsFinderInfo;
            SMB_LOG_DIR_CACHE("Finder Info for <%s> \n", pb->entryp->name);
        }
        
        /* Save the Max Access */
        pb->entryp->fattr.fa_valid_mask |= FA_MAX_ACCESS_VALID;
        pb->entryp->fattr.fa_max_access = pb->fattr.fa_max_access;
        
        /*SMB_LOG_DIR_CACHE("Max access 0x%x <%s> \n",
                          pb->entryp->fattr.fa_max_access,
                          pb->entryp->name);*/
    }
    else {
        /* 
         * Successfully read in the Finder Info
         */
        pb->entryp->flags &= ~kCacheEntryNeedsFinderInfo;
        pb->entryp->fattr.fa_valid_mask |= FA_FINDERINFO_VALID;
        
        /* Verify returned size */
        if (pb->readp->ret_len != AFP_INFO_SIZE) {
            /* Could be a 0 size returned meaning no Finder Info */
            if (pb->readp->ret_len != 0) {
                SMB_LOG_DIR_CACHE("%s Finder Info size mismatch %u != %d \n",
                                  pb->entryp->name,
                                  pb->readp->ret_len, AFP_INFO_SIZE);
            }
        }
        else {
            /* Correct size, so just copy it in */
            bcopy(&pb->finfo[AFP_INFO_FINDER_OFFSET],
                  pb->entryp->fattr.fa_finder_info,
                  sizeof(u_int8_t) * 32);
#if 1
            SMB_LOG_DIR_CACHE("%s Finder Info retrieved \n",
                              pb->entryp->name);
#else
            SMB_LOG_DIR_CACHE("Finder Info 0x%x 0x%x 0x%x 0x%x for %s \n",
                              pb->entryp->fattr.fa_finder_info[0],
                              pb->entryp->fattr.fa_finder_info[1],
                              pb->entryp->fattr.fa_finder_info[2],
                              pb->entryp->fattr.fa_finder_info[3],
                              pb->entryp->name);
#endif
        }
    }
 
bad:
    if (error) {
        /*
         * Error Handling. At this point, we should have valid information and if
         * we do not, then some earlier error must have occurred, so fill in with
         * default values.
         */
        if (flags & kDirCacheGetStreamInfo) {
            /* Assume no streams */
            pb->entryp->flags &= ~kCacheEntryNeedsMetaData;
            
            pb->entryp->fattr.fa_valid_mask |= FA_FSTATUS_VALID;
            pb->entryp->fattr.fa_fstatus = kNO_SUBSTREAMS;
            
            SMB_LOG_DIR_CACHE("Assuming no streams for <%s> \n",
                              pb->entryp->name);
        }
        else {
            /* Assume zero Finder Info */
            pb->entryp->flags &= ~kCacheEntryNeedsFinderInfo;
            
            pb->entryp->fattr.fa_valid_mask |= FA_FINDERINFO_VALID;
            bzero(&pb->entryp->fattr.fa_finder_info,
                  sizeof(pb->entryp->fattr.fa_finder_info));
            
            SMB_LOG_DIR_CACHE("Assuming zero Finder Info for <%s> \n",
                              pb->entryp->name);
        }
        
        if (!(pb->entryp->fattr.fa_valid_mask & FA_MAX_ACCESS_VALID)) {
			tmp_error = 0;
			if (pb->createp) {
				tmp_error = smb_ntstatus_to_errno(pb->createp->ret_ntstatus);
			}

			pb->entryp->fattr.fa_valid_mask |= FA_MAX_ACCESS_VALID;
			
			if (tmp_error == EACCES) {
				/* Assume no access */
				pb->entryp->fattr.fa_max_access = 0;
				
				SMB_LOG_DIR_CACHE("Assuming no access for <%s> \n",
								  pb->entryp->name);
			}
			else {
				/* Assume full access */
				pb->entryp->fattr.fa_max_access = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;
				
				SMB_LOG_DIR_CACHE("Assuming full max access for <%s> \n",
								  pb->entryp->name);
			}
        }
        
        /* Reset error to 0 */
        error = 0;
    }
    
    return(error);
}

static int
smb2fs_smb_cmpd_query_dir(struct smbfs_fctx *ctx,
                          struct smb2_query_dir_rq *queryp,
                          vfs_context_t context)
{
    int error, tmp_error;
	struct mdchain *mdp;
    size_t next_cmd_offset = 0;
	struct smb_rq *create_rqp = NULL;
	struct smb_rq *query_rqp = NULL;
    struct smb2_create_rq *createp = NULL;
    struct smbnode *create_np = NULL;
    uint32_t desired_access = SMB2_FILE_READ_ATTRIBUTES | SMB2_FILE_LIST_DIRECTORY | SMB2_SYNCHRONIZE;
    uint32_t share_access = NTCREATEX_SHARE_ACCESS_ALL;
    uint64_t create_flags = 0;
    uint32_t create_options = 0;
    uint32_t disposition = FILE_OPEN;
    enum vtype vnode_type = VDIR;
    int n_parent_locked = 0;
    vnode_t par_vp = NULL;
	struct smb2_durable_handle *dur_handlep = NULL;
	struct smb_session *sessionp = NULL;
	int remove_lease = 0;
	struct smb_lease *leasep = NULL;
	
    /*
     * For this function, vnode_type is VDIR as the Open will be done on the
     * parent dir and then the Query Dir will be done on that directory.
     */

    /*
     * SMB 2/3 searches do not want any paths (ie no '\') in the search
     * pattern so we have to open the actual dir that we are going to
     * search.
     *
     * Get the FID for the dir we are going to search
     */
    if (ctx->f_lookupName == NULL) {
        /*
         * No name, open the parent of dnp, search pattern will
         * be dnp->n_name
         */
        lck_rw_lock_shared(&ctx->f_dnp->n_parent_rwlock); /* do our own lock */
        n_parent_locked = 1;
        
        par_vp = smbfs_smb_get_parent(ctx->f_dnp, 0); /* dont do locking */
        if (par_vp == NULL) {
            SMBERROR_LOCK(ctx->f_dnp, "Failed to get parent for <%s> \n",
                          ctx->f_dnp->n_name);
            error = ENOENT;
            goto bad;
        }
        else {
            create_np = VTOSMB(par_vp);
        }
    }
    else {
        /*
         * Have a name, so just open dnp and search pattern will
         * be ctx->f_lookupName
         */
        create_np = ctx->f_dnp;
    }
    
resend:
    /*
     * Build the Create call
     */
    
    /* fid is -1 for compound requests */
    ctx->f_create_fid = 0xffffffffffffffff;
    
    create_options = smb2fs_smb_get_create_options(ctx->f_share, create_np,
                                                   NULL, NULL,
                                                   vnode_type, 0);
	
	/* Try to get a dir lease */
	sessionp = SS_TO_SESSION(ctx->f_share);
	if (sessionp == NULL) {
		/* Should never happen */
		SMBERROR("sessionp is null \n");
		error = EINVAL;
		goto bad;
	}

	/*
	 * If server supports dir leases, then try to get one for enumerations
     * Do not request a dir lease if this is for readdir since it does not
     * cache dir entries.
	 */
	if ((SMBV_SMB3_OR_LATER(sessionp)) &&
		(sessionp->session_sopt.sv_capabilities & SMB2_GLOBAL_CAP_DIRECTORY_LEASING) &&
		!(ctx->f_share->ss_mount->sm_args.altflags & SMBFS_MNT_DIR_LEASE_OFF) &&
        (ctx->f_is_readdir == 0)) {
		dur_handlep = &ctx->f_dnp->d_dur_handle;

		lck_mtx_lock(&dur_handlep->lock);
		
		if ((dur_handlep->flags != 0) &&
			(dur_handlep->flags != SMB2_LEASE_BROKEN) &&
			(dur_handlep->flags != (SMB2_LEASE_BROKEN | SMB2_LEASE_V2)) &&
			(dur_handlep->flags != (SMB2_LEASE_BROKEN | SMB2_LEASE_V2 | SMB2_LEASE_PARENT_LEASE_KEY_SET))) {
			SMBERROR_LOCK(ctx->f_dnp, "Durable Handle flags not zero <0x%llx> on <%s>\n",
					 dur_handlep->flags, ctx->f_dnp->n_name);
			dur_handlep->flags = 0;
		}
		else {
			dur_handlep->flags = 0;
		}
		
		create_flags |= SMB2_CREATE_DIR_LEASE;

		/* 
		 * <32164561> A lease break can arrive from the server BEFORE the
		 * Create response with the granted lease arrives.
		 * Assume that the lease will be granted and add it into the lease
		 * table right away. Remove it if we do not get a lease.
		 *
		 * Fill in the FID if lease is successfully granted. For dir leases,
		 * the fid is not currently used.
		 */
		smb2_lease_hash_add(ctx->f_dnp->n_vnode, 0xffffffffffffffff,
							dur_handlep->lease_key_hi,
							dur_handlep->lease_key_low);
		
		/* If lease gets granted, then change remove_lease to 0 */
		remove_lease = 1;

		lck_mtx_unlock(&dur_handlep->lock);
	}
	
    error = smb2fs_smb_ntcreatex(ctx->f_share, create_np,
                                 NULL, 0,
                                 NULL, 0,
                                 desired_access, vnode_type,
                                 share_access, disposition,
                                 create_flags, create_options,
                                 &ctx->f_create_fid, NULL,
                                 &create_rqp, &createp,
                                 dur_handlep, context);
    if (error) {
        SMBERROR("smb2fs_smb_ntcreatex failed %d\n", error);
        goto bad;
    }
    
    /* Update Create hdr */
    error = smb2_rq_update_cmpd_hdr(create_rqp, SMB2_CMPD_FIRST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }

    /* Create the Query Dir */
    queryp->mc_flags |= (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_MC_REPLAY_FLAG):0;
    error = smb2_smb_query_dir(ctx->f_share, queryp, &query_rqp, create_rqp->sr_iod, context);
    if (error) {
        SMBERROR("smb2_smb_query_dir failed %d\n", error);
        goto bad;
    }

    /* Update Query Hdr */
    error = smb2_rq_update_cmpd_hdr(query_rqp, SMB2_CMPD_LAST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /* Chain Query Dir to the Create */
    create_rqp->sr_next_rqp = query_rqp;
    
    /* 
     * Send the compound request of Create/Query Dir
     */
    error = smb_rq_simple(create_rqp);
    
    if ((error) && (create_rqp->sr_flags & SMBR_RECONNECTED)) {
        SMB_LOG_MC("resending messageid %llu cmd %u (sr_flags: 0x%x).\n", create_rqp->sr_messageid, create_rqp->sr_command, create_rqp->sr_flags);
        if (create_rqp->sr_flags & SMBR_ALT_CH_DISCON) {
            /* An alternate channel got disconnected. Resend with the REPLAY flag set */
            create_flags |= SMB2_CREATE_REPLAY_FLAG;
        }

        /* Rebuild and try sending again */
        smb_rq_done(create_rqp);
        create_rqp = NULL;
        
        smb_rq_done(query_rqp);
        query_rqp = NULL;
        
        SMB_FREE(createp, M_SMBTEMP);
        createp = NULL;
		
		/* Remove the lease too */
		if (remove_lease == 1) {
			dur_handlep = &ctx->f_dnp->d_dur_handle;
			
			/* Try to remove the lease */
			lck_mtx_lock(&dur_handlep->lock);
			smb2_lease_hash_remove(ctx->f_dnp->n_vnode, NULL,
								   dur_handlep->lease_key_hi,
								   dur_handlep->lease_key_low);
			lck_mtx_unlock(&dur_handlep->lock);
			
			remove_lease = 0;
		}

		goto resend;
    }

    createp->ret_ntstatus = create_rqp->sr_ntstatus;

    /* Get pointer to response data */
    smb_rq_getreply(create_rqp, &mdp);
    
    if (error) {
        /* Create failed, try parsing the Query Dir */
        if (error != ENOENT) {
            SMBDEBUG("smb_rq_simple failed %d id %lld\n",
                     error, create_rqp->sr_messageid);
        }
        goto parse_query;
    }
    
    /* 
     * Parse the Create response.
     */
    error = smb2_smb_parse_create(ctx->f_share, mdp, createp);
    if (error) {
        /* Create parsing failed, try parsing the Query Dir */
        SMBERROR("smb2_smb_parse_create failed %d id %lld\n", 
                 error, create_rqp->sr_messageid);
        goto parse_query;
    }
	
	/* Did we ask for and get a dir lease? */
	if (create_flags & SMB2_CREATE_DIR_LEASE) {
		lck_mtx_lock(&ctx->f_dnp->d_dur_handle.lock);
		
		dur_handlep = &ctx->f_dnp->d_dur_handle;
		if (dur_handlep->flags & SMB2_LEASE_GRANTED) {
			lck_mtx_unlock(&ctx->f_dnp->d_dur_handle.lock);

			/* Update the leasep->fid */
			lck_mtx_lock(&global_Lease_hash_lock);

			leasep = smb2_lease_hash_get(dur_handlep->lease_key_hi,
										 dur_handlep->lease_key_low);
			if (leasep != NULL) {
				leasep->fid = createp->ret_fid;
			}
			else {
				/* Should not happen */
				SMBERROR_LOCK(ctx->f_dnp, "Warning: No lease found for <%s> \n",
							  ctx->f_dnp->n_name);
			}

			lck_mtx_unlock(&global_Lease_hash_lock);

			/* We got the lease, so dont remove it */
			remove_lease = 0;
		}
		else {
			lck_mtx_unlock(&ctx->f_dnp->d_dur_handle.lock);
		}
	}

	/*
     * No need to call smb2fs_smb_parse_ntcreatex since this is Query Dir and
     * the only vnode we have is the parent dir being searched.
     */
    
    /* At this point, the dir was successfully opened */
    ctx->f_need_close = TRUE;
    ctx->f_create_fid = createp->ret_fid;

parse_query:
    /* Consume any pad bytes */
    tmp_error = smb2_rq_next_command(create_rqp, &next_cmd_offset, mdp);
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("create smb2_rq_next_command failed %d id %lld\n", 
                 tmp_error, create_rqp->sr_messageid);
        error = error ? error : tmp_error;
        goto bad;
    }
    
    /* 
     * Parse Query Dir SMB 2/3 header 
     */
    tmp_error = smb2_rq_parse_header(query_rqp, &mdp);
    queryp->ret_ntstatus = query_rqp->sr_ntstatus;
    if (tmp_error) {
        /* Query Dir got an error */
        if (!error) {
            if (tmp_error != ENOENT) {
                SMBDEBUG("query smb2_rq_parse_header failed %d, id %lld\n",
                         tmp_error, query_rqp->sr_messageid);
            }
            error = tmp_error;
        }
        goto bad;
    }
    
    /* Parse the Query Dir response */
    tmp_error = smb2_smb_parse_query_dir(mdp, queryp);
    if (tmp_error) {
        /* Query Dir parsing got an error */
        if (!error) {
            SMBERROR("smb2_smb_parse_query_dir failed %d id %lld\n", 
                     tmp_error, query_rqp->sr_messageid);
            error = tmp_error;
        }
        goto bad;
    }

bad:
    ctx->f_create_rqp = create_rqp;   /* save rqp so it can be freed later */
    ctx->f_query_rqp = query_rqp;     /* save rqp so it can be freed later */
	
	/* Something failed or did not get granted a lease, remove it from table */
	if (remove_lease == 1) {
		dur_handlep = &ctx->f_dnp->d_dur_handle;
		
		lck_mtx_lock(&dur_handlep->lock);
		
		/* Try to remove the lease */
		smb2_lease_hash_remove(ctx->f_dnp->n_vnode, NULL,
							   dur_handlep->lease_key_hi,
							   dur_handlep->lease_key_low);
		
		lck_mtx_unlock(&dur_handlep->lock);
	}
	
	if (n_parent_locked) {
        lck_rw_unlock_shared(&ctx->f_dnp->n_parent_rwlock);
    }
    
    if (par_vp != NULL) {
        vnode_put(par_vp);
    }

    if (createp != NULL) {
        SMB_FREE(createp, M_SMBTEMP);
    }
    
    return error;
}

int
smb2fs_smb_cmpd_query_dir_one(struct smb_share *share, struct smbnode *np,
                              const char *query_namep, size_t query_name_len,
                              struct smbfattr *fap, char **namep, size_t *name_lenp,
                              vfs_context_t context)
{
	int error, tmp_error;
    uint16_t info_level;
    uint8_t info_class, flags;
    SMBFID fid = 0;
    struct smb2_create_rq *createp = NULL;
    struct smb2_query_dir_rq *queryp = NULL;
    struct smb2_close_rq *closep = NULL;
	struct smb_rq *create_rqp = NULL;
	struct smb_rq *query_rqp = NULL;
	struct smb_rq *close_rqp = NULL;
	struct mdchain *mdp;
    size_t next_cmd_offset = 0;
    uint32_t need_delete_fid = 0;
    uint32_t desired_access = SMB2_FILE_READ_ATTRIBUTES | SMB2_FILE_LIST_DIRECTORY | SMB2_SYNCHRONIZE;
    uint32_t share_access = NTCREATEX_SHARE_ACCESS_ALL;
    uint64_t create_flags = 0;
    char *network_name = NULL;
    uint32_t network_name_len = 0;
    char *local_name = NULL;
    size_t local_name_len = 0;
    size_t max_network_name_buffer_size = 0;
    struct smbmount *smp;
    uint32_t create_options = 0;
    uint32_t disposition = FILE_OPEN;
    enum vtype vnode_type = VDIR;
    char *tmp_namep = NULL;
    int n_parent_locked = 0;
    vnode_t par_vp = NULL;
    
    /*
     * For this function, vnode_type is VDIR as the Open will be done on the
     * parent dir and then the Query Dir will be done on that directory.
     */

	if (fap) {
		nanouptime(&fap->fa_reqtime);
	}

	smp = np->n_mount;
	if ((np->n_ino == smp->sm_root_ino) && (query_namep == NULL)) {
        /* 
         * For the root vnode, we just need limited info, so a Create/Close
         * will work. Cant do a Create/Query Dir/Close on the root vnode since
         * we have no parent to do the Create on. 
         */
        if (fap) {
            fap->fa_ino = smp->sm_root_ino; /* default value */
        }

        /* Add SMB2_CREATE_DO_CREATE to get the root vnode'd File ID */
        create_flags = SMB2_CREATE_GET_MAX_ACCESS | SMB2_CREATE_DO_CREATE;
        
        error = smb2fs_smb_cmpd_create(share, np,
                                       NULL, 0,
                                       NULL, 0,
                                       SMB2_FILE_READ_ATTRIBUTES | SMB2_SYNCHRONIZE, VDIR,
                                       NTCREATEX_SHARE_ACCESS_ALL, FILE_OPEN,
                                       create_flags, NULL,
                                       NULL, fap,
                                       NULL, context);
        if ((!error) && fap) {
            fap->fa_attr = SMB_EFA_DIRECTORY;
            fap->fa_vtype = VDIR;
            fap->fa_valid_mask |= FA_VTYPE_VALID;
        }
        
        return (error);
    }
    
    /*
	 * Unicode requires 4 * max file name len, codepage requires 3 * max file
	 * name, so lets just always use the unicode size.
	 */
	max_network_name_buffer_size = share->ss_maxfilenamelen * 4;
	SMB_MALLOC(network_name, char *, max_network_name_buffer_size, M_TEMP,
               M_WAITOK | M_ZERO);
	if (network_name == NULL) {
        SMBERROR("network_name malloc failed\n");
		error = ENOMEM;
        goto bad;
    }

    fid = 0xffffffffffffffff;   /* fid is -1 for compound requests */

    info_level = SMB_FIND_BOTH_DIRECTORY_INFO;
    
    /*
     * Set up for the Query Dir call
     */
    SMB_MALLOC(queryp,
               struct smb2_query_dir_rq *,
               sizeof(struct smb2_query_dir_rq),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (queryp == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    /* Just want first search entry returned and start from beginning */
    flags = SMB2_RETURN_SINGLE_ENTRY | SMB2_RESTART_SCANS;
    
    switch (info_level) {
        case SMB_FIND_FULL_DIRECTORY_INFO:
            /* For SMB 2/3, get the file/dir IDs too but no short names */
            info_class = FileIdFullDirectoryInformation;
            break;
            
        case SMB_FIND_BOTH_DIRECTORY_INFO:
            /* For SMB 2/3, get the file/dir IDs too */
            info_class = FileIdBothDirectoryInformation;
            break;
            
        default:
            SMBERROR("invalid infolevel %d", info_level);
            error = EINVAL;
            goto bad;
    }
    
    queryp->file_info_class = info_class;
    queryp->flags = flags;
    queryp->file_index = 0;             /* no FileIndex from prev search */
    queryp->fid = fid;
    queryp->mc_flags = 0;
    queryp->output_buffer_len = 64 * 1024;  /* 64K should be large enough */
    
    /*
     * Not a wildcard, so use UTF_SFM_CONVERSIONS
     */
    queryp->name_flags = UTF_SFM_CONVERSIONS;
    
    if (query_namep != NULL) {
        /* We have a dnp and a name to search for in that dir */
        queryp->dnp = np;
        queryp->namep = (char*) query_namep;
        queryp->name_len = (uint32_t) query_name_len;
    }
    else {
        /* 
         * We are looking for np, so use np's parent and np->n_name
         */
        lck_rw_lock_shared(&np->n_parent_rwlock); /* do our own lock */
        n_parent_locked = 1;
        
        par_vp = smbfs_smb_get_parent(np, 0); /* dont do locking */
        if (par_vp != NULL) {
            queryp->dnp = VTOSMB(par_vp);
        }
        else {
            /* This could happen during a forced unmount */
            SMBERROR_LOCK(np, "Failed to get parent for <%s> \n",
                          np->n_name);
            error = ENOENT;
            goto bad;
        }
        
        lck_rw_lock_shared(&np->n_name_rwlock);
        tmp_namep = smb_strndup(np->n_name, np->n_nmlen);
        queryp->namep = tmp_namep;
        lck_rw_unlock_shared(&np->n_name_rwlock);

        queryp->name_len = (uint32_t) np->n_nmlen;
    }

resend:
    /*
     * Build the Create call
     */
    DBG_ASSERT (vnode_vtype(queryp->dnp->n_vnode) == VDIR);

    create_options = smb2fs_smb_get_create_options(share, queryp->dnp,
                                                   NULL, NULL,
                                                   vnode_type, 0);
    error = smb2fs_smb_ntcreatex(share, queryp->dnp,
                                 NULL, 0,
                                 NULL, 0,
                                 desired_access, vnode_type,
                                 share_access, disposition,
                                 create_flags, create_options,
                                 &fid, NULL,
                                 &create_rqp, &createp,
                                 NULL, context);
    if (error) {
        SMBERROR("smb2fs_smb_ntcreatex failed %d\n", error);
        goto bad;
    }
    
    /* Update Create hdr */
    error = smb2_rq_update_cmpd_hdr(create_rqp, SMB2_CMPD_FIRST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /*
     * Build the Query Dir request
     */
    queryp->mc_flags |= (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_MC_REPLAY_FLAG):0;
    error = smb2_smb_query_dir(share, queryp, &query_rqp, create_rqp->sr_iod, context);
    if (error) {
        SMBERROR("smb2_smb_query_dir failed %d\n", error);
        goto bad;
    }
    
    /* Update Query Hdr */
    error = smb2_rq_update_cmpd_hdr(query_rqp, SMB2_CMPD_MIDDLE);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /* Chain Query Dir to the Create */
    create_rqp->sr_next_rqp = query_rqp;
        
    /*
     * Build the Close request
     */
    error = smb2_smb_close_fid(share, fid, &close_rqp, &closep, create_rqp->sr_iod, context);
    if (error) {
        SMBERROR("smb2_smb_close_fid failed %d\n", error);
        goto bad;
    }
    *close_rqp->sr_flagsp |= (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_FLAGS_REPLAY_OPERATIONS):0;

    /* Update Close hdr */
    error = smb2_rq_update_cmpd_hdr(close_rqp, SMB2_CMPD_LAST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /* Chain Close to the Query Info */
    query_rqp->sr_next_rqp = close_rqp;

    /*
     * Send the compound request of Create/QueryDir/Close
     */
    error = smb_rq_simple(create_rqp);
    
    if ((error) && (create_rqp->sr_flags & SMBR_RECONNECTED)) {
        SMB_LOG_MC("resending messageid %llu cmd %u (sr_flags: 0x%x).\n", create_rqp->sr_messageid, create_rqp->sr_command, create_rqp->sr_flags);
        if (create_rqp->sr_flags & SMBR_ALT_CH_DISCON) {
            /* An alternate channel got disconnected. Resend with the REPLAY flag set */
            create_flags |= SMB2_CREATE_REPLAY_FLAG;
        }

        /* Rebuild and try sending again */
        smb_rq_done(create_rqp);
        create_rqp = NULL;
        
        smb_rq_done(query_rqp);
        query_rqp = NULL;
        
        smb_rq_done(close_rqp);
        close_rqp = NULL;
        
        SMB_FREE(createp, M_SMBTEMP);
        createp = NULL;
        
        SMB_FREE(closep, M_SMBTEMP);
        closep = NULL;
        
        goto resend;
    }

    createp->ret_ntstatus = create_rqp->sr_ntstatus;
    
    /* Get pointer to response data */
    smb_rq_getreply(create_rqp, &mdp);
    
    if (error) {
        /* Create failed, try parsing the Query Dir */
        if ((error != ENOENT) && (error != EACCES)) {
            SMBDEBUG("smb_rq_simple failed %d id %lld\n",
                     error, create_rqp->sr_messageid);
        }
        goto parse_query;
    }
    
    /*
     * Parse the Create response.
     */
    error = smb2_smb_parse_create(share, mdp, createp);
    if (error) {
        /* Create parsing failed, try parsing the Query Dir */
        SMBERROR("smb2_smb_parse_create failed %d id %lld\n",
                 error, create_rqp->sr_messageid);
        goto parse_query;
    }
    
    /* At this point, fid has been entered into fid table */
    need_delete_fid = 1;

    /*
     * No need to call smb2fs_smb_parse_ntcreatex since this is Query Dir and
     * the only vnode we have is the parent dir being searched.
     */
    
parse_query:
    /* Consume any pad bytes */
    tmp_error = smb2_rq_next_command(create_rqp, &next_cmd_offset, mdp);
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("create smb2_rq_next_command failed %d id %lld\n",
                 tmp_error, create_rqp->sr_messageid);
        error = error ? error : tmp_error;
        goto bad;
    }
    
    /*
     * Parse Query Dir SMB 2/3 header
     */
    tmp_error = smb2_rq_parse_header(query_rqp, &mdp);
    queryp->ret_ntstatus = query_rqp->sr_ntstatus;
    if (tmp_error) {
        /* Query Dir got an error */
        if (!error) {
            if (tmp_error != ENOENT) {
                SMBDEBUG("query smb2_rq_parse_header failed %d, id %lld\n",
                         tmp_error, query_rqp->sr_messageid);
            }
            error = tmp_error;
        }
        goto parse_close;
    }
    
    /* Parse the Query Dir response */
    tmp_error = smb2_smb_parse_query_dir(mdp, queryp);
    if (tmp_error) {
        /* Query Dir parsing got an error */
        if (!error) {
            SMBERROR("smb2_smb_parse_query_dir failed %d id %lld\n",
                     tmp_error, query_rqp->sr_messageid);
            error = tmp_error;
        }
        goto parse_close;
    }

    /*
     * Parse the only entry out of the output buffer
     */
	switch (queryp->file_info_class) {
        case FileIdFullDirectoryInformation:
        case FileIdBothDirectoryInformation:
            /* Call the parsing function */
            tmp_error = smb2_smb_parse_query_dir_both_dir_info(share, mdp,
                                                               info_level,
                                                               NULL, fap,
                                                               network_name, &network_name_len,
                                                               max_network_name_buffer_size);
            if (tmp_error) {
                /* Query Dir parsing got an error */
                if (!error) {
                    SMBERROR("smb2_smb_parse_query_dir_both_dir_info failed %d id %lld\n",
                             tmp_error, query_rqp->sr_messageid);
                    error = tmp_error;
                }
                goto parse_close;
            }
            
            /*
             * Convert network name to a malloc'd local name and return
             * that name
             */
            local_name_len = network_name_len;
            local_name = smbfs_ntwrkname_tolocal(network_name, &local_name_len,
                                                 SMB_UNICODE_STRINGS(SS_TO_SESSION(share)));
            
            if (!(SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS)) {
                /* Server does not support File IDs */
				if (fap) {
					fap->fa_ino = smbfs_getino(np, local_name, local_name_len);
				}
            }
            
            /* Return the malloc'd name from the server */
            if (namep) {
                *namep = local_name;
                local_name = NULL;
            }
            
            if (name_lenp) {
                *name_lenp = local_name_len;
            }

            break;

        default:
            if (!error) {
                SMBERROR("unexpected info level %d\n", queryp->file_info_class);
                error = EINVAL;
            }
            goto parse_close;
	}

parse_close:
    /* Update closep fid so it gets freed from FID table */
    closep->fid = createp->ret_fid;
    
    /* Consume any pad bytes */
    tmp_error = smb2_rq_next_command(query_rqp, &next_cmd_offset, mdp);
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("query smb2_rq_next_command failed %d\n", tmp_error);
        error = error ? error : tmp_error;
        goto bad;
    }
    
    /*
     * Parse Close SMB 2/3 header
     */
    tmp_error = smb2_rq_parse_header(close_rqp, &mdp);
    closep->ret_ntstatus = close_rqp->sr_ntstatus;
    if (tmp_error) {
        /* Close got an error */
        if (!error) {
            SMBDEBUG("close smb2_rq_parse_header failed %d id %lld\n",
                     tmp_error, close_rqp->sr_messageid);
            error = tmp_error;
        }
        goto bad;
    }
    
    /* Parse the Close response */
    tmp_error = smb2_smb_parse_close(mdp, closep);
    if (tmp_error) {
        /* Close parsing got an error */
        if (!error) {
            SMBERROR("smb2_smb_parse_close failed %d id %lld\n",
                     tmp_error, close_rqp->sr_messageid);
            error = tmp_error;
        }
        goto bad;
    }
    
    /* At this point, fid has been removed from fid table */
    need_delete_fid = 0;

bad:
    if (need_delete_fid == 1) {
        /*
         * Close failed but the Create worked and was successfully parsed.
         * Try issuing the Close request again.
         */
        tmp_error = smb2_smb_close_fid(share, createp->ret_fid,
                                       NULL, NULL, NULL, context);
        if (tmp_error) {
            SMBERROR("Second close failed %d\n", tmp_error);
        }
    }
    
    if (tmp_namep) {
        SMB_FREE(tmp_namep, M_SMBSTR);
    }
    
    if (n_parent_locked) {
        lck_rw_unlock_shared(&np->n_parent_rwlock);
    }
    
    if (par_vp != NULL) {
        vnode_put(par_vp);
    }
    
    if (create_rqp != NULL) {
        smb_rq_done(create_rqp);
    }
    if (query_rqp != NULL) {
        smb_rq_done(query_rqp);
    }
    if (close_rqp != NULL) {
        smb_rq_done(close_rqp);
    }
    
    if (createp != NULL) {
        SMB_FREE(createp, M_SMBTEMP);
    }
    if (queryp != NULL) {
        SMB_FREE(queryp, M_SMBTEMP);
    }
    if (closep != NULL) {
        SMB_FREE(closep, M_SMBTEMP);
    }
    
    if (network_name != NULL) {
        SMB_FREE(network_name, M_SMBTEMP);
    }
    if (local_name != NULL) {
        SMB_FREE(local_name, M_SMBTEMP);
    }

	return error;
}

int
smb2fs_smb_cmpd_reparse_point_get(struct smb_share *share, struct smbnode *np,
								  const char *namep, size_t name_len,
                                  struct uio *ioctl_uiop, uint64_t *sizep,
                                  vfs_context_t context)
{
	int error, tmp_error;
    SMBFID fid = 0;
    struct smbfattr *fap = NULL;
    struct smb2_create_rq *createp = NULL;
    struct smb2_ioctl_rq *ioctlp = NULL;
    struct smb2_close_rq *closep = NULL;
	struct smb_rq *create_rqp = NULL;
	struct smb_rq *ioctl_rqp = NULL;
	struct smb_rq *close_rqp = NULL;
	struct mdchain *mdp;
    size_t next_cmd_offset = 0;
    uint32_t need_delete_fid = 0;
    uint32_t create_desired_access = SMB2_FILE_READ_DATA |
                                     SMB2_FILE_READ_ATTRIBUTES;
    uint32_t share_access = NTCREATEX_SHARE_ACCESS_ALL;
    uint64_t create_flags = SMB2_CREATE_GET_MAX_ACCESS;
    uint32_t create_options = 0;
    uint32_t disposition = FILE_OPEN;
    enum vtype vnode_type = VLNK;
    
    /*
	 * smb2fs_smb_cmpd_reparse_point_get() has various ways it can be called
	 * 1) np - open item (np) which is the reparse point
	 * 2) np, namep - open the child (namep) which is the reparse point in 
	 *                parent dir (np)
	 *
	 * vnode_type is VLNK as the Open will be done on the reparse point and
	 * then the Ioctl will be done on the reparse point.
	 *
	 * ioctl_uiop can be null which usually means that we are just getting the
	 * reparse point size in sizep
	 */

    SMB_MALLOC(fap,
               struct smbfattr *, 
               sizeof(struct smbfattr), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (fap == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    SMB_MALLOC(ioctlp,
               struct smb2_ioctl_rq *,
               sizeof(struct smb2_ioctl_rq),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (ioctlp == NULL) {
		SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
resend:
    /*
     * Build the Create call 
     */
    create_options = smb2fs_smb_get_create_options(share, np,
                                                   namep, NULL,
                                                   vnode_type, 1);
    error = smb2fs_smb_ntcreatex(share, np,
                                 namep, name_len,
                                 NULL, 0,
                                 create_desired_access, vnode_type,
                                 share_access, disposition,
                                 create_flags, create_options,
                                 &fid, fap,
                                 &create_rqp, &createp, 
                                 NULL, context);
    if (error) {
        SMBERROR("smb2fs_smb_ntcreatex failed %d\n", error);
        goto bad;
    }
    
    /* Update Create hdr */
    error = smb2_rq_update_cmpd_hdr(create_rqp, SMB2_CMPD_FIRST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /* 
     * Build the IOCTL request 
     */
    ioctlp->share = share;
    ioctlp->ctl_code = FSCTL_GET_REPARSE_POINT;
    fid = 0xffffffffffffffff;   /* fid is -1 for compound requests */
    ioctlp->fid = fid;
    ioctlp->mc_flags = (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_MC_REPLAY_FLAG):0;

	ioctlp->snd_input_len = 0;
	ioctlp->snd_output_len = 0;
	ioctlp->rcv_input_len = 0;
    /* reparse points should not need more than 16K of data */
	ioctlp->rcv_output_len = 16 * 1024; 
    
    error = smb2_smb_ioctl(share, create_rqp->sr_iod, ioctlp, &ioctl_rqp, context);
    if (error) {
        SMBERROR("smb2_smb_ioctl failed %d\n", error);
        goto bad;
    }
    
    /* Update IOCTL hdr */
    error = smb2_rq_update_cmpd_hdr(ioctl_rqp, SMB2_CMPD_MIDDLE);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /* Chain IOCTL to the Create */
    create_rqp->sr_next_rqp = ioctl_rqp;
    
    /* 
     * Build the Close request 
     */
    error = smb2_smb_close_fid(share, fid, &close_rqp, &closep, create_rqp->sr_iod, context);
    if (error) {
        SMBERROR("smb2_smb_close_fid failed %d\n", error);
        goto bad;
    }
    *close_rqp->sr_flagsp |= (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_FLAGS_REPLAY_OPERATIONS):0;

    /* Update Close hdr */
    error = smb2_rq_update_cmpd_hdr(close_rqp, SMB2_CMPD_LAST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /* Chain Close to the IOCTL */
    ioctl_rqp->sr_next_rqp = close_rqp;
    
    /* 
     * Send the compound request of Create/IOCTL/Close 
     */
    error = smb_rq_simple(create_rqp);
    
    if ((error) && (create_rqp->sr_flags & SMBR_RECONNECTED)) {
        SMB_LOG_MC("resending messageid %llu cmd %u (sr_flags: 0x%x).\n", create_rqp->sr_messageid, create_rqp->sr_command, create_rqp->sr_flags);
        if (create_rqp->sr_flags & SMBR_ALT_CH_DISCON) {
            /* An alternate channel got disconnected. Resend with the REPLAY flag set */
            create_flags |= SMB2_CREATE_REPLAY_FLAG;
        }

        /* Rebuild and try sending again */
        smb_rq_done(create_rqp);
        create_rqp = NULL;
        
        smb_rq_done(ioctl_rqp);
        ioctl_rqp = NULL;
        
        smb_rq_done(close_rqp);
        close_rqp = NULL;
        
        SMB_FREE(createp, M_SMBTEMP);
        createp = NULL;
        
        SMB_FREE(closep, M_SMBTEMP);
        closep = NULL;
        
        goto resend;
    }

    createp->ret_ntstatus = create_rqp->sr_ntstatus;
    
    /* Get pointer to response data */
    smb_rq_getreply(create_rqp, &mdp);
    
    if (error) {
        /* Create failed, try parsing the IOCTL */
        SMBDEBUG("smb_rq_simple failed %d id %lld\n",
                 error, create_rqp->sr_messageid);
        goto parse_ioctl;
    }
    
    /* 
     * Parse the Create response.
     */
    error = smb2_smb_parse_create(share, mdp, createp);
    if (error) {
        /* Create parsing failed, try parsing the IOCTL */
        SMBERROR("smb2_smb_parse_create failed %d id %lld\n", 
                 error, create_rqp->sr_messageid);
        goto parse_ioctl;
    }
    
    /* At this point, fid has been entered into fid table */
    need_delete_fid = 1;
    
    /*
     * Fill in fap and possibly update vnode's meta data caches
     */
    error = smb2fs_smb_parse_ntcreatex(share, np, createp,
                                       &fid, fap, context);
    if (error) {
        /* Updating meta data cache failed, try parsing the Ioctl */
        SMBERROR("smb2fs_smb_parse_ntcreatex failed %d id %lld\n", 
                 error, create_rqp->sr_messageid);
    }

parse_ioctl:
    /* Consume any pad bytes */
    tmp_error = smb2_rq_next_command(create_rqp, &next_cmd_offset, mdp);
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("create smb2_rq_next_command failed %d id %lld\n", 
                 tmp_error, create_rqp->sr_messageid);
        goto bad;
    }
    
    /* 
     * Parse IOCTL SMB 2/3 header 
     */
    tmp_error = smb2_rq_parse_header(ioctl_rqp, &mdp);
    ioctlp->ret_ntstatus = ioctl_rqp->sr_ntstatus;
    if (tmp_error) {
        /* IOCTL got an error, try parsing the Close */
        if (!error) {
            SMBDEBUG("ioctl smb2_rq_parse_header failed %d, id %lld\n",
                     tmp_error, ioctl_rqp->sr_messageid);
            error = tmp_error;
        }
        goto parse_close;
    }
    
    /* Parse the IOCTL response */
    tmp_error = smb2_smb_parse_ioctl(mdp, ioctlp);
    if (tmp_error) {
        /* IOCTL parsing got an error, try parsing the Close */
        if (!error) {
            SMBERROR("smb2_smb_parse_ioctl failed %d id %lld\n", 
                     tmp_error, ioctl_rqp->sr_messageid);
            error = tmp_error;
        }
        goto parse_close;
    }
    
    /*
     * At this point, we know the IOCTL worked.
     * The IOCTL will return the path in rcv_output_buffer and rcv_output_len
     * Update the np with the symlink info if namep is NULL.
     * smbfs_update_symlink_cache will deal with any null pointers 
     */
	if (namep == NULL) {
		smbfs_update_symlink_cache(np, (char *) ioctlp->rcv_output_buffer,
								   ioctlp->rcv_output_len);
	}
	
	/* Return symlink string if they want it */
	if (ioctl_uiop != NULL) {
		tmp_error = uiomove((char *) ioctlp->rcv_output_buffer,
							(int) ioctlp->rcv_output_len,
							ioctl_uiop);
		if (tmp_error) {
			/* uiomove failed */
			if (!error) {
				SMBERROR("uiomove failed %d id %lld\n",
						 tmp_error, ioctl_rqp->sr_messageid);
				error = tmp_error;
			}
			goto parse_close;
		}
	}
	
	/* Return symlink size if they want it */
	if (sizep != NULL) {
		*sizep = ioctlp->rcv_output_len;
	}

parse_close:
    /* Update closep fid so it gets freed from FID table */
    closep->fid = createp->ret_fid;
    
    /* Consume any pad bytes */
    tmp_error = smb2_rq_next_command(ioctl_rqp, &next_cmd_offset, mdp);
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("ioctl smb2_rq_next_command failed %d\n", tmp_error);
        error = error ? error : tmp_error;
        goto bad;    
    }
    
    /* 
     * Parse Close SMB 2/3 header
     */
    tmp_error = smb2_rq_parse_header(close_rqp, &mdp);
    closep->ret_ntstatus = close_rqp->sr_ntstatus;
    if (tmp_error) {
        if (!error) {
            SMBDEBUG("close smb2_rq_parse_header failed %d id %lld\n",
                     tmp_error, close_rqp->sr_messageid);
            error = tmp_error;
        }
        goto bad;    
    }
    
    /* Parse the Close response */
    tmp_error = smb2_smb_parse_close(mdp, closep);
    if (tmp_error) {
        if (!error) {
            SMBERROR("smb2_smb_parse_close failed %d id %lld\n", 
                     tmp_error, close_rqp->sr_messageid);
            error = tmp_error;
        }
        goto bad;    
    }
    
    /* At this point, fid has been removed from fid table */
    need_delete_fid = 0;
    
bad:
    if (need_delete_fid == 1) {
        /*
         * Close failed but the Create worked and was successfully parsed.
         * Try issuing the Close request again.
         */
        tmp_error = smb2_smb_close_fid(share, createp->ret_fid,
                                       NULL, NULL, NULL, context);
        if (tmp_error) {
            SMBERROR("Second close failed %d\n", tmp_error);
        }
    }
    
    if (create_rqp != NULL) {
        smb_rq_done(create_rqp);
    }
    if (ioctl_rqp != NULL) {
        smb_rq_done(ioctl_rqp);
    }
    if (close_rqp != NULL) {
        smb_rq_done(close_rqp);
    }
    
    if (createp != NULL) {
        SMB_FREE(createp, M_SMBTEMP);
    }
	if (ioctlp != NULL) {
		if (ioctlp->rcv_output_buffer != NULL) {
			SMB_FREE(ioctlp->rcv_output_buffer, M_SMBTEMP);
		}
		
		SMB_FREE(ioctlp, M_SMBTEMP);
	}
    if (closep != NULL) {
        SMB_FREE(closep, M_SMBTEMP);
    }
    
    if (fap != NULL) {
        SMB_FREE(fap, M_SMBTEMP);
    }
    
	return error;
}

static int
smb2fs_smb_cmpd_reparse_point_set(struct smb_share *share, struct smbnode *create_np,
                                  const char *namep, size_t name_len,
                                  char *targetp, size_t target_len,
                                  struct smbfattr *fap, vfs_context_t context)
{
	int error, tmp_error;
    int ioctl_error= EINVAL;    /* assume IOCTL failed */
    SMBFID fid = 0;
    struct smb2_create_rq *createp = NULL;
    struct smb2_query_info_rq *queryp = NULL;
    struct smb2_ioctl_rq *ioctlp = NULL;
    struct smb2_close_rq *closep = NULL;
	struct smb_rq *create_rqp = NULL;
	struct smb_rq *query_rqp = NULL;
	struct smb_rq *ioctl_rqp = NULL;
	struct smb_rq *close_rqp = NULL;
	struct mdchain *mdp;
    size_t next_cmd_offset = 0;
    uint32_t need_delete_fid = 0, create_worked = 0;
    uint32_t create_desired_access = SMB2_FILE_WRITE_DATA |
                                     SMB2_FILE_WRITE_ATTRIBUTES |
                                     SMB2_DELETE;
    uint32_t share_access = NTCREATEX_SHARE_ACCESS_ALL;
    uint64_t create_flags = SMB2_CREATE_DO_CREATE | SMB2_CREATE_GET_MAX_ACCESS;
	size_t path_len;
	char *pathp = NULL;
	struct smbmount *smp = create_np->n_mount;
    int add_query = 0;
    uint64_t inode_number = 0;
    uint32_t inode_number_len;
    uint32_t create_options = 0;
    uint32_t disposition = FILE_CREATE;
    enum vtype vnode_type = VREG;
    
    /*
     * For this function, vnode_type is VREG as the Open will be done on the
     * reparse point and then the Ioctl will be done on the reparse point.
     */

    if (fap == NULL) {
        /* This should never happen */
        SMBERROR("fap is NULL \n");
		error = EINVAL;
		goto bad;
    }
    
    /*
     * This function can do Create/Query/Ioctl/Close, or Create/Ioctl/Close
     * If File IDs are supported, then the Query is added to get the file ID.
     */

    if (SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS) {
        /* Need the file id, so add a query into the compound request */
        add_query = 1;
    }

    /* Convert target to a network style path */
	path_len = (target_len * 2) + 2;	/* Start with the max possible size */
	SMB_MALLOC(pathp, char *, path_len, M_TEMP, M_WAITOK | M_ZERO);
	if (pathp == NULL) {
		error = ENOMEM;
		goto bad;
	}
    
	error = smb_convert_path_to_network(targetp, target_len, pathp, &path_len,
										'\\', SMB_UTF_SFM_CONVERSIONS, 
										SMB_UNICODE_STRINGS(SS_TO_SESSION(share)));
	if (error) {
		goto bad;
	}

    if (add_query) {
        SMB_MALLOC(queryp,
                   struct smb2_query_info_rq *,
                   sizeof(struct smb2_query_info_rq),
                   M_SMBTEMP,
                   M_WAITOK | M_ZERO);
        if (queryp == NULL) {
            SMBERROR("SMB_MALLOC failed\n");
            error = ENOMEM;
            goto bad;
        }
    }

    SMB_MALLOC(ioctlp,
               struct smb2_ioctl_rq *,
               sizeof(struct smb2_ioctl_rq),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (ioctlp == NULL) {
		SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
resend:
    /*
     * Build the Create call 
     */
    create_options = smb2fs_smb_get_create_options(share, create_np,
                                                   namep, NULL,
                                                   vnode_type, 1);
    error = smb2fs_smb_ntcreatex(share, create_np,
                                 namep, name_len,
                                 NULL, 0,
                                 create_desired_access, vnode_type,
                                 share_access, disposition,
                                 create_flags, create_options,
                                 &fid, fap,
                                 &create_rqp, &createp, 
                                 NULL, context);
    if (error) {
        SMBERROR("smb2fs_smb_ntcreatex failed %d\n", error);
        goto bad;
    }
    
    /* Update Create hdr */
    error = smb2_rq_update_cmpd_hdr(create_rqp, SMB2_CMPD_FIRST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    fid = 0xffffffffffffffff;   /* fid is -1 for compound requests */

    if (add_query) {
        /*
         * Build the Query Info request (to get File ID)
         */
        inode_number_len = (uint32_t) sizeof(inode_number);
        
        queryp->info_type = SMB2_0_INFO_FILE;
        queryp->file_info_class = FileInternalInformation;
        queryp->add_info = 0;
        queryp->flags = 0;
        queryp->output_buffer_len = inode_number_len;
        queryp->output_buffer = (uint8_t *) &inode_number;
        queryp->input_buffer_len = 0;
        queryp->input_buffer = NULL;
        queryp->ret_buffer_len = 0;
        queryp->fid = fid;
        queryp->mc_flags = (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_MC_REPLAY_FLAG):0;

        error = smb2_smb_query_info(share, queryp, &query_rqp, create_rqp->sr_iod, context);
        if (error) {
            SMBERROR("smb2_smb_query_info failed %d\n", error);
            goto bad;
        }
        
        /* Update Query hdr */
        error = smb2_rq_update_cmpd_hdr(query_rqp, SMB2_CMPD_MIDDLE);
        if (error) {
            SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
            goto bad;
        }
        
        /* Chain Query Info to the Create */
        create_rqp->sr_next_rqp = query_rqp;
    }
    
    /*
     * Build the IOCTL request 
     * path and path len is passed in via snd_input_buffer and snd_input_len
     */
    ioctlp->share = share;
    ioctlp->ctl_code = FSCTL_SET_REPARSE_POINT;
    ioctlp->fid = fid;
    ioctlp->mc_flags = (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_MC_REPLAY_FLAG):0;

	ioctlp->snd_input_len = (uint32_t) path_len;
	ioctlp->snd_output_len = 0;
	ioctlp->rcv_input_len = 0;
	ioctlp->rcv_output_len = 0;
    ioctlp->snd_input_buffer = (uint8_t *) pathp;

    error = smb2_smb_ioctl(share, create_rqp->sr_iod, ioctlp, &ioctl_rqp, context);
    if (error) {
        SMBERROR("smb2_smb_ioctl failed %d\n", error);
        goto bad;
    }
    
    /* Update IOCTL hdr */
    error = smb2_rq_update_cmpd_hdr(ioctl_rqp, SMB2_CMPD_MIDDLE);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    if (add_query) {
        /* Chain IOCTL to the Query */
        query_rqp->sr_next_rqp = ioctl_rqp;
    }
    else {
        /* Chain IOCTL to the Create */
        create_rqp->sr_next_rqp = ioctl_rqp;
    }
    
    /* 
     * Build the Close request 
     */
    error = smb2_smb_close_fid(share, fid, &close_rqp, &closep, create_rqp->sr_iod, context);
    if (error) {
        SMBERROR("smb2_smb_close_fid failed %d\n", error);
        goto bad;
    }
    *close_rqp->sr_flagsp |= (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_FLAGS_REPLAY_OPERATIONS):0;
    
    /* Update Close hdr */
    error = smb2_rq_update_cmpd_hdr(close_rqp, SMB2_CMPD_LAST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /* Chain Close to the IOCTL */
    ioctl_rqp->sr_next_rqp = close_rqp;
    
    /* 
     * Send the compound request of Create/IOCTL/Close 
     */
    error = smb_rq_simple(create_rqp);
    
    if ((error) && (create_rqp->sr_flags & SMBR_RECONNECTED)) {
        SMB_LOG_MC("resending messageid %llu cmd %u (sr_flags: 0x%x).\n", create_rqp->sr_messageid, create_rqp->sr_command, create_rqp->sr_flags);
        if (create_rqp->sr_flags & SMBR_ALT_CH_DISCON) {
            /* An alternate channel got disconnected. Resend with the REPLAY flag set */
            create_flags |= SMB2_CREATE_REPLAY_FLAG;
        }

        /* Rebuild and try sending again */
        smb_rq_done(create_rqp);
        create_rqp = NULL;
        
        if (query_rqp) {
            smb_rq_done(query_rqp);
            query_rqp = NULL;
        }

        smb_rq_done(ioctl_rqp);
        ioctl_rqp = NULL;
        
        smb_rq_done(close_rqp);
        close_rqp = NULL;
        
        SMB_FREE(createp, M_SMBTEMP);
        createp = NULL;
        
        SMB_FREE(closep, M_SMBTEMP);
        closep = NULL;
        
        goto resend;
    }

    createp->ret_ntstatus = create_rqp->sr_ntstatus;
    
    /* Get pointer to response data */
    smb_rq_getreply(create_rqp, &mdp);
    
    if (error) {
        /* Create failed, try parsing the IOCTL */
        SMBDEBUG("smb_rq_simple failed %d id %lld\n",
                 error, create_rqp->sr_messageid);
        goto parse_query;
    }
    
    /* 
     * Parse the Create response.
     */
    error = smb2_smb_parse_create(share, mdp, createp);
    if (error) {
        /* Create parsing failed, try parsing the IOCTL */
        SMBERROR("smb2_smb_parse_create failed %d id %lld\n", 
                 error, create_rqp->sr_messageid);
        goto parse_query;
    }
    
    /* At this point, fid has been entered into fid table */
    need_delete_fid = 1;
    create_worked = 1;
    
    /*
     * Fill in fap and possibly update vnode's meta data caches
     */
    error = smb2fs_smb_parse_ntcreatex(share, create_np, createp,
                                       &fid, fap, context);
    if (error) {
        /* Updating meta data cache failed, try parsing the IOCTL */
        SMBERROR("smb2fs_smb_parse_ntcreatex failed %d id %lld\n", 
                 error, create_rqp->sr_messageid);
    }
    
parse_query:
    if (query_rqp != NULL) {
        /* Consume any pad bytes */
        tmp_error = smb2_rq_next_command(create_rqp, &next_cmd_offset, mdp);
        if (tmp_error) {
            /* Failed to find next command, so can't parse rest of the responses */
            SMBERROR("create smb2_rq_next_command failed %d id %lld\n",
                     tmp_error, create_rqp->sr_messageid);
            error = error ? error : tmp_error;
            goto bad;
        }
        
        /*
         * Parse Query Info SMB 2/3 header
         */
        tmp_error = smb2_rq_parse_header(query_rqp, &mdp);
        queryp->ret_ntstatus = query_rqp->sr_ntstatus;
        if (tmp_error) {
            /* Query Info got an error, try parsing the Close */
            if (!error) {
                SMBDEBUG("query smb2_rq_parse_header failed %d, id %lld\n",
                         tmp_error, query_rqp->sr_messageid);
                error = tmp_error;
            }
            goto parse_ioctl;
        }
        
        /* Parse the Query Info response */
        tmp_error = smb2_smb_parse_query_info(mdp, queryp);
        if (tmp_error) {
            /* Query Info parsing got an error, try parsing the Close */
            if (!error) {
                if (tmp_error != ENOATTR) {
                    SMBERROR("smb2_smb_parse_query_info failed %d id %lld\n",
                             tmp_error, query_rqp->sr_messageid);
                }
                error = tmp_error;
            }
            goto parse_ioctl;
        }
        else {
            /* Query worked, so get the inode number */
            if (fap) {
                fap->fa_ino = inode_number;
                smb2fs_smb_file_id_check(share, fap->fa_ino, NULL, 0);
            }
        }
    }

parse_ioctl:
    /* Consume any pad bytes */
    tmp_error = smb2_rq_next_command(query_rqp != NULL ? query_rqp : create_rqp,
                                     &next_cmd_offset, mdp);
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("create smb2_rq_next_command failed %d id %lld\n", 
                 tmp_error, create_rqp->sr_messageid);
        goto bad;
    }
    
    /* 
     * Parse IOCTL SMB 2/3 header 
     */
    tmp_error = smb2_rq_parse_header(ioctl_rqp, &mdp);
    ioctlp->ret_ntstatus = ioctl_rqp->sr_ntstatus;
    ioctl_error = tmp_error;
    if (tmp_error) {
        /* IOCTL got an error, try parsing the Close */
        if (!error) {
            SMBDEBUG("ioctl smb2_rq_parse_header failed %d, id %lld\n",
                     tmp_error, ioctl_rqp->sr_messageid);
            error = tmp_error;
        }
        goto parse_close;
    }
    
    /* Parse the IOCTL response */
    tmp_error = smb2_smb_parse_ioctl(mdp, ioctlp);
    if (tmp_error) {
        /* IOCTL parsing got an error, try parsing the Close */
        if (!error) {
            SMBERROR("smb2_smb_parse_ioctl failed %d id %lld\n", 
                     tmp_error, ioctl_rqp->sr_messageid);
            error = tmp_error;
        }
        goto parse_close;
    }

parse_close:
    /* Update closep fid so it gets freed from FID table */
    closep->fid = createp->ret_fid;
    
    /* Consume any pad bytes */
    tmp_error = smb2_rq_next_command(ioctl_rqp, &next_cmd_offset, mdp);
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("ioctl smb2_rq_next_command failed %d\n", tmp_error);
        error = error ? error : tmp_error;
        goto bad;    
    }
    
    /* 
     * Parse Close SMB 2/3 header
     */
    tmp_error = smb2_rq_parse_header(close_rqp, &mdp);
    closep->ret_ntstatus = close_rqp->sr_ntstatus;
    if (tmp_error) {
        if (!error) {
            SMBDEBUG("close smb2_rq_parse_header failed %d id %lld\n",
                     tmp_error, close_rqp->sr_messageid);
            error = tmp_error;
        }
        goto bad;    
    }
    
    /* Parse the Close response */
    tmp_error = smb2_smb_parse_close(mdp, closep);
    if (tmp_error) {
        if (!error) {
            SMBERROR("smb2_smb_parse_close failed %d id %lld\n", 
                     tmp_error, close_rqp->sr_messageid);
            error = tmp_error;
        }
        goto bad;    
    }
    
    /* At this point, fid has been removed from fid table */
    need_delete_fid = 0;
    
bad:
    if (need_delete_fid == 1) {
        /*
         * Close failed but the Create worked and was successfully parsed.
         * Try issuing the Close request again.
         */
        tmp_error = smb2_smb_close_fid(share, createp->ret_fid,
                                       NULL, NULL, NULL, context);
        if (tmp_error) {
            SMBERROR("Second close failed %d\n", tmp_error);
        }
    }
    
    /* Only check for IOCTL errors if the create worked */
    if (create_worked == 1) {
        if (ioctl_error) {
            SMBDEBUG("smb2_smb_ioctl failed %d ntstatus %d\n",
                     ioctl_error, ioctlp->ret_ntstatus);
            
            /* Failed to create the symlink, remove the file */
            (void) smb2fs_smb_delete(share, create_np, VREG,
                                     namep, name_len,
                                     0, context);
        }
        else {
            /* IOCTL worked, reset any other fap information */
            fap->fa_size = target_len;
            /* FSCTL_SET_REPARSE_POINT succeeded, so mark it as a reparse point */
            fap->fa_attr |= SMB_EFA_REPARSE_POINT;
            fap->fa_valid_mask |= FA_REPARSE_TAG_VALID;
            fap->fa_reparse_tag = IO_REPARSE_TAG_SYMLINK;
            fap->fa_valid_mask |= FA_VTYPE_VALID;
            fap->fa_vtype = VLNK;
        }

        /* 
         * Windows systems requires special user access to create a reparse symlinks.
         * They default to only allow administrator symlink create access. This can
         * be changed on the server, but we are going to run into this issue. So if
         * we get an access error on the fsctl then we assume this user doesn't have
         * create symlink rights and we need to fallback to the old Conrad/Steve
         * symlinks. Since the create worked, we know the user has access to the file
         * system, they just don't have create symlink rights. We never fallback if 
         * the server is running darwin.
         */
        if ((ioctl_error) && !(UNIX_SERVER(SS_TO_SESSION(share)))) {
            /* 
             * <14281932> Could be NetApp server not supporting reparse 
             * points and returning STATUS_INVALID_DEVICE_REQUEST.
             */
            if ((ioctl_error == EACCES) ||
                (ioctlp->ret_ntstatus == STATUS_INVALID_DEVICE_REQUEST)) {
                smp->sm_flags &= ~MNT_SUPPORTS_REPARSE_SYMLINKS;
                
                error = smbfs_smb_create_windows_symlink(share, create_np,
                                                         namep, name_len,
                                                         targetp, target_len,
                                                         fap, context);
                if (!error) {
                    SMBDEBUG("smbfs_smb_create_windows_symlink failed %d\n", error);
                }
            }
        }
    }
	
    if (create_rqp != NULL) {
        smb_rq_done(create_rqp);
    }
    if (query_rqp != NULL) {
        smb_rq_done(query_rqp);
    }
    if (ioctl_rqp != NULL) {
        smb_rq_done(ioctl_rqp);
    }
    if (close_rqp != NULL) {
        smb_rq_done(close_rqp);
    }
    
    if (createp != NULL) {
        SMB_FREE(createp, M_SMBTEMP);
    }
    if (queryp != NULL) {
        SMB_FREE(queryp, M_SMBTEMP);
    }
    if (ioctlp != NULL) {
        SMB_FREE(ioctlp, M_SMBTEMP);
    }
    if (closep != NULL) {
        SMB_FREE(closep, M_SMBTEMP);
    }
    
    if (pathp != NULL) {
        SMB_FREE(pathp, M_TEMP);
    }
    
	return error;
}

int
smb2fs_smb_cmpd_resolve_id(struct smb_share *share, struct smbnode *np,
                           uint64_t ino, uint32_t *resolve_errorp, char **pathp,
                           vfs_context_t context)
{
	int error, tmp_error;
    struct smbfattr *fap = NULL;
    struct smb2_create_rq *createp = NULL;
    struct smb2_close_rq *closep = NULL;
	struct smb_rq *create_rqp = NULL;
	struct smb_rq *close_rqp = NULL;
	struct mdchain *mdp;
    size_t next_cmd_offset = 0;
    uint32_t need_delete_fid = 0;
    SMBFID fid = 0;
    struct smb2_create_ctx_resolve_id resolve_id;
    uint32_t create_options = 0;
    uint32_t disposition = FILE_OPEN;
    enum vtype vnode_type = VDIR;
    uint64_t create_flags = SMB2_CREATE_AAPL_RESOLVE_ID;

    
    /*
     * For this function, vnode_type is VDIR we always do the Resolve ID on the
     * root node.  The item will be Opened and the Resolve ID will be done on
     * it.
     */

    SMB_MALLOC(fap,
               struct smbfattr *,
               sizeof(struct smbfattr),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (fap == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    /* Fill in Resolve ID context */
    resolve_id.file_id = ino;
    resolve_id.ret_errorp = resolve_errorp;
    resolve_id.ret_pathp = pathp;

resend:
    /*
     * Build the Create call
     */
    create_options = smb2fs_smb_get_create_options(share, np,
                                                   NULL, NULL,
                                                   vnode_type, 0);
    error = smb2fs_smb_ntcreatex(share, np,
                                 NULL, 0,
                                 NULL, 0,
                                 SMB2_FILE_READ_ATTRIBUTES | SMB2_SYNCHRONIZE, vnode_type,
                                 NTCREATEX_SHARE_ACCESS_ALL, disposition,
                                 create_flags, create_options,
                                 &fid, fap,
                                 &create_rqp, &createp,
                                 &resolve_id, context);
    if (error) {
        SMBERROR("smb2fs_smb_ntcreatex failed %d\n", error);
        goto bad;
    }
    
    /* Update Create hdr */
    error = smb2_rq_update_cmpd_hdr(create_rqp, SMB2_CMPD_FIRST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    fid = 0xffffffffffffffff;   /* fid is -1 for compound requests */
    
    /*
     * Build the Close request
     */
    error = smb2_smb_close_fid(share, fid, &close_rqp, &closep, create_rqp->sr_iod, context);
    if (error) {
        SMBERROR("smb2_smb_close_fid failed %d\n", error);
        goto bad;
    }
    *close_rqp->sr_flagsp |= (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_FLAGS_REPLAY_OPERATIONS):0;

    /* Update Close hdr */
    error = smb2_rq_update_cmpd_hdr(close_rqp, SMB2_CMPD_LAST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /* Chain Close to the Create */
    create_rqp->sr_next_rqp = close_rqp;
    
    /*
     * Send the compound request of Create/Close
     */
    error = smb_rq_simple(create_rqp);
    
    if ((error) && (create_rqp->sr_flags & SMBR_RECONNECTED)) {
        SMB_LOG_MC("resending messageid %llu cmd %u (sr_flags: 0x%x).\n", create_rqp->sr_messageid, create_rqp->sr_command, create_rqp->sr_flags);
        if (create_rqp->sr_flags & SMBR_ALT_CH_DISCON) {
            /* An alternate channel got disconnected. Resend with the REPLAY flag set */
            create_flags |= SMB2_CREATE_REPLAY_FLAG;
        }

        /* Rebuild and try sending again */
        smb_rq_done(create_rqp);
        create_rqp = NULL;
        
        smb_rq_done(close_rqp);
        close_rqp = NULL;
        
        SMB_FREE(createp, M_SMBTEMP);
        createp = NULL;

        SMB_FREE(closep, M_SMBTEMP);
        closep = NULL;
        
        goto resend;
    }

    createp->ret_ntstatus = create_rqp->sr_ntstatus;
    
    /* Get pointer to response data */
    smb_rq_getreply(create_rqp, &mdp);
    
    if (error) {
        /* Create failed, try parsing the Close */
        if (error != ENOENT) {
            SMBDEBUG("smb_rq_simple failed %d id %lld\n",
                     error, create_rqp->sr_messageid);
        }
        goto parse_close;
    }
    
    /*
     * Parse the Create response.
     */
    error = smb2_smb_parse_create(share, mdp, createp);
    if (error) {
        /* Create parsing failed, try parsing the Close */
        SMBERROR("smb2_smb_parse_create failed %d id %lld\n",
                 error, create_rqp->sr_messageid);
        goto parse_close;
    }
    
    /* At this point, fid has been entered into fid table */
    need_delete_fid = 1;

parse_close:
    /* Update closep fid so it gets freed from FID table */
    closep->fid = createp->ret_fid;
    
    /* Consume any pad bytes */
    tmp_error = smb2_rq_next_command(create_rqp, &next_cmd_offset, mdp);
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("create smb2_rq_next_command failed %d id %lld\n",
                 tmp_error, create_rqp->sr_messageid);
        error = error ? error : tmp_error;
        goto bad;
    }
    
    /*
     * Parse Close SMB 2/3 header
     */
    tmp_error = smb2_rq_parse_header(close_rqp, &mdp);
    closep->ret_ntstatus = close_rqp->sr_ntstatus;
    if (tmp_error) {
        if (!error) {
            SMBDEBUG("close smb2_rq_parse_header failed %d id %lld\n",
                     tmp_error, close_rqp->sr_messageid);
            error = tmp_error;
        }
        goto bad;
    }
    
    /* Parse the Close response */
    tmp_error = smb2_smb_parse_close(mdp, closep);
    if (tmp_error) {
        if (!error) {
            SMBERROR("smb2_smb_parse_close failed %d id %lld\n",
                     tmp_error, close_rqp->sr_messageid);
            error = tmp_error;
        }
        goto bad;
    }
    
    /* At this point, fid has been removed from fid table */
    need_delete_fid = 0;
    
bad:
    if (need_delete_fid == 1) {
        /*
         * Close failed but the Create worked and was successfully parsed.
         * Try issuing the Close request again.
         */
        tmp_error = smb2_smb_close_fid(share, createp->ret_fid,
                                       NULL, NULL, NULL, context);
        if (tmp_error) {
            SMBERROR("Second close failed %d\n", tmp_error);
        }
    }
    
    if (create_rqp != NULL) {
        smb_rq_done(create_rqp);
    }
    if (close_rqp != NULL) {
        smb_rq_done(close_rqp);
    }
    
    if (createp != NULL) {
        SMB_FREE(createp, M_SMBTEMP);
    }
    if (closep != NULL) {
        SMB_FREE(closep, M_SMBTEMP);
    }
    
    if (fap != NULL) {
        SMB_FREE(fap, M_SMBTEMP);
    }

	return error;
}

static int
smb2fs_smb_cmpd_set_info(struct smb_share *share, struct smbnode *create_np, enum vtype vnode_type,
                         const char *create_namep, size_t create_name_len,
                         uint32_t create_xattr, uint32_t create_desired_access,
                         uint8_t setinfo_info_type, uint8_t setinfo_file_info_class,
                         uint32_t setinfo_add_info,
                         uint32_t setinfo_input_buffer_len, uint8_t *setinfo_input_buffer,
                         uint32_t *setinfo_ntstatus,
                         vfs_context_t context)
{
#pragma unused(setinfo_input_buffer_len)
	int error, tmp_error;
    SMBFID fid = 0;
    struct smbfattr *fap = NULL;
    struct smb2_create_rq *createp = NULL;
    struct smb2_set_info_rq *infop = NULL;
    struct smb2_close_rq *closep = NULL;
	struct smb_rq *create_rqp = NULL;
	struct smb_rq *setinfo_rqp = NULL;
	struct smb_rq *close_rqp = NULL;
	struct mdchain *mdp;
    size_t next_cmd_offset = 0;
    uint32_t need_delete_fid = 0;
    uint32_t share_access = NTCREATEX_SHARE_ACCESS_ALL;
    uint64_t create_flags = create_xattr ? SMB2_CREATE_IS_NAMED_STREAM : 0;
    char *file_namep = NULL, *stream_namep = NULL;
    size_t file_name_len = 0, stream_name_len = 0;
    uint32_t create_options = 0;
    uint32_t disposition = FILE_OPEN;

    /*
     * For this function, vnode_type is the item's type. The item will be
     * Opened and the Set Info will be done on the item.
     */

    if ((setinfo_info_type == SMB2_0_INFO_FILE) &&
        (setinfo_file_info_class == FileDispositionInformation)) {
        /* 
         * <17346821> Must be a Delete. Set share_access to 
         * NTCREATEX_SHARE_ACCESS_NONE so that we can attempt to delete the
         * item right now.
         */
        share_access = NTCREATEX_SHARE_ACCESS_NONE;
    }
    
    *setinfo_ntstatus = 0;
    
    SMB_MALLOC(fap, 
               struct smbfattr *, 
               sizeof(struct smbfattr), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (fap == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }

    SMB_MALLOC(infop,
               struct smb2_set_info_rq *,
               sizeof(struct smb2_set_info_rq),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (infop == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
resend:
    /*
     * Build the Create call 
     */
    create_flags |= SMB2_CREATE_GET_MAX_ACCESS;
    
    if (!(create_flags & SMB2_CREATE_IS_NAMED_STREAM)) {
        file_namep = (char *) create_namep;
        file_name_len = create_name_len;
    }
    else {
        /* create_namep is actually the stream name */
        stream_namep = (char *) create_namep;
        stream_name_len = create_name_len;
    }

    create_options = smb2fs_smb_get_create_options(share, create_np,
                                                   file_namep, stream_namep,
                                                   vnode_type, 1);
    error = smb2fs_smb_ntcreatex(share, create_np,
                                 file_namep, file_name_len,
                                 stream_namep, stream_name_len,
                                 create_desired_access, vnode_type,
                                 share_access, disposition,
                                 create_flags, create_options,
                                 &fid, fap,
                                 &create_rqp, &createp, 
                                 NULL, context);
    
    if (error) {
        SMBERROR("smb2fs_smb_ntcreatex failed %d\n", error);
        goto bad;
    }
    
    /* Update Create hdr */
    error = smb2_rq_update_cmpd_hdr(create_rqp, SMB2_CMPD_FIRST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /* 
     * Build the Set Info request 
     */
    infop->info_type = setinfo_info_type;
    infop->file_info_class = setinfo_file_info_class;
    infop->add_info = setinfo_add_info;
    fid = 0xffffffffffffffff;   /* fid is -1 for compound requests */
    infop->fid = fid;
    infop->input_buffer = setinfo_input_buffer;
    infop->mc_flags = (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_MC_REPLAY_FLAG):0;

    error = smb2_smb_set_info(share, infop, &setinfo_rqp, create_rqp->sr_iod, context);
    if (error) {
        SMBERROR("smb2_smb_set_info failed %d\n", error);
        goto bad;
    }

    /* Update Set Info hdr */
    error = smb2_rq_update_cmpd_hdr(setinfo_rqp, SMB2_CMPD_MIDDLE);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /* Chain Set Info to the Create */
    create_rqp->sr_next_rqp = setinfo_rqp;
    
    /* 
     * Build the Close request 
     */
    error = smb2_smb_close_fid(share, fid, &close_rqp, &closep, create_rqp->sr_iod, context);
    if (error) {
        SMBERROR("smb2_smb_close_fid failed %d\n", error);
        goto bad;
    }
    *close_rqp->sr_flagsp |= (create_flags & SMB2_CREATE_REPLAY_FLAG)?(SMB2_FLAGS_REPLAY_OPERATIONS):0;

    
    /* Update Close hdr */
    error = smb2_rq_update_cmpd_hdr(close_rqp, SMB2_CMPD_LAST);
    if (error) {
        SMBERROR("smb2_rq_update_cmpd_hdr failed %d\n", error);
        goto bad;
    }
    
    /* Chain Close to the Query Info */
    setinfo_rqp->sr_next_rqp = close_rqp;
    
    /* 
     * Send the compound request of Create/SetInfo/Close 
     */
    error = smb_rq_simple(create_rqp);
    
    if ((error) && (create_rqp->sr_flags & SMBR_RECONNECTED)) {
        SMB_LOG_MC("resending messageid %llu cmd %u (sr_flags: 0x%x).\n", create_rqp->sr_messageid, create_rqp->sr_command, create_rqp->sr_flags);
        if (create_rqp->sr_flags & SMBR_ALT_CH_DISCON) {
            /* An alternate channel got disconnected. Resend with the REPLAY flag set */
            create_flags |= SMB2_CREATE_REPLAY_FLAG;
        }

        /* Rebuild and try sending again */
        smb_rq_done(create_rqp);
        create_rqp = NULL;
        
        smb_rq_done(setinfo_rqp);
        setinfo_rqp = NULL;
        
        smb_rq_done(close_rqp);
        close_rqp = NULL;
        
        SMB_FREE(createp, M_SMBTEMP);
        createp = NULL;
        
        SMB_FREE(closep, M_SMBTEMP);
        closep = NULL;
        
        goto resend;
    }

    createp->ret_ntstatus = create_rqp->sr_ntstatus;

    /* Get pointer to response data */
    smb_rq_getreply(create_rqp, &mdp);
    
    if (error) {
        /* Create failed, try parsing the Set Info */
        if (error != ENOENT) {
            SMBDEBUG("smb_rq_simple failed %d id %lld\n",
                     error, create_rqp->sr_messageid);
        }
        goto parse_setinfo;
    }
    
    /* 
     * Parse the Create response.
     */
    error = smb2_smb_parse_create(share, mdp, createp);
    if (error) {
        /* Create parsing failed, try parsing the Set Info */
        SMBERROR("smb2_smb_parse_create failed %d id %lld\n", 
                 error, create_rqp->sr_messageid);
        goto parse_setinfo;
    }
    
    /* At this point, fid has been entered into fid table */
    need_delete_fid = 1;
    
    /*
     * Fill in fap and possibly update vnode's meta data caches
     */
    error = smb2fs_smb_parse_ntcreatex(share, create_np, createp,
                                       &fid, fap, context);
    if (error) {
        /* Updating meta data cache failed, try parsing the IOCTL */
        SMBERROR("smb2fs_smb_parse_ntcreatex failed %d id %lld\n", 
                 error, create_rqp->sr_messageid);
    }

parse_setinfo:
    /* Consume any pad bytes */
    tmp_error = smb2_rq_next_command(create_rqp, &next_cmd_offset, mdp);
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("create smb2_rq_next_command failed %d id %lld\n", 
                 tmp_error, create_rqp->sr_messageid);
        goto bad;
    }
    
    /* 
     * Parse Set Info SMB 2/3 header 
     */
    tmp_error = smb2_rq_parse_header(setinfo_rqp, &mdp);
    infop->ret_ntstatus = setinfo_rqp->sr_ntstatus;
    *setinfo_ntstatus = setinfo_rqp->sr_ntstatus;
    if (tmp_error) {
        /* Set Info got an error, try parsing the Close */
        if (!error) {
            if ((tmp_error != EACCES) && (tmp_error != ENOTEMPTY)) {
                SMBDEBUG("setinfo smb2_rq_parse_header failed %d, id %lld\n",
                         tmp_error, setinfo_rqp->sr_messageid);
            }
            error = tmp_error;
        }
        goto parse_close;
    }
    
    /* Parse the Set Info response */
    tmp_error = smb2_smb_parse_set_info(mdp, infop);
    if (tmp_error) {
        /* Query Info parsing got an error, try parsing the Close */
        if (!error) {
            if (tmp_error != EACCES) {
                SMBERROR("smb2_smb_parse_set_info failed %d id %lld\n",
                         tmp_error, setinfo_rqp->sr_messageid);
            }
            error = tmp_error;
        }
        goto parse_close;
    }

parse_close:
    /* Update closep fid so it gets freed from FID table */
    closep->fid = createp->ret_fid;
    
    /* Consume any pad bytes */
    tmp_error = smb2_rq_next_command(setinfo_rqp, &next_cmd_offset, mdp);
    if (tmp_error) {
        /* Failed to find next command, so can't parse rest of the responses */
        SMBERROR("setinfo smb2_rq_next_command failed %d\n", tmp_error);
        error = error ? error : tmp_error;
        goto bad;    
    }
    
    /* 
     * Parse Close SMB 2/3 header
     */
    tmp_error = smb2_rq_parse_header(close_rqp, &mdp);
    closep->ret_ntstatus = close_rqp->sr_ntstatus;
    if (tmp_error) {
        if (!error) {
            SMBDEBUG("close smb2_rq_parse_header failed %d id %lld\n",
                     tmp_error, close_rqp->sr_messageid);
            error = tmp_error;
        }
        goto bad;    
    }
    
    /* Parse the Close response */
    tmp_error = smb2_smb_parse_close(mdp, closep);
    if (tmp_error) {
        if (!error) {
            SMBERROR("smb2_smb_parse_close failed %d id %lld\n", 
                     tmp_error, close_rqp->sr_messageid);
            error = tmp_error;
        }
        goto bad;    
    }
    
    /* At this point, fid has been removed from fid table */
    need_delete_fid = 0;
    
bad:
    if (need_delete_fid == 1) {
        /* 
         * Close failed but the Create worked and was successfully parsed.
         * Try issuing the Close request again.
         */
        tmp_error = smb2_smb_close_fid(share, createp->ret_fid,
                                       NULL, NULL, NULL, context);
        if (tmp_error) {
            SMBERROR("Second close failed %d\n", tmp_error);
        }
    }
    
    if (create_rqp != NULL) {
        smb_rq_done(create_rqp);
    }
    if (setinfo_rqp != NULL) {
        smb_rq_done(setinfo_rqp);
    }
    if (close_rqp != NULL) {
        smb_rq_done(close_rqp);
    }
    
    if (createp != NULL) {
        SMB_FREE(createp, M_SMBTEMP);
    }
    if (infop != NULL) {
        SMB_FREE(infop, M_SMBTEMP);
    }
    if (closep != NULL) {
        SMB_FREE(closep, M_SMBTEMP);
    }
    
    if (fap != NULL) {
        SMB_FREE(fap, M_SMBTEMP);
    }

	return error;
}

int
smb2fs_smb_change_notify(struct smb_share *share, uint32_t output_buffer_len,
                         uint32_t completion_filter, 
                         void *fn_callback, void *fn_callback_args,
                         vfs_context_t context)
{
    int error;
    struct watch_item *watch_item = fn_callback_args;
    struct smb2_change_notify_rq *changep = NULL;
    uint32_t flags = 0;
    
    SMB_MALLOC(changep, 
               struct smb2_change_notify_rq *, 
               sizeof(struct smb2_change_notify_rq), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (changep == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    /* 
     * Set up for the Change Notify call
     */
    if (watch_item->watchTree) {
        flags |= SMB2_WATCH_TREE;
    }
    changep->flags = flags;
    if (watch_item->isServerMsg) {
        /* special FID to let server know this is a
         * Mac-to-Mac srvmsg notify
         */
        changep->fid = 0xffffffffffffffff;
    } else {
        changep->fid = VTOSMB(watch_item->vp)->d_fid;
    }
    changep->output_buffer_len = output_buffer_len;
    changep->filter = completion_filter;
    changep->fn_callback = fn_callback;
    changep->fn_callback_args = fn_callback_args;
    
    /*
     * Do the Change Notify call
     * Save the rqp into watch_item->rqp
     */
    error = smb2_smb_change_notify(share, changep, &watch_item->rqp, context);
    if (error) {
        SMBERROR("smb2_smb_change_notify failed %d\n", error);
        goto bad;
    }
    
bad:    
    if (changep != NULL) {
        SMB_FREE(changep, M_SMBTEMP);
    }

    return error;
}

int
smb2fs_smb_check_dur_handle_v2(struct smb_share *share, struct smbnode *dnp,
							   uint32_t *timeout, vfs_context_t context)
{
	int error;
	SMBFID fid = 0;
	uint32_t create_desired_access = SMB2_FILE_READ_DATA |
									 SMB2_FILE_READ_ATTRIBUTES |
	                                 SMB2_FILE_READ_EA;
	enum vtype vnode_type = VREG;
	uint32_t share_access = NTCREATEX_SHARE_ACCESS_READ;
	uint32_t disposition = FILE_OPEN_IF;
	struct smb2_durable_handle dur_handle;
	struct smbfattr *fap = NULL;
	struct smb_session *sessionp = NULL;
    char create_name[] = ".com.apple.timemachine.supported";
	size_t name_len = 0;
	int dur_need_free = 0;
    UInt8 uuid[16] = {0};
    char name[MAXPATHLEN] = {0};

	/*
	 * Try to open the traditional Time Machine supported file at the root
	 * of the share. Also check to see if we can get a Durable Handle V2.
	 * The file will be Opened and then closed/deleted.
	 */
	if (dnp == NULL) {
		SMBERROR("dnp is null \n");
		error = EINVAL;
		goto bad;
	}
	else {
		if (!vnode_isdir(dnp->n_vnode)) {
			SMBERROR("dnp is not a directory \n");
			error = EINVAL;
			goto bad;
		}
	}
	
	if (share == NULL) {
		SMBERROR("share is null \n");
		error = EINVAL;
		goto bad;
	}
	
	sessionp = SS_TO_SESSION(share);
	if ((sessionp != NULL) &&
		!SMBV_SMB3_OR_LATER(sessionp)) {
		SMBDEBUG("Not SMB 3.x or later \n");
		error = ENOTSUP;
		goto bad;
	}
	
	SMB_MALLOC(fap,
			   struct smbfattr *,
			   sizeof(struct smbfattr),
			   M_SMBTEMP,
			   M_WAITOK | M_ZERO);
	if (fap == NULL) {
		SMBERROR("SMB_MALLOC failed\n");
		error = ENOMEM;
		goto bad;
	}

	/* 
	 * Yeah, this is not completely correct as we should be passing in the 
	 * file vnode np instead of dnp, but this is only checking for durable
	 * handle V2 support, so its fine 
	 */
	smb2_smb_dur_handle_init(share, dnp,
							 (SMB2_DURABLE_HANDLE_REQUEST | SMB2_NEW_LEASE_KEY), &dur_handle);
	dur_handle.req_lease_state = SMB2_LEASE_READ_CACHING | SMB2_LEASE_HANDLE_CACHING;
	
	dur_need_free = 1;
	
	lck_mtx_lock(&dur_handle.lock);
	/* Set flag to allow the check for Durable Handle V2 */
	dur_handle.flags |= SMB2_DURABLE_HANDLE_V2_CHECK;
	lck_mtx_unlock(&dur_handle.lock);

    /*
     * Generate unique uuid to use as part of the name. Its possible that
     * we create the file, but get disconnected before we can delete the file.
     * Then next person that tries a backup may get access denied if they
     * are different from the person that created the existing file which
     * would cause the check for dur handles to fail.
     */
    uuid_generate(uuid);
    snprintf(name, sizeof(name), "%s-%x%x%x%x-%x%x-%x%x-%x%x-%x%x%x%x%x%x",
             create_name,
             uuid[0], uuid[1], uuid[2], uuid[3],
             uuid[4], uuid[5],
             uuid[6], uuid[7],
             uuid[8], uuid[9],
             uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
    name_len = strlen(name);

	error = smbfs_smb_ntcreatex(share, dnp,
								create_desired_access, share_access, vnode_type,
								&fid, name, name_len,
								disposition, 0, fap,
								FALSE, &dur_handle, context);
	if (error) {
		SMBDEBUG("smbfs_smb_ntcreatex failed %d\n", error);
		goto bad;
	}
	
	if (dur_handle.flags & SMB2_DURABLE_HANDLE_FAIL) {
		/* 
		 * Create worked, but something failed on getting durable handle 
		 * or lease. Still need to close and delete the file.
		 */
		error = EIO;
        SMBERROR("Something failed on getting Durable Handle\n");
		goto cleanup;
	}
	
	/* Durable Handle V2 must be supported */
	if ((dur_handle.flags & SMB2_DURABLE_HANDLE_GRANTED)
        || (dur_handle.flags & SMB2_PERSISTENT_HANDLE_GRANTED)) {
        SMB_LOG_IO("%s supported. Default Timeout %d\n", (dur_handle.flags & SMB2_DURABLE_HANDLE_GRANTED)
                 ? "Durable Handle V2" : "Persistent Handle", dur_handle.timeout);

		if (timeout != NULL) {
			/* Convert mSecs to seconds */
			*timeout = dur_handle.timeout / 1000;
		}
	}
	else {
		SMBERROR("Durable Handle V2 or Persistent Handles not supported. \n");
		error = ENOTSUP;
	}

cleanup:
	(void) smbfs_smb_close(share, fid, context);
	
	if (fap->fa_created_disp == FILE_CREATE) {
		/* We created the file, so attempt to delete it now */
		smbfs_smb_delete(share, dnp, VREG, name, name_len, 0, context);
	}
	
bad:
	if (fap != NULL) {
		SMB_FREE(fap, M_SMBTEMP);
	}

	if (dur_need_free) {
		/* No need to remove lease from hash table since we naver addeed it */
		lck_mtx_lock(&dur_handle.lock);
		dur_handle.flags = 0;
		lck_mtx_unlock(&dur_handle.lock);
		
		smb2_smb_dur_handle_free(&dur_handle);
	}
	
	return error;
}

int
smbfs_smb_close(struct smb_share *share, SMBFID fid, vfs_context_t context)
{
	int error;
	
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2_smb_close_fid(share, fid, NULL, NULL, NULL, context);
    }
    else {
        error = smb1fs_smb_close(share, fid, context);
    }
	
    /*
     * ENOTCONN isn't interesting - if the connection is closed,
     * so are all our FIDs - and ENXIO is also not interesting,
     * as it means a forced unmount was done.
     *
     * EBADF means the fid is no longer good. Reconnect will make this happen.
     * Should we check to see if the open was broken on the reconnect or does it
     * really matter?
     *
     * Don't clog up the system log with warnings about those failures
     * on closes.
     */
    if ((error == ENOTCONN) || (error == ENXIO) || (error == EBADF)) {
        error = 0;
    }
	
	return error;
}

/*
 * This routine is used for both Mac-to-Mac and Mac-to-Windows copyfile
 * operations.  For Mac-to-Mac, the FSCTL_SRV_COPYCHUNK ioctl is sent with
 * a chunk count of zero, because the server uses copyfile(3) and doesn't need
 * a list of chunks from the client.  To specify Mac-to-Mac semantics, the
 * mac_to_mac parameter should be set to TRUE.
 */
static int
smb2fs_smb_copychunks(struct smb_share *share, SMBFID src_fid,
                      SMBFID targ_fid, uint64_t src_file_len,
                      int mac_to_mac, vfs_context_t context)
{
    struct smb2_ioctl_rq            *ioctlp = NULL;
    struct smb2_copychunk           *copychunk_hdr;
    struct smb2_copychunk_chunk     *copychunk_element;
    struct smb2_copychunk_result    *copychunk_result;
    char                            *sendbuf = NULL;
    uint32_t                        sendbuf_len, chunk_count;
    uint32_t                        max_chunk_len, retry;
    uint64_t                        remaining_len, this_len, src_offset;
    u_char                          resume_key[SMB2_RESUME_KEY_LEN];
    int error = 0;
	
    /* Allocate a copychunk header with an array of chunk elements */
    sendbuf_len = sizeof(struct smb2_copychunk) +
    (sizeof(struct smb2_copychunk_chunk) * SMB2_COPYCHUNK_ARR_SIZE);
	
    /* setup a send buffer for the copychunk headers */
    SMB_MALLOC(sendbuf,
               char *,
               sendbuf_len,
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (sendbuf == NULL) {
		SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto out;
    }
	
    /* Begin by getting a resume key from the source */
    error = smb2fs_smb_request_resume_key(share, src_fid, resume_key, context);
	
    if (error) {
        SMBERROR("Failed to get resume key, error: %d\n", error);
        goto out;
    }
    
    /*
     * Build the IOCTL request
     */
    SMB_MALLOC(ioctlp,
               struct smb2_ioctl_rq *,
               sizeof(struct smb2_ioctl_rq),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (ioctlp == NULL) {
		SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto out;
    }
    
    /* We will initialize ioctlp->snd_input_len later */
    ioctlp->share = share;
    ioctlp->ctl_code = FSCTL_SRV_COPYCHUNK;
    ioctlp->fid = targ_fid;
    ioctlp->snd_input_buffer = (uint8_t *)sendbuf;
    ioctlp->rcv_output_len = sizeof(struct smb2_copychunk_result);
    ioctlp->mc_flags = 0;

    copychunk_hdr = (struct smb2_copychunk *)sendbuf;
    
    /* setup copy chunk hdr */
    memcpy(copychunk_hdr->source_key, resume_key, SMB2_RESUME_KEY_LEN);
    copychunk_hdr->reserved = 0;
    
    if (mac_to_mac == TRUE) {
        /* Mac-to-Mac case */
        
        /* Just a header, no chunks */
        ioctlp->snd_input_len = sizeof(struct smb2_copychunk);
        
        copychunk_hdr->chunk_count = 0;
        
        error = smb2_smb_ioctl(share, NULL, ioctlp, NULL, context);
        
        if (error) {
            SMBDEBUG("smb2_smb_ioctl error: %d\n", error);
            goto out;
        }
        
        /* sanity check */
        if (ioctlp->rcv_output_len < sizeof(struct smb2_copychunk_result)) {
            /* big problem, response too small, nothing we can do */
            SMBERROR("rcv_output_buffer too small, expected: %lu, got: %u\n",
                     sizeof(struct smb2_copychunk_result), ioctlp->rcv_output_len);
            error = EINVAL;
            goto out;
        }
        
        // Check results
        if (ioctlp->ret_ntstatus != STATUS_SUCCESS) {
            SMBDEBUG("copychunk result: nt_stat: 0x%0x\n", ioctlp->ret_ntstatus);
            
            /* map the nt_status to an errno */
            error = smb_ntstatus_to_errno(ioctlp->ret_ntstatus);
            goto out;
        }
    } else {
        /* Non Mac-to-Mac case */
        
        retry = 0;
        copychunk_element = (struct smb2_copychunk_chunk *)(sendbuf + sizeof(struct smb2_copychunk));
        max_chunk_len = SMB2_COPYCHUNK_MAX_CHUNK_LEN;

again:
        remaining_len = src_file_len;
        src_offset = 0;
        while (remaining_len) {
            
            /* Fillup the chunk array */
            error = smb2fs_smb_fillchunk_arr(copychunk_element,
                                    SMB2_COPYCHUNK_ARR_SIZE,
                                    remaining_len, max_chunk_len,
                                    src_offset, src_offset,
                                     &chunk_count, &this_len);
            if (error) {
                goto out;
            }
            
            remaining_len -= this_len;
            src_offset += this_len;
            copychunk_hdr->chunk_count = chunk_count;
            
            /* snd_input_len depends on how many chunks we're sending */
            ioctlp->snd_input_len = sizeof(struct smb2_copychunk) +
                (sizeof(struct smb2_copychunk_chunk) * chunk_count);
        
            error = smb2_smb_ioctl(share, NULL, ioctlp, NULL, context);
        
            if (error) {
                SMBDEBUG("smb2_smb_ioctl error: %d, remain: %llu, max_chunk: %u, count: %u, this_len: %llu\n",
                         error, remaining_len, max_chunk_len, chunk_count, this_len);
                
                if (ioctlp->ret_ntstatus != STATUS_INVALID_PARAMETER) {
                    goto out;
                }
            }
        
            /* sanity check */
            if ( (ioctlp->rcv_output_len < sizeof(struct smb2_copychunk_result)) ||
                (ioctlp->rcv_output_buffer == NULL) ) {
                /* big problem, response too small, nothing we can do */
                SMBERROR("rcv_output_buffer too small, expected: %lu, got: %u\n",
                     sizeof(struct smb2_copychunk_result), ioctlp->rcv_output_len);
                error = EINVAL;
                goto out;
            }
            
            // Check results
            copychunk_result = (struct smb2_copychunk_result *)ioctlp->rcv_output_buffer;
        
            if (ioctlp->ret_ntstatus == STATUS_INVALID_PARAMETER && !retry) {
                /*
                 * Exceeded server's maximum chunk length.  We will try
                 * once more using the server's value, which is
                 * returned by the server in chunk_bytes_written.
                 * See <rdar://problem/14750992>.
                 */
                
                if (copychunk_result->chunk_bytes_written < max_chunk_len) {
                    max_chunk_len = copychunk_result->chunk_bytes_written;
                    retry = 1;
  
                    SMB_FREE(ioctlp->rcv_output_buffer, M_SMBTEMP);
                    goto again;
                }
            }
            
            if (ioctlp->ret_ntstatus != STATUS_SUCCESS) {
                SMBDEBUG("smb2_smb_ioctl result: nt_stat: 0x%0x\n", ioctlp->ret_ntstatus);
            
                /* map the nt_status to an errno */
                error = smb_ntstatus_to_errno(ioctlp->ret_ntstatus);
                goto out;
            }
        
            if (copychunk_result->chunks_written != copychunk_hdr->chunk_count) {
                SMBERROR("copychunk error: chunks_written: %u, expected: %u\n",
                         copychunk_result->chunks_written, copychunk_hdr->chunk_count);
                error = EIO;
                goto out;
            }
        
            if (copychunk_result->total_bytes_written != this_len) {
                SMBERROR("copychunk error: total_bytes_written: %u, expected: %llu\n",
                         copychunk_result->total_bytes_written, this_len);
                error = EIO;
                goto out;
            }
        
            SMB_FREE(ioctlp->rcv_output_buffer, M_SMBTEMP);
        }
    }
out:
    // clean house
    if (sendbuf != NULL) {
        SMB_FREE(sendbuf, M_SMBTEMP);
    }
    
    if (ioctlp != NULL) {
        if (ioctlp->rcv_output_buffer != NULL) {
            SMB_FREE(ioctlp->rcv_output_buffer, M_SMBTEMP);
        }
        
        SMB_FREE(ioctlp, M_SMBTEMP);
    }
    
    return (error);
}

int
smb2fs_smb_copyfile(struct smb_share *share, struct smbnode *src_np,
                    struct smbnode *tdnp, const char *tnamep,
                    size_t tname_len, vfs_context_t context)
{
    struct smbfattr *sfap = NULL, *tfap = NULL;
    char            *xattr_list = NULL, *xattrp = NULL;
    SMBFID          src_fid = 0, targ_fid = 0, src_xattr_fid = 0, targ_xattr_fid = 0;
    size_t          xattrlist_len = 0, remaining_len = 0, this_len;
    uint32_t        desired_access, share_access, disp;
    boolean_t       src_is_open = FALSE, targ_is_open = FALSE;
    boolean_t       srcxattr_is_open = FALSE, targxattr_is_open = FALSE;
    uint64_t        create_flags, src_file_len;
    struct timespec mtime;
    int             error = 0;
    uint32_t        ntstatus = 0;
    uint32_t        target_created = 0;
    uint32_t create_options = 0;
    enum vtype vnode_type = VREG;
    
    if ((SS_TO_SESSION(share)->session_misc_flags & SMBV_OSX_SERVER) &&
        (SS_TO_SESSION(share)->session_server_caps & kAAPL_SUPPORTS_OSX_COPYFILE)) {
        /* Mac-to-Mac copyfile */
        return smb2fs_smb_copyfile_mac(share, src_np,
                                       tdnp, tnamep,
                                       tname_len, context);
    }

    /* We'll need a couple of fattrs */
    SMB_MALLOC(sfap,
               struct smbfattr *,
               sizeof(struct smbfattr),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (sfap == NULL) {
        SMBERROR("SMB_MALLOC failed, sfap\n");
        error = ENOMEM;
        goto out;
    }
    
    SMB_MALLOC(tfap,
               struct smbfattr *,
               sizeof(struct smbfattr),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (tfap == NULL) {
        SMBERROR("SMB_MALLOC failed, tfap\n");
        error = ENOMEM;
        goto out;
    }
    
    /*******************************/
    /* Get list of src file xattrs */
    /*******************************/
    if (share->ss_attributes & FILE_NAMED_STREAMS) {
        error = smb2fs_smb_listxattrs(share, src_np, &xattr_list,
                                      &xattrlist_len, context);
        if (error) {
            SMBDEBUG("listxattrs failed, error: %d\n", error);
            goto out;
        }
    }
    
    /********************/
    /* Open source file */
    /********************/
    desired_access = SMB2_FILE_READ_DATA | SMB2_FILE_READ_ATTRIBUTES | SMB2_FILE_READ_EA;
    share_access = NTCREATEX_SHARE_ACCESS_READ;
    disp = FILE_OPEN;
    create_flags = 0;
    
    create_options = smb2fs_smb_get_create_options(share, src_np,
                                                   NULL, NULL,
                                                   vnode_type, 0);
    error = smb2fs_smb_ntcreatex(share, src_np,
                                 NULL, 0,
                                 NULL, 0,
                                 desired_access, vnode_type,
                                 share_access, disp,
                                 create_flags, create_options,
                                 &src_fid, sfap,
                                 NULL, NULL,
                                 NULL, context);
    
    if (error) {
        SMBDEBUG("smb2fs_smb_ntcreatex failed (src file) %d\n", error);
        goto out;
    }
    
    src_is_open = TRUE;
    src_file_len = sfap->fa_size;
    
    /***************************/
    /* Open/Create target file */
    /***************************/
    desired_access = SMB2_FILE_READ_DATA | SMB2_FILE_WRITE_DATA | SMB2_FILE_APPEND_DATA |
    SMB2_FILE_READ_ATTRIBUTES | SMB2_FILE_WRITE_ATTRIBUTES | SMB2_FILE_WRITE_EA;
    disp = FILE_OVERWRITE_IF;
    create_flags = SMB2_CREATE_DO_CREATE;
    share_access = 0; /* No Access */
    
    /* Do a Create */
    error = smb2fs_smb_cmpd_create(share, tdnp,
                                   tnamep, tname_len,
                                   NULL, 0,
                                   desired_access, VREG,
                                   share_access, disp,
                                   create_flags, &ntstatus,
                                   &targ_fid, tfap,
                                   NULL, context);
    
    if (error) {
        SMBDEBUG("smb2fs_smb_cmpd_create failed (targ file) %d\n", error);
        goto out;
    }
    
    targ_is_open = TRUE;
    target_created = 1;
    
    /*************************************/
    /* Now initiate the server-side copy */
    /*************************************/
    error = smb2fs_smb_copychunks(share, src_fid,
                                  targ_fid, src_file_len,
                                  FALSE, context);
    
    if (error) {
        SMBDEBUG("smb2fs_smb_copychunks failed (file data) %d\n", error);
        goto out;
    }
    
    /********************************/
    /* Set metadata of target file. */
    /********************************/
    
    /* Set EOF of target file */
    error = smb2fs_smb_set_eof(share, targ_fid, src_file_len, context);
    if (error) {
        SMBDEBUG("failed setting target eof, error: %d\n", error);
        goto out;
    }
    
    /*
     * Set LastWriteTime timestamp of target file.
     *
     * Note: Windows clients will also set LastChangeTime here,
     * but we don't allow anyone to set the change time.
     */
    nanotime(&mtime);
    error = smbfs_smb_setfattrNT(share, 0, targ_fid, NULL, &mtime, NULL, context);
    if (error) {
        SMBDEBUG("failed setting mtime: %d\n", error);
        goto out;
    }
    
    /*********************************/
    /* Close source and target files */
    /*********************************/
    smb2_smb_close_fid(share, src_fid, NULL, NULL, NULL, context);
    smb2_smb_close_fid(share, targ_fid, NULL, NULL, NULL, context);
    src_is_open = FALSE;
    targ_is_open = FALSE;
    
    /*******************************/
    /* Copy named streams (if any) */
    /*******************************/
    xattrp = xattr_list;
    remaining_len = xattrlist_len;
    while (remaining_len) {
        this_len = strnlen(xattrp, xattrlist_len);
        
        /********************************************/
        /* Open src_xattr, Open/Create target xattr */
        /********************************************/
        desired_access = SMB2_FILE_READ_DATA | SMB2_FILE_READ_ATTRIBUTES | SMB2_FILE_READ_EA;
        share_access = NTCREATEX_SHARE_ACCESS_READ;
        disp = FILE_OPEN;
        create_flags = SMB2_CREATE_IS_NAMED_STREAM;
        
        create_options = smb2fs_smb_get_create_options(share, src_np,
                                                       NULL, xattrp,
                                                       vnode_type, 0);
        error = smb2fs_smb_ntcreatex(share, src_np,
                                     NULL, 0,
                                     xattrp, this_len,
                                     desired_access, vnode_type,
                                     share_access, disp,
                                     create_flags, create_options,
                                     &src_xattr_fid, sfap,
                                     NULL, NULL,
                                     NULL, context);
        if (error) {
            SMBDEBUG("smb2fs_smb_ntcreatex failed (src xattr), error: %d\n", error);
            goto out;
        }
        
        srcxattr_is_open = TRUE;
        src_file_len = sfap->fa_size;
        
        /* Open/Create target file xattr */
        desired_access = SMB2_FILE_READ_DATA | SMB2_FILE_WRITE_DATA | SMB2_FILE_APPEND_DATA |
        SMB2_FILE_READ_ATTRIBUTES | SMB2_FILE_WRITE_ATTRIBUTES | SMB2_FILE_WRITE_EA;
        disp = FILE_OVERWRITE_IF;
        create_flags = SMB2_CREATE_IS_NAMED_STREAM | SMB2_CREATE_DO_CREATE;
        share_access = 0; /* No Access */
        
        /* Do a Create */
        error = smb2fs_smb_cmpd_create(share, tdnp,
                                       tnamep, tname_len,
                                       xattrp, this_len,
                                       desired_access, VREG,
                                       share_access, disp,
                                       create_flags, &ntstatus,
                                       &targ_xattr_fid, tfap,
                                       NULL, context);
        
        if (error) {
            SMBDEBUG("smb2fs_smb_cmpd_create failed (targ xattr), error: %d\n", error);
            goto out;
        }
        
        targxattr_is_open = TRUE;
        
        /*************************************/
        /* Now initiate the server-side copy */
        /*************************************/
        error = smb2fs_smb_copychunks(share, src_xattr_fid,
                                      targ_xattr_fid, src_file_len,
                                      FALSE, context);
        
        if (error) {
            SMBDEBUG("smb2fs_smb_copychunks failed (xattr), error: %d\n", error);
            goto out;
        }
        
        /*********************************/
        /* Set metadata of target xattr. */
        /*********************************/
        
        /* Set EOF */
        error = smb2fs_smb_set_eof(share, targ_xattr_fid, src_file_len, context);
        if (error) {
            SMBDEBUG("failed setting target xattr eof, error: %d\n", error);
            goto out;
        }
        
        /*
         * Set LastWriteTime timestamp of target file.
         *
         * Note: Windows clients will also set LastChangeTime here,
         * but we don't allow anyone to set the change time.
         */
        nanotime(&mtime);
        error = smbfs_smb_setfattrNT(share, 0, targ_xattr_fid, NULL, &mtime, NULL, context);
        if (error) {
            SMBDEBUG("failed setting xattr mtime: %d\n", error);
            goto out;
        }
        
        /**********************************/
        /* Close source and target xattrs */
        /**********************************/
        smb2_smb_close_fid(share, src_xattr_fid, NULL, NULL, NULL, context);
        smb2_smb_close_fid(share, targ_xattr_fid, NULL, NULL, NULL, context);
        srcxattr_is_open = FALSE;
        targxattr_is_open = FALSE;
        
        /* Skip over terminating NULL, advance to next xattr name */
        this_len += 1;
        remaining_len -= this_len;
        if (xattrlist_len) {
            xattrp += this_len;
        }
    }
    
out:
    /* Clean house */
    if (src_is_open == TRUE) {
        smb2_smb_close_fid(share, src_fid, NULL, NULL, NULL, context);
    }
    
    if (srcxattr_is_open == TRUE) {
        smb2_smb_close_fid(share, src_xattr_fid, NULL, NULL, NULL, context);
    }
    
    if (targ_is_open == TRUE) {
        smb2_smb_close_fid(share, targ_fid, NULL, NULL, NULL, context);
    }
    
    if (targxattr_is_open == TRUE) {
        smb2_smb_close_fid(share, targ_xattr_fid, NULL, NULL, NULL, context);
    }
    
    if (xattr_list != NULL) {
        SMB_FREE(xattr_list, M_SMBTEMP);
    }
    
    if (sfap != NULL) {
        SMB_FREE(sfap, M_SMBTEMP);
    }
    
    if (tfap != NULL) {
        SMB_FREE(tfap, M_SMBTEMP);
    }
    
    if ((error) && (target_created == 1)) {
        /* Try to delete the target file we created */
        smbfs_smb_delete(share, tdnp, VREG,
                         tnamep, tname_len,
                         0, context);
    }
    
    return error;
}

static int
smb2fs_smb_copyfile_mac(struct smb_share *share, struct smbnode *src_np,
                        struct smbnode *tdnp, const char *tnamep, size_t tname_len,
                        vfs_context_t context)
{
    struct smbfattr *sfap = NULL, *tfap = NULL;
    SMBFID          src_fid = 0, targ_fid = 0;
    uint32_t        desired_access, share_access, disp;
    boolean_t       src_is_open = FALSE, targ_is_open = FALSE;
    uint64_t        create_flags;
    int             error = 0;
    uint32_t        ntstatus = 0;
    uint32_t        target_created = 0;
    uint32_t create_options = 0;
    enum vtype vnode_type = VREG;
    
    /* We'll need a couple of fattrs */
    SMB_MALLOC(sfap,
               struct smbfattr *,
               sizeof(struct smbfattr),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (sfap == NULL) {
        SMBERROR("SMB_MALLOC failed, sfap\n");
        error = ENOMEM;
        goto out;
    }
    
    SMB_MALLOC(tfap,
               struct smbfattr *,
               sizeof(struct smbfattr),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (tfap == NULL) {
        SMBERROR("SMB_MALLOC failed, tfap\n");
        error = ENOMEM;
        goto out;
    }
        
    /*********************************************/
    /* Open source file, Open/Create target file */
    /*********************************************/
    desired_access = SMB2_FILE_READ_DATA | SMB2_FILE_READ_ATTRIBUTES | SMB2_FILE_READ_EA;
    share_access = NTCREATEX_SHARE_ACCESS_READ;
    disp = FILE_OPEN;
    create_flags = 0;
    
    create_options = smb2fs_smb_get_create_options(share, src_np,
                                                   NULL, NULL,
                                                   vnode_type, 0);
    error = smb2fs_smb_ntcreatex(share, src_np,
                                 NULL, 0,
                                 NULL, 0,
                                 desired_access, vnode_type,
                                 share_access, disp,
                                 create_flags, create_options,
                                 &src_fid, sfap,
                                 NULL, NULL,
                                 NULL, context);
    
    if (error) {
        SMBDEBUG("smb2fs_smb_ntcreatex failed (src file) %d\n", error);
        goto out;
    }
    
    src_is_open = TRUE;
    
    /***************************/
    /* Open/Create target file */
    /***************************/
    desired_access = SMB2_FILE_READ_DATA | SMB2_FILE_WRITE_DATA | SMB2_FILE_APPEND_DATA |
    SMB2_FILE_READ_ATTRIBUTES | SMB2_FILE_WRITE_ATTRIBUTES | SMB2_FILE_WRITE_EA;
    disp = FILE_OVERWRITE_IF;
    create_flags = SMB2_CREATE_DO_CREATE;
    share_access = 0; /* No Access */
    
    /* Do a Create */
    error = smb2fs_smb_cmpd_create(share, tdnp,
                                   tnamep, tname_len,
                                   NULL, 0,
                                   desired_access, VREG,
                                   share_access, disp,
                                   create_flags, &ntstatus,
                                   &targ_fid, tfap,
                                   NULL, context);
    
    if (error) {
        SMBDEBUG("smb2fs_smb_cmpd_create failed (targ file) %d\n", error);
        goto out;
    }
    
    targ_is_open = TRUE;
    target_created = 1;
    
    /*************************************/
    /* Now initiate the server-side copy */
    /*************************************/
    error = smb2fs_smb_copychunks(share, src_fid,
                                  targ_fid, 0,
                                  TRUE, context);
    
    if (error) {
        SMBDEBUG("smb2fs_smb_copychunks_mac failed (file data) %d\n", error);
        goto out;
    }
        
    /*********************************/
    /* Close source and target files */
    /*********************************/
    smb2_smb_close_fid(share, src_fid, NULL, NULL, NULL, context);
    smb2_smb_close_fid(share, targ_fid, NULL, NULL, NULL, context);
    src_is_open = FALSE;
    targ_is_open = FALSE;
        
out:
    /* Clean house */
    if (src_is_open == TRUE) {
        smb2_smb_close_fid(share, src_fid, NULL, NULL, NULL, context);
    }
    
    if (targ_is_open == TRUE) {
        smb2_smb_close_fid(share, targ_fid, NULL, NULL, NULL, context);
    }
    
    if (sfap != NULL) {
        SMB_FREE(sfap, M_SMBTEMP);
    }
    
    if (tfap != NULL) {
        SMB_FREE(tfap, M_SMBTEMP);
    }
    
    if ((error) && (target_created == 1)) {
        /* Try to delete the target file we created */
        smbfs_smb_delete(share, tdnp, VREG,
                         tnamep, tname_len,
                         0, context);
    }

    return error;
}

int
smbfs_smb_create_reparse_symlink(struct smb_share *share, struct smbnode *dnp,
                                 const char *namep, size_t name_len,
                                 char *targetp, size_t target_len,
                                 struct smbfattr *fap, vfs_context_t context)
{
    int error;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_cmpd_reparse_point_set(share, dnp,
                                                  namep, name_len,
                                                  targetp, target_len,
                                                  fap, context);
    }
    else {
        error = smb1fs_smb_create_reparse_symlink(share, dnp,
                                                  namep, name_len,
                                                  targetp, target_len,
                                                  fap, context);
    }
    
    return error;
    
}

static int
smb2fs_smb_delete(struct smb_share *share, struct smbnode *np, enum vtype vnode_type,
                  const char *namep, size_t name_len,
                  int xattr, vfs_context_t context)
{
    int error;
    uint32_t setinfo_ntstatus;
    uint8_t delete_byte = 1;

    /*
     * Looking at Win <-> Win with SMB 2/3, delete is handled by opening the file
     * with Delete and "Read Attributes", then a Set Info is done to set
     * "Delete on close", then a Close is sent.
     */
    
    /*
     * Do the Compound Create/SetInfo/Close call
     */
    error = smb2fs_smb_cmpd_set_info(share, np, vnode_type,
                                     namep, name_len,
                                     xattr, SMB2_FILE_READ_ATTRIBUTES | SMB2_STD_ACCESS_DELETE | SMB2_SYNCHRONIZE, 
                                     SMB2_0_INFO_FILE, FileDispositionInformation,
                                     0,
                                     sizeof (delete_byte), (uint8_t *) &delete_byte,
                                     &setinfo_ntstatus,
                                     context);
    if (error) {
        if (error != ENOTEMPTY) {
            SMBDEBUG_LOCK(np, "smb2fs_smb_cmpd_set_info failed %d %s:%s\n", error, np->n_name, namep);
        }
        goto bad;
    }
    
bad:    
    return error;
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smbfs_smb_delete(struct smb_share *share, struct smbnode *np, enum vtype vnode_type,
				 const char *name, size_t nmlen,
                 int xattr, vfs_context_t context)
{
    int error;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_delete (share, np, vnode_type,
                                   name, nmlen,
                                   xattr, context);
    }
    else {
        error = smb1fs_smb_delete (share, np, name, nmlen, xattr, context);
    }
    
    return error;
}

static uint32_t
smb2fs_smb_fillchunk_arr(struct smb2_copychunk_chunk *chunk_arr,
                         uint32_t chunk_arr_size,
                         uint64_t total_len, uint32_t max_chunk_len,
                         uint64_t src_offset, uint64_t trg_offset,
                         uint32_t *chunk_count_out, uint64_t *total_len_out)
{
    uint64_t len_remaining = total_len;
    uint64_t total_chunk_len, src_off, trg_off;
    uint32_t i, chunk_count, this_chunk_len;
    
    src_off = src_offset;
    trg_off = trg_offset;
    
    chunk_count = 0;
    total_chunk_len = 0;
    for (i = 0; i < chunk_arr_size; i++) {
        if (!len_remaining) {
            // all done
            break;
        }
        
        if (len_remaining > max_chunk_len) {
            this_chunk_len = max_chunk_len;
        }
        else {
            /* Cast is fine, here */
            this_chunk_len = (uint32_t)len_remaining;
        }
        
        chunk_arr[i].length = this_chunk_len;
        chunk_arr[i].reserved = 0;
        chunk_arr[i].source_offset = src_off;
        chunk_arr[i].target_offset = trg_off;
        
        total_chunk_len += this_chunk_len;
        len_remaining -= this_chunk_len;
        chunk_count++;
        src_off += this_chunk_len;
        trg_off += this_chunk_len;
    }
    
    *chunk_count_out = chunk_count;
    *total_len_out = total_chunk_len;
    return (0);
}

int
smbfs_smb_findclose(struct smbfs_fctx *ctx, vfs_context_t context)
{	
    int error;
    
    if (SS_TO_SESSION(ctx->f_share)->session_flags & SMBV_SMB2) {
        if (ctx->f_create_rqp) {
            smb_rq_done(ctx->f_create_rqp);
            ctx->f_create_rqp = NULL;
        }
        if (ctx->f_query_rqp) {
            smb_rq_done(ctx->f_query_rqp);
            ctx->f_query_rqp = NULL;
        }
        
        /* Close Create FID if we need to */
        if (ctx->f_need_close == TRUE) {
            error = smb2_smb_close_fid(ctx->f_share, ctx->f_create_fid, 
                                       NULL, NULL, NULL, context);
            if (error) {
                SMBDEBUG("smb2_smb_close_fid failed %d\n", error);
            }

			ctx->f_need_close = FALSE;
        }
        
        /* We are done with the share release our reference */
        smb_share_rele(ctx->f_share, context);
        
        if (ctx->f_LocalName) {
            SMB_FREE(ctx->f_LocalName, M_SMBFSDATA);
        }
        
        if (ctx->f_NetworkNameBuffer) {
            SMB_FREE(ctx->f_NetworkNameBuffer, M_SMBFSDATA);
        }
        
        if (ctx->f_rname) {
            SMB_FREE(ctx->f_rname, M_SMBFSDATA);
        }
        
        SMB_FREE(ctx, M_SMBFSDATA);
        return 0;
    }
    else {
        error = smb1fs_smb_findclose(ctx, context);
        return (error);
    }
    
}

static int
smb2fs_smb_findnext(struct smbfs_fctx *ctx, vfs_context_t context)
{
	struct timespec ts;
    int error = EINVAL;
	struct mdchain *mdp;
    struct smb2_query_dir_rq *queryp = NULL;
    uint8_t info_class, flags; 
    uint32_t file_index;
    int attempts = 0;
    
    SMB_MALLOC(queryp,
               struct smb2_query_dir_rq *,
               sizeof(struct smb2_query_dir_rq),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (queryp == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
	if (ctx->f_output_buf_len == 0) {
        /*
         * if no more output buffer bytes to parse, then we have finished
         * parsing out all the entries from this search.
         */
		if (ctx->f_flags & SMBFS_RDD_EOF) {
            error = ENOENT;
            goto bad;
        }
        
		nanouptime(&ts);
        
        /* free any previous search requests */
        if (ctx->f_create_rqp) {
            smb_rq_done(ctx->f_create_rqp);
            ctx->f_create_rqp = NULL;
        }
        if (ctx->f_query_rqp) {
            smb_rq_done(ctx->f_query_rqp);
            ctx->f_query_rqp = NULL;
        }
        
        /* Clear resume file name */
        ctx->f_flags &= ~SMBFS_RDD_GOTRNAME;

        /* Is the dir already open? */
        if (ctx->f_need_close == FALSE) {
            /*
             * Dir needs to be opened first so do a Create/Query Dir
             * fid is -1 for compound requests
             */
            ctx->f_create_fid = 0xffffffffffffffff;
        }
        
        /*
         * Set up for the Query Dir call 
         */
        
        /* If not a wildcard search, then want first search entry returned */
        flags = 0;
        if (ctx->f_flags & SMBFS_RDD_FINDSINGLE) {
            flags |= SMB2_RETURN_SINGLE_ENTRY;
        }
        
        /* Resuming a search happens by default, no need to use File Index */
        file_index = 0;

        if (ctx->f_flags & SMBFS_RDD_FINDFIRST) {
            /* Because this is first search, set some flags */
            flags |= SMB2_RESTART_SCANS;    /* start search from beginning */
            file_index = 0;                 /* no FileIndex from prev search */
        } 

        switch (ctx->f_infolevel) {
            case SMB_FIND_FULL_DIRECTORY_INFO:
                /* For SMB 2/3, get the file/dir IDs too but no short names */
                info_class = FileIdFullDirectoryInformation;
                break;

            case SMB_FIND_BOTH_DIRECTORY_INFO:
                /* For SMB 2/3, get the file/dir IDs too */
                info_class = FileIdBothDirectoryInformation;
                break;
                
            default:
                SMBERROR("invalid infolevel %d", ctx->f_infolevel);
                error = EINVAL;
                goto bad;
        }
        
again:
        queryp->file_info_class = info_class;
        queryp->flags = flags;
        queryp->file_index = file_index;
        queryp->fid = ctx->f_create_fid;
        queryp->mc_flags = 0;
        
        /* handle servers that dislike large output buffer lens */
        if (SS_TO_SESSION(ctx->f_share)->session_misc_flags & SMBV_64K_QUERY_DIR) {
            queryp->output_buffer_len = kSMB_64K;
        }
        else {
            queryp->output_buffer_len = MIN(SS_TO_SESSION(ctx->f_share)->session_txmax,
                                            kSMB_MAX_TX);
        }

        /* 
         * Copy in whether to use UTF_SFM_CONVERSIONS or not 
         * Seems like if NOT a wildcard, then use UTF_SFM_CONVERSIONS
         */
        queryp->name_flags = ctx->f_sfm_conversion;
        
        queryp->dnp = ctx->f_dnp;
        queryp->namep = (char*) ctx->f_lookupName;
        queryp->name_len = (uint32_t) ctx->f_lookupNameLen;

        if (ctx->f_need_close == FALSE) {
            /* Build and send a Create/Query dir */
            error = smb2fs_smb_cmpd_query_dir(ctx, queryp, context);
        }
        else {
            /* Just send a single Query Dir */
            error = smb2_smb_query_dir(ctx->f_share, queryp, NULL, NULL, context);

            /* save f_query_rqp so it can be freed later */
            ctx->f_query_rqp = queryp->ret_rqp;
        }

        if (error) {
            if (error == ENOENT) {
                ctx->f_flags |= SMBFS_RDD_EOF;

                /*
                 * If there are no more entries, then free the f_create_rqp
                 * and f_query_rqp now to free up the iod reference being held
                 * by those rqp's. Since the Query Dir return ENOENT, there
                 * are no entries to parse so we dont need to hang on to those
                 * rqp's.
                 */

                if (ctx->f_create_rqp) {
                    smb_rq_done(ctx->f_create_rqp);
                    ctx->f_create_rqp = NULL;
                }
                if (ctx->f_query_rqp) {
                    smb_rq_done(ctx->f_query_rqp);
                    ctx->f_query_rqp = NULL;
                }
            }
            
            /* handle servers that dislike large output buffer lens */
            if ((error == EINVAL) && 
                (queryp->ret_ntstatus == STATUS_INVALID_PARAMETER) &&
                !((SS_TO_SESSION(ctx->f_share)->session_misc_flags) & SMBV_64K_QUERY_DIR) &&
                (attempts == 0)) {
                SMBWARNING("SMB 2/3 server cant handle large OutputBufferLength in Query_Dir. Reducing to 64Kb.\n");
                SS_TO_SESSION(ctx->f_share)->session_misc_flags |= SMBV_64K_QUERY_DIR;
                attempts += 1;
                
                if (ctx->f_create_rqp) {
                    smb_rq_done(ctx->f_create_rqp);
                    ctx->f_create_rqp = NULL;
                }
                if (ctx->f_query_rqp) {
                    smb_rq_done(ctx->f_query_rqp);
                    ctx->f_query_rqp = NULL;
                }
                goto again;
            }
            
            goto bad;
        }

        ctx->f_output_buf_len = queryp->ret_buffer_len;
        
        if (ctx->f_flags & SMBFS_RDD_FINDFIRST) {
            /* next find will be a Find Next */
            ctx->f_flags &= ~SMBFS_RDD_FINDFIRST;
        }
        
        ctx->f_eofs = 0;
        ctx->f_attr.fa_reqtime = ts;
	}
    
    /*
     * Either we did a new search and we are parsing the first entry out or
     * we are just parsing more names out of a previous search.
     */
    ctx->f_NetworkNameLen = 0;
    
    /* at this point, mdp is pointing to output buffer */
    if (ctx->f_create_rqp != NULL) {
        /*
         * <14227703> Check to see if server is using non compound replies.
         * If SMB2_RESPONSE is set in queyr_rqp, then server is not using 
         * compound replies and thusreply is in the query_rqp
         */
        if (!(ctx->f_query_rqp->sr_extflags & SMB2_RESPONSE)) {
            /* Did a compound request so data is in create_rqp */
            smb_rq_getreply(ctx->f_create_rqp, &mdp);
        }
        else {
            /* Server does not support compound replies */
            smb_rq_getreply(ctx->f_query_rqp, &mdp);
        }
    }
    else {
        /* Only a Query Dir, so data is in query_rqp */
        smb_rq_getreply(ctx->f_query_rqp, &mdp);
    }
    
    /* 
     * Parse one entry out of the output buffer and store results into ctx 
     */
	switch (ctx->f_infolevel) {
        case SMB_FIND_FULL_DIRECTORY_INFO:
        case SMB_FIND_BOTH_DIRECTORY_INFO:
            /* Call the parsing function */
            error = smb2_smb_parse_query_dir_both_dir_info(ctx->f_share, mdp,
                                                           ctx->f_infolevel,
                                                           ctx, &ctx->f_attr,
                                                           ctx->f_NetworkNameBuffer, &ctx->f_NetworkNameLen,
                                                           ctx->f_MaxNetworkNameBufferSize);
            if (error) {
                goto bad;
            }            
            break;
        default:
            SMBERROR("unexpected info level %d\n", ctx->f_infolevel);
            return EINVAL;
	}
    
bad:
    if (queryp != NULL) {
        SMB_FREE(queryp, M_SMBTEMP);
    }
    
    return error;
}

int
smbfs_smb_findnext(struct smbfs_fctx *ctx, vfs_context_t context)
{
   	struct smb_session *sessionp = SS_TO_SESSION(ctx->f_share);
	int error;
    
    if (sessionp->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_findnext(ctx, context);
    }
    else {
        error = smb1fs_smb_findnext(ctx, context);
    }
    
    return error;
}

/*
 * This routine will send a flush across the wire to the server. This is an expensive
 * operation that should only be done when the user request it. 
 *
 * The calling routine must hold a reference on the share
 *
 */ 
int 
smbfs_smb_flush(struct smb_share *share, SMBFID fid, uint32_t full_sync,
				vfs_context_t context)
{
    int error;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2_smb_flush(share, fid, full_sync, context);
    }
    else {
        error = smb1fs_smb_flush(share, fid, context);
    }
    return error;
}

uint32_t
smb2fs_smb_get_create_options(struct smb_share *share, struct smbnode *np,
                              const char *namep, const char *strm_namep,
                              enum vtype vnode_type, uint32_t check_reparse)
{
    uint32_t create_options = 0;
    
    /*
     * smb2fs_smb_get_create_options() has various ways it can be called
     * 1) np - open item (np)
     * 2) np, namep - open the child (namep) in parent dir (np)
     * 3) np, strm_namep - open named stream (strm_namep) of item (np)
     * 4) np, namep, strm_namep - open named stream of (strm_namep) of
     * child (namep) in parent dir (np)
     */
    
    /* If its a dir, then set the dir option */
    if (vnode_type == VDIR) {
        create_options |= NTCREATEX_OPTIONS_DIRECTORY;
    }
    
    /*
     * If it a reparse point, then we have to be careful on whether to set
     * NTCREATEX_OPTIONS_OPEN_REPARSE_POINT or not.
     *
     * Most of the time we do not set the reparse bit. This allows the
     * reparse point to go ahead and trigger a mount if needed.  For example,
     * if the reparse point is a folder that is really an NTFS mount.
     *
     * If we are doing Create/QueryInfo or Create/SetInfo on the reparse point,
     * then set the reparse bit so we change the reparse point itself and not
     * its target.
     */
    if (check_reparse == 0) {
        /* Go ahead and trigger the reparse point */
        goto done;
    }
    
    if ((share) && !(share->ss_attributes & FILE_SUPPORTS_REPARSE_POINTS)) {
        /* No reparse points, so no need to set reparse point bit */
        goto done;
    }
        
    /* Case (1) */
    if ((np) &&
        (namep == NULL) &&
        (strm_namep == NULL)) {
        
        /* Check for dataless file */
        if ((np->n_dosattr & SMB_EFA_REPARSE_POINT) &&
            (np->n_reparse_tag == IO_REPARSE_TAG_STORAGE_SYNC)) {
            /*
             * If M bit is set, then its a newer server and we know for sure that
             * reading the file will recall it. Opening the file is fine.
             *
             * We want to always set NTCREATEX_OPTIONS_OPEN_REPARSE_POINT to try
             * to keep virus software from possibly recalling the file on their own
             * when they see a file open for read access.
             */
            if (np->n_dosattr & SMB_EFA_RECALL_ON_DATA_ACCESS) {
                create_options |= NTCREATEX_OPTIONS_OPEN_REPARSE_POINT;
            }
            else {
                /*
                 * If P or O bit is set, for now behave just like the M bit
                 * to try to keep virus software from recalling the file.
                 *
                 * This is separated out in case we change our mind on this
                 * behavior.
                 */
                if (np->n_dosattr & (SMB_EFA_OFFLINE | SMB_EFA_SPARSE)) {
                    create_options |= NTCREATEX_OPTIONS_OPEN_REPARSE_POINT;
                }
            }
        }
        else {
            /*
             * Previous behavior
             *
             * If its some type of reparse point and it has not been moved to
             * offline storage, open the reparse point itself.
             */
            if ((np->n_dosattr & SMB_EFA_REPARSE_POINT) &&
                !(np->n_dosattr & SMB_EFA_OFFLINE)) {
                create_options |= NTCREATEX_OPTIONS_OPEN_REPARSE_POINT;
            }
        }
    }

    /* 
     * For cases (2) - (4), the Create will be on a parent dir or opening a 
     * named stream. In either case we should not need to set the
     * NTCREATEX_OPTIONS_OPEN_REPARSE_POINT bit.
     *
     * If the vnode type is VLNK, it must be a dir enumeration getting 
     * information about a sym link.
     */
    if (vnode_type == VLNK) {
        create_options |= NTCREATEX_OPTIONS_OPEN_REPARSE_POINT;
    }

done:
    return(create_options);
}

static int
smb2fs_smb_listxattrs(struct smb_share *share, struct smbnode *np, char **xattrlist,
                      size_t *xattrlist_len, vfs_context_t context)
{
    uio_t   xuio = NULL;
    size_t  xattr_len = 0;
    char    *xattrb = NULL;
    uint32_t flags = 0, max_access = 0;
    int     error = 0;
    enum vtype vnode_type = VREG;

    if (np == NULL) {
        SMBERROR("Missing vnode\n");
        error = EINVAL;
        goto out;
    }

    if ((np) && (np->n_vnode)) {
        /* Use vnode to determine type */
        vnode_type = vnode_isdir(np->n_vnode) ? VDIR : VREG;
    }

    /* Only pass stream_buf_sizep, so we can determine the size of the list */
    error = smb2fs_smb_qstreaminfo(share, np, vnode_type,
                                   NULL, 0,
                                   NULL,
                                   NULL, &xattr_len,
                                   NULL, NULL,
                                   &flags, &max_access,
                                   context);
    if (error) {
        if (error !=ENOATTR) {
            SMBERROR("qstreaminfo error: %d\n", error);
            goto out;
        }
        error = 0;
    }
    
    if (!xattr_len) {
        /* np does not have any xattrs */
        goto out;
    }
    
    /* Setup a uio and buffer for the xattr list */
    SMB_MALLOC(xattrb,
               char *,
               xattr_len,
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (xattrb == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto out;
    }
    
    xuio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
    if (xuio == NULL) {
        SMBERROR("uio_create failed\n");
        error = ENOMEM;
        goto out;
    }
    
    error = uio_addiov(xuio, CAST_USER_ADDR_T(xattrb), xattr_len);
    
    if (error) {
        SMBERROR("uio_addiov failed, error: %d", error);
        goto out;
    }
    
    uio_setoffset(xuio, 0);
    
    /* Only pass a uio and stream_buf_sizep, to get the entire list of xattrs */
    flags = SMB_NO_TRANSLATE_NAMES;  /* Do not translate AFP_Resource & AFP_AfpInfo */
    error = smb2fs_smb_qstreaminfo(share, np, vnode_type,
                                   NULL, 0,
                                   NULL,
                                   xuio, &xattr_len,
                                   NULL, NULL,
                                   &flags, &max_access,
                                   context);
    
    if (error) {
        SMBDEBUG("qstream failed, error: %d", error);
        goto out;
    }
    
    /* Return the results */
    *xattrlist = xattrb;
    *xattrlist_len = xattr_len;
    
    /* Clean up */
    uio_free(xuio);
    
    return (0);

out:
    if (xuio != NULL) {
        uio_free(xuio);
    }
    if (xattrb != NULL) {
        SMB_FREE(xattrb, M_SMBTEMP);
    }
    
    *xattrlist = NULL;
    *xattrlist_len = 0;
    
    return (error);
}

int 
smbfs_smb_lock(struct smb_share *share, int op, SMBFID fid, uint32_t pid,
               off_t start, uint64_t len, uint32_t timo, vfs_context_t context)
{
    int error;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2_smb_lock(share, op, fid, start, len, context);
    }
    else {
        error = smb1fs_smb_lock(share, op, fid, pid, start, len, timo, context);
    }
    return error;
}

static int
smb2fs_smb_markfordelete(struct smb_share *share, SMBFID fid, vfs_context_t context)
{
    int error;
    struct smb2_set_info_rq *infop = NULL;
    uint8_t delete_byte = 1;
    
    SMB_MALLOC(infop, 
               struct smb2_set_info_rq *, 
               sizeof(struct smb2_set_info_rq), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (infop == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }

    /* 
     * Set up for the Set Info call
     */
    infop->info_type = SMB2_0_INFO_FILE;
    infop->file_info_class = FileDispositionInformation;
    infop->add_info = 0;
    infop->fid = fid;
    infop->input_buffer = (uint8_t *) &delete_byte;
    infop->mc_flags = 0;
    
    error = smb2_smb_set_info(share, infop, NULL, NULL, context);
    if (error) {
        SMBDEBUG("smb2_smb_set_info failed %d ntstatus %d\n",
                 error,
                 infop->ret_ntstatus);
        goto bad;
    }
    
bad:    
    if (infop != NULL) {
        SMB_FREE(infop, M_SMBTEMP);
    }
    
    return error;
}

/*
 * smbfs_smb_markfordelete
 *
 * We have an open file that they want to delete. This call will tell the 
 * server to delete the file when the last close happens. Currenly we know that 
 * XP, Windows 2000 and Windows 2003 support this call. SAMBA does support the
 * call, but currently has a bug that prevents it from working.
 *
 * The calling routine must hold a reference on the share
 *
 */
int
smbfs_smb_markfordelete(struct smb_share *share, SMBFID fid, vfs_context_t context)
{
    int error;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_markfordelete(share, fid, context);
    }
    else {
        error = smb1fs_smb_markfordelete(share, fid, context);
    }
    return error;
}

int 
smb2fs_smb_ntcreatex(struct smb_share *share, struct smbnode *np, 
                     const char *namep, size_t in_nmlen, 
                     const char *strm_namep, size_t in_strm_nmlen,
                     uint32_t desired_access, enum vtype vnode_type,
                     uint32_t share_access, uint32_t disposition,
                     uint64_t create_flags, uint32_t create_options,
                     SMBFID *fidp, struct smbfattr *fap,
                     struct smb_rq **compound_rqp, struct smb2_create_rq **in_createp,
                     void *create_contextp, vfs_context_t context)
{
	uint32_t file_attributes;
	int error;
	size_t name_len = in_nmlen;	
	size_t strm_name_len = in_strm_nmlen;
    /* Don't change the input name length, we need it for making the ino number */
    struct smb2_create_rq *createp = NULL;
    uint32_t impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
    uint8_t oplock_level = SMB2_OPLOCK_LEVEL_NONE;
    char *snamep = NULL;
    
    /*
     * smb2fs_smb_ntcreatex() has various ways it can be called
     * 1) np - open item (np)
     * 2) np, namep - open the child (namep) in parent dir (np)
     * 3) np, strm_namep - open named stream (strm_namep) of item (np)
     * 4) np, namep, strm_namep - open named stream of (strm_namep) of 
     * child (namep) in parent dir (np)
     */

    if (fap) {
        bzero(fap, sizeof(*fap));
        nanouptime(&fap->fa_reqtime);
    }
    
    SMB_MALLOC(createp, 
               struct smb2_create_rq *, 
               sizeof(struct smb2_create_rq), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (createp == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    /* determine file attributes */
    if (vnode_type == VDIR) {
        file_attributes = SMB_EFA_DIRECTORY;
    }
    else {
        file_attributes = SMB_EFA_NORMAL;
    }
	if ((disposition != FILE_OPEN) &&
        (!(create_flags & SMB2_CREATE_IS_NAMED_STREAM))) {
		if (file_attributes == SMB_EFA_NORMAL) {
			file_attributes |= SMB_EFA_ARCHIVE;
        }
		if ((namep) && (*namep == '.')) {
			file_attributes |= SMB_EFA_HIDDEN;
        }
	}
    
    /* start wth the passed in flag settings */
    createp->flags |= create_flags;

    /* Do they want to open the resource fork? */
    if ((np) && (np->n_vnode) && 
        (vnode_isnamedstream(np->n_vnode)) && 
        (!strm_namep) && (!(create_flags & SMB2_CREATE_IS_NAMED_STREAM))) {
        lck_rw_lock_shared(&np->n_name_rwlock);
        snamep = smb_strndup(np->n_sname, np->n_snmlen);
        strm_namep = snamep;
        lck_rw_unlock_shared(&np->n_name_rwlock);

        strm_name_len = np->n_snmlen;
        createp->flags |= SMB2_CREATE_IS_NAMED_STREAM;
    }
    
    if (create_flags & SMB2_CREATE_DUR_HANDLE_RECONNECT) {
        impersonation_level = 0;
        file_attributes = 0;
        create_options = 0;
        createp->create_contextp = create_contextp;
        oplock_level = SMB2_OPLOCK_LEVEL_LEASE;
    }
    else {
        if (create_flags & (SMB2_CREATE_DUR_HANDLE | SMB2_CREATE_DIR_LEASE)) {
            createp->create_contextp = create_contextp;
            oplock_level = SMB2_OPLOCK_LEVEL_LEASE;
        }
        else {
            if (create_flags & SMB2_CREATE_HANDLE_RECONNECT) {
                createp->create_contextp = create_contextp;
            }
        }
    }
    
    if (create_flags & SMB2_CREATE_AAPL_RESOLVE_ID) {
        createp->create_contextp = create_contextp;
    }

    /*
     * Set up for the Create call
     */
    createp->oplock_level = oplock_level;
    createp->impersonate_level = impersonation_level;
    
    createp->desired_access = desired_access;
    if (create_flags & SMB2_CREATE_GET_MAX_ACCESS) {
        /* since getting max access, make sure SMB2_FILE_READ_ATTRIBUTES set */
        createp->desired_access |= SMB2_FILE_READ_ATTRIBUTES;
    }
    createp->file_attributes = file_attributes;
    createp->share_access = share_access;
    createp->disposition = disposition;
    createp->create_options = create_options;
    createp->name_len = (uint32_t) name_len;
    createp->namep = (char *) namep;
    createp->strm_name_len = (uint32_t) strm_name_len;
    createp->strm_namep = (char *) strm_namep;
    createp->dnp = np;
    createp->mc_flags = (create_flags & SMB2_CREATE_REPLAY_FLAG)?SMB2_MC_REPLAY_FLAG:0;

    /* 
     * Do the Create call 
     */
    error = smb2_smb_create(share, createp, compound_rqp, context);
    if (error) {
        if ((error != ENOENT) && (error != EEXIST) && (error != EBUSY)) {
            SMBDEBUG("smb2_smb_create failed %d ntstatus 0x%x\n",
                     error,
                     createp->ret_ntstatus);
        }
        goto bad;
    }
    
    /* Building a compound requests */
    if (in_createp != NULL) {
        *in_createp = createp;
        return (0);
    }

	if (fidp) {
		*fidp = createp->ret_fid;
    }
    
    /* 
     * Fill in fap and possibly update vnode's meta data caches
     */
    error = smb2fs_smb_parse_ntcreatex(share, np, createp, 
                                       fidp, fap, context);
    
bad:
    if (createp != NULL) {
        SMB_FREE(createp, M_SMBTEMP);
    }

    if (snamep) {
        SMB_FREE(snamep, M_SMBSTR);
    }
    
	return error;
}

/*
 * Modern create/open of file or directory.
 *
 * If disp is FILE_OPEN then this is an open attempt, and:
 *   If xattr then name is the stream to be opened at np,
 *   Else np should be opened.
 *   ...we won't touch *fidp,
 * Else this is a creation attempt, and:
 *   If xattr then name is the stream to create at np,
 *   Else name is the thing to create under directory np.
 *   ...we will return *fidp,
 *
 * The calling routine must hold a reference on the share
 *
 * Either pass in np which is the file/dir to open OR
 * pass in dnp and a name 
 *
 */
int 
smbfs_smb_ntcreatex(struct smb_share *share, struct smbnode *np, 
                    uint32_t rights, uint32_t shareMode, enum vtype vt,
                    SMBFID *fidp, const char *name, size_t in_nmlen,
                    uint32_t disp, int xattr, struct smbfattr *fap,
                    int do_create, struct smb2_durable_handle *dur_handlep, vfs_context_t context)
{
    int error;
    uint64_t create_flags = 0;
    char *file_namep = NULL, *stream_namep = NULL;
    size_t file_name_len = 0, stream_name_len = 0;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        create_flags = SMB2_CREATE_GET_MAX_ACCESS;   /* Always set want_max_access */

        if (do_create) {
            create_flags |= SMB2_CREATE_DO_CREATE;
        }

        if (dur_handlep) {
            if (dur_handlep->flags & (SMB2_DURABLE_HANDLE_RECONNECT | SMB2_PERSISTENT_HANDLE_RECONNECT)) {
                create_flags = SMB2_CREATE_DUR_HANDLE_RECONNECT;
            }
            else {
                if (dur_handlep->flags & (SMB2_DURABLE_HANDLE_REQUEST | SMB2_PERSISTENT_HANDLE_REQUEST)) {
                    create_flags = SMB2_CREATE_DUR_HANDLE;
                }
                else {
                    /* Reconnect reopening shared fork file */
                    create_flags = SMB2_CREATE_HANDLE_RECONNECT;
                }
            }
        }
        
        if (!xattr) {
            file_namep = (char *) name;
            file_name_len = in_nmlen;
        }
        else {
            /* name is actually the stream name */
            create_flags |= SMB2_CREATE_IS_NAMED_STREAM;
            
            stream_namep = (char *) name;
            stream_name_len = in_nmlen;
        }
        
        error = smb2fs_smb_cmpd_create(share, np,
                                       file_namep, file_name_len,
                                       stream_namep, stream_name_len,
                                       rights, vt,
                                       shareMode, disp,
                                       create_flags, NULL,
                                       fidp, fap,
                                       dur_handlep, context);
    }
    else {
        error = smb1fs_smb_ntcreatex(share, np, rights, shareMode, vt, fidp, 
                                     name, in_nmlen, disp, xattr, fap,
                                     do_create, context);
    }
    
    return error;
}

/*
 * This routine chains the open and read into one message. This routine is used 
 * only for reading data out of a stream. If we decided to use it for something 
 * else then we will need to make some changes.
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_smb_openread(struct smb_share *share, struct smbnode *np,
                   SMBFID *fidp, uint32_t desired_access,
                   uio_t uio, size_t *sizep, const char *stream_namep,
				   struct timespec *mtimep, vfs_context_t context)
{
    int error;
	size_t stream_name_len = strnlen(stream_namep,
                                     share->ss_maxfilenamelen + 1);
    uint32_t max_access;
    
    /* This is only used when dealing with xattr calls */
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        /* 
         * Do a Create/Read or Create/Read/Close. The Close is added if 
         * fidp is NULL
         */
        error = smb2fs_smb_cmpd_create_read(share, np,
                                            NULL, 0, 
                                            stream_namep, stream_name_len,
                                            desired_access, uio,
                                            sizep, &max_access,
                                            fidp, mtimep,
                                            context);
    }
    else {
        error = smb1fs_smb_openread(share, np, fidp, desired_access, uio, sizep,
                                    stream_namep, mtimep, context);
    }
    
    return error;
}

static int
smb2fs_smb_parse_ntcreatex(struct smb_share *share, struct smbnode *np,
                           struct smb2_create_rq *createp,
                           SMBFID *fidp, struct smbfattr *fap,
                           vfs_context_t context)
{
    /*
     * smb2fs_smb_ntcreatex() has various ways it can be called
     * (1) np - open item (np)
     * (2) np, namep - open the child (namep) in parent dir (np)
     * (3) np, strm_namep - open named stream (strm_namep) of item (np)
     * (4) np, namep, strm_namep - open named stream of (strm_namep) of 
     *    child (namep) in parent dir (np)
     *
     * If there is a namep, then do not update the vnode np as that will
     * be the parent vnode.
     */

    DBG_ASSERT(fap != NULL);	
    
    /* 
     * Copy results out into fap
     */
    fap->fa_created_disp = createp->ret_create_action;
    smb_time_NT2local(createp->ret_create_time, &fap->fa_crtime);
    smb_time_NT2local(createp->ret_access_time, &fap->fa_atime);
    smb_time_NT2local(createp->ret_write_time, &fap->fa_mtime);
    smb_time_NT2local(createp->ret_change_time, &fap->fa_chtime);
    
    /*
     * Because of the Steve/Conrad Symlinks we can never be completely
     * sure that we have the correct vnode type if its a file. For 
     * directories we always know the correct information.
     */
    fap->fa_attr = createp->ret_attributes;
    if (fap->fa_attr & SMB_EFA_DIRECTORY) {
        fap->fa_valid_mask |= FA_VTYPE_VALID;
    }
    fap->fa_vtype = (fap->fa_attr & SMB_EFA_DIRECTORY) ? VDIR : VREG;
    
    fap->fa_data_alloc = createp->ret_alloc_size;
    fap->fa_size = createp->ret_eof;
    
    if (createp->flags & SMB2_CREATE_GET_MAX_ACCESS) {
        fap->fa_max_access = createp->ret_max_access;
        fap->fa_valid_mask |= FA_MAX_ACCESS_VALID;
        
        /* Special case the root vnode */
        if (createp->namep == NULL) {
            /* Must be (1) or (3) and could be the root vnode */
            if ((np) && (np->n_vnode) && (vnode_isvroot(np->n_vnode))) {
                /*
                 * its the root folder, if no Execute, but share grants
                 * Execute then grant Execute to root folder
                 */
                if ((!(np->maxAccessRights & SMB2_FILE_EXECUTE)) &&
                    (share->maxAccessRights & SMB2_FILE_EXECUTE)) {
                    fap->fa_max_access |= SMB2_FILE_EXECUTE;
                }
                
                /*
                 * its the root, if no ReadAttr, but share grants
                 * ReadAttr then grant ReadAttr to root
                 */
                if ((!(np->maxAccessRights & SMB2_FILE_READ_ATTRIBUTES)) &&
                    (share->maxAccessRights & SMB2_FILE_READ_ATTRIBUTES)) {
                    fap->fa_max_access |= SMB2_FILE_READ_ATTRIBUTES;
                }
            }
        }
    }
    
	if (fidp) {
		*fidp = createp->ret_fid;
    }
    
    /*
     * If not a directory, check if node needs to be reopened,
     * if so, then don't update anything at this point.
     * See <rdar://problem/11366143>.
     */
    if ((np) && (np->n_vnode) && !(vnode_isdir(np->n_vnode))) {
        lck_mtx_lock(&np->f_openStateLock);
        if (np->f_openState & kInReopen) {
            lck_mtx_unlock(&np->f_openStateLock);
            goto done;
        }
        lck_mtx_unlock(&np->f_openStateLock);
    }

    if (createp->flags & SMB2_CREATE_IS_NAMED_STREAM) {
		/* 
         * Must be (3) or (4). 
         * If an EA or Stream then we are done. We dont update that main
         * data vnode with information from a stream. Possibly we could since
         * the data should be valid...
         */
		goto done;
	}
	
    if ((np) && (createp->flags & SMB2_CREATE_DO_CREATE)) {
        if (!(SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS)) {
            /*
             * File server does not support File IDs
             */
            
            /*
             * Must be (2). We are creating the item so create the ino number.
             * Since its a create, it must be the data fork and thus
             * createp->name_len is the same as the orig in_nmlen.
             */
            
            /* This is a create so better have a name, but root does not have a name */
            fap->fa_ino = smbfs_getino(np, createp->namep, createp->name_len);
            goto done;			
        }
    }

    if (createp->namep) {
        /* 
         * SMB2_CREATE_DO_CREATE is not set, but still have a name, so 
         * must be (2)
         */
        goto done;
    }
    
	/* If this is a SYMLINK, then n_vnode could be set to NULL */
	if ((np) && (np->n_vnode == NULL)) {
		goto done;
	}
    
	/* 
 	 * At this point, it must be (1).
     * We have all the meta data attributes so update the cache. If the 
	 * calling routine is setting an attribute it should not change the 
	 * smb node value until after the open has completed. NOTE: The old 
	 * code would only update the cache if the mtime, attributes and size 
	 * haven't changed.
 	 */
    if (np) {
        smbfs_attr_cacheenter(share, np->n_vnode, fap, TRUE, context);
    }

done:
	return 0;
}

static int
smb2fs_smb_qfsattr(struct smbmount *smp,
                   struct FILE_FS_ATTRIBUTE_INFORMATION *fs_attrs,
                   vfs_context_t context)
{
    struct smb_share *share = smp->sm_share;
	int error;
    uint32_t output_buffer_len;
    
    output_buffer_len = sizeof(struct FILE_FS_ATTRIBUTE_INFORMATION);
    output_buffer_len += PATH_MAX;

    /*
     * Do the Compound Create/Query Info/Close call
     * Query results are passed back in *fs_attrs
     *
     * Have to pass in submount path if it exists because we have no root vnode,
     * and smbmount and smb_share may not be fully set up yet.
     * 
     * This is always done on the root of the share
     */
    error = smb2fs_smb_cmpd_query(share, NULL, VDIR,
                                  (smp->sm_args.network_path_len == 0 ? NULL : smp->sm_args.network_path),
                                  smp->sm_args.network_path_len,
                                  0, SMB2_FILE_READ_ATTRIBUTES | SMB2_SYNCHRONIZE,
                                  SMB2_0_INFO_FILESYSTEM, FileFsAttributeInformation,
                                  0, NULL,
                                  &output_buffer_len, (uint8_t *) fs_attrs,
                                  context);
   
	return error;
}

/* 
 * Since the first thing we do is set the default values there is no longer 
 * any reason to return an error for this routine. Some servers may not support
 * this call. We should not fail the mount just because they do not support this
 * call. 
 *
 * The calling routine must hold a reference on the share
 * 
 */
void 
smbfs_smb_qfsattr(struct smbmount *smp, vfs_context_t context)
{
    struct smb_share *share = smp->sm_share;
   	struct smb_session *sessionp = SS_TO_SESSION(share);
    struct FILE_FS_ATTRIBUTE_INFORMATION fs_attrs;
	int error;
	size_t fs_nmlen;	/* The sized malloced for fs_name */
	char *fsname = NULL;
    
	/* Start with the default values */
	share->ss_fstype = SMB_FS_FAT;	/* default to FAT File System */
	share->ss_attributes = 0;
	share->ss_maxfilenamelen = 255;
    
    bzero(&fs_attrs, sizeof(fs_attrs));
    
    if (sessionp->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_qfsattr(smp, &fs_attrs, context);
    }
    else {
        /* 
         * Goal is to leave the SMB 1 code unchanged as much as possible
         * to minimize the risk.  That is why there is duplicate code here
         * for the SMB 2/3 path and also in SMB 1 code path.
         */
        smb1fs_qfsattr(share, context);
        return;
    }
    
    if (error) {
		/* This is a very bad server */
		SMBWARNING("Server returned a bad SMB_QFS_ATTRIBUTE_INFO message\n");
		/* Don't believe them when they say they are unix */
		SS_TO_SESSION(share)->session_sopt.sv_caps &= ~SMB_CAP_UNIX;
        return;
    }
    
    share->ss_attributes = fs_attrs.file_system_attrs;
    share->ss_maxfilenamelen = fs_attrs.max_component_name_len;
    
    if (!SMB_UNICODE_STRINGS(SS_TO_SESSION(share))) {
        SMBERROR("Unicode must be supported\n");
        goto done;
    }
    
    /* If have a file system name, determine what it is */
	if (fs_attrs.file_system_namep != NULL) {
        /* Convert Unicode string */
        fs_nmlen = fs_attrs.file_system_name_len;
        fsname = smbfs_ntwrkname_tolocal(fs_attrs.file_system_namep, 
                                         &fs_nmlen, 
                                         SMB_UNICODE_STRINGS(SS_TO_SESSION(share)));
        if (fsname == NULL) {
            SMBERROR("fs_attr name failed to convert\n");
            goto done;	/* Should never happen, but just to be safe */
        }
		
		fs_nmlen += 1; /* Include the null byte for the compare */
        
		/*
		 * Let's start keeping track of the file system type. Most
		 * things we need to do differently really depend on the
		 * file system type. As an example we know that FAT file systems
		 * do not update the modify time on directories.
		 */
		if (strncmp(fsname, "FAT", fs_nmlen) == 0)
			share->ss_fstype = SMB_FS_FAT;
		else if (strncmp(fsname, "FAT12", fs_nmlen) == 0)
			share->ss_fstype = SMB_FS_FAT;
		else if (strncmp(fsname, "FAT16", fs_nmlen) == 0)
			share->ss_fstype = SMB_FS_FAT;
		else if (strncmp(fsname, "FAT32", fs_nmlen) == 0)
			share->ss_fstype = SMB_FS_FAT;
		else if (strncmp(fsname, "CDFS", fs_nmlen) == 0)
			share->ss_fstype = SMB_FS_CDFS;
		else if (strncmp(fsname, "UDF", fs_nmlen) == 0)
			share->ss_fstype = SMB_FS_UDF;
        else if (strncmp(fsname, "NTFS", fs_nmlen) == 0)
            share->ss_fstype = SMB_FS_NTFS_UNKNOWN;	/* Could be lying */
		else if (strncmp(fsname, "ReFS", fs_nmlen) == 0)
			share->ss_fstype = SMB_FS_NTFS_UNKNOWN;	/* ReFS is a NTFS variant */
		else if (strncmp(fsname, "MAFS", fs_nmlen) == 0)
			share->ss_fstype = SMB_FS_NTFS_UNKNOWN;	/* MAFS is a NTFS variant */
		
		SMBWARNING("%s/%s type '%s', attr 0x%x, maxfilename %d, ss_fstype %d\n",
				   SS_TO_SESSION(share)->session_srvname, share->ss_name, fsname, 
				   share->ss_attributes, share->ss_maxfilenamelen,
				   share->ss_fstype);
        
        /*
		 * NT4 will not return the FILE_NAMED_STREAMS bit in the ss_attributes
		 * even though they support streams. So if its a NT4 server and a
		 * NTFS file format then turn on the streams flag.
		 */
        if ((SS_TO_SESSION(share)->session_flags & SMBV_NT4) && 
            (share->ss_fstype & SMB_FS_NTFS_UNKNOWN)) {
            share->ss_attributes |= FILE_NAMED_STREAMS;
        }
        /* 
         * The server says they support streams and they say they are NTFS. So mark
         * the subtype as NTFS. Remember a lot of non Windows servers pretend
         * their NTFS so they can support ACLs, but they aren't really because they have
         * no stream support. This allows us to tell the difference.
         */
        if ((share->ss_fstype == SMB_FS_NTFS_UNKNOWN) && 
            (share->ss_attributes & FILE_NAMED_STREAMS)) {
            share->ss_fstype = SMB_FS_NTFS;	/* Real NTFS Volume */
        }
        else if ((share->ss_fstype == SMB_FS_NTFS_UNKNOWN) && 
                 (UNIX_SERVER(SS_TO_SESSION(share)))) {
            share->ss_fstype = SMB_FS_NTFS_UNIX;	
            /* UNIX system lying about being NTFS */
        }
        
        /*
         * Note: If this is an OS X SMB 2/3 server, smbfs_mount() will
         * set share->ss_fstype() to SMB_FS_MAC_OS_X.  We cannot do
         * that here because at this point we haven't yet queried the
         * server for AAPL create context support.
         */
    }
    
done: 
    if (fs_attrs.file_system_namep != NULL) {
        SMB_FREE(fs_attrs.file_system_namep, M_SMBFSDATA);
    }
    if (fsname != NULL) {
        SMB_FREE(fsname, M_SMBSTR);
    }
}

static int
smb2fs_smb_qpathinfo(struct smb_share *share, struct smbnode *np, enum vtype vnode_type,
                     struct smbfattr *fap, short infolevel, 
                     const char **namep, size_t *name_lenp, 
                     vfs_context_t context)
{
    struct FILE_ALL_INFORMATION *all_infop = NULL;
	int error = EINVAL;
	const char *name = (namep ? *namep : NULL);
	size_t name_len = (name_lenp ? *name_lenp : 0);
    uint32_t output_buffer_len;
    
    SMB_MALLOC(all_infop, 
               struct FILE_ALL_INFORMATION *, 
               sizeof(struct FILE_ALL_INFORMATION), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (all_infop == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    /* 
     * Set up for the Query Info call
     */
    switch (infolevel) {
        case SMB_QFILEINFO_ALL_INFO:
            all_infop->share = share;
            all_infop->np = np;
            all_infop->fap = fap;
            all_infop->namep = namep;
            all_infop->name_lenp = name_lenp;
            break;
            
        default:
            SMBERROR("Unimplemented infolevel %d\n", infolevel);
            goto bad;
            break;
    }

    output_buffer_len = SMB2_FILE_ALL_INFO_LEN;
    
    /*
     * Do the Compound Create/SetInfo/Close call
     * Query results are passed back in *fap
     */
    error = smb2fs_smb_cmpd_query(share, np, vnode_type,
                                  name, name_len,
                                  0, SMB2_FILE_READ_ATTRIBUTES | SMB2_SYNCHRONIZE,
                                  SMB2_0_INFO_FILE, FileAllInformation,
                                  0, NULL,
                                  &output_buffer_len, (uint8_t *) all_infop,
                                  context);

bad:
    if (all_infop != NULL) {
        SMB_FREE(all_infop, M_SMBTEMP);
    }
    
	return error;
}

/*
 * The calling routine must hold a reference on the share
 */
int
smbfs_smb_qpathinfo(struct smb_share *share, struct smbnode *np, enum vtype vnode_type,
                    struct smbfattr *fap, short infolevel,
                    const char **namep, size_t *nmlenp,
                    vfs_context_t context)
{
   	struct smb_session *sessionp = SS_TO_SESSION(share);
	int error;
    
    if (sessionp->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_qpathinfo(share, np, vnode_type,
                                     fap, infolevel,
                                     namep, nmlenp,
                                     context);
    }
    else {
        error = smb1fs_smb_qpathinfo(share,
                                     np,
                                     fap,
                                     infolevel,
                                     namep,
                                     nmlenp,
                                     context);
    }
    
    return error;
}

/*
 * When calling this routine be very careful when passing the arguments. 
 * Depending on the arguments different actions will be taken with this routine. 
 *
 * The calling routine must hold a reference on the share
 *
 */
static int
smb2fs_smb_qstreaminfo(struct smb_share *share, struct smbnode *np, enum vtype vnode_type,
                       const char *namep, size_t name_len,
                       const char *stream_namep,
                       uio_t uio, size_t *sizep,
                       uint64_t *stream_sizep, uint64_t *stream_alloc_sizep,
                       uint32_t *stream_flagsp, uint32_t *max_accessp,
                       vfs_context_t context)
{
    /* 
     * For this function, vnode_type is the item's type. The item will be 
     * opened and checked for named streams.
     */

    /* 
     * Two usage cases:
     * 1) Given np and namep == NULL. Query the path of np
     * 2) Given np and namep (must be readdirattr). Query PARENT np and 
     * child namep. In this case, do not update vnode np as that is the parent.
     * 
     * In both cases, Query for a list of streams and if streams are found, 
     * see if they match stream_namep that was passed in.
     */
    struct FILE_STREAM_INFORMATION *stream_infop = NULL;
	int error;
    uint32_t output_buffer_len;
    
    if (namep == NULL) {
        /* Not readdirattr case */
        if ((np->n_fstatus & kNO_SUBSTREAMS) ||
            (np->n_dosattr & SMB_EFA_REPARSE_POINT)) {
            error = ENOATTR;
            goto bad;
        }
    }
	
	if (sizep) {
		*sizep = 0;
    }
    
    SMB_MALLOC(stream_infop, 
               struct FILE_STREAM_INFORMATION *, 
               sizeof(struct FILE_STREAM_INFORMATION), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (stream_infop == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    /* 
     * Set up for the Query Info call
     *
     * We have two typical usages for this call
     * 1) listxattr - uio and/or stream_buf_sizep 
     *  a) if uio and stream_buf_sizep, then return all xattrs If stream_flagsp has
     *     SMB_NO_TRANSLATE_NAMES set, then AFP_Resource & AFP_AfpInfo are not translated
     *     to extended attribute names.
     *  b) if only stream_buf_sizep, then they are just trying to determine size
     *     of a buffer sufficiently large enough to hold all the xattr names
     * 2) Checking for existence of a specific stream
     *  a) If stream_namep and stream_sizep/stream_alloc_sizep, then look for 
     *     that specific stream and if found, return its logical and alloc sizes
     *  b) If only stream_namep, just check for existence of that specific stream
     */
    stream_infop->share = share;
    stream_infop->np = np;
    stream_infop->namep = namep;
    stream_infop->name_len = name_len;
    stream_infop->uio = uio;
    stream_infop->stream_buf_sizep = sizep;
    stream_infop->stream_namep = stream_namep;
    stream_infop->stream_sizep = stream_sizep;
    stream_infop->stream_alloc_sizep = stream_alloc_sizep;
    stream_infop->stream_flagsp = stream_flagsp;

    /*
     * Some servers do not grant us that many credits, so limit credit
     * usage to just 1. That should be plenty for getting
     * FileStreamInformation which is just a list of very short names like
     * ::$DATA
     */
    output_buffer_len = 64 * 1024;
    
    /*
     * Do the Compound Create/QueryInfo/Close call
     * Query results are passed back in uio
     */
    error = smb2fs_smb_cmpd_query(share, np, vnode_type,
                                  namep, name_len,
                                  0, SMB2_FILE_READ_ATTRIBUTES | SMB2_SYNCHRONIZE,
                                  SMB2_0_INFO_FILE, FileStreamInformation,
                                  0, max_accessp,
                                  &output_buffer_len, (uint8_t *) stream_infop,
                                  context);
    
    if (error) {
        if ((error != ENOATTR) && (error != EINVAL) &&
            (error != EACCES) && (error != ENOENT)) {
            SMBDEBUG("smb2fs_smb_cmpd_query failed %d\n", error);
        }
        goto bad;
    }

    /* Is there only the data stream and no other named streams? */
    if ((namep == NULL) && (stream_flagsp != NULL)) {
        /* Not readdirattr case */
        if (*stream_flagsp & SMB_NO_SUBSTREAMS) {
            np->n_fstatus |= kNO_SUBSTREAMS;
        }
    }

bad:
    if (stream_infop != NULL) {
        SMB_FREE(stream_infop, M_SMBTEMP);
    }
    
	return error;
}

/*
 * When calling this routine be very careful when passing the arguments. 
 * Depending on the arguments different actions will be taken with this routine. 
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_smb_qstreaminfo(struct smb_share *share, struct smbnode *np, enum vtype vnode_type,
                      const char *namep, size_t name_len,
                      const char *stream_namep,
                      uio_t uio, size_t *sizep,
                      uint64_t *strm_sizep, uint64_t *strm_alloc_sizep,
                      uint32_t *stream_flags, uint32_t *max_accessp,
                      vfs_context_t context)
{
   	struct smb_session *sessionp = SS_TO_SESSION(share);
	int error;
    
    if (sessionp->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_qstreaminfo(share, np, vnode_type,
                                       namep, name_len,
                                       stream_namep,
                                       uio, sizep,
                                       strm_sizep, strm_alloc_sizep,
                                       stream_flags, max_accessp,
                                       context);
    }
    else {
        error = smb1fs_smb_qstreaminfo(share, np,
                                       namep, name_len,
                                       stream_namep, 
                                       uio, sizep,
                                       strm_sizep, strm_alloc_sizep,
                                       stream_flags,
                                       context);
    }
    
    return error;
    
}

/*
 * We should replace it with something more modern. See <rdar://problem/7595213>. 
 * This routine is only used to test the existence of an item or to get its 
 * DOS attributes when changing the status of the HIDDEN bit.
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_smb_query_info(struct smb_share *share, struct smbnode *dnp, enum vtype vnode_type,
                     const char *in_name, size_t len, uint32_t *attr, 
                     vfs_context_t context)
{
   	struct smb_session *sessionp = SS_TO_SESSION(share);
	const char *name = in_name;
	size_t name_len = len;
    struct smbfattr *fap = NULL;
	int error;
    
    if (sessionp->session_flags & SMBV_SMB2) {
        SMB_MALLOC(fap, 
                   struct smbfattr *, 
                   sizeof(struct smbfattr), 
                   M_SMBTEMP, 
                   M_WAITOK | M_ZERO);
        if (fap == NULL) {
            SMBERROR("SMB_MALLOC failed\n");
            error = ENOMEM;
            goto out;
        }
        
        if (sessionp->session_misc_flags & SMBV_NO_QUERYINFO) {
            /* Use Query Dir instead of Query Info, but only if SMB 2/3 */
            error = smb2fs_smb_cmpd_query_dir_one(share, dnp,
                                                  name, name_len,
                                                  fap, (char **) &name, &name_len,
                                                  context);
        }
        else {
            /* This is always a parent dir dnp and a name in that dir */
            error = smb2fs_smb_qpathinfo(share, dnp, vnode_type,
                                         fap, SMB_QFILEINFO_ALL_INFO,
                                         &name, &name_len,
                                         context);
        }
        
        if (error) {
            goto out;
        }
        
        /* return attributes if they want them */
        if (attr != NULL) {
            *attr = fap->fa_attr;
        }
        
        /* if got returned a new name, free it since we do not need it */
        if (name != in_name) {
            SMB_FREE(name, M_SMBNODENAME);
        }
    }
    else {
        error = smb1fs_smb_query_info(share,
                                      dnp,
                                      name,
                                      len,
                                      attr,
                                      context);
    }
    
out:
    if (fap != NULL) {
        SMB_FREE(fap, M_SMBTEMP);
    }
    
    return error;
}

static int 
smb2fs_smb_rename(struct smb_share *share, struct smbnode *src,
                  struct smbnode *tdnp, const char *tnamep, size_t tname_len,
                  vfs_context_t context)
{
    int error;
    uint32_t setinfo_ntstatus;
    struct smb2_set_info_file_rename_info *renamep = NULL;
    enum vtype vnode_type = VREG;

    /*
     * Looking at Win <-> Win with SMB 2/3, rename/move is handled by opening the 
     * file with Delete and "Read Attributes", then a Set Info is done to move
     * or rename the file.  Finally a Close is sent.
     */
    
    SMB_MALLOC(renamep, 
               struct smb2_set_info_file_rename_info *, 
               sizeof(struct smb2_set_info_file_rename_info), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (renamep == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    /* 
     * Set up for the Set Info call
     */
    renamep->replace_if_exists = 0;
    renamep->tname_len = (uint32_t) tname_len;
    renamep->tdnp = tdnp;
    renamep->tnamep = (char*) tnamep;
    
    if ((src) && (src->n_vnode)) {
        /* Use vnode to determine type */
        vnode_type = vnode_isdir(src->n_vnode) ? VDIR : VREG;
    }

    /*
     * Do the Compound Create/SetInfo/Close call
     * Delete access will allow us to rename an open file
     */
    error = smb2fs_smb_cmpd_set_info(share, src, vnode_type,
                                     NULL, 0,
                                     0, SMB2_FILE_READ_ATTRIBUTES | SMB2_STD_ACCESS_DELETE | SMB2_SYNCHRONIZE,
                                     SMB2_0_INFO_FILE, FileRenameInformation,
                                     0,
                                     sizeof (*renamep), (uint8_t *) renamep,
                                     &setinfo_ntstatus,
                                     context);
    if (error) {
        if (error != EACCES) {
            SMBDEBUG("smb2fs_smb_cmpd_set_info failed %d\n", error);
        }
        goto bad;
    }

bad:
    if (renamep != NULL) {
        SMB_FREE(renamep, M_SMBTEMP);
    }
    
    return error;
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smbfs_smb_rename(struct smb_share *share, struct smbnode *src, 
				 struct smbnode *tdnp, const char *tname, size_t tnmlen, 
				 vfs_context_t context)
{
    int error;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_rename(share, src, tdnp, tname, tnmlen, context);
    }
    else {
        error = smb1fs_smb_rename(share, src, tdnp, tname, tnmlen, context);
    }
    
    return error;
}

int
smbfs_smb_reparse_read_symlink(struct smb_share *share, struct smbnode *np,
                               struct uio *uiop, vfs_context_t context)
{
    int error;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_cmpd_reparse_point_get(share, np,
												  NULL, 0,
												  uiop, NULL,
												  context);
    }
    else {
        error = smb1fs_smb_reparse_read_symlink(share, np, uiop, context);
    }
    
    return error;

}

static int
smb2fs_smb_request_resume_key(struct smb_share *share, SMBFID fid, u_char *resume_key,
                              vfs_context_t context)
{
    struct smb2_ioctl_rq *ioctlp = NULL;
    int error = 0;
    
    /*
     * Build the IOCTL request
     */
    SMB_MALLOC(ioctlp,
               struct smb2_ioctl_rq *,
               sizeof(struct smb2_ioctl_rq),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (ioctlp == NULL) {
		SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    ioctlp->share = share;
    ioctlp->ctl_code = FSCTL_SRV_REQUEST_RESUME_KEY;
    ioctlp->fid = fid;
    ioctlp->mc_flags = 0;

	ioctlp->snd_input_len = 0;
	ioctlp->snd_output_len = 0;
	ioctlp->rcv_input_len = 0;
	ioctlp->rcv_output_len = 0x20;
    
    error = smb2_smb_ioctl(share, NULL, ioctlp, NULL, context);
    
    if (!error) {
        if (ioctlp->rcv_output_len < SMB2_RESUME_KEY_LEN) {
            SMBERROR("Resume key too short %d \n", ioctlp->rcv_output_len);
            error = EBADRPC;
        }
        else {
            memcpy(resume_key, ioctlp->rcv_output_buffer, SMB2_RESUME_KEY_LEN);
        }
        
        SMB_FREE(ioctlp->rcv_output_buffer, M_TEMP);
    }
    
bad:
    if (ioctlp != NULL) {
        SMB_FREE(ioctlp, M_SMBTEMP);
    }

    return (error);
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smbfs_smb_rmdir(struct smb_share *share, struct smbnode *np, 
                vfs_context_t context)
{
    int error;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_delete (share, np, VDIR,
                                   NULL, 0,
                                   0, context);
    }
    else {
        error = smb1fs_smb_rmdir (share, np, context);
    }
    
    return error;
}

int
smb2fs_smb_security_get(struct smb_share *share, struct smbnode *np,
                        uint32_t desired_access, uint32_t security_attrs,
                        struct ntsecdesc **resp, uint32_t *resp_len,
                        vfs_context_t context)
{
	int error;
    enum vtype vnode_type = VREG;
    
    /* Try to determine the vnode type for the Create Request */

    if ((np) && (np->n_vnode)) {
        /* Use vnode to determine type */
        vnode_type = vnode_isdir(np->n_vnode) ? VDIR : VREG;
    }

    /*
     * Do the Compound Create/QueryInfo/Close call
     */
    error = smb2fs_smb_cmpd_query(share, np, vnode_type,
                                  NULL, 0,
                                  0, desired_access,
                                  SMB2_0_INFO_SECURITY, 0,
                                  security_attrs, NULL,
                                  resp_len, (uint8_t *) resp,
                                  context);
    if (error) {
        SMBDEBUG("smb2fs_smb_cmpd_query failed %d\n", error);
    }
    
    return error;
}

int
smb2fs_smb_security_set(struct smb_share *share, struct smbnode *np,
                        uint32_t desired_access,
                        uint32_t security_attrs, uint16_t control_flags,
                        struct ntsid *owner, struct ntsid *group,
                        struct ntacl *sacl, struct ntacl *dacl,
                        vfs_context_t context)
{
	int error;
    uint32_t setinfo_ntstatus;
    struct smb2_set_info_security sec_info;
    enum vtype vnode_type = VREG;

    /*
     * Set up for the Set Info call
     */
    sec_info.security_attrs = security_attrs;
    sec_info.control_flags = control_flags;
    sec_info.owner = owner;
    sec_info.group = group;
    sec_info.sacl = sacl;
    sec_info.dacl = dacl;
    
    if ((np) && (np->n_vnode)) {
        /* Use vnode to determine type */
        vnode_type = vnode_isdir(np->n_vnode) ? VDIR : VREG;
    }

    /*
     * Do the Compound Create/SetInfo/Close call
     */
    error = smb2fs_smb_cmpd_set_info(share, np, vnode_type,
                                     NULL, 0,
                                     0, desired_access,
                                     SMB2_0_INFO_SECURITY, 0,
                                     security_attrs,
                                     sizeof (sec_info), (uint8_t *) &sec_info,
                                     &setinfo_ntstatus,
                                     context);
    if (error) {
        SMBDEBUG("smb2fs_smb_cmpd_set_info failed %d, ntstatus 0x%x\n",
                 error, setinfo_ntstatus);
        
        if (setinfo_ntstatus == STATUS_INVALID_SID) {
            /*
             * If the server returns STATUS_INVALID_SID, then just pretend
             * that we set the security info even though it "failed".
             * See <rdar://problem/10852453>.
             */
            error = 0;
        }
    }

    return error;
}

int
smbfs_smb_setsec(struct smb_share *share, struct smbnode *np,
                 uint32_t desired_access, SMBFID fid,
                 uint32_t selector, uint16_t control_flags,
                 struct ntsid *owner, struct ntsid *group,
                 struct ntacl *sacl, struct ntacl *dacl,
                 vfs_context_t context)
{
	int error;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_security_set(share, np, desired_access,
                                        selector, control_flags,
                                        owner, group,
                                        sacl, dacl,
                                        context);
    }
    else {
        error = smb1fs_setsec(share, fid, selector, control_flags,
                            owner, group, sacl, dacl, context);
    }
    
    return (error);
}

static int
smb2fs_smb_set_allocation(struct smb_share *share, SMBFID fid, 
                          uint64_t new_size, vfs_context_t context)
{
    int error;
    struct smb2_set_info_rq *infop = NULL;
    
    SMB_MALLOC(infop, 
               struct smb2_set_info_rq *, 
               sizeof(struct smb2_set_info_rq), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (infop == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    /* 
     * Set up for the Set Info call
     */
    infop->info_type = SMB2_0_INFO_FILE;
    infop->file_info_class = FileAllocationInformation;
    infop->add_info = 0;
    infop->fid = fid;
    infop->input_buffer = (uint8_t *) &new_size;
    infop->mc_flags = 0;
    
    error = smb2_smb_set_info(share, infop, NULL, NULL, context);
    if (error) {
        SMBDEBUG("smb2_smb_set_info failed %d ntstatus %d\n",
                 error,
                 infop->ret_ntstatus);
        goto bad;
    }
    
bad:    
    if (infop != NULL) {
        SMB_FREE(infop, M_SMBTEMP);
    }
    
    return error;
}

/*
 * This routine will send an allocation across the wire to the server. 
 *
 * The calling routine must hold a reference on the share
 *
 */ 
int
smbfs_smb_set_allocation(struct smb_share *share, SMBFID fid, uint64_t new_size,
                         vfs_context_t context)
{
    int error;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_set_allocation(share, fid, new_size, context);
    }
    else {
        error = smb1fs_set_allocation(share, fid, new_size, context);
    }
    return error;
}


static int
smb2fs_smb_set_eof(struct smb_share *share, SMBFID fid, uint64_t newsize,
                  vfs_context_t context)
{
    int error;
    struct smb2_set_info_rq *infop = NULL;
    
    SMB_MALLOC(infop, 
               struct smb2_set_info_rq *, 
               sizeof(struct smb2_set_info_rq), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (infop == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    /* 
     * Set up for the Set Info call
     */
    infop->info_type = SMB2_0_INFO_FILE;
    infop->file_info_class = FileEndOfFileInformation;
    infop->add_info = 0;
    infop->fid = fid;
    infop->input_buffer = (uint8_t *) &newsize;
    infop->mc_flags = 0;
    
    /*
     * Do the Set Info call
     */
    error = smb2_smb_set_info(share, infop, NULL, NULL, context);
    if (error) {
        SMBDEBUG("smb2_smb_set_info failed %d ntstatus %d\n",
                 error,
                 infop->ret_ntstatus);
        goto bad;
    }
    
bad:
    if (infop != NULL) {
        SMB_FREE(infop, M_SMBTEMP);
    }
    
	return error;
}

/*
 * This routine will send a seteof across the wire to the server. 
 *
 * The calling routine must hold a reference on the share
 *
 */ 
int 
smbfs_smb_seteof(struct smb_share *share, SMBFID fid, uint64_t newsize, 
                 vfs_context_t context)
{
	int error;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_set_eof(share, fid, newsize, context);
    }
    else {
        error = smb1fs_seteof(share, fid, newsize, context);
    }
    return error;
}

static int
smb2fs_smb_set_file_basic_info(struct smb_share *share,
                               struct smb2_set_info_file_basic_info **infopp,
                               struct timespec *crtime, struct timespec *atime,
                               struct timespec *mtime, uint32_t file_attrs)
{
    uint64_t tm;
    
    SMB_MALLOC(*infopp, 
               struct smb2_set_info_file_basic_info *, 
               sizeof(struct smb2_set_info_file_basic_info), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (*infopp == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        return (ENOMEM);
    }
    
    /* 
     * Set Creation time
     */
	tm = 0;
	if (crtime) {
		smb_time_local2NT(crtime, &tm, (share->ss_fstype == SMB_FS_FAT));
        (*infopp)->create_time = tm;
	}
    
    /* Set last access time */	
	tm = 0;
	if (atime) {
		smb_time_local2NT(atime, &tm, (share->ss_fstype == SMB_FS_FAT));
        (*infopp)->access_time = tm;
	}
    
	/* Set last write time */		
	tm = 0;
	if (mtime) {
		smb_time_local2NT(mtime, &tm, (share->ss_fstype == SMB_FS_FAT));
        (*infopp)->write_time = tm;
	}
    
    /* We never allow anyone to set the change time */		
    
	/* set file attributes */		
    (*infopp)->attributes = file_attrs;
    
    return (0);
}

static int
smb2fs_smb_setfattrNT(struct smb_share *share, uint32_t attr, SMBFID fid,
                      struct timespec *crtime, struct timespec *mtime,
                      struct timespec *atime, vfs_context_t context)
{
    int error;
    struct smb2_set_info_rq *infop = NULL;
    struct smb2_set_info_file_basic_info *basic_infop = NULL;
    
    /* Allocate and fill out the file basic info */
    error = smb2fs_smb_set_file_basic_info(share, &basic_infop,
                                           crtime, atime, mtime, attr);
    if (error) {
        SMBDEBUG("smb2fs_smb_set_file_basic_info failed %d\n", error);
        goto bad;
    }
    
    SMB_MALLOC(infop, 
               struct smb2_set_info_rq *, 
               sizeof(struct smb2_set_info_rq), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (infop == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    /* 
     * Set up for the Set Info call
     */
    infop->info_type = SMB2_0_INFO_FILE;
    infop->file_info_class = FileBasicInformation;
    infop->add_info = 0;
    infop->fid = fid;
    infop->input_buffer = (uint8_t *) basic_infop;
    infop->mc_flags = 0;

    /*
     * Do the Set Info call
     */
    error = smb2_smb_set_info(share, infop, NULL, NULL, context);
    if (error) {
        SMBDEBUG("smb2_smb_set_info failed %d ntstatus %d\n",
                 error,
                 infop->ret_ntstatus);
        goto bad;
    }
    
bad:
    if (infop != NULL) {
        SMB_FREE(infop, M_SMBTEMP);
    }
    
    if (basic_infop != NULL) {
        SMB_FREE(basic_infop, M_SMBTEMP);
    }
    
    return error;
}

/*
 * Same as smbfs_smb_setpattrNT except with a file handle. Note once we remove 
 * Windows 98 support we can remove passing the node into this routine.
 *
 * The calling routine must hold a reference on the share
 *
 */
int
smbfs_smb_setfattrNT(struct smb_share *share, uint32_t attr, SMBFID fid, 
                     struct timespec *crtime, struct timespec *mtime, 
					 struct timespec *atime, vfs_context_t context)
{
    int error;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_setfattrNT(share, attr, fid, crtime, mtime, atime,
                                      context);
    }
    else {
        error = smb1fs_smb_setfattrNT(share, attr, fid, crtime, mtime, atime,
                                      context);
    }
    return error;
}

/*
 * Set DOS file attributes, may want to replace with a more modern call
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_smb_setpattr(struct smb_share *share, struct smbnode *np, enum vtype vnode_type,
                   const char *namep, size_t name_len,
                   uint16_t attr, vfs_context_t context)
{
    int error;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_setpattrNT(share, np, vnode_type,
                                      namep, name_len,
                                      attr, NULL,
                                      NULL, NULL,
                                      context);
    }
    else {
        error = smb1fs_smb_setpattr(share, np,
                                    namep, name_len,
                                    attr, context);
    }
    return error;
}

int
smb2fs_smb_setpattrNT(struct smb_share *share, struct smbnode *np, enum vtype vnode_type,
                      const char *namep, size_t name_len,
                      uint32_t attr, struct timespec *crtime, 
                      struct timespec *mtime, struct timespec *atime, 
                      vfs_context_t context)
{
    int error;
    uint32_t setinfo_ntstatus;
    struct smb2_set_info_file_basic_info *basic_infop = NULL;
    
    /* Allocate and fill out the file basic info */
    error = smb2fs_smb_set_file_basic_info(share, &basic_infop,
                                           crtime, atime, mtime, attr);
    if (error) {
        SMBDEBUG("smb2fs_smb_set_file_basic_info failed %d\n", error);
        goto bad;
    }
    
    /*
     * Do the Compound Create/SetInfo/Close call
     */
    error = smb2fs_smb_cmpd_set_info(share, np, vnode_type,
                                     namep, name_len,
                                     0, SMB2_FILE_WRITE_ATTRIBUTES | SMB2_SYNCHRONIZE,
                                     SMB2_0_INFO_FILE, FileBasicInformation,
                                     0,
                                     sizeof (*basic_infop), (uint8_t *) basic_infop,
                                     &setinfo_ntstatus,
                                     context);
    if (error) {
        SMBDEBUG("smb2fs_smb_cmpd_set_info failed %d\n", error);
        goto bad;
    }

bad:    
    if (basic_infop != NULL) {
        SMB_FREE(basic_infop, M_SMBTEMP);
    }

    return error;
}

/*
 * BASIC_INFO works with Samba, but Win2K servers say it is an invalid information
 * level on a SET_PATH_INFO.  Note Win2K does support *BASIC_INFO on a SET_FILE_INFO, 
 * and they support the equivalent *BASIC_INFORMATION on SET_PATH_INFO. Go figure.
 *
 * The calling routine must hold a reference on the share
 *
 */
int
smbfs_smb_setpattrNT(struct smb_share *share, struct smbnode *np, enum vtype vnode_type,
                     const char *namep, size_t name_len,
                     uint32_t attr, struct timespec *crtime,
                     struct timespec *mtime, struct timespec *atime,
                     vfs_context_t context)
{
    int error;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_setpattrNT(share, np, vnode_type,
                                      namep, name_len,
                                      attr, crtime,
                                      mtime, atime,
                                      context);
    }
    else {
        error = smb1fs_smb_setpattrNT(share, np, attr, crtime, mtime, atime,
                                      context);
    }
    return error;
}

static int
smb2fs_smb_statfs(struct smbmount *smp,
                  struct FILE_FS_SIZE_INFORMATION *fs_size,
                  vfs_context_t context)
{
    struct smb_share *share = smp->sm_share;
	int error;
    uint32_t output_buffer_len;
    
    output_buffer_len = sizeof(struct FILE_FS_SIZE_INFORMATION);
    
    /*
     * Do the Compound Create/GetInfo/Close call
     * Query results are passed back in *fs_size
     *
     * Have to pass in submount path if it exists because we have no root vnode,
     * and smbmount and smb_share may not be fully set up yet.
     *
     * This is always done on the root of the share.
     */
    error = smb2fs_smb_cmpd_query(share, NULL, VDIR,
                                  (smp->sm_args.network_path_len == 0 ? NULL : smp->sm_args.network_path),
                                  smp->sm_args.network_path_len,
                                  0, SMB2_FILE_READ_ATTRIBUTES | SMB2_SYNCHRONIZE,
                                  SMB2_0_INFO_FILESYSTEM, FileFsSizeInformation,
                                  0, NULL,
                                  &output_buffer_len, (uint8_t *) fs_size,
                                  context);
    
	return error;
}

/*
 * The calling routine must hold a reference on the share
 */
int
smbfs_smb_statfs(struct smbmount *smp, struct vfsstatfs *sbp,
                 vfs_context_t context)
{
    struct smb_share *share = smp->sm_share;
   	struct smb_session *sessionp = SS_TO_SESSION(share);
    struct FILE_FS_SIZE_INFORMATION fs_size;
	int error;
	uint64_t s, t, f;
	size_t xmax;

    bzero(&fs_size, sizeof(fs_size));
    
    if (sessionp->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_statfs(smp, &fs_size, context);
    }
    else {
        /* 
         * Goal is to leave the SMB 1 code unchanged as much as possible
         * to minimize the risk.  That is why there is duplicate code here
         * for the SMB 2/3 path and also in SMB 1 code path.
         */
        error = smb1fs_statfs(share, sbp, context);
        return (error);
    }
    
	if (error) {
        return error;
    }
    
    t = fs_size.total_alloc_units;
    f = fs_size.avail_alloc_units;
    s = fs_size.bytes_per_sector;
    s *= fs_size.sectors_per_alloc_unit;
    /*
     * Don't allow over-large blocksizes as they determine
     * Finder List-view size granularities.  On the other
     * hand, we mustn't let the block count overflow the
     * 31 bits available.
     */
    while (s > 16 * 1024) {
        if (t > LONG_MAX)
            break;
        s /= 2;
        t *= 2;
        f *= 2;
    }
    while (t > LONG_MAX) {
        t /= 2;
        f /= 2;
        s *= 2;
    }
    sbp->f_bsize = (uint32_t)s;	/* fundamental file system block size */
    sbp->f_blocks= t;	/* total data blocks in file system */
    sbp->f_bfree = f;	/* free blocks in fs */
    sbp->f_bavail= f;	/* free blocks avail to non-superuser */
	sbp->f_files = sbp->f_blocks - 2;  /* max files is constrained by total blocks */
    sbp->f_ffree = sbp->f_bavail;
    
    /* 
     * Done with the network stuff, now get the iosize. This code was moved from 
     * smbfs_vfs_getattr to here 
     *
     * The min size of f_iosize is 1 MB and the max is SMB_IOMAX.
     * 
     * We used to report back a multiple of the max io size supported by the 
     * server and then read/write that amount in synchronous calls (SMB 1 still
     * does this). Now, we just let the applications decide how large and how
     * many IO blocks they want to give us. Finder will scale up to 8 MB IO 
     * blocks depending on the transfer times for each block. UBC will give us
     * multiple blocks of f_iosize depending on their internal logic. UBC gets
     * the f_iosize from us because we call vfs_setioattr().
     */
    xmax = max(SS_TO_SESSION(share)->session_rxmax, SS_TO_SESSION(share)->session_wxmax);
    
    /*
     * <Is this still needed???>
     * Now we want to make sure it will land on both a PAGE_SIZE boundary and a
     * smb xfer size boundary. So first mod the xfer size by the page size, then
     * subtract that from page size. This will give us the extra amount that 
     * will be needed to get it on a page boundary. Now divide the page size by 
     * this amount. This will give us the number of xmax it will take to make 
     * f_iosize land on a page size and xfer boundary. 
     */
    xmax = (PAGE_SIZE / (PAGE_SIZE - (xmax % PAGE_SIZE))) * xmax;
    
    if (xmax < SMB_IOMIN) {
        xmax = SMB_IOMIN;
    }
    
    if (xmax > SMB_IOMAX)
        sbp->f_iosize = SMB_IOMAX;
    else
        sbp->f_iosize = xmax;
    
    return error;
}

int
smb2fs_smb_validate_neg_info(struct smb_share *share, vfs_context_t context)
{
    struct smb_session *sessionp = SS_TO_SESSION(share);
	int error = 0;
    struct smb2_ioctl_rq *ioctlp = NULL;
    struct smb2_secure_neg_info req;
    struct smb2_secure_neg_info reply;
	struct smb_sopt *sp = NULL;
    int i, try_count = 0;
    struct smbiod *iod = NULL;

    if (sessionp == NULL) {
        SMBERROR("sessionp is null \n");
        return EINVAL;
    }
    
    /*
     * Only SMB 3.x and non Anonymous/Guest supports validate negotiate
     */
    if (!(sessionp->session_flags & SMBV_SMB2) ||
        (sessionp->session_flags & (SMBV_SMB2002 | SMBV_SMB21)) ||
        (sessionp->session_flags & SMBV_ANONYMOUS_ACCESS) ||
        (sessionp->session_flags & SMBV_GUEST_ACCESS)) {
        return 0;
    }
    
    sp = &sessionp->session_sopt;
    if (sp == NULL) {
        SMBERROR("sp is null \n");
        return EINVAL;
    }
    
    SMB_MALLOC(ioctlp,
               struct smb2_ioctl_rq *,
               sizeof(struct smb2_ioctl_rq),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (ioctlp == NULL) {
		SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }

try_again:
    /* Validate-Negotiate must be done on main channel */
    error = smb_iod_get_main_iod(SS_TO_SESSION(share), &iod, __FUNCTION__);
    if (error) {
        SMBERROR("smb_iod_get_main_iod failed %d\n", error);
        goto bad;
    }

    SMB_LOG_MC("id %u validate_neg with 0x%x \n", iod->iod_id, iod->iod_flags); 

    try_count += 1;
    if (try_count > 10) {
        /*
         * If we tried more than 10 times to do validate neg, then something
         * is seriously wrong and we need to give up.
         */
        SMBERROR("Too many attempts for Validate Neg <%d> \n", try_count);
        error = EAUTH;
        goto bad;
    }
    
    bzero(&req, sizeof(req));
    bzero(&reply, sizeof(reply));
    
    /*
     * Fill in the Secure Negotiate Request Info using the saved info from
     * the last Negotiate sent. Could be from first login or the last time
     * we reconnected on this session.
     */
    memcpy(&req.guid, sessionp->session_client_guid, 16);

    if (iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL) {
        /* Security Mode */
        req.security_mode = smb2_smb_get_client_security_mode(sessionp);
        req.capabilities =  smb2_smb_get_client_capabilities(sessionp);

        /* Get dialect count */
        uint16_t dialect_count, dialects[SMB3_MAX_DIALECTS];
        error = smb2_smb_get_client_dialects(sessionp,
                                             TRUE,  // Provide only the negotiated dialect
                                             &dialect_count,
                                             dialects, sizeof(dialects));
        if (error) {
            goto bad;
        }

        req.dialect_count = dialect_count;
        for (i = 0; i < dialect_count; i++) {     /* Dialects */
            req.dialects[i] = dialects[i];
        }


    } else {
        /* Security Mode */
        req.security_mode = sessionp->neg_security_mode;
        req.capabilities = sessionp->neg_capabilities;

        /* We have a max of SMB3_MAX_DIALECTS dialects at this time */
        if (sessionp->neg_dialect_count > SMB3_MAX_DIALECTS) {
            /* Should never happen */
            SMBERROR("Dialect count too high <%hu>? \n", sessionp->neg_dialect_count);
            error = ENOMEM;
            goto bad;
        }
        req.dialect_count = sessionp->neg_dialect_count;

        if (sizeof(req.dialects) < (sizeof(uint16_t) * req.dialect_count)) {
            /* Should never happen */
            SMBERROR("Not enough space for dialects <%ld> \n", sizeof(req.dialects));
            error = ENOMEM;
            goto bad;
        }

        for (i = 0; i < sessionp->neg_dialect_count; i++) {     /* Dialects */
            req.dialects[i] = sessionp->neg_dialects[i];
        }
    }

    /*
     * Build and send the IOCTL request
     */
    ioctlp->share = share;
    ioctlp->ctl_code = FSCTL_VALIDATE_NEGOTIATE_INFO;
    ioctlp->fid = 0;
    ioctlp->mc_flags = 0;

	ioctlp->snd_output_len = 0;
	ioctlp->rcv_input_len = 0;
    
    /*
     * Calculate len of Validate Negotiate Request
     * 24 = Capabilities (4) + GUID (16) + SecurityMode (2) + DialectCount (2)
     * +2 bytes per Dialect added
     */
    ioctlp->snd_input_len = 24;
    ioctlp->snd_input_len += 2 * req.dialect_count;
	ioctlp->snd_input_buffer = (uint8_t *) &req;
    
    ioctlp->rcv_output_len = sizeof(reply);
    ioctlp->rcv_output_buffer = (uint8_t *) &reply;
    
    error = smb2_smb_ioctl(share, iod, ioctlp, NULL, context);
    if (error) {
        if (error == ESTALE) {
           /*
            * The validate negotiate was in flight when we disconnected and we
            * want to resend it after a successful reconnect. We need to
            * completely rebuild the Valid Negotiate request as the dialect
            * count has changed to be just 1 due to the reconnect.
            */
            SMBWARNING("Rebuilding ValidateNeg \n");
            smb_iod_rel(iod, NULL, __FUNCTION__);
            goto try_again;
        }
        
        if ((error == EINVAL) || (error == ENOTSUP)) {
            /* 
             * A signed reply with STATUS_NOT_SUPPORTED is acceptable, but 
             * we can not validate the server capabilites. Just leave with
             * no error and without checking capabilities.
             *
             * NetApp is returning STATUS_INVALID_DEVICE_REQUEST instead which
             * is not correct.
             */
            SMBWARNING("Server does not fully support Validate Negotiate %d\n", error);
            error = 0;
        }
        else {
            SMBERROR("smb2_smb_ioctl failed %d\n", error);
        }
        
        goto bad;
    }
    
    /* Verify that the server info matches our current session */
    
	/* Check Capabilities */
	if (reply.capabilities != sp->sv_capabilities) {
		SMBERROR("Server capabilities do not match \n");
		error = EAUTH;
        goto bad;
	}
    
	/* Check GUID */
	if (memcmp(&reply.guid, &sp->sv_guid, sizeof(reply.guid)) != 0) {
		SMBERROR("Server GUID does not match \n");
		error = EAUTH;
        goto bad;
	}
    
 	/* Check Security Mode */
	if (reply.security_mode != sp->sv_security_mode) {
		SMBERROR("Server security mode does not match \n");
		error = EAUTH;
        goto bad;
	}
    
 	/* Check Dialect */
    switch (reply.dialects[0]) {
        case SMB2_DIALECT_0302:
            if (!(sessionp->session_flags & (SMBV_SMB2 | SMBV_SMB302))) {
                SMBERROR("Server dialect does not match \n");
                error = EAUTH;
            }
            break;
        case SMB2_DIALECT_0300:
            if (!(sessionp->session_flags & (SMBV_SMB2 | SMBV_SMB30))) {
                SMBERROR("Server dialect does not match \n");
                error = EAUTH;
            }
            break;
        case SMB2_DIALECT_0210:
            if (!(sessionp->session_flags & (SMBV_SMB2 | SMBV_SMB21))) {
                SMBERROR("Server dialect does not match \n");
                error = EAUTH;
            }
            break;
        case SMB2_DIALECT_0202:
            if (!(sessionp->session_flags & (SMBV_SMB2 | SMBV_SMB2002))) {
                SMBERROR("Server dialect does not match \n");
                error = EAUTH;
            }
            break;
            
        default:
            SMBERROR("Unknown server dialect does not match \n");
            error = EAUTH;
            break;
    }
    
bad:
    if (ioctlp != NULL) {
        SMB_FREE(ioctlp, M_SMBTEMP);
    }
    
    smb_iod_rel(iod, NULL, __FUNCTION__);
	return error;
}

int
smb2fs_smb_query_network_interface_info(struct smb_share *share, vfs_context_t context)
{
    struct smb_session *sessionp = SS_TO_SESSION(share);
    int error = 0;
    struct smb2_ioctl_rq *ioctlp = NULL;
    struct smb2_network_info_reply reply;

    if (sessionp == NULL) {
        SMBERROR("sessionp is null \n");
        return EINVAL;
    }
    
    if (!(sessionp->session_flags & SMBV_MULTICHANNEL_ON)) {
        /* Multichannel is not supported on this session */
        return 0;
    }
    
    bzero(&reply, sizeof(reply));
    SMB_MALLOC(reply.buff,
               void *,
               QUERY_NETWORK_INTERFACE_MAX_REPLY,
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (reply.buff == NULL) {
        SMBERROR("SMB_MALLOC failed for reply.buff\n");
        error = ENOMEM;
        goto bad;
    }
    reply.buff_size = QUERY_NETWORK_INTERFACE_MAX_REPLY;

    SMB_MALLOC(ioctlp,
               struct smb2_ioctl_rq *,
               sizeof(struct smb2_ioctl_rq),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (ioctlp == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    /*
     * Build and send the IOCTL request
     */
    ioctlp->share = share;
    ioctlp->ctl_code = FSCTL_QUERY_NETWORK_INTERFACE_INFO;
    ioctlp->fid = 0;
    ioctlp->mc_flags = 0;

    ioctlp->snd_output_len = 0;
    ioctlp->rcv_input_len = 0;
    
    ioctlp->snd_input_len = 0;
    
    ioctlp->rcv_output_len = reply.buff_size;
    ioctlp->rcv_output_buffer = (uint8_t *)reply.buff;
    
    error = smb2_smb_ioctl(share, NULL, ioctlp, NULL, context);
    if (error) {
        SMBERROR("smb2_smb_ioctl for query_network_interface_info failed %d\n", error);
        goto bad;
    }
    
    /*
     * Build interface tables
     */
    /* Parse the server interfaces response */
    error = smb2_mc_query_info_response_event(&sessionp->session_interface_table, ioctlp->rcv_output_buffer, ioctlp->ret_output_len);
    if (error) {
        SMBERROR("smb2_mc_query_info_response_event returned %d.\n", error);
        goto bad;
    }

bad:
    if (reply.buff) {
        SMB_FREE(reply.buff, M_SMBTEMP);
    }
    
    if (ioctlp != NULL) {
        SMB_FREE(ioctlp, M_SMBTEMP);
    }

    return error;
}
