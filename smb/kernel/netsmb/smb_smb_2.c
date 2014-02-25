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

#include <sys/msfscc.h>
#include <sys/smb_apple.h>
#include <libkern/OSAtomic.h>

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_packets_2.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_conn_2.h>
#include <netsmb/smb_tran.h>
#include <netsmb/smb_gss.h>
#include <netsmb/smb_fid.h>
#include <smbfs/smbfs_subr.h>
#include <smbfs/smbfs_subr_2.h>
#include <smbfs/smbfs_node.h>
#include <netsmb/smb_converter.h>
#include <smbfs/smbfs.h>

#define kMAX_READ_BLOCKS 4      /* Must be larger than Write number of blocks */
#define kMAX_WRITE_BLOCKS 2

#include <sys/sysctl.h>


static uint32_t smb_maxwrite = 512 * 1024;	/* Default max write size */
static uint32_t smb_maxread = 1024 * 1024;	/* Default max read size */

SYSCTL_DECL(_net_smb_fs);
SYSCTL_INT(_net_smb_fs, OID_AUTO, maxwrite, CTLFLAG_RW, &smb_maxwrite, 0, "");
SYSCTL_INT(_net_smb_fs, OID_AUTO, maxread, CTLFLAG_RW, &smb_maxread, 0, "");


/*
 * Note:  The _smb_ in the function name indicates that these functions are 
 * marshalling the request and unmarshalling the replies.
 *
 * They have a generic _smb_ structure that is used to pass in information and
 * to pass back the results.
 *
 */

static int
smb2_smb_parse_create_contexts(struct smb_share *share, struct mdchain *mdp,
                               uint32_t *ret_context_offset,
                               struct smb2_create_rq *createp);
static int
smb2_smb_parse_create_str(struct smb_share *share, struct mdchain *mdp,
                          uint32_t is_path, char **ret_str);
static int
smb2_smb_parse_negotiate(struct smb_vc *vcp, struct smb_rq *rqp, int smb1_req);

int
smb2_smb_read_one(struct smb_share *share, struct smb2_rw_rq *readp,
                  user_ssize_t *len, user_ssize_t *rresid,
                  struct smb_rq **compound_rqp, vfs_context_t context);

static int
smb2_smb_read_uio(struct smb_share *share, SMBFID fid, uio_t uio,
                  vfs_context_t context);

static int
smb2_smb_read_write_async(struct smb_share *share,
                          struct smb2_rw_rq *in_read_writep,
                          user_ssize_t *len, user_ssize_t *rresid,
                          uint32_t do_read, vfs_context_t context);

static int
smb2_smb_read_write_fill(struct smb_share *share,
                         struct smb2_rw_rq *master_read_writep,
                         struct smb2_rw_rq *read_writep, struct smb_rq **rqp,
                         uint32_t do_read, vfs_context_t context);

static int
smb2_smb_write_one(struct smb_share *share, struct smb2_rw_rq *args,
                   user_ssize_t *len, user_ssize_t *rresid,
                   struct smb_rq **compound_rqp, vfs_context_t context);

static uint32_t
smb2_vc_maxread(struct smb_vc *vcp);

static uint32_t
smb2_vc_maxtransact(struct smb_vc *vcp);

static uint32_t
smb2_vc_maxwrite(struct smb_vc *vcp);


static int
smb2_smb_add_create_contexts(struct smb_share *share, struct smb2_create_rq *createp, struct mbchain *mbp,
                             uint16_t name_offset, uint16_t name_len,
                             uint32_t *context_offset_ptr, uint32_t *context_len_ptr)
{
    int error = 0;
    uint32_t context_offset, prev_content_size;
    uint32_t context_len, *next_context_ptr;
    int pad_bytes = 0;
    uint64_t aapl_req_bitmap =  kAAPL_SERVER_CAPS |
                                kAAPL_VOLUME_CAPS |
                                kAAPL_MODEL_INFO;
    uint64_t aapl_client_caps = kAAPL_SUPPORTS_READ_DIR_ATTR |
                                kAAPL_SUPPORTS_OSX_COPYFILE |
                                kAAPL_UNIX_BASED;
    struct smb2_create_ctx_resolve_id *resolve_idp = NULL;
    struct smb2_durable_handle *dur_handlep = NULL;
    SMB2FID smb2_fid;
    uint32_t lease_state;

    if (!(createp->flags & (SMB2_CREATE_GET_MAX_ACCESS |
                            SMB2_CREATE_AAPL_QUERY |
                            SMB2_CREATE_AAPL_RESOLVE_ID |
                            SMB2_CREATE_DUR_HANDLE |
                            SMB2_CREATE_DUR_HANDLE_RECONNECT))) {
        /* No contexts to add */
        context_len = 0;
        *context_len_ptr = htolel(context_len);
        context_offset = 0;
        *context_offset_ptr = htolel(context_offset);
    }
    else {
        context_len = 0;
        next_context_ptr = NULL;
        prev_content_size = 0;
        
        /* Set the Context Offset */
        context_offset = name_offset + letohs(name_len);
        if (name_len == 0) {
            context_offset += 2; /* for null bytes */
        }
        
        if ((context_offset % 8) != 0) {
            /* Contexts MUST start on next 8 byte boundary! */
            pad_bytes = 8 - (context_offset % 8);
            mb_put_mem(mbp, NULL, pad_bytes, MB_MZERO);
            context_offset += pad_bytes;
        }
        
        *context_offset_ptr = htolel(context_offset);
        
        /*
         * Add the Contexts - Apple ones should be added first
         *
         */
        if (createp->flags & SMB2_CREATE_AAPL_QUERY) {
            /* Check for OS X Server */
            if (next_context_ptr != NULL) {
                /* Set prev context next ptr */
                *next_context_ptr = htolel(prev_content_size);
            }
            
            context_len += 48;
            prev_content_size = 48;
            
            next_context_ptr = mb_reserve(mbp, sizeof(uint32_t));   /* Next */
            *next_context_ptr = htolel(0);  /* Assume we are last context */
            mb_put_uint16le(mbp, 16);       /* Name Offset */
            mb_put_uint16le(mbp, 4);        /* Name Length */
            mb_put_uint16le(mbp, 0);        /* Reserved */
            mb_put_uint16le(mbp, 24);       /* Data Offset */
            mb_put_uint32le(mbp, 24);       /* Data Length */
            /* Name is a string constant and thus its not byte swapped uint32 */
            mb_put_uint32be(mbp, SMB2_CREATE_AAPL);
            mb_put_uint32le(mbp, 0);        /* Pad to 8 byte boundary */
            mb_put_uint32le(mbp, kAAPL_SERVER_QUERY); /* Sub Command */
            mb_put_uint32le(mbp, 0);        /* Reserved */
            mb_put_uint64le(mbp, aapl_req_bitmap);    /* req bitmap */
            mb_put_uint64le(mbp, aapl_client_caps);   /* client capabilities */
        }
        
        if (createp->flags & SMB2_CREATE_AAPL_RESOLVE_ID) {
            /*
             * Do Resolve ID
             * create_contextp points to struct smb2_create_ctx_resolve_id
             */
            resolve_idp = createp->create_contextp;
            if (resolve_idp == NULL) {
                SMBDEBUG("resolve_idp is NULL \n");
                error = EINVAL;
                goto bad;
            }
            
            /* Set default return values */
            *resolve_idp->ret_errorp = ENOENT;
            *resolve_idp->ret_pathp = NULL;
            
            /* AAPL Resolve ID */
            if (next_context_ptr != NULL) {
                /* Set prev context next ptr */
                *next_context_ptr = htolel(prev_content_size);
            }
            
            context_len += 40;
            prev_content_size = 40;
            
            next_context_ptr = mb_reserve(mbp, sizeof(uint32_t));   /* Next */
            *next_context_ptr = htolel(0);  /* Assume we are last context */
            mb_put_uint16le(mbp, 16);       /* Name Offset */
            mb_put_uint16le(mbp, 4);        /* Name Length */
            mb_put_uint16le(mbp, 0);        /* Reserved */
            mb_put_uint16le(mbp, 24);       /* Data Offset */
            mb_put_uint32le(mbp, 16);       /* Data Length */
            /* Name is a string constant and thus its not byte swapped uint32 */
            mb_put_uint32be(mbp, SMB2_CREATE_AAPL);
            mb_put_uint32le(mbp, 0);        /* Pad to 8 byte boundary */
            mb_put_uint32le(mbp, kAAPL_RESOLVE_ID); /* Sub Command */
            mb_put_uint32le(mbp, 0);        /* Reserved */
            mb_put_uint64le(mbp, resolve_idp->file_id); /* File ID */
        }
        
        if (createp->flags & SMB2_CREATE_GET_MAX_ACCESS) {
            /* Get max access */
            if (next_context_ptr != NULL) {
                /* Set prev context next ptr */
                *next_context_ptr = htolel(prev_content_size);
            }
            
            context_len += 24;
            prev_content_size = 24;
            
            next_context_ptr = mb_reserve(mbp, sizeof(uint32_t));   /* Next */
            *next_context_ptr = htolel(0);  /* Assume we are last context */
            mb_put_uint16le(mbp, 16);       /* Name Offset */
            mb_put_uint16le(mbp, 4);        /* Name Length */
            mb_put_uint16le(mbp, 0);        /* Reserved */
            mb_put_uint16le(mbp, 0);        /* Data Offset */
            mb_put_uint32le(mbp, 0);        /* Data Length */
            /* Name is a string constant and thus its not byte swapped uint32 */
            mb_put_uint32be(mbp, SMB2_CREATE_QUERY_MAXIMAL_ACCESS);
            mb_put_uint32le(mbp, 0);        /* Pad to 8 byte boundary */
        }

        if ((createp->flags & SMB2_CREATE_DUR_HANDLE) ||
            (createp->flags & SMB2_CREATE_DUR_HANDLE_RECONNECT)) {
            dur_handlep = createp->create_contextp;
            if (dur_handlep == NULL) {
                SMBERROR("dur_handlep is NULL \n");
                error = EBADRPC;
                goto bad;
            }

            /*
             * Both of these calls need a lease context
             * Add Lease Request
             */
            /* Lease State */
            if (createp->flags & SMB2_CREATE_DUR_HANDLE_RECONNECT) {
                /* Reconnect, have to request exact same lease */
                lease_state = dur_handlep->lease_state;
            }
            else {
                /* New lease, so lways want Read and Handle lease */
                lease_state = SMB2_LEASE_READ_CACHING | SMB2_LEASE_HANDLE_CACHING;
            }

            if (next_context_ptr != NULL) {
                /* Set prev context next ptr */
                *next_context_ptr = htolel(prev_content_size);
            }
            
            context_len += 56;
            prev_content_size = 56;
            
            next_context_ptr = mb_reserve(mbp, sizeof(uint32_t));   /* Next */
            *next_context_ptr = htolel(0);  /* Assume we are last context */
            mb_put_uint16le(mbp, 16);       /* Name Offset */
            mb_put_uint16le(mbp, 4);        /* Name Length */
            mb_put_uint16le(mbp, 0);        /* Reserved */
            mb_put_uint16le(mbp, 24);       /* Data Offset */
            mb_put_uint32le(mbp, 32);       /* Data Length */
            /* Name is a string constant and thus its not byte swapped uint32 */
            mb_put_uint32be(mbp, SMB2_CREATE_REQUEST_LEASE);
            mb_put_uint32le(mbp, 0);        /* Pad to 8 byte boundary */
            mb_put_uint64le(mbp, dur_handlep->lease_key_hi);  /* Lease Key High */
            mb_put_uint64le(mbp, dur_handlep->lease_key_low); /* Lease Key Low */
            mb_put_uint32le(mbp, lease_state);  /* Lease State */
            mb_put_uint32le(mbp, 0);        /* Lease Flags */
            mb_put_uint64le(mbp, 0);        /* Lease Duration */
        }
        
        if (createp->flags & SMB2_CREATE_DUR_HANDLE) {
            /*
             * Add Durable Handle Request
             */
            if (next_context_ptr != NULL) {
                /* Set prev context next ptr */
                *next_context_ptr = htolel(prev_content_size);
            }
            
            context_len += 40;
            prev_content_size = 40;
            
            next_context_ptr = mb_reserve(mbp, sizeof(uint32_t));   /* Next */
            *next_context_ptr = htolel(0);  /* Assume we are last context */
            mb_put_uint16le(mbp, 16);       /* Name Offset */
            mb_put_uint16le(mbp, 4);        /* Name Length */
            mb_put_uint16le(mbp, 0);        /* Reserved */
            mb_put_uint16le(mbp, 24);       /* Data Offset */
            mb_put_uint32le(mbp, 16);       /* Data Length */
            /* Name is a string constant and thus its not byte swapped uint32 */
            mb_put_uint32be(mbp, SMB2_CREATE_DURABLE_HANDLE_REQUEST);
            mb_put_uint32le(mbp, 0);        /* Pad to 8 byte boundary */
            mb_put_uint64le(mbp, 0);        /* Durable Request */
            mb_put_uint64le(mbp, 0);        /* Durable Request */
        }
        
        if (createp->flags & SMB2_CREATE_DUR_HANDLE_RECONNECT) {
            /*
             * Durable Handle Reconnect
             */

            /* map fid to smb2 fid */
            error = smb_fid_get_kernel_fid(share, dur_handlep->fid, 0, &smb2_fid);
            if (error) {
                goto bad;
            }
            
            if (next_context_ptr != NULL) {
                /* Set prev context next ptr */
                *next_context_ptr = htolel(prev_content_size);
            }
            
            context_len += 40;
            /* prev_content_size = 40; */   /* Last context so not needed */

            next_context_ptr = mb_reserve(mbp, sizeof(uint32_t));   /* Next */
            *next_context_ptr = htolel(0);  /* Assume we are last context */
            mb_put_uint16le(mbp, 16);       /* Name Offset */
            mb_put_uint16le(mbp, 4);        /* Name Length */
            mb_put_uint16le(mbp, 0);        /* Reserved */
            mb_put_uint16le(mbp, 24);       /* Data Offset */
            mb_put_uint32le(mbp, 16);       /* Data Length */
            /* Name is a string constant and thus its not byte swapped uint32 */
            mb_put_uint32be(mbp, SMB2_CREATE_DURABLE_HANDLE_RECONNECT);
            mb_put_uint32le(mbp, 0);        /* Pad to 8 byte boundary */
            mb_put_uint64le(mbp, smb2_fid.fid_persistent); /* FID */
            mb_put_uint64le(mbp, smb2_fid.fid_volatile);   /* FID */
        }

        /* Set the Context Length */
        *context_len_ptr = htolel(context_len);
    }

bad:
    return (error);
}

/*
 * The calling routine must hold a reference on the share
 */
int
smb2_smb_change_notify(struct smb_share *share, void *args_ptr, 
                       struct smb_rq **in_rqp, vfs_context_t context)
{
    struct smb2_change_notify_rq *changep = args_ptr;
	struct smb_rq *rqp;
	struct mbchain *mbp;
	int error;
    SMB2FID smb2_fid;
    
    /* 
     * Allocate request and header for a Change Notify 
     * Win 7 Client sends this as a Sync request, so match their behavior
     */
    error = smb2_rq_alloc(SSTOCP(share), SMB2_CHANGE_NOTIFY, 
                          &changep->output_buffer_len, 
                          context, &rqp);
    if (error) {
        return error;
    }
    
    /* Set up the async call back */
    rqp->sr_flags |= SMBR_ASYNC;
    rqp->sr_callback = changep->fn_callback;
    rqp->sr_callback_args = changep->fn_callback_args;

    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB2 Change Notify Request
     */
    mb_put_uint16le(mbp, 32);                           /* Struct size */
    mb_put_uint16le(mbp, changep->flags);               /* Flags */
    mb_put_uint32le(mbp, changep->output_buffer_len);   /* Output buffer len */
    
    /* map fid to smb2 fid */
    error = smb_fid_get_kernel_fid(share, changep->fid, 0, &smb2_fid);
    if (error) {
        goto bad;
    }
    mb_put_uint64le(mbp, smb2_fid.fid_persistent);      /* FID */
    mb_put_uint64le(mbp, smb2_fid.fid_volatile);        /* FID */
    mb_put_uint32le(mbp, changep->filter);              /* Completion Filter */
    mb_put_uint32le(mbp, 0);                            /* Reserved */
    
    if (in_rqp != NULL) {
        *in_rqp = rqp;
    }
    
    /* 
     * This is always an async send, so just enqueue it directly to be sent 
     * No need to handle reconnect as the Change Notify code will handle
     * resending the Change Notify later.
     */
    error = smb_iod_rq_enqueue(rqp);
    if (error) {
        /* failed to enqueue, so manually clean up */
        if (in_rqp != NULL) {
            *in_rqp = NULL;
        }
        smb_rq_done(rqp);
        goto bad;
    }
    
bad:    
    /* 
     * Note: smb_rq_done will be done in reset_notify_change
     */

    return error;
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smb2_smb_close(struct smb_share *share, void *arg_ptr,
               struct smb_rq **compound_rqp, vfs_context_t context)
{
    struct smb2_close_rq *closep = arg_ptr;
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
    uint16_t flags;
    SMB2FID smb2_fid;
    
resend:
    /* Allocate request and header for a Close */
    error = smb2_rq_alloc(SSTOCP(share), SMB2_CLOSE, NULL, context, &rqp);
    if (error) {
        return error;
    }
    
    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB2 Close Request
     */
    mb_put_uint16le(mbp, 24);       /* Struct size */
    flags = closep->flags;          /* only take lower 16 bits for now */
    mb_put_uint16le(mbp, flags);    /* Flags */
    mb_put_uint32le(mbp, 0);        /* Reserved */

    /* map fid to smb2 fid */
    error = smb_fid_get_kernel_fid(share, closep->fid, 0, &smb2_fid);
    if (error) {
        goto bad;
    }
    mb_put_uint64le(mbp, smb2_fid.fid_persistent); /* FID */
    mb_put_uint64le(mbp, smb2_fid.fid_volatile);   /* FID */
    
    if (compound_rqp != NULL) {
        /* 
         * building a compound request, add padding to 8 bytes and just
         * return this built request.
         */
        smb2_rq_align8(rqp);
        rqp->sr_flags |= SMBR_COMPOUND_RQ;
        *compound_rqp = rqp;
        return (0);
    }

    error = smb_rq_simple(rqp);
    closep->ret_ntstatus = rqp->sr_ntstatus;
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            goto resend;
        }
        
        goto bad;
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    error = smb2_smb_parse_close(mdp, closep);
    if (error) {
        goto bad;
    }
bad:    
	smb_rq_done(rqp);
    return error;
}

int
smb2_smb_close_fid(struct smb_share *share, 
                   SMBFID fid, struct smb_rq **compound_rqp, 
                   struct smb2_close_rq **in_closep,
                   vfs_context_t context)
{
	int error;
    struct smb2_close_rq *closep = NULL;
    
    SMB_MALLOC(closep, 
               struct smb2_close_rq *, 
               sizeof(struct smb2_close_rq), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (closep == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    closep->share = share;
    closep->flags = 0;  /* dont want any returned attributes */
    closep->fid = fid;
    
    error = smb2_smb_close(share, closep, compound_rqp, context);
    if (error) {
        SMBDEBUG("smb2_smb_close failed %d ntstatus 0x%x\n",
                 error,
                 closep->ret_ntstatus);
    }
    
    /* Used for compound requests */
    if (in_closep != NULL) {
        *in_closep = closep;
        closep = NULL;     /* dont free it */
    }

bad:
    if (closep != NULL) {
        SMB_FREE(closep, M_SMBTEMP);
    }
    
	return error;
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smb2_smb_create(struct smb_share *share, void *arg_ptr, 
                struct smb_rq **compound_rqp, vfs_context_t context)
{
	int error;
    struct smb2_create_rq *createp = arg_ptr;
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
    uint32_t *context_offset_ptr, *context_len_ptr;
	uint16_t *name_len, name_offset;
	uint8_t sep_char = '\\';

resend:
    /* Allocate request and header for a Create */
    error = smb2_rq_alloc(SSTOCP(share), SMB2_CREATE, NULL, context, &rqp);
    if (error) {
        return error;
    }
    
    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB2 Create Request
     */
    mb_put_uint16le(mbp, 57);                       /* Struct size */
    mb_put_uint8(mbp, 0);                           /* Security flags */
    mb_put_uint8(mbp, createp->oplock_level);       /* Oplock level */
    mb_put_uint32le(mbp, createp->impersonate_level); /* Impersonation Level */
    mb_put_uint64le(mbp, 0);                        /* Create flags */
    mb_put_uint64le(mbp, 0);                        /* Reserved */
    mb_put_uint32le(mbp, createp->desired_access);  /* Desired access */
    mb_put_uint32le(mbp, createp->file_attributes); /* File attributes */
    mb_put_uint32le(mbp, createp->share_access);    /* Share access */
    mb_put_uint32le(mbp, createp->disposition);     /* Create disposition */
    mb_put_uint32le(mbp, createp->create_options);  /* Create options */
    name_offset = 120;
    mb_put_uint16le(mbp, name_offset);              /* Name offset */
    name_len = mb_reserve(mbp, sizeof(uint16_t));   /* Name len */
    context_offset_ptr = mb_reserve(mbp, sizeof(uint32_t)); /* Context offset */
    context_len_ptr = mb_reserve(mbp, sizeof(uint32_t));    /* Context len */
    
    /* 
     * Build the path
     *
     * There are several variations on how the path can be built
     * 1) Blank name - createp->dnp and createp->name_len = 0. Root of share.
     * 2) Just createp->dnp. Create path to dnp.
     * 3) createp->dnp and createp->namep. Create path to parent dnp, then add
     *    namep on to the end of the path. Usually parent + child path.
     * 4) createp->dnp and createp->strm_namep and SMB2_CREATE_IS_NAMED_STREAM.
     *    Create path to dnp and strm_namep is stream name to add to the path.
     * 5) Just createp->namep and no create->dnp. Only used when 
     *    SMB2_CREATE_NAME_IS_PATH is set. namep points to a pre built path to 
     *    just copy in. smb_usr_check_dir() calling us or early in mount.
     * 6) createp->dnp and createp->namep and createp->strm_namep. From
     *    readdirattr. Create path to parent dnp, add child of namep, then
     *    add stream name in strm_namep on. Used for reading Finder Info stream.
     */
    if ((createp->name_len > 0) || (createp->dnp != NULL)) {
        if (!(createp->flags & SMB2_CREATE_NAME_IS_PATH)) {
            /* Create the network path and insert it */
            smb2_rq_bstart(rqp, name_len);
            error = smb2fs_fullpath(mbp, createp->dnp, 
                                    createp->namep, createp->name_len, 
                                    createp->strm_namep, createp->strm_name_len,
                                    UTF_SFM_CONVERSIONS, sep_char);
            if (error) {
                SMBERROR("error %d from smb_put_dmem for name\n", error);
                goto bad;
            }		
            smb_rq_bend(rqp);           /* now fill in name_len */
        }
        else {
            /* 
             * Network path is already in createp->namep, just copy it in.
             * Must be smb_usr_check_dir() calling us or early in mount.
             */
            mb_put_mem(mbp, (char *) createp->namep, createp->name_len,
                       MB_MSYSTEM);
            *name_len = htoles(createp->name_len);

            /* 
             * This is a DFS operation so set the flag in the header 
             * If this flag is set, then the file name can be prefixed with DFS
             * link information.
             */
            *rqp->sr_flagsp |= htolel(SMB2_FLAGS_DFS_OPERATIONS);
        }
    }
    else {
        /* blank name */
        *name_len = htoles(0);      /* set name len to 0 */
    }
    
    if (*name_len == 0) {
        /* if blank name, fill in blank name with 0x0000 */
        mb_put_uint16le(mbp, 0);    
    }
    
    /* 
     * Add in the create contexts 
     */
    error = smb2_smb_add_create_contexts(share, createp, mbp,
                                         name_offset, *name_len,
                                         context_offset_ptr, context_len_ptr);
    if (error) {
        goto bad;
    }
    
    if (compound_rqp != NULL) {
        /*
         * building a compound request, add padding to 8 bytes and just
         * return this built request.
         */
        smb2_rq_align8(rqp);
        rqp->sr_flags |= SMBR_COMPOUND_RQ;
        *compound_rqp = rqp;
        return (0);
    }
    
    error = smb_rq_simple(rqp);
    createp->ret_ntstatus = rqp->sr_ntstatus;
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            goto resend;
        }

        goto bad;
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    error = smb2_smb_parse_create(share, mdp, createp);
    if (error) {
        goto bad;
    }

bad:    
    smb_rq_done(rqp);
    return error;
}

int
smb2_smb_dur_handle_init(struct smb_share *share, struct smbnode *np,
                         struct smb2_durable_handle *dur_handle)
{
    int error = ENOTSUP;
    struct smb_vc *vcp = SSTOVC(share);

    memset(dur_handle, 0, sizeof(*dur_handle));
    
    if ((share == NULL) || (np == NULL)) {
        SMBERROR("share or np is null \n");
        return EINVAL;
    }
    
    /*
     * Only SMB 2.1 and servers that support leasing can do
     * reconnect. File IDs are also required for handling lease breaks
     */
    if ((SSTOVC(share)->vc_flags & SMBV_SMB21) &&
        (SSTOVC(share)->vc_sopt.sv_capabilities & SMB2_GLOBAL_CAP_LEASING) &&
        (SSTOVC(share)->vc_misc_flags & SMBV_HAS_FILEIDS)) {
        
        dur_handle->flags |= SMB2_DURABLE_HANDLE_REQUEST;
        
        /*
         * Lease Key
         * 1) Upper uint32_t is atomic count to keep key unique
         * 2) Next uint32_t is tree ID so can identify the share
         * 3) Lower uint64_t is the File ID so can find the node
         *    Main node and all named streams have same File ID, thus the need for
         *    the atomic count in the upper uint32_t
         */
        dur_handle->lease_key_hi = OSAddAtomic(1, &vcp->vc_lease_key);
        dur_handle->lease_key_hi = dur_handle->lease_key_hi << 32;
        dur_handle->lease_key_hi |= share->ss_tree_id;
        
        dur_handle->lease_key_low = smbfs_hash(share, np->n_ino,
                                               np->n_name, np->n_nmlen);
        error = 0;
    }
    
    return error;
}

void
smb2_smb_dur_handle_parse_lease_key(uint64_t lease_key_hi, uint64_t lease_key_low,
                                    uint32_t *tree_id, uint64_t *hash_val)
{
    /* uint32_t high = lease_key_hi >> 32; */
    uint32_t low = lease_key_hi & 0x00000000ffffffff;
    
    *tree_id = low;
    *hash_val = lease_key_low;
}

static int
smb2_smb_echo(struct smb_vc *vcp, int timeout, vfs_context_t context)
{
    struct smb_rq *rqp;
    struct mbchain *mbp;
    int error;
    
    /* Allocate request and header for an Echo */
    error = smb2_rq_alloc(VCTOCP(vcp), SMB2_ECHO, NULL, context, &rqp);
    if (error) {
        return error;
    }
    
    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB2 Echo Request
     */
    mb_put_uint16le(mbp, 4);                    /* Struct size */
    mb_put_uint16le(mbp, 0);                    /* Reserved */
    
    error = smb_rq_simple_timed(rqp, timeout);
    if (error) {
        /* Dont care if reconnect causes this request to fail */
        goto bad;
    }
    
    /* 
     * This really is just an Async request since the timeout value is
     * set to the magic value of SMBNOREPLYWAIT. It can not be a sync call
     * because its called from smb_iod_sendall() and we dont want it to become
     * a recursive call with first request blocked waiting on an Echo which is
     * waiting on another Echo and so on.
     *
     * NOTE: Assume we get back one credit in the Echo response
     */

    /* pretend like it did not get sent to recover SMB2 credits */
    rqp->sr_extflags &= ~SMB2_REQ_SENT;
    
bad:    
    smb_rq_done(rqp);
    return error;
}

/*
 * This call is done on the vc not the share. Really should be an async call
 * if we ever get the request queue to work async.
 */
int
smb_smb_echo(struct smb_vc *vcp, int timeout, uint32_t EchoCount,
             vfs_context_t context)
{
    int error;
    
    if (vcp->vc_flags & SMBV_SMB2) {
        error = smb2_smb_echo(vcp, timeout, context);
    }
    else {
        error = smb1_echo(vcp, timeout, EchoCount, context);
    }
    
    return (error);
}

int
smb2_smb_flush(struct smb_share *share, SMBFID fid, vfs_context_t context)
{
    struct smb_rq *rqp;
    struct mbchain *mbp;
    struct mdchain *mdp;
    int error;
    uint16_t reserved_uint16;
    uint16_t length;
    SMB2FID smb2_fid;
    
resend:
    /* Allocate request and header for a Flush */
    error = smb2_rq_alloc(SSTOCP(share), SMB2_FLUSH, NULL, context, &rqp);
    if (error) {
        return error;
    }
    
    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB2 Flush Request
     */
    mb_put_uint16le(mbp, 24);                   /* Struct size */
    mb_put_uint16le(mbp, 0);                    /* Reserved */
    mb_put_uint32le(mbp, 0);                    /* Reserved */

    /* map fid to smb2 fid */
    error = smb_fid_get_kernel_fid(share, fid, 0, &smb2_fid);
    if (error) {
        goto bad;
    }
    mb_put_uint64le(mbp, smb2_fid.fid_persistent);   /* FID */
    mb_put_uint64le(mbp, smb2_fid.fid_volatile);     /* FID */
    
    error = smb_rq_simple(rqp);
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            goto resend;
        }

        goto bad;
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    /* 
     * Parse SMB2 Flush Response 
     * We are already pointing to begining of Response data
     */
    
    /* Check structure size is 4 */
    error = md_get_uint16le(mdp, &length);
    if (error) {
        goto bad;
    }
    if (length != 4) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)length);
        error = EBADRPC;
        goto bad;
    }
    
    /* Get Reserved */
    error = md_get_uint16le(mdp, &reserved_uint16);
    if (error) {
        goto bad;
    }
    
bad:    
    smb_rq_done(rqp);
    return error;
}

static uint64_t
smb2_smb_get_alloc_size(struct smbmount *smp, uint64_t logical_size)
{
    uint64_t alloc_size;
    
    if (logical_size == 0) {
        alloc_size = 0;
    }
    else {
        alloc_size = ((logical_size / smp->sm_statfsbuf.f_bsize) * smp->sm_statfsbuf.f_bsize) + smp->sm_statfsbuf.f_bsize;
    }
    
    return(alloc_size);
}

int
smb2_smb_gss_session_setup(struct smb_vc *vcp, uint16_t *session_flags,
                           vfs_context_t context)
{
	struct smb_rq *rqp = NULL;
	struct smb_gss *gp = &vcp->vc_gss;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
	uint32_t tokenlen;
	uint32_t maxtokenlen;
	uint8_t *tokenptr;
    uint16_t length;
	uint16_t sec_buf_offset;
	uint16_t sec_buf_len;
	
#ifdef SMB_DEBUG
	/* For testing use a smaller max size */
	maxtokenlen = 2048;
#else // SMB_DEBUG
	/* Get the max blob size we can send in Session Setup message */
	maxtokenlen = vcp->vc_txmax - (SMB2_HDRLEN); /* %%% To Do - is this right? */
#endif // SMB_DEBUG
    
	tokenptr = gp->gss_token;	/* Get the start of the Kerberos blob */
    
    do {        
        if (rqp) {	/* If looping release curr rqp, before getting another */
            smb_rq_done(rqp);
        }
        
        /* Allocate request and header for a Session Setup */
        error = smb2_rq_alloc(VCTOCP(vcp), SMB2_SESSION_SETUP, NULL, 
                              context, &rqp);
        if (error) {
            break;
        }
        
        /* 
         * Fill in Session Setup part 
         * Cant use a struct ptr due to var length security blob that
         * could be some giant size greater than a single mbuf.
         */
        smb_rq_getrequest(rqp, &mbp);
        
        mb_put_uint16le(mbp, 25);       /* Struct size */
        mb_put_uint8(mbp, 0);           /* VcNumber */
        
        /*Security Mode */
		if (vcp->vc_misc_flags & SMBV_CLIENT_SIGNING_REQUIRED) {
			mb_put_uint8(mbp, (SMB2_NEGOTIATE_SIGNING_ENABLED | SMB2_NEGOTIATE_SIGNING_REQUIRED));
		}
		else {
			mb_put_uint8(mbp, SMB2_NEGOTIATE_SIGNING_ENABLED);
		}
		mb_put_uint32le(mbp, SMB2_GLOBAL_CAP_DFS);         /* Capabilities */
        mb_put_uint32le(mbp, 0);        /* Channel - always 0 */
        mb_put_uint16le(mbp, 88);       /* Sec buffer offset */
        tokenlen = (gp->gss_tokenlen > maxtokenlen) ? maxtokenlen : gp->gss_tokenlen;
        mb_put_uint16le(mbp, tokenlen); /* Sec buffer len */
        mb_put_uint64le(mbp, vcp->vc_prev_session_id);      /* Prev SessID */
        
        /* SPNEGO blob */
        mb_put_mem(mbp, (caddr_t) tokenptr, tokenlen, MB_MSYSTEM);
        
        /* Send the request and check for reply */
        error = smb_rq_simple_timed(rqp, SMBSSNSETUPTIMO);
        
        if ((error) && (rqp->sr_flags & SMBR_RECONNECTED)) {
            /* Rebuild and try sending again */
            continue;
        }
        
        /* Move the pointer to the next offset in the blob */
        tokenptr += tokenlen;
        /* Subtract the size we have already sent */
        gp->gss_tokenlen -= tokenlen;
    } while (gp->gss_tokenlen && (error == EAGAIN));
    
	/* Free the gss spnego token that we sent */
	SMB_FREE(gp->gss_token, M_SMBTEMP);
    gp->gss_token = NULL;
	gp->gss_tokenlen = 0;
	gp->gss_smb_error = error;	/* Hold on to the last smb error returned */
	/* EAGAIN is not  really an  error, reset it to no error */
	if (error == EAGAIN) {
		error = 0;
	}
	/* At this point error should have the correct error, we only support NTStatus code with extended security */
	if (error) {
		SMB_LOG_AUTH("Extended security authorization failed! %d\n", error);
		goto bad;
	}
    
	/*
	 * Save the servers vc identifier. Seems Samba will give us a new one for
	 * every loop of a SetUpAndX NTLMSSP response. Windows server just return
	 * the same one every time. We assume here the last one is the one we 
	 * should always use. Seems to make Samba work correctly.
	 */
	vcp->vc_smbuid = rqp->sr_rpuid;
	
	/* Get the reply  and decode the result */
	smb_rq_getreply(rqp, &mdp);
    
    /* Using SMB2 */
    
    /* 
     * Parse SMB2 Session Setup Response 
     * We are already pointing to begining of Response data
     * Cant cast to sturct pointer due to var len security blob
     */
    
    /* 
     * This is the first Session Setup Response packet here
     */
    if (vcp->vc_session_id == 0) {
        vcp->vc_session_id = rqp->sr_rspsessionid;
    }
    
    /* Check structure size is 9 */
    error = md_get_uint16le(mdp, &length);
    if (error) {
        goto bad;
    }
    if (length != 9) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)length);
        error = EBADRPC;
        goto bad;
    }
    
    /* Get Session Flags and return them */
    error = md_get_uint16le(mdp, session_flags);
    if (error) {
        goto bad;
    }
    
    /* Get Security Blob offset and length */
    error = md_get_uint16le(mdp, &sec_buf_offset);
    if (error) {
        goto bad;
    }
    
    error = md_get_uint16le(mdp, &sec_buf_len);
    if (error) {
        goto bad;
    }
    
    /* 
     * Security buffer offset if from the beginning of SMB2 Header
     * Calculate how much further we have to go to get to it.
     */
    sec_buf_offset -= SMB2_HDRLEN;
    sec_buf_offset -= 8;   /* already parse 8 bytes worth of the response */
    
    if (sec_buf_offset > 0) {
        error = md_get_mem(mdp, NULL, sec_buf_offset, MB_MSYSTEM);
        if (error) {
            goto bad;
        }
    }
    
    /*
     * Set the gss token from the server
     */
    gp->gss_tokenlen = sec_buf_len;
    if (sec_buf_len) {
        SMB_MALLOC(gp->gss_token, uint8_t *, gp->gss_tokenlen, M_SMBTEMP, M_WAITOK);
        if (gp->gss_token == NULL) {
            error = ENOMEM;
            goto bad;
        }
        error = md_get_mem(mdp, (caddr_t) gp->gss_token, gp->gss_tokenlen, MB_MSYSTEM);
        if (error) {
            goto bad;
        }
    } else {
        gp->gss_token = NULL;
    }
    
bad:	
	smb_rq_done(rqp);
    
    if (error) {
        /*
         * If Session Setup failed, then the session_id is no longer valid
         * and have to get a new one to use
         */
        vcp->vc_session_id = 0;
    }
    
	return (error);
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smb2_smb_ioctl(struct smb_share *share, void *arg_ptr,
               struct smb_rq **compound_rqp, vfs_context_t context)
{
    struct smb2_ioctl_rq *ioctlp = arg_ptr;
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
    uint16_t input_buffer_offset;
    SMB2FID smb2_fid;
	uint16_t reparse_len;
	uint16_t substitute_name_offset, substitute_name_len;
	uint16_t print_name_offset, print_name_len;
    uint32_t input_count;
 	struct smb2_get_dfs_referral *dfs_referral;
    uint32_t input_len;

resend:
    /* Allocate request and header for an IOCTL */
    error = smb2_rq_alloc(SSTOCP(share), SMB2_IOCTL, NULL, context, &rqp);
    if (error) {
        return error;
    }
    
    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB2 IOCTL Request
     */
    mb_put_uint16le(mbp, 57);                       /* Struct size */
    mb_put_uint16le(mbp, 0);                        /* Reserved */
    mb_put_uint32le(mbp, ioctlp->ctl_code);         /* Ctl code */

    if ((ioctlp->ctl_code != FSCTL_DFS_GET_REFERRALS) && 
        (ioctlp->ctl_code != FSCTL_PIPE_WAIT)) {
        /* map fid to smb2 fid */
        error = smb_fid_get_kernel_fid(share, ioctlp->fid, 0, &smb2_fid);
        if (error) {
            goto bad;
        }
        mb_put_uint64le(mbp, smb2_fid.fid_persistent); /* FID */
        mb_put_uint64le(mbp, smb2_fid.fid_volatile);   /* FID */
    }
    else {
        /* must be -1 */
        mb_put_uint64le(mbp, -1);                   /* FID */
        mb_put_uint64le(mbp, -1);                   /* FID */
    }

    switch(ioctlp->ctl_code) {
        case FSCTL_DFS_GET_REFERRALS:
            dfs_referral = (struct smb2_get_dfs_referral *) ioctlp->snd_input_buffer;
            
            /*
             * Request has
             * (1) MaxReferralLevel (UInt16)
             * (2) RequestFileName (variable) in UTF16 and NULL terminated.
             *
             * Note: dfs_referral->file_name_len is already in network format
             * (ie its already in UTF16 format) by the time we get here.
             */
            input_len = 2;                              /* Max ref levels */
            input_len += dfs_referral->file_name_len;   /* Add file name len */
            input_len += 2;                             /* Add NULL */

            input_buffer_offset = SMB2_HDRLEN;
            input_buffer_offset += 56;
            mb_put_uint32le(mbp, input_buffer_offset);  /* Input offset */
            mb_put_uint32le(mbp, input_len);            /* Input count */
            mb_put_uint32le(mbp, 0);                    /* Max input resp */
            mb_put_uint32le(mbp, 0);                    /* Output offset */
            mb_put_uint32le(mbp, 0);                    /* Output count */
            mb_put_uint32le(mbp, ioctlp->rcv_output_len); /* Max output resp */
            mb_put_uint32le(mbp, SMB2_IOCTL_IS_FSCTL);  /* Flags */
            mb_put_uint32le(mbp, 0);                    /* Reserved2 */
            
            /* 
             * Get DFS Referral Info 
             */
            
            /* Max referral levels */
            mb_put_uint16le(mbp, dfs_referral->max_referral_level);
            
            /* 
             * File name is already in network format, just copy it in.
             */
            error = mb_put_mem(mbp, (char *) dfs_referral->file_namep,
                               dfs_referral->file_name_len, MB_MSYSTEM);
            if (error) {
                goto bad;
            }
            
            /* RequestFileName needs to be NULL terminated, so add NULL */
            mb_put_uint16le(mbp, 0);    
            
            break;

        case FSCTL_GET_REPARSE_POINT:
            mb_put_uint32le(mbp, 0);                    /* Input offset */
            mb_put_uint32le(mbp, 0);                    /* Input count */
            mb_put_uint32le(mbp, 0);                    /* Max input resp */
            mb_put_uint32le(mbp, 0);                    /* Output offset */
            mb_put_uint32le(mbp, 0);                    /* Output count */
            mb_put_uint32le(mbp, ioctlp->rcv_output_len); /* Max output resp */
            mb_put_uint32le(mbp, SMB2_IOCTL_IS_FSCTL);  /* Flags */
            mb_put_uint32le(mbp, 0);                    /* Reserved2 */
            break;
            
        case FSCTL_PIPE_TRANSCEIVE:
            /* For Pipe Transceive, the pipe data is in input fields */
            if (ioctlp->snd_input_len == 0) {
                SMBERROR("Pipe transceive needs > 0 data\n");
                error = EBADRPC;
                goto bad;
            }
            input_buffer_offset = SMB2_HDRLEN;
            input_buffer_offset += 56;
            mb_put_uint32le(mbp, input_buffer_offset);  /* Input offset */
            mb_put_uint32le(mbp, ioctlp->snd_input_len);/* Input count */
            mb_put_uint32le(mbp, 0);                    /* Max input resp */
            mb_put_uint32le(mbp, 0);                    /* Output offset */
            mb_put_uint32le(mbp, 0);                    /* Output count */
            mb_put_uint32le(mbp, ioctlp->rcv_output_len); /* Max output resp */
            mb_put_uint32le(mbp, SMB2_IOCTL_IS_FSCTL);  /* Flags */
            mb_put_uint32le(mbp, 0);                    /* Reserved2 */
            
            /* Copy pipe data into the Buffer */
            error = mb_put_uio(mbp, ioctlp->snd_input_uio, 
                               ioctlp->snd_input_len);  /* Pipe data */
            if (error) {
                goto bad;
            }
            break;
            
        case FSCTL_SET_REPARSE_POINT:
            /* 
             * Build Symbolic Link Reparse Data into the Buffer 
             * path should be in snd_input_buffer
             * path len should be in snd_input_buffer_len
             */
            substitute_name_offset = 0;
            substitute_name_len = ioctlp->snd_input_len;
            print_name_offset = substitute_name_len;
            print_name_len = substitute_name_len;
            
            /* reparse_len starts from Substitute Name Offset, thus the 12 */
            reparse_len = substitute_name_len + print_name_len + 12;
            
            /* 
             * input_count has to add in reparse tag, data len, and reserved
             * which add up to be 8 more bytes
             */
            input_count = reparse_len + 8;
            
            mb_put_uint32le(mbp, 120);                  /* Input offset */
            mb_put_uint32le(mbp, input_count);          /* Input count */
            mb_put_uint32le(mbp, 0);                    /* Max input resp */
            mb_put_uint32le(mbp, 0);                    /* Output offset */
            mb_put_uint32le(mbp, 0);                    /* Output count */
            mb_put_uint32le(mbp, 0);                    /* Max output resp */
            mb_put_uint32le(mbp, SMB2_IOCTL_IS_FSCTL);  /* Flags */
            mb_put_uint32le(mbp, 0);                    /* Reserved2 */
            
            mb_put_uint32le(mbp, IO_REPARSE_TAG_SYMLINK);   /* Reparse Tag */
            mb_put_uint16le(mbp, reparse_len);              /* Reparse Data Len */
            mb_put_uint16le(mbp, 0);                        /* Reserved */
            mb_put_uint16le(mbp, substitute_name_offset);   /* Sub Name Offset */
            mb_put_uint16le(mbp, substitute_name_len);      /* Sub Name Len */
            mb_put_uint16le(mbp, print_name_offset);        /* Print Name Offset */
            mb_put_uint16le(mbp, print_name_len);           /* Print Name Len */

            /*
             * Flags can be SYMLINK_FLAG_ABSOLUTE or SYMLINK_FLAG_RELATIVE.
             * If the path starts with a slash assume its absolute otherwise 
             * it must be relative.
             *
             * Note: path has already been converted to network format
             */
            if (*ioctlp->snd_input_buffer == '\\') {
                mb_put_uint32le(mbp, SYMLINK_FLAG_ABSOLUTE);
            } 
            else {
                mb_put_uint32le(mbp, SYMLINK_FLAG_RELATIVE);
            }	
            
            /* Fill in PathBuffer */
            mb_put_mem(mbp, (char *) ioctlp->snd_input_buffer,
                       substitute_name_len, MB_MSYSTEM);
            mb_put_mem(mbp, (char *) ioctlp->snd_input_buffer,
                       print_name_len, MB_MSYSTEM);
            break;
            
        case FSCTL_SRV_COPYCHUNK:
            mb_put_uint32le(mbp, 120);                  /* Input offset */
            mb_put_uint32le(mbp, ioctlp->snd_input_len); /* Input count */
            mb_put_uint32le(mbp, 0);                    /* Max input resp */
            mb_put_uint32le(mbp, 0);                    /* Output offset */
            mb_put_uint32le(mbp, 0);                    /* Output count */
            mb_put_uint32le(mbp, ioctlp->rcv_output_len); /* Max output resp */
            mb_put_uint32le(mbp, SMB2_IOCTL_IS_FSCTL);  /* Flags */
            mb_put_uint32le(mbp, 0);                    /* Reserved2 */
            
            /* Fill in copychunk buffer */
            mb_put_mem(mbp, (char *) ioctlp->snd_input_buffer,
                       ioctlp->snd_input_len, MB_MSYSTEM);
            
            if (SSTOVC(share)->vc_misc_flags & SMBV_OSX_SERVER) {
                /* Tell iod not to timeout this request */
                rqp->sr_flags |= SMBR_NO_TIMEOUT;
            }
            break;
            
        case FSCTL_SRV_REQUEST_RESUME_KEY:
            mb_put_uint32le(mbp, 0);                    /* Input offset */
            mb_put_uint32le(mbp, 0);                    /* Input count */
            mb_put_uint32le(mbp, 0);                    /* Max input resp */
            mb_put_uint32le(mbp, 0);                    /* Output offset */
            mb_put_uint32le(mbp, 0);                    /* Output count */
            mb_put_uint32le(mbp, ioctlp->rcv_output_len); /* Max output resp */
            mb_put_uint32le(mbp, SMB2_IOCTL_IS_FSCTL);  /* Flags */
            mb_put_uint32le(mbp, 0);
            break;

        default:
            SMBERROR("Unsupported ioctl: %d\n", ioctlp->ctl_code);
            error = EBADRPC;
            goto bad;
    }

    if (compound_rqp != NULL) {
        /* 
         * building a compound request, add padding to 8 bytes and just
         * return this built request.
         */
        smb2_rq_align8(rqp);
        rqp->sr_flags |= SMBR_COMPOUND_RQ;
        *compound_rqp = rqp;
        return (0);
    }

    error = smb_rq_simple(rqp);
    ioctlp->ret_ntstatus = rqp->sr_ntstatus;
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            goto resend;
        }

        if ( (ioctlp->ctl_code == FSCTL_SRV_COPYCHUNK) &&
             (error == EINVAL) ) {
            /*
             * smb2fs_smb_copychunks() needs information returned in the
             * IOCTL response buffer to handle the error.  For example the
             * response buffer contains the server's max-bytes-per-chunk limit.
             * See <rdar://problem/14750992>.
             */
            smb_rq_getreply(rqp, &mdp);
            smb2_smb_parse_ioctl(mdp, ioctlp);
        }
        goto bad;
    }

    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    error = smb2_smb_parse_ioctl(mdp, ioctlp);
    if (error) {
        goto bad;
    }

bad:    
	smb_rq_done(rqp);
    return error;
}

int
smb2_smb_lease_break_ack(struct smb_share *share, uint64_t lease_key_hi, uint64_t lease_key_low,
                         uint32_t lease_state, uint32_t *ret_lease_state, vfs_context_t context)
{
    struct smb_rq *rqp;
    struct mbchain *mbp;
    struct mdchain *mdp;
    int error;
    uint16_t length;
    uint64_t rsp_lease_key_hi, rsp_lease_key_low;
    
resend:
    *ret_lease_state = 0;

    /* Allocate request and header for a Lease Break Acknowledgement */
    error = smb2_rq_alloc(SSTOCP(share), SMB2_OPLOCK_BREAK, NULL, context, &rqp);
    if (error) {
        return error;
    }
    
    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB2 Lease Break Acknowledgement
     */
    mb_put_uint16le(mbp, 36);                   /* Struct size */
    mb_put_uint16le(mbp, 0);                    /* Reserved */
    mb_put_uint32le(mbp, 0);                    /* Flags (unused) */
    mb_put_uint64le(mbp, lease_key_hi);         /* Lease Key High */
    mb_put_uint64le(mbp, lease_key_low);        /* Lease Key Low */
    mb_put_uint32le(mbp, lease_state);         /* Lease State */
    mb_put_uint64le(mbp, 0);                    /* Lease Duration (unused) */
    
    error = smb_rq_simple(rqp);
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            goto resend;
        }
        
        goto bad;
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    /*
     * Parse SMB2 Lease Break Acknowledgement
     * We are already pointing to begining of Response data
     */
    
    /* Check structure size is 36 */
    error = md_get_uint16le(mdp, &length);
    if (error) {
        goto bad;
    }
    if (length != 36) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)length);
        error = EBADRPC;
        goto bad;
    }
    
    /* Get Reserved */
    error = md_get_uint16le(mdp, NULL);
    if (error) {
        goto bad;
    }
    
    /* Get Flags (ignored) */
    error = md_get_uint32le(mdp, NULL);
    if (error) {
        goto bad;
    }

    /* Get Lease Key */
    error = md_get_uint64le(mdp, &rsp_lease_key_hi);
    if (error) {
        goto bad;
    }
    
    error = md_get_uint64le(mdp, &rsp_lease_key_low);
    if (error) {
        goto bad;
    }
    
    if ((lease_key_hi != rsp_lease_key_hi) ||
        (lease_key_low != rsp_lease_key_low)) {
        SMBERROR("Lease key mismatch: 0x%llx:0x%llx != 0x%llx:0x%llx\n",
                 lease_key_hi, lease_key_low,
                 rsp_lease_key_hi, rsp_lease_key_low);
        error = EBADRPC;
        goto bad;
    }

    /* Get Lease State */
    error = md_get_uint32le(mdp, ret_lease_state);
    if (error) {
        goto bad;
    }

    /* Get Lease Duration (ignored) */
    error = md_get_uint64le(mdp, NULL);
    if (error) {
        goto bad;
    }

bad:
    smb_rq_done(rqp);
    return error;
}

int
smb2_smb_lock(struct smb_share *share, int op, SMBFID fid,
              off_t offset, uint64_t length, vfs_context_t context)
{
    int error;
    struct smb_rq *rqp = NULL;
    struct mbchain *mbp;
    struct mdchain *mdp;
    SMB2FID smb2_fid;
    uint32_t flags = SMB2_LOCKFLAG_FAIL_IMMEDIATELY; /* %%% Correct ??? */
    uint16_t len, reserved_uint16;
    
    switch (op) {
        case SMB_LOCK_EXCL:
            flags |= SMB2_LOCKFLAG_EXCLUSIVE_LOCK;
            break;
            
        case SMB_LOCK_SHARED:
            flags |= SMB2_LOCKFLAG_SHARED_LOCK;
            break;
            
        case SMB_LOCK_RELEASE:
            flags = 0;  /* cant have any other flags set with unlock */
            flags |= SMB2_LOCKFLAG_UNLOCK;
            break;

        default:
            SMBERROR("Unknown lock type %d\n", op);
            error = EINVAL;
            goto bad;
    }

resend:
    /* Allocate request and header for a Lock */
    error = smb2_rq_alloc(SSTOCP(share), SMB2_LOCK, NULL, context, &rqp);
    if (error) {
        return error;
    }
    
    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB2 Lock Request
     */
    mb_put_uint16le(mbp, 48);       /* Struct size */
    mb_put_uint16le(mbp, 1);        /* LockCount */
    mb_put_uint32le(mbp, 0);        /* LockSequence %%% Should this be something unique??? */
    
    /* map fid to smb2 fid */
    error = smb_fid_get_kernel_fid(share, fid, 0, &smb2_fid);
    if (error) {
        goto bad;
    }
    mb_put_uint64le(mbp, smb2_fid.fid_persistent);      /* FID */
    mb_put_uint64le(mbp, smb2_fid.fid_volatile);        /* FID */
    
    mb_put_uint64le(mbp, offset);                       /* Offset */
    mb_put_uint64le(mbp, length);                       /* Length */
    mb_put_uint32le(mbp, flags);                        /* Flags */
    mb_put_uint32le(mbp, 0);                            /* Reserved */

    error = smb_rq_simple(rqp);
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            goto resend;
        }

        goto bad;
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    /* 
     * Parse SMB2 Lock Response 
     * We are already pointing to begining of Response data
     */
    
    /* Check structure size is 4 */
    error = md_get_uint16le(mdp, &len);
    if (error) {
        goto bad;
    }
    if (len != 4) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)length);
        error = EBADRPC;
        goto bad;
    }
    
    /* Get Reserved */
    error = md_get_uint16le(mdp, &reserved_uint16);
    if (error) {
        goto bad;
    }
    
bad:    
    if (rqp != NULL) {
        smb_rq_done(rqp);
    }
    return error;
}

static int
smb2_smb_logoff(struct smb_vc *vcp, vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
	uint16_t length;
    
resend:
    /* Allocate request and header for a Logoff */
	error = smb2_rq_alloc(VCTOCP(vcp), SMB2_LOGOFF, NULL, context, &rqp);
	if (error)
		return error;
    
    /* 
     * Fill in Logoff part 
     * Dont use struct ptr since its only 4 bytes long
     */
    smb_rq_getrequest(rqp, &mbp);
    
    mb_put_uint16le(mbp, 4);       /* Struct size */
    mb_put_uint16le(mbp, 0);       /* Reserved */
    
    error = smb_rq_simple(rqp);
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            goto resend;
        }

        goto bad;
    }
    
    /* Always assume the log off worked and clear out session id */
    vcp->vc_session_id = 0;
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    /* 
     * Parse SMB2 Logoff 
     * We are already pointing to begining of Response data
     */
    
    /* Check structure size is 4 */
    error = md_get_uint16le(mdp, &length);
    if (error) {
        goto bad;
    }
    if (length != 4) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)length);
        error = EBADRPC;
        goto bad;
    }
    
bad:
	smb_rq_done(rqp);
	return error;
}

int
smb_smb_ssnclose(struct smb_vc *vcp, vfs_context_t context)
{
	int error;
    
	if (vcp->vc_smbuid == SMB_UID_UNKNOWN)
		return 0;
    
	if (smb_smb_nomux(vcp, __FUNCTION__, context) != 0)
		return EINVAL;
    
    if (vcp->vc_flags & SMBV_SMB2) {
        error = smb2_smb_logoff(vcp, context);
    }
    else {
        error = smb1_smb_ssnclose(vcp, context);
    }
    return (error);
}

int
smb2_smb_negotiate(struct smb_vc *vcp, struct smb_rq *in_rqp, int inReconnect,
                   vfs_context_t user_context, vfs_context_t context)
{
	struct smb_sopt *sp = NULL;
	struct smb_rq *rqp = NULL;
	struct mbchain *mbp;
	int error;
	uint32_t original_caps;
    uint8_t *guidp;
    uint32_t capabilities = 0;
    
    /*
     * Init some vars
     */
    vcp->vc_hflags = SMB_FLAGS_CASELESS;
	/* Leave SMB_FLAGS2_UNICODE "off" - no need to do anything */ 
	vcp->vc_hflags2 |= SMB_FLAGS2_ERR_STATUS;
	sp = &vcp->vc_sopt;
	original_caps = sp->sv_caps;
    
    if (in_rqp != NULL) {
        /* 
         * Parse SMB2.x Neg Response that we got from our SMB1 Neg Request 
         */
        error = smb2_smb_parse_negotiate(vcp, in_rqp, 1);
        if (error) {
            SMBERROR("smb2_smb_negotiate_parse for SMB1 failed %d\n", error);
            goto bad;
        }
        
        /* See if we got SMB 2.002 or SMB 2.1 */
        if ((sp->sv_dialect == SMB2_DIALECT_0202) ||
            (sp->sv_dialect == SMB2_DIALECT_0210)) {
            /* 
             * For SMB 2.002 or 2.1, we are done with the Neg and can go
             * directly to the SessionSetup phase
             */
            if (sp->sv_dialect == SMB2_DIALECT_0202) {
                vcp->vc_flags |= SMBV_SMB2002;
                SMBDEBUG("SMB 2.002 DIALECT\n");
            }
            else {
                vcp->vc_flags |= SMBV_SMB21;
                SMBDEBUG("SMB 2.1 DIALECT\n");
            }
            
            goto do_session_setup;
        }
        
        /* 
         * For SMB2.1 and later, start over with a SMB2 Negotiate request from 
         * client since the client SMB2 Negotiate will have information that
         * server will need.
         */
    }
    
resend:
    /* Allocate request and header for a Negotiate */
    error = smb2_rq_alloc(VCTOCP(vcp), SMB2_NEGOTIATE, NULL, context, &rqp);
    if (error) {
        return error;
    }
    
    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB2 Negotiate Request
     */
	mb_put_uint16le(mbp, 36);                               /* Struct Size */

    if (in_rqp != NULL) {
        /* only put in the SMB 2.x version the server told us it supports */
        mb_put_uint16le(mbp, 1);                            /* Dialect Count */
    }
    else {
        /* put in all SMB 2.x versions we support (hard coded to 2 dialects) */
        mb_put_uint16le(mbp, 2);                            /* Dialect Count */
    }
	/* Security Mode */
	if (vcp->vc_misc_flags & SMBV_CLIENT_SIGNING_REQUIRED) {
		mb_put_uint16le(mbp, (SMB2_NEGOTIATE_SIGNING_ENABLED | SMB2_NEGOTIATE_SIGNING_REQUIRED));
	}
	else {
		mb_put_uint16le(mbp, SMB2_NEGOTIATE_SIGNING_ENABLED);
	}
	mb_put_uint16le(mbp, 0);                                /*  Reserved */
    
    /* 
     * When we support SMB 3.x, fill in the Capabilities field 
     */
#if 0 
    capabilities |= (SMB2_GLOBAL_CAP_DFS |
                     SMB2_GLOBAL_CAP_LARGE_MTU |
                     SMB2_GLOBAL_CAP_ENCRYPTION);
#endif
    
	mb_put_uint32le(mbp, capabilities);                     /* Capabilities */
    
    guidp = (uint8_t *) mb_reserve(mbp, 16);                /* Client GUID */
    memcpy(guidp, vcp->vc_client_guid, 16);
    
	mb_put_uint64le(mbp, 0);                                /* Start Time */

    if (in_rqp != NULL) {
        /* only put in the SMB 2.x version the server told us it supports */
        if (vcp->vc_flags & SMBV_SMB2002) {
            mb_put_uint16le(mbp, SMB2_DIALECT_0202);        /* 2.002 Dialect */
        }
        else {
            mb_put_uint16le(mbp, SMB2_DIALECT_0210);        /* 2.1 Dialect */
        }
    }
    else {
        /* only support two dialects of SMB 2.x at this time */
        mb_put_uint16le(mbp, SMB2_DIALECT_0202);            /* 2.002 Dialect */
        mb_put_uint16le(mbp, SMB2_DIALECT_0210);            /* 2.1 Dialect */
    }
    
    error = smb_rq_simple(rqp);
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            goto resend;
        }
        
        goto bad;
    }
    
    /* 
     * Parse SMB2.x Neg Response that we got from our SMB2.x Neg Request 
     */
    error = smb2_smb_parse_negotiate(vcp, rqp, 0);
    if (error) {
        SMBERROR("smb2_smb_negotiate_parse failed %d\n", error);
        goto bad;
    }
    
do_session_setup:
    /* If no token then say we have no length */
    if (vcp->negotiate_token == NULL) {
        vcp->negotiate_tokenlen = 0;
    }
    error = smb_gss_negotiate(vcp, user_context);
    if (error) {
        SMBERROR("smb_gss_negotiate error %d\n", error);
        goto bad;
    }
    
    /* Check max read/write sizes */
    vcp->vc_rxmax = smb2_vc_maxread(vcp);
    vcp->vc_wxmax = smb2_vc_maxwrite(vcp);
    vcp->vc_txmax = smb2_vc_maxtransact(vcp);
    
	/* When doing a reconnect we never allow them to change the encode */
	if (inReconnect) {
        /* this is set via AAPL, so clear it from the original_caps */
        original_caps &= ~SMB_CAP_UNIX;
        
		if (original_caps != sp->sv_caps)
			SMBWARNING("Reconnecting with different sv_caps %x != %x\n", original_caps, sp->sv_caps);
	}
    
bad:
	if (rqp != NULL) {
        smb_rq_done(rqp);  
    }
	return error;
}

int 
smb_smb_negotiate(struct smb_vc *vcp, vfs_context_t user_context, 
                  int inReconnect, vfs_context_t context)
{
    int error = EINVAL;
    
    if (inReconnect) {
        /* Doing reconnect:
         * If using SMB2, then stay with SMB2
         * If SMB1, then stay with SMB1
         */
        if (vcp->vc_flags & SMBV_SMB2) {
            SMBDEBUG("Reconnecting SMB 2.x only\n");
            /*
             * For SMB2 Only Negotiate and not the Multi Protocol
             * Negotiate, message_id has to start with 0.
             */
            vcp->vc_message_id = 0;
            error = smb2_smb_negotiate(vcp, NULL, inReconnect,
                                       user_context, context);
        }
        else {
            SMBDEBUG("Reconnecting SMB 1.x only\n");
            error = smb1_smb_negotiate(vcp, user_context, inReconnect, 1, 
                                       context);
        }
    }
    else {
        /* Not in reconnect: */
        if (!(vcp->vc_misc_flags & (SMBV_NEG_SMB1_ONLY)) &&
            !(vcp->vc_misc_flags & (SMBV_NEG_SMB2_ONLY))) {
            /* Do normal SMB 1.x negotiate and try to negotiate to SMB 2.x */
            error = smb1_smb_negotiate(vcp, user_context, inReconnect, 0, 
                                       context);
        }
        else {
            if (vcp->vc_misc_flags & SMBV_NEG_SMB1_ONLY) {
                /* Only allow SMB 1.x */
                SMBDEBUG("SMB 1.x only\n");
                error = smb1_smb_negotiate(vcp, user_context, inReconnect, 1, 
                                           context);
            }
            else {
                if (vcp->vc_misc_flags & SMBV_NEG_SMB2_ONLY) {
                    /*
                     * For SMB2 Only Negotiate and not the Multi Protocol
                     * Negotiate, message_id has to start with 0.
                     */
                    vcp->vc_message_id = 0;
                    SMBDEBUG("SMB 2.x only\n");
                    error = smb2_smb_negotiate(vcp, NULL, inReconnect,
                                               user_context, context);
                }
            }
        }
    }
        
    return (error);
}

int
smb2_smb_parse_change_notify(struct smb_rq *rqp, uint32_t *events)
{
	int error;
	uint16_t length;
    uint16_t output_buffer_offset;
    uint32_t output_buffer_len;
	struct mdchain *mdp;
    uint32_t next_entry_offset, action;
    
    *events = 0;
    
    /*
     * Change Notify is an Async request, get reply and parse it now
     */
    error = smb_rq_reply(rqp);
    if (error) {
        SMBERROR("smb_rq_reply failed %d\n", error);
        goto bad;
    }

    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
        
    /* 
     * Parse SMB2 Change Notify Response 
     */
    
    /* Check structure size is 9 */
    error = md_get_uint16le(mdp, &length);
    if (error) {
        goto bad;
    }
    if (length != 9) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)length);
        error = EBADRPC;
        goto bad;
    }
    
    /* Get Output buffer offset */
    error = md_get_uint16le(mdp, &output_buffer_offset);
    if (error) {
        goto bad;
    }
    
    /* Get Output buffer len */
    error = md_get_uint32le(mdp, &output_buffer_len);
    if (error) {
        goto bad;
    }
    
    /* 
     * Output buffer offset is from the beginning of SMB2 Header
     * Calculate how much further we have to go to get to it.
     */
    output_buffer_offset -= SMB2_HDRLEN;
    output_buffer_offset -= 8;   /* already parsed 8 bytes worth of the response */
    
    if (output_buffer_offset > 0) {
        error = md_get_mem(mdp, NULL, output_buffer_offset, MB_MSYSTEM);
        if (error) {
            goto bad;
        }
    }
    
    /* 
     * Parse the Array of FILE_NOTIFY_INFORMATION
     *
	 * Note that the server doesn't have to return any data, so no next offset 
     * field is not an error.
	 */
	if (output_buffer_len && (md_get_uint32le(mdp, &next_entry_offset) == 0)) {
		do {
			/* since we already moved pass next offset don't count it */
			if (next_entry_offset >= sizeof(uint32_t)) {
				next_entry_offset -= (uint32_t) sizeof(uint32_t);
            }
			
			error = md_get_uint32le(mdp, &action);				
			if (error) {
				break;
            }
			
			/* since we already moved pass action don't count it */
			if (next_entry_offset >= sizeof(uint32_t)) {
				next_entry_offset -= (uint32_t)sizeof(uint32_t);
            }
            
            /* Ignore the FileNameLength and FileName1 */
			if (next_entry_offset) {
				error = md_get_mem(mdp, NULL, next_entry_offset, MB_MSYSTEM);
				if (!error) {
					error = md_get_uint32le(mdp, &next_entry_offset);
                }
				if (error) {
					break;
                }
			}
			
			switch (action) {
				case FILE_ACTION_ADDED:
					*events |= VNODE_EVENT_FILE_CREATED | VNODE_EVENT_DIR_CREATED;
					break;
				case FILE_ACTION_REMOVED:
					*events |= VNODE_EVENT_FILE_REMOVED | VNODE_EVENT_DIR_REMOVED;
					break;
				case FILE_ACTION_MODIFIED:
					*events |= VNODE_EVENT_ATTRIB;
					break;
				case FILE_ACTION_RENAMED_OLD_NAME:
				case FILE_ACTION_RENAMED_NEW_NAME:
					*events |= VNODE_EVENT_RENAME;
					break;
				case FILE_ACTION_ADDED_STREAM:
				case FILE_ACTION_REMOVED_STREAM:
				case FILE_ACTION_MODIFIED_STREAM:
					/* Should we try to clear all named stream cache? */
					*events |= VNODE_EVENT_ATTRIB;
					break;
				default:
					error = ENOTSUP;
					break;
			}
		} while (next_entry_offset);
    }
	
	if (error || (*events == 0)) {
		*events = VNODE_EVENT_ATTRIB | VNODE_EVENT_WRITE;
    }
    
	if (error) {
		SMBWARNING("error = %d\n", error);
	}

bad:    
    return error;
}

int
smb2_smb_parse_close(struct mdchain *mdp, struct smb2_close_rq *closep)
{
	int error, tmp_error;
    uint16_t flags, ret_flags;
    uint32_t reservedInt;
    uint16_t length;
    SMB2FID smb2_fid; 

    /* 
     * Parse SMB2 Close Response 
     * We are already pointing to begining of Response data
     */
    
    flags = closep->flags;          /* only take lower 16 bits for now */

    /* Check structure size is 60 */
    error = md_get_uint16le(mdp, &length);
    if (error) {
        goto bad;
    }
    if (length != 60) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)length);
        error = EBADRPC;
        goto bad;
    }
    
    /* Get Flags */
    error = md_get_uint16le(mdp, &ret_flags);
    if (error) {
        goto bad;
    }
    
    /* We asked for attributes, did the server give them to us? */
    if ((flags & SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB) && 
        !(ret_flags & SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB)) {
        SMBERROR("Bad SMB2 Server, did not return close attributes\n");
        error = EBADRPC;
        goto bad;
    }
    
    /* Get the attributes */
    if (ret_flags & SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB) {
        /* Get Reserved byte */
        error = md_get_uint32le(mdp, &reservedInt);
        if (error) {
            goto bad;
        }
        
        /* Get Creation Time */
        error = md_get_uint64le(mdp, &closep->ret_create_time);
        if (error) {
            goto bad;
        }
        
        /* Get Last Access Time */
        error = md_get_uint64le(mdp, &closep->ret_access_time);
        if (error) {
            goto bad;
        }
        
        /* Get Last Write Time */
        error = md_get_uint64le(mdp, &closep->ret_write_time);
        if (error) {
            goto bad;
        }
        
        /* Get Change Time */
        error = md_get_uint64le(mdp, &closep->ret_change_time);
        if (error) {
            goto bad;
        }
        
        /* Get Allocation Size */
        error = md_get_uint64le(mdp, &closep->ret_alloc_size);
        if (error) {
            goto bad;
        }
        
        /* Get EOF */
        error = md_get_uint64le(mdp, &closep->ret_eof);
        if (error) {
            goto bad;
        }
        
        /* Get File Attributes */
        error = md_get_uint32le(mdp, &closep->ret_attributes);
        if (error) {
            goto bad;
        }
    }
    
bad:    
    /* 
     * If we are calling the parse close code, then the close reply had no
     * error in the status, so go ahead and remove the fid
     */
    tmp_error = smb_fid_get_kernel_fid(closep->share, closep->fid, 1, 
                                       &smb2_fid);
    if (tmp_error) {
        SMBERROR("Failed to remove fid from internal table\n");
    }
    
    return error;
}

static int
smb2_smb_parse_copychunk_response(struct mdchain *mdp,
                                  struct smb2_ioctl_rq *ioctlp)
{
	int     error = 0;
    char    *copychunk_resp = NULL;
    
    SMB_MALLOC(copychunk_resp, char *, (size_t) ioctlp->ret_output_len, M_TEMP,
               M_WAITOK | M_ZERO);
    if (copychunk_resp == NULL) {
        error = ENOMEM;
        goto bad;
    }
    
    error = md_get_mem(mdp, (void *) copychunk_resp,
                       (size_t) ioctlp->ret_output_len, MB_MSYSTEM);
    if (!error) {
		ioctlp->rcv_output_buffer = (uint8_t *) copychunk_resp;
        ioctlp->rcv_output_len = (uint32_t) ioctlp->ret_output_len;
    }
bad:
    return (error);
}

int
smb2_smb_parse_create(struct smb_share *share, struct mdchain *mdp, 
                      struct smb2_create_rq *createp)
{
	int error;
	uint16_t length;
    uint8_t reservedByte;
    uint32_t reservedInt;
	uint32_t ret_context_offset;
	uint32_t ret_context_length;
    SMB2FID smb2_fid;
    struct smb_vc *vcp = SSTOVC(share);

    /*
     * Parse SMB2 Create Response 
     * We are already pointing to begining of Response data
     */
    
    /* Check structure size is 89 */
    error = md_get_uint16le(mdp, &length);
    if (error) {
        goto bad;
    }
    if (length != 89) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)length);
        error = EBADRPC;
        goto bad;
    }
    
    /* Get Oplock level granted */
    error = md_get_uint8(mdp, &createp->ret_oplock_level);
    if (error) {
        goto bad;
    }
    
    /* Get Reserved byte */
    error = md_get_uint8(mdp, &reservedByte);
    if (error) {
        goto bad;
    }
    
    /* Get Create Action */
    error = md_get_uint32le(mdp, &createp->ret_create_action);
    if (error) {
        goto bad;
    }
    
    /* Get Creation Time */
    error = md_get_uint64le(mdp, &createp->ret_create_time);
    if (error) {
        goto bad;
    }
    
    /* Get Last Access Time */
    error = md_get_uint64le(mdp, &createp->ret_access_time);
    if (error) {
        goto bad;
    }
    
    /* Get Last Write Time */
    error = md_get_uint64le(mdp, &createp->ret_write_time);
    if (error) {
        goto bad;
    }
    
    /* Get Change Time */
    error = md_get_uint64le(mdp, &createp->ret_change_time);
    if (error) {
        goto bad;
    }
    
    /* Get Allocation Size */
    error = md_get_uint64le(mdp, &createp->ret_alloc_size);
    if (error) {
        goto bad;
    }
    
    /* Get EOF */
    error = md_get_uint64le(mdp, &createp->ret_eof);
    if (error) {
        goto bad;
    }
    
    /* Get File Attributes */
    error = md_get_uint32le(mdp, &createp->ret_attributes);
    if (error) {
        goto bad;
    }
    
    /* Get Reserved */
    error = md_get_uint32le(mdp, &reservedInt);
    if (error) {
        goto bad;
    }
    
    /* Get SMB2 File ID and create user fid to return */
    error = md_get_uint64le(mdp, &smb2_fid.fid_persistent);
    if (error) {
        goto bad;
    }
    error = md_get_uint64le(mdp, &smb2_fid.fid_volatile);
    if (error) {
        goto bad;
    }
    
    /* Get Context Offset */
    error = md_get_uint32le(mdp, &ret_context_offset);
    if (error) {
        goto bad;
    }
    
    /* Get Context Length */
    error = md_get_uint32le(mdp, &ret_context_length);
    if (error) {
        goto bad;
    }
    
    /* 
     * Context offset is from the beginning of SMB2 Header
     * Calculate how much further we have to go to get to it.
     */  

    /* give them full access and let server deny them */
    createp->ret_max_access = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;

    if (ret_context_length > 0) {
        /* Parse the Create Contexts */
        error = smb2_smb_parse_create_contexts(share, mdp,
                                               &ret_context_offset,
                                               createp);
        if (error) {
            goto bad;
        }
    }
    
    /* Did we try to see if this is an OS X server and fail? */
    if ((createp->flags & SMB2_CREATE_AAPL_QUERY) &&
        !(vcp->vc_misc_flags & SMBV_OSX_SERVER)) {
        SMBDEBUG("Found a NON OS X server\n");
        vcp->vc_misc_flags |= SMBV_OTHER_SERVER;
    }

    /* Only create user fid if the create worked */
    if (error == 0) {
        error = smb_fid_get_user_fid(share, smb2_fid, &createp->ret_fid);
        if (error) {
            goto bad;
        }
    }

bad:
    return error;
}

static int
smb2_smb_parse_create_contexts(struct smb_share *share, struct mdchain *mdp,
                               uint32_t *ret_context_offset,
                               struct smb2_create_rq *createp)
{
	int error;
    uint32_t rsp_context_next;
    uint16_t rsp_context_name_offset, rsp_context_name_len;
    uint16_t rsp_context_data_offset;
    uint32_t rsp_context_data_len;
    uint32_t rsp_context_name;
    uint32_t max_access_status, max_access;
    uint32_t sub_command, min_context_len;
    uint64_t server_bitmap = 0;
	char *local_str = NULL;
    struct smb_vc *vcp = SSTOVC(share);
    struct smb2_create_ctx_resolve_id *resolve_idp = NULL;
    struct smb2_durable_handle *dur_handlep = NULL;
    uint32_t ntstatus = 0;
    struct mdchain md_context_shadow;
    uint64_t lease_key_hi;
    uint64_t lease_key_low = 0;

    /* Read in any pad bytes */
    *ret_context_offset -= SMB2_HDRLEN;
    /* already parsed 88 bytes worth of this response */
    *ret_context_offset -= 88;
    
    if (ret_context_offset > 0) {
        error = md_get_mem(mdp, NULL, *ret_context_offset, MB_MSYSTEM);
        if (error) {
            goto bad;
        }
    }
    
    do {
        /*
         * Offsets are from the beginning of the context, so save a
         * copy of the beginning of the context. Also allows us to handle
         * malformed contexts as long as the Next field is correct.
         */
        md_shadow_copy(mdp, &md_context_shadow);

        /* 
         * Parse the Context header 
         */
        
        /* Get Context next */
        error = md_get_uint32le(&md_context_shadow, &rsp_context_next);
        if (error) {
            goto bad;
        }
        
        /* Get Context name offset */
        error = md_get_uint16le(&md_context_shadow, &rsp_context_name_offset);
        if (error) {
            goto bad;
        }
        
        /* Get Context name length */
        error = md_get_uint16le(&md_context_shadow, &rsp_context_name_len);
        if (error) {
            goto bad;
        }
        if (rsp_context_name_len != 4) {
            SMBERROR("Illegal context name length: %u\n",
                     rsp_context_name_len);
            error = EBADRPC;
            goto bad;
        }
        
        /* Get reserve bytes and ignore them */
        error = md_get_uint16le(&md_context_shadow, NULL);
        if (error) {
            goto bad;
        }
        
        /* Get Context data offset */
        error = md_get_uint16le(&md_context_shadow, &rsp_context_data_offset);
        if (error) {
            goto bad;
        }
        
        /* Get Context data length */
        error = md_get_uint32le(&md_context_shadow, &rsp_context_data_len);
        if (error) {
            goto bad;
        }
        
        /*
         * Get Context name - its a string constant and thus its not
         * byte swapped uint32
         */
        error = md_get_uint32be(&md_context_shadow, &rsp_context_name);
        if (error) {
            goto bad;
        }
        
        /*
         * Get reserve bytes and ignore them. Context names always seem to
         * be 4 bytes and thus there are always 4 pad bytes after them to
         * get to an 8 byte boundary.
         */
        error = md_get_uint32le(&md_context_shadow, NULL);
        if (error) {
            goto bad;
        }

        switch (rsp_context_name) {
            case SMB2_CREATE_QUERY_MAXIMAL_ACCESS:
                if (rsp_context_data_len != 8) {
                    SMBERROR("Illegal MxAc data len: %u\n",
                             rsp_context_data_len);
                    error = EBADRPC;
                    goto bad;
                }

                /* Get Max Access Query Status */
                error = md_get_uint32le(&md_context_shadow, &max_access_status);
                if (error) {
                    goto bad;
                }
                
                if (max_access_status != 0) {
                    SMBDEBUG("MxAc Query Status failed: 0x%x\n",
                             max_access_status);
                }
                else {
                    /* Get Max Access */
                    error = md_get_uint32le(&md_context_shadow, &max_access);
                    if (error) {
                        goto bad;
                    }
                    
                    createp->ret_max_access = max_access;
                }
                break;
                
            case SMB2_CREATE_REQUEST_LEASE:    /* RqLs */
                dur_handlep = createp->create_contextp;
                if (dur_handlep == NULL) {
                    SMBERROR("dur_handlep is NULL \n");
                    error = EBADRPC;
                    goto bad;
                }

                if (rsp_context_data_len != 32) {
                    SMBERROR("Illegal RqLs data len: %u\n",
                             rsp_context_data_len);
                    error = EBADRPC;
                    goto bad;
                }
                
                /* Get Lease Key */
                error = md_get_uint64le(&md_context_shadow, &lease_key_hi);
                if (error) {
                    goto bad;
                }
                
                error = md_get_uint64le(&md_context_shadow, &lease_key_low);
                if (error) {
                    goto bad;
                }
                
                if ((lease_key_hi != dur_handlep->lease_key_hi) ||
                    (lease_key_low != dur_handlep->lease_key_low)) {
                    SMBERROR("Lease key mismatch: 0x%llx:0x%llx != 0x%llx:0x%llx\n",
                             dur_handlep->lease_key_hi, dur_handlep->lease_key_low,
                             lease_key_hi, lease_key_low);
                    error = EBADRPC;
                    goto bad;
                }

                /* Get Lease State */
                error = md_get_uint32le(&md_context_shadow,
                                        &dur_handlep->lease_state);
                if (error) {
                    goto bad;
                }

                /* Get Lease Flags - always 0 */
                error = md_get_uint32le(&md_context_shadow, NULL);
                if (error) {
                    goto bad;
                }
                
                /* Get Lease Duration - always 0 */
                error = md_get_uint64le(&md_context_shadow, NULL);
                if (error) {
                    goto bad;
                }
                
                dur_handlep->flags |= SMB2_LEASE_GRANTED;
                
                break;
                
            case SMB2_CREATE_DURABLE_HANDLE_REQUEST:    /* DHnQ */
                dur_handlep = createp->create_contextp;
                if (dur_handlep == NULL) {
                    SMBERROR("dur_handlep is NULL \n");
                    error = EBADRPC;
                    goto bad;
                }

                if (rsp_context_data_len != 8) {
                    SMBERROR("Illegal DHnQ data len: %u\n",
                             rsp_context_data_len);
                    error = EBADRPC;
                    goto bad;
                }
                
                /* Get Reserved and ignore it */
                error = md_get_uint64le(&md_context_shadow, NULL);
                if (error) {
                    goto bad;
                }

                dur_handlep->flags |= SMB2_DURABLE_HANDLE_GRANTED;
                dur_handlep->flags &= ~SMB2_DURABLE_HANDLE_REQUEST;
                break;

            case SMB2_CREATE_DURABLE_HANDLE_RECONNECT:    /* DHnC */
                /* The response to a DHnC seems to be a RqLs and DHnQ reply */
                break;

            case SMB2_CREATE_AAPL:    /* AAPL */
                /*
                 * Check for min length of context.
                 * Must have CommandCode and Reserved which is 8 bytes
                 */
                min_context_len = 8;
                if (rsp_context_data_len < min_context_len) {
                    SMBERROR("Illegal AAPL data len: %u\n",
                             rsp_context_data_len);
                    error = EBADRPC;
                    goto bad;
                }

                /* Get sub command */
                error = md_get_uint32le(&md_context_shadow, &sub_command);
                if (error) {
                    goto bad;
                }
                
                /* Get reserved */
                error = md_get_uint32le(&md_context_shadow, NULL);
                if (error) {
                    goto bad;
                }
                
                switch (sub_command) {
                    case kAAPL_SERVER_QUERY:
                        /*
                         * Check for min length of context.
                         * ReplyBitMap which is 8 bytes
                         */
                        min_context_len += 8;
                        
                        if (rsp_context_data_len < min_context_len) {
                            /* Must have command and pad */
                            SMBERROR("Illegal AAPL data len (cmd/pad): %u\n",
                                     rsp_context_data_len);
                            error = EBADRPC;
                            goto bad;
                        }

                        /* Get server reply bitmap */
                        error = md_get_uint64le(&md_context_shadow, &server_bitmap);
                        if (error) {
                            goto bad;
                        }
                        
                        /*
                         * Check for min length of context.
                         * Depending on what bits are set in the ReplyBitMap
                         * will tell us what min length we need.
                         */
                        if (server_bitmap & kAAPL_SERVER_CAPS) {
                            min_context_len += 8;
                        }
                        
                        if (server_bitmap & kAAPL_VOLUME_CAPS) {
                            min_context_len += 8;
                        }
                        
                        if (server_bitmap & kAAPL_MODEL_INFO) {
                            /* Pad2 (4) + ModelStringLen (4) */
                            min_context_len += 8;
                        }
                        
                        if (rsp_context_data_len < min_context_len) {
                            SMBERROR("Illegal kAAPL_SERVER_QUERY data len: %u, reply_bitmap 0x%llx\n",
                                     rsp_context_data_len, server_bitmap);
                            error = EBADRPC;
                            goto bad;
                        }
                        
                        if (server_bitmap & kAAPL_SERVER_CAPS) {
                            /* Get server capabilites */
                            error = md_get_uint64le(&md_context_shadow, &vcp->vc_server_caps);
                            if (error) {
                                goto bad;
                            }
                        }
                        if (vcp->vc_server_caps & kAAPL_UNIX_BASED) {
                            /* Server is unix based */
                            VC_CAPS(vcp) |= SMB_CAP_UNIX;
                        }
                        
                        if (server_bitmap & kAAPL_VOLUME_CAPS) {
                            /* Get volume capabilites */
                            error = md_get_uint64le(&md_context_shadow, &vcp->vc_volume_caps);
                            if (error) {
                                goto bad;
                            }
                        }
                        
                        if (server_bitmap & kAAPL_MODEL_INFO) {
                            /* Get pad */
                            error = md_get_uint32le(&md_context_shadow, NULL);
                            if (error) {
                                goto bad;
                            }

                            smb2_smb_parse_create_str(share, &md_context_shadow,
                                                      0, &local_str);
                            if (error) {
                                goto bad;
                            }

                            if (vcp->vc_model_info) {
                                SMB_FREE(vcp->vc_model_info, M_SMBTEMP);
                            }
                            vcp->vc_model_info = local_str;
                        }
                        
                        /*
                         * Its a OS X server or at least one pretending to be
                         * one.
                         */
                        SMBDEBUG("Found an OS X server\n");
                        vcp->vc_misc_flags |= SMBV_OSX_SERVER;
                        break;
                        
                    case kAAPL_RESOLVE_ID:
                        resolve_idp = createp->create_contextp;
                        if (resolve_idp == NULL) {
                            SMBDEBUG("resolve_idp is NULL \n");
                            error = EBADRPC;
                            goto bad;
                        }
                        
                       /*
                         * Check for min length of context.
                         * resolve_id_error (4), path_len (4) = 8
                         */
                        min_context_len += 8;
                        
                        if (rsp_context_data_len < min_context_len) {
                            /* Must have command and pad */
                            SMBERROR("Illegal kAAPL_RESOLVE_ID data len (cmd/pad): %u\n",
                                     rsp_context_data_len);
                            error = EBADRPC;
                            goto bad;
                        }
                        
                        /* Get resolve ID error */
                        error = md_get_uint32le(&md_context_shadow, &ntstatus);
                        if (error) {
                            goto bad;
                        }
                        
                        /* Convert nt status to errno */
                        *resolve_idp->ret_errorp = smb_ntstatus_to_errno(ntstatus);

                        smb2_smb_parse_create_str(share, &md_context_shadow,
                                                  1, resolve_idp->ret_pathp);
                        if (error) {
                            goto bad;
                        }
                        break;

                    default:
                        SMBERROR("Unknown AAPL subcommand: %d\n", sub_command);
                        error = EBADRPC;
                        goto bad;
                        break;
                }
                
                break;
                
            default:
                SMBERROR("Unknown context name: 0x%x\n",
                         rsp_context_name);
                error = EBADRPC;
                goto bad;
                break;
        } /* end of switch */
        
        /* Move to the next context in the main mdp chain */
        if (rsp_context_next != 0) {
            error = md_get_mem(mdp, NULL, rsp_context_next, MB_MSYSTEM);
            if (error) {
                goto bad;
            }
        }
    } while (rsp_context_next != 0);

bad:
    return error;
}

static int
smb2_smb_parse_create_str(struct smb_share *share, struct mdchain *mdp,
                          uint32_t is_path, char **ret_str)
{
	int error;
    uint32_t str_len = 0;
	char *network_str = NULL;
	char *local_str = NULL;
    size_t name_size;
	size_t local_str_len;
    
    /* Get string len */
    error = md_get_uint32le(mdp, &str_len);
    if (error) {
        goto bad;
    }

    /*
     * Make sure it is something in reason. Don't allocate
     * it if it doesn't make sense.
     */
    if (str_len == 0) {
        *ret_str = NULL;
        error = 0;
        goto bad;
    }
    else {
        if (str_len >= (SMB_MAXFNAMELEN * 2)) {
            SMBERROR("Illegal string len %d\n",
                     str_len);
            error = EBADRPC;
            goto bad;
        }
    }
    
    SMB_MALLOC(network_str, char *, str_len, M_SMBFSDATA, M_WAITOK);
    if (network_str == NULL) {
        SMBERROR("Malloc failed for model string %d\n", str_len);
        error = ENOMEM;
        goto bad;
    }
    else {
        error = md_get_mem(mdp, (void *)network_str, str_len, MB_MSYSTEM);
        if (error) {
            goto bad;
        }
    }
    
    /* Convert the name to a UTF-8 string  */
    name_size = str_len;
    
    if (is_path) {
        /* 
         * Have to convert the '\' to '/'
         *
         * Converting one UTF16 char could result in up to 9 bytes of Unicode so
         * make sure to malloc len * 9 number of bytes.
         */
        local_str_len = name_size * 9 + 1;
        SMB_MALLOC(local_str, char *, local_str_len, M_TEMP, M_WAITOK | M_ZERO);
        
        if (local_str != NULL) {
            error = smb_convert_network_to_path(network_str, name_size,
                                                local_str, &local_str_len,
                                                '\\', UTF_SFM_CONVERSIONS,
                                                SMB_UNICODE_STRINGS(SSTOVC(share)));
            if (error) {
                SMBERROR("smb_convert_network_to_path failed %d\n", error);
                SMB_FREE(local_str, M_TEMP);
                local_str = NULL;
            }
        }
        else {
            /* Malloc failed, just error out */
            error = ENOMEM;
        }
    }
    else {
        local_str = smbfs_ntwrkname_tolocal((const char *)network_str, &name_size,
                                            TRUE);
        if (local_str == NULL) {
            SMBERROR("smbfs_ntwrkname_tolocal return NULL\n");
            error = EBADRPC;
        }
    }

    if (network_str != NULL) {
        /* Free network_name buffer */
        SMB_FREE(network_str, M_SMBFSDATA);
        network_str = NULL;
    }
    
    /* Return the local string */
    *ret_str = local_str;
    
bad:
    return error;    
}

static int
smb2_smb_parse_file_all_info(struct mdchain *mdp, void *args)
{
	int error;
    struct FILE_ALL_INFORMATION *all_infop = args;
    struct smbfattr *fap = all_infop->fap;
	uint64_t llint;
	uint32_t size, ea_size, access_flags;
 	size_t nmlen;
	char *ntwrkname = NULL;
	char *filename = NULL;
    
    /* Get creation time */
    error = md_get_uint64le(mdp, &llint);
    if (error) {
        goto bad;
    }
    if (llint) {
        smb_time_NT2local(llint, &fap->fa_crtime);
    }
    
    /* Get last access time */
    error = md_get_uint64le(mdp, &llint);
    if (error) {
        goto bad;
    }
    if (llint) {
        smb_time_NT2local(llint, &fap->fa_atime);
    }
    
    /* Get last write time */
    error = md_get_uint64le(mdp, &llint);
    if (error) {
        goto bad;
    }
    if (llint) {
        smb_time_NT2local(llint, &fap->fa_mtime);
    }
    
    /* Get change time */
    error = md_get_uint64le(mdp, &llint);
    if (error) {
        goto bad;
    }
    if (llint) {
        smb_time_NT2local(llint, &fap->fa_chtime);
    }
    
    /*
     * Get file attributes
     * SNIA CIFS Technical Reference is wrong, this should be
     * a ULONG
     */
    error = md_get_uint32le(mdp, &fap->fa_attr);
    if (error) {
        goto bad;
    }
    
    /*
     * Because of the Steve/Conrad Symlinks we can never be completely
     * sure that we have the correct vnode type if its a file. For
     * directories we always know the correct information.
     */
    if (fap->fa_attr & SMB_EFA_DIRECTORY) {
        fap->fa_valid_mask |= FA_VTYPE_VALID;
    }
    fap->fa_vtype = (fap->fa_attr & SMB_EFA_DIRECTORY) ? VDIR : VREG;
    
    /*
     * Get pad
     * SNIA CIFS Technical Reference is wrong, this should be
     * a ULONG PAD
     */
    error = md_get_uint32le(mdp, NULL);
    if (error) {
        goto bad;
    }
    
    /* Get allocation size */
    error = md_get_uint64le(mdp, &fap->fa_data_alloc);
    if (error) {
        goto bad;
    }
    
    /* Get EOF */
    error = md_get_uint64le(mdp, &fap->fa_size);
    if (error) {
        goto bad;
    }
    
    /* Get hard link count and ignore it */
    error = md_get_uint32le(mdp, NULL);
    if (error) {
        goto bad;
    }
    
    /* Get delete pending byte and ignore it */
    error = md_get_uint8(mdp, NULL);
    if (error) {
        goto bad;
    }
    
    /* Get directory or file byte and ignore it */
    error = md_get_uint8(mdp, NULL);
    if (error) {
        goto bad;
    }
    
    /* 
     * Get 2 bytes of pad and ignore it
     * At this point the SNIA CIFS Technical Reference is wrong. 
     * It should have the following: 
     *			USHORT		Unknown;
     *			ULONG		EASize;
     *			ULONG		PathNameLength;
     *			STRING		FullPath;
     * We need to be careful just in case someone followed the 
     * Technical Reference.
     */
    error = md_get_uint16(mdp, NULL);	/* Unknown */
    if (error) {
        goto bad;
    }
    
    /* Get File ID and save it */
    error = md_get_uint64le(mdp, &fap->fa_ino);
    if (error) {
        goto bad;
    }
    smb2fs_smb_file_id_check(all_infop->share, fap->fa_ino, NULL, 0);
    
    /*
     * Get EA size and ignore it
     *
     * Confirmed from MS:
     * When the attribute has the Reparse Point bit set then the EASize
     * contains the reparse tag info. This behavior is consistent for 
     * Full, Both, FullId, or BothId query dir calls.  It will pack the 
     * reparse tag into the EaSize value if ATTRIBUTE_REPARSE_POINT is set.  
     * I verified with local MS Engineers, and they also checking to make 
     * sure the behavior is covered in MS-FSA. 
     *
     * EAs and reparse points cannot both be in a file at the same
     * time. We return different information for each case.
     *
     * NOTE: This is not true for this call (SMB_QFILEINFO_ALL_INFO), they
     * return the reparse bit but the eaSize size is always zero?
     */
    error = md_get_uint32le(mdp, &ea_size);	/* extended attributes size */
    if (error) {
        goto bad;
    }

    /* Get AccessFlags and ignore it */
    error = md_get_uint32le(mdp, &access_flags);
    if (error) {
        goto bad;
    }

    /* Get Position Information and ignore it */
    error = md_get_uint64le(mdp, NULL);
    if (error) {
        goto bad;
    }
   
    /* Get Mode Information and ignore it */
    error = md_get_uint32le(mdp, NULL);
    if (error) {
        goto bad;
    }
    
    /* Get Alignment Information and ignore it */
    error = md_get_uint32le(mdp, NULL);
    if (error) {
        goto bad;
    }
    
    if (!(SSTOVC(all_infop->share)->vc_misc_flags & SMBV_HAS_FILEIDS)) {
        /* 
         * Server does not support File IDs
         * Assume node number has not changed for now.
         * If we parse the file name, then will calculate new inode number
         */
        fap->fa_ino = all_infop->np->n_ino;
    }

    /*
     * We don't care about the name, so we are done
     */
    if (all_infop->namep == NULL) {
        goto bad;
    }
    
    /* 
     * NOTE: When accessing the root node the name may not be what you would
     * expect. Windows will return a back slash if the item being shared is
     * a drive and in all other cases the name of the directory being shared.
     * We never ask for the name in the root node case so this should never
     * be an issue.
     */
    
    /* Get the file name len */
    error = md_get_uint32le(mdp, &size);
    if (error) {
        goto bad;
    }
    
    /* 
     * Make sure it is something in reason. Don't allocate it, 
     * if it doesn't make sense. 
     */
    if ((size <= 0) || (size >= (SMB_MAXFNAMELEN * 2))) {
        SMBERROR("Illegal file name len %d\n", size);
        error = EINVAL;
        goto bad;
    }

    nmlen = size;
    
    /* 
     * Since this is a full path only check SMB_MAXFNAMELEN length 
     * until we get the component. We just allocate what we need
     * need here. 
     */
    SMB_MALLOC(ntwrkname, char *, nmlen, M_SMBFSDATA, M_WAITOK);
    if (ntwrkname == NULL) {
        error = ENOMEM;
    }
    else {
        error = md_get_mem(mdp, (void *)ntwrkname, nmlen,
                           MB_MSYSTEM);	/* Full path name */
    }
    
    if (error) {
        goto bad;
    }
    
    /*
     * Here is the problem. They return the full path when we only 
     * need the last component. So we need to find the last back 
     * slash. So first remove any trailing nulls from the path.
     * Now start at the end of the path name and work our way back 
     * stopping when we find the first back slash. For UTF-16 make 
     * sure there is a null byte after the back slash.
     */
    if (!SMB_UNICODE_STRINGS(SSTOVC(all_infop->share))) {
        SMBERROR("Unicode must be supported\n");
        goto bad;
    }
    
    /* Don't count any trailing nulls in the name. */
    if ((nmlen > 1 && ntwrkname[nmlen - 1] == 0) &&
        (ntwrkname[nmlen - 2] == 0)) {
        nmlen -= 2;
    }
    
    /* 
     * Now get the file name. We need to start at the end
     * and work our way backwards.
     */
    if (nmlen > 1) {
        filename = &ntwrkname[nmlen - 2];
    }
    else {
        filename = ntwrkname;   
    }
    
    /* Work backwards until we reach the begining or find a '\' (0x5c) */
    while (filename > ntwrkname) {
        if ((*filename == 0x5c) && (*(filename + 1) == 0x00)) {
            break;
        }
        filename -= 2;
    }
    
    /* 
     * Found a back slash, move passed it and now we have 
     * the real file name. 
     */
    if ((*filename == 0x5c) && (*(filename + 1) == 0x00)) {
        filename += 2;
    }
    
    /* Reset the name length */
    nmlen = &ntwrkname[nmlen] - filename;
    
    /* Convert the name to a UTF-8 string  */
    filename = smbfs_ntwrkname_tolocal((const char *)filename, 
                                       &nmlen, 
                                       TRUE);
    
    if (filename == NULL) {
        error = EINVAL;
        SMBERROR("smbfs_ntwrkname_tolocal return NULL\n");
        goto bad;
    }
    
    if (nmlen > SMB_MAXFNAMELEN) {
        error = EINVAL;
        SMBERROR("Filename %s nmlen = %ld\n", filename, nmlen);
        goto bad;
    }
    
    *all_infop->namep = smb_strndup(filename, nmlen);			
    if (all_infop->name_lenp) {
        /* Return the name length */       
        *all_infop->name_lenp = nmlen;
    }
    
    if (!(SSTOVC(all_infop->share)->vc_misc_flags & SMBV_HAS_FILEIDS)) {
        /* 
         * Server does not support File IDs.
         * if we created a name, get inode number which is based on the name 
         */
        if ((*all_infop->namep) && (all_infop->name_lenp)) {
            fap->fa_ino = smbfs_getino(all_infop->np,
                                       *all_infop->namep,
                                       *all_infop->name_lenp);
        }
    }
    
bad:  
    if (ntwrkname != NULL) {
        /* Free the buffer that holds the name from the network */
        SMB_FREE(ntwrkname, M_SMBFSDATA);
    }
    
    if (filename != NULL) {
        /* Free the buffer that holds the name from the network */
        SMB_FREE(filename, M_TEMP);
    }
    
    return error;
}

static int
smb2_smb_parse_file_stream_info(struct mdchain *mdp, void *args,
                                uint32_t buffer_len)
{
	int error;
    struct FILE_STREAM_INFORMATION *stream_infop = args;
	enum stream_types stream_type = kNoStream;
    /* Are they looking for a specific stream? */
	int found_stream = (stream_infop->stream_namep) ? FALSE : TRUE; 
    struct smbnode *np = stream_infop->np;
	struct timespec ts;
	uint32_t next_entry_offset, network_name_len, parsed, nlen;
	uint32_t translate_names;
    uint64_t stream_size, alloc_size;
    char *network_name, *full_stream_name;
    size_t full_stream_name_len;
    const char *stream_name;
    const char *fname;
	struct smbmount *smp = stream_infop->share->ss_mount;
    
    translate_names = ((*stream_infop->stream_flagsp) & SMB_NO_TRANSLATE_NAMES) ? 0 : 1;
    *stream_infop->stream_flagsp = 0;
    
    /* Special case when no stream info found */
    if (buffer_len == 0) {
        /* if no output data, then no attrs were found */
        error = ENOATTR;
        goto done;
    }

    do {
        network_name = NULL;
        network_name_len = 0;
        full_stream_name = NULL;
        full_stream_name_len = 0;
        stream_name = NULL;
        next_entry_offset = 0;
        stream_size = 0;
        alloc_size = 0;
        
        /* Get next entry offset */
        error = md_get_uint32le(mdp, &next_entry_offset);
        if (error) {
            goto done;
        }
        parsed = 4;

        /* Get Stream Name Length */
        error = md_get_uint32le(mdp, &network_name_len);
        if (error) {
            goto done;
        }
        parsed += 4;

        /* Get Stream Size */
        error = md_get_uint64le(mdp, &stream_size); 
        if (error) {
            goto done;
        }
        parsed += 8;
        
        /* Get Alloc Size */
        error = md_get_uint64le(mdp, &alloc_size); 
        if (error) {
            goto done;
        }
        parsed += 8;
        
        /*
         * Alloc Size is derived from logical size
         * to be consistent with Query Dir with ReadDirAttr support 
         */
        alloc_size = smb2_smb_get_alloc_size(smp, stream_size);

        /*
         * Sanity check to limit DoS or buffer overrun attempts.
         * Make sure the length is not bigger than our max buffer size.
         */
        if (network_name_len > SSTOVC(stream_infop->share)->vc_txmax) {
            error = EBADRPC;
            goto done;
        }
        
        /* Get Stream Name */
        SMB_MALLOC(network_name, char *, network_name_len, M_SMBFSDATA, 
                   M_WAITOK | M_ZERO);
        if (network_name == NULL) {
            error = ENOMEM;
            goto done;
        }
        
        error = md_get_mem(mdp, network_name, network_name_len, MB_MSYSTEM);
        if (error) {
            SMB_FREE(network_name, M_SMBFSDATA);
            goto done;
        }
        parsed += network_name_len;
        
        /* 
		 * Ignore a trailing null, not that we expect them 
		 * NOTE: MS-CIFS states that the stream name is alway in UNICODE. We
		 * only support streams if the server supports UNICODE.
		 */
		if ((network_name_len > 1) && 
            !network_name[network_name_len - 1] && 
            !network_name[network_name_len - 2]) {
            network_name_len -= 2;
        }
        
        /* Convert network name to local name */
        full_stream_name_len = network_name_len;
        full_stream_name = smbfs_ntwrkname_tolocal(network_name,
                                                   &full_stream_name_len,
                                                   SMB_UNICODE_STRINGS(SSTOVC(stream_infop->share)));
        if (network_name != NULL) {
            SMB_FREE(network_name, M_SMBFSDATA);
            network_name = NULL;
        }

        /* 
         * Two usage cases (in stream_infop):
         * 1) Given np and namep == NULL. Query the path of np
         * 2) Given np and namep (must be readdirattr). Query PARENT np and 
         * child namep. In this case, do not update vnode np as that is the parent.
         * 
         * In both cases, Query for a list of streams and if streams are found, 
         * see if they match stream_namep that was passed in.
         */

        /*
		 * We should now have a name in the form : <foo> :$DATA Where <foo> is 
		 * UTF-8 w/o null termination. If it isn't in that form we want to LOG it 
		 * and skip it. Note we want to skip w/o logging the "data fork" entry,
		 * which is simply ::$DATA Otherwise we want to uiomove out <foo> with a 
         * null added.
		 */
        if (stream_infop->namep == NULL) {
            fname = np->n_name;
        }
        else {
            /* readdirattr case - parent np and child namep */
            fname = stream_infop->namep;
        }
        
        if (smbfs_smb_undollardata(fname, full_stream_name, &full_stream_name_len)) {
			/* the "+ 1" skips over the leading colon */
			stream_name = full_stream_name + 1;

			/* Is it the Resource Fork? If not, is it the Finder Info? */
			if ((full_stream_name_len >= sizeof(SFM_RESOURCEFORK_NAME)) &&
				(!strncasecmp(stream_name, SFM_RESOURCEFORK_NAME, sizeof(SFM_RESOURCEFORK_NAME)))) {
                /*
                 * Its the Resource Fork.
                 */
                stream_type |= kResourceFrk;

                if (stream_infop->namep == NULL) {
                    /* Update resource fork size in meta data of vnode */
                    lck_mtx_lock(&np->rfrkMetaLock);
                    np->rfrk_size = stream_size;
                    np->rfrk_alloc_size = alloc_size;
                    nanouptime(&ts);
                    np->rfrk_cache_timer = ts.tv_sec;
                    lck_mtx_unlock(&np->rfrkMetaLock);
                }
    
				/* 
				 * The Resource Fork and Finder info names are special and get 
				 * translated between stream names and extended attribute names. 
				 * In this case we need to make sure the correct name gets used. 
				 * So if we are looking for a specific stream use its stream name 
				 * otherwise use its extended attribute name.
				 */
                if ((stream_infop->uio == NULL) && 
                    (stream_infop->stream_buf_sizep == NULL) &&
                    (stream_infop->stream_namep != NULL)) {
                    /* They are looking for a specific stream */
                    stream_name = SFM_RESOURCEFORK_NAME;
                    full_stream_name_len = sizeof(SFM_RESOURCEFORK_NAME);
                } 
                else {
                    /* Must be doing listxattr */
                    if (translate_names) {
                        stream_name = XATTR_RESOURCEFORK_NAME;
                        full_stream_name_len = sizeof(XATTR_RESOURCEFORK_NAME);
                        full_stream_name_len = sizeof(XATTR_RESOURCEFORK_NAME);
                    } else {
                        stream_name = SFM_RESOURCEFORK_NAME;
                        full_stream_name_len = sizeof(SFM_RESOURCEFORK_NAME);
                    }
                }
                
                /*
                 * The uio means we are getting this from a listxattr call, so 
                 * never display zero length resource forks. Resource forks  
                 * should always contain a resource map. Seems CoreService never 
                 * deleted the resource fork, they just set the eof to zero. We 
                 * need to handle these 0 length resource forks here.
                 */
                if (stream_infop->uio && (stream_size == 0)) {
                    goto skipentry;	
                }
                
            } else if ((full_stream_name_len >= sizeof(SFM_FINDERINFO_NAME)) &&
                       (!strncasecmp(stream_name, SFM_FINDERINFO_NAME, sizeof(SFM_FINDERINFO_NAME)))) {
                /*
                 * Its the Finder Info.
                 */
                stream_type |= kFinderInfo;
                
				/* 
				 * The Resource Fork and Finder info names are special and get 
				 * translated between stream names and extended attribute names. 
				 * In this case we need to make sure the correct name gets used. 
				 * So if we are looking for a specific stream use its stream name 
				 * otherwise use its extended attribute name.
				 */
                if ((stream_infop->uio == NULL) &&
                    (stream_infop->stream_buf_sizep == NULL) &&
                    (stream_infop->stream_namep != NULL)) {
                    /* They are looking for a specific stream */
                    stream_name = SFM_FINDERINFO_NAME;
                    full_stream_name_len = sizeof(SFM_FINDERINFO_NAME);
                }
                else {
                    /* Must be doing listxattr */
                    if (translate_names) {
                        stream_name = XATTR_FINDERINFO_NAME;
                        full_stream_name_len = sizeof(XATTR_FINDERINFO_NAME);
                    } else {
                        stream_name = SFM_FINDERINFO_NAME;
                        full_stream_name_len = sizeof(SFM_FINDERINFO_NAME);
                    }
                }

                if (stream_size == 0) {
                    /*
                     * They have an AFP_Info stream and it has no size must be 
                     * a Samba server. We treat this the same as if the file 
                     * has no Finder Info
                     */
                    goto skipentry;
                }
            }
            
            /*
             * Depending on what is passed in we handle the data in two  
             * different ways.
             *	1. If they have a uio then just put all the stream names into the 
             * uio buffer. Must be doing a listxattr to get back list of all 
             * xattrs.
             *	2. If they pass in a stream name then they just want the size of 
             * that stream.
             *
             * NOTE: If there is nothing in the stream we will not return it in 
             * the list. This allows us to hide empty streams from copy engines. 
             *
             * We never return SFM_DESKTOP_NAME or SFM_IDINDEX_NAME streams.
             */
            if (((full_stream_name_len >= sizeof(SFM_DESKTOP_NAME)) &&
                 (!strncasecmp(stream_name, SFM_DESKTOP_NAME, sizeof(SFM_DESKTOP_NAME)))) ||
                ((full_stream_name_len >= sizeof(SFM_IDINDEX_NAME)) &&
                 (!strncasecmp(stream_name, SFM_IDINDEX_NAME, sizeof(SFM_IDINDEX_NAME))))) {
                    /*
                     * Its the Desktop Name or the ID Index Name
                     */
                    
                    /* 
                     * If they are looking for the Desktop Name, then they are 
                     * checking to see if its a SFM Volume.
                     */
                    if ((stream_infop->stream_namep != NULL) &&
                        (!strncasecmp(SFM_DESKTOP_NAME, stream_infop->stream_namep, sizeof(SFM_DESKTOP_NAME)))) {
                        found_stream = TRUE;
                    }
                    goto skipentry;
                } else if (stream_infop->uio != NULL) {
                    /* Case (1) - listxattr, so copy the stream name into uio */
                    uiomove(stream_name, (int) full_stream_name_len,
                            stream_infop->uio);
                }
                else if (!found_stream && 
                         (stream_infop->stream_namep != NULL) &&
                         (stream_infop->stream_sizep != NULL)) {
                    /* 
                     * Case (2) They are looking for a specific stream name, 
                     * but have not found it yet. Check to see if current stream 
                     * matches what they are looking for. If it does, then
                     * return its size.
                     */ 
                    nlen = (uint32_t) strnlen(stream_infop->stream_namep, 
                                              stream_infop->share->ss_maxfilenamelen+1);
                    if ((full_stream_name_len >= nlen) &&
                        (!strncasecmp(stream_name, stream_infop->stream_namep, nlen))) {
                        *stream_infop->stream_sizep = stream_size;
                        *stream_infop->stream_alloc_sizep = alloc_size;
                        found_stream = TRUE;
                    }
                }

            if (stream_infop->stream_buf_sizep) {
                /*
                 * Must be doing listxattr.
                 *
                 * They could be trying to determine the size of a buffer 
                 * sufficiently large enough to hold all the xattr names.
                 *
                 * This has several problem, but we cannot solve them all here. 
                 * First someone can create a stream/EA between this call and 
                 * the one they make to get the data. Second this will cause an 
                 * extra round of traffic. We could cache all of this, but how 
                 * long would we keep this information around. It could also 
                 * require a large buffer.
                 */
                *stream_infop->stream_buf_sizep += full_stream_name_len;
            }

		} /* smbfs_smb_undollardata */
        
skipentry:
        if (full_stream_name != NULL) {
            SMB_FREE(full_stream_name, M_SMBFSDATA);
            full_stream_name = NULL;
        }
        
        /* 
         * next_entry_offset is offset to next entry.
         * Subtract out the number of bytes that we have already parsed and
         * that will tell us how many pad bytes are left to be consumed.
         */
		if (next_entry_offset > parsed) {
			next_entry_offset -= parsed;
			if (next_entry_offset > SSTOVC(stream_infop->share)->vc_txmax) {
                /* went past the end of max allowable packet, so illegal */
                error = EBADRPC;
                goto out;
			}
            /* consume any pad bytes */
			md_get_mem(mdp, NULL, next_entry_offset, MB_MSYSTEM);
		}
	} while (next_entry_offset && !error);
    
out:
    if (full_stream_name != NULL) {
        SMB_FREE(full_stream_name, M_SMBFSDATA);
    }
   
done:
	/* 
     * If we searched the entire list and did not find a finder info stream, 
     * then reset the cache timer. 
     */
    if ((stream_type & kFinderInfo) != kFinderInfo) {
        /* set stream_flag indicating no Finder Info */
        *stream_infop->stream_flagsp |= SMB_NO_FINDER_INFO;
        
        if (stream_infop->namep == NULL) {
            /* Negative cache the Finder Info in the vnode */
            bzero(np->finfo, sizeof(np->finfo));
            nanouptime(&ts);
            np->finfo_cache = ts.tv_sec;
        }
    }

    /* 
     * If we searched the entire list and did not find a resource stream, 
     * then reset the cache timer. 
     */
    if ((stream_type & kResourceFrk) != kResourceFrk) {
        /* set stream_flag indicating no Resource Fork */
        *stream_infop->stream_flagsp |= SMB_NO_RESOURCE_FORK;
        
        if (stream_infop->namep == NULL) {
            /* Negative cache the resource fork in the vnode */
            lck_mtx_lock(&np->rfrkMetaLock);
            
            nanouptime(&ts);
            np->rfrk_size = 0;
            np->rfrk_alloc_size = 0;
            np->rfrk_cache_timer = ts.tv_sec;
            
            lck_mtx_unlock(&np->rfrkMetaLock);
        }
    }
	
	if ((found_stream == FALSE) || (error == ENOENT)) {
        /* We did not find the stream we were looking for, remap error code */
		error = ENOATTR;
    }
    
	return (error);
}

static int
smb2_smb_parse_fs_attr(struct mdchain *mdp, void *args)
{
	int error;
    struct FILE_FS_ATTRIBUTE_INFORMATION *fs_attrs = args;
    
    error = md_get_uint32le(mdp, &fs_attrs->file_system_attrs);
    if (error) {
        goto bad;
    }
    
    error = md_get_uint32le(mdp, &fs_attrs->max_component_name_len);
    if (error) {
        goto bad;
    }
    /* Make sure max_component_name_len is something reasonable.
     * See <rdar://problem/12171424>.
     */
    if (fs_attrs->max_component_name_len > (SMB_MAXFNAMELEN * 2)) {
        SMBERROR("Illegal file name len %u\n", fs_attrs->max_component_name_len);
        fs_attrs->max_component_name_len = (SMB_MAXFNAMELEN * 2);
    }
    
    error = md_get_uint32le(mdp, &fs_attrs->file_system_name_len);
    if (error) {
        goto bad;
    }
    
    /* return the file system name if it will fit */
    if ((fs_attrs->file_system_name_len > 0) &&
        (fs_attrs->file_system_name_len < PATH_MAX)) {        
        SMB_MALLOC(fs_attrs->file_system_namep, 
                   char *, 
                   fs_attrs->file_system_name_len, 
                   M_SMBFSDATA, 
                   M_WAITOK | M_ZERO);
        
        if (fs_attrs->file_system_namep != NULL) {
            error = md_get_mem(mdp, 
                               fs_attrs->file_system_namep,
                               fs_attrs->file_system_name_len, 
                               MB_MSYSTEM);
            if (error) {
                goto bad;
            }
        }
        else {
            error = ENOMEM;
        }
    }
    
bad:    
    return error;
}

static int
smb2_smb_parse_fs_size(struct mdchain *mdp, void *args)
{
	int error;
    struct FILE_FS_SIZE_INFORMATION *fs_size = args;
    
    error = md_get_uint64le(mdp, &fs_size->total_alloc_units);
    if (error) {
        goto bad;
    }
    error = md_get_uint64le(mdp, &fs_size->avail_alloc_units);
    if (error) {
        goto bad;
    }
    error = md_get_uint32le(mdp, &fs_size->sectors_per_alloc_unit);
    if (error) {
        goto bad;
    }
    error = md_get_uint32le(mdp, &fs_size->bytes_per_sector);
    if (error) {
        goto bad;
    }
    
bad:    
    return error;
}

static int
smb2_smb_parse_get_reparse_point(struct mdchain *mdp,
                                 struct smb2_ioctl_rq *ioctlp)
{
	int error;
    uint32_t tag, flags;
    uint16_t data_length, reserved, substitute_name_offset, substitute_name_len;
    uint16_t print_name_offset, print_name_len;
	char *network_name = NULL;
	char *path = NULL;
	size_t path_len;
    
    /*
     * Parse SMB2 IOCTL Response for FSCTL_GET_REPARSE_POINT
     * We are already pointing to begining of Symbolic Link Reparse Data Buffer
     */
    
    /* Check Reparse Tag is IO_REPARSE_TAG_SYMLINK */
	error = md_get_uint32le(mdp, &tag);
    if (error) {
        goto bad;
    }
    if (tag != IO_REPARSE_TAG_SYMLINK) {
        SMBERROR("Bad reparse tag: %u\n", (uint32_t) tag);
        error = EBADRPC;
        goto bad;
    }
    
    /* Get Reparse Data Length */
    error = md_get_uint16le(mdp, &data_length);
    if (error) {
        goto bad;
    }
	
    /* Get Reserved */
    error = md_get_uint16le(mdp, &reserved);
    if (error) {
        goto bad;
    }
    
    /* Get Substitute Name Offset */
    error = md_get_uint16le(mdp, &substitute_name_offset);
    if (error) {
        goto bad;
    }
    
    /* Get Substitute Name Length */
    error = md_get_uint16le(mdp, &substitute_name_len);
    if (error) {
        goto bad;
    }
    
    /* Get Print Name Offset */
    error = md_get_uint16le(mdp, &print_name_offset);
    if (error) {
        goto bad;
    }
    
    /* Get Print Name Length */
    error = md_get_uint16le(mdp, &print_name_len);
    if (error) {
        goto bad;
    }
    
    /* 
     * Get Flags
	 * Flags field can be either SYMLINK_FLAG_ABSOLUTE or SYMLINK_FLAG_RELATIVE,
	 * in either case we don't care and just ignore it for now.
	 */
    error = md_get_uint32le(mdp, &flags);
    if (error) {
        goto bad;
    }
    
	SMBSYMDEBUG("reparseLen = %d SubstituteNameOffset = %d SubstituteNameLength = %d PrintNameOffset = %d PrintNameLength = %d Flags = %d\n",
                data_length,
                substitute_name_offset, substitute_name_len,
                print_name_offset, print_name_len,
                flags);

	/*
	 * Mount Point Reparse Data Buffer
	 * A mount point has a substitute name and a print name associated with it. 
	 * The substitute name is a pathname (section 2.1.5) identifying the target 
	 * of the mount point. The print name SHOULD be an informative pathname 
	 * (section 2.1.5), suitable for display to a user, that also identifies the 
	 * target of the mount point. Neither of these pathnames can contain dot 
	 * directory names.
	 * 
	 * So the above implies that we should always use the substitute name, but
	 * my guess is they are always the same in symbolic link case.
	 */
    
	/* Never allocate more than our transaction size buffer */
	if ((substitute_name_len == 0) ||
        (substitute_name_len > SSTOVC(ioctlp->share)->vc_txmax)) {
		error = ENOMEM;
		SMBSYMDEBUG("%s SubstituteNameLength too large or zero %d \n", np->n_name, SubstituteNameLength);
		goto bad;
	}
	
    /* 
     * Note: the substitute name offset is calculated from byte 0 of the 
     * Path Buffer and not from the beginning of the response packet.
     */
	if (substitute_name_offset) {
		error = md_get_mem(mdp, NULL, substitute_name_offset, MB_MSYSTEM);
        if (error) {
            goto bad;
        }
	}
	
    /* Get the Substitute Name */
	SMB_MALLOC(network_name, char *, (size_t) substitute_name_len, M_TEMP,
               M_WAITOK | M_ZERO);
	if (network_name == NULL) {
		error = ENOMEM;
        goto bad;
	}
    
    error = md_get_mem(mdp, (void *) network_name,
                       (size_t) substitute_name_len, MB_MSYSTEM);
	if (error) {
        goto bad;
	}
    
    /* 
     * Convert the Substitute name into a path
     * Converting one UTF16 char could result in up to 9 bytes of Unicode so
     * make sure to malloc len * 9 number of bytes.
     */
	path_len = substitute_name_len * 9 + 1;
	SMB_MALLOC(path, char *, path_len, M_TEMP, M_WAITOK | M_ZERO);
	if (path == NULL) {
		error = ENOMEM;
        goto bad;
    }

    error = smb_convert_network_to_path(network_name, substitute_name_len,
                                        path, &path_len,
                                        '\\', UTF_SFM_CONVERSIONS,
                                        SMB_UNICODE_STRINGS(SSTOVC(ioctlp->share)));

    if (!error) {
        /* Return the path */
		ioctlp->rcv_output_buffer = (uint8_t *) path;
		ioctlp->rcv_output_len = (uint32_t) path_len;
		path = NULL;    /* so we dont free it later */
	}
	
bad:
    if (network_name != NULL) {
        SMB_FREE(network_name, M_TEMP);
    }
        
    if (path != NULL) {
        SMB_FREE(path, M_TEMP);
    }

	return error;
}

static int
smb2_smb_parse_get_resume_key(struct mdchain *mdp,
                              struct smb2_ioctl_rq *ioctlp)
{
	int     error = 0;
    char    *resume_key = NULL;
    
    SMB_MALLOC(resume_key, char *, (size_t) ioctlp->ret_output_len, M_TEMP,
               M_WAITOK | M_ZERO);
    if (resume_key == NULL) {
        error = ENOMEM;
        goto bad;
    }
    
    error = md_get_mem(mdp, (void *) resume_key,
                       (size_t) ioctlp->ret_output_len, MB_MSYSTEM);
    if (!error) {
		ioctlp->rcv_output_buffer = (uint8_t *) resume_key;
        ioctlp->rcv_output_len = (uint32_t) ioctlp->ret_output_len;
    }
    
bad:
    return (error);
}

int
smb2_smb_parse_ioctl(struct mdchain *mdp,
                     struct smb2_ioctl_rq *ioctlp)
{
    int error;
    uint16_t length;
    uint16_t reserved_uint16;
    uint32_t ret_ctlcode;
    SMB2FID ret_fid;
    uint32_t ret_input_offset;
    uint32_t ret_output_offset;
    uint32_t ret_flags;
    uint32_t reserved_uint32;
    
    /* 
     * Parse SMB2 IOCTL Response 
     * We are already pointing to begining of Response data
     */
    
    /* Check structure size is 49 */
    error = md_get_uint16le(mdp, &length);
    if (error) {
        goto bad;
    }
    if (length != 49) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)length);
        error = EBADRPC;
        goto bad;
    }
    
    /* Get Reserved */
    error = md_get_uint16le(mdp, &reserved_uint16);
    if (error) {
        goto bad;
    }
    
    /* Get CtlCode */
    error = md_get_uint32le(mdp, &ret_ctlcode);
    if (error) {
        goto bad;
    }
    
    /* Get File ID (currently unused) */
    error = md_get_uint64le(mdp, &ret_fid.fid_persistent);
    if (error) {
        goto bad;
    }
    error = md_get_uint64le(mdp, &ret_fid.fid_volatile);
    if (error) {
        goto bad;
    }
    
    /* Get Input offset */
    error = md_get_uint32le(mdp, &ret_input_offset);
    if (error) {
        goto bad;
    }
    
    /* Get Input count */
    error = md_get_uint32le(mdp, &ioctlp->ret_input_len);
    if (error) {
        goto bad;
    }
    
    /* Get Output offset */
    error = md_get_uint32le(mdp, &ret_output_offset);
    if (error) {
        goto bad;
    }
    
    /* Get Output count */
    error = md_get_uint32le(mdp, &ioctlp->ret_output_len);
    if (error) {
        goto bad;
    }
    
    /* Get Flags */
    error = md_get_uint32le(mdp, &ret_flags);
    if (error) {
        goto bad;
    }
    
    /* Get Reserved2 */
    error = md_get_uint32le(mdp, &reserved_uint32);
    if (error) {
        goto bad;
    }
    
    /* At this point, mdp is pointing at the Buffer */
    
    switch(ret_ctlcode) {
        case FSCTL_GET_REPARSE_POINT:
            error = smb2_smb_parse_get_reparse_point(mdp, ioctlp);
            break;
            
        case FSCTL_DFS_GET_REFERRALS:
        case FSCTL_PIPE_TRANSCEIVE:
            /* validate some return values */
            if (ioctlp->ret_input_len != 0) {
                SMBDEBUG("input_count is supposed to be 0: %u\n",
                         (uint32_t) ioctlp->ret_input_len);
            }
            
            if (ret_flags != 0) {
                /* Client is supposed to ignore this field */
                SMBDEBUG("flags is supposed to be 0: %u\n", (uint32_t) ret_flags);
            }
            
            /* 
             * Data offset is from the beginning of SMB2 Header
             * Calculate how much further we have to go to get to it.
             */
            ret_output_offset -= SMB2_HDRLEN;
            /* already parsed 48 bytes worth of the response */
            ret_output_offset -= 48;
            
            if (ret_output_offset > 0) {
                error = md_get_mem(mdp, NULL, ret_output_offset, MB_MSYSTEM);
                if (error) {
                    goto bad;
                }
            }
            
            /* Get the returned pipe data */
            if (ioctlp->ret_output_len != 0) {
                /* read pipe data into the buffer pointed at by the uio */
                error = md_get_uio(mdp, ioctlp->rcv_output_uio, 
                                   ioctlp->ret_output_len);
            }
            break;
            
        case FSCTL_SET_REPARSE_POINT:
            /* Nothing to parse in this reply */
            break;

        case FSCTL_SRV_REQUEST_RESUME_KEY:
            /* validate some return values */
            if (ioctlp->ret_input_len != 0) {
                SMBDEBUG("input_count is supposed to be 0: %u\n",
                         (uint32_t) ioctlp->ret_input_len);
            }
            
            if (ioctlp->ret_output_len != 0x20) {
                /* MUST be 0x20 */
                SMBDEBUG("output_count is supposed to be 32, got: %u\n", (uint32_t) ioctlp->ret_output_len);
            }
            
            /*
             * Data offset is from the beginning of SMB2 Header
             * Calculate how much further we have to go to get to it.
             */
            ret_output_offset -= SMB2_HDRLEN;
            /* already parsed 48 bytes worth of the response */
            ret_output_offset -= 48;
            
            // Advance to start of copychunk_response buffer
            if (ret_output_offset > 0) {
                error = md_get_mem(mdp, NULL, ret_output_offset, MB_MSYSTEM);
                if (error) {
                    goto bad;
                }
            }
            
            /* Now fetch the resume key */
            error = smb2_smb_parse_get_resume_key(mdp, ioctlp);
            
            break;
            
        case FSCTL_SRV_COPYCHUNK:
            /* validate some return values */
            if (ioctlp->ret_input_len != 0) {
                SMBDEBUG("input_count is supposed to be 0: %u\n",
                         (uint32_t) ioctlp->ret_input_len);
            }
            
            if (ioctlp->ret_output_len != 0xC) {
                /* MUST be 0x20 */
                SMBDEBUG("output_count is supposed to be 12, got: %u\n", (uint32_t) ioctlp->ret_output_len);
            }
            
            /* Now fetch the copychunk_response */
            error = smb2_smb_parse_copychunk_response(mdp, ioctlp);
            
            if (error) {
                SMBDEBUG("smb2_smb_parse_copychunk_response failed, error: %d\n", error);
            }
            
            break;

        default:
            SMBERROR("Unsupported ret ioctl: %d\n", ret_ctlcode);
            error = EBADRPC;
            goto bad;
    }
    
bad:    
    return error;
}

int
smb2_smb_parse_lease_break(struct smbiod *iod, mbuf_t m)
{
    int error;
	struct smb_vc *vcp = iod->iod_vc;
	struct smb_rq rqp;
	struct mdchain *mdp = NULL;
	uint16_t length, epoch;
    uint32_t flags, tree_id = 0;
    uint64_t lease_key_hi;      /* atomic increment number */
    uint64_t lease_key_low;     /* node hash value */
    uint32_t curr_lease_state, new_lease_state, ret_lease_state;
    uint64_t hash_val = 0;
    struct smb_share *share = NULL, *tshare;
    int found_share = 0;
	struct smbmount *smp = NULL;
    
	bzero(&rqp, sizeof(rqp));
    
    rqp.sr_vc = vcp;
    
    smb_rq_getreply(&rqp, &mdp);
    md_initm(mdp, m);
    
    error = smb2_rq_parse_header(&rqp, &mdp);
    if (error) {
        SMBDEBUG("smb2_rq_parse_header failed %d for lease break \n", error);
        goto bad;
    }

    /*
     * Parse SMB2 Lease Break Response
     * We are already pointing to begining of Response data
     */
    
    /* Check structure size is 44 */
    error = md_get_uint16le(mdp, &length);
    if (error) {
        goto bad;
    }
    if (length != 44) {
        SMBERROR("Bad struct size: %u\n", (uint32_t) length);
        error = EBADRPC;
        goto bad;
    }
    
    /* Get New Epoch (ignored) */
    error = md_get_uint16le(mdp, &epoch);
    if (error) {
        goto bad;
    }
    
    /* Get Flags */
    error = md_get_uint32le(mdp, &flags);
    if (error) {
        goto bad;
    }
    
    /* Get Lease Key */
    error = md_get_uint64le(mdp, &lease_key_hi);
    if (error) {
        goto bad;
    }
    
    error = md_get_uint64le(mdp, &lease_key_low);
    if (error) {
        goto bad;
    }

    /* Get Current Lease State */
    error = md_get_uint32le(mdp, &curr_lease_state);
    if (error) {
        goto bad;
    }

    /* Get New Lease State */
    error = md_get_uint32le(mdp, &new_lease_state);
    if (error) {
        goto bad;
    }

    /* Get Break Reason (ignored) */
    error = md_get_uint32le(mdp, NULL);
    if (error) {
        goto bad;
    }

    /* Get Access Mask Hint (ignored) */
    error = md_get_uint32le(mdp, NULL);
    if (error) {
        goto bad;
    }
    
    /* Get Share Mask Hint (ignored) */
    error = md_get_uint32le(mdp, NULL);
    if (error) {
        goto bad;
    }

    smb2_smb_dur_handle_parse_lease_key(lease_key_hi, lease_key_low,
                                        &tree_id, &hash_val);

    /*
     * Find the share
     */
	smb_vc_lock(vcp);	/* lock the vc so we can search the list */

	SMBCO_FOREACH_SAFE(share, VCTOCP(vcp), tshare) {
		/*
		 * Since we have the vc lock we know the share can't be freed, but
		 * it could be going away (Tree Disconnect is in process). Take a
		 * reference so no one else can disconnect it out from underneath
		 * us.
		 */
		smb_share_ref(share);
        
		if (share->ss_flags & SMBO_GONE) {
			/* Skip any shares that are being disconnected */
			smb_share_rele(share, iod->iod_context);
			continue;
		}
        
        if (share->ss_tree_id == tree_id) {
            found_share = 1;
            smb_share_rele(share, iod->iod_context);
            break;
        }
        
        smb_share_rele(share, iod->iod_context);
	}
    
	smb_vc_unlock(vcp);

    if (found_share == 0) {
        SMBERROR("No matching share found for lease break \n");
        error = ENOENT;
        goto bad;
    }
    
	smp = share->ss_mount;

    /* Try to find the vnode and upates its lease state */
    error = smbfs_handle_lease_break(smp, lease_key_hi, lease_key_low, new_lease_state);
    if (error) {
        goto bad;
    }
    
    if (flags & SMB2_NOTIFY_BREAK_LEASE_FLAG_ACK_REQUIRED) {
        //lck_mtx_lock(&share->ss_shlock); /* this causes a deadlock... */
        error = smb2_smb_lease_break_ack(share, lease_key_hi, lease_key_low,
                                         new_lease_state, &ret_lease_state,
                                         iod->iod_context);
        //lck_mtx_unlock(&share->ss_shlock);
    }
    
bad:
    md_done(mdp);
    
    return error;
}

static int
smb2_smb_parse_negotiate(struct smb_vc *vcp, struct smb_rq *rqp, int smb1_req)
{
	uint16_t length;
	uint16_t sec_buf_offset;
	uint16_t sec_buf_len;
	uint8_t server_guid[16], curr_time[8], boot_time[8];
	uint16_t reserved16;
	uint32_t reserved;
	uint16_t smb2_security_mode;
	struct smb_sopt *sp = &vcp->vc_sopt;
	struct mdchain *mdp;
	int error;
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    /*
     * Parse SMB2 Negotiate Response
     * We are already pointing to begining of Response data
     */
    
    /* Check structure size is 65 */
    error = md_get_uint16le(mdp, &length);
    if (error) {
        goto bad;
    }
    if (length != 65) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)length);
        error = EBADRPC;
        goto bad;
    }
    
    /* Get Security Mode */
    error = md_get_uint16le(mdp, &smb2_security_mode);
    if (error) {
        goto bad;
    }
    
    if (vcp->vc_misc_flags & SMBV_CLIENT_SIGNING_REQUIRED) {
        if (smb2_security_mode & (SMB2_NEGOTIATE_SIGNING_ENABLED | SMB2_NEGOTIATE_SIGNING_REQUIRED)) {
            if (smb2_security_mode & (SMB2_NEGOTIATE_SIGNING_REQUIRED))
                vcp->vc_flags |= SMBV_SIGNING_REQUIRED;
        } else {
            SMBERROR(" SMB2 Client requires Signing, server doesn't!\n");
            error = EAUTH;
            goto bad;
        }
    }
    
    if (smb2_security_mode & SMB2_NEGOTIATE_SIGNING_ENABLED)
        vcp->vc_flags |= SMBV_SIGNING;
    
    if ( (smb2_security_mode & SMB2_NEGOTIATE_SIGNING_REQUIRED) ||
        (vcp->vc_misc_flags & SMBV_CLIENT_SIGNING_REQUIRED)) {
        /* Server requires signing */
        vcp->vc_hflags2 |= SMB_FLAGS2_SECURITY_SIGNATURE;
    }
    
    /* always set User Level Access and supports challenge/resp */
    vcp->vc_flags |= SMBV_USER_SECURITY | SMBV_ENCRYPT_PASSWORD;
    
    /*
     * Get Dialect.
     * if the server responded without an error then they must support one of
     * our dialects.
     */
    error = md_get_uint16le(mdp, &sp->sv_dialect);
    if (error) {
        goto bad;
    }
    
    /* Must get either SMB 2.002 or SMB ??? (ie SMB 2.x) */
    switch (sp->sv_dialect) {
        case SMB2_DIALECT_0210:
            vcp->vc_flags |= SMBV_SMB2 | SMBV_SMB21;
            break;
        case SMB2_DIALECT_02ff:
            /* not sure what dialect yet, still have to negotiate some more */
            vcp->vc_flags |= SMBV_SMB2;
            break;
        case SMB2_DIALECT_0202:
            vcp->vc_flags |= SMBV_SMB2 | SMBV_SMB2002;
            break;
            
        default:
            SMBERROR("Unknown dialect 0x%x\n", sp->sv_dialect);
            error = ENOTSUP;
            goto bad;
    }
    
    if ((sp->sv_dialect != SMB2_DIALECT_0202) &&
        (sp->sv_dialect != SMB2_DIALECT_0210) &&
        (smb1_req == 1)) {
        /*
         * We started with SMB1 Negotiate requst and got back a SMB 2 Negotiate
         * response and that is the response we are parsing right now.
         *
         * For SMB 2.002 and 2.1, we need to continue parsing the SMB 2
         * Negotiate response as we are going directly to the Session Setup
         * exchange next.
         *
         * Any other dialect from the server, we will start over with a
         * SMB 2 Negotiate request from the client. The SMB 2 Negotiate request 
         * will have information about the client that the server will need. 
         * In this case, we dont have to finish parsing the SMB 1 response and 
         * can just skip out.
         *
         * NOTE: If the server is replying with SMB 2.1 to a SMB 1 Negotiate
         * request, then this is wrong according to [MS-SMB2] sections 3.3.5.3.1
         * and 4.2 where it states the server should be returning SMB 2.??.
         * Windows 8 Client handles this by going directly to the Session Setup
         * and that is the behavior we are following.
         */
        error = 0;
        goto bad;
    }
    
    /* Get UInt16 Reserved bytes */
    error = md_get_uint16le(mdp, &reserved16);
    if (error) {
        goto bad;
    }
    
    /*
     * Get Server GUID
     * %%% To Do - Do something with Server GUID field???
     */
    error = md_get_mem(mdp, (caddr_t)server_guid, 16, MB_MSYSTEM);
    if (error) {
        goto bad;
    }
    
    /* Get Server Capabilities */
    error = md_get_uint32le(mdp, &sp->sv_capabilities);
    if (error) {
        goto bad;
    }
    
    /*
     * %%% To Do - too many places are looking at sv_capabilities
     * so for now, prefill it in.  Later we should use the
     * sv_capabilities for SMB2.
     */
    sp->sv_caps = (SMB_CAP_UNICODE |
                   SMB_CAP_LARGE_FILES |
                   /* SMB_CAP_NT_SMBS <not used> */
                   SMB_CAP_RPC_REMOTE_APIS |
                   SMB_CAP_STATUS32 |
                   /* SMB_CAP_LEVEL_II_OPLOCKS <not used> */
                   SMB_CAP_LOCK_AND_READ |
                   /* SMB_CAP_NT_FIND <not used> */
                   /* SMB_CAP_INFOLEVEL_PASSTHRU <not used> */
                   SMB_CAP_LARGE_READX |
                   SMB_CAP_LARGE_WRITEX |
                   SMB_CAP_EXT_SECURITY);
    
    if (sp->sv_capabilities & SMB2_GLOBAL_CAP_DFS) {
        sp->sv_caps |= SMB_CAP_DFS;
    }
    
    /* Get Max Transact/Read/Write sizes */
    error = md_get_uint32le(mdp, &sp->sv_maxtransact);
    if (error) {
        goto bad;
    }
    
    error = md_get_uint32le(mdp, &sp->sv_maxread);
    if (error) {
        goto bad;
    }
    
    error = md_get_uint32le(mdp, &sp->sv_maxwrite);
    if (error) {
        goto bad;
    }
    
    /*
     * Get Server system time and start time
     * %%% To Do - Do something with these times???
     */
    error = md_get_mem(mdp, (caddr_t)curr_time, 8, MB_MSYSTEM);
    if (error)
        goto bad;
    
    error = md_get_mem(mdp, (caddr_t)boot_time, 8, MB_MSYSTEM);
    if (error) {
        goto bad;
    }
    
    /* Get Security Blob offset and length */
    error = md_get_uint16le(mdp, &sec_buf_offset);
    if (error) {
        goto bad;
    }
    
    error = md_get_uint16le(mdp, &sec_buf_len);
    if (error) {
        goto bad;
    }
    
    /* Get Reserved bytes */
    error = md_get_uint32le(mdp, &reserved);
    if (error) {
        goto bad;
    }
    
    /*
     * Security buffer offset is from the beginning of SMB2 Header
     * Calculate how much further we have to go to get to it.
     */
    sec_buf_offset -= SMB2_HDRLEN;
    sec_buf_offset -= 64;   /* already parse 64 bytes worth of the response */
    
    if (sec_buf_offset > 0) {
        error = md_get_mem(mdp, NULL, sec_buf_offset, MB_MSYSTEM);
        if (error) {
            goto bad;
        }
    }
    
    /* Its ok if the sec_buf_len is 0 */
    if (sec_buf_len > 0) {
        SMB_MALLOC(vcp->negotiate_token, uint8_t *, sec_buf_len, M_SMBTEMP, M_WAITOK);
        if (vcp->negotiate_token) {
            vcp->negotiate_tokenlen = sec_buf_len;
            error = md_get_mem(mdp,
                               (void *)vcp->negotiate_token,
                               vcp->negotiate_tokenlen,
                               MB_MSYSTEM);
            if (error) {
                goto bad;
            }
        }
        else {
            error = ENOMEM;
        }
    }
    
bad:
	return error;
}

int
smb2_smb_parse_query_dir(struct mdchain *mdp,
                         struct smb2_query_dir_rq *queryp)
{
	int error;
	uint16_t length;
    uint16_t output_buf_offset;
    
    /* NOTE: queryp must still have the data from building the request */
    
    /* 
     * Parse SMB2 Query Directory 
     * We are already pointing to begining of Response data
     */
    
    /* Check structure size is 9 */
    error = md_get_uint16le(mdp, &length);
    if (error) {
        SMBERROR("failed getting struct size\n");
        goto bad;
    }
    if (length != 9) {
        SMBERROR("Bad struct size: %u\n", (uint32_t) length);
        error = EBADRPC;
        goto bad;
    }
    
    /* Get Output buffer offset */
    error = md_get_uint16le(mdp, &output_buf_offset);
    if (error) {
        SMBERROR("failed getting output buffer offset\n");
        goto bad;
    }
    
    /* Get Output buffer len */
    error = md_get_uint32le(mdp, &queryp->ret_buffer_len);
    if (error) {
        SMBERROR("failed getting output buffer len\n");
        goto bad;
    }
    
    /* 
     * Output buffer offset is from the beginning of SMB2 Header
     * Calculate how much further we have to go to get to it.
     */
    output_buf_offset -= SMB2_HDRLEN;
    output_buf_offset -= 8;   /* already parsed 8 bytes worth of the response */
    
    if (output_buf_offset > 0) {
        error = md_get_mem(mdp, NULL, output_buf_offset, MB_MSYSTEM);
        if (error) {
            SMBERROR("failed getting output buffer offset bytes\n");
            goto bad;
        }
    }
    
bad:    
    return error;
}

int
smb2_smb_parse_query_dir_both_dir_info(struct smb_share *share, struct mdchain *mdp,
                                       uint16_t info_level,
                                       void *ctxp, struct smbfattr *fap,
                                       char *network_name, uint32_t *network_name_len,
                                       size_t max_network_name_buffer_size)
{
    int error, cnt;
	uint32_t next, dattr, file_index = 0;
	uint64_t llint;
    struct smbfs_fctx *ctx = ctxp;
	uint32_t fxsz, recsz = 0;
	uint32_t ea_size;
    uint32_t last_entry = 0;
    struct smb_vc *vcp = SSTOVC(share);
	struct smbmount *smp = share->ss_mount;
    struct finder_file_info file_finfo;
    struct finder_folder_info folder_finfo;
    uint16_t unix_mode = 0;
    
    /* Get Next Offset */
    error = md_get_uint32le(mdp, &next);
    if (error) {
        SMBERROR("failed getting next\n");
        goto bad;
    }
    
    /*
     * if next is set to 0, then there are no more entries to be parsed in the
     * buffer.  Some third party servers will have extra 0 bytes at the end
     * of the last entry, thus we need to rely upon checking next == 0.
     */
    if (next == 0) {
        last_entry = 1;
    }
    
    /* Get File Index */
    error = md_get_uint32le(mdp, &file_index);
    if (error) {
        SMBERROR("failed getting index\n");
        goto bad;
    }
    
    /* Get Create Time */
    error = md_get_uint64le(mdp, &llint);
    if (error) {
        SMBERROR("failed getting crtime\n");
        goto bad;
    }
    if (llint) {
        smb_time_NT2local(llint, &fap->fa_crtime);
    }
    
    /* Get Last Access Time */
    error = md_get_uint64le(mdp, &llint);
    if (error) {
        SMBERROR("failed getting atime\n");
        goto bad;
    }
    if (llint) {
        smb_time_NT2local(llint, &fap->fa_atime);
    }
    
    /* Get Last Write Time */
    error = md_get_uint64le(mdp, &llint);
    if (error) {
        SMBERROR("failed getting wrtime\n");
        goto bad;
    }
    if (llint) {
        smb_time_NT2local(llint, &fap->fa_mtime);
    }
    
    /* Get Last Change Time */
    error = md_get_uint64le(mdp, &llint);
    if (error) {
        SMBERROR("failed getting ctime\n");
        goto bad;
    }
    if (llint) {
        smb_time_NT2local(llint, &fap->fa_chtime);
    }
    
    /* Get EOF */
    error = md_get_uint64le(mdp, &llint);
    if (error) {
        SMBERROR("failed getting eof\n");
        goto bad;
    }
    fap->fa_size = llint;
    
    /* Get Allocation Size */
    error = md_get_uint64le(mdp, &llint);
    if (error) {
        SMBERROR("failed getting alloc size\n");
        goto bad;
    }
    fap->fa_data_alloc = llint;
    
    
    /* Get File Attributes */
    error = md_get_uint32le(mdp, &dattr);
    if (error) {
        SMBERROR("failed getting file attr\n");
        goto bad;
    }
    fap->fa_attr = dattr;
    
    /*
     * Because of the Steve/Conrad Symlinks we can never be completely
     * sure that we have the correct vnode type if its a file. Since we
     * don't support Steve/Conrad Symlinks with Darwin we can always count
     * on the vtype being correct. For directories we always know the
     * correct information.
     */
    if (UNIX_SERVER(vcp) || (fap->fa_attr & SMB_EFA_DIRECTORY)) {
        fap->fa_valid_mask |= FA_VTYPE_VALID;
    }
    fap->fa_vtype = (fap->fa_attr & SMB_EFA_DIRECTORY) ? VDIR : VREG;
    
    /* Get File Name Length */
    error = md_get_uint32le(mdp, network_name_len);
    if (error) {
        SMBERROR("failed getting file name leng\n");
        goto bad;
    }
    
    fxsz = 64; /* size of info up to filename */
    
    /*
     * Confirmed from MS:
     * When the attribute has the Reparse Point bit set then the ea_size
     * contains the reparse tag info. This behavior is consistent for
     * Full, Both, FullId, or BothId query dir calls.  It will pack the
     * reparse tag into the ea_size value if ATTRIBUTE_REPARSE_POINT is set.
     * I verified with local MS Engineers, and they also checking to make
     * sure the behavior is covered in MS-FSA.
     *
     * EAs and reparse points cannot both be in a file at the same
     * time. We return different information for each case.
     */
    fap->fa_valid_mask |= FA_REPARSE_TAG_VALID;
    
    /* Get EA Size */
    error = md_get_uint32le(mdp, &ea_size);	/* extended attributes size */
    if (error) {
        SMBERROR("failed getting ea_size\n");
        goto bad;
    }
    if (fap->fa_attr & SMB_EFA_REPARSE_POINT) {
        fap->fa_reparse_tag = ea_size;
        if (fap->fa_reparse_tag == IO_REPARSE_TAG_SYMLINK) {
            fap->fa_valid_mask |= FA_VTYPE_VALID;
            fap->fa_vtype = VLNK;
        }
    }
    else {
        fap->fa_reparse_tag = IO_REPARSE_TAG_RESERVED_ZERO;
    }
    
    fxsz += 4; /* Add in EA Size */
    
    if (info_level == SMB_FIND_FULL_DIRECTORY_INFO) {
        /* SMB_FIND_FULL_DIRECTORY_INFO does not have short name fields */
        
        /* Get Reserved bytes and ignore it */
        error = md_get_uint32le(mdp, NULL);
        if (error) {
            SMBERROR("failed getting resv\n");
            goto bad;
        }
        
        /*
         * Add in reserve bytes (4)
         */
        fxsz += 4;
    }
    else {
        /* Get Short Name Length and ignore it */
        error = md_get_uint8(mdp, NULL);
        if (error) {
            SMBERROR("failed getting short name len\n");
            goto bad;
        }
        fxsz += 1;  /* Add in Short Name length */
        
        /* Get Reserved byte and ignore it */
        error = md_get_uint8(mdp, NULL);
        if (error) {
            SMBERROR("failed getting resv\n");
            goto bad;
        }
        fxsz += 1;  /* Add in Reserved byte */
        
        if ((vcp->vc_misc_flags & SMBV_OSX_SERVER) &&
            (vcp->vc_server_caps & kAAPL_SUPPORTS_READ_DIR_ATTR)) {
            /* OS X Server that supports readdir_attr shortcuts */
            
            /*
             * ea_size is the max access unless its a reparse point
             */
            if (!(fap->fa_attr & SMB_EFA_REPARSE_POINT)) {
                fap->fa_max_access = ea_size;
                fap->fa_valid_mask |= FA_MAX_ACCESS_VALID;
            }
            else {
                /* If its a reparse point, assume full access */
                fap->fa_max_access = SA_RIGHT_FILE_ALL_ACCESS |
                STD_RIGHT_ALL_ACCESS;
                fap->fa_valid_mask |= FA_MAX_ACCESS_VALID;
            }
            
            /*
             * Next is the resource fork logical length
             */
            error = md_get_uint64le(mdp, &fap->fa_rsrc_size);
            if (error) {
                SMBERROR("failed getting rsrc fork len\n");
                goto bad;
            }
            
            /*
             * Alloc Size is derived from logical size
             */
            fap->fa_rsrc_alloc = smb2_smb_get_alloc_size(smp, fap->fa_rsrc_size);
            
            fap->fa_valid_mask |= FA_RSRC_FORK_VALID;
            
            /*
             * Next is the compressed Finder Info
             */
            if (fap->fa_vtype == VDIR) {
                /* Folder Finder Info */
                memset(&folder_finfo, 0, sizeof(folder_finfo));
                
                error = md_get_uint64le(mdp, &folder_finfo.reserved1);
                if (error) {
                    SMBERROR("failed getting folder finfo reserved1\n");
                    goto bad;
                }
                
                error = md_get_uint16le(mdp, &folder_finfo.finder_flags);
                if (error) {
                    SMBERROR("failed getting folder finfo flags\n");
                    goto bad;
                }
                
                error = md_get_uint16le(mdp, &folder_finfo.finder_ext_flags);
                if (error) {
                    SMBERROR("failed getting folder finfo ext flags\n");
                    goto bad;
                }
                
                error = md_get_uint32le(mdp, &folder_finfo.finder_date_added);
                if (error) {
                    SMBERROR("failed getting folder finfo date added\n");
                    goto bad;
                }
                
                memcpy(&fap->fa_finder_info, &folder_finfo, 32);
            }
            else {
                /* File Finder Info */
                memset(&file_finfo, 0, sizeof(file_finfo));
                
                error = md_get_uint32le(mdp, &file_finfo.finder_type);
                if (error) {
                    SMBERROR("failed getting file finfo type\n");
                    goto bad;
                }
                
                error = md_get_uint32le(mdp, &file_finfo.finder_creator);
                if (error) {
                    SMBERROR("failed getting file finfo creator\n");
                    goto bad;
                }

                error = md_get_uint16le(mdp, &file_finfo.finder_flags);
                if (error) {
                    SMBERROR("failed getting file finfo flags\n");
                    goto bad;
                }
                
                error = md_get_uint16le(mdp, &file_finfo.finder_ext_flags);
                if (error) {
                    SMBERROR("failed getting file finfo ext flags\n");
                    goto bad;
                }
                
                error = md_get_uint32le(mdp, &file_finfo.finder_date_added);
                if (error) {
                    SMBERROR("failed getting file finfo date added\n");
                    goto bad;
                }
                
                memcpy(&fap->fa_finder_info, &file_finfo, 32);
            }
            fap->fa_valid_mask |= FA_FINDERINFO_VALID;

            /*
             * Last is the Unix mode bits if kAAPL_UNIX_BASED is set,
             * otherwise they are just reserved2 to be ignored. If the mode
             * bits are 0 (should be impossible), then ignore them.
             */
            /* Get Unix mode bits */
            error = md_get_uint16le(mdp, &unix_mode);
            if (error) {
                SMBERROR("failed getting unix_mode\n");
                goto bad;
            }

            if ((vcp->vc_server_caps & kAAPL_UNIX_BASED) &&
                (unix_mode != 0)) {
                fap->fa_permissions = unix_mode;
                fap->fa_valid_mask |= FA_UNIX_MODES_VALID;
            }

            /*
             * Parsed 26 bytes of info
             */
            fxsz += 26;
        }
        else {
            /*
             * Get Short name and ignore it
             * Skip 8.3 short name, defined to be 12 WCHAR or 24 bytes.
             */
            error = md_get_mem(mdp, NULL, 24, MB_MSYSTEM);
            if (error) {
                SMBERROR("failed getting short name\n");
                goto bad;
            }
            
            /* 
             * Add in short name (24).
             */
            fxsz += 24;
            
            /* Get Reserved bytes and ignore it */
            error = md_get_uint16le(mdp, NULL);
            if (error) {
                SMBERROR("failed getting resv\n");
                goto bad;
            }
            
            /*
             * Add in reserve bytes (2)
             */
            fxsz += 2;
        }
    }
    
    /* Get File ID and save it */
    error = md_get_uint64le(mdp, &fap->fa_ino);
    if (error) {
        goto bad;
    }
    
    /*
     * Add in File ID (8)
     */
    fxsz += 8;
    
    /* Update recsz to point to next record */
    recsz = next ? next : fxsz + *network_name_len;
    
    /* Get File Name */
	if ((size_t) *network_name_len > max_network_name_buffer_size) {
		*network_name_len = (uint32_t) max_network_name_buffer_size;
    }
	error = md_get_mem(mdp, network_name, *network_name_len, MB_MSYSTEM);
	if (error) {
        SMBERROR("failed getting file name\n");
		return error;
    }
    
    smb2fs_smb_file_id_check(share, fap->fa_ino, network_name, *network_name_len);
    
    /* Get any additional pad bytes before next entry */
	if (next) {
		cnt = next - *network_name_len - fxsz;
		if (cnt > 0) {
			error = md_get_mem(mdp, NULL, cnt, MB_MSYSTEM);
            if (error) {
                SMBERROR("failed getting add pad\n");
                goto bad;
            }
        }
		else if (cnt < 0) {
			SMBERROR("out of sync\n");
			return EBADRPC;
		}
	}
    
	/* Don't count any trailing null in the name. */
    if ((*network_name_len > 1) &&
        (network_name[*network_name_len - 1] == 0) &&
        (network_name[*network_name_len - 2] == 0)) {
        *network_name_len -= 2;
    }
    
	if (*network_name_len == 0) {
		return EBADRPC;
    }
    
    if (ctx != NULL) {
        /* save where we last left off searching */
        ctx->f_resume_file_index = file_index;
        
        next = ctx->f_eofs + recsz;
        ctx->f_eofs = next;
        ctx->f_output_buf_len -= recsz;
        
        if (last_entry == 1) {
            /* only padding bytes left so ignore them */
            ctx->f_output_buf_len = 0;
        }
    }
    else {
        /* Must be only parsing one entry out */
    }
    
bad:
    return error;
}

int
smb2_smb_parse_query_info(struct mdchain *mdp, 
                          struct smb2_query_info_rq *queryp)
{
	int error;
	uint16_t length;
    uint16_t output_buffer_offset;
    uint64_t *inode_numberp;

    /* NOTE: queryp must still have the data from building the request */
    
    /* 
     * Parse SMB2 Query Info Response 
     * We are already pointing to begining of Response data
     */
    
    /* Check structure size is 9 */
    error = md_get_uint16le(mdp, &length);
    if (error) {
        goto bad;
    }
    if (length != 9) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)length);
        error = EBADRPC;
        goto bad;
    }
    
    /* Get Output buffer offset */
    error = md_get_uint16le(mdp, &output_buffer_offset);
    if (error) {
        goto bad;
    }
    
    /* Get Output buffer len */
    error = md_get_uint32le(mdp, &queryp->ret_buffer_len);
    if (error) {
        goto bad;
    }
    if (queryp->ret_buffer_len > queryp->output_buffer_len) {
        SMBERROR("Bad output buffer len: %u > %u\n", 
                 queryp->ret_buffer_len,
                 queryp->output_buffer_len);
        error = EBADRPC;
        goto bad;
    }
    
    /* 
     * Output buffer offset is from the beginning of SMB2 Header
     * Calculate how much further we have to go to get to it.
     */
    output_buffer_offset -= SMB2_HDRLEN;
    output_buffer_offset -= 8;   /* already parsed 8 bytes worth of the response */
    
    if (output_buffer_offset > 0) {
        error = md_get_mem(mdp, NULL, output_buffer_offset, MB_MSYSTEM);
        if (error) {
            goto bad;
        }
    }
    
    /* 
     * If there is an output buffer, then parse it depending on the type 
     * Note: ret_buffer_len of 0 is ok, just means no data found (ie no attr)
     */

    /* Special case stream info parsing since it can handle ret_buffer_len == 0 */
    if ((queryp->info_type == SMB2_0_INFO_FILE) &&
        (queryp->file_info_class == FileStreamInformation)) {
        error = smb2_smb_parse_file_stream_info(mdp, queryp->output_buffer,
                                                queryp->ret_buffer_len);
        goto bad;
    }
    
    if (queryp->ret_buffer_len > 0) {
        switch (queryp->info_type) {
            case SMB2_0_INFO_FILE:
                switch (queryp->file_info_class) {
                    case FileAllInformation:
                        error = smb2_smb_parse_file_all_info(mdp, 
                                                             queryp->output_buffer);
                        break;
                        
                    case FileInternalInformation:
                        inode_numberp = (uint64_t *) queryp->output_buffer;
                        
                        /* Get inode number */
                        error = md_get_uint64le(mdp, inode_numberp);
                        if (error) {
                            goto bad;
                        }
                        break;

                    case FileStreamInformation:
                        /* This was handled above so should never get here */
                        DBG_ASSERT(0);
                        break;
                        
                    default:
                        break;
                }
                break;
                
            case SMB2_0_INFO_FILESYSTEM:
                switch (queryp->file_info_class) {
                    case FileFsAttributeInformation:
                        error = smb2_smb_parse_fs_attr(mdp, 
                                                       queryp->output_buffer);
                        break;
                        
                    case FileFsSizeInformation:
                        error = smb2_smb_parse_fs_size(mdp, 
                                                       queryp->output_buffer);
                        break;
                        
                    default:
                        break;
                }
                break;
                
            case SMB2_0_INFO_SECURITY:
                error = smb2_smb_parse_security(mdp, queryp);
                break;
                
            default:
                break;
        }
    }
    
bad:    
    return error;
}

int
smb2_smb_parse_read_one(struct mdchain *mdp,
                        user_ssize_t *rresid,
                        struct smb2_rw_rq *readp)
{
	int error;
	uint16_t length;
    uint8_t data_offset;
    uint8_t reserved1;
    uint32_t reserved2;
    
    /*
     * Parse SMB2 Read Response
     * We are already pointing to begining of Response data
     */
    
    /* Check structure size is 17 */
    error = md_get_uint16le(mdp, &length);
    if (error) {
        goto bad;
    }
    if (length != 17) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)length);
        error = EBADRPC;
        goto bad;
    }
    
    /* Get Data Offset */
    error = md_get_uint8(mdp, &data_offset);
    if (error) {
        goto bad;
    }
    
    /* Get Reserved1 */
    error = md_get_uint8(mdp, &reserved1);
    if (error) {
        goto bad;
    }
    
    /* Get Data Length read */
    error = md_get_uint32le(mdp, &readp->ret_len);
    if (error) {
        goto bad;
    }
    
    /* Get Data Remaining (always 0) */
    error = md_get_uint32le(mdp, &reserved2);
    if (error) {
        goto bad;
    }
    
    /* Get Reserved2 (always 0) */
    error = md_get_uint32le(mdp, &reserved2);
    if (error) {
        goto bad;
    }
    
    /*
     * Data offset is from the beginning of SMB2 Header
     * Calculate how much further we have to go to get to it.
     */
    data_offset -= SMB2_HDRLEN;
    data_offset -= 16;   /* already parsed 16 bytes worth of the response */
    
    if (data_offset > 0) {
        error = md_get_mem(mdp, NULL, data_offset, MB_MSYSTEM);
        if (error) {
            goto bad;
        }
    }
    
    /* Get the data */
    if (readp->ret_len == 0) {
        /* no data returned */
        *rresid = 0;
    }
    else {
        /* read data into the buffer pointed at by the uio */
		error = md_get_uio(mdp, readp->auio, readp->ret_len);
		if (!error) {
            *rresid = readp->ret_len;
        }
    }
    
bad:
    return error;
}

int
smb2_smb_parse_set_info(struct mdchain *mdp,
                        struct smb2_set_info_rq *infop)
{
#pragma unused(infop)
    int error;
    uint16_t length;
    
    /* 
     * Parse SMB2 Set Info Response 
     * We are already pointing to begining of Response data
     */
    
    /* Check structure size is 2 */
    error = md_get_uint16le(mdp, &length);
    if (error) {
        goto bad;
    }
    if (length != 2) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)length);
        error = EBADRPC;
        goto bad;
    }
    
bad:    
    return error;
}

int
smb2_smb_parse_security(struct mdchain *mdp,
                        struct smb2_query_info_rq *queryp)
{
    int error;
    struct ntsecdesc **resp = (struct ntsecdesc ** ) queryp->output_buffer;
    
    /* 
     * Parse SMB2 Get Security Response
     * We are already pointing to begining of Response data
     */
    
    /* 
     * smb2_smb_parse_query_info already checked that the output buffer
     * is large enough for this reply
     */
    
    /* 
     * All the calling routines expect to get NT Security Descriptor so must 
     * have received at least sizeof(struct ntsecdesc) amount of data
     */
    if (queryp->ret_buffer_len >= sizeof(struct ntsecdesc)) {
        SMB_MALLOC(*resp, struct ntsecdesc *, queryp->ret_buffer_len,
                   M_TEMP, M_WAITOK);
        if (*resp == NULL) {
            error = ENOMEM;
        }
        else {
            error = md_get_mem(mdp, (caddr_t)*resp, queryp->ret_buffer_len,
                               MB_MSYSTEM);
            if (error) {
                SMBERROR("md_get_mem failed %d\n", error);
            }
        }
    }
    else {
        SMBERROR("Output buffer len %d < ntsecdesc %ld\n",
                 queryp->ret_buffer_len, sizeof(struct ntsecdesc));
        error = EBADRPC; 
    }	
    
    return error;
}

int
smb2_smb_parse_svrmsg_notify(struct smb_rq *rqp,
                             uint32_t *svr_action,
                             uint32_t *delay)
{
    int error;
    uint16_t length;
    uint16_t output_buffer_offset;
    uint32_t output_buffer_len;
    struct mdchain *mdp;
    uint32_t next_entry_offset, action, delay_val;
    char *filename = NULL;
    
    /*
     * SvrMsg Notify is an Async request, get reply and parse it now.
     *
     * Note: smb_rq_reply() calls smb_iod_removerq() to remove the
     * rqp from the iod queue.
     */
    error = smb_rq_reply(rqp);
    if (error) {
        SMBDEBUG("smb_rq_reply failed %d\n", error);
        goto done;
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    /*
     * Parse SMB2 Change Notify Response
     */
    
    /* Check structure size is 9 */
    error = md_get_uint16le(mdp, &length);
    if (error) {
        goto done;
    }
    if (length != 9) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)length);
        error = EBADRPC;
        goto done;
    }
    
    /* Get Output buffer offset */
    error = md_get_uint16le(mdp, &output_buffer_offset);
    if (error) {
        goto done;
    }
    
    /* Get Output buffer len */
    error = md_get_uint32le(mdp, &output_buffer_len);
    if (error) {
        goto done;
    }
    
    /* buffer length better not be zero */
    if (!output_buffer_len) {
        SMBDEBUG("svrmsg notify with zero buffer len\n");
        error = EINVAL;
        goto done;
    }
    
    /*
     * Output buffer offset is from the beginning of SMB2 Header
     * Calculate how much further we have to go to get to it.
     */
    output_buffer_offset -= SMB2_HDRLEN;
    output_buffer_offset -= 8;   /* already parsed 8 bytes worth of the response */
    
    if (output_buffer_offset > 0) {
        error = md_get_mem(mdp, NULL, output_buffer_offset, MB_MSYSTEM);
        if (error) {
            goto done;
        }
    }
    
    error = md_get_uint32le(mdp, &next_entry_offset);
    if (error) {
        SMBDEBUG("Parse error getting next_entry_offset: %d", error);
        goto done;
    }
    
    /*
     * Parse the the single FILE_NOTIFY_INFORMATION element
     */
    
    /* action field */
    error = md_get_uint32le(mdp, &action);
    if (error) {
        SMBDEBUG("Parse error getting action: %d\n", error);
        goto done;
    }
    
    /* Get filename */
    error = smb2_smb_parse_create_str(rqp->sr_share, mdp, 0, &filename);
    
    if (error || (filename == NULL)) {
        SMBDEBUG("Parse error getting file name: %d\n", error);
        goto done;
    }
    
    /* Check for correct file name: "com.apple.svrmsg" */
    if (strncmp(filename, "com.apple.svrmsg", strlen("com.apple.svrmsg"))) {
        SMBDEBUG("Unexected filename: %s\n", filename);
        error = EINVAL;
        goto done;
    }
    
    /* translate Change action to svrmsg action */
    switch (action) {
        case FILE_ACTION_REMOVED:
            *svr_action = SVRMSG_SHUTDOWN_START;
            
            /* Get delay parameter  */
            error = md_get_uint32le(mdp, &delay_val);
            if (error) {
                SMBDEBUG("Error parsing delay parameter: %d\n", error);
                goto done;
            }
            
            SMBDEBUG("Received SVRMSG_SHUTDOWN_START, delay: %u\n", delay_val);
            *delay = delay_val;
            
            break;
            
        case FILE_ACTION_ADDED:
            SMBDEBUG("Received SVRMSG_SHUTDOWN_CANCELLED\n");
            *svr_action = SVRMSG_SHUTDOWN_CANCELLED;
            break;
            
        default:
            /* Unknown svrmsg */
            SMBERROR("Unsupported svrmsg, action: %u\n", action);
            error = EINVAL;
            goto done;
            break;
    }
    
done:
    if (filename != NULL) {
        SMB_FREE(filename, M_TEMP);
    }
    return error;
}

static int
smb2_smb_parse_write_one(struct mdchain *mdp,
                         user_ssize_t *rresid,
                         struct smb2_rw_rq *writep)
{
	int error;
	uint16_t length;
    uint16_t reserved1;
    uint32_t reserved2;
    
    /*
     * Parse SMB2 Write Response
     * We are already pointing to begining of Response data
     */
    
    /* Check structure size is 17 */
    error = md_get_uint16le(mdp, &length);
    if (error) {
        goto bad;
    }
    if (length != 17) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)length);
        error = EBADRPC;
        goto bad;
    }

    /* Get Reserved */
    error = md_get_uint16le(mdp, &reserved1);
    if (error) {
        goto bad;
    }
    
    /* Get Data Length written */
    error = md_get_uint32le(mdp, &writep->ret_len);
    if (error) {
        goto bad;
    }
    
    /* Get Data Remaining (always 0) */
    error = md_get_uint32le(mdp, &reserved2);
    if (error) {
        goto bad;
    }
    
    /* Get Reserved2 (always 0) */
    error = md_get_uint32le(mdp, &reserved2);
    if (error) {
        goto bad;
    }
    
    *rresid = writep->ret_len;

bad:
    return error;
}

int
smb2_smb_query_dir(struct smb_share *share, void *args_ptr,
                   struct smb_rq **compound_rqp, vfs_context_t context)
{
    struct smb2_query_dir_rq *queryp = args_ptr;
    struct smb_rq *rqp;
    struct mbchain *mbp;
    struct mdchain *mdp;
    int error;
    uint16_t *name_len, name_offset;
    uint8_t sep_char = '\\';
    SMB2FID smb2_fid;
    
    /* Allocate request and header for a Query Directory */
resend:
    queryp->ret_rqp = NULL;
    error = smb2_rq_alloc(SSTOCP(share), SMB2_QUERY_DIRECTORY, 
                          &queryp->output_buffer_len, 
                          context, &rqp);
    if (error) {
        return error;
    }
    queryp->ret_rqp = rqp;
    
    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB2 Query_Directory Request
     */
    mb_put_uint16le(mbp, 33);                           /* Struct size */
    mb_put_uint8(mbp, queryp->file_info_class);         /* FileInformationClass */
    mb_put_uint8(mbp, queryp->flags);                   /* Flags */
    mb_put_uint32le(mbp, queryp->file_index);           /* FileIndex */

    /* map fid to smb2 fid */
    error = smb_fid_get_kernel_fid(share, queryp->fid, 0, &smb2_fid);
    if (error) {
        goto bad;
    }
    mb_put_uint64le(mbp, smb2_fid.fid_persistent);      /* FID */
    mb_put_uint64le(mbp, smb2_fid.fid_volatile);        /* FID */
    
    name_offset = 96;
    mb_put_uint16le(mbp, name_offset);                  /* FileNameOffset */
    name_len = mb_reserve(mbp, sizeof(uint16_t));       /* FileNameLen */
    mb_put_uint32le(mbp, queryp->output_buffer_len);    /* Output Buf Len */

    /* Add in the name if any */
    if ((queryp->dnp) || (queryp->name_len > 0)) {
        smb2_rq_bstart(rqp, name_len);
        
        /* 
         * SMB2 searches do not want any paths (ie no '\') in the search 
         * pattern. The dir to be searched should already be open.
         * If we have a namep, then use that as the search pattern.
         * If namep is NULL, then use dnp->n_name as the search pattern.
         */
        if (queryp->namep == NULL) {
            /* 
             * no namep, thus dnp->n_parent was opened so look for dnp->n_name 
             * Note: if you pass in dnp to smb2fs_fullpath it will auto add
             * the full path to dnp which could include '\' and we dont want
             * that.
             */
            if (queryp->dnp == NULL) {
                SMBERROR("queryp->dnp is NULL\n");
                error = EINVAL;
                goto bad;
            }
            
            error = smb2fs_fullpath(mbp, NULL,
                                    queryp->dnp->n_name, queryp->dnp->n_nmlen,
                                    NULL, 0,
                                    queryp->name_flags, sep_char);
            
        }
        else {
            /* have a namep, thus dnp was opened so look for namep */
            error = smb2fs_fullpath(mbp, NULL, 
                                    queryp->namep, queryp->name_len,
                                    NULL, 0,
                                    queryp->name_flags, sep_char);
            
        }
        if (error) {
            SMBERROR("error 0x%x from smb_put_dmem for name\n", error);
            goto bad;
        }		
        smb_rq_bend(rqp);           /* now fill in name_len */
    }
    else {
        /* blank name */
        *name_len = htoles(0);      /* set name len to 0 */
        mb_put_uint16le(mbp, 0);    /* fill in blank name 0x0000 */
    }
    
    if (compound_rqp != NULL) {
        /* 
         * building a compound request, add padding to 8 bytes and just
         * return this built request.
         */
        smb2_rq_align8(rqp);
        rqp->sr_flags |= SMBR_COMPOUND_RQ;
        *compound_rqp = rqp;
        return (0);
    }

    error = smb_rq_simple(rqp);
    queryp->ret_ntstatus = rqp->sr_ntstatus;
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            goto resend;
        }
        
        goto bad;
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    error = smb2_smb_parse_query_dir(mdp, queryp);
    if (error) {
        goto bad;
    }
    
bad:
    /* 
     * Note: smb_rq_done will be done in smb2fs_smb_findnext or 
     * smbfs_smb_findclose 
     */
    return error;
}

/*
 * The calling routine must hold a reference on the share
 */
int
smb2_smb_query_info(struct smb_share *share, void *args_ptr, 
                    struct smb_rq **compound_rqp, vfs_context_t context)
{
    struct smb2_query_info_rq *queryp = args_ptr;
	struct smb_rq *rqp = NULL;
	struct mbchain *mbp;
	struct mdchain *mdp;
    uint16_t input_buffer_offset;
	int error;
    SMB2FID smb2_fid;
   
    /* Validate info_type and file_info_class */
    switch (queryp->info_type) {
        case SMB2_0_INFO_FILE:
            switch (queryp->file_info_class) {
                case FileAllInformation:
                case FileInternalInformation:
                case FileStreamInformation:
                    /* These are valid */
                    break;
                default:
                    SMBERROR("Unknown SMB2_0_INFO_FILE/file_info_class: %d\n", 
                             (uint32_t)queryp->file_info_class);
                    error = EINVAL;
                    goto bad;
            }
            break;
            
        case SMB2_0_INFO_FILESYSTEM:
            switch (queryp->file_info_class) {
                case FileFsAttributeInformation:
                case FileFsSizeInformation:
                    /* These are valid */
                    break;
                    
                default:
                    SMBERROR("Unknown SMB2_0_INFO_FILESYSTEM/file_info_class: %d\n", 
                             (uint32_t)queryp->file_info_class);
                    error = EINVAL;
                    goto bad;
            }
            break;
            
        case SMB2_0_INFO_SECURITY:
            if (queryp->file_info_class != 0) {
                SMBERROR("Unknown SMB2_0_INFO_SECURITY/file_info_class: %d\n",
                         (uint32_t)queryp->file_info_class);
                error = EINVAL;
                goto bad;
            }
            break;
            
        default:
            SMBERROR("Unknown info_type: %d\n", (uint32_t)queryp->info_type);
            error = EINVAL;
            goto bad;
    }
    
resend:
    /* Allocate request and header for a Query Info */
    error = smb2_rq_alloc(SSTOCP(share), SMB2_QUERY_INFO, 
                          &queryp->output_buffer_len, 
                          context, &rqp);
    if (error) {
        return error;
    }
    
    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB2 Query_Info Request
     */
    mb_put_uint16le(mbp, 41);                       /* Struct size */
    mb_put_uint8(mbp, queryp->info_type);           /* Info type */
    mb_put_uint8(mbp, queryp->file_info_class);     /* File info class */
    mb_put_uint32le(mbp, queryp->output_buffer_len);/* Output buffer len */
    
    if (queryp->input_buffer_len > 0) {
        input_buffer_offset = SMB2_HDRLEN;
        input_buffer_offset += 40;
        mb_put_uint16le(mbp, input_buffer_offset);  /* Input buffer offset */
    }
    else {
        mb_put_uint16le(mbp, 0);                    /* Input buffer offset */
    }
    mb_put_uint16le(mbp, 0);                        /* Reserved */
    mb_put_uint32le(mbp, queryp->input_buffer_len); /* Input buffer len */
    mb_put_uint32le(mbp, queryp->add_info);         /* Additional info */
    mb_put_uint32le(mbp, queryp->flags);            /* Flags */

    /* map fid to smb2 fid */
    error = smb_fid_get_kernel_fid(share, queryp->fid, 0, &smb2_fid);
    if (error) {
        goto bad;
    }
    mb_put_uint64le(mbp, smb2_fid.fid_persistent);  /* FID */
    mb_put_uint64le(mbp, smb2_fid.fid_volatile);    /* FID */
    
    /* Add input buffer if any */
    if (queryp->input_buffer_len > 0) {
		mb_put_mem(mbp, 
                   (caddr_t) queryp->input_buffer, 
                   queryp->input_buffer_len, 
                   MB_MSYSTEM);
    }

    if (compound_rqp != NULL) {
        /* 
         * building a compound request, add padding to 8 bytes and just
         * return this built request.
         */
        smb2_rq_align8(rqp);
        rqp->sr_flags |= SMBR_COMPOUND_RQ;
        *compound_rqp = rqp;
        return (0);
    }

    error = smb_rq_simple(rqp);
    queryp->ret_ntstatus = rqp->sr_ntstatus;
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            goto resend;
        }
        
        goto bad;
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    error = smb2_smb_parse_query_info(mdp, queryp);
    if (error) {
        goto bad;
    }

bad:
    if (rqp != NULL) {
        smb_rq_done(rqp);
    }
    
    return error;
}

int
smb2_smb_read(struct smb_share *share, void *arg_ptr, vfs_context_t context)
{
    struct smb2_rw_rq *readp = arg_ptr;
	user_ssize_t total_size, len, resid = 0;
	int error = 0;
    
    /* assume we can read it all in one request */
	total_size = uio_resid(readp->auio);
    
	while (total_size > 0) {
        /* read "len" amount of data */
		len = total_size;
        
		error = smb2_smb_read_write_async(share, readp, &len, &resid, 1,
                                          context);
        if (error)
			break;
        
        /* subtract actual amount of data read "resid" from the total left */
		total_size -= resid;
        
		/* Nothing else to read we are done */
		if (!resid) {
			SMB_LOG_IO("Server zero bytes read\n");
			break;
		}
        
		if (resid < len) {
            /* assume nothing more to read, so we are done */
            break;
        }
	}
    
    if (error == ENODATA) {
        /* ntstatus holds the EOF error */
        error = 0;
    }
    
	return error;
}

/*
 * *len is amount of data requested (updated with actual size attempted)
 * *rresid is actual amount of data read
 */
int
smb2_smb_read_one(struct smb_share *share,
                  struct smb2_rw_rq *readp,
                  user_ssize_t *len,
                  user_ssize_t *rresid,
                  struct smb_rq **compound_rqp,
                  vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
    uint32_t min_length = 1;
    SMB2FID smb2_fid;
    uint32_t len32;
    uint32_t remaining = (uint32_t ) *len;
    
    len32 = (uint32_t) MIN(SSTOVC(share)->vc_rxmax, *len);

resend:
    /*
     * Allocate request and header for a Read
     * Available credits may reduce the read size
     */
    error = smb2_rq_alloc(SSTOCP(share), SMB2_READ, &len32, context, &rqp);
    if (error) {
        return error;
    }
    *len = len32;
    
    if (remaining > len32) {
        remaining -= len32;
    }
    else {
        remaining = 0;
    }

    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB2 Read Request
     */
    mb_put_uint16le(mbp, 49);                       /* Struct size */
    mb_put_uint16le(mbp, 0);                        /* Padding and Reserved */
    mb_put_uint32le(mbp, (uint32_t) *len);          /* Length of read */
	mb_put_uint64le(mbp, uio_offset(readp->auio));   /* Offset */

    /* map fid to smb2 fid */
    error = smb_fid_get_kernel_fid(share, readp->fid, 0, &smb2_fid);
    if (error) {
        goto bad;
    }
    mb_put_uint64le(mbp, smb2_fid.fid_persistent);  /* FID */
    mb_put_uint64le(mbp, smb2_fid.fid_volatile);    /* FID */
    
    mb_put_uint32le(mbp, (uint32_t) min_length);    /* Min len of read */
    mb_put_uint32le(mbp, 0);                        /* Channel */
    mb_put_uint32le(mbp, remaining);                /* Remaining */
    mb_put_uint32le(mbp, 0);                        /* Channel offset/len */
    mb_put_uint8(mbp, 0);                           /* Buffer */

    if (compound_rqp != NULL) {
        /* 
         * building a compound request, add padding to 8 bytes and just
         * return this built request.
         */
        smb2_rq_align8(rqp);
        rqp->sr_flags |= SMBR_COMPOUND_RQ;
        *compound_rqp = rqp;
        return (0);
    }

    error = smb_rq_simple(rqp);
    readp->ret_ntstatus = rqp->sr_ntstatus;
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            /* Rebuild and try sending again */
            remaining += len32;
            
            smb_rq_done(rqp);
            rqp = NULL;
            goto resend;
        }
        
        goto bad;
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    error = smb2_smb_parse_read_one(mdp, rresid, readp);
    if (error) {
        goto bad;
    }
    
bad:    
	smb_rq_done(rqp);
    return error;
}

static int
smb2_smb_read_uio(struct smb_share *share, SMBFID fid, uio_t uio,
                  vfs_context_t context)
{
	int error;
 	struct smb2_rw_rq *readp = NULL;
    
    SMB_MALLOC(readp,
               struct smb2_rw_rq *,
               sizeof(struct smb2_rw_rq),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (readp == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        return error;
    }
    
    readp->remaining = 0;
    readp->write_flags = 0;
    readp->fid = fid;
    readp->auio = uio;
    
    error = smb2_smb_read(share, readp, context);
    
    if (readp != NULL) {
        SMB_FREE(readp, M_SMBTEMP);
    }
    
	return error;
}

static int
smb2_smb_read_write_async(struct smb_share *share,
                          struct smb2_rw_rq *in_read_writep,
                          user_ssize_t *len,
                          user_ssize_t *rresid,
                          uint32_t do_read,
                          vfs_context_t context)
{
	int error;
	struct mdchain *mdp;
    int i, j;
    struct quantum {
        struct smb2_rw_rq *read_writep;
        struct smb_rq *rqp;
        int pending;
        user_ssize_t resid;
    };
    struct quantum rw_pb[kMAX_READ_BLOCKS]; /* This should be the larger nbr */
    int done = 0;
    int reconnect = 0;
    struct smb2_rw_rq tmp_read_write;
    user_ssize_t saved_len, saved_rresid;
    int max_pb;
        
    /*
     * This function does multiple Async Reads/Writes
     */
    
    /* Save values in case reconnect occurs */
    saved_len = *len;
    saved_rresid = *rresid;
    
    /* Is there enough data to warrent a compound read/write? */
    if (do_read) {
        max_pb = kMAX_READ_BLOCKS;
        if (*len <= SSTOVC(share)->vc_rxmax) {
            /* Only need single read */
            return (smb2_smb_read_one(share, in_read_writep, len, rresid, NULL,
                                      context));
        }
    }
    else {
        max_pb = kMAX_WRITE_BLOCKS;
        if (*len <= SSTOVC(share)->vc_wxmax) {
            /* Only need single write */
            return (smb2_smb_write_one(share, in_read_writep, len, rresid, NULL,
                                       context));
        }
    }
    
    /* Use a temp smb2_rw_rq instead of in_read_writep */
    bzero(&tmp_read_write, sizeof(tmp_read_write));
    tmp_read_write.remaining = in_read_writep->remaining;
    tmp_read_write.write_flags = in_read_writep->write_flags;
    tmp_read_write.fid = in_read_writep->fid;
    tmp_read_write.io_len = in_read_writep->io_len;
    tmp_read_write.auio = uio_duplicate(in_read_writep->auio);
    
    /* Zero out param blocks */
    for (i = 0; i < max_pb; i++) {
        rw_pb[i].read_writep = NULL;
        rw_pb[i].rqp = NULL;
        rw_pb[i].pending = 0;
        rw_pb[i].resid = 0;
    }
    
resend:
    
    /* Fill in initial requests */
    for (i = 0; i < max_pb; i++) {
        /* Malloc the read_writep */
        SMB_MALLOC(rw_pb[i].read_writep,
                   struct smb2_rw_rq *,
                   sizeof(struct smb2_rw_rq),
                   M_SMBTEMP,
                   M_WAITOK | M_ZERO);
        if (rw_pb[i].read_writep == NULL) {
            SMBERROR("SMB_MALLOC failed\n");
            error = ENOMEM;
            goto bad;
        }
        
        /*
         * Fill in the Read/Write request
         */
        error = smb2_smb_read_write_fill(share, &tmp_read_write,
                                         rw_pb[i].read_writep, &rw_pb[i].rqp,
                                         do_read, context);
        
        if (error) {
            if (error == ENOBUFS) {
                /* Running out of credits, clear error and send what we have */
                SMBDEBUG("low on credits %d\n", error);
                error = 0;
                
                /* i equals number of initial rw_pb's filled in */
                i += 1;
                break;
            }
            else {
                SMBERROR("smb2_smb_fillin_read/write failed %d\n", error);
                goto bad;
            }
        }
        
        /* Any more to read or write? If not, then send what we have */
        if (uio_resid(tmp_read_write.auio) == 0) {
            /* i equals number of initial rw_pb's filled in */
            i += 1;
            break;
        }
    }
    
    /*
     * Send initial requests
     */
    for (j = 0; j < i; j++) {
        error = smb_iod_rq_enqueue(rw_pb[j].rqp);
        if (error) {
            SMBERROR("smb_iod_rq_enqueue failed %d\n", error);
            goto bad;
        }
        rw_pb[j].pending = 1;
    }
    
    /* Wait for replies and refill requests as needed */
    while (!done) {
        /* Assume we are done */
        done = 1;
        
        for (j = 0; j < i; j++) {
            if (rw_pb[j].pending == 1) {
                error = smb_rq_reply(rw_pb[j].rqp);
                rw_pb[j].pending = 0;
                if (error) {
                    if (rw_pb[j].rqp->sr_flags & SMBR_RECONNECTED) {
                        SMBDEBUG("reconnected on read/write[%d]\n", j);
                        reconnect = 1;
                    }
                    else {
                        SMBERROR("smb_rq_reply failed %d\n", error);
                    }
                    
                    goto bad;
                }
                
                if (!tmp_read_write.ret_ntstatus) {
                    tmp_read_write.ret_ntstatus = rw_pb[j].rqp->sr_ntstatus;
                }
                
                /* Now get pointer to response data */
                smb_rq_getreply(rw_pb[j].rqp, &mdp);
                
                if (do_read) {
                    error = smb2_smb_parse_read_one(mdp,
                                                    &rw_pb[j].resid,
                                                    rw_pb[j].read_writep);
                }
                else {
                    error = smb2_smb_parse_write_one(mdp,
                                                     &rw_pb[j].resid,
                                                     rw_pb[j].read_writep);
                }
                
                if (error) {
                    SMBERROR("parse failed %d\n", error);
                    goto bad;
                }
                
                /*
                 * Make sure resid is same as actual amt of data we asked
                 * for.  If we are reading from a named pipe (i.e. svrsvc),
                 * this can actually happen (read less from the pipe
                 * than we requested).
                 */
                if (rw_pb[j].resid != rw_pb[j].read_writep->io_len) {
                    SMBERROR("IO Mismatched. Requested %lld but got %lld\n",
                             rw_pb[j].read_writep->io_len, rw_pb[j].resid);
                }
                
                /* Add up amount of IO that we have done so far */
                *rresid += rw_pb[j].resid;
                tmp_read_write.ret_len += rw_pb[j].resid;
                
                if (uio_resid(tmp_read_write.auio)) {
                    /* More data to request */
                    error = smb2_smb_read_write_fill(share,
                                                     &tmp_read_write,
                                                     rw_pb[j].read_writep,
                                                     &rw_pb[j].rqp,
                                                     do_read,
                                                     context);
                    
                    if ((error) && !(error == ENOBUFS)) {
                        /* Being low on credits is ok to ignore */
                        SMBERROR("smb2_smb_fillin_read/write2 failed %d\n", error);
                        goto bad;
                    }
                    
                    error = smb_iod_rq_enqueue(rw_pb[j].rqp);
                    if (error) {
                        SMBERROR("smb_iod_rq_enqueue failed %d\n", error);
                        goto bad;
                    }
                    rw_pb[j].pending = 1;
                    
                    /* Not done yet */
                    done = 0;
                }
            }
        }
    }
    
bad:
    /* Cleanup time */
    for (i = 0; i < max_pb; i++) {
        /* If it has not finished, then wait for it to finish */
        if (rw_pb[i].pending == 1) {
            error = smb_rq_reply(rw_pb[i].rqp);
            rw_pb[i].pending = 0;
        }
        
        if (rw_pb[i].rqp != NULL) {
            smb_rq_done(rw_pb[i].rqp);
            rw_pb[i].rqp = NULL;
        }
        
        if (rw_pb[i].read_writep != NULL) {
            if (rw_pb[i].read_writep->auio) {
                uio_free(rw_pb[i].read_writep->auio);
                rw_pb[i].read_writep->auio = NULL;
            }
            
            SMB_FREE(rw_pb[i].read_writep, M_SMBTEMP);
            rw_pb[i].read_writep = NULL;
        }
        
        rw_pb[i].resid = 0;
    }

    if (reconnect == 1) {
        /* If failed due to reconnect, restore original values and try again */
        *len = saved_len;
        *rresid = saved_rresid;

        if (tmp_read_write.auio) {
            uio_free(tmp_read_write.auio);
        }

        bzero(&tmp_read_write, sizeof(tmp_read_write));
        tmp_read_write.remaining = in_read_writep->remaining;
        tmp_read_write.write_flags = in_read_writep->write_flags;
        tmp_read_write.fid = in_read_writep->fid;
        tmp_read_write.io_len = in_read_writep->io_len;
        tmp_read_write.auio = uio_duplicate(in_read_writep->auio);
        
        reconnect = 0;
        goto resend;
    }
    
    /* Always update the ret_ntstatus */
    in_read_writep->ret_ntstatus = tmp_read_write.ret_ntstatus;
    
    if ((error == 0) || ((error == ENODATA) && (do_read) && (*rresid != 0))) {
        /*
         *
         * No errors, or we ENODATA and we are returning some
         * data (this can occur when reading from named pipes).
         * So update the rest of the values
         */
        in_read_writep->remaining = tmp_read_write.remaining;
        in_read_writep->io_len = tmp_read_write.io_len;
        in_read_writep->ret_len = tmp_read_write.ret_len;

        uio_update(in_read_writep->auio, *rresid);
    }

    if (tmp_read_write.auio) {
        uio_free(tmp_read_write.auio);
    }
    
	return error;
}

static int
smb2_smb_read_write_fill(struct smb_share *share,
                         struct smb2_rw_rq *master_read_writep,
                         struct smb2_rw_rq *read_writep,
                         struct smb_rq **rqp,
                         uint32_t do_read,
                         vfs_context_t context)
{
	int error;
	user_ssize_t len, saved_len, resid = 0;
    
    if (do_read) {
        len = MIN(SSTOVC(share)->vc_rxmax, uio_resid(master_read_writep->auio));
    }
    else {
        len = MIN(SSTOVC(share)->vc_wxmax, uio_resid(master_read_writep->auio));
    }
    
    saved_len = len;
    
    /*
     * Free previous info
     */
    if (*rqp != NULL) {
        smb_rq_done(*rqp);
        *rqp = NULL;
    }
    
    if (read_writep->auio) {
        uio_free(read_writep->auio);
        read_writep->auio = NULL;
    }
    
    /*
     * Fill in the Read/Write call
     */
    read_writep->remaining = master_read_writep->remaining;
    read_writep->write_flags = master_read_writep->write_flags;
    read_writep->fid = master_read_writep->fid;
    read_writep->auio = uio_duplicate(master_read_writep->auio);
    read_writep->ret_ntstatus = 0;
    read_writep->ret_len = 0;
    
    if (do_read) {
        error = smb2_smb_read_one(share, read_writep, &len, &resid, rqp,
                                  context);
    }
    else {
        error = smb2_smb_write_one(share, read_writep, &len, &resid, rqp,
                                   context);
    }
    
    if (error) {
        SMBERROR("smb2_smb_read/write_one failed %d\n", error);
        goto bad;
    }
    
    if (len != saved_len) {
        error = ENOBUFS;
    }
    
    /* Save actual io len that is being requested */
    read_writep->io_len = len;
    
    /* In this situation, its not a compound request */
    (*rqp)->sr_flags &= ~SMBR_COMPOUND_RQ;
    
    if (do_read == 0) {
        (*rqp)->sr_timo = SMBWRTTIMO;
    }
    else {
        (*rqp)->sr_timo = (*rqp)->sr_vc->vc_timo;
    }
	(*rqp)->sr_state = SMBRQ_NOTSENT;
    
    /* ASSUME read/write will work and read entire amount */
    uio_update(master_read_writep->auio, len);
    
bad:
    return (error);
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smb_smb_read(struct smb_share *share, SMBFID fid, uio_t uio, vfs_context_t context)
{
    int error;
    
    if (SSTOVC(share)->vc_flags & SMBV_SMB2) {
        error = smb2_smb_read_uio(share, fid, uio, context);
    }
    else {
        error = smb1_read(share, fid, uio, context);
    }
    
    return (error);
}

int
smb2_smb_set_info(struct smb_share *share, void *args_ptr, 
                  struct smb_rq **compound_rqp, vfs_context_t context)
{
    struct smb2_set_info_rq *infop = args_ptr;
    struct smb_rq *rqp;
    struct mbchain *mbp;
    struct mdchain *mdp;
    int error;
    uint32_t *buffer_len;
    uint64_t *llptr;
    struct smb2_set_info_file_basic_info *basic_infop;
    struct smb2_set_info_file_rename_info *renamep;
    struct smb2_set_info_security *sec_infop;
    uint8_t *reserved;
    uint32_t *name_len;
	uint8_t sep_char = '\\';
    uint32_t len;
    uint32_t junk_bytes;
    SMB2FID smb2_fid;
    uint16_t control_flags;
	uint32_t offset;
	struct ntsecdesc ntsd;
    
resend:
    /*
     * Allocate request and header for a Set Info 
     * NOTE: at this time, all the Set Infos are < 64K which fits in 1 credit
     */
    error = smb2_rq_alloc(SSTOCP(share), SMB2_SET_INFO, NULL, context, &rqp);
    if (error) {
        return error;
    }
    
    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB2 Set Info Request
     */
    mb_put_uint16le(mbp, 33);                       /* Struct size */
    mb_put_uint8(mbp, infop->info_type);            /* Info Type */
    mb_put_uint8(mbp, infop->file_info_class);      /* File Info Class */
    buffer_len = mb_reserve(mbp, sizeof(uint32_t)); /* Buffer Len */
    mb_put_uint16le(mbp, SMB2_HDRLEN + 32);         /* Buffer Offset */
    mb_put_uint16le(mbp, 0);                        /* Reserved */
    mb_put_uint32le(mbp, infop->add_info);          /* Additional Info */

    /* map fid to smb2 fid */
    error = smb_fid_get_kernel_fid(share, infop->fid, 0, &smb2_fid);
    if (error) {
        goto bad;
    }
    mb_put_uint64le(mbp, smb2_fid.fid_persistent);  /* FID */
    mb_put_uint64le(mbp, smb2_fid.fid_volatile);    /* FID */
    
    /* 
     * buffer len and buffer depend on File Info Class 
     * NOTE: if a new File Info Class is added that is larger then 64K, then
     * have to change the above smb2_rq_alloc to set currect credit charge!
     */
    switch (infop->file_info_class) {
        case FileAllocationInformation:
            /* 
             * buffer len = 8
             * input buffer point to uint64_t 
             */
            *buffer_len = htolel(8);
            
            llptr = (uint64_t *) infop->input_buffer;
            mb_put_uint64le(mbp, *llptr);           /* New Alloc Size */
            break;
            
        case FileBasicInformation:
            /* 
             * buffer len = 40
             * input buffer point to struct smb2_set_info_file_basic_info 
             */
            *buffer_len = htolel(40);
            
            basic_infop = (struct smb2_set_info_file_basic_info *) infop->input_buffer;
            mb_put_uint64le(mbp, basic_infop->create_time); /* Create time */
            mb_put_uint64le(mbp, basic_infop->access_time); /* Access time */
            mb_put_uint64le(mbp, basic_infop->write_time);  /* Write time */
            mb_put_uint64le(mbp, basic_infop->change_time); /* Change time */
            mb_put_uint32le(mbp, basic_infop->attributes);  /* Attributes */
            mb_put_uint32le(mbp, 0);                       /* Reserved */
            break;
            
        case FileDispositionInformation:
            /* 
             * buffer len = 1
             * input buffer point to uint8_t 
             */
            *buffer_len = htolel(1);
            mb_put_uint8(mbp, *infop->input_buffer); /* Delete on close byte */
            break;
            
        case FileEndOfFileInformation:
            /* 
             * buffer len = 8
             * input buffer point to uint64_t 
             */
            *buffer_len = htolel(8);	
            
            llptr = (uint64_t *) infop->input_buffer;
            mb_put_uint64le(mbp, *llptr);           /* new EOF */
            break;
            
        case FileRenameInformation:
            /* 
             * buffer len = 20 + dest file name len
             * input buffer point to struct smb2_set_info_file_rename_info 
             */
            renamep = (struct smb2_set_info_file_rename_info *) infop->input_buffer;
            
            mb_put_uint8(mbp, renamep->replace_if_exists);  /* Replace Flag */
            reserved = mb_reserve(mbp, 7);                  /* Reserved */
            bzero(reserved, 7);
            mb_put_uint64le(mbp, 0);                        /* Root Dir (0) */
            name_len = mb_reserve(mbp, 4);                  /* File Name Len */
            
            /* Insert the destination name */
            if ((renamep->tname_len > 0) || (renamep->tdnp != NULL)) {
                smb2_rq_bstart32(rqp, name_len);
                error = smb2fs_fullpath(mbp, renamep->tdnp, 
                                        renamep->tnamep, renamep->tname_len,
                                        NULL, 0,
                                        UTF_SFM_CONVERSIONS, sep_char);
                if (error) {
                    SMBERROR("error %d from smb_put_dmem for name\n", error);
                    goto bad;
                }
                smb_rq_bend32(rqp);           /* now fill in name_len */
                
                /* 
                 * Need to add 6 bytes of junk to match Win Client.
                 * Also makes WireShark happy with decode
                 */
                junk_bytes = 4;
                mb_put_mem(mbp, NULL, junk_bytes, MB_MZERO);
                
                /* Fill in buffer len */
                len = 20 + letohl(*name_len) + junk_bytes;
                *buffer_len = htolel(len);	
            }
            else {
                /* blank name is not allowed */
                error = EINVAL;
                goto bad;
            }
            
            break;
            
        default:
            if (infop->info_type != SMB2_0_INFO_SECURITY) {
                SMBERROR("Unknown file info class: %d\n", 
                         (uint32_t)infop->file_info_class);
                error = EINVAL;
                goto bad;
            }
    }

    if (infop->info_type == SMB2_0_INFO_SECURITY) {
        /* Build the NT Security Descriptor */
        sec_infop = (struct smb2_set_info_security *) infop->input_buffer;
        
        bzero(&ntsd, sizeof ntsd);
        ntsd.Revision = 0x01;	/* Should we make this a define? */
        /*
         * A note about flags ("SECURITY_DESCRIPTOR_CONTROL" in MSDN)
         * We set here only those bits we can be sure must be set.  The rest
         * are up to the caller.  In particular, the caller may intentionally
         * set an acl PRESENT bit while giving us a null pointer for the
         * acl - that sets a null acl, denying access to everyone.
         */
        control_flags = sec_infop->control_flags;
        control_flags |= SE_SELF_RELATIVE;
        
        offset = (uint32_t)sizeof(ntsd);
        
        if (sec_infop->owner) {
            ntsd.OffsetOwner = htolel(offset);
            offset += (uint32_t)sidlen(sec_infop->owner);
        }

        if (sec_infop->group) {
            ntsd.OffsetGroup = htolel(offset);
            offset += (uint32_t)sidlen(sec_infop->group);
        }

        if (sec_infop->sacl) {
            control_flags |= SE_SACL_PRESENT | SE_SACL_AUTO_INHERITED | SE_SACL_AUTO_INHERIT_REQ;
            ntsd.OffsetSacl = htolel(offset);
            offset += acllen(sec_infop->sacl);
        }

        if (sec_infop->dacl) {
            control_flags |= SE_DACL_PRESENT | SE_DACL_AUTO_INHERITED | SE_DACL_AUTO_INHERIT_REQ;
            ntsd.OffsetDacl = htolel(offset);
            offset += acllen(sec_infop->dacl);
        }
        
        /* Fill Buffer Len */
        *buffer_len = htolel(offset);
        
        ntsd.ControlFlags = htoles(control_flags);
        
        /* Put in the NT Security Descriptor Header */
        mb_put_mem(mbp, (caddr_t)&ntsd, sizeof ntsd, MB_MSYSTEM);
        
        /* If owner, add it in */
        if (sec_infop->owner)
            mb_put_mem(mbp, (caddr_t)sec_infop->owner, sidlen(sec_infop->owner),
                       MB_MSYSTEM);
        
        /* If group, add it in */
        if (sec_infop->group)
            mb_put_mem(mbp, (caddr_t)sec_infop->group, sidlen(sec_infop->group),
                       MB_MSYSTEM);
        
        /* If SACL, add it in */
        if (sec_infop->sacl)
            mb_put_mem(mbp, (caddr_t)sec_infop->sacl, acllen(sec_infop->sacl),
                       MB_MSYSTEM);
        
        /* If DACL, add it in */
        if (sec_infop->dacl)
            mb_put_mem(mbp, (caddr_t)sec_infop->dacl, acllen(sec_infop->dacl),
                       MB_MSYSTEM);
    }
    
    if (compound_rqp != NULL) {
        /* 
         * building a compound request, add padding to 8 bytes and just
         * return this built request.
         */
        smb2_rq_align8(rqp);
        rqp->sr_flags |= SMBR_COMPOUND_RQ;
        *compound_rqp = rqp;
        return (0);
    }

    error = smb_rq_simple(rqp);
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            goto resend;
        }
        
        goto bad;
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    error = smb2_smb_parse_set_info(mdp, infop);
    if (error) {
        goto bad;
    }

bad:    
    smb_rq_done(rqp);
    return error;
}

int 
smb2_smb_tree_connect(struct smb_vc *vcp, struct smb_share *share,
                      const char *serverName, size_t serverNameLen,
                      vfs_context_t context)
{
#pragma unused(vcp)
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
    uint16_t structure_size;
    uint8_t share_type;
    uint8_t reserved;
    
	share->ss_tree_id = SMB2_TID_UNKNOWN;
    
resend:
    /* Allocate request and header for a Tree Connect */
	error = smb2_rq_alloc(SSTOCP(share), SMB2_TREE_CONNECT, NULL, context, &rqp);
	if (error)
		goto treeconnect_exit;
    
    /* 
     * Build the Tree Connect Request
     * Cant use struct cast because of the var length path name
     */
    smb_rq_getrequest(rqp, &mbp);
    
    mb_put_uint16le(mbp, 9);       /* Struct size */
    mb_put_uint16le(mbp, 0);       /* Reserved */
    mb_put_uint16le(mbp, 72);      /* Path Offset */
	smb_rq_bstart(rqp);            /* Path Length - save ptr to it */
    
    /* start with '\\' */
	smb_put_dmem(mbp, "\\\\", 2, NO_SFM_CONVERSIONS, TRUE, NULL);
    
	/*
	 * User land code now passes down the server name in the proper format that 
	 * can be used in a tree connection. This is too complicated of an issue to
	 * be handle in the kernel, but we do know the following:
	 *
	 * The server's NetBIOS name will always work, but we can't always get it because
	 * of firewalls. Window cluster system require the name to be a NetBIOS
	 * name or the cluster's fully qualified dns domain name.
	 *
	 * Windows XP will not allow DNS names to be used and in fact requires a
	 * name that must fit in a NetBIOS name. So if we don't have the NetBIOS
	 * name we can send the IPv4 address in presentation form (xxx.xxx.xxx.xxx).
	 *
	 * If we are doing IPv6 then it looks like we can just send the server name
	 * provided by the user. The same goes for Bonjour names. 
	 *
	 * We now always use the name passed in and let the calling routine decide.
	 */
    /* Then Server Name */
	error = smb_put_dmem(mbp, 
                         serverName, 
                         serverNameLen, 
                         NO_SFM_CONVERSIONS,
                         TRUE,
                         NULL);
	if (error) {
		SMBERROR("error %d from smb_put_dmem for srvr name\n", error);
		goto bad;
	}		
    
	/* Then '\' */
    error = smb_put_dmem(mbp, "\\", 1, NO_SFM_CONVERSIONS, TRUE, NULL);
	if (error) {
		SMBERROR("error %d from smb_put_dmem for back slash\n", error);
		goto bad;
	}		
    
    /* 
     * Finally add the share name. 
     * Dont use smb_put_dstring since I dont want the ending null bytes
     */
    error = smb_put_dmem(mbp, 
                         share->ss_name, 
                         strnlen(share->ss_name, SMB_MAXSHARENAMELEN), 
                         NO_SFM_CONVERSIONS,
                         TRUE, 
                         NULL);
	if (error) {
		SMBERROR("error %d from smb_put_dmem for share name\n", error);
		goto bad;
	}		
    
	smb_rq_bend(rqp);           /* now fill in the actual Path Length */
    
	error = smb_rq_simple(rqp);
	if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            goto resend;
        }
        
		goto bad;
    }
    
    /* Now get pointer to response data */
	smb_rq_getreply(rqp, &mdp);
    
    /* 
     * Parse SMB2 Tree Connect Response 
     * We are already pointing to begining of Response data
     */
    
    /* Save tree id */
    share->ss_tree_id = rqp->sr_rsptreeid;
    
    /* Check structure size is 16 */
    error = md_get_uint16le(mdp, &structure_size);
    if (error)
        goto bad;
    if (structure_size != 16) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)structure_size);
        error = EBADRPC;
        goto bad;
    }
    
    /* Get Share Type */
    /* %%% To Do - do we do anything with this??? */
    error = md_get_uint8(mdp, &share_type);
    if (error)
        goto bad;
    share->ss_share_type = share_type;
    
    /* Get Reserved byte */
    error = md_get_uint8(mdp, &reserved);
    if (error)
        goto bad;
    
    /* Get Share Flags */
    error = md_get_uint32le(mdp, &share->ss_share_flags);
    if (error)
        goto bad;
    
    /* Get Capabilities */
    error = md_get_uint32le(mdp, &share->ss_share_caps);
    if (error)
        goto bad;
    
    /* Map SMB2 capabilities to SMB1 share->optionalSupport */
    if (share->ss_share_caps & SMB2_SHARE_CAP_DFS) {
        share->optionalSupport |= SMB_SHARE_IS_IN_DFS;
    }
    
    /* Get Maximal Access */
    error = md_get_uint32le(mdp, &share->maxAccessRights);
    if (error)
        goto bad;
    
    /* Update share state */
	lck_mtx_lock(&share->ss_stlock);
	share->ss_flags |= SMBS_CONNECTED;
	lck_mtx_unlock(&share->ss_stlock);
bad:
	smb_rq_done(rqp);
    
treeconnect_exit:
	return error;
}

static int 
smb2_smb_tree_disconnect(struct smb_share *share, vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
	uint16_t length;

	if (share->ss_tree_id == SMB2_TID_UNKNOWN) {
        /* no valid tree id to disconnect on, just ignore */
		return 0;
    }
    
resend:
    /* Allocate request and header for a Tree Disconnect */
	error = smb2_rq_alloc(SSTOCP(share), SMB2_TREE_DISCONNECT, NULL, 
                          context, &rqp);
	if (error) {
		return error;
    }
    
    /* 
     * Fill in Tree Disconnect part 
     * Dont use struct ptr since its only 4 bytes long
     */
    smb_rq_getrequest(rqp, &mbp);
    
    mb_put_uint16le(mbp, 4);       /* Struct size */
    mb_put_uint16le(mbp, 0);       /* Reserved */
    
    error = smb_rq_simple(rqp);
	share->ss_tree_id = SMB2_TID_UNKNOWN;   /* dont care if got error or not */
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            goto resend;
        }
        
        goto bad;
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    /* 
     * Parse SMB2 Tree Disconnect 
     * We are already pointing to begining of Response data
     */
    
    /* Check structure size is 4 */
    error = md_get_uint16le(mdp, &length);
    if (error) {
        goto bad;
    }
    if (length != 4) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)length);
        error = EBADRPC;
        goto bad;
    }
    
bad:
	smb_rq_done(rqp);
	return error;
}

int 
smb_smb_treedisconnect(struct smb_share *share, vfs_context_t context)
{
    struct smb_vc *vcp = SSTOVC(share);
	int error;

    if (vcp->vc_flags & SMBV_SMB2) {
        error = smb2_smb_tree_disconnect(share, context);
    }
    else {
        error = smb1_smb_treedisconnect(share, context);
    }
    return (error);
}

int
smb2_smb_write(struct smb_share *share, void *arg_ptr, vfs_context_t context)
{
    struct smb2_rw_rq *writep = arg_ptr;
	int error = 0;
	user_ssize_t orig_resid, len, total_size, resid = 0;
	off_t orig_offset;
    
    /* assume we can write it all in one request */
	total_size = orig_resid = uio_resid(writep->auio);
	orig_offset = uio_offset(writep->auio);
	
	while (total_size > 0) {
        /* write "len" amount of data */
		len = total_size;
        
		error = smb2_smb_read_write_async(share, writep, &len, &resid, 0,
                                          context);
		if (error) {
			break;
        }
        
        /* did we fail to write out the data? */
		if (resid < len) {
			error = EIO;
			break;
		}
        
        /* subtract actual amount of data written "resid" from the total left */
		total_size -= resid;
	}
    
	if (error) {
		/*
		 * Errors can happen on the copyin, the rpc, etc.  So they
		 * imply resid is unreliable.  The only safe thing is
		 * to pretend zero bytes made it.  We needn't restore the
		 * iovs because callers don't depend on them in error
		 * paths - uio_resid and uio_offset are what matter.
		 */
		uio_setresid(writep->auio, orig_resid);
		uio_setoffset(writep->auio, orig_offset);
	}
	return error;
}

/*
 * "*len" is amount of data requested (updated with actual size attempted)
 * "*rresid" is actual amount of data written
 */
static int 
smb2_smb_write_one(struct smb_share *share,
                   struct smb2_rw_rq *writep,
                   user_ssize_t *len,
                   user_ssize_t *rresid,
                   struct smb_rq **compound_rqp,
                   vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
    uint16_t data_offset;
    SMB2FID smb2_fid;
    uint32_t len32;
    uint32_t remaining = (uint32_t ) *len;

	len32 = (uint32_t) MIN(SSTOVC(share)->vc_wxmax, *len);

resend:
    /*
     * Allocate request and header for a Write
     * Available credits may reduce the write size
     */
    error = smb2_rq_alloc(SSTOCP(share), SMB2_WRITE, &len32, context, &rqp);
    if (error) {
        return error;
    }
    *len = len32;
    
    if (remaining > len32) {
        remaining -= len32;
    }
    else {
        remaining = 0;
    }

    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB2 Write Request
     */
    mb_put_uint16le(mbp, 49);                       /* Struct size */
    data_offset = SMB2_HDRLEN;
    data_offset += 48;
    mb_put_uint16le(mbp, data_offset);              /* Data Offset */
    mb_put_uint32le(mbp, (uint32_t) *len);          /* Length of write */
	mb_put_uint64le(mbp, uio_offset(writep->auio)); /* Offset */

    /* map fid to smb2 fid */
    error = smb_fid_get_kernel_fid(share, writep->fid, 0, &smb2_fid);
    if (error) {
        goto bad;
    }
    mb_put_uint64le(mbp, smb2_fid.fid_persistent);  /* FID */
    mb_put_uint64le(mbp, smb2_fid.fid_volatile);    /* FID */

    mb_put_uint32le(mbp, 0);                        /* Channel */
    mb_put_uint32le(mbp, remaining);                /* Remaining */
    mb_put_uint32le(mbp, 0);                        /* Channel offset/len */
    mb_put_uint32le(mbp, writep->write_flags);      /* Write flags */
    error = mb_put_uio(mbp, writep->auio, (int)*len); /* Write data */
    if (error) {
        goto bad;
    }
    
    if (compound_rqp != NULL) {
        /*
         * building a compound request, add padding to 8 bytes and just
         * return this built request.
         *
         * <14560979> Some servers cant handle this 8 byte alignment, so 
         * disable it for now. At this time, we never use compound writes so
         * this should be fine for now.
         */
        //smb2_rq_align8(rqp);
        rqp->sr_flags |= SMBR_COMPOUND_RQ;
        *compound_rqp = rqp;
        return (0);
    }

    error = smb_rq_simple_timed(rqp, SMBWRTTIMO);
    writep->ret_ntstatus = rqp->sr_ntstatus;
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            /* Rebuild and try sending again */
            remaining += len32;
            
            smb_rq_done(rqp);
            rqp = NULL;
            goto resend;
        }
        
        goto bad;
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    error = smb2_smb_parse_write_one(mdp, rresid, writep);
    if (error) {
        goto bad;
    }
    
bad:    
	smb_rq_done(rqp);
    return error;
}

static int
smb2_smb_write_uio(struct smb_share *share, SMBFID fid, uio_t uio, int ioflag,
                   vfs_context_t context)
{
    int error;
	uint32_t writeMode = (ioflag & IO_SYNC) ? SMB2_WRITEFLAG_WRITE_THROUGH : 0;
 	struct smb2_rw_rq *writep = NULL;
        
    SMB_MALLOC(writep, 
               struct smb2_rw_rq *, 
               sizeof(struct smb2_rw_rq), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (writep == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        return error;
    }
    
    writep->remaining = 0;
    writep->write_flags = writeMode;
    writep->fid = fid;
    writep->auio = uio;
    
    error = smb2_smb_write(share, writep, context);
    
    if (writep != NULL) {
        SMB_FREE(writep, M_SMBTEMP);
    }
    
	return error;
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smb_smb_write(struct smb_share *share, SMBFID fid, uio_t uio, int ioflag,
              vfs_context_t context)
{
    int error;
    
    if (SSTOVC(share)->vc_flags & SMBV_SMB2) {
        error = smb2_smb_write_uio(share, fid, uio, ioflag, context);
    }
    else {
        error = smb1_write(share, fid, uio, ioflag, context);
    }
    
    return (error);
}


static uint32_t
smb2_vc_maxread(struct smb_vc *vcp)
{
	if (vcp->vc_sopt.sv_maxread > smb_maxread) {
		return smb_maxread;
	}
	return vcp->vc_sopt.sv_maxread;
}

static uint32_t
smb2_vc_maxtransact(struct smb_vc *vcp)
{
	uint32_t socksize = vcp->vc_sopt.sv_maxtransact;
	uint32_t maxmsgsize;
    
	/* Make sure we never use a size bigger than the socket can support */
	SMB_TRAN_GETPARAM(vcp, SMBTP_SNDSZ, &socksize);
    
    maxmsgsize = MIN(vcp->vc_sopt.sv_maxtransact, socksize);

    return (maxmsgsize);
}

/*
 * Figure out the largest write we can do to the server.
 */
static uint32_t
smb2_vc_maxwrite(struct smb_vc *vcp)
{
	uint32_t socksize = vcp->vc_sopt.sv_maxwrite;
	uint32_t maxmsgsize = vcp->vc_sopt.sv_maxwrite;
	uint32_t hdrsize = SMB2_HDRLEN;
	
	hdrsize += SMB2_WRITE_REQ_HDRLEN;    /* SMB2 Write Request Header */
	
	/* Make sure we never use a size bigger than the socket can support */
	SMB_TRAN_GETPARAM(vcp, SMBTP_SNDSZ, &socksize);
	maxmsgsize = MIN(maxmsgsize, socksize);
	maxmsgsize -= hdrsize;
    
    /* Now make it page aligned */
    if (maxmsgsize > PAGE_SIZE)
        maxmsgsize = (maxmsgsize / PAGE_SIZE) * PAGE_SIZE;
    
	SMB_LOG_IO("%s maxwrite = %u sock = %u, sv_maxwrite:= %u\n", vcp->vc_srvname,
			   maxmsgsize, socksize, vcp->vc_sopt.sv_maxwrite);
    
	if (maxmsgsize > smb_maxwrite) {
		return smb_maxwrite;
	}
	return maxmsgsize;
}
