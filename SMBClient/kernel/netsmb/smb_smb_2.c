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

#include <sys/msfscc.h>
#include <sys/smb_apple.h>
#include <libkern/OSAtomic.h>
#include <sys/random.h>

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
#include <smbclient/ntstatus.h>
#include <smbclient/smbclient_internal.h>
#include <netsmb/smb_sleephandler.h>
#include <netsmb/smb_read_write.h>
#include <netsmb/smb_dev_2.h>

#include <sys/sysctl.h>


extern lck_mtx_t global_Lease_hash_lock;
extern lck_grp_t *smb_rw_group;

extern uint32_t smb_maxsegreadsize;
extern uint32_t smb_maxsegwritesize;

static uint32_t smb_maxwrite = kDefaultMaxIOSize;	/* Default max write size */
static uint32_t smb_maxread = kDefaultMaxIOSize;	/* Default max read size */

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

void smb2_smb_adjust_quantum_sizes(struct smb_share *share, int32_t doingRead,
                                   uint32_t inQuantumSize, uint32_t inQuantumNbr,
                                   struct timeval totalTime, uint64_t totalDataLen);

static int
smb2_smb_parse_create_contexts(struct smb_share *share, struct mdchain *mdp,
                               uint32_t *ret_context_offset,
                               struct smb2_create_rq *createp);
static int
smb2_smb_parse_create_str(struct smb_share *share, struct mdchain *mdp,
                          uint32_t is_path, char **ret_str, size_t* ret_str_allocsize);
static int
smb2_smb_parse_negotiate(struct smb_session *sessionp, struct smb_rq *rqp, int smb1_req);

static int
smb2_smb_parse_negotiate_contexts(struct smbiod *iod,
                                  struct mdchain *mdp,
                                  uint32_t neg_bytes_parsed,
                                  uint32_t neg_context_offset,
                                  uint16_t neg_context_count);

static int
smb2_smb_read_uio(struct smb_share *share, SMBFID fid, uio_t uio,
                  uint32_t allow_compression, vfs_context_t context);

static int
smb2_smb_read_write_async(struct smb_share *share,
                          struct smb2_rw_rq *in_read_writep,
                          user_ssize_t *len, user_ssize_t *rresid,
                          uint32_t do_read, uint32_t *allow_compressionp,
                          vfs_context_t context);

static int
smb2_smb_read_write_fill(struct smb_share *share,
                         struct smb2_rw_rq *master_read_writep,
                         struct smb2_rw_rq *read_writep, struct smb_rq **rqp,
                         uint32_t do_read, uint32_t quantum_size,
                         uint32_t *allow_compressionp,
                         vfs_context_t context);

static uint32_t
smb2_session_maxtransact(struct smb_session *sessionp);


/* get any share of a given session
   This function takes a ref count on the share */
int
smb_get_share(struct smb_session *sessionp, struct smb_share **sharepp) {
    struct smb_share *share, *tshare;

    if (!sharepp) {
        SMBDEBUG("NULL sharepp.\n");
        return(ENOENT);
    }

    *sharepp = NULL;

    if (!sessionp) {
        SMBDEBUG("NULL session.\n");
        return(ENOENT);
    }

    smb_session_lock(sessionp);
    
    SMBCO_FOREACH_SAFE(share, SESSION_TO_CP(sessionp), tshare) {
        lck_mtx_lock(&share->ss_stlock);

        if (!(share->ss_flags & SMBO_GONE)) {
            smb_share_ref(share);
            lck_mtx_unlock(&(share)->ss_stlock);
            *sharepp = share;
            smb_session_unlock(sessionp);
            return(0);
        }

        lck_mtx_unlock(&(share)->ss_stlock);
    }

    SMBDEBUG("No valid share.\n");
    smb_session_unlock(sessionp);
    return(ENOENT);
}

/*
 * smb_get_utf8_char_count()
 *
 * Returns the number of chars in an UTF8 string that could have multibytes
 * per char. Not super fast, but should be very safe/accurate.
 *
 * Bytes 0x01 - 0x7F are ASCII chars
 * Bytes 0xC0 - 0xFF are part of the multi bytes chars
 */
static int smb_get_utf8_char_count(char s[], size_t maxlen)
{
    int i = 0, count = 0;
    
    while (s[i]) {
        if ((s[i] & 0xC0) != 0x80) {
            /* Count only non multibyte chars */
            count++;
        }
        i++;
        
        if (i > (int) maxlen) {
            break;
        }
    }

    return(count);
}

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
                                kAAPL_UNIX_BASED |
								kAAPL_SUPPORTS_NFS_ACE |
                                kAAPL_SUPPORTS_READ_DIR_ATTR_V2 |
                                kAAPL_SUPPORTS_HIFI;
    struct smb2_create_ctx_resolve_id *resolve_idp = NULL;
    struct smb2_dur_hndl_and_lease *dur_hndl_leasep = NULL;
    SMB2FID smb2_fid;
    uint32_t lease_state;
	struct smb_session *sessionp = SS_TO_SESSION(share);
	uint32_t dur_handle_timeout = 0;
    struct timespec
    snapshot_time = {0};
    uint64_t tm = 0;

    if (!(createp->flags & (SMB2_CREATE_GET_MAX_ACCESS |
                            SMB2_CREATE_AAPL_QUERY |
							SMB2_CREATE_AAPL_RESOLVE_ID |
                            SMB2_CREATE_DUR_HANDLE |
                            SMB2_CREATE_DUR_HANDLE_RECONNECT |
                            SMB2_CREATE_DIR_LEASE |
                            SMB2_CREATE_FILE_LEASE |
                            SMB2_CREATE_ADD_TIME_WARP |
                            SMB2_CREATE_QUERY_DISK_ID))) {
        /* No contexts to add */
        context_len = 0;
        *context_len_ptr = htolel(context_len);
        context_offset = 0;
        *context_offset_ptr = htolel(context_offset);
    }
    else {
		/*
		 * All Create Contexts start with 24 bytes which is the 
		 * Next (4), NameOffset (2), NameLength (2), Reserved (2), 
		 * DataOffset (2), DataLength (4), ContextName (usually 4), 
		 * Pad bytes to align (usually 4)
		 *
		 * context_len = 24 + context data
		 */
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
            resolve_idp->ret_pathp_allocsize = 0;
            
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
        
        if (createp->flags & SMB2_CREATE_ADD_TIME_WARP) {
            /* Time Warp Token */
            if (next_context_ptr != NULL) {
                /* Set prev context next ptr */
                *next_context_ptr = htolel(prev_content_size);
            }

            context_len += 32;
            prev_content_size = 32;

            next_context_ptr = mb_reserve(mbp, sizeof(uint32_t));   /* Next */
            *next_context_ptr = htolel(0);  /* Assume we are last context */
            mb_put_uint16le(mbp, 16);       /* Name Offset */
            mb_put_uint16le(mbp, 4);        /* Name Length */
            mb_put_uint16le(mbp, 0);        /* Reserved */
            mb_put_uint16le(mbp, 24);       /* Data Offset */
            mb_put_uint32le(mbp, 8);        /* Data Length */
            /* Name is a string constant and thus its not byte swapped uint32 */
            mb_put_uint32be(mbp, SMB2_CREATE_TIMEWARP_TOKEN);
            mb_put_uint32le(mbp, 0);        /* Pad to 8 byte boundary */

            /* Convert snapshot time to SMB time */
            snapshot_time.tv_sec = SS_TO_SESSION(share)->snapshot_local_time;

            smb_time_local2NT(&snapshot_time, &tm,
                              (share->ss_fstype == SMB_FS_FAT));
            mb_put_uint64le(mbp, tm);       /* Time Warp timestamp */
        }

        if (createp->flags & SMB2_CREATE_QUERY_DISK_ID) {
            /* Get on disk IDs */
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
            mb_put_uint32be(mbp, SMB2_CREATE_QUERY_ON_DISK_ID);
            mb_put_uint32le(mbp, 0);        /* Pad to 8 byte boundary */
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

		/*
		 * It states in [MS-SMB] 2.2.13.2 that "There is no required ordering 
		 * when multiple Create Context structures are used. The server MUST 
		 * support receiving the contexts in any order."
		 */
		if (createp->flags & SMB2_CREATE_DUR_HANDLE) {
            /*
             * Add Durable Handle Request
             */
            if (next_context_ptr != NULL) {
                /* Set prev context next ptr */
                *next_context_ptr = htolel(prev_content_size);
            }
			
            dur_hndl_leasep = createp->create_contextp;
			if (dur_hndl_leasep == NULL) {
				SMBERROR("dur_hndl_leasep is NULL \n");
				error = EBADRPC;
				goto bad;
			}

            if (dur_hndl_leasep->dur_handlep == NULL) {
                SMBERROR("dur_handlep is NULL \n");
                error = EBADRPC;
                goto bad;
            }

            /*
			 * Since a lot of third party servers do not support
			 * Durable Handle V2, only use them with servers that pass the
			 * check for supporting them. Currently only Time Machine mounts
			 * will check for Durable Handle V2 support via the fsctl.
			 *
			 * Also allow the check for Durable Handle V2
			 */
			if ((sessionp->session_misc_flags & SMBV_HAS_DUR_HNDL_V2) ||
				(dur_hndl_leasep->dur_handlep->flags & SMB2_DURABLE_HANDLE_V2_CHECK)) {
				/*
				 * SMB 3.x SMB2_CREATE_DURABLE_HANDLE_REQUEST_V2
				 */
				if (sessionp->session_misc_flags & SMBV_MNT_TIME_MACHINE) {
					/* Durable Handle V2 Time out is in milliseconds */
					if (sessionp->session_dur_hndl_v2_desired_timeout != 0) {
						dur_handle_timeout = sessionp->session_dur_hndl_v2_desired_timeout * 1000;
					}
				}
				
				/* Save the requested dur handle timeout */
                smbnode_dur_handle_lock(dur_hndl_leasep->dur_handlep, smb2_smb_add_create_contexts);
                
                dur_hndl_leasep->dur_handlep->timeout = dur_handle_timeout;

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
				mb_put_uint32be(mbp, SMB2_CREATE_DURABLE_HANDLE_REQUEST_V2);
				mb_put_uint32le(mbp, 0);        /* Pad to 8 byte boundary */
				mb_put_uint32le(mbp, dur_handle_timeout);	/* Timeout */
				

				if (dur_hndl_leasep->dur_handlep->flags & SMB2_PERSISTENT_HANDLE_REQUEST) {
					/* Request persistent handle */
					mb_put_uint32le(mbp, SMB2_DHANDLE_FLAG_PERSISTENT);
				}
				else {
					/* Persistent handles not supported by this server */
					mb_put_uint32le(mbp, 0);
				}
				
				mb_put_uint64le(mbp, 0);        /* Reserved */
				/* Add the CreateGuid */
				mb_put_mem(mbp, (char *) dur_hndl_leasep->dur_handlep->create_guid,
						   16, MB_MSYSTEM);
				
				/* Remember that we requested Dur Handle V2 */
                dur_hndl_leasep->dur_handlep->flags |= SMB2_DURABLE_HANDLE_V2;
				
                smbnode_dur_handle_unlock(dur_hndl_leasep->dur_handlep);
			}
			else {
				/*
				 * SMB 2.x SMB2_CREATE_DURABLE_HANDLE_REQUEST
				 */
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
        }
				
		if (createp->flags & SMB2_CREATE_DUR_HANDLE_RECONNECT) {
			/*
			 * Durable Handle Reconnect
			 */
            dur_hndl_leasep = createp->create_contextp;
            if (dur_hndl_leasep == NULL) {
                SMBERROR("dur_hndl_leasep is NULL \n");
                error = EBADRPC;
                goto bad;
            }

            if (dur_hndl_leasep->dur_handlep == NULL) {
                SMBERROR("dur_handlep is NULL \n");
                error = EBADRPC;
                goto bad;
            }

			/* map fid to SMB 2 fid */
			error = smb_fid_get_kernel_fid(share, dur_hndl_leasep->dur_handlep->fid, 0, &smb2_fid);
			if (error) {
				goto bad;
			}
			
			if (next_context_ptr != NULL) {
				/* Set prev context next ptr */
				*next_context_ptr = htolel(prev_content_size);
			}
			
			/*
			 * Since a lot of third party servers do not support
			 * Durable Handle V2, only use them with servers that pass the
			 * check for supporting them. Currently only Time Machine mounts
			 * will check for Durable Handle V2 support via the fsctl.
			 */
			if (sessionp->session_misc_flags & SMBV_HAS_DUR_HNDL_V2) {
				/*
				 * SMB 3.x SMB2_CREATE_DURABLE_HANDLE_RECONNECT_V2
				 */
				context_len += 64;
				prev_content_size = 64;
				
				next_context_ptr = mb_reserve(mbp, sizeof(uint32_t));   /* Next */
				*next_context_ptr = htolel(0);  /* Assume we are last context */
				mb_put_uint16le(mbp, 16);       /* Name Offset */
				mb_put_uint16le(mbp, 4);        /* Name Length */
				mb_put_uint16le(mbp, 0);        /* Reserved */
				mb_put_uint16le(mbp, 24);       /* Data Offset */
				mb_put_uint32le(mbp, 36);       /* Data Length */
				/* Name is a string constant and thus its not byte swapped uint32 */
				mb_put_uint32be(mbp, SMB2_CREATE_DURABLE_HANDLE_RECONNECT_V2);
				mb_put_uint32le(mbp, 0);        /* Pad to 8 byte boundary */
				mb_put_uint64le(mbp, smb2_fid.fid_persistent); /* FID */
				mb_put_uint64le(mbp, smb2_fid.fid_volatile);   /* FID */
				/* Add the CreateGuid */
				mb_put_mem(mbp, (char *) dur_hndl_leasep->dur_handlep->create_guid,
						   16, MB_MSYSTEM);
				if (dur_hndl_leasep->dur_handlep->flags & SMB2_PERSISTENT_HANDLE_RECONNECT) {
					/* Request persistent handle reconnect */
					SMBERROR("Requesting reconnect with persistent handle \n");
					mb_put_uint32le(mbp, SMB2_DHANDLE_FLAG_PERSISTENT);
				}
				else {
					/* Persistent handles not supported by this server */
					mb_put_uint32le(mbp, 0);
				}
				
				/* 4 bytes of pad to end on 8 byte boundary */
				mb_put_uint32le(mbp, 0);
			}
			else {
				/*
				 * SMB 2.x SMB2_CREATE_DURABLE_HANDLE_RECONNECT
				 */
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
				mb_put_uint32be(mbp, SMB2_CREATE_DURABLE_HANDLE_RECONNECT);
				mb_put_uint32le(mbp, 0);        /* Pad to 8 byte boundary */
				mb_put_uint64le(mbp, smb2_fid.fid_persistent); /* FID */
				mb_put_uint64le(mbp, smb2_fid.fid_volatile);   /* FID */
			}
		}

		/*
		 * Requesting a Durable Handle requires that you also request a
		 * Lease. See if we need to add the Lease request.
		 */
		if ((createp->flags & SMB2_CREATE_DUR_HANDLE) ||
			(createp->flags & SMB2_CREATE_DUR_HANDLE_RECONNECT) ||
			(createp->flags & SMB2_CREATE_DIR_LEASE) ||
            (createp->flags & SMB2_CREATE_FILE_LEASE)) {
            dur_hndl_leasep = createp->create_contextp;
            if (dur_hndl_leasep == NULL) {
                SMBERROR("dur_hndl_leasep is NULL \n");
                error = EBADRPC;
                goto bad;
            }

            if (dur_hndl_leasep->leasep == NULL) {
                SMBERROR("leasep is NULL \n");
                error = EBADRPC;
                goto bad;
            }

			/* Lock the lease */
            smbnode_lease_lock(dur_hndl_leasep->leasep, smb2_smb_add_create_contexts);

            /*
             * Creating a Durable Handle and Reconnecting a Durable Handle
             * both require a lease context
             *
             * Or just a plain directory lease request
             */
            
            /* Lease State */
            if (createp->flags & SMB2_CREATE_DUR_HANDLE_RECONNECT) {
                /* Reconnect, have to request exact same lease */
                lease_state = dur_hndl_leasep->leasep->lease_state;
            }
            else {
                /* New lease */
                lease_state = dur_hndl_leasep->leasep->req_lease_state;
            }
            
            if (next_context_ptr != NULL) {
                /* Set prev context next ptr */
                *next_context_ptr = htolel(prev_content_size);
            }
            
            /*
             * To be safer, use a Lease V2 only if Durable Handle V2
             * is supported
             *
             * Also allow the check for Durable Handle V2
             *
             * Dir Leases are always V2
             */
            if ((sessionp->session_misc_flags & SMBV_HAS_DUR_HNDL_V2) ||
                ((dur_hndl_leasep->dur_handlep != NULL) && (dur_hndl_leasep->dur_handlep->flags & SMB2_DURABLE_HANDLE_V2_CHECK)) ||
                (createp->flags & SMB2_CREATE_DIR_LEASE)) {
                /*
                 * SMB 3.x has a new lease format
                 */
                context_len += 80;
                //prev_content_size = 80; /* Unused since last context */
                
                next_context_ptr = mb_reserve(mbp, sizeof(uint32_t));   /* Next */
                *next_context_ptr = htolel(0);  /* Assume we are last context */
                mb_put_uint16le(mbp, 16);       /* Name Offset */
                mb_put_uint16le(mbp, 4);        /* Name Length */
                mb_put_uint16le(mbp, 0);        /* Reserved */
                mb_put_uint16le(mbp, 24);       /* Data Offset */
                mb_put_uint32le(mbp, 52);       /* Data Length */
                /* Name is a string constant and thus its not byte swapped uint32 */
                mb_put_uint32be(mbp, SMB2_CREATE_REQUEST_LEASE_V2);
                mb_put_uint32le(mbp, 0);        /* Pad to 8 byte boundary */
                mb_put_uint64le(mbp, dur_hndl_leasep->leasep->lease_key_hi);  /* Lease Key High */
                mb_put_uint64le(mbp, dur_hndl_leasep->leasep->lease_key_low); /* Lease Key Low */
                mb_put_uint32le(mbp, lease_state);  /* Lease State */
                
                if ((dur_hndl_leasep->leasep->par_lease_key_hi != 0) &&
                    (dur_hndl_leasep->leasep->par_lease_key_low != 0)) {
                    /* Parent Lease Key passed in */
                    mb_put_uint32le(mbp, SMB2_LEASE_FLAG_PARENT_LEASE_KEY_SET);
                    mb_put_uint64le(mbp, 0);        /* Lease Duration */
                    /* Add the ParentLeaseKey */
                    mb_put_uint64le(mbp, dur_hndl_leasep->leasep->par_lease_key_hi);
                    mb_put_uint64le(mbp, dur_hndl_leasep->leasep->par_lease_key_low);
                }
                else {
                    /* ParentLeaseKey not being used */
                    mb_put_uint32le(mbp, 0);	/* Flags */
                    mb_put_uint64le(mbp, 0);    /* Lease Duration */
                    mb_put_uint64le(mbp, 0);	/* ParentLeaseKey Upper */
                    mb_put_uint64le(mbp, 0);	/* ParentLeaseKey Lower */
                }
                
                /* [MS-SMB2] 3.2.4.3.8 Epoch should be set to 0 */
                mb_put_uint16le(mbp, 0);        /* Epoch */
                mb_put_uint16le(mbp, 0);        /* Reserved */
                
                /* 4 bytes of pad to end on 8 byte boundary */
                mb_put_uint32le(mbp, 0);
                
                /* Remember that we requested Lease V2 */
                dur_hndl_leasep->leasep->flags |= SMB2_LEASE_V2;
            }
            else {
                /*
                 * SMB 2.x lease format
                 */
                context_len += 56;
                //prev_content_size = 56; /* Unused since last context */
                
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
                mb_put_uint64le(mbp, dur_hndl_leasep->leasep->lease_key_hi);  /* Lease Key High */
                mb_put_uint64le(mbp, dur_hndl_leasep->leasep->lease_key_low); /* Lease Key Low */
                mb_put_uint32le(mbp, lease_state);  /* Lease State */
                mb_put_uint32le(mbp, 0);        /* Lease Flags */
                mb_put_uint64le(mbp, 0);        /* Lease Duration */
            }
			
			/* Unlock the lease */
            smbnode_lease_unlock(dur_hndl_leasep->leasep);
		}

		/* Set the Context Length */
        *context_len_ptr = htolel(context_len);
    }

bad:
    return (error);
}

static int
smb2_smb_add_negotiate_contexts(struct smb_session *sessionp,
                                struct smbiod *iod,
                                struct mbchain *mbp,
                                uint16_t *neg_context_countp,
                                int inReconnect)
{
    int error = 0;
    uint32_t context_len;
    int pad_bytes = 0;
    uint32_t *data_len_ptr;
    size_t name_len = 0;
    uint16_t neg_context_cnt = 0;
    uint32_t encrypt_algorithm_cnt = 0;
    uint32_t sign_algorithm_cnt = 0;
    uint32_t compression_algorithm_cnt = 0;

    /*
     * All Negotiate Contexts start with 8 bytes which is the
     * ContextType (2), DataLength (2), Reserved (4), then Data (variable len)
     * Pad bytes to align (usually 4)
     *
     * context_len = 8 + context data
     */

    /*
     * Add SMB2_PREAUTH_INTEGRITY_CAPABILITIES context first
     * Generate a new Salt variable for each Negotiate
     */
    read_random((void *)&iod->iod_pre_auth_int_salt,
                sizeof(iod->iod_pre_auth_int_salt));

    context_len = 8;        /* Neg Context Header Len */
    context_len += 38;      /* This context data len is always 38 */

    mb_put_uint16le(mbp, SMB2_PREAUTH_INTEGRITY_CAPABILITIES);  /* ContextType */
    mb_put_uint16le(mbp, 38);                                   /* DataLength */
    mb_put_uint32le(mbp, 0);                                    /* Reserved */
    mb_put_uint16le(mbp, 1);                                    /* HashAlgorithmCount */
    mb_put_uint16le(mbp, 32);                                   /* SaltLength */
    mb_put_uint16le(mbp, SMB2_PREAUTH_INTEGRITY_SHA512);        /* HashAlgorithms */
    mb_put_mem(mbp, (char *) iod->iod_pre_auth_int_salt,
               32, MB_MSYSTEM);                                 /* Salt */

    neg_context_cnt += 1;

    /* Add pad bytes if needed */
    if ((context_len % 8) != 0) {
        /* Contexts MUST start on next 8 byte boundary! */
        pad_bytes = 8 - (context_len % 8);
        mb_put_mem(mbp, NULL, pad_bytes, MB_MZERO);
    }

    /*
     * Add SMB2_ENCRYPTION_CAPABILITIES context next
     * Put the cipher in order of most to least preferred
     */
    if (inReconnect) {
        /*
         * Doing reconnect or alternate channel
         * Only put in cipher we are using
         */
        context_len = 8;        /* Neg Context Header Len */
        context_len += 4;       /* This context data len is currently 4 */

        mb_put_uint16le(mbp, SMB2_ENCRYPTION_CAPABILITIES); /* ContextType */
        mb_put_uint16le(mbp, 4);                            /* DataLength */
        mb_put_uint32le(mbp, 0);                            /* Reserved */
        mb_put_uint16le(mbp, 1);                            /* CipherCount */
        mb_put_uint16le(mbp, sessionp->session_smb3_encrypt_ciper);  /* SMB 3.1.1 Cipher */
    }
    else {
        context_len = 8;        /* Neg Context Header Len */

        if (sessionp->session_misc_flags & SMBV_ENABLE_AES_128_CCM) {
            encrypt_algorithm_cnt += 1;
        }
        if (sessionp->session_misc_flags & SMBV_ENABLE_AES_128_GCM) {
            encrypt_algorithm_cnt += 1;
        }
        if (sessionp->session_misc_flags & SMBV_ENABLE_AES_256_CCM) {
            encrypt_algorithm_cnt += 1;
        }
        if (sessionp->session_misc_flags & SMBV_ENABLE_AES_256_GCM) {
            encrypt_algorithm_cnt += 1;
        }

        if (encrypt_algorithm_cnt == 0) {
            /* Paranoid check */
            SMBERROR("No encryption algorithm set? Assuming AES-128-CCM \n");
            sessionp->session_misc_flags |= SMBV_ENABLE_AES_128_CCM;
            encrypt_algorithm_cnt = 1;
        }

        context_len += 2 + (2 * encrypt_algorithm_cnt);
        
        mb_put_uint16le(mbp, SMB2_ENCRYPTION_CAPABILITIES); /* ContextType */
        mb_put_uint16le(mbp, (2 + (2 * encrypt_algorithm_cnt))); /* DataLength */
        mb_put_uint32le(mbp, 0);                            /* Reserved */
        mb_put_uint16le(mbp, encrypt_algorithm_cnt);        /* CipherCount */
        /* [MS-SMB2] 2.2.3.1.2 Most prefered cipher at the beginning */
        if (sessionp->session_misc_flags & SMBV_ENABLE_AES_256_GCM) {
            /* SMB 3.1.1 Cipher */
            mb_put_uint16le(mbp, SMB2_ENCRYPTION_AES_256_GCM);
        }
        if (sessionp->session_misc_flags & SMBV_ENABLE_AES_256_CCM) {
            /* SMB 3.1.1 Cipher */
            mb_put_uint16le(mbp, SMB2_ENCRYPTION_AES_256_CCM);
        }
        if (sessionp->session_misc_flags & SMBV_ENABLE_AES_128_GCM) {
            /* SMB 3.1.1 Cipher */
            mb_put_uint16le(mbp, SMB2_ENCRYPTION_AES_128_GCM);
        }
        if (sessionp->session_misc_flags & SMBV_ENABLE_AES_128_CCM) {
            /* SMB 3.0.2 Cipher */
            mb_put_uint16le(mbp, SMB2_ENCRYPTION_AES_128_CCM);
        }
    }

    neg_context_cnt += 1;

    /* Add pad bytes if needed */
    if ((context_len % 8) != 0) {
        /* Contexts MUST start on next 8 byte boundary! */
        pad_bytes = 8 - (context_len % 8);
        mb_put_mem(mbp, NULL, pad_bytes, MB_MZERO);
    }

    /*
     * Add SMB2_COMPRESSION_CAPABILITIES context next
     * For reconnect, leave in all algorithms since server picks algorithm
     * to use, but is always allowed to select SMB2_COMPRESSION_NONE
     *
     */
    context_len = 8;        /* Neg Context Header Len */
    
    if (sessionp->client_compression_algorithms_map & SMB2_COMPRESSION_LZNT1_ENABLED) {
        compression_algorithm_cnt += 1;
    }
    if (sessionp->client_compression_algorithms_map & SMB2_COMPRESSION_LZ77_ENABLED) {
        compression_algorithm_cnt += 1;
    }
    if (sessionp->client_compression_algorithms_map & SMB2_COMPRESSION_LZ77_HUFFMAN_ENABLED) {
        compression_algorithm_cnt += 1;
    }
    if (sessionp->client_compression_algorithms_map & SMB2_COMPRESSION_PATTERN_V1_ENABLED) {
        /* Pattern algoritm is only valid if chained compression is supported */
        if (sessionp->session_misc_flags & SMBV_COMPRESSION_CHAINING_OFF) {
            /* Disable Pattern algorithm since chained compression is disabled */
            sessionp->client_compression_algorithms_map &= ~SMB2_COMPRESSION_PATTERN_V1_ENABLED;
        }
        else {
            compression_algorithm_cnt += 1;
        }
    }

    if (compression_algorithm_cnt == 0) {
        /* No compression algorithm enabled */
        context_len += 10;      /* This context data len is currently 10 */

        SMBERROR("No compression algorithm enabled? \n");
        mb_put_uint16le(mbp, SMB2_COMPRESSION_CAPABILITIES);    /* ContextType */
        mb_put_uint16le(mbp, 10);                               /* DataLength */
        mb_put_uint32le(mbp, 0);                                /* Reserved */
        mb_put_uint16le(mbp, 1);                                /* CompressionAlgorithmCount */
        mb_put_uint16le(mbp, 0);                                /* Padding */
        mb_put_uint32le(mbp, SMB2_COMPRESSION_CAPABILITIES_FLAG_NONE); /* Flags */
        mb_put_uint16le(mbp, SMB2_COMPRESSION_NONE);            /* CompressionAlgorithms */
    }
    else {
        /* At least one compression algorithm is enabled */
        context_len += 8 + (2 * compression_algorithm_cnt);
        
        mb_put_uint16le(mbp, SMB2_COMPRESSION_CAPABILITIES);    /* ContextType */
        mb_put_uint16le(mbp, (8 + (2 * compression_algorithm_cnt))); /* DataLength */
        mb_put_uint32le(mbp, 0);                                /* Reserved */
        mb_put_uint16le(mbp, compression_algorithm_cnt);        /* CompressionAlgorithmCount */
        mb_put_uint16le(mbp, 0);                                /* Padding */
        
        if (sessionp->session_misc_flags & SMBV_COMPRESSION_CHAINING_OFF) {
            mb_put_uint32le(mbp, SMB2_COMPRESSION_CAPABILITIES_FLAG_NONE);      /* Flags */
        }
        else {
            mb_put_uint32le(mbp, SMB2_COMPRESSION_CAPABILITIES_FLAG_CHAINED);   /* Flags */
        }
        
        /* [MS-SMB2] 2.2.3.1.3 Most prefered compression algorithms at the beginning */
        if (sessionp->client_compression_algorithms_map & SMB2_COMPRESSION_LZ77_HUFFMAN_ENABLED) {
            mb_put_uint16le(mbp, SMB2_COMPRESSION_LZ77_HUFFMAN);
        }
        if (sessionp->client_compression_algorithms_map & SMB2_COMPRESSION_LZ77_ENABLED) {
            mb_put_uint16le(mbp, SMB2_COMPRESSION_LZ77);
        }
        if (sessionp->client_compression_algorithms_map & SMB2_COMPRESSION_LZNT1_ENABLED) {
            mb_put_uint16le(mbp, SMB2_COMPRESSION_LZNT1);
        }
        if (sessionp->client_compression_algorithms_map & SMB2_COMPRESSION_PATTERN_V1_ENABLED) {
            mb_put_uint16le(mbp, SMB2_COMPRESSION_PATTERN_V1);
        }
    }

    neg_context_cnt += 1;

    /* Add pad bytes if needed */
    if ((context_len % 8) != 0) {
        /* Contexts MUST start on next 8 byte boundary! */
        pad_bytes = 8 - (context_len % 8);
        mb_put_mem(mbp, NULL, pad_bytes, MB_MZERO);
    }

    /*
     * Add SMB2_SIGNING_CAPABILITIES context next
     */
    if (inReconnect) {
        /*
         * Doing reconnect or alternate channel
         * Only put in algorithm we are using
         */
        context_len = 8;        /* Neg Context Header Len */
        context_len += 4;       /* This context data len is currently 4 */

        mb_put_uint16le(mbp, SMB2_SIGNING_CAPABILITIES);    /* ContextType */
        mb_put_uint16le(mbp, 4);                            /* DataLength */
        mb_put_uint32le(mbp, 0);                            /* Reserved */
        mb_put_uint16le(mbp, 1);                            /* SigningAlgorithmCount */
        mb_put_uint16le(mbp, sessionp->session_smb3_signing_algorithm);  /* SigningAlgorithm */
    }
    else {
        if (sessionp->session_misc_flags & SMBV_ENABLE_AES_128_CMAC) {
            sign_algorithm_cnt++;
        }
        if (sessionp->session_misc_flags & SMBV_ENABLE_AES_128_GMAC) {
            sign_algorithm_cnt++;
        }
        if (sign_algorithm_cnt == 0) {
            /* Paranoid check */
            SMBERROR("No signing algorithm set? Assuming AES-128-CMAC \n");
            sessionp->session_misc_flags |= SMBV_ENABLE_AES_128_CMAC;
            sign_algorithm_cnt = 1;
        }
    
        context_len = 8;
        context_len += 2 + (2 * sign_algorithm_cnt);
        
        mb_put_uint16le(mbp, SMB2_SIGNING_CAPABILITIES);    /* ContextType */
        mb_put_uint16le(mbp, 2 + (2 * sign_algorithm_cnt));    /* DataLength */
        mb_put_uint32le(mbp, 0);                            /* Reserved */
        mb_put_uint16le(mbp, sign_algorithm_cnt);           /* SigningAlgorithmCount */

        if (sessionp->session_misc_flags & SMBV_ENABLE_AES_128_GMAC) {
            mb_put_uint16le(mbp, SMB2_SIGNING_AES_GMAC);    /* SigningAlgorithm */
        }

        if (sessionp->session_misc_flags & SMBV_ENABLE_AES_128_CMAC) {
            mb_put_uint16le(mbp, SMB2_SIGNING_AES_CMAC);    /* SigningAlgorithm */
        }

    }
    neg_context_cnt += 1;
    
    /* Add pad bytes if needed */
    if ((context_len % 8) != 0) {
        /* Contexts MUST start on next 8 byte boundary! */
        pad_bytes = 8 - (context_len % 8);
        mb_put_mem(mbp, NULL, pad_bytes, MB_MZERO);
    }
    
    /*
     * Add SMB2_NETNAME_NEGOTIATE_CONTEXT_ID context next
     */
    //context_len = 8;        /* Neg Context Header Len */

    mb_put_uint16le(mbp, SMB2_NETNAME_NEGOTIATE_CONTEXT_ID);    /* ContextType */
    data_len_ptr = mb_reserve(mbp, sizeof(uint16_t));           /* Reserve space for DataLength */
    mb_put_uint32le(mbp, 0);                                    /* Reserved */
    error = smb_put_dmem(mbp,
                         sessionp->session_srvname,
                         strlen(sessionp->session_srvname),
                         NO_SFM_CONVERSIONS, TRUE, &name_len);
    if (error) {
        return error;
    }

    *data_len_ptr = htoles(name_len);        /* Set DataLength */

    //context_len += (uint32_t) name_len;

    neg_context_cnt += 1;

    *neg_context_countp = htoles(neg_context_cnt); /* Set NegContextCount */

    return (error);
}

void smb2_smb_adjust_quantum_sizes(struct smb_share *share, int32_t doingRead,
                                   uint32_t inQuantumSize, uint32_t inQuantumNbr,
                                   struct timeval totalTime, uint64_t totalDataLen)
{
    struct smb_session *sessionp = NULL;
    uint64_t totalElapsedTimeMicroSecs = 0; /* time to send totalDataLen amount of data */
    uint64_t bytesPerSec = 0;               /* curr bytes per second */
    uint32_t currQuantumSize = 0;
    uint32_t newQuantumNumber = 0, newQuantumSize = 0;
    uint32_t changedQuantumSize = 0;
    uint32_t estimated_credits = 0;
    int32_t check_credits = 0;
    int32_t throttle = 0;
    uint64_t *bytes_secp = NULL;
    uint32_t *quantum_sizep = NULL;
    uint32_t *quantum_countp = NULL, which_checked = 0;
    uint32_t *total_timep = NULL;
    uint64_t *total_bytesp = NULL;

    if (share == NULL) {
        SMBERROR("share is null? \n");
        return;
    }
    
    sessionp = SS_TO_SESSION(share);
    if (sessionp == NULL) {
        SMBERROR("sessionp is null? \n");
        return;
    }

    struct smbiod *iod = NULL;
    if (smb_iod_get_main_iod(sessionp, &iod, __FUNCTION__)) { // TBD: Do we need a for loop on all iods?
        SMBERROR("Invalid iod\n");
        return;
    }

    /* Paranoid checks */
    if ((totalTime.tv_sec == 0) && (totalTime.tv_usec == 0)) {
        SMBERROR("totalTime is 0? \n");
        smb_iod_rel(iod, NULL, __FUNCTION__);
        return;
    }
    
    if (totalDataLen == 0) {
        SMBERROR("totalDataLen is 0? \n");
        smb_iod_rel(iod, NULL, __FUNCTION__);
        return;
    }

    /* calculate bytes per second */
    totalElapsedTimeMicroSecs = totalTime.tv_sec * 1000000;
    totalElapsedTimeMicroSecs += totalTime.tv_usec;
    
    if (totalElapsedTimeMicroSecs == 0) {
        /* Paranoid check */
        SMBERROR("totalElapsedTimeMicroSecs is 0? \n");
        smb_iod_rel(iod, NULL, __FUNCTION__);
        return;
    }

    bytesPerSec = (totalDataLen * 1000000) / totalElapsedTimeMicroSecs;

    SMB_LOG_KTRACE(SMB_DBG_ADJUST_QUANTUM_SIZES | DBG_FUNC_START, doingRead, bytesPerSec, inQuantumNbr, inQuantumSize, 0);

    if (bytesPerSec == 0) {
        /* Paranoid check */
        SMBERROR("bytesPerSec is 0? \n");
        goto exit;
    }

    lck_mtx_lock(&sessionp->iod_quantum_lock);

    if (doingRead) {
        bytes_secp = sessionp->iod_readBytePerSec;
        quantum_sizep = sessionp->iod_readSizes;
        quantum_countp = sessionp->iod_readCounts;
        currQuantumSize = sessionp->iod_readQuantumSize;
        
        total_timep = sessionp->iod_readTotalTime;
        total_bytesp = sessionp->iod_readTotalBytes;
    }
    else {
        bytes_secp = sessionp->iod_writeBytePerSec;
        quantum_sizep = sessionp->iod_writeSizes;
        quantum_countp = sessionp->iod_writeCounts;
        currQuantumSize = sessionp->iod_writeQuantumSize;

        total_timep = sessionp->iod_writeTotalTime;
        total_bytesp = sessionp->iod_writeTotalBytes;
    }

    if (inQuantumSize == quantum_sizep[2]) {
        /* Keep track of cumulative time and bytes */
        total_timep[2] += totalElapsedTimeMicroSecs;
        total_bytesp[2] += totalDataLen;
        bytesPerSec = (total_bytesp[2] * 1000000) / total_timep[2];
        
        if (total_timep[2] >= sessionp->rw_max_check_time) {
            /* Save cumulative bytes/sec */
            bytes_secp[2] = bytesPerSec;
        }
        
        which_checked = 3;
        SMB_LOG_IO("Max size: total bytes %llu, total microSecs %u, currBytes/Sec %llu \n",
                   total_bytesp[2], total_timep[2], bytesPerSec);
    }
    else {
        if (inQuantumSize == quantum_sizep[1]) {
            total_timep[1] += totalElapsedTimeMicroSecs;
            total_bytesp[1] += totalDataLen;
            bytesPerSec = (total_bytesp[1] * 1000000) / total_timep[1];

            if (total_timep[1] >= sessionp->rw_max_check_time) {
                /* Save cumulative bytes/sec */
                bytes_secp[1] = bytesPerSec;
            }

            which_checked = 2;
            SMB_LOG_IO("Med size: total bytes %llu, total microSecs %u, currBytes/Sec %llu \n",
                       total_bytesp[1], total_timep[1], bytesPerSec);
        }
        else {
            if (inQuantumSize == quantum_sizep[0]) {
                total_timep[0] += totalElapsedTimeMicroSecs;
                total_bytesp[0] += totalDataLen;
                bytesPerSec = (total_bytesp[0] * 1000000) / total_timep[0];

                if (total_timep[0] >= sessionp->rw_max_check_time) {
                    /* Save cumulative bytes/sec */
                    bytes_secp[0] = bytesPerSec;
                }

                which_checked = 2;
                SMB_LOG_IO("Min size: total bytes %llu, total microSecs %u, currBytes/Sec %llu \n",
                           total_bytesp[0], total_timep[0], bytesPerSec);
            }
            else {
                SMBERROR("No match for quantumSize %u \n", inQuantumSize);
                lck_mtx_unlock(&sessionp->iod_quantum_lock);
                goto exit;
            }
        }
    }

    /*
     * If we have all three bytes/sec, then check to see which size
     * (min, med, max) had the fastest cumulative bytes/sec and use that size
     */
    if ((bytes_secp[0] != 0) &&
        (bytes_secp[1] != 0) &&
        (bytes_secp[2] != 0)) {
        if ((bytes_secp[2] > bytes_secp[0]) &&
            (bytes_secp[2] > bytes_secp[1])) {
            /* Largest is the fastest */
            newQuantumSize = quantum_sizep[2];
            newQuantumNumber = quantum_countp[2];
        }
        else {
            if ((bytes_secp[0] > bytes_secp[2]) &&
                (bytes_secp[0] > bytes_secp[1])) {
                /* Smallest is the fastest */
                newQuantumSize = quantum_sizep[0];
                newQuantumNumber = quantum_countp[0];
            }
            else {
                /* Assume medium is the fastest */
                newQuantumSize = quantum_sizep[1];
                newQuantumNumber = quantum_countp[1];
            }
        }

        /* Are we changing quantum size? */
        if (newQuantumSize != currQuantumSize) {
            changedQuantumSize = 1;
            if (newQuantumSize > currQuantumSize) {
                /* If we are increasing, then need to check credits */
                check_credits = 1;
            }
        }
    }

    lck_mtx_unlock(&sessionp->iod_quantum_lock);

    /*
     * If possibly increasing number of credits that we need, then check to
     * make sure we do not exceed our credit limits
     */
    if (check_credits == 1) {
        estimated_credits = (newQuantumSize * newQuantumNumber) / (64 * 1024);
        
        if (estimated_credits > iod->iod_credits_max) {
            /* Should never happen */
            SMBERROR("est credits %d > max credits %d??? \n",
                      estimated_credits, iod->iod_credits_max);

            throttle = 1;
        }
        else {
            if ((iod->iod_credits_max - estimated_credits) < kCREDIT_QUANTUM_RESERVE) {
                SMBERROR("remaining credits %d < low Quantum credits %d \n",
                           (iod->iod_credits_max - estimated_credits),
                           kCREDIT_QUANTUM_RESERVE);
                throttle = 1;
            }
        }
        
        if (throttle) {
            /*
             * Increasing the quantum size does not increase the credit
             * load by that much. We must be at max number of credits
             * granted so dont increase quantum size.
             */
            SMBERROR("Skip quantum size increase due to throttle \n");
            goto exit;
        }
    }

    if (changedQuantumSize == 1) {
        lck_mtx_lock(&sessionp->iod_quantum_lock);

        if (doingRead == 1) {
            SMB_LOG_IO("AdjustQuantumSizes: new read quantum count %d size %d\n",
                       newQuantumNumber, newQuantumSize);
            
            SMB_LOG_KTRACE(SMB_DBG_ADJUST_QUANTUM_SIZES | DBG_FUNC_NONE, 0xabc001, newQuantumNumber, newQuantumSize, 0, 0);
            
            sessionp->iod_readQuantumSize = newQuantumSize;
            sessionp->iod_readQuantumNumber = newQuantumNumber;
        }
        else {
            SMB_LOG_IO("AdjustQuantumSizes: new write quantum count %d size %d\n",
                       newQuantumNumber, newQuantumSize);
            
            SMB_LOG_KTRACE(SMB_DBG_ADJUST_QUANTUM_SIZES | DBG_FUNC_NONE, 0xabc002, newQuantumNumber, newQuantumSize, 0, 0);

            sessionp->iod_writeQuantumSize = newQuantumSize;
            sessionp->iod_writeQuantumNumber = newQuantumNumber;
        }

        lck_mtx_unlock(&sessionp->iod_quantum_lock);
    }
exit:
    SMB_LOG_KTRACE(SMB_DBG_ADJUST_QUANTUM_SIZES | DBG_FUNC_END, 0, which_checked, 0, 0, 0);

    smb_iod_rel(iod, NULL, __FUNCTION__);
    return;
}

static void smb2_smb_get_quantum_sizes(struct smb_session *sessionp, user_ssize_t len, int32_t doingRead,
                                       uint32_t *retQuantumSize, uint32_t *retQuantumNbr,
                                       int *recheck)
{
    struct timeval current_time = {0}, elapsed_time = {0};
    uint32_t *quantum_sizep = NULL;
    uint32_t *quantum_countp = NULL;
    uint32_t *total_timep = NULL;

    /* Paranoid checks */
    if ((sessionp == NULL) ||
        (retQuantumSize == NULL) ||
        (retQuantumNbr == NULL) ||
        (recheck == NULL)) {
        SMBERROR("Null pointer passed in! \n");
        return;
    }

    lck_mtx_lock(&sessionp->iod_quantum_lock);

    /*
     * For dev testing to force using the medium quantum size and number.
     * Default is to be off and use the byte/sec method.
     */
    if ((sessionp->rw_gb_threshold != 0) &&
        ((sessionp->active_channel_speed / 1000000000) > sessionp->rw_gb_threshold)) {
        /* Force using the med quantum size */
        if (doingRead) {
            *retQuantumSize = sessionp->iod_readSizes[1];
            *retQuantumNbr = sessionp->iod_readCounts[1];
        }
        else {
            *retQuantumSize = sessionp->iod_writeSizes[1];
            *retQuantumNbr = sessionp->iod_writeCounts[1];
        }

        SMB_LOG_IO("Force using quantum count %d size %d\n",
                   *retQuantumNbr, *retQuantumSize);

        goto exit;
    }

    /*
     * Is it time to recheck quantum sizes and do we have a large enoungh
     * IO request to be able to recheck?
     */
    microtime(&current_time);
    timersub (&current_time, &sessionp->iod_last_recheck_time, &elapsed_time);
    if (elapsed_time.tv_sec > kQuantumRecheckTimeOut) {
        /*
         * I did have code that would determine what the current size we
         * were using and only recheck the other two, and the in use size was
         * constantly updating its bytes/sec. I prefer being able to see all
         * three speeds rechecked and their results.  Simplier code too.
         */
        bzero(sessionp->iod_readBytePerSec, sizeof(sessionp->iod_readBytePerSec));
        bzero(sessionp->iod_readTotalTime, sizeof(sessionp->iod_readTotalTime));
        bzero(sessionp->iod_readTotalBytes, sizeof(sessionp->iod_readTotalBytes));

        bzero(sessionp->iod_writeBytePerSec, sizeof(sessionp->iod_writeBytePerSec));
        bzero(sessionp->iod_writeTotalTime, sizeof(sessionp->iod_writeTotalTime));
        bzero(sessionp->iod_writeTotalBytes, sizeof(sessionp->iod_writeTotalBytes));
    }

    if (doingRead) {
        quantum_sizep = sessionp->iod_readSizes;
        quantum_countp = sessionp->iod_readCounts;
        total_timep = sessionp->iod_readTotalTime;

        /* assume we stay with current settings */
        *retQuantumSize = sessionp->iod_readQuantumSize;
        *retQuantumNbr = sessionp->iod_readQuantumNumber;
        
        /* Ensure all comparisons are done with same length of IO */
        if (len < smb_maxsegreadsize) {
            goto exit;
        }
    }
    else {
        quantum_sizep = sessionp->iod_writeSizes;
        quantum_countp = sessionp->iod_writeCounts;
        total_timep = sessionp->iod_writeTotalTime;

        /* assume we stay with current settings */
        *retQuantumSize = sessionp->iod_writeQuantumSize;
        *retQuantumNbr = sessionp->iod_writeQuantumNumber;
        
        /* Ensure all comparisons are done with same length of IO */
        if (len < smb_maxsegwritesize) {
            goto exit;
        }
    }

    /*
     * Do multiple sequential checks of max, med, then min
     */
    if ((*recheck == 0) && (total_timep[2] < sessionp->rw_max_check_time)) {
        /*
         * Check max quantum size first. Since first reads tend to be
         * slow starting probably due to setup of read ahead, best to
         * start with largest IO. This attempts to minimizes the impact
         * of the slow read start to the bytes/sec calculations.
         */
        *retQuantumSize = quantum_sizep[2];
        *retQuantumNbr = quantum_countp[2];
        *recheck = 1;
    }

    if ((*recheck == 0) && (total_timep[1] < sessionp->rw_max_check_time)) {
        /* Check med quantum size */
        *retQuantumSize = quantum_sizep[1];
        *retQuantumNbr = quantum_countp[1];
        *recheck = 1;
    }

    if ((*recheck == 0) && (total_timep[0] < sessionp->rw_max_check_time)) {
        /* Check min quantum size */
        *retQuantumSize = quantum_sizep[0];
        *retQuantumNbr = quantum_countp[0];
        *recheck = 1;
    }

    if (*recheck == 1) {
        /* save time that we last checked speeds */
        microtime(&sessionp->iod_last_recheck_time);
    }

exit:
    /* Try to have at least one quantum per channel */
    if (sessionp->active_channel_count > kQuantumMedNumber) {
        *retQuantumNbr += sessionp->active_channel_count - kQuantumMedNumber;
    }

    lck_mtx_unlock(&sessionp->iod_quantum_lock);
}

/*
 * The calling routine must hold a reference on the share
 */
int
smb2_smb_change_notify(struct smb_share *share, struct smb2_change_notify_rq *changep,
                       struct smb_rq **in_rqp, vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	int error;
    SMB2FID smb2_fid;
    struct smbiod *iod = NULL;

    error = smb_iod_get_any_iod(SS_TO_SESSION(share), &iod, __FUNCTION__);
    if (error) {
        return error;
    }

    /* 
     * Allocate request and header for a Change Notify 
     * Win 7 Client sends this as a Sync request, so match their behavior
     */
    error = smb2_rq_alloc(SSTOCP(share), SMB2_CHANGE_NOTIFY, 
                          &changep->output_buffer_len, 
                          context, iod, &rqp);
    if (error) {
        smb_iod_rel(iod, NULL, __FUNCTION__);
        goto bad;
    }

    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

    /* Set up the async call back */
    rqp->sr_flags |= SMBR_ASYNC;
    rqp->sr_callback = changep->fn_callback;
    rqp->sr_callback_args = changep->fn_callback_args;

    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB 2/3 Change Notify Request
     */
    mb_put_uint16le(mbp, 32);                           /* Struct size */
    mb_put_uint16le(mbp, changep->flags);               /* Flags */
    mb_put_uint32le(mbp, changep->output_buffer_len);   /* Output buffer len */
    
    /* map fid to SMB 2/3 fid */
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
smb2_smb_close(struct smb_share *share, struct smb2_close_rq *closep,
               struct smb_rq **compound_rqp, struct smbiod *iod, vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
    uint16_t flags;
    SMB2FID smb2_fid;
    uint8_t cmd = SMB2_CLOSE;

    if (closep->flags & SMB2_CMD_NO_BLOCK) {
        /* Dont wait for credits, but return an error instead */
        closep->flags &= ~SMB2_CMD_NO_BLOCK;
        cmd |= SMB2_NO_BLOCK;
    }

resend:
    if (iod) {
        error = smb_iod_ref(iod, __FUNCTION__);
    } else {
        error = smb_iod_get_any_iod(SS_TO_SESSION(share), &iod, __FUNCTION__);
    }
    if (error) {
        return error;
    }

    /* Allocate request and header for a Close */
    error = smb2_rq_alloc(SSTOCP(share), cmd, NULL, context, iod, &rqp);
    if (error) {
        smb_iod_rel(iod, NULL, __FUNCTION__);
        return error;
    }
    
    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

    rqp->sr_extflags |= SMB2_NON_IDEMPOTENT;

	smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB 2/3 Close Request
     */
    mb_put_uint16le(mbp, 24);       /* Struct size */
    flags = closep->flags;          /* only take lower 16 bits for now */
    mb_put_uint16le(mbp, flags);    /* Flags */
    mb_put_uint32le(mbp, 0);        /* Reserved */

    /* map fid to SMB 2/3 fid */
    error = smb_fid_get_kernel_fid(share, closep->fid, 0, &smb2_fid);
    if (error) {
        goto bad;
    }
    mb_put_uint64le(mbp, smb2_fid.fid_persistent); /* FID */
    mb_put_uint64le(mbp, smb2_fid.fid_volatile);   /* FID */
    
    if (closep->mc_flags & SMB2_MC_REPLAY_FLAG) {
        /* This message is replayed - sent after a channel has been disconnected */
        *rqp->sr_flagsp |= SMB2_FLAGS_REPLAY_OPERATIONS;
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
    closep->ret_ntstatus = rqp->sr_ntstatus;
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            SMB_LOG_MC("resending messageid %llu cmd %u (sr_flags: 0x%x).\n", rqp->sr_messageid, rqp->sr_command, rqp->sr_flags);
            if (rqp->sr_flags & SMBR_ALT_CH_DISCON) {
                /* An alternate channel got disconnected. Resend with the REPLAY flag set */
                closep->mc_flags |= SMB2_MC_REPLAY_FLAG;
            }

            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            iod = NULL;
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
                   struct smbiod *iod,
                   vfs_context_t context)
{
	int error;
    struct smb2_close_rq *closep = NULL;
    
    SMB_MALLOC_TYPE(closep, struct smb2_close_rq, Z_WAITOK_ZERO);
    if (closep == NULL) {
        SMBERROR("SMB_MALLOC_TYPE failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    closep->share = share;
    closep->flags = 0;  /* dont want any returned attributes */
    closep->fid = fid;
    closep->mc_flags = 0;
    
    error = smb2_smb_close(share, closep, compound_rqp, iod, context);
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
        SMB_FREE_TYPE(struct smb2_close_rq, closep);
    }
    
	return error;
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smb2_smb_create(struct smb_share *share, struct smb2_create_rq *createp,
                struct smb_rq **compound_rqp, vfs_context_t context)
{
	int error;
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
    uint32_t *context_offset_ptr, *context_len_ptr;
	uint16_t *name_len, name_offset;
    uint8_t sep_char = '\\';
    uint8_t cmd = SMB2_CREATE;
    vnode_t par_vp = NULL;
    struct smbiod *iod = NULL;

    if (createp->flags & SMB2_CMD_NO_BLOCK) {
        /* Dont wait for credits, but return an error instead */
        createp->flags &= ~SMB2_CMD_NO_BLOCK;
        cmd |= SMB2_NO_BLOCK;
    }

    /* Snapshot mounts are always read only */
    if ((SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_SNAPSHOT) &&
        (strcmp(share->ss_name, "IPC$") != 0)) {
        createp->flags |= SMB2_CREATE_ADD_TIME_WARP;
    }

resend:
    error = smb_iod_get_any_iod(SS_TO_SESSION(share), &iod, __FUNCTION__);
    if (error) {
        return error;
    }

    /* Allocate request and header for a Create */
    error = smb2_rq_alloc(SSTOCP(share), cmd, NULL, context, iod, &rqp);
    if (error) {
        smb_iod_rel(iod, NULL, __FUNCTION__);
        return error;
    }
    
    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

    rqp->sr_extflags |= SMB2_NON_IDEMPOTENT;

	smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB 2/3 Create Request
     */
    mb_put_uint16le(mbp, 57);                       /* Struct size */
    mb_put_uint8(mbp, 0);                           /* Security flags */

    /* If we are using persistent handle, Oplock level is none */
    if ((createp->flags & (SMB2_CREATE_DUR_HANDLE | SMB2_CREATE_DUR_HANDLE_RECONNECT)) &&
        (SS_TO_SESSION(share)->session_sopt.sv_active_capabilities & SMB2_GLOBAL_CAP_PERSISTENT_HANDLES) &&
        (share->ss_share_caps & SMB2_SHARE_CAP_CONTINUOUS_AVAILABILITY)) {
        mb_put_uint8(mbp, SMB2_OPLOCK_LEVEL_NONE);  /* Oplock level */
    }
    else {
        mb_put_uint8(mbp, createp->oplock_level);   /* Oplock level */
    }

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

    /* <17602533> Assume delete access so Finder can attempt rename */
    if (createp->flags & SMB2_CREATE_GET_MAX_ACCESS) {
        /* Assume delete access and let server control it */
        createp->flags |= SMB2_CREATE_ASSUME_DELETE;
        
        if (createp->dnp != NULL) {
            if (createp->namep == NULL) {
                /* dnp is the item to be opened */
                par_vp = smbfs_smb_get_parent(createp->dnp, kShareLock);
                if (par_vp != NULL) {
                    if (!(VTOSMB(par_vp)->maxAccessRights & SMB2_FILE_DELETE_CHILD)) {
                        /*
                         * If parent DENIES delete child, then do not assume child
                         * has delete access.
                         */
                        createp->flags &= ~SMB2_CREATE_ASSUME_DELETE;
                    }
                    
                    vnode_put(par_vp);
                }
                else {
                    /* 
                     * Could not get the parent, but thats ok, we will just 
                     * assume that we have delete access.
                     */
                    
                    if (createp->dnp->n_parent_vid != 0) {
                        /* Parent got recycled already? */
                        SMBWARNING_LOCK(createp->dnp, "Missing parent for <%s> \n",
                                        createp->dnp->n_name);
                    }
                }
            }
            else {
                /* dnp must be the parent of item to be opened. parent + child */
                if (!(createp->dnp->maxAccessRights & SMB2_FILE_DELETE_CHILD)) {
                    /*
                     * If parent DENIES delete child, then do not assume child
                     * has delete access.
                     */
                    createp->flags &= ~SMB2_CREATE_ASSUME_DELETE;
                }
            }
        }
    }

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

			if (createp->flags & SMB2_CREATE_SET_DFS_FLAG) {
				/*
				 * This is a DFS operation so set the flag in the header
				 * If this flag is set, then the file name can be prefixed
				 * with DFS link information.
				 */
				*rqp->sr_flagsp |= htolel(SMB2_FLAGS_DFS_OPERATIONS);
			}
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
    
    if (createp->mc_flags & SMB2_MC_REPLAY_FLAG) {
        /* This message is replayed - sent after a channel has been disconnected */
        *rqp->sr_flagsp |= SMB2_FLAGS_REPLAY_OPERATIONS;
    }

    if (compound_rqp != NULL) {
        /*
         * building a compound request, add padding to 8 bytes and just
         * return this built request.
         * do not release the iod as of yet.
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
            SMB_LOG_MC("resending messageid %llu cmd %u (sr_flags: 0x%x).\n", rqp->sr_messageid, rqp->sr_command, rqp->sr_flags);
            if (rqp->sr_flags & SMBR_ALT_CH_DISCON) {
                /* An alternate channel got disconnected. Resend with the REPLAY flag set */
                createp->mc_flags |= SMB2_MC_REPLAY_FLAG;
            }

            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            iod = NULL;
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

    if (SS_TO_SESSION(share)->session_sopt.sv_active_capabilities &
        SMB2_GLOBAL_CAP_LEASING) {
        if (createp->ret_oplock_level != SMB2_OPLOCK_LEVEL_LEASE &&
            createp->ret_oplock_level != SMB2_OPLOCK_LEVEL_NONE) {
            /*
             * we don't support oplocks other than leasing
             * if a create response returns an oplock:
             * 1) turn off leasing for this session
             * 2) close the file and reopen it
             */
            SS_TO_SESSION(share)->session_sopt.sv_active_capabilities &=
                                                    ~SMB2_GLOBAL_CAP_LEASING;
            
            struct smb2_close_rq closep = {0};
            closep.share = share;
            closep.fid = createp->ret_fid;
            
            error = smb2_smb_close(share, &closep, NULL, iod, context);
            if (error) {
                SMBDEBUG("smb2_smb_close failed %d ntstatus 0x%x\n",
                         error,
                         closep.ret_ntstatus);
                goto bad;
            }
            
            
            /* Rebuild and try sending again */
            createp->oplock_level = SMB2_OPLOCK_LEVEL_NONE;
            smb_rq_done(rqp);
            rqp = NULL;
            iod = NULL;
            goto resend;
        }
    }
bad:    
    smb_rq_done(rqp);
    return error;
}


static int
smb2_smb_echo(struct smbiod *iod, int timeout, vfs_context_t context)
{
    struct smb_rq *rqp;
    struct mbchain *mbp;
    int error;
    
    /* take a ref for the iod, so that smb_rq_done can release */
    smb_iod_ref(iod, __FUNCTION__);

    /* Allocate request and header for an Echo */
    error = smb2_rq_alloc(SESSION_TO_CP(iod->iod_session), SMB2_ECHO, NULL, context, iod, &rqp);
    if (error) {
        smb_iod_rel(iod, NULL, __FUNCTION__);
        return error;
    }
    
    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

    SMB_LOG_KTRACE(SMB_DBG_SMB_ECHO | DBG_FUNC_START,
                   iod->iod_id, 0, 0, 0, 0);

    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB 2/3 Echo Request
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

    /* pretend like it did not get sent to recover SMB 2/3 credits */
    rqp->sr_extflags &= ~SMB2_REQ_SENT;
    
bad:    
    smb_rq_done(rqp);

    SMB_LOG_KTRACE(SMB_DBG_SMB_ECHO | DBG_FUNC_END, error, 0, 0, 0, 0);
    
    return error;
}

/*
 * This call is done on the session not the share. Really should be an async call
 * if we ever get the request queue to work async.
 */
int
smb_smb_echo(struct smbiod *iod, int timeout, uint32_t EchoCount,
             vfs_context_t context)
{
    struct smb_session *sessionp = iod->iod_session;
    int error;
    
    if (sessionp->session_flags & SMBV_SMB2) {
        error = smb2_smb_echo(iod, timeout, context);
    }
    else {
        error = smb1_echo(sessionp, timeout, EchoCount, context);
    }
    
    return (error);
}

int
smb2_smb_flush(struct smb_share *share, struct smb2_flush_rq *flushp,
               struct smb_rq **compound_rqp, struct smbiod *iod,
               vfs_context_t context)
{
    struct smb_rq *rqp;
    struct mbchain *mbp;
    struct mdchain *mdp;
    int error;
    SMB2FID smb2_fid;

resend:
    if (iod) {
        error = smb_iod_ref(iod, __FUNCTION__);
    }
    else {
        error = smb_iod_get_any_iod(SS_TO_SESSION(share), &iod, __FUNCTION__);
    }
    if (error) {
        return error;
    }

    /* Allocate request and header for a Flush */
    error = smb2_rq_alloc(SSTOCP(share), SMB2_FLUSH, NULL, context, iod, &rqp);
    if (error) {
        smb_iod_rel(iod, NULL, __FUNCTION__);
        return error;
    }
    
    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB 2/3 Flush Request
     */
    mb_put_uint16le(mbp, 24);                   /* Struct size */
	
	if (flushp->full_sync == 1) {
		mb_put_uint16le(mbp, 0xffff);           /* Do F_FULLSYNC */
	}
	else {
		mb_put_uint16le(mbp, 0);                /* Reserved1 */
	}
    mb_put_uint32le(mbp, 0);                    /* Reserved2 */

    /* map fid to SMB 2/3 fid */
    error = smb_fid_get_kernel_fid(share, flushp->fid, 0, &smb2_fid);
    if (error) {
        goto bad;
    }
    mb_put_uint64le(mbp, smb2_fid.fid_persistent);   /* FID */
    mb_put_uint64le(mbp, smb2_fid.fid_volatile);     /* FID */

    if (flushp->mc_flags & SMB2_MC_REPLAY_FLAG) {
        /* This message is replayed - sent after a channel has been disconnected */
        *rqp->sr_flagsp |= SMB2_FLAGS_REPLAY_OPERATIONS;
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
    flushp->ret_ntstatus = rqp->sr_ntstatus;
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            SMB_LOG_MC("resending messageid %llu cmd %u (sr_flags: 0x%x).\n",
                       rqp->sr_messageid, rqp->sr_command, rqp->sr_flags);
            if (rqp->sr_flags & SMBR_ALT_CH_DISCON) {
                /* An alternate channel got disconnected. Resend with the REPLAY flag set */
                flushp->mc_flags |= SMB2_MC_REPLAY_FLAG;
            }

            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            iod = NULL;
            goto resend;
        }

        goto bad;
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    error = smb2_smb_parse_flush(mdp, flushp);
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
    uint64_t alloc_size = 0;
    
    if (logical_size == 0) {
        alloc_size = 0;
    }
    else {
        if (smp != NULL) {
            lck_mtx_lock(&smp->sm_statfslock);
            
            if (smp->sm_statfsbuf.f_bsize != 0) {
                alloc_size = ((logical_size / smp->sm_statfsbuf.f_bsize) * smp->sm_statfsbuf.f_bsize) + smp->sm_statfsbuf.f_bsize;
            }
            else {
                SMBERROR("smp->sm_statfsbuf.f_bsize is 0 \n");
            }
            
            lck_mtx_unlock(&smp->sm_statfslock);
        }
        else {
            SMBERROR("smp is NULL \n");
        }
    }
    
    return(alloc_size);
}

uint32_t
smb2_smb_get_client_capabilities(struct smb_session *sessionp)
{
    uint32_t capabilities = 0;
    uint32_t do_smb3_caps = 0;
    
    if (sessionp == NULL) {
        SMBERROR("Null sessionp \n");
        goto out;
    }
    
    if (sessionp->session_misc_flags & SMBV_NEG_SMB3_ENABLED) {
        /* We can report SMB 3 capabilities */
        do_smb3_caps = 1;
    }
    
    /*
     * If its SMB 3.x, fill in the Capabilities field
     */
    if (do_smb3_caps) {
        capabilities |= SMB2_GLOBAL_CAP_DFS |
                        SMB2_GLOBAL_CAP_LEASING |
                        SMB2_GLOBAL_CAP_LARGE_MTU |
                        SMB2_GLOBAL_CAP_PERSISTENT_HANDLES |
                        SMB2_GLOBAL_CAP_DIRECTORY_LEASING |
                        SMB2_GLOBAL_CAP_ENCRYPTION;

        if (sessionp->session_flags & SMBV_MULTICHANNEL_ON) {
            capabilities |= SMB2_GLOBAL_CAP_MULTI_CHANNEL;
        }
    }

out:
    return(capabilities);
}

uint32_t
smb2_smb_get_client_dialects(struct smb_session *sessionp, int inReconnect,
                             uint16_t *dialect_cnt, uint16_t dialects[],
                             size_t max_dialects_size)
{
    uint32_t error = 0;
    
    /* We have a max of SMB3_MAX_DIALECTS dialects at this time */
    if (max_dialects_size < (sizeof(uint16_t) * SMB3_MAX_DIALECTS)) {
        SMBERROR("Not enough space for dialects %ld \n", max_dialects_size);
        return (ENOMEM);
    }
    
    if (!inReconnect) {
        /*
         * Not in reconnect
         */
		if ((sessionp->session_misc_flags & SMBV_NEG_SMB3_ENABLED) &&
			(sessionp->session_misc_flags & SMBV_NEG_SMB2_ENABLED)) {
			/* SMB 2/3 - five dialects at this time */
            if (sessionp->session_flags & SMBV_DISABLE_311) {
                /* SMB v3.1.1 is disabled */
                SMBWARNING("SMB311 is disabled \n");
                *dialect_cnt = 4;
            }
            else {
                *dialect_cnt = 5;
            }
            
            dialects[0] = SMB2_DIALECT_0202;        /* 2.002 Dialect */
			dialects[1] = SMB2_DIALECT_0210;        /* 2.1 Dialect */
			dialects[2] = SMB2_DIALECT_0300;        /* 3.0 Dialect */
            dialects[3] = SMB2_DIALECT_0302;        /* 3.0.2 Dialect */
            dialects[4] = SMB2_DIALECT_0311;        /* 3.1.1 Dialect */
		}
		else if ((sessionp->session_misc_flags & SMBV_NEG_SMB3_ENABLED) &&
				 !(sessionp->session_misc_flags & SMBV_NEG_SMB2_ENABLED)) {
			/* only support three dialects of SMB 3 */
			*dialect_cnt = 3;
			
			dialects[0] = SMB2_DIALECT_0300;        /* 3.0 Dialect */
            dialects[1] = SMB2_DIALECT_0302;        /* 3.0.2 Dialect */
            dialects[2] = SMB2_DIALECT_0311;        /* 3.1.1 Dialect */
		}
		else if (!(sessionp->session_misc_flags & SMBV_NEG_SMB3_ENABLED) &&
				 (sessionp->session_misc_flags & SMBV_NEG_SMB2_ENABLED)) {
			/* only support two dialects of SMB 2 */
			*dialect_cnt = 2;
			
			dialects[0] = SMB2_DIALECT_0202;        /* 2.002 Dialect */
			dialects[1] = SMB2_DIALECT_0210;        /* 2.1 Dialect */
		}
    }
    else {
        /*
         * In reconnect, stay with whatever version we had before
         * unless session_max_dialect is set
         */
        *dialect_cnt = 1;

        if (sessionp->session_max_dialect != 0) {
           /*
            * <rdar://problem/102811989>
            * This happens in DFS, when a tree connect response returns BAD_CLUSTER_DIALECT
            * In this case we need to negotiate with max_dialect
            */
            dialects[0] = SMB2_DIALECT_0202;        /* 2.002 Dialect */
            dialects[1] = SMB2_DIALECT_0210;        /* 2.1 Dialect */
            dialects[2] = SMB2_DIALECT_0300;        /* 3.0 Dialect */
            dialects[3] = SMB2_DIALECT_0302;        /* 3.0.2 Dialect */
            dialects[4] = SMB2_DIALECT_0311;        /* 3.1.1 Dialect */
            switch (sessionp->session_max_dialect) {
                case SMB2_DIALECT_0202:
                    *dialect_cnt = 1;
                    break;
                case SMB2_DIALECT_0210:
                    *dialect_cnt = 2;
                    break;
                case SMB2_DIALECT_0300:
                    *dialect_cnt = 3;
                    break;
                case SMB2_DIALECT_0302:
                    *dialect_cnt = 4;
                    break;
                case SMB2_DIALECT_0311:
                    *dialect_cnt = 5;
                    break;
                default:
                    // Should not happen
                    SMBERROR("session_max_dialect set to unknown dialect 0x%x",
                             sessionp->session_max_dialect);
                    *dialect_cnt = 5;
            }
            /* Zero out the part of the array that's not needed */
            memset(dialects + *dialect_cnt, 0, sizeof(uint16_t)*(max_dialects_size - *dialect_cnt));

            /* Zero session_max_dialect so the next reconnect uses only one dialect*/
            sessionp->session_max_dialect = 0;
       }
        else if (sessionp->session_flags & SMBV_SMB311) {
            dialects[0] = SMB2_DIALECT_0311;        /* 3.1.1 Dialect */
        }
        else if (sessionp->session_flags & SMBV_SMB302) {
            dialects[0] = SMB2_DIALECT_0302;        /* 3.0.2 Dialect */
        }
        else if (sessionp->session_flags & SMBV_SMB30) {
            dialects[0] = SMB2_DIALECT_0300;        /* 3.0 Dialect */
        }
        else if (sessionp->session_flags & SMBV_SMB21) {
            dialects[0] = SMB2_DIALECT_0210;        /* 2.1 Dialect */
        }
        else if (sessionp->session_flags & SMBV_SMB2002) {
            dialects[0] = SMB2_DIALECT_0202;        /* 2.002 Dialect */
        }
        else {
            SMBERROR("Unknown dialect for reconnect 0x%x \n", sessionp->session_flags);
            error = EINVAL;
        }
    }
    
    return (error);
}

uint16_t
smb2_smb_get_client_security_mode(struct smb_session *sessionp)
{
    uint16_t security_mode = 0;
    
    if (sessionp == NULL) {
        SMBERROR("Null sessionp \n");
    }
    
    /* [MS-SMB2] 2.2.3 Client sets either bit, but not both */
    if ((sessionp) && (sessionp->session_misc_flags & SMBV_CLIENT_SIGNING_REQUIRED)) {
        security_mode |= SMB2_NEGOTIATE_SIGNING_REQUIRED;
	}
    else {
        security_mode |= SMB2_NEGOTIATE_SIGNING_ENABLED;
    }
    
    return (security_mode);
}

/* Send out an session_setup request and process the response */
int
smb2_smb_gss_session_setup(struct smbiod *iod, uint16_t *reply_flags,
                           vfs_context_t context)
{
    struct smb_session *sessionp = iod->iod_session;
	struct smb_rq *rqp = NULL;
	struct smb_gss *gp = &iod->iod_gss;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
	uint32_t tokenlen;
	uint32_t maxtokenlen;
	uint8_t *tokenptr;
    uint16_t length;
	uint16_t sec_buf_offset;
	uint16_t sec_buf_len;
    uint16_t security_mode = 0;
    uint8_t security_mode_byte;
	
#ifdef SMB_DEBUG
	/* For testing use a smaller max size */
	maxtokenlen = 2048;
#else // SMB_DEBUG
	/* Get the max blob size we can send in Session Setup message */
	maxtokenlen = sessionp->session_txmax - (SMB2_HDRLEN); /* %%% To Do - is this right? */
#endif // SMB_DEBUG
    
	tokenptr = gp->gss_token;	/* Get the start of the Kerberos blob */
    
    do {
        if (rqp) {	/* If looping release curr rqp, before getting another */
            smb_rq_done(rqp);
            rqp = NULL;
        }
        /* we need to take a ref to the iod here, smb_rq_done will release */
        smb_iod_ref(iod, __FUNCTION__);
        
        /* Allocate request and header for a Session Setup */
        error = smb2_rq_alloc(SESSION_TO_CP(sessionp), SMB2_SESSION_SETUP, NULL, 
                              context, iod, &rqp);
        if (error) {
            SMBDEBUG("id %d alloc error %d.\n", iod->iod_id, error);
            break;
        }

    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

        /*
         * Fill in Session Setup part
         * Cant use a struct ptr due to var length security blob that
         * could be some giant size greater than a single mbuf.
         */
        smb_rq_getrequest(rqp, &mbp);
        
        mb_put_uint16le(mbp, 25);       /* Struct size */

        if (iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL) {
            /*
             * Alt Channel must:
             * 1. Be signed ([MS-SMB2] 3.3.5.5) using signing key from original
             *    session.
             * 2. Have session binding flag set
             * 3. Session ID must not be 0 (means we are in reconnect)
             *
             */
            rqp->sr_flags |= SMBR_SIGNED;

            mb_put_uint8(mbp, SMB2_SESSION_FLAG_BINDING);   /* Flags */

            if (rqp->sr_rqsessionid == 0) {
                SMBDEBUG("id %d Cancelling Session Binding due to reconnect \n",
                         iod->iod_id);
                error = EAUTH;
                goto bad;
            }
        }
        else {
            /* Main channel */
            mb_put_uint8(mbp, 0);                           /* Flags */
        }

        /* Security Mode (UInt8 in SessSetup instead of UInt16 in Neg) */
        security_mode = smb2_smb_get_client_security_mode(sessionp);
        security_mode_byte = security_mode;
        mb_put_uint8(mbp, security_mode_byte);

		mb_put_uint32le(mbp, SMB2_GLOBAL_CAP_DFS);         /* Capabilities */
        mb_put_uint32le(mbp, 0);        /* Channel - always 0 */
        mb_put_uint16le(mbp, 88);       /* Sec buffer offset */
        tokenlen = (gp->gss_tokenlen > maxtokenlen) ? maxtokenlen : gp->gss_tokenlen;
        mb_put_uint16le(mbp, tokenlen); /* Sec buffer len */
        mb_put_uint64le(mbp, sessionp->session_prev_session_id);      /* Prev SessID */
        
        /* SPNEGO blob */
        mb_put_mem(mbp, (caddr_t) tokenptr, tokenlen, MB_MSYSTEM);
        
        /* Send the request and check for reply */
        error = smb_rq_simple_timed(rqp, SMBSSNSETUPTIMO);

        if ((error) && (rqp->sr_flags & SMBR_RECONNECTED)) {
            if (rqp->sr_flags & SMBR_ALT_CH_DISCON) {
                SMBERROR("id %d An alternate channel got disconnected during session setup. error out",
                         iod->iod_id);
                goto bad;
            }

            /* Rebuild and try sending again */
            continue;
        }
        
        if (!(iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL)) {
            /* This is the first Session Setup Response packet here */
            if (sessionp->session_session_id == 0) {
                sessionp->session_session_id = rqp->sr_rspsessionid;
            }
        }

        /* Move the pointer to the next offset in the blob */
        tokenptr += tokenlen;
        /* Subtract the size we have already sent */
        gp->gss_tokenlen -= tokenlen;
    } while (gp->gss_tokenlen && (error == EAGAIN));
    
	/* Free the gss spnego token that we sent */
    SMB_FREE_DATA(gp->gss_token, gp->gss_token_allocsize);
	gp->gss_tokenlen = 0;
	gp->gss_smb_error = error;	/* Hold on to the last smb error returned */
	/* EAGAIN is not really an error, reset it to no error */
	if (error == EAGAIN) {
		error = 0;
	}
	/* At this point error should have the correct error, we only support NTStatus code with extended security */
	if (error) {
		SMB_LOG_AUTH("id %d Extended security authorization failed! %d\n",
                     iod->iod_id, error);
		goto bad;
	}
    
	/*
	 * Save the servers session identifier. Seems Samba will give us a new one for
	 * every loop of a SetUpAndX NTLMSSP response. Windows server just return
	 * the same one every time. We assume here the last one is the one we 
	 * should always use. Seems to make Samba work correctly.
	 */
    if (!(iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL)) {
        sessionp->session_smbuid = rqp->sr_rpuid;
    }
	
	/* Get the reply  and decode the result */
	smb_rq_getreply(rqp, &mdp);
    
    /* Using SMB 2/3 */
    
    /* 
     * Parse SMB 2/3 Session Setup Response 
     * We are already pointing to begining of Response data
     * Cant cast to sturct pointer due to var len security blob
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
    
    /* Get Session Flags and return them */
    error = md_get_uint16le(mdp, reply_flags);
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
     * Security buffer offset if from the beginning of SMB 2/3 Header
     * Calculate how much further we have to go to get to it.
     */
    if (sec_buf_offset > 0) {
        sec_buf_offset -= SMB2_HDRLEN;
        sec_buf_offset -= 8;   /* already parse 8 bytes worth of the response */

        error = md_get_mem(mdp, NULL, sec_buf_offset, MB_MSYSTEM);
        if (error) {
            goto bad;
        }
    }
    
    /*
     * Set the gss token from the server
     */
    gp->gss_tokenlen = sec_buf_len;
    gp->gss_token_allocsize = sec_buf_len;
    if (sec_buf_len) {
        SMB_MALLOC_DATA(gp->gss_token, gp->gss_token_allocsize, Z_WAITOK);
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
    if (rqp) {
        smb_rq_done(rqp);
    }
    
    if (error) {
        /*
         * If Session Setup failed, then the session_id is no longer valid
         * and have to get a new one to use
         */
        if (!(iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL)) {
            sessionp->session_session_id = 0;
        }
    }
    
	return (error);
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smb2_smb_ioctl(struct smb_share *share, struct smbiod *iod, struct smb2_ioctl_rq *ioctlp,
               struct smb_rq **compound_rqp, vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error, i;
    uint16_t input_buffer_offset;
    SMB2FID smb2_fid;
	uint16_t reparse_len;
	uint16_t substitute_name_offset, substitute_name_len;
	uint16_t print_name_offset, print_name_len;
    uint32_t input_count;
 	struct smb2_get_dfs_referral *dfs_referral;
    uint32_t input_len;
    struct smb2_secure_neg_info *neg_req = NULL;
    uint8_t *guidp = NULL;
    uio_t auio = NULL;

    if (ioctlp == NULL) {
        SMBERROR("ioctlp is null \n");
        return (EBADRPC);
    }
    
resend:
    if (iod) {
        error = smb_iod_ref(iod, __FUNCTION__);
    } else {
        error = smb_iod_get_any_iod(SS_TO_SESSION(share), &iod, __FUNCTION__);
    }
    if (error) {
        return error;
    }

    /* Allocate request and header for an IOCTL */
    error = smb2_rq_alloc(SSTOCP(share), SMB2_IOCTL, NULL, context, iod, &rqp);
    if (error) {
        smb_iod_rel(iod, NULL, __FUNCTION__);
        return error;
    }
    
    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB 2/3 IOCTL Request
     */
    mb_put_uint16le(mbp, 57);                       /* Struct size */
    mb_put_uint16le(mbp, 0);                        /* Reserved */
    mb_put_uint32le(mbp, ioctlp->ctl_code);         /* Ctl code */

    switch (ioctlp->ctl_code) {
        case FSCTL_DFS_GET_REFERRALS:
        case FSCTL_PIPE_WAIT:
        case FSCTL_VALIDATE_NEGOTIATE_INFO:
        case FSCTL_QUERY_NETWORK_INTERFACE_INFO:
            /* must be -1 */
            mb_put_uint64le(mbp, -1);                   /* FID */
            mb_put_uint64le(mbp, -1);                   /* FID */
            break;
            
        default:
            /* map fid to SMB 2/3 fid */
            error = smb_fid_get_kernel_fid(share, ioctlp->fid, 0, &smb2_fid);
            if (error) {
                goto bad;
            }
            mb_put_uint64le(mbp, smb2_fid.fid_persistent); /* FID */
            mb_put_uint64le(mbp, smb2_fid.fid_volatile);   /* FID */
            break;
    }

    switch(ioctlp->ctl_code) {
        case FSCTL_DFS_GET_REFERRALS:
            dfs_referral = (struct smb2_get_dfs_referral *) ioctlp->snd_input_buffer;
            if (dfs_referral == NULL) {
                SMBERROR("dfs_referral is null \n");
                error = EBADRPC;
                goto bad;
            }
            
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
            if ((dfs_referral->file_namep == NULL) &&
                (dfs_referral->file_name_len != 0)) {
                SMBERROR("dfs_referral->file_namep is null \n");
                error = EBADRPC;
                goto bad;
            }

            error = mb_put_mem(mbp, (char *) dfs_referral->file_namep,
                               dfs_referral->file_name_len, MB_MSYSTEM);
            if (error) {
                goto bad;
            }
            
            /* RequestFileName needs to be NULL terminated, so add NULL */
            mb_put_uint16le(mbp, 0);    
            
            break;

        case FSCTL_GET_REPARSE_POINT:
        case FSCTL_SRV_ENUMERATE_SNAPSHOTS:
        case FSCTL_SRV_REQUEST_RESUME_KEY:
        case FSCTL_QUERY_NETWORK_INTERFACE_INFO:
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
        case FSCTL_PIPE_WAIT:
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
            if ((ioctlp->snd_input_uio == NULL) &&
                (ioctlp->snd_input_len != 0)) {
                SMBERROR("ioctlp->snd_input_uio is null \n");
                error = EBADRPC;
                goto bad;
            }
            
            /* Make a copy of uio in case we need to resend */
            auio = uio_duplicate(ioctlp->snd_input_uio);
            if (auio == NULL) {
                SMBERROR("uio_duplicate failed\n");
                error = ENOMEM;
                goto bad;
            }
            
            error = mb_put_uio(mbp, auio,
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
			rqp->sr_extflags |= SMB2_NON_IDEMPOTENT;
			
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
            if (ioctlp->snd_input_buffer == NULL) {
                SMBERROR("ioctlp->snd_input_buffer is null \n");
                error = EBADRPC;
                goto bad;
            }

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
            if ((ioctlp->snd_input_buffer == NULL) &&
                (ioctlp->snd_input_len != 0)) {
                SMBERROR("ioctlp->snd_input_buffer is null \n");
                error = EBADRPC;
                goto bad;
            }

            mb_put_mem(mbp, (char *) ioctlp->snd_input_buffer,
                       ioctlp->snd_input_len, MB_MSYSTEM);
            
            if (SS_TO_SESSION(share)->session_misc_flags & SMBV_OSX_SERVER) {
                /* Tell iod not to timeout this request */
                rqp->sr_flags |= SMBR_NO_TIMEOUT;
            }
            break;
            
        case FSCTL_VALIDATE_NEGOTIATE_INFO:
            /* This request must be signed */
            rqp->sr_flags |= SMBR_SIGNED;
            
            mb_put_uint32le(mbp, 120);                  /* Input offset */
            mb_put_uint32le(mbp, ioctlp->snd_input_len); /* Input count */
            mb_put_uint32le(mbp, 0);                    /* Max input resp */
            mb_put_uint32le(mbp, 0);                    /* Output offset */
            mb_put_uint32le(mbp, 0);                    /* Output count */
            mb_put_uint32le(mbp, ioctlp->rcv_output_len); /* Max output resp */
            mb_put_uint32le(mbp, SMB2_IOCTL_IS_FSCTL);  /* Flags */
            mb_put_uint32le(mbp, 0);                    /* Reserved2 */
            
            /* Fill in Validate Negotiate Request */
            neg_req = (struct smb2_secure_neg_info *) ioctlp->snd_input_buffer;
            
            if (neg_req == NULL) {
                SMBERROR("neg_req is null \n");
                error = EBADRPC;
                goto bad;
            }

            mb_put_uint32le(mbp, neg_req->capabilities);    /* Capabilities */

            guidp = (uint8_t *) mb_reserve(mbp, 16);        /* Client GUID */
            memcpy(guidp, neg_req->guid, 16);
            
            mb_put_uint16le(mbp, neg_req->security_mode);   /* Security Mode */
            mb_put_uint16le(mbp, neg_req->dialect_count);   /* Dialect Count */
            
            for (i = 0; i < neg_req->dialect_count; i++) {
                mb_put_uint16le(mbp, neg_req->dialects[i]);  /* Dialects */
            }
            
            break;

        default:
            SMBERROR("Unsupported ioctl: %d\n", ioctlp->ctl_code);
            error = EBADRPC;
            goto bad;
    }

    if (ioctlp->mc_flags & SMB2_MC_REPLAY_FLAG) {
        /* This message is replayed - sent after a channel has been disconnected */
        *rqp->sr_flagsp |= SMB2_FLAGS_REPLAY_OPERATIONS;
    }

    if (compound_rqp != NULL) {
        /* 
         * building a compound request, add padding to 8 bytes and just
         * return this built request.
         */
        smb2_rq_align8(rqp);
        rqp->sr_flags |= SMBR_COMPOUND_RQ;
        *compound_rqp = rqp;

        if (auio != NULL) {
            /* ASSUME ioctl will work and write entire amount */
            smbfs_uio_update(ioctlp->snd_input_uio, ioctlp->snd_input_len);

            uio_free(auio);
            auio = NULL;
        }

        return (0);
    }

    error = smb_rq_simple(rqp); // Send ioctl and wait for the reply
    ioctlp->ret_ntstatus = rqp->sr_ntstatus;
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            if (ioctlp->ctl_code == FSCTL_VALIDATE_NEGOTIATE_INFO) {
                /*
                 * Can not rebuild and send this again from here because a
                 * reconnect will change the dialect count down to 1.
                 */
                error = ESTALE;
                goto bad;
            }
            
            /* Rebuild and try sending again */
            if (auio != NULL) {
                uio_free(auio);
                auio = NULL;
            }

            SMB_LOG_MC("resending messageid %llu cmd %u.\n", rqp->sr_messageid, rqp->sr_command);
            if (rqp->sr_flags & SMBR_ALT_CH_DISCON) {
                /* An alternate channel got disconnected. Resend with the REPLAY flag set */
                ioctlp->mc_flags |= SMB2_MC_REPLAY_FLAG;
            }

            smb_rq_done(rqp);
            iod = NULL;
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
    else {
        if (auio != NULL) {
            /* Update uio with amount of data written */
            smbfs_uio_update(ioctlp->snd_input_uio, ioctlp->snd_input_len);
        }
    }

    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    error = smb2_smb_parse_ioctl(mdp, ioctlp);
    if (error) {
        goto bad;
    }

bad:    
    if (auio != NULL) {
        uio_free(auio);
    }

    smb_rq_done(rqp);
    return error;
}

int
smb2_smb_lease_break_ack(struct smb_share *share, struct smbiod *iod,
                         uint64_t lease_key_hi, uint64_t lease_key_low,
                         uint32_t lease_state, vfs_context_t context)
{
    struct smb_rq *rqp;
    struct mbchain *mbp;
    struct mdchain *mdp;
    int error;
    uint16_t length;
    uint64_t rsp_lease_key_hi, rsp_lease_key_low;
    bool replay = false;
    uint32_t ret_lease_state = 0;
    
resend:
    if (iod) {
        error = smb_iod_ref(iod, __FUNCTION__);
    } else {
        error = smb_iod_get_any_iod(SS_TO_SESSION(share), &iod, __FUNCTION__);
    }
    if (error) {
        SMBERROR("Failed to find iod for lease key: 0x%llx:0x%llx lease state: 0x%x \n",
                 lease_key_hi, lease_key_low, lease_state);
        return error;
    }

    /* Allocate request and header for a Lease Break Acknowledgement */
    error = smb2_rq_alloc(SSTOCP(share), SMB2_OPLOCK_BREAK, NULL, context, iod, &rqp);
    if (error) {
        smb_iod_rel(iod, NULL, __FUNCTION__);
        return error;
    }
    
    /*
     * <80752785> Have to be careful here since we always send the lease break
     * acknowledgement on the same iod the lease break arrived on. BUT, the
     * share might not be on the same iod that the lease break arrived on.
     * We MUST make sure the Lease Break Ack iod and sessionID match up, so
     * take the sessionID from the session attached to the IOD and use that
     * in the rqp.
     */
    rqp->sr_rqsessionid = iod->iod_session->session_session_id;
    
    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB 2/3 Lease Break Acknowledgement
     */
    mb_put_uint16le(mbp, 36);                   /* Struct size */
    mb_put_uint16le(mbp, 0);                    /* Reserved */
    mb_put_uint32le(mbp, 0);                    /* Flags (unused) */
    mb_put_uint64le(mbp, lease_key_hi);         /* Lease Key High */
    mb_put_uint64le(mbp, lease_key_low);        /* Lease Key Low */
    mb_put_uint32le(mbp, lease_state);          /* Lease State */
    mb_put_uint64le(mbp, 0);                    /* Lease Duration (unused) */

    if (replay) {
        /* This message is replayed - sent after a channel has been disconnected */
        *rqp->sr_flagsp |= SMB2_FLAGS_REPLAY_OPERATIONS;
    }

    error = smb_rq_simple(rqp);
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            SMB_LOG_MC("resending messageid %llu cmd %u.\n", rqp->sr_messageid, rqp->sr_command);
            if (rqp->sr_flags & SMBR_ALT_CH_DISCON) {
                /* An alternate channel got disconnected. Resend with the REPLAY flag set */
                replay = true;
            }

            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            iod = NULL;
            goto resend;
        }
        
        SMBERROR("smb_rq_simple() failed %d for lease key: 0x%llx:0x%llx lease state: 0x%x \n",
                 error, lease_key_hi, lease_key_low, lease_state);
        goto bad;
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    /*
     * Parse SMB 2/3 Lease Break Acknowledgement
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

    /* Get returned Lease State and do nothing with it at this time */
    error = md_get_uint32le(mdp, &ret_lease_state);
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
smb2_smb_lease_break_ack_queue(struct smb_share *share, struct smbiod *iod,
                               uint64_t lease_key_hi, uint64_t lease_key_low,
                               uint32_t lease_state, vfs_context_t context)
{
    struct smb_rw_arg *rw_pb_ptr = NULL;
    
    /* Malloc a smb_rw */
    SMB_MALLOC_TYPE(rw_pb_ptr, struct smb_rw_arg, Z_WAITOK_ZERO);
    if (rw_pb_ptr == NULL) {
        SMBERROR("SMB_MALLOC_TYPE failed\n");
        return ENOMEM;
    }

    /* Fill it out */
    lck_mtx_init(&rw_pb_ptr->rw_arg_lock, smb_rw_group, LCK_ATTR_NULL);

    rw_pb_ptr->command = SMB_LEASE_BREAK_ACK;
    rw_pb_ptr->lease.share = share;
    rw_pb_ptr->lease.iod = iod;
    rw_pb_ptr->lease.lease_key_hi = lease_key_hi;
    rw_pb_ptr->lease.lease_key_low = lease_key_low;
    rw_pb_ptr->lease.lease_state = lease_state;
    rw_pb_ptr->lease.context = context;

    /*
     * Queue up the lease break ack to be sent by rw helper threads
     * The rw helper threads will call smb2_smb_lease_break_ack() to
     * actually send the ack request and handle the reply
     *
     * Note: the rw helper thread is responsible for freeing rw_arg_lock
     * and this rw_pb_ptr
     */
    rw_pb_ptr->flags |= SMB_RW_IN_USE;
    smb_rw_proxy(rw_pb_ptr);
    
    return(0);
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
    struct smbiod *iod = NULL;
    bool replay = false;

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
    error = smb_iod_get_any_iod(SS_TO_SESSION(share), &iod, __FUNCTION__);
    if (error) {
        return error;
    }

    /* Allocate request and header for a Lock */
    error = smb2_rq_alloc(SSTOCP(share), SMB2_LOCK, NULL, context, iod, &rqp);
    if (error) {
        smb_iod_rel(iod, NULL, __FUNCTION__);
        return error;
    }

    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

    rqp->sr_extflags |= SMB2_NON_IDEMPOTENT;

    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB 2/3 Lock Request
     */
    mb_put_uint16le(mbp, 48);       /* Struct size */
    mb_put_uint16le(mbp, 1);        /* LockCount */
    mb_put_uint32le(mbp, 0);        /* LockSequence %%% Should this be something unique??? */
    
    /* map fid to SMB 2/3 fid */
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

    if (replay) {
        /* This message is replayed - sent after a channel has been disconnected */
        *rqp->sr_flagsp |= SMB2_FLAGS_REPLAY_OPERATIONS;
    }

    error = smb_rq_simple(rqp);
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            SMB_LOG_MC("resending messageid %llu cmd %u.\n", rqp->sr_messageid, rqp->sr_command);
            if (rqp->sr_flags & SMBR_ALT_CH_DISCON) {
                /* An alternate channel got disconnected. Resend with the REPLAY flag set */
                replay = true;
            }

            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            iod = NULL;
            goto resend;
        }

        goto bad;
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    /* 
     * Parse SMB 2/3 Lock Response 
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
smb2_smb_logoff(struct smb_session *sessionp, vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
	uint16_t length;
    struct smbiod *iod = NULL;
    bool replay = false;

resend:
    error = smb_iod_get_any_iod(sessionp, &iod, __FUNCTION__);
    if (error) {
        return error;
    }

    /* Allocate request and header for a Logoff */
	error = smb2_rq_alloc(SESSION_TO_CP(sessionp), SMB2_LOGOFF, NULL, context, iod, &rqp);
    if (error) {
        smb_iod_rel(iod, NULL, __FUNCTION__);
		return error;
    }
    
    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

    /*
     * Fill in Logoff part 
     * Dont use struct ptr since its only 4 bytes long
     */
    smb_rq_getrequest(rqp, &mbp);
    
    mb_put_uint16le(mbp, 4);       /* Struct size */
    mb_put_uint16le(mbp, 0);       /* Reserved */

    if (replay) {
        /* This message is replayed - sent after a channel has been disconnected */
        *rqp->sr_flagsp |= SMB2_FLAGS_REPLAY_OPERATIONS;
    }

    error = smb_rq_simple(rqp);
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            SMB_LOG_MC("resending messageid %llu cmd %u.\n", rqp->sr_messageid, rqp->sr_command);
            if (rqp->sr_flags & SMBR_ALT_CH_DISCON) {
                /* An alternate channel got disconnected. Resend with the REPLAY flag set */
                replay = true;
            }

            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            iod = NULL;
            goto resend;
        }

        goto bad;
    }
    
    /* Always assume the log off worked and clear out session id */
    sessionp->session_session_id = 0;
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    /* 
     * Parse SMB 2/3 Logoff 
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
smb_smb_ssnclose(struct smb_session *sessionp, vfs_context_t context)
{
	int error;
    
	if (sessionp->session_smbuid == SMB_UID_UNKNOWN)
		return 0;
    
	if (smb_smb_nomux(sessionp, __FUNCTION__, context) != 0)
		return EINVAL;
    
    if (sessionp->session_flags & SMBV_SMB2) {
        error = smb2_smb_logoff(sessionp, context);
    }
    else {
        error = smb1_smb_ssnclose(sessionp, context);
    }
    return (error);
}

int
smb2_smb_negotiate(struct smbiod *iod, struct smb_rq *in_rqp, int inReconnect,
                   vfs_context_t user_context, vfs_context_t context)
{
    struct smb_session *sessionp = NULL;
	struct smb_sopt *sp = NULL;
	struct smb_rq *rqp = NULL;
	struct mbchain *mbp;
	int error;
    uint32_t original_caps, neg_len = 0;
    uint8_t *guidp;
    int i;
    uint32_t *neg_context_offsetp;
    uint16_t *neg_context_countp;
    int pad_bytes = 0;
    int add_neg_contexts = 0;

    /* Sanity checks */
    if (iod == NULL) {
        SMBERROR(" iod is null? \n");
        error = EINVAL;
        goto bad;
    }
    sessionp = iod->iod_session;

    if (sessionp == NULL) {
        SMBERROR(" sessionp is null? \n");
        error = EINVAL;
        goto bad;
    }
    sp = &sessionp->session_sopt;

    if (sp == NULL) {
        SMBERROR(" sp is null? \n");
        error = EINVAL;
        goto bad;
    }
    original_caps = sp->sv_caps;

    if (!(iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL)) {
        /* Main channel: Init some session vars */
        sessionp->session_hflags = SMB_FLAGS_CASELESS;
        /* Leave SMB_FLAGS2_UNICODE "off" - no need to do anything */
        sessionp->session_hflags2 |= SMB_FLAGS2_ERR_STATUS;

        if (in_rqp != NULL) {
            /*
             * Auto Negotiate from SMB 1
             * Parse SMB 2/3 Neg Response that we got from our SMB 1 Neg Request
             */
            error = smb2_smb_parse_negotiate(sessionp, in_rqp, 1);
            if (error) {
                SMBERROR("smb2_smb_negotiate_parse for SMB 1 failed %d\n", error);
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
                    sessionp->session_flags |= SMBV_SMB2002;
                    SMBDEBUG("SMB 2.002 DIALECT\n");
                }
                else {
                    sessionp->session_flags |= SMBV_SMB21;
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
    }

resend:
    /* take a reference on the iod, smb_rq_done will release */
    smb_iod_ref(iod, __FUNCTION__);
    
    /* Allocate request and header for a Negotiate */
    error = smb2_rq_alloc(SESSION_TO_CP(sessionp), SMB2_NEGOTIATE, NULL, context, iod, &rqp);
    if (error) {
        return error;
    }
    
    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB 2/3 Negotiate Request
     */
	mb_put_uint16le(mbp, 36);                               /* Struct Size */

    /* Get dialect count */
    if ((inReconnect) ||
        (iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL)) {
        /*
         * Alt channel: Only one dialect and dont update session dialects
         * We do want to update the sessionp neg dialect fields so that
         * subsequent Validate Neg will work properly.
         */
        error = smb2_smb_get_client_dialects(sessionp,
                                             TRUE,
                                             &sessionp->neg_dialect_count,
                                             sessionp->neg_dialects,
                                             sizeof(sessionp->neg_dialects));
    }
    else {
        /* Main channel: Get all the possible dialects */
        error = smb2_smb_get_client_dialects(sessionp,
                                             inReconnect,
                                             &sessionp->neg_dialect_count,
                                             sessionp->neg_dialects,
                                             sizeof(sessionp->neg_dialects));
    }
    if (error) {
        return error;
    }

    mb_put_uint16le(mbp, sessionp->neg_dialect_count);      /* Dialect Count */

    /* Security Mode */
    sessionp->neg_security_mode = smb2_smb_get_client_security_mode(sessionp);
    mb_put_uint16le(mbp, sessionp->neg_security_mode);
    
	mb_put_uint16le(mbp, 0);                                /*  Reserved */
    
    /* 
     * If its SMB 3.x, fill in the Capabilities field 
     */
    sessionp->neg_capabilities = smb2_smb_get_client_capabilities(sessionp);
	mb_put_uint32le(mbp, sessionp->neg_capabilities);       /* Capabilities */
    
    guidp = (uint8_t *) mb_reserve(mbp, 16);                /* Client GUID */
    memcpy(guidp, sessionp->session_client_guid, 16);

    neg_context_offsetp = mb_reserve(mbp, sizeof(uint32_t));    /* Reserve space for NegContextOffset */
    neg_context_countp = mb_reserve(mbp, sizeof(uint16_t));     /* Reserve space for NegContextCount */
    mb_put_uint16le(mbp, 0);                                /* Reserved */

    for (i = 0; i < sessionp->neg_dialect_count; i++) { /* Dialects */
        mb_put_uint16le(mbp, sessionp->neg_dialects[i]);
    }

    neg_len = 36 + (2 * sessionp->neg_dialect_count);

    if (!(sessionp->session_flags & SMBV_DISABLE_311)) {
        /* If its SMB 3.1.1 (and its enabled), add in the Negotiate Contexts */
        if ((neg_len % 8) != 0) {
            /* Contexts MUST start on next 8 byte boundary! */
            pad_bytes = 8 - (neg_len % 8);
            mb_put_mem(mbp, NULL, pad_bytes, MB_MZERO);
            neg_len += pad_bytes;
        }

        if ((inReconnect) ||
            (iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL)) {
            /* Is the one dialect SMB 3.1.1? */
            if (sessionp->neg_dialects[0] == SMB2_DIALECT_0311) {
                add_neg_contexts = 1;
            }
        }
        else {
            if (sessionp->session_misc_flags & SMBV_NEG_SMB3_ENABLED) {
                add_neg_contexts = 1;
            }
        }
    }

    if (add_neg_contexts) {
        /* Neg context offset is HdrLen (64) + NegLen */
        *neg_context_offsetp = htolel(neg_len + 64); /* Set NegContextOffset */

        if (iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL) {
            /* Alt channel: add only one crypto cipher */
            error = smb2_smb_add_negotiate_contexts(sessionp, iod, mbp,
                                                    neg_context_countp,
                                                    TRUE);
        }
        else {
            /* Main channel: Add all crypto ciphers we support */
           error = smb2_smb_add_negotiate_contexts(sessionp, iod, mbp,
                                                    neg_context_countp,
                                                    inReconnect);
        }
        if (error) {
            SMBERROR("smb2_smb_add_negotiate_contexts failed %d \n", error);
            goto bad;
        }
    }
    else {
        /* If not including SMB 3.1.1 dialect, then set these to 0 */
        *neg_context_offsetp = 0;
        *neg_context_countp = 0;
    }

    /* Send the Negotiate Request and wait for a reply */
    error = smb_rq_simple(rqp);
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            SMB_LOG_MC("resending messageid %llu cmd %u.\n", rqp->sr_messageid, rqp->sr_command);

            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            goto resend;
        }
        
        goto bad;
    }

    /*
     * Parse SMB 2/3 Negotiate Response
     */
    error = smb2_smb_parse_negotiate(sessionp, rqp, 0);
    if (error) {
        SMBERROR("smb2_smb_negotiate_parse failed %d\n", error);
        goto bad;
    }
    
do_session_setup:
    /* Client requires signing, make sure Server supports signing */
	if ((sessionp->session_misc_flags & SMBV_CLIENT_SIGNING_REQUIRED) &&
		(sessionp->session_misc_flags & SMBV_SMB2_SIGNING_REQUIRED) &&
		(sessionp->session_flags & (SMBV_SMB21 | SMBV_SMB2002)) &&
		!(sp->sv_security_mode & SMB2_NEGOTIATE_SIGNING_ENABLED)) {
		SMBERROR(" SMB Client requires signing for SMB 2.x, but server has signing disabled.\n");
		error = EAUTH;
		goto bad;
	}

	/* Client requires signing, make sure Server supports signing */
	if ((sessionp->session_misc_flags & SMBV_CLIENT_SIGNING_REQUIRED) &&
		(sessionp->session_misc_flags & SMBV_SMB3_SIGNING_REQUIRED) &&
		(SMBV_SMB3_OR_LATER(sessionp)) &&
		!(sp->sv_security_mode & SMB2_NEGOTIATE_SIGNING_ENABLED)) {
		SMBERROR(" SMB Client requires signing for SMB 3.x, but server has signing disabled.\n");
		error = EAUTH;
		goto bad;
	}

    if (!(iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL)) {
        /*
         * Main channel
         */

        /* If no token then say we have no length */
        if (iod->negotiate_token == NULL) {
            iod->negotiate_tokenlen = 0;
        }
        error = smb_gss_negotiate(iod, user_context);
        if (error) {
            SMBERROR("smb_gss_negotiate error %d\n", error);
            goto bad;
        }

        sessionp->session_rxmax = smb2_session_maxread(sessionp, smb_maxread);
        sessionp->session_wxmax = smb2_session_maxwrite(sessionp, smb_maxwrite);
        sessionp->session_txmax = smb2_session_maxtransact(sessionp);

        /* When doing a reconnect we never allow them to change the encode */
        if (inReconnect) {
            /* this is set via AAPL, so clear it from the original_caps */
            original_caps &= ~SMB_CAP_UNIX;

            if (original_caps != sp->sv_caps)
                SMBWARNING("Reconnecting with different sv_caps %x != %x\n",
                           original_caps, sp->sv_caps);
        }
    }

bad:
	if (rqp != NULL) {
        smb_rq_done(rqp);  
    }
	return error;
}

int
smb_smb_negotiate(struct smbiod *iod, vfs_context_t user_context,
                  int inReconnect, vfs_context_t context)
{
    int error = EINVAL;
    struct smb_session *sessionp = iod->iod_session;
    
    if (inReconnect) {
        /* Doing reconnect or alternate channel:
         * If using SMB 2/3, then stay with SMB 2/3
         * If SMB 1, then stay with SMB 1
         */
        if (sessionp->session_flags & SMBV_SMB2) {
            SMBDEBUG("id %d Connecting SMB 2/3.\n", iod->iod_id);
            /*
             * For SMB 2/3 Only Negotiate and not the Multi Protocol
             * Negotiate, message_id has to start with 0.
             */
            iod->iod_message_id = 0;
            error = smb2_smb_negotiate(iod, NULL, inReconnect,
                                       user_context, context);
        }
        else {
            error = smb1_smb_negotiate(sessionp, user_context, inReconnect, 1,
                                       context);
        }
    }
    else {
        /* Not in reconnect: */
		if (sessionp->session_misc_flags &
			(SMBV_NEG_SMB2_ENABLED | SMBV_NEG_SMB3_ENABLED)) {
			/*
			 * This could be any of the following
			 * 1) SMB 1/2/3 enabled
			 * 2) SMB 2/3 enabled and SMB 1 disabled
			 * 3) SMB 2 enabled and SMB 1 and 3 disabled
			 * 4) SMB 3 enabled and SMB 1 and 2 disabled
			 *
			 * If SMB 1 is enabled, start with SMB 1 and try to negotiate up to
			 * SMB 2/3.
			 *
			 * Selecting just SMB 2 or 3 is done in 
			 * smb2_smb_get_client_dialects()
			 */
			if (!(sessionp->session_misc_flags & SMBV_NEG_SMB1_ENABLED)) {
				/* Only SMB 2/3 is allowed */
				iod->iod_message_id = 0;
				error = smb2_smb_negotiate(iod, NULL, inReconnect,
										   user_context, context);
			}
			else {
				/*
				 * If SMB 1 is enabled, start with SMB 1 and try to negotiate
				 * up to SMB 2/3.
				 */
				error = smb1_smb_negotiate(sessionp, user_context, inReconnect,
										   0, context);
			}
			goto out;
		}

		if (!(sessionp->session_misc_flags & (SMBV_NEG_SMB2_ENABLED | SMBV_NEG_SMB3_ENABLED)) &&
			(sessionp->session_misc_flags & SMBV_NEG_SMB1_ENABLED)) {
			/*
			 * SMB 2/3 disabled and SMB 1 enabled
			 * Do SMB 1 only negotiate
			 */
			error = smb1_smb_negotiate(sessionp, user_context, inReconnect, 1,
									   context);
			goto out;
		}
    }
out:
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
     * Parse SMB 2/3 Change Notify Response 
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
     * Output buffer offset is from the beginning of SMB 2/3 Header
     * Calculate how much further we have to go to get to it.
     */
    if (output_buffer_offset > 0) {
        output_buffer_offset -= SMB2_HDRLEN;
        output_buffer_offset -= 8;   /* already parsed 8 bytes worth of the response */

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
    uint16_t flags;
    uint32_t reservedInt;
    uint16_t length;
    SMB2FID smb2_fid; 

    /* 
     * Parse SMB 2/3 Close Response 
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
    error = md_get_uint16le(mdp, &closep->ret_flags);
    if (error) {
        goto bad;
    }
    
    /* We asked for attributes, did the server give them to us? */
    if ((flags & SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB) &&
        !(closep->ret_flags & SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB)) {
        /* Log an error, but dont fail the close */
        SMBERROR("Bad SMB 2/3 Server, did not return close attributes\n");
        //error = EBADRPC;
        //goto bad;
    }
    
    /* Get the attributes */
    if (closep->ret_flags & SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB) {
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
        /* Check for a bad server returning invalid values */
        if (closep->ret_create_time == 0) {
            SMBWARNING("Bad SMB 2/3 Server, close attr of CreationTime is 0 \n");
            /* Assume all the close attrs are invalid */
            closep->ret_flags &= ~SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB;
        }
        
        /* Get Last Access Time */
        error = md_get_uint64le(mdp, &closep->ret_access_time);
        if (error) {
            goto bad;
        }
        /* Check for a bad server returning invalid values */
        if (closep->ret_access_time == 0) {
            SMBERROR("Bad SMB 2/3 Server, close attr of LastAccessTime is 0 \n");
            /* Assume all the close attrs are invalid */
            closep->ret_flags &= ~SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB;
        }

        /* Get Last Write Time */
        error = md_get_uint64le(mdp, &closep->ret_write_time);
        if (error) {
            goto bad;
        }
        /* Check for a bad server returning invalid values */
        if (closep->ret_write_time == 0) {
            SMBERROR("Bad SMB 2/3 Server, close attr of LastWriteTime is 0 \n");
            /* Assume all the close attrs are invalid */
            closep->ret_flags &= ~SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB;
        }

        /* Get Change Time */
        error = md_get_uint64le(mdp, &closep->ret_change_time);
        if (error) {
            goto bad;
        }
        /* Check for a bad server returning invalid values */
        if (closep->ret_create_time == 0) {
            SMBERROR("Bad SMB 2/3 Server, close attr of ChangeTime is 0 \n");
            /* Assume all the close attrs are invalid */
            closep->ret_flags &= ~SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB;
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
smb2_smb_parse_ioctl_get_data(struct mdchain *mdp, struct smb2_ioctl_rq *ioctlp)
{
    int     error = 0;
    char    *data = NULL;
    
    SMB_MALLOC_DATA(data, ioctlp->ret_output_len, Z_WAITOK_ZERO);
    if (data == NULL) {
        error = ENOMEM;
        goto bad;
    }
    
    error = md_get_mem(mdp, (void *) data,
                       (size_t) ioctlp->ret_output_len, MB_MSYSTEM);
    if (error) {
        SMB_FREE_DATA(data, ioctlp->ret_output_len);
        goto bad;
    }

    ioctlp->rcv_output_buffer = (uint8_t *) data;
    ioctlp->rcv_output_len = (uint32_t) ioctlp->ret_output_len;
    ioctlp->rcv_output_allocsize = ioctlp->rcv_output_len;

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
    struct smb_session *sessionp = NULL;
    struct smb2_dur_hndl_and_lease *dur_hndl_leasep = NULL;

	if (share == NULL) {
		SMBERROR("share is null \n");
		error = EBADRPC;
		goto bad;
	}
	
	sessionp = SS_TO_SESSION(share);
	if (sessionp == NULL) {
		SMBERROR("sessionp is null \n");
		error = EBADRPC;
		goto bad;
	}
	
    /*
     * Parse SMB 2/3 Create Response
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
    
    /* Get SMB 2/3 File ID and create user fid to return */
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
     * Context offset is from the beginning of SMB 2/3 Header
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
        !(sessionp->session_misc_flags & SMBV_OSX_SERVER)) {
        SMBDEBUG("Found a NON OS X server\n");
        sessionp->session_misc_flags |= SMBV_OTHER_SERVER;
    }

    if (error == 0) {
        if (!(createp->flags & (SMB2_CREATE_DUR_HANDLE_RECONNECT | SMB2_CREATE_HANDLE_RECONNECT))) {
            /* Only create user fid if the create worked */
            error = smb_fid_get_user_fid(share, smb2_fid, &createp->ret_fid);
            if (error) {
                goto bad;
            }
        }
        else {
            /* Update user fid with new kernel fid */
            dur_hndl_leasep = createp->create_contextp;
            if (dur_hndl_leasep == NULL) {
                SMBERROR("dur_hndl_leasep is NULL \n");
                error = EBADRPC;
                goto bad;
            }

            if (dur_hndl_leasep->dur_handlep == NULL) {
                SMBERROR("dur_handlep is NULL \n");
                error = EBADRPC;
                goto bad;
            }

            error = smb_fid_update_kernel_fid(share, dur_hndl_leasep->dur_handlep->fid,
                                              smb2_fid);
            if (!error) {
                /* 
                 * The user fid remains the same, but the kernel fid has been
                 * successfully updated.
                 */
                createp->ret_fid = dur_hndl_leasep->dur_handlep->fid;
            }
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
    size_t local_str_allocsize = 0;
    struct smb_session *sessionp = SS_TO_SESSION(share);
    struct smb2_create_ctx_resolve_id *resolve_idp = NULL;
    struct smb2_dur_hndl_and_lease *dur_hndl_leasep = NULL;
    uint32_t ntstatus = 0;
    struct mdchain md_context_shadow;
    uint64_t lease_key_hi;
    uint64_t lease_key_low = 0;
	uint32_t flags = 0;
	uint16_t server_epoch = 0;
	uint32_t timeout = 0;
	uint32_t new_lease_state = 0;

    /* Read in any pad bytes */
    if (*ret_context_offset > 0) {
        *ret_context_offset -= SMB2_HDRLEN;
        /* already parsed 88 bytes worth of this response */
        *ret_context_offset -= 88;

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
            case SMB2_CREATE_QUERY_ON_DISK_ID:
                if (rsp_context_data_len != 32) {
                    SMBERROR("Illegal QFid data len: %u\n",
                             rsp_context_data_len);
                    error = EBADRPC;
                    goto bad;
                }

                /* Get DiskFileId */
                error = md_get_uint64le(&md_context_shadow,
                                        &createp->ret_disk_file_id);
                if (error) {
                    goto bad;
                }

                /* Get VolumeId */
                error = md_get_uint64le(&md_context_shadow,
                                        &createp->ret_volume_id);
                if (error) {
                    goto bad;
                }

                /* Get Reserved1 and ignore */
                error = md_get_uint64le(&md_context_shadow, NULL);
                if (error) {
                    goto bad;
                }

                /* Get Reserved2 and ignore */
                error = md_get_uint64le(&md_context_shadow, NULL);
                if (error) {
                    goto bad;
                }
                break;

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
                    
                    if (createp->flags & SMB2_CREATE_ASSUME_DELETE) {
                        max_access |= SMB2_DELETE;
                    }
                    
                    createp->ret_max_access = max_access;
                }
                break;
                
            case SMB2_CREATE_REQUEST_LEASE:    /* RqLs */
				/* 
				 * This is also the response to 
				 * SMB2_CREATE_DURABLE_HANDLE_RECONNECT_V2 
				 */
                dur_hndl_leasep = createp->create_contextp;
                if (dur_hndl_leasep == NULL) {
                    SMBERROR("dur_hndl_leasep is NULL \n");
                    error = EBADRPC;
                    goto bad;
                }

                if (dur_hndl_leasep->leasep == NULL) {
                    SMBERROR("leasep is NULL \n");
                    error = EBADRPC;
                    goto bad;
                }

                if ((rsp_context_data_len != 32) &&
					(rsp_context_data_len != 52)) {
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
                
				/* Lock the lease */
                smbnode_lease_lock(dur_hndl_leasep->leasep, smb2_smb_parse_create_contexts);

                if ((lease_key_hi != dur_hndl_leasep->leasep->lease_key_hi) ||
                    (lease_key_low != dur_hndl_leasep->leasep->lease_key_low)) {
                    SMBERROR("Lease key mismatch: 0x%llx:0x%llx != 0x%llx:0x%llx\n",
                             dur_hndl_leasep->leasep->lease_key_hi,
                             dur_hndl_leasep->leasep->lease_key_low,
                             lease_key_hi, lease_key_low);
                    error = EBADRPC;
                    smbnode_lease_unlock(dur_hndl_leasep->leasep);
                    goto bad;
                }

                /* Get Lease State */
                error = md_get_uint32le(&md_context_shadow, &new_lease_state);
                if (error) {
                    smbnode_lease_unlock(dur_hndl_leasep->leasep);
                    goto bad;
                }

                /* Get Lease Flags */
                error = md_get_uint32le(&md_context_shadow, &flags);
                if (error) {
                    smbnode_lease_unlock(dur_hndl_leasep->leasep);
                    goto bad;
                }
				
				if (flags & SMB2_LEASE_FLAG_PARENT_LEASE_KEY_SET) {
                    dur_hndl_leasep->leasep->flags |= SMB2_LEASE_PARENT_LEASE_KEY_SET;
				}
				
                /* Get Lease Duration - always 0 */
                error = md_get_uint64le(&md_context_shadow, NULL);
                if (error) {
                    smbnode_lease_unlock(dur_hndl_leasep->leasep);
                    goto bad;
                }
				
				if (rsp_context_data_len == 52) {
					/*
					 * Because of the length, this must be a Lease V2 response
					 */
					
					/* Get Parent Lease Key */
					error = md_get_uint64le(&md_context_shadow, &lease_key_hi);
					if (error) {
                        smbnode_lease_unlock(dur_hndl_leasep->leasep);
						goto bad;
					}
					
					error = md_get_uint64le(&md_context_shadow, &lease_key_low);
					if (error) {
                        smbnode_lease_unlock(dur_hndl_leasep->leasep);
						goto bad;
					}
					
					if (dur_hndl_leasep->leasep->flags & SMB2_LEASE_PARENT_LEASE_KEY_SET) {
						/* 
						 * Only check returned lease key values if the parent
						 * lease key bit is set. If they are different, just
						 * log an error and then ignore it.
						 */
						if ((lease_key_hi != dur_hndl_leasep->leasep->par_lease_key_hi) ||
							(lease_key_low != dur_hndl_leasep->leasep->par_lease_key_low)) {
							SMBERROR("Parent Lease key mismatch: 0x%llx:0x%llx != 0x%llx:0x%llx\n",
                                     dur_hndl_leasep->leasep->par_lease_key_hi,
                                     dur_hndl_leasep->leasep->par_lease_key_low,
									 lease_key_hi, lease_key_low);
						}
					}
					
					/* Get Epoch */
					error = md_get_uint16le(&md_context_shadow, &server_epoch);
					if (error) {
                        smbnode_lease_unlock(dur_hndl_leasep->leasep);
						goto bad;
					}
					
					/* Check delta_epoch later */
                    dur_hndl_leasep->leasep->epoch = server_epoch;
                    dur_hndl_leasep->leasep->lease_state = new_lease_state;
					
					/* Get Reserved */
					error = md_get_uint16le(&md_context_shadow, NULL);
					if (error) {
                        smbnode_lease_unlock(dur_hndl_leasep->leasep);
						goto bad;
					}
					
                    /* Did we request a Lease V2? */
                    if (!(dur_hndl_leasep->leasep->flags & SMB2_LEASE_V2)) {
                        /* We got a V2 response, but we sent a V1 request? */
                        SMBERROR("Received lease V2 response for a lease V1 request \n");
                        dur_hndl_leasep->leasep->flags |= SMB2_LEASE_FAIL;
                    }
                }
                else {
                    /* Must be Lease V1 response */
                    dur_hndl_leasep->leasep->lease_state = new_lease_state;
                    
                    /* Did we request a Lease V1? */
                    if (dur_hndl_leasep->leasep->flags & SMB2_LEASE_V2) {
                        /* We got a V1 response, but we sent a V2 request? */
                        SMBERROR("Received lease V1 response for a lease V2 request \n");
                        dur_hndl_leasep->leasep->flags |= SMB2_LEASE_FAIL;
                    }
                }
                
                /*
                 * For SMB2_CREATE_DURABLE_HANDLE_RECONNECT, the response MAY
                 * only contain a lease reply [MS-SMB2] 3.3.5.9.7
                 *
                 * For SMB2_CREATE_DURABLE_HANDLE_RECONNECT_V2, the response is
                 * only a lease reply [MS-SMB2] 3.3.5.9.12
                 *
                 * For both durable handle reconnect types, need to set that
                 * we got the durable handle
                 */
                if (dur_hndl_leasep->dur_handlep != NULL) {
                    if (dur_hndl_leasep->dur_handlep->flags & SMB2_PERSISTENT_HANDLE_RECONNECT) {
                        /* We asked for a persistent handle */
                        dur_hndl_leasep->dur_handlep->flags |= SMB2_PERSISTENT_HANDLE_GRANTED;
                        dur_hndl_leasep->dur_handlep->flags &= ~SMB2_PERSISTENT_HANDLE_RECONNECT;
                    }
                    else {
                        if (dur_hndl_leasep->dur_handlep->flags & SMB2_DURABLE_HANDLE_RECONNECT) {
                            /* We asked for a durable handle */
                            dur_hndl_leasep->dur_handlep->flags |= SMB2_DURABLE_HANDLE_GRANTED;
                            dur_hndl_leasep->dur_handlep->flags &= ~SMB2_DURABLE_HANDLE_RECONNECT;
                        }
                        
                        /* Could just be a dir lease response */
                    }
                 }

                dur_hndl_leasep->leasep->flags |= SMB2_LEASE_GRANTED;
                
                /* Unlock the lease */
                smbnode_lease_unlock(dur_hndl_leasep->leasep);

                break;

            case SMB2_CREATE_DURABLE_HANDLE_REQUEST:    /* DHnQ */
                dur_hndl_leasep = createp->create_contextp;
                if (dur_hndl_leasep == NULL) {
                    SMBERROR("dur_hndl_leasep is NULL \n");
                    error = EBADRPC;
                    goto bad;
                }

                if (dur_hndl_leasep->dur_handlep == NULL) {
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

				/* Lock the dur handle */
                smbnode_dur_handle_lock(dur_hndl_leasep->dur_handlep, smb2_smb_parse_create_contexts);

				/* Did we request a Durable Handle V1? */
				if (dur_hndl_leasep->dur_handlep->flags & SMB2_DURABLE_HANDLE_V2) {
					/* We got a V1 response, but we sent a V2 request? */
					SMBERROR("Received Durable Handle V1 response for a Durable Handle V2 request \n");
                    dur_hndl_leasep->dur_handlep->flags |= SMB2_DURABLE_HANDLE_FAIL;
				}

                dur_hndl_leasep->dur_handlep->flags |= SMB2_DURABLE_HANDLE_GRANTED;
                dur_hndl_leasep->dur_handlep->flags &= ~SMB2_DURABLE_HANDLE_REQUEST;

				/* Unlock the dur handle */
                smbnode_dur_handle_unlock(dur_hndl_leasep->dur_handlep);

				break;

			case SMB2_CREATE_DURABLE_HANDLE_REQUEST_V2:    /* DH2Q */
                dur_hndl_leasep = createp->create_contextp;
                if (dur_hndl_leasep == NULL) {
                    SMBERROR("dur_hndl_leasep is NULL \n");
                    error = EBADRPC;
                    goto bad;
                }

                if (dur_hndl_leasep->dur_handlep == NULL) {
                    SMBERROR("dur_handlep is NULL \n");
                    error = EBADRPC;
                    goto bad;
                }

				if (rsp_context_data_len != 8) {
					SMBERROR("Illegal DH2Q data len: %u\n",
							 rsp_context_data_len);
					error = EBADRPC;
					goto bad;
				}
				
				/* Get Timeout */
				error = md_get_uint32le(&md_context_shadow, &timeout);
				if (error) {
					goto bad;
				}
				
				/* Lock the dur handle */
                smbnode_dur_handle_lock(dur_hndl_leasep->dur_handlep, smb2_smb_parse_create_contexts);

				/* If we requested a specific timeout, see if we got it */
				if ((dur_hndl_leasep->dur_handlep->timeout != 0) &&
					(dur_hndl_leasep->dur_handlep->timeout != timeout)) {
					SMBERROR("Granted Dur Handle V2 timeout %d does not match requested %d \n",
							 timeout, dur_hndl_leasep->dur_handlep->timeout);
				}
				
                dur_hndl_leasep->dur_handlep->timeout = timeout;
				
				/* Get Flags */
				error = md_get_uint32le(&md_context_shadow, &flags);
				if (error) {
                    smbnode_dur_handle_unlock(dur_hndl_leasep->dur_handlep);
					goto bad;
				}

				/* Did we request a Durable Handle V2? */
				if (!(dur_hndl_leasep->dur_handlep->flags & SMB2_DURABLE_HANDLE_V2)) {
					/* We got a V2 response, but we sent a V1 request? */
					SMBERROR("Received Durable Handle V2 response for a Durable Handle V1 request \n");
                    dur_hndl_leasep->dur_handlep->flags |= SMB2_DURABLE_HANDLE_FAIL;
				}

				if (dur_hndl_leasep->dur_handlep->flags & SMB2_PERSISTENT_HANDLE_REQUEST) {
					/* We asked for a persistent handle */
					if (flags & SMB2_DHANDLE_FLAG_PERSISTENT) {
                        /* And we got a persistent handle as expected */
                        dur_hndl_leasep->dur_handlep->flags |= SMB2_PERSISTENT_HANDLE_GRANTED;
                        dur_hndl_leasep->dur_handlep->flags &= ~SMB2_PERSISTENT_HANDLE_REQUEST;
					}
					else {
						/* Can this ever happen??? */
						SMBERROR("Server failed to grant persistent handle??? \n");
                        
                        /*
                         * Weird, we got a durable V2 handle instead of a
                         * persistent handle.
                         */
                        dur_hndl_leasep->dur_handlep->flags |= SMB2_DURABLE_HANDLE_GRANTED;
                        dur_hndl_leasep->dur_handlep->flags &= ~SMB2_DURABLE_HANDLE_REQUEST;
					}
				}
				else {
					/* We asked for a durable V2 handle */
					if (flags & SMB2_DHANDLE_FLAG_PERSISTENT) {
						/* Can this ever happen??? */
						SMBERROR("Server unexpectedly granted persistent handle??? \n");
					}
					else {
                        /* And we got a durable V2 handle as expected */
                        dur_hndl_leasep->dur_handlep->flags |= SMB2_DURABLE_HANDLE_GRANTED;
                        dur_hndl_leasep->dur_handlep->flags &= ~SMB2_DURABLE_HANDLE_REQUEST;
					}
				}
				
				/* Unlock the dur handle */
                smbnode_dur_handle_unlock(dur_hndl_leasep->dur_handlep);

				break;

			case SMB2_CREATE_DURABLE_HANDLE_RECONNECT:    /* DHnC */
                /* The response to a DHnC seems to be a RqLs and DHnQ reply */
                break;

			case SMB2_CREATE_DURABLE_HANDLE_RECONNECT_V2:    /* DH2C */
				/* The response to a DH2C seems to be ONLY a RqLs reply */
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
                            error = md_get_uint64le(&md_context_shadow, &sessionp->session_server_caps);
                            if (error) {
                                goto bad;
                            }
                        }
                        if (sessionp->session_server_caps & kAAPL_UNIX_BASED) {
                            /* Server is unix based */
                            SESSION_CAPS(sessionp) |= SMB_CAP_UNIX;
                        }
                        if (sessionp->session_server_caps & kAAPL_SUPPORTS_OSX_COPYFILE) {
                            /* Server supports COPY_CHUNK IOCTL */
                            sessionp->session_misc_flags |= SMBV_HAS_COPYCHUNK;
                        }
                        if (sessionp->session_server_caps & kAAPL_SUPPORTS_HIFI) {
                            /*
                             * Server is running in HiFi mode so client needs
                             * to run in HiFi mode too.
                             */
                            if (sessionp->session_flags & SMBV_SFS_ACCESS) {
                                SMBWARNING("Server in High Fidelity mode \n");
                                sessionp->session_misc_flags |= SMBV_MNT_HIGH_FIDELITY;
                            }
                            else {
                                SMBWARNING("Server in HiFi mode, but NOT in super guest mode! \n");
                            }
                        }

                        if (server_bitmap & kAAPL_VOLUME_CAPS) {
                            /* Get volume capabilites */
                            error = md_get_uint64le(&md_context_shadow, &sessionp->session_volume_caps);
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

                            local_str = NULL;
                            smb2_smb_parse_create_str(share, &md_context_shadow,
                                                      0, &local_str, &local_str_allocsize);
                            if (error) {
                                goto bad;
                            }

                            lck_mtx_lock(&sessionp->session_model_info_lock);
                            bzero(sessionp->session_model_info,
                                  sizeof(sessionp->session_model_info));
                            lck_mtx_unlock(&sessionp->session_model_info_lock);

                            /*
                             * Set max string len of the model info at
                             * (SMB_MAXFNAMELEN * 2) - 1 to keep it reasonable 
                             * sized
                             */
                            if (local_str != NULL) {
                                if (strlen(local_str) >= (SMB_MAXFNAMELEN * 2)) {
                                    local_str[(SMB_MAXFNAMELEN * 2) - 1] = 0;
                                }

                                lck_mtx_lock(&sessionp->session_model_info_lock);
                                strlcpy(sessionp->session_model_info,
                                        local_str,
                                        sizeof(sessionp->session_model_info));
                                lck_mtx_unlock(&sessionp->session_model_info_lock);

                                SMB_FREE_DATA(local_str, local_str_allocsize);
                            }
                        }
                        
                        /*
                         * Its a OS X server or at least one pretending to be
                         * one.
                         */
                        SMBDEBUG("Found an OS X server\n");
                        sessionp->session_misc_flags |= SMBV_OSX_SERVER;
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
                                                  1, resolve_idp->ret_pathp, &resolve_idp->ret_pathp_allocsize);
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
                          uint32_t is_path, char **ret_str, size_t* ret_str_allocsize)
{
	int error;
    uint32_t str_len = 0;
	char *network_str = NULL;
	char *local_str = NULL;
    size_t name_size;
	size_t local_str_len;
	uint32_t max_len = SMB_MAXFNAMELEN * 2;
    
    /* Get string len */
    error = md_get_uint32le(mdp, &str_len);
    if (error) {
        goto bad;
    }

	/* 
	 * Default max_len is the max pathname component length in characters
	 * But if its a path, then the max_len is MAXPATHLEN.
	 * Multiple by 2 since its a UTF16 string.
	 */
	if (is_path) {
		max_len = MAXPATHLEN * 2;
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
        if (str_len > max_len) {
            SMBERROR("Illegal string len <%d> > <%d>\n",
                     str_len, max_len);
            error = EBADRPC;
            goto bad;
        }
    }
    
    SMB_MALLOC_DATA(network_str, str_len, Z_WAITOK);
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
        SMB_MALLOC_DATA(local_str, local_str_len, Z_WAITOK_ZERO);
        if(ret_str_allocsize) {
            *ret_str_allocsize = local_str_len;
        }
        
        if (local_str != NULL) {
            error = smb_convert_network_to_path(network_str, name_size,
                                                local_str, &local_str_len,
                                                '\\', UTF_SFM_CONVERSIONS,
                                                SMB_UNICODE_STRINGS(SS_TO_SESSION(share)));
            if (error) {
                SMBERROR("smb_convert_network_to_path failed %d\n", error);
                SMB_FREE_DATA(local_str, *ret_str_allocsize);
            }
        }
        else {
            /* Malloc failed, just error out */
            error = ENOMEM;
        }
    }
    else {
        local_str = smbfs_ntwrkname_tolocal((const char *)network_str, &name_size, &local_str_len,
                                            TRUE);
        if(ret_str_allocsize) {
            *ret_str_allocsize = local_str_len;
        }
        if (local_str == NULL) {
            SMBERROR("smbfs_ntwrkname_tolocal return NULL\n");
            error = EBADRPC;
        }
    }

    /* Return the local string */
    *ret_str = local_str;
bad:
    if (network_str != NULL) {
        /* Free network_name buffer */
        SMB_FREE_DATA(network_str, str_len);
    }
    
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
 	size_t nmlen, filename_allocsize = 0, ntwrkname_allocsize = 0, name_allocsize = 0;
	char *ntwrkname = NULL;
	char *filename = NULL;
    int utf8_char_cnt = 0;

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
    
    if (!(SS_TO_SESSION(all_infop->share)->session_misc_flags & SMBV_HAS_FILEIDS)) {
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
    
	/* Windows Server 2016 and Windows 10 now return file name len of 0 */
	if (size == 0) {
		SMBDEBUG("File name len of <0>\n");
		/* Leave with no error */
		goto bad;
	}

	/*
     * Make sure it is something in reason. Don't allocate it,
     * if it doesn't make sense. This is a possibly a full
     * path to the root of the share here.
     */
    if (size >= (SMB_MAXPATHLEN * 2)) {
        SMBERROR("Illegal file name len %d\n", size);
        error = EINVAL;
        goto bad;
    }

    nmlen = size;
    
    /* 
     * Since this is a full path, only check SMB_MAXFNAMELEN length
     * after we get just the filename. We just allocate what we need
     * need here. 
     */
    SMB_MALLOC_DATA(ntwrkname, nmlen, Z_WAITOK);
    ntwrkname_allocsize = nmlen;
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
    if (!SMB_UNICODE_STRINGS(SS_TO_SESSION(all_infop->share))) {
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
                                       &nmlen, &filename_allocsize,
                                       TRUE);
    if (filename == NULL) {
        /*
         * <105229518> Some third party servers return a badly formatted
         * network path (like ending with a "\"). Just ignore it and pretend
         * like we did not get a path. This is similar to how Windows servers
         * do not return a path at all for this request.
         */
        error = 0;
        SMBERROR("smbfs_ntwrkname_tolocal returned NULL. Bad path from server? \n");
        goto bad;
    }

    if (nmlen > SMB_MAXFNAMELEN) {
        /* Also check char count in case its multi byte UTF8 name */
        utf8_char_cnt = smb_get_utf8_char_count(filename, filename_allocsize);
        if (utf8_char_cnt > SMB_MAXFNAMELEN) {
            error = EINVAL;
            SMBERROR("Filename %s utf8_char_cnt = %d\n", filename, utf8_char_cnt);
            goto bad;
        }
    }
    
    *all_infop->namep = smb_strndup(filename, nmlen, &name_allocsize);
    if (all_infop->name_lenp) {
        /* Return the name length */       
        *all_infop->name_lenp = nmlen;
    }
    if (all_infop->name_allocsize) {
        *all_infop->name_allocsize = name_allocsize;
    }
    
    if (!(SS_TO_SESSION(all_infop->share)->session_misc_flags & SMBV_HAS_FILEIDS)) {
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
        SMB_FREE_DATA(ntwrkname, ntwrkname_allocsize);
    }
    
    if (filename != NULL) {
        /* Free the buffer that holds the name from the network */
        SMB_FREE_DATA(filename, filename_allocsize);
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
    size_t full_stream_name_len, full_stream_alloc_size;
    const char *stream_name;
    const char *fname;
	struct smbmount *smp = stream_infop->share->ss_mount;
    uint32_t is_data_stream = 0;
    uint32_t found_data_stream = 0;
    uint32_t stream_count = 0;
    int n_name_locked = 0;
    size_t network_name_allocsize = 0;
    
    translate_names = ((*stream_infop->stream_flagsp) & SMB_NO_TRANSLATE_NAMES) ? 0 : 1;
    *stream_infop->stream_flagsp = 0;
    
    /* Special case when no stream info found */
    if (buffer_len == 0) {
        /* if no output data, then no attrs were found */
        error = ENOATTR;
        
        /* This item has no named streams other than data stream */
        *stream_infop->stream_flagsp |= SMB_NO_SUBSTREAMS;
        goto done;
    }

    do {
        network_name = NULL;
        network_name_len = 0;
        full_stream_name = NULL;
        full_stream_name_len = 0;
        full_stream_alloc_size = 0;
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
        if (network_name_len > SS_TO_SESSION(stream_infop->share)->session_txmax) {
            error = EBADRPC;
            goto done;
        }
        
        /* Get Stream Name */
        network_name_allocsize = network_name_len;
        SMB_MALLOC_DATA(network_name, network_name_allocsize, Z_WAITOK);
        if (network_name == NULL) {
            error = ENOMEM;
            goto done;
        }
        
        error = md_get_mem(mdp, network_name, network_name_len, MB_MSYSTEM);
        if (error) {
            SMB_FREE_DATA(network_name, network_name_allocsize);
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
                                                   &full_stream_name_len, &full_stream_alloc_size,
                                                   SMB_UNICODE_STRINGS(SS_TO_SESSION(stream_infop->share)));
        if (network_name != NULL) {
            SMB_FREE_DATA(network_name, network_name_allocsize);
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
            lck_rw_lock_shared(&np->n_name_rwlock);
            n_name_locked = 1;
            fname = np->n_name;
        }
        else {
            /* readdirattr case - parent np and child namep */
            fname = stream_infop->namep;
        }
        
        stream_count += 1;
        if (smbfs_smb_undollardata(fname, full_stream_name,
                                   &full_stream_name_len, &is_data_stream)) {
            if (n_name_locked) {
                lck_rw_unlock_shared(&np->n_name_rwlock);
                n_name_locked = 0;
            }
            
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
                    /*
                     * Update resource fork size in meta data of vnode
                     * Dirs never have resource forks.
                     */
                    if (!(vnode_isdir(np->n_vnode))) {
                        lck_mtx_lock(&np->rfrkMetaLock);
                        np->rfrk_size = stream_size;
                        np->rfrk_alloc_size = alloc_size;
                        nanouptime(&ts);
                        np->rfrk_cache_timer = ts.tv_sec;
                        lck_mtx_unlock(&np->rfrkMetaLock);
                    }
                    else {
                        SMBERROR_LOCK(np, "Why is the server returning a resource fork on a dir <%s>??? \n",
                                      np->n_name);
                    }
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
        else {
            if (n_name_locked) {
                lck_rw_unlock_shared(&np->n_name_rwlock);
                n_name_locked = 0;
            }

            /*
             * smbfs_smb_undollardata() returns 0 for bad stream names,
             * data fork, or protected xattrs
             */
            if (is_data_stream == 1) {
                /* Found a data stream that we are ignoring */
                found_data_stream = 1;
            }
        }
        
skipentry:
        if (full_stream_name != NULL) {
            SMB_FREE_DATA(full_stream_name, full_stream_alloc_size);
        }
        
        /* 
         * next_entry_offset is offset to next entry.
         * Subtract out the number of bytes that we have already parsed and
         * that will tell us how many pad bytes are left to be consumed.
         */
		if (next_entry_offset > parsed) {
			next_entry_offset -= parsed;
			if (next_entry_offset > SS_TO_SESSION(stream_infop->share)->session_txmax) {
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
        SMB_FREE_DATA(full_stream_name, full_stream_alloc_size);
    }
   
done:
	if ((stream_count == 1) && (found_data_stream ==1)) {
        /* This item has no named streams other than data stream */
        *stream_infop->stream_flagsp |= SMB_NO_SUBSTREAMS;
    }
    
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
            np->finfo_cache_timer = ts.tv_sec;
        }
    }

    /* 
     * If we searched the entire list and did not find a resource stream, 
     * then reset the cache timer. 
     */
    if ((stream_type & kResourceFrk) != kResourceFrk) {
        /* set stream_flag indicating no Resource Fork */
        *stream_infop->stream_flagsp |= SMB_NO_RESOURCE_FORK;
        
        if ((stream_infop->namep == NULL) && !(vnode_isdir(np->n_vnode))) {
            /*
             * Negative cache the resource fork in the vnode
             * Dirs never have resource forks.
             */
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

int
smb2_smb_parse_flush(struct mdchain *mdp, struct smb2_flush_rq *flushp)
{
#pragma unused(flushp)
    int error;
    uint16_t reserved_uint16;
    uint16_t length;

    /*
     * Parse SMB 2/3 Flush Response
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
    return error;
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
        SMB_MALLOC_DATA(fs_attrs->file_system_namep, fs_attrs->file_system_name_len, Z_WAITOK_ZERO);
        
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
    uint16_t data_length, reserved, substitute_name_offset, substitute_name_len = 0;
    uint16_t print_name_offset, print_name_len;
	char *network_name = NULL;
	char *path = NULL;
	size_t path_len = 0, path_allocsize = 0;
    
    /*
     * Parse SMB 2/3 IOCTL Response for FSCTL_GET_REPARSE_POINT
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
        (substitute_name_len > SS_TO_SESSION(ioctlp->share)->session_txmax)) {
		error = ENOMEM;
		SMBSYMDEBUG("SubstituteNameLength too large or zero %d \n", SubstituteNameLength);
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
    SMB_MALLOC_DATA(network_name, substitute_name_len, Z_WAITOK_ZERO);
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
    path_allocsize = path_len;
    SMB_MALLOC_DATA(path, path_allocsize, Z_WAITOK_ZERO);
	if (path == NULL) {
		error = ENOMEM;
        goto bad;
    }

    error = smb_convert_network_to_path(network_name, substitute_name_len,
                                        path, &path_len,
                                        '\\', UTF_SFM_CONVERSIONS,
                                        SMB_UNICODE_STRINGS(SS_TO_SESSION(ioctlp->share)));

    if (!error) {
        /* Return the path */
		ioctlp->rcv_output_buffer = (uint8_t *) path;
		ioctlp->rcv_output_len = (uint32_t) path_len;
        ioctlp->rcv_output_allocsize = path_allocsize;
		path = NULL;    /* so we dont free it later */
	}
	
bad:
    if (network_name != NULL) {
        SMB_FREE_DATA(network_name, substitute_name_len);
    }
        
    if (path != NULL) {
        SMB_FREE_DATA(path, path_allocsize);
    }

	return error;
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
    struct smb2_secure_neg_info *neg_reply = NULL;
    
    /* 
     * Parse SMB 2/3 IOCTL Response 
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
        case FSCTL_PIPE_WAIT:
        case FSCTL_SRV_ENUMERATE_SNAPSHOTS:
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
             * Data offset is from the beginning of SMB 2/3 Header
             * Calculate how much further we have to go to get to it.
             */
            if (ret_output_offset > 0) {
                ret_output_offset -= SMB2_HDRLEN;
                /* already parsed 48 bytes worth of the response */
                ret_output_offset -= 48;

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
             * Data offset is from the beginning of SMB 2/3 Header
             * Calculate how much further we have to go to get to it.
             */
            // Advance to start of copychunk_response buffer
            if (ret_output_offset > 0) {
                ret_output_offset -= SMB2_HDRLEN;
                /* already parsed 48 bytes worth of the response */
                ret_output_offset -= 48;

                error = md_get_mem(mdp, NULL, ret_output_offset, MB_MSYSTEM);
                if (error) {
                    goto bad;
                }
            }
            
            /* Now fetch the resume key */
            error = smb2_smb_parse_ioctl_get_data(mdp, ioctlp);
            
            break;
            
        case FSCTL_SRV_COPYCHUNK:
            /* validate some return values */
            if (ioctlp->ret_input_len != 0) {
                SMBDEBUG("input_count is supposed to be 0: %u\n",
                         (uint32_t) ioctlp->ret_input_len);
            }
            
            if (ioctlp->ret_output_len != 0xC) {
                /* MUST be 0x0C */
                SMBDEBUG("output_count is supposed to be 12, got: %u\n", (uint32_t) ioctlp->ret_output_len);
            }
            
            /* Now fetch the copychunk_response */
            error = smb2_smb_parse_ioctl_get_data(mdp, ioctlp);
            
            if (error) {
                SMBDEBUG("smb2_smb_parse_copychunk_response failed, error: %d\n", error);
            }
            
            break;

        case FSCTL_VALIDATE_NEGOTIATE_INFO:
            neg_reply = (struct smb2_secure_neg_info *) ioctlp->rcv_output_buffer;

            if (ioctlp->ret_output_len != 0x18) {
                /* MUST be 0x18 */
                SMBDEBUG("output_count is supposed to be 24, got: %u\n", (uint32_t) ioctlp->ret_output_len);
            }
            
            if (ioctlp->rcv_output_len < 24) {
                SMBERROR("Validate Neg output buffer too small: %d\n",
                         ioctlp->rcv_output_len);
                error = EBADRPC;
                goto bad;
            }
            
            
            /* Get Capabilities */
            error = md_get_uint32le(mdp, &neg_reply->capabilities);
            if (error) {
                goto bad;
            }
            
            /* Get Server GUID */
            error = md_get_mem(mdp, (caddr_t)&neg_reply->guid, 16, MB_MSYSTEM);
            if (error) {
                goto bad;
            }
            
            /* Get Security Mode */
            error = md_get_uint16le(mdp, &neg_reply->security_mode);
            if (error) {
                goto bad;
            }

            /* Get Dialect */
            neg_reply->dialect_count = 1;
            error = md_get_uint16le(mdp, &neg_reply->dialects[0]);
            if (error) {
                goto bad;
            }
            
            break;
            
        case FSCTL_QUERY_NETWORK_INTERFACE_INFO:
            if (ioctlp->ret_output_len > ioctlp->rcv_output_len) {
                SMBERROR("FSCTL_QUERY_NETWORK_INTERFACE_INFO data too long. ret_output_len=%u\n",
                         ioctlp->ret_output_len);
                error = EINVAL;
                goto bad;
            }
            error = md_get_mem(mdp, (caddr_t)ioctlp->rcv_output_buffer, ioctlp->ret_output_len, MB_MSYSTEM);
            if (error) {
                SMBERROR("FSCTL_QUERY_NETWORK_INTERFACE_INFO error pulling data\n");
                goto bad;
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
	struct smb_session *sessionp = iod->iod_session;
	struct smb_rq rqp;
	struct mdchain *mdp = NULL;
	uint16_t length = 0;
	uint16_t server_epoch = 0;
	uint32_t flags;
    uint64_t lease_key_hi, lease_key_low;
    uint32_t curr_lease_state, new_lease_state;
	
	bzero(&rqp, sizeof(rqp));
    
    rqp.sr_session = sessionp;
	
	/* 
	 * Incoming Lease breaks are unsigned. Set the sr_command so that signing
	 * will not get checked when parsing the header
	 */
	rqp.sr_command = SMB2_OPLOCK_BREAK;
    
    smb_rq_getreply(&rqp, &mdp);
    md_initm(mdp, m);

    error = smb2_rq_parse_header(&rqp, &mdp, 0);
    if (error) {
        SMBERROR("smb2_rq_parse_header failed %d for lease break \n", error);
        goto bad;
    }

    /*
     * Parse SMB 2/3 Lease Break Response
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
    error = md_get_uint16le(mdp, &server_epoch);
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
	
	/* Enqueue this lease to be handle by the lease thread */
	smb_iod_lease_enqueue(iod, server_epoch, flags,
						  lease_key_hi, lease_key_low,
						  curr_lease_state, new_lease_state);

bad:
    md_done(mdp);

	return error;
}

static int
smb2_smb_parse_negotiate(struct smb_session *sessionp, struct smb_rq *rqp, int smb1_req)
{
    struct smb_sopt *sp = &sessionp->session_sopt;
    struct smbiod *iod = rqp->sr_iod;
    uint16_t sec_buf_offset;
    uint16_t sec_buf_len;
    uint8_t curr_time[8], boot_time[8];
    int error;
    struct mdchain *mdp;
    uint16_t neg_context_count;
    uint32_t neg_context_offset = 0, neg_bytes_parsed = 0;
    uint16_t temp16;
    uint32_t temp32;
    uint8_t temp_guid[16];

    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);

    /*
     * Parse SMB 2/3 Negotiate Response
     * We are already pointing to begining of Response data
     */

    /* Check structure size is 65 */
    error = md_get_uint16le(mdp, &temp16);
    if (error) {
        goto bad;
    }
    if (temp16 != 65) {
        SMBERROR("Bad struct size: %u\n", (uint32_t) temp16);
        error = EBADRPC;
        goto bad;
    }

    /* Get Security Mode */
    error = md_get_uint16le(mdp, &temp16);
    if (error) {
        goto bad;
    }

    if (iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL) {
        /* Alt Channel: validate */
        if (temp16 != sp->sv_security_mode) {
            SMBERROR("sv_security_mode mismatch (0x%x 0x%x).\n",
                     temp16, sp->sv_security_mode);
            error = EINVAL;
            goto bad;
        }
    }
    else {
        /* Main Channel: save value */
        sp->sv_security_mode = temp16;

        /* Save some signing related flags about what the server supports */
        if (sp->sv_security_mode & SMB2_NEGOTIATE_SIGNING_ENABLED)
            sessionp->session_flags |= SMBV_SIGNING;

        if (sp->sv_security_mode & (SMB2_NEGOTIATE_SIGNING_REQUIRED))
            sessionp->session_flags |= SMBV_SIGNING_REQUIRED;

        /* Turn on signing if either Server or client requires it */
        if ((sp->sv_security_mode & SMB2_NEGOTIATE_SIGNING_REQUIRED) ||
            (sessionp->session_misc_flags & SMBV_CLIENT_SIGNING_REQUIRED)) {
            sessionp->session_hflags2 |= SMB_FLAGS2_SECURITY_SIGNATURE;
        }

        /* always set User Level Access and supports challenge/resp */
        sessionp->session_flags |= SMBV_USER_SECURITY | SMBV_ENCRYPT_PASSWORD;
    }

    /*
     * Get Dialect.
     * if the server responded without an error then they must support one of
     * our dialects.
     */
    error = md_get_uint16le(mdp, &temp16);
    if (error) {
        goto bad;
    }
    
    if (iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL) {
        /* Alt Channel: validate */
        if (temp16 != sp->sv_dialect) {
            SMBERROR("sv_dialect mismatch (0x%x 0x%x).\n",
                     temp16, sp->sv_dialect);
            error = EINVAL;
            goto bad;
        }
    }
    else {
        /* Main Channel: save value */
        sp->sv_dialect = temp16;

        /* What dialect did we get? */
        switch (sp->sv_dialect) {
            case SMB2_DIALECT_0311:
                sessionp->session_flags |= SMBV_SMB2 | SMBV_SMB311;
                break;
            case SMB2_DIALECT_0302:
                sessionp->session_flags |= SMBV_SMB2 | SMBV_SMB302;
                break;
            case SMB2_DIALECT_0300:
                sessionp->session_flags |= SMBV_SMB2 | SMBV_SMB30;
                break;
            case SMB2_DIALECT_0210:
                sessionp->session_flags |= SMBV_SMB2 | SMBV_SMB21;
                break;
            case SMB2_DIALECT_02ff:
                /* not sure what dialect yet, still have to negotiate some more */
                sessionp->session_flags |= SMBV_SMB2;
                break;
            case SMB2_DIALECT_0202:
                sessionp->session_flags |= SMBV_SMB2 | SMBV_SMB2002;
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
             * We started with SMB1 Negotiate requst and got back a SMB 2/3
             * Negotiate response and that is the response we are parsing right now.
             *
             * For SMB 2.002 and 2.1, we need to continue parsing the SMB 2/3
             * Negotiate response as we are going directly to the Session Setup
             * exchange next.
             *
             * Any other dialect from the server, we will start over with a
             * SMB 2/3 Negotiate request from the client. The SMB 2/3 Negotiate
             * request will have information about the client that the server will
             * need. In this case, we dont have to finish parsing the SMB 1
             * response and can just skip out.
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
    }

    /* Get NegContextCount if SMB 3.1.1 */
    error = md_get_uint16le(mdp, &neg_context_count);
    if (error) {
        goto bad;
    }
    
    /*
     * Get Server GUID
     */
    error = md_get_mem(mdp, (caddr_t)&temp_guid, 16, MB_MSYSTEM);
    if (error) {
        goto bad;
    }
    
    if (iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL) {
        /* Alt Channel: validate */
        if (memcmp(temp_guid, sp->sv_guid, 16)) {
            SMBERROR("server_guid mismatch.\n");
            error = EINVAL;
            goto bad;
        }
    }
    else {
        /* Main Channel: save value */
        memcpy(sp->sv_guid, temp_guid, 16);
    }

    /* Get Server Capabilities */
    error = md_get_uint32le(mdp, &temp32);
    if (error) {
        goto bad;
    }

    if (iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL) {
        /* Alt Channel: validate */
        if (temp32 != sp->sv_saved_capabilities) {
            SMBERROR("sv_saved_capabilities mismatch (0x%x 0x%x).\n",
                     temp32, sp->sv_saved_capabilities);
            error = EINVAL;
            goto bad;
        }
    }
    else {
        /* Main Channel: save value */
        sp->sv_saved_capabilities = temp32;

        if (!(temp32 & SMB2_GLOBAL_CAP_LEASING)) {
            /* we don't support dir leasing if leasing is not supported, we should: rdar://103353045 */
            SMBWARNING("Server does not support leasing but supports dir leases? Disabling dir leases. \n");
            temp32 &= ~SMB2_GLOBAL_CAP_DIRECTORY_LEASING;
        }
        
        sp->sv_active_capabilities = temp32;

        /*
         * %%% To Do - too many places are looking at sv_active_capabilities
         * so for now, prefill it in.  Later we should use the
         * sv_active_capabilities for SMB 2/3.
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

        if (sp->sv_active_capabilities & SMB2_GLOBAL_CAP_DFS) {
            sp->sv_caps |= SMB_CAP_DFS;
        }

        if ((SMBV_SMB3_OR_LATER(sessionp)) &&
            (sp->sv_active_capabilities & SMB2_GLOBAL_CAP_MULTI_CHANNEL) &&
            (sessionp->neg_capabilities & SMB2_GLOBAL_CAP_MULTI_CHANNEL)) {
            sessionp->session_flags |= SMBV_MULTICHANNEL_ON;
        }
        else {
            sessionp->session_flags &= ~SMBV_MULTICHANNEL_ON;
        }
        
        if ((SMBV_SMB21_OR_LATER(sessionp)) &&
            (sp->sv_active_capabilities & SMB2_GLOBAL_CAP_LEASING) &&
            (sessionp->session_resp_wait_timeout < SMB_RESP_TIMO_W_LEASING)) {
                /* <93379514>:
                 * Lease breaks may take up to 35 sec to timeout.
                 * During that time the server does not reply to keepAlives (echos).
                 * We want to make sure we don't close the connection just because of a lease-break.
                 * See [MS-SMB2] section 3.3.2.5 Lease Break Acknowledgment Timer
                 */
                sessionp->session_resp_wait_timeout = SMB_RESP_TIMO_W_LEASING;
                SMBWARNING("session_resp_wait_timeout changed to %d\n", sessionp->session_resp_wait_timeout);
        }
    }

    /* Get Max Transact/Read/Write sizes */
    error = md_get_uint32le(mdp, &temp32);
    if (error) {
        goto bad;
    }

    if (iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL) {
        /* Alt Channel: validate */
        if (temp32 != sp->sv_maxtransact) {
            SMBERROR("sv_maxtransact mismatch (0x%x 0x%x).\n",
                     temp32, sp->sv_maxtransact);
            error = EINVAL;
            goto bad;
        }
    }
    else {
        /* Main Channel: save value */
        sp->sv_maxtransact = temp32;
    }

    error = md_get_uint32le(mdp, &temp32);
    if (error) {
        goto bad;
    }
    
    if (iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL) {
        /* Alt Channel: validate */
        if (temp32 != sp->sv_maxread) {
            SMBERROR("sv_maxread mismatch (0x%x 0x%x).\n",
                     temp32, sp->sv_maxread);
            error = EINVAL;
            goto bad;
        }
    }
    else {
        /* Main Channel: save value */
        sp->sv_maxread = temp32;
    }

    error = md_get_uint32le(mdp, &temp32);
    if (error) {
        goto bad;
    }
    
    if (iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL) {
        /* Alt Channel: validate */
        if (temp32 != sp->sv_maxwrite) {
            SMBERROR("sv_maxwrite mismatch (0x%x 0x%x).\n",
                     temp32, sp->sv_maxwrite);
            error = EINVAL;
            goto bad;
        }
    }
    else {
        /* Main Channel: save value */
        sp->sv_maxwrite = temp32;
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
    
    /* Get NegotiateContextOffset if SMB 3.1.1 */
    error = md_get_uint32le(mdp, &neg_context_offset);
    if (error) {
        goto bad;
    }
    
    neg_bytes_parsed = 64;

    /*
     * Security buffer offset is from the beginning of SMB 2/3 Header
     * Calculate how much further we have to go to get to it.
     */
    if (sec_buf_offset > 0) {
        sec_buf_offset -= SMB2_HDRLEN;
        sec_buf_offset -= 64;   /* already parse 64 bytes worth of the response */

        error = md_get_mem(mdp, NULL, sec_buf_offset, MB_MSYSTEM);
        if (error) {
            goto bad;
        }

        neg_bytes_parsed += sec_buf_offset;
    }
    
    /* Its ok if the sec_buf_len is 0 */
    if (sec_buf_len > 0) {
        SMB_MALLOC_DATA(iod->negotiate_token, sec_buf_len, Z_WAITOK_ZERO);
        if (iod->negotiate_token) {
            iod->negotiate_tokenlen = sec_buf_len;
            error = md_get_mem(mdp,
                               (void *)iod->negotiate_token,
                               iod->negotiate_tokenlen,
                               MB_MSYSTEM);
            if (error) {
                goto bad;
            }

            neg_bytes_parsed += sec_buf_len;
        }
        else {
            error = ENOMEM;
        }
    }
    
    if (sessionp->session_flags & SMBV_SMB311) {
        error = smb2_smb_parse_negotiate_contexts(iod,
                                                  mdp,
                                                  neg_bytes_parsed,
                                                  neg_context_offset,
                                                  neg_context_count);

    }

bad:
	return error;
}

static int
smb2_smb_parse_negotiate_contexts(struct smbiod *iod,
                                  struct mdchain *mdp,
                                  uint32_t neg_bytes_parsed,
                                  uint32_t neg_context_offset,
                                  uint16_t neg_context_count)
{
    struct smb_session *sessionp = NULL;
    int error = 0;
    uint16_t context_type, context_data_len;
    uint16_t hash_algorithm_cnt, salt_len, hash_algoritms;
    uint16_t cipher_cnt, cipher_algoritm;
    uint16_t compression_algorithm_cnt, compression_algorithm;
    uint32_t compression_flags;
    uint16_t signing_cnt, signing_algorithm;
    int i;
    int pad_bytes = 0;
    struct mdchain md_context_shadow;

    if (iod == NULL) {
        SMBERROR("iod is null? \n");
        return(EINVAL);
    }
    sessionp = iod->iod_session;

    if (neg_context_offset > 0) {
        neg_context_offset -= SMB2_HDRLEN;
        neg_context_offset -= neg_bytes_parsed;

        error = md_get_mem(mdp, NULL, neg_context_offset, MB_MSYSTEM);
        if (error) {
            goto bad;
        }
    }

    do {
        /*
         * Offsets are from the beginning of the context, so save a
         * copy of the beginning of the context.
         */
        md_shadow_copy(mdp, &md_context_shadow);

        /*
         * Parse the Context header
         */

        /* Get ContextType */
        error = md_get_uint16le(&md_context_shadow, &context_type);
        if (error) {
            goto bad;
        }

        /* Get DataLength */
        error = md_get_uint16le(&md_context_shadow, &context_data_len);
        if (error) {
            goto bad;
        }

        /* Get reserve bytes and ignore them */
        error = md_get_uint32le(&md_context_shadow, NULL);
        if (error) {
            goto bad;
        }

        /*
         * Notes
         * 1. SMB2_ENCRYPTION_CAPABILITIES must return one cipher
         * 2. SMB2_COMPRESSION_CAPABILITIES. We do not support compression, so
         *    can ignore this reply
         * 2. SMB2_NETNAME_NEGOTIATE_CONTEXT_ID has no response context
         * 3. SMB2_RDMA_TRANSFORM_CAPABILITIES we never use, so can ignore
         */
        switch (context_type) {
            case SMB2_PREAUTH_INTEGRITY_CAPABILITIES:
                /*
                 * At this time there is no other pre auth integrity hash
                 * algorithm defined and the salt value is automatically
                 * included in the hash, so nothing for us to do with this
                 * response.
                 */

                /* Get HashAlgorithmCount */
                error = md_get_uint16le(&md_context_shadow, &hash_algorithm_cnt);
                if (error) {
                    goto bad;
                }

                /* Get SaltLength */
                error = md_get_uint16le(&md_context_shadow, &salt_len);
                if (error) {
                    goto bad;
                }

                /* Get HashAlgorithms */
                for (i = 0; i < hash_algorithm_cnt; i++) {
                    error = md_get_uint16le(&md_context_shadow, &hash_algoritms);
                    if (error) {
                        goto bad;
                    }
                }

                /* Get Salt and ignore them */
                error = md_get_mem(&md_context_shadow, NULL, salt_len, MB_MSYSTEM);
                if (error) {
                    goto bad;
                }

               break;

            case SMB2_ENCRYPTION_CAPABILITIES:
                /* Get CipherCount */
                error = md_get_uint16le(&md_context_shadow, &cipher_cnt);
                if (error) {
                    goto bad;
                }
                if (cipher_cnt != 1) {
                    SMBERROR("CipherCount is supposed to be 1 (%d) \n",
                             cipher_cnt);
                    error = EBADRPC;
                    goto bad;
                }

               /* Get Cipher to use for encryption */
                error = md_get_uint16le(&md_context_shadow, &cipher_algoritm);
                if (error) {
                    goto bad;
                }

                if (iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL) {
                    /* Alt Channel: validate */
                    if (cipher_algoritm != sessionp->session_smb3_encrypt_ciper) {
                        SMBERROR("cipher algorithm mismatch (0x%x 0x%x).\n",
                                 cipher_algoritm, sessionp->session_smb3_encrypt_ciper);
                        error = EINVAL;
                        goto bad;
                    }
                }
                else {
                    /* Main Channel: save value */
                    sessionp->session_smb3_encrypt_ciper = cipher_algoritm;
                }

                break;

            case SMB2_COMPRESSION_CAPABILITIES:
                /* Get CompressionAlgorithmCount */
                error = md_get_uint16le(&md_context_shadow, &compression_algorithm_cnt);
                if (error) {
                    goto bad;
                }

                /* Get Padding and ignore it */
                error = md_get_uint16le(&md_context_shadow, NULL);
                if (error) {
                    goto bad;
                }

                /* Get Flags */
                error = md_get_uint32le(&md_context_shadow, &compression_flags);
                if (error) {
                    goto bad;
                }

                /*
                 * If client supports chained compression, then check to see if
                 * server supports chained compression.
                 */
                if (!(sessionp->session_misc_flags & SMBV_COMPRESSION_CHAINING_OFF)) {
                    /* Is chained compression supported by the server? */
                    if (compression_flags == SMB2_COMPRESSION_CAPABILITIES_FLAG_NONE) {
                        /*
                         * Server does not support chained compression so
                         * disable it
                         */
                        sessionp->session_misc_flags |= SMBV_COMPRESSION_CHAINING_OFF;
                    }
                }
                
                /* Get CompressionAlgorithms */
                for (i = 0; i < compression_algorithm_cnt; i++) {
                    error = md_get_uint16le(&md_context_shadow, &compression_algorithm);
                    if (error) {
                        goto bad;
                    }
                    
                    switch(compression_algorithm) {
                        case SMB2_COMPRESSION_LZ77_HUFFMAN:
                            sessionp->server_compression_algorithms_map |= SMB2_COMPRESSION_LZ77_HUFFMAN_ENABLED;
                            break;
                        case SMB2_COMPRESSION_LZ77:
                            sessionp->server_compression_algorithms_map |= SMB2_COMPRESSION_LZ77_ENABLED;
                            break;
                        case SMB2_COMPRESSION_LZNT1:
                            sessionp->server_compression_algorithms_map |= SMB2_COMPRESSION_LZNT1_ENABLED;
                            break;
                        case SMB2_COMPRESSION_PATTERN_V1:
                            sessionp->server_compression_algorithms_map |= SMB2_COMPRESSION_PATTERN_V1_ENABLED;
                            break;
                        default:
                            SMBERROR("Unknown compression algorithm: 0x%x\n", compression_algorithm);
                            break;
                    }
                }
                break;
                
            case SMB2_SIGNING_CAPABILITIES:
                /* Get SigningAlgorithmCount */
                error = md_get_uint16le(&md_context_shadow, &signing_cnt);
                if (error) {
                    goto bad;
                }
                if (signing_cnt != 1) {
                    SMBERROR("Server must set SigningCnt to 1 (%d) \n",
                             signing_cnt);
                    error = EBADRPC;
                    goto bad;
                }

                /* Get SigningAlgorithm */
                error = md_get_uint16le(&md_context_shadow, &signing_algorithm);
                if (error) {
                    goto bad;
                }
                
                if ((signing_algorithm != SMB2_SIGNING_AES_CMAC) &&
                    (signing_algorithm != SMB2_SIGNING_AES_GMAC)) {
                    SMBERROR("Server provided an invalid signing algorithm (%u)",
                             signing_algorithm);
                    error = EINVAL;
                    goto bad;
                }
                sessionp->session_smb3_signing_algorithm = signing_algorithm;
                break;

            case SMB2_TRANSPORT_CAPABILITIES:
               /* Ignore this for now */
                break;

            case SMB2_RDMA_TRANSFORM_CAPABILITIES:
                /* Ignore this for now */
                break;

            default:
                /* Unknown contexts are just ignored */
                SMBERROR("Unknown context type: 0x%x\n", context_type);
                break;
        } /* end of switch */

        neg_context_count -= 1;

        /* Move to the next context in the main mdp chain */
        if (neg_context_count > 0) {
            /* Add in the ContextType, DataLength, Reserved bytes */
            context_data_len += 8;

            /* Align to 8 byte boundary */
            if ((context_data_len % 8) != 0) {
                /* Contexts MUST start on next 8 byte boundary! */
                pad_bytes = 8 - (context_data_len % 8);
                context_data_len += pad_bytes;
            }

            error = md_get_mem(mdp, NULL, context_data_len, MB_MSYSTEM);
            if (error) {
                goto bad;
            }
        }
    } while (neg_context_count != 0);

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
    uint32_t read_size = 0;
    
    /* NOTE: queryp must still have the data from building the request */
    
    /* 
     * Parse SMB 2/3 Query Directory 
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
     * Output buffer offset is from the beginning of SMB 2/3 Header
     * Calculate how much further we have to go to get to it.
     */
    if (output_buf_offset > 0) {
        output_buf_offset -= SMB2_HDRLEN;
        output_buf_offset -= 8;   /* already parsed 8 bytes worth of the response */

        error = md_get_mem(mdp, NULL, output_buf_offset, MB_MSYSTEM);
        if (error) {
            SMBERROR("failed getting output buffer offset bytes\n");
            goto bad;
        }
    }
    
    if ((queryp->rcv_output_uio != NULL) &&
        (queryp->output_buffer_len > 0)) {
        /* 
         * If have a rcv_output_uio, then must be from user space and thus a 
         * test tool. Just copy out the results to their buffer and let them 
         * parse it.
         */
        read_size = min(queryp->output_buffer_len, queryp->ret_buffer_len);
        error = md_get_uio(mdp, queryp->rcv_output_uio, read_size);
        
        if (!error) {
            queryp->output_buffer_len = read_size;
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
    struct smb_session *sessionp = SS_TO_SESSION(share);
	struct smbmount *smp = share->ss_mount;
    struct finder_file_info file_finfo;
    struct finder_folder_info folder_finfo;
    uint16_t unix_mode = 0;
    uint16_t flags = 0;
    struct smbfs_fctx_query_t *current_query = NULL;
    
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
    if (UNIX_SERVER(sessionp) || (fap->fa_attr & SMB_EFA_DIRECTORY)) {
        fap->fa_valid_mask |= FA_VTYPE_VALID;
    }
    fap->fa_vtype = (fap->fa_attr & SMB_EFA_DIRECTORY) ? VDIR : VREG;
    
    /* Set default uid/gid values */
    fap->fa_uid = KAUTH_UID_NONE;
    fap->fa_gid = KAUTH_GID_NONE;

    /* Get File Name Length */
    error = md_get_uint32le(mdp, network_name_len);
    if (error) {
        SMBERROR("failed getting file name len\n");
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
        /*
         * If kAAPL_SUPPORTS_READ_DIR_ATTR_V2 is not supported, then we need to
         * read in Short Name Length (uint8_t) and Reserved (uint8_t) and both
         * are then ignored. Just read in a uint16_t instead and ignore it.
         *
         * If kAAPL_SUPPORTS_READ_DIR_ATTR_V2 is supported, then the Short Name
         * Length and Reserved are replaced with flags (uint16_t).
         */
        error = md_get_uint16(mdp, &flags);
        if (error) {
            SMBERROR("failed getting flags \n");
            goto bad;
        }
        fxsz += 2;

        if ((sessionp->session_misc_flags & SMBV_OSX_SERVER) &&
            ((sessionp->session_server_caps & kAAPL_SUPPORTS_READ_DIR_ATTR) ||
             (sessionp->session_server_caps & kAAPL_SUPPORTS_READ_DIR_ATTR_V2))) {
            /* OS X Server that supports readdir_attr shortcuts */

            /* V2 version supports a flags field */
            fap->fa_fstatus &= ~kNO_SUBSTREAMS;
            fap->fa_valid_mask &= ~FA_FSTATUS_VALID;
            
            if (sessionp->session_server_caps & kAAPL_SUPPORTS_READ_DIR_ATTR_V2) {
                if (flags & kAAPL_READ_DIR_NO_XATTR) {
                    /* No xattrs on item */
                    fap->fa_fstatus |= kNO_SUBSTREAMS;
                }
                else {
                    /* Must have xattrs on item */
                    fap->fa_fstatus &= ~kNO_SUBSTREAMS;
                }
                
                fap->fa_valid_mask |= FA_FSTATUS_VALID;
            }

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

            if ((sessionp->session_server_caps & kAAPL_UNIX_BASED) &&
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
        current_query = STAILQ_FIRST(&ctx->f_queries);
        /* save where we last left off searching */
        ctx->f_resume_file_index = file_index;
        
        next = ctx->f_eofs + recsz;
        ctx->f_eofs = next;
        current_query->output_buf_len -= recsz;
        ctx->f_queries_total_memory -= recsz;
        if (last_entry == 1) {
            /* only padding bytes left so ignore them */
            current_query->output_buf_len = 0;
            ctx->f_queries_total_memory -= current_query->output_buf_len;
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
     * Parse SMB 2/3 Query Info Response 
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
     * Output buffer offset is from the beginning of SMB 2/3 Header
     * Calculate how much further we have to go to get to it.
     */
    if (output_buffer_offset > 0) {
        output_buffer_offset -= SMB2_HDRLEN;
        output_buffer_offset -= 8;   /* already parsed 8 bytes worth of the response */

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
     * Parse SMB 2/3 Read Response
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
     * Data offset is from the beginning of SMB 2/3 Header
     * Calculate how much further we have to go to get to it.
     */
    if (data_offset > 0) {
        data_offset -= SMB2_HDRLEN;
        data_offset -= 16;   /* already parsed 16 bytes worth of the response */

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
        if (error) {
            SMBERROR("md_get_uio ret %d \n", error);
        }

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
     * Parse SMB 2/3 Set Info Response 
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
     * Parse SMB 2/3 Get Security Response
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
        SMB_MALLOC_DATA(*resp, queryp->ret_buffer_len, Z_WAITOK);
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
    size_t filename_allocsize = 0;
    
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
     * Parse SMB 2/3 Change Notify Response
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
     * Output buffer offset is from the beginning of SMB 2/3 Header
     * Calculate how much further we have to go to get to it.
     */
    if (output_buffer_offset > 0) {
        output_buffer_offset -= SMB2_HDRLEN;
        output_buffer_offset -= 8;   /* already parsed 8 bytes worth of the response */

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
    error = smb2_smb_parse_create_str(rqp->sr_share, mdp, 0, &filename, &filename_allocsize);
    
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
        SMB_FREE_DATA(filename, filename_allocsize);
    }
    return error;
}

int
smb2_smb_parse_write_one(struct mdchain *mdp,
                         user_ssize_t *rresid,
                         struct smb2_rw_rq *writep)
{
	int error;
	uint16_t length;
    uint16_t reserved1;
    uint32_t reserved2;
    
    /*
     * Parse SMB 2/3 Write Response
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
smb2_smb_query_dir(struct smb_share *share, struct smb2_query_dir_rq *queryp,
                   struct smb_rq **compound_rqp,
                   struct smbiod *iod,
                   vfs_context_t context)
{
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

    if (iod) {
        error = smb_iod_ref(iod, __FUNCTION__);
    } else {
        error = smb_iod_get_any_iod(SS_TO_SESSION(share), &iod, __FUNCTION__);
    }
    if (error) {
        return error;
    }

    error = smb2_rq_alloc(SSTOCP(share), SMB2_QUERY_DIRECTORY,
                          &queryp->output_buffer_len, 
                          context, iod, &rqp);
    if (error) {
        smb_iod_rel(iod, NULL, __FUNCTION__);
        return error;
    }

    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

    queryp->ret_rqp = rqp;
    
    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB 2/3 Query_Directory Request
     */
    mb_put_uint16le(mbp, 33);                           /* Struct size */
    mb_put_uint8(mbp, queryp->file_info_class);         /* FileInformationClass */
    mb_put_uint8(mbp, queryp->flags);                   /* Flags */
    mb_put_uint32le(mbp, queryp->file_index);           /* FileIndex */

    /* map fid to SMB 2/3 fid */
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
         * SMB 2/3 searches do not want any paths (ie no '\') in the search 
         * pattern. The dir to be searched should already be open.
         * If we have a namep, then use that as the search pattern.
         * If namep is NULL, then use dnp->n_name as the search pattern.
         */
        if (queryp->namep == NULL) {
            /* 
             * no namep, thus dnp's parent was opened so look for dnp->n_name 
             * Note: if you pass in dnp to smb2fs_fullpath it will auto add
             * the full path to dnp which could include '\' and we dont want
             * that.
             */
            if (queryp->dnp == NULL) {
                SMBERROR("queryp->dnp is NULL\n");
                error = EINVAL;
                goto bad;
            }
            
            lck_rw_lock_shared(&queryp->dnp->n_name_rwlock);
            error = smb2fs_fullpath(mbp, NULL,
                                    queryp->dnp->n_name, queryp->dnp->n_nmlen,
                                    NULL, 0,
                                    queryp->name_flags, sep_char);
            
            lck_rw_unlock_shared(&queryp->dnp->n_name_rwlock);
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
    
    if (queryp->mc_flags & SMB2_MC_REPLAY_FLAG) {
        /* This message is replayed - sent after a channel has been disconnected */
        *rqp->sr_flagsp |= SMB2_FLAGS_REPLAY_OPERATIONS;
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
            SMB_LOG_MC("resending messageid %llu cmd %u.\n", rqp->sr_messageid, rqp->sr_command);
            if (rqp->sr_flags & SMBR_ALT_CH_DISCON) {
                /* An alternate channel got disconnected. Resend with the REPLAY flag set */
                queryp->mc_flags |= SMB2_MC_REPLAY_FLAG;
            }

            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            iod = NULL;
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
smb2_smb_query_info(struct smb_share *share, struct smb2_query_info_rq *queryp,
                    struct smb_rq **compound_rqp, struct smbiod *iod, vfs_context_t context)
{
	struct smb_rq *rqp = NULL;
	struct mbchain *mbp;
	struct mdchain *mdp;
    uint16_t input_buffer_offset;
	int error;
    SMB2FID smb2_fid;
    uint8_t cmd = SMB2_QUERY_INFO;

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
    
    if (queryp->flags & SMB2_CMD_NO_BLOCK) {
        /* Dont wait for credits, but return an error instead */
        queryp->flags &= ~SMB2_CMD_NO_BLOCK;
        cmd |= SMB2_NO_BLOCK;
    }

resend:
    if (iod) {
        error = smb_iod_ref(iod, __FUNCTION__);
    } else {
        error = smb_iod_get_any_iod(SS_TO_SESSION(share), &iod, __FUNCTION__);
    }
    if (error) {
        return error;
    }

    /* Allocate request and header for a Query Info */
    error = smb2_rq_alloc(SSTOCP(share), cmd,
                          &queryp->output_buffer_len, 
                          context, iod, &rqp);
    if (error) {
        smb_iod_rel(iod, NULL, __FUNCTION__);
        return error;
    }
    
    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB 2/3 Query_Info Request
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

    /* map fid to SMB 2/3 fid */
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

    if (queryp->mc_flags & SMB2_MC_REPLAY_FLAG) {
        /* This message is replayed - sent after a channel has been disconnected */
        *rqp->sr_flagsp |= SMB2_FLAGS_REPLAY_OPERATIONS;
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
            SMB_LOG_MC("resending messageid %llu cmd %u.\n", rqp->sr_messageid, rqp->sr_command);
            if (rqp->sr_flags & SMBR_ALT_CH_DISCON) {
                /* An alternate channel got disconnected. Resend with the REPLAY flag set */
                queryp->mc_flags |= SMB2_MC_REPLAY_FLAG;
            }

            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            iod = NULL;
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
smb2_smb_read(struct smb_share *share, struct smb2_rw_rq *readp,
              uint32_t allow_compression, vfs_context_t context)
{
	user_ssize_t total_size, len, resid = 0;
	int error = 0;
    /*
     * Reads do not ever change the allow_compression flag so just pass
     * the address of a local var
     */
    uint32_t tmp_allow_compression = allow_compression;
    
    SMB_LOG_KTRACE(SMB_DBG_SMB_READ | DBG_FUNC_START,
                   uio_offset(readp->auio),
                   uio_resid(readp->auio), 0, 0, 0);

    /* assume we can read it all in one request */
	total_size = uio_resid(readp->auio);
    
	while (total_size > 0) {
        /* read "len" amount of data */
		len = total_size;
        
		error = smb2_smb_read_write_async(share, readp, &len, &resid, 1,
                                          &tmp_allow_compression, context);
        if (error) {
            SMBERROR("smb2_smb_read_write_async failed with an error %d\n", error);
            break;
        }

        /* subtract actual amount of data read "resid" from the total left */
		total_size -= resid;
        
		/* Nothing else to read we are done */
		if (!resid) {
			SMB_LOG_IO("Server zero bytes read\n");
			break;
		}
        
		if (resid < len) {
            /* assume nothing more to read, so we are done */
            SMBERROR("Unexpected partial read! resid %lld, len %lld\n", resid, len);
            break;
        }
	}
    
    if (error == ENODATA) {
        /* ntstatus holds the EOF error */
        error = 0;
    }
    
    SMB_LOG_KTRACE(SMB_DBG_SMB_READ | DBG_FUNC_END, error, 0, 0, 0, 0);
	return error;
}

/*
 * *len is amount of data requested (updated with actual size attempted)
 * *rresid is actual amount of data read
 */
int
smb2_smb_read_one(struct smb_share *share, struct smb2_rw_rq *readp,
                  user_ssize_t *len, user_ssize_t *rresid,
                  struct smb_rq **compound_rqp, struct smbiod *iod,
                  uint32_t allow_compression, vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
    uint32_t min_length = 1;
    SMB2FID smb2_fid;
    uint32_t len32;
    uint32_t remaining = (uint32_t ) *len;
    uint8_t cmd = SMB2_READ;
    user_ssize_t tmp_resid = 0;
    int do_short_read = 0;
    user_ssize_t short_read_len = 0;
    struct smb_session *sessionp = SS_TO_SESSION(share);

    len32 = (uint32_t) MIN(sessionp->session_rxmax, *len);

    if (readp->flags & SMB2_CMD_NO_BLOCK) {
        /* Dont wait for credits, but return an error instead */
        readp->flags &= ~SMB2_CMD_NO_BLOCK;
        cmd |= SMB2_NO_BLOCK;
    }
    
resend:
    if (iod) {
        error = smb_iod_ref(iod, __FUNCTION__);
    } else {
        // Multichannel: use any iod to send
        error = smb_iod_get_any_iod(sessionp, &iod, __FUNCTION__);
    }
    if (error) {
        return error;
    }

    /*
     * Allocate request and header for a Read
     * Available credits may reduce the read size
     */
    error = smb2_rq_alloc(SSTOCP(share), cmd, &len32, context, iod, &rqp);
    if (error) {
        smb_iod_rel(iod, NULL, __FUNCTION__);
        return error;
    }

    if (compound_rqp == NULL) {
        /* ktrace only if doing actual read and not building part of a cmpd */
        SMB_LOG_KTRACE(SMB_DBG_SMB_READ_ONE | DBG_FUNC_START, smb_hideaddr(rqp), uio_offset(readp->auio), len32, 0, 0);
    }

    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

    if (do_short_read == 0) {
        /* Just doing a normal read */
        *len = len32;

        if (remaining > len32) {
            remaining -= len32;
        }
        else {
            remaining = 0;
        }
    }
    else {
        /*
         * <63197657> The less common case where we had a short
         * read. Request just the missing read data
         */
        if (len32 != short_read_len) {
            /*
             * The read len for the missing bytes got reduced due to not enough
             * credits? Can this ever happen? If so, give up
             */
            SMBERROR("Not enough credits for short read %d != %lld \n",
                     len32, short_read_len);
            error = EIO;
            goto bad;
        }
        remaining = 0;
    }

    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB 2/3 Read Request
     */
    mb_put_uint16le(mbp, 49);                       /* Struct size */
    mb_put_uint8(mbp, 0);                           /* Padding */
    
    /* Should we request a compressed Read reply? */
    if (SMBV_SMB3_OR_LATER(sessionp) &&
        (sessionp->server_compression_algorithms_map != 0) &&
        (allow_compression == 1) &&
        (len32 > sessionp->compression_io_threshold)) {
        mb_put_uint8(mbp, SMB2_READFLAG_REQUEST_COMPRESSED); /* Flags */
    }
    else {
        mb_put_uint8(mbp, 0);                       /* Flags */
    }

    mb_put_uint32le(mbp, (uint32_t) len32);         /* Length of read */
	mb_put_uint64le(mbp, uio_offset(readp->auio));  /* Offset */

    /* map fid to SMB 2/3 fid */
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

    if (readp->mc_flags & SMB2_MC_REPLAY_FLAG) {
        *rqp->sr_flagsp |= SMB2_FLAGS_REPLAY_OPERATIONS;
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
    readp->ret_ntstatus = rqp->sr_ntstatus;
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            SMB_LOG_MC("resending messageid %llu cmd %u.\n", rqp->sr_messageid, rqp->sr_command);
            if (rqp->sr_flags & SMBR_ALT_CH_DISCON) {
                /* An alternate channel got disconnected. Resend with the REPLAY flag set */
                readp->mc_flags |= SMB2_MC_REPLAY_FLAG;
            }

            /* Rebuild and try sending again */
            remaining += len32;
            
            smb_rq_done(rqp);
            rqp = NULL;
            iod = NULL;
            
            SMB_LOG_KTRACE(SMB_DBG_SMB_READ_ONE | DBG_FUNC_END, error, 0xabc001, 0, 0, 0);
            goto resend;
        }
        
        goto bad;
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);

    tmp_resid = 0;
    error = smb2_smb_parse_read_one(mdp, &tmp_resid, readp);
    if (error) {
        goto bad;
    }

    /* Add up amount of data that we have read so far */
    *rresid += tmp_resid;

    if (tmp_resid != len32) {
        SMBWARNING("IO Mismatched. Requested %u but got %lld\n",
                   len32, tmp_resid);
        /*
         * <72062477> skip the short read if its a named pipe being used.
         * Named pipes are always on the share IPC$.
         */
        if ((tmp_resid < len32) &&
            (strcmp(share->ss_name, "IPC$") != 0)) {
            /*
             * <63197657> Need to reissue a read for the missing
             * bytes.
             *
             * len32 is data len requested
             * tmp_resid is the data len actually received
             */
            do_short_read = 1;
            short_read_len = len32 - tmp_resid;
            len32 = (uint32_t) short_read_len; /* reset for rebuilding rqp */

            SMBWARNING("Short read requesting len %lld offset %lld \n",
                       short_read_len,
                       uio_offset(readp->auio));

            smb_rq_done(rqp);
            rqp = NULL;
            iod = NULL;

            SMB_LOG_KTRACE(SMB_DBG_SMB_READ_ONE | DBG_FUNC_END, error, 0xabc002, 0, 0, 0);
            goto resend;
       }
    }

bad:    
	smb_rq_done(rqp);
    
    SMB_LOG_KTRACE(SMB_DBG_SMB_READ_ONE | DBG_FUNC_END, error, 0, 0, 0, 0);
    return error;
}

static int
smb2_smb_read_uio(struct smb_share *share, SMBFID fid, uio_t uio,
                  uint32_t allow_compression, vfs_context_t context)
{
	int error;
 	struct smb2_rw_rq *readp = NULL;
    
    SMB_MALLOC_TYPE(readp, struct smb2_rw_rq, Z_WAITOK_ZERO);
    if (readp == NULL) {
        SMBERROR("SMB_MALLOC_TYPE failed\n");
        error = ENOMEM;
        return error;
    }
    
    readp->flags = 0;
    readp->remaining = 0;
    readp->write_flags = 0;
    readp->fid = fid;
    readp->auio = uio;
    readp->mc_flags = 0;
    
    error = smb2_smb_read(share, readp, allow_compression, context);
    
    if (readp != NULL) {
        SMB_FREE_TYPE(struct smb2_rw_rq, readp);
    }
    
	return error;
}

static int
smb2_smb_read_write_async(struct smb_share *share,
                          struct smb2_rw_rq *in_read_writep,
                          user_ssize_t *len,
                          user_ssize_t *rresid,
                          uint32_t do_read, uint32_t *allow_compressionp,
                          vfs_context_t context)
{
    int error = 0;
    struct mdchain *mdp;
    unsigned int i, j;
    struct smb_rw_arg *rw_pb = NULL;
    int done = 0;
    int reconnect = 0, recheck = 0;
    struct smb2_rw_rq tmp_read_write;
    user_ssize_t saved_len, saved_rresid;
    struct smb_session *sessionp = NULL;
    uint32_t quantumSize = 0;
    uint32_t quantumNbr = 0;
    uint32_t single_thread = 1;
    struct timeval start_time = {0}, current_time = {0}, elapsed_time = {0};
    int do_short_read = 0;
    user_ssize_t short_read_len = 0;
    struct smb2_rw_rq short_read_rq = {0};
    uint32_t max_io_size = 0, compress_write_fails = 0;

    SMB_LOG_KTRACE(SMB_DBG_SMB_RW_ASYNC | DBG_FUNC_START, uio_offset(in_read_writep->auio), uio_resid(in_read_writep->auio), do_read, *len, 0);

	/*
     * This function does multiple Async Reads/Writes
     */
    
	if (share == NULL) {
		SMBERROR("share is null? \n");
		error = ENOMEM;
		goto done;
	}
	
	sessionp = SS_TO_SESSION(share);
	if (sessionp == NULL) {
		SMBERROR("sessionp is null? \n");
		error = ENOMEM;
		goto done;
	}

    /* Save values in case reconnect occurs */
    saved_len = *len;
    saved_rresid = *rresid;
    
    /* Will the IO fit in a single IO request? */
    if (do_read) {
        max_io_size = smb2_session_max_io_size(SS_TO_SESSION(share), SMB2_READ);

        if ((*len <= max_io_size) ||
            (in_read_writep->flags & SMB2_SYNC_IO)) {
            /* Only need single read */
            error = smb2_smb_read_one(share, in_read_writep,
                                      len, rresid,
                                      NULL, NULL,
                                      *allow_compressionp, context);
            goto done;
        }
    }
    else {
        max_io_size = smb2_session_max_io_size(SS_TO_SESSION(share), SMB2_WRITE);
        if ((*len <= max_io_size) ||
            (in_read_writep->flags & SMB2_SYNC_IO)) {
            /* Only need single write */
            error = smb2_smb_write_one(share, in_read_writep, len, rresid,
                                       NULL, NULL,
                                       allow_compressionp, context);
            goto done;
        }
    }

    /*
     * If signing or sealing being used, then use multi thread model. The
     * read/write helper threads will do the signing or sealing work in
     * parallel for all outgoing requests and thus should be faster. Incoming
     * replies get their signing or sealing verified by each iod's read thread
     * for parallelism when we have more than one channel.
     *
     * One problem with the multi thread model is that the bytes/sec
     * calculations for which quantum size/count to use is skewed by the
     * various thread contention and that can cause a slower quantum size/count
     * to be selected.  
     
     * %%% TO DO %%% Try adding a time stamp sr_time_replied when reply arrives
     * for each rqp, then get elapsed time per rqp with
     * elapsedTime += (sr_time_replied - rqp->sr_time_sent)
     *
     * If not signing or sealing, use single thread model since its performance
     * is equivalent and its bytes/sec calculations are much more accurate.
     */
    if (sessionp->session_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE) {
        single_thread = 0;
    }
    else {
        if (SMBV_SMB3_OR_LATER(sessionp)) {
            /* Check if session is encrypted */
            if (sessionp->session_sopt.sv_sessflags & SMB2_SESSION_FLAG_ENCRYPT_DATA) {
                /* Session sealing being used */
                single_thread = 0;
            }
            else {
                if (share->ss_share_flags & SMB2_SHAREFLAG_ENCRYPT_DATA) {
                    /* Share is sealed */
                    single_thread = 0;
                }
            }
        }
    }

    /* This is for dev testing to force single or multi thread mode */
    switch(sessionp->rw_gb_threshold) {
        case 2:
            /* Force single thread mode */
            single_thread = 0;
            break;
        case 3:
            /* Force multi thread mode */
            single_thread = 1;
            break;
        default:
            /* Do nothing */
            break;
    }

    if (sessionp->session_sopt.sv_active_capabilities & SMB2_GLOBAL_CAP_LARGE_MTU) {
        /* Get the quantum size and number to use */
        smb2_smb_get_quantum_sizes(sessionp, *len, do_read, &quantumSize, &quantumNbr, &recheck);
    }
    else {
        /*
         * If server does not support large MTUs, then SMB_MAX_RW_HASH_SZ * 2
         * number of quantums should be sufficient
         */
        quantumNbr = kSmallMTUMaxNumber;
        if (do_read) {
            quantumSize = SS_TO_SESSION(share)->session_rxmax;
        }
        else {
            quantumSize = SS_TO_SESSION(share)->session_wxmax;
        }
    }

    SMB_MALLOC_TYPE_COUNT(rw_pb, struct smb_rw_arg, quantumNbr, Z_WAITOK);
    if (rw_pb == NULL) {
        SMBERROR("failed to allocate rw_pb");
        error = ENOMEM;
        goto done;
    }

    SMB_LOG_KTRACE(SMB_DBG_SMB_RW_ASYNC | DBG_FUNC_NONE, 0xabc001, single_thread, quantumNbr, quantumSize, 0);

    SMB_LOG_IO("do_read %d single thread %d length <%lld> quantum_nbr <%d> quantum_size <%d> \n",
               do_read, single_thread, saved_len, quantumNbr, quantumSize);
    
resend:
    /* Use a temp smb2_rw_rq instead of in_read_writep */
    tmp_read_write.flags = 0;
    tmp_read_write.remaining = in_read_writep->remaining;
    tmp_read_write.write_flags = in_read_writep->write_flags;
    tmp_read_write.fid = in_read_writep->fid;
    tmp_read_write.mc_flags = 0;
    tmp_read_write.io_len = in_read_writep->io_len;
    tmp_read_write.auio = uio_duplicate(in_read_writep->auio);
    if (tmp_read_write.auio == NULL) {
        SMBERROR("uio_duplicate failed\n");
        error = ENOMEM;
        goto done;
    }
    tmp_read_write.ret_ntstatus = 0;
    tmp_read_write.ret_len = 0;

    /* Zero out param blocks */
    for (i = 0; i < quantumNbr; i++) {
        bzero(&rw_pb[i], sizeof(rw_pb[i]));
        
        rw_pb[i].command = SMB_READ_WRITE;
        if (!single_thread) {
            lck_mtx_init(&rw_pb[i].rw_arg_lock, smb_rw_group, LCK_ATTR_NULL);
        }
    }

    /*
     * %%% TO DO for multichannel.
     * How to calculate how much time was spent on each iod and how much data
     * was sent on each iod so we can calulate a bytes/sec for each iod.
     * iods (especially different speed NICs) can have different quantum sizes
     * and quantum numbers that work the fastest for that NIC. The current
     * code is assuming the same quantum size for all iods and that will need
     * to be fixed.
     *
     * I tried timing from when request was sent to when we processed the
     * reply, but it doesnt take into account all the overlapping IOs that
     * happen on the wire. Mainly, the request timings just kept indicating
     * the wrong quantumSize/quantumNbr set was the fastest.
     *
     * Watch out on reading that the first several responses take a longer
     * time to return probably due to read ahead caching on the server.
     *
     * Also have to handle multiple shares doing copies at the same time on a
     * session that has multiple active channels.
     */
    microtime(&start_time);  /* save time that we started */

    /* Fill in initial requests */
    for (i = 0; i < quantumNbr; i++) {
        bool no_credits = false;

        /* Malloc the read_writep */
        SMB_MALLOC_TYPE(rw_pb[i].rw.read_writep, struct smb2_rw_rq, Z_WAITOK_ZERO);
        if (rw_pb[i].rw.read_writep == NULL) {
            SMBERROR("SMB_MALLOC_TYPE failed\n");
            error = ENOMEM;
            goto bad;
        }
        
        /*
         * Fill in the Read/Write request
         */
        error = smb2_smb_read_write_fill(share, &tmp_read_write,
                                         rw_pb[i].rw.read_writep, &rw_pb[i].rw.rqp,
                                         do_read, quantumSize,
                                         allow_compressionp, context);
        
        if (error) {
            if (error == ENOBUFS) {
				if (rw_pb[i].rw.rqp == NULL) {
					SMBERROR("smb2_smb_fillin_read/write failed %d\n", error);
					goto bad;
				}
				
                /* Running out of credits, clear error and send what we have */
                SMBDEBUG("low on credits %d\n", error);
                error = 0;
                no_credits = true;
            }
            else {
                SMBERROR("smb2_smb_fillin_read/write failed %d\n", error);
                goto bad;
            }
        }

        rw_pb[i].error = 0;

        if (!single_thread) {
            /* Queue it up to be sent by rw helper threads */
            rw_pb[i].flags |= SMB_RW_IN_USE;
            smb_rw_proxy(&rw_pb[i]);
        }

        /* Any more to read or write? If not, then send what we have */
        if (uio_resid(tmp_read_write.auio) == 0 || no_credits) {
            /* i equals number of initial rw_pb's filled in */
            i += 1;
            break;
        }
    }
    
    SMB_LOG_KTRACE(SMB_DBG_SMB_RW_ASYNC | DBG_FUNC_NONE, 0xabc002, i, 0, 0, 0);

    if (single_thread) {
        /*
         * Send initial requests
         */
        for (j = 0; j < i; j++) {
            SMB_LOG_KTRACE(SMB_DBG_SMB_RW_ASYNC | DBG_FUNC_NONE, 0xabc003, smb_hideaddr(rw_pb[j].rw.rqp), j, 0, 0);
            
            error = smb_iod_rq_enqueue(rw_pb[j].rw.rqp);
            if (error) {
                SMBERROR("smb_iod_rq_enqueue failed %d\n", error);
                goto bad;
            }
            rw_pb[j].flags |= SMB_RW_IN_USE;
        }
    }

    /* Wait for replies and refill requests as needed */
	done = 0;
    while (!done) {
        /* Assume we are done */
        done = 1;
        
        for (j = 0; j < i; j++) {
            if (!single_thread) {
                lck_mtx_lock(&rw_pb[j].rw_arg_lock);
            }

            if (rw_pb[j].flags & SMB_RW_IN_USE) {
                /* Need to wait for the reply to arrive */
                if (single_thread) {
                    error = smb_rq_reply(rw_pb[j].rw.rqp);
                    SMB_LOG_KTRACE(SMB_DBG_SMB_RW_ASYNC | DBG_FUNC_NONE, 0xabc004, smb_hideaddr(rw_pb[j].rw.rqp), j, error, 0);
                }
                else {
                    while (!(rw_pb[j].flags & SMB_RW_REPLY_RCVD)) {
                        error = msleep(&rw_pb[j].flags, &rw_pb[j].rw_arg_lock, PSOCK,
                                       "smb_rw_reply_wait", NULL);
                        if (error) {
                            SMBERROR("msleep for rw reply error %d\n", error);
                        }
                    }
                    error = rw_pb[j].error;
                }

                rw_pb[j].flags &= ~SMB_RW_IN_USE;

                if (error) {
                    if ((rw_pb[j].rw.rqp != NULL) &&
                        (rw_pb[j].rw.rqp->sr_flags & SMBR_RECONNECTED)) {
                        SMBDEBUG("reconnected on read/write[%d]\n", j);
                        reconnect = 1;
                    }
                    else {
                        SMBERROR("smb_rq_reply failed %d\n", error);
                    }

                    if (!single_thread) {
                        lck_mtx_unlock(&rw_pb[j].rw_arg_lock);
                    }
                    goto bad;
                }

                if ((!tmp_read_write.ret_ntstatus) &&
                    (rw_pb[j].rw.rqp != NULL)) {
                    tmp_read_write.ret_ntstatus = rw_pb[j].rw.rqp->sr_ntstatus;
                }
                
                /* Now get pointer to response data */
                smb_rq_getreply(rw_pb[j].rw.rqp, &mdp);
                
                if (do_read) {
                    error = smb2_smb_parse_read_one(mdp,
                                                    &rw_pb[j].rw.resid,
                                                    rw_pb[j].rw.read_writep);
                }
                else {
                    /*
                     * If compression allowed, check to see if write compression
                     * failed on this data
                     */
                    if ((*allow_compressionp != 0) &&
                        (rw_pb[j].rw.rqp != NULL) &&
                        (rw_pb[j].rw.rqp->sr_extflags & SMB2_FAILED_COMPRESS_WRITE)) {
                        compress_write_fails += 1;
                        
                        if (compress_write_fails > sessionp->compression_max_fail_cnt) {
                            SMB_LOG_COMPRESS("Too many write compression failures (%d > %d), compression will be disabled for this file \n",
                                             compress_write_fails, sessionp->compression_max_fail_cnt);
                            *allow_compressionp = 0;
                        }
                    }
                    
                    error = smb2_smb_parse_write_one(mdp,
                                                     &rw_pb[j].rw.resid,
                                                     rw_pb[j].rw.read_writep);
                }

                if (error) {
                    SMBERROR("parse failed for [%d] %d mid <%lld>\n",
                             j, error, (rw_pb[j].rw.rqp == NULL) ? 0 : rw_pb[j].rw.rqp->sr_messageid);
                    if (!single_thread) {
                        lck_mtx_unlock(&rw_pb[j].rw_arg_lock);
                    }
                    goto bad;
                }

                /* Add up amount of IO that we have done so far */
                *rresid += rw_pb[j].rw.resid;
                tmp_read_write.ret_len += rw_pb[j].rw.resid;

                /*
                 * Make sure resid is same as actual amt of data we asked
                 * for.  If we are reading from a named pipe (i.e. svrsvc),
                 * this can actually happen (read less from the pipe
                 * than we requested).
                 */
                if (rw_pb[j].rw.resid != rw_pb[j].rw.read_writep->io_len) {
                    SMBWARNING("IO Mismatched. Requested %lld but got %lld\n",
                               rw_pb[j].rw.read_writep->io_len, rw_pb[j].rw.resid);
                    /*
                     * <72062477> skip the short read if its a named pipe being
                     * used. Named pipes are always on the share IPC$.
                     */
                    if ((do_read) &&
                        (rw_pb[j].rw.resid < rw_pb[j].rw.read_writep->io_len) &&
                        (strcmp(share->ss_name, "IPC$") != 0)) {
                        /*
                         * <63197657> Need to reissue a read for the missing
                         * bytes.
                         *
                         * rw_pb[j].read_writep->io_len is data len requested
                         * rw_pb[j].resid is the data len actually received
                         */
                        do_short_read = 1;
                        short_read_len = rw_pb[j].rw.read_writep->io_len - rw_pb[j].rw.resid;

                        /*
                         * Copy the short read's smb2_rw_rq info so I can
                         * call smb2_smb_read_write_fill().
                         */
                        short_read_rq.flags = rw_pb[j].rw.read_writep->flags;
                        short_read_rq.remaining = rw_pb[j].rw.read_writep->remaining;
                        short_read_rq.write_flags = rw_pb[j].rw.read_writep->write_flags;
                        short_read_rq.fid = rw_pb[j].rw.read_writep->fid;
                        short_read_rq.auio = uio_duplicate(rw_pb[j].rw.read_writep->auio);
                        short_read_rq.io_len = rw_pb[j].rw.read_writep->io_len;

                        /*
                         * The uio_resid(short_read_rq.auio) is still set for
                         * the total remaining amount of data when this read
                         * was sent so need to set the uio to only ask for the
                         * missing data len.
                         *
                         * The uio offset should be fine as when we parsed the
                         * read into uio, it updated the offset to the correct
                         * value for us.
                         */
                        uio_setresid(short_read_rq.auio, short_read_len);

                        SMBWARNING("Short read requesting len %lld offset %lld \n",
                                   uio_resid(short_read_rq.auio),
                                   uio_offset(short_read_rq.auio));
                   }
                }
                
                if (uio_resid(tmp_read_write.auio) || (do_short_read == 1)) {
                    if (do_short_read == 0) {
                        /*
                         * The more common case where we have more data to
                         * read or write.
                         */
                        error = smb2_smb_read_write_fill(share,
                                                         &tmp_read_write,
                                                         rw_pb[j].rw.read_writep,
                                                         &rw_pb[j].rw.rqp,
                                                         do_read, quantumSize,
                                                         allow_compressionp, context);
                    }
                    else {
                        /*
                         * <63197657> The less common case where we had a short
                         * read. Request just the missing read data
                         */
                        error = smb2_smb_read_write_fill(share,
                                                         &short_read_rq,
                                                         rw_pb[j].rw.read_writep,
                                                         &rw_pb[j].rw.rqp,
                                                         do_read, quantumSize,
                                                         allow_compressionp, context);
                        uio_free(short_read_rq.auio);
                        do_short_read = 0;
                    }

                    if ((error) && !(error == ENOBUFS)) {
                        /* Being low on credits is ok to ignore */
                        SMBERROR("smb2_smb_fillin_read/write2 failed %d on [%d] flags 0x%x \n",
                                 error, j, rw_pb[j].flags);
                        if (!single_thread) {
                            lck_mtx_unlock(&rw_pb[j].rw_arg_lock);
                        }
                        goto bad;
                    }

                    if (!single_thread) {
                        /* Clear reply received flag */
                        rw_pb[j].flags &= ~SMB_RW_REPLY_RCVD;
                    }

                    /* Queue it up to be sent */
                    rw_pb[j].error = 0;

                    if (single_thread) {
                        SMB_LOG_KTRACE(SMB_DBG_SMB_RW_ASYNC | DBG_FUNC_NONE, 0xabc003, smb_hideaddr(rw_pb[j].rw.rqp), j, 0, 0);

                        error = smb_iod_rq_enqueue(rw_pb[j].rw.rqp);
                        if (error) {
                            SMBERROR("smb_iod_rq_enqueue failed %d\n", error);
                            goto bad;
                        }
                        rw_pb[j].flags |= SMB_RW_IN_USE;
                    }
                    else {
                        rw_pb[j].flags |= SMB_RW_IN_USE;
                        smb_rw_proxy(&rw_pb[j]);
                    }

                    /* Not done yet */
                    done = 0;
                }
            } /* If SMB_RW_IN_USE */

            if (!single_thread) {
                lck_mtx_unlock(&rw_pb[j].rw_arg_lock);
            }
        } /* For loop */
    } /* While loop */

bad:
    microtime(&current_time);  /* get time that we finished */

    SMB_LOG_KTRACE(SMB_DBG_SMB_RW_ASYNC | DBG_FUNC_NONE, 0xabc005, error, 0, 0, 0);

    for (i = 0; i < quantumNbr; i++) {
        if (!single_thread) {
            lck_mtx_lock(&rw_pb[i].rw_arg_lock);
        }

        /* If it has not finished, then wait for it to finish */
        if (rw_pb[i].flags & SMB_RW_IN_USE) {
            /* Need to wait for the reply to arrive */
            if (single_thread) {
                error = smb_rq_reply(rw_pb[i].rw.rqp);
                SMB_LOG_KTRACE(SMB_DBG_SMB_RW_ASYNC | DBG_FUNC_NONE, 0xabc004, smb_hideaddr(rw_pb[i].rw.rqp), i, error, 0);
            }
            else {
                while (!(rw_pb[i].flags & SMB_RW_REPLY_RCVD)) {
                    error = msleep(&rw_pb[i].flags, &rw_pb[i].rw_arg_lock, PSOCK,
                                   "smb_rw_reply_wait clean", NULL);
                    if (error) {
                        SMBERROR("msleep for rw reply clean %d\n", error);
                    }
                }
                error = rw_pb[i].error;
            }
        }

        if (rw_pb[i].rw.rqp != NULL) {
            smb_rq_done(rw_pb[i].rw.rqp);
            rw_pb[i].rw.rqp = NULL;
        }

        if (rw_pb[i].rw.read_writep != NULL) {
            if (rw_pb[i].rw.read_writep->auio) {
                uio_free(rw_pb[i].rw.read_writep->auio);
                rw_pb[i].rw.read_writep->auio = NULL;
            }

            SMB_FREE_TYPE(struct smb2_rw_rq, rw_pb[i].rw.read_writep);
        }

        rw_pb[i].rw.resid = 0;

        if (!single_thread) {
            lck_mtx_unlock(&rw_pb[i].rw_arg_lock);
            lck_mtx_destroy(&rw_pb[i].rw_arg_lock, smb_rw_group);
        }
    }

    if (reconnect == 1) {
        /* If failed due to reconnect, restore original values and try again */
        *len = saved_len;
        *rresid = saved_rresid;

        if (tmp_read_write.auio) {
            uio_free(tmp_read_write.auio);
			tmp_read_write.auio = NULL;
        }

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

        smbfs_uio_update(in_read_writep->auio, *rresid);
    }

    if (tmp_read_write.auio) {
        uio_free(tmp_read_write.auio);
    }

    /*
     * Only if
     * 1. Server supports large MTUs
     * 2. Multiple read/writes were done
     * 3. We are rechecking the speeds
     * Then call smb2_smb_adjust_quantum_sizes() to see if we need to change
     * quantum size and number.
     */
    if ((error == 0) &&
        (sessionp->session_sopt.sv_active_capabilities & SMB2_GLOBAL_CAP_LARGE_MTU) &&
        (recheck == 1)) {
        /* Get elapsed time for this transfer */
        timersub (&current_time, &start_time, &elapsed_time);

        SMB_LOG_IO("quantumSize %d, etime %ld:%d len %lld \n",
                   quantumSize, elapsed_time.tv_sec, elapsed_time.tv_usec, saved_len);
        smb2_smb_adjust_quantum_sizes(share, do_read,
                                      quantumSize, quantumNbr,
                                      elapsed_time, saved_len);
    }

done:
    if (rw_pb != NULL) {
        SMB_FREE_TYPE_COUNT(struct smb_rw_arg, quantumNbr, rw_pb);
    }

    SMB_LOG_KTRACE(SMB_DBG_SMB_RW_ASYNC | DBG_FUNC_END, error, *rresid, 0, 0, 0);
	return error;
}

static int
smb2_smb_read_write_fill(struct smb_share *share,
                         struct smb2_rw_rq *master_read_writep,
                         struct smb2_rw_rq *read_writep,
                         struct smb_rq **rqp,
                         uint32_t do_read, uint32_t quantum_size,
                         uint32_t *allow_compressionp,
                         vfs_context_t context)
{
	int error;
	user_ssize_t len, saved_len, resid = 0;
    
    if (do_read) {
        len = MIN(quantum_size, uio_resid(master_read_writep->auio));
    }
    else {
        len = MIN(quantum_size, uio_resid(master_read_writep->auio));
    }
    
    SMB_LOG_KTRACE(SMB_DBG_SMB_RW_FILL | DBG_FUNC_START, uio_offset(master_read_writep->auio), len, 0, 0, 0);

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
    if (read_writep->auio == NULL) {
        SMBERROR("uio_duplicate failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    read_writep->ret_ntstatus = 0;
    read_writep->ret_len = 0;
    
    if (do_read) {
        error = smb2_smb_read_one(share, read_writep,
                                  &len, &resid,
                                  rqp, NULL,
                                  *allow_compressionp, context);
    }
    else {
        error = smb2_smb_write_one(share, read_writep, &len, &resid, rqp, NULL,
                                   allow_compressionp, context);
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
        (*rqp)->sr_timo = (*rqp)->sr_session->session_timo;
    }
	(*rqp)->sr_state = SMBRQ_NOTSENT;
    
    /* ASSUME read/write will work and read entire amount */
    smbfs_uio_update(master_read_writep->auio, len);
    
bad:
    SMB_LOG_KTRACE(SMB_DBG_SMB_RW_FILL | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smb_smb_read(struct smb_share *share, SMBFID fid, uio_t uio,
             uint32_t allow_compression, vfs_context_t context)
{
    int error;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2_smb_read_uio(share, fid, uio,
                                  allow_compression, context);
    }
    else {
        error = smb1_read(share, fid, uio, context);
    }
    
    return (error);
}

int
smb2_smb_set_info(struct smb_share *share, struct smb2_set_info_rq *infop,
                  struct smb_rq **compound_rqp,
                  struct smbiod *iod, vfs_context_t context)
{
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
    if (iod) {
        error = smb_iod_ref(iod, __FUNCTION__);
    } else {
        error = smb_iod_get_any_iod(SS_TO_SESSION(share), &iod, __FUNCTION__);
    }
    if (error) {
        return error;
    }

    error = smb2_rq_alloc(SSTOCP(share), SMB2_SET_INFO, NULL, context, iod, &rqp);
    if (error) {
        smb_iod_rel(iod, NULL, __FUNCTION__);
        return error;
    }

    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

    smb_rq_getrequest(rqp, &mbp);
    
    /*
     * Build the SMB 2/3 Set Info Request
     */
    mb_put_uint16le(mbp, 33);                       /* Struct size */
    mb_put_uint8(mbp, infop->info_type);            /* Info Type */
    mb_put_uint8(mbp, infop->file_info_class);      /* File Info Class */
    buffer_len = mb_reserve(mbp, sizeof(uint32_t)); /* Buffer Len */
    mb_put_uint16le(mbp, SMB2_HDRLEN + 32);         /* Buffer Offset */
    mb_put_uint16le(mbp, 0);                        /* Reserved */
    mb_put_uint32le(mbp, infop->add_info);          /* Additional Info */

    /* map fid to SMB 2/3 fid */
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

			rqp->sr_extflags |= SMB2_NON_IDEMPOTENT;
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
            
			rqp->sr_extflags |= SMB2_NON_IDEMPOTENT;
            break;
            
        default:
			if (infop->info_type == SMB2_0_INFO_SECURITY) {
				rqp->sr_extflags |= SMB2_NON_IDEMPOTENT;
			}
			else {
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

    if (infop->mc_flags & SMB2_MC_REPLAY_FLAG) {
        *rqp->sr_flagsp |= SMB2_FLAGS_REPLAY_OPERATIONS;
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
            SMB_LOG_MC("resending messageid %llu cmd %u.\n", rqp->sr_messageid, rqp->sr_command);
            if (rqp->sr_flags & SMBR_ALT_CH_DISCON) {
                /* An alternate channel got disconnected. Resend with the REPLAY flag set */
                infop->mc_flags |= SMB2_MC_REPLAY_FLAG;
            }

            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            iod = NULL;
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

/* Parse the error response and get the max_dialect */
static int
smb2_smb_tree_connect_bad_cluster_dialect(struct smb_rq* rqp)
{
    int error = 0;
    uint32_t byte_count = 0;
    uint16_t structure_size = 0, max_dialect = 0;
    uint8_t error_context_count = 0, reserved = 0;
    struct mdchain *mdp = NULL;
    struct smb_session *sessionp = rqp->sr_session;

    smb_rq_getreply(rqp, &mdp);

    /* Check structure size is 9 */
    error = md_get_uint16le(mdp, &structure_size);
    if (error) {
        goto bad;
    }
    if (structure_size != 9) {
        SMBERROR("Bad struct size: %u\n", (uint32_t)structure_size);
        error = EBADRPC;
        goto bad;
    }

    /* Should have 1 error context */
    error = md_get_uint8(mdp, &error_context_count);
    if (error) {
        goto bad;
    }
    if (error_context_count != 1) {
        SMBERROR("Too many error contexts: %u\n", (uint32_t)error_context_count);
        error = EBADRPC;
        goto bad;
    }

    /* reserved */
    error = md_get_uint8(mdp, &reserved);
    if (error) {
        goto bad;
    }

    /* Get the byte count of the error context */
    error = md_get_uint32le(mdp, &byte_count);
    if (error) {
        goto bad;
    }
    if (byte_count < 2) {
        SMBERROR("Error context data byte count <%d> < 2", byte_count);
        error = EINVAL;
        goto bad;
    }
    /* Read all data except last two bytes */
    error = md_get_mem(mdp, NULL, byte_count - 2, MB_MSYSTEM);
    if (error) {
        goto bad;
    }

    /* The last two bytes contain max_dialect */
    error = md_get_uint16le(mdp, &max_dialect);
    if (error) {
        goto bad;
    }

    switch(max_dialect) {
        case SMB2_DIALECT_0202:
        case SMB2_DIALECT_0210:
            /* turn off SMB3 for this session, reconnecting with SMB2 */
            sessionp->session_flags &= ~(SMBV_SMB30 | SMBV_SMB302 | SMBV_SMB311);
        case SMB2_DIALECT_0300:
        case SMB2_DIALECT_0302:
        case SMB2_DIALECT_0311:
            sessionp->session_max_dialect = max_dialect;
            break;
        default:
            SMBERROR("Unknown dialect 0x%x", max_dialect);
            error = EINVAL;
            goto bad;
    }

bad:
    return error;
}

int 
smb2_smb_tree_connect(struct smb_session *sessionp, struct smb_share *share,
                      const char *serverName, size_t serverNameLen,
                      vfs_context_t context)
{
#pragma unused(sessionp)
	struct smb_rq *rqp = NULL;
	struct mbchain *mbp = NULL;
	struct mdchain *mdp = NULL;
	int error = 0;
    uint16_t structure_size = 0;
    uint8_t share_type = 0;
    uint8_t reserved = 0;
	share->ss_tree_id = SMB2_TID_UNKNOWN;
    struct smbiod *iod = NULL;
    bool replay = false;
    uint32_t was_encrypted = 0;
    uint32_t sleep_attempts = 0;
    struct timespec sleeptime = {0};

resend:
    /*
     * Tree-connect uses main_iod and not any_iod,
     * because it may be needed during reconnect as
     * well.
     */
    error = smb_iod_get_main_iod(sessionp, &iod, __FUNCTION__);
    if (error) {
        return error;
    }

    /* Allocate request and header for a Tree Connect */
	error = smb2_rq_alloc(SSTOCP(share), SMB2_TREE_CONNECT, NULL, context, iod, &rqp);
    if (error) {
        smb_iod_rel(iod, NULL, __FUNCTION__);
		goto treeconnect_exit;
    }
    
    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

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

    if (replay) {
        *rqp->sr_flagsp |= SMB2_FLAGS_REPLAY_OPERATIONS;
    }
    
    if (sessionp->session_flags & SMBV_SMB311) {
        /*
         * To finish the pre auth integrity check the client has to send
         * a signed Tree Connect when the server does not require signing.
         */
        rqp->sr_flags |= SMBR_SIGNED;
    }

    error = smb_rq_simple(rqp);
	if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            SMB_LOG_MC("resending messageid %llu cmd %u.\n", rqp->sr_messageid, rqp->sr_command);
            if (rqp->sr_flags & SMBR_ALT_CH_DISCON) {
                /* An alternate channel got disconnected. Resend with the REPLAY flag set */
                replay = true;
            }
            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            iod = NULL;
            goto resend;
        }
        
        if (rqp->sr_ntstatus == STATUS_SMB_BAD_CLUSTER_DIALECT) {
            /*
             * <rdar://problem/102811989>
             * 3.2.5.5: The SMB dialect is not supported and need to renegotiate
             * with ErrorContextData as the new maximum dialect
             */
            error = smb2_smb_tree_connect_bad_cluster_dialect(rqp);
            SMBERROR("TreeConnect returned bad_culster_dialect with max_dialect 0x%x",
                     sessionp->session_max_dialect);
            if (error) {
                SMBERROR("smb2_smb_tree_connect_bad_cluster_dialect failed with %d", error);
                goto bad;
            }

            /* start reconnect with max_dialect */
            smb_iod_start_reconnect(iod);

            /* sleep for one second at a time */
            sleeptime.tv_sec = 1;
            sleeptime.tv_nsec = 0;

            SMBERROR("Waiting for reconnect to finish for %d seconds",
                     sessionp->reconnect_wait_time);
            /* Wait for reconnect to finish and return succesfully */
            while (iod->iod_state != SMBIOD_ST_SESSION_ACTIVE) {
                msleep(&iod->iod_state, NULL, 0, "waiting for reconnect", &sleeptime);
                sleep_attempts++;
                if (sleep_attempts > sessionp->reconnect_wait_time ||
                    iod->iod_state == SMBIOD_ST_DEAD) {
                    /*
                     * did the server go down while we are reconnecting?
                     * reconnect_wait_time is the most we'll wait for reconnect, give up
                     */
                    SMBERROR("Failed to reconnect with %d attempts, iod_state: %d",
                             sleep_attempts,
                             iod->iod_state);
                    error = ENOTCONN;
                    goto bad;
                }
            }
            SMBERROR("Finished waiting for reconnect, made %d attempts", sleep_attempts);
        }
		goto bad;
    }
    
    /* Now get pointer to response data */
	smb_rq_getreply(rqp, &mdp);
    
    /* 
     * Parse SMB 2/3 Tree Connect Response 
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
    
    /*
     * Could be a reconnect, save if share was set to be encrypted. This is
     * for the case of mount_smbfs mount option where only one share is being
     * encrypted
     */
    if (share->ss_share_flags & SMB2_SHAREFLAG_ENCRYPT_DATA) {
        was_encrypted = 1;
    }
    
    /* Get Share Flags */
    error = md_get_uint32le(mdp, &share->ss_share_flags);
    if (error)
        goto bad;
    
    /*
     * Is share encryption being forced on?
     *
     * [MS-SMB2] 3.3.4.1.4
     * If Connection.Dialect belongs to the SMB 3.x dialect family and
     * Connection.ClientCapabilities includes the SMB2_GLOBAL_CAP_ENCRYPTION
     * bit, the server MUST encrypt the message before sending, if
     * IsEncryptionSupported is TRUE and any of the following conditions are
     * satisfied:
     *     If the message being sent is any response to a client request for
     *     which Request.IsEncrypted is TRUE.
     *
     * This is probably from the nsmb.conf option being set meaning that all
     * shares will be encrypted.
     */
    if ((sessionp->session_misc_flags & SMBV_FORCE_SHARE_ENCRYPT) ||
        (was_encrypted == 1)) {
        /*
         * If forcing share encryption, then pretend like the share told us
         * they are using share encryption.
         */
        SMBWARNING("Share level encryption forced on \n ");
        share->ss_share_flags |= SMB2_SHAREFLAG_ENCRYPT_DATA;
    }
    
    if ((sessionp->session_misc_flags & SMBV_FORCE_IPC_ENCRYPT) &&
        (strcmp(share->ss_name, "IPC$") == 0)) {
        /*
         * Its the IPC$ share and some other share in this session must be
         * encrypted, so encrypt IPC$ too.
         */
        SMBWARNING("Share level encryption forced on for IPC$ \n ");
        share->ss_share_flags |= SMB2_SHAREFLAG_ENCRYPT_DATA;
    }

    /* Get Capabilities */
    error = md_get_uint32le(mdp, &share->ss_share_caps);
    if (error)
        goto bad;
    
    /* Map SMB 2/3 capabilities to SMB 1 share->optionalSupport */
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
    struct smbiod *iod = NULL;
    bool replay = false;

	if (share->ss_tree_id == SMB2_TID_UNKNOWN) {
        /* no valid tree id to disconnect on, just ignore */
		return 0;
    }
    
resend:
    error = smb_iod_get_any_iod(SS_TO_SESSION(share), &iod, __FUNCTION__);
    if (error) {
        return error;
    }

    /* Allocate request and header for a Tree Disconnect */
	error = smb2_rq_alloc(SSTOCP(share), SMB2_TREE_DISCONNECT, NULL, 
                          context, iod, &rqp);
	if (error) {
        smb_iod_rel(iod, NULL, __FUNCTION__);
		return error;
    }
    
    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

    /*
     * Fill in Tree Disconnect part 
     * Dont use struct ptr since its only 4 bytes long
     */
    smb_rq_getrequest(rqp, &mbp);
    
    mb_put_uint16le(mbp, 4);       /* Struct size */
    mb_put_uint16le(mbp, 0);       /* Reserved */

    if (replay) {
        *rqp->sr_flagsp |= SMB2_FLAGS_REPLAY_OPERATIONS;
    }
    
    error = smb_rq_simple(rqp);
	share->ss_tree_id = SMB2_TID_UNKNOWN;   /* dont care if got error or not */
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            SMB_LOG_MC("resending messageid %llu cmd %u.\n", rqp->sr_messageid, rqp->sr_command);
            if (rqp->sr_flags & SMBR_ALT_CH_DISCON) {
                /* An alternate channel got disconnected. Resend with the REPLAY flag set */
                replay = true;
            }

            /* Rebuild and try sending again */
            smb_rq_done(rqp);
            rqp = NULL;
            iod = NULL;
            goto resend;
        }
        
        goto bad;
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    /* 
     * Parse SMB 2/3 Tree Disconnect 
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
    struct smb_session *sessionp = SS_TO_SESSION(share);
	int error;

    if (sessionp->session_flags & SMBV_SMB2) {
        error = smb2_smb_tree_disconnect(share, context);
    }
    else {
        error = smb1_smb_treedisconnect(share, context);
    }
    return (error);
}

int
smb2_smb_write(struct smb_share *share, struct smb2_rw_rq *writep,
               uint32_t *allow_compressionp, vfs_context_t context)
{
	int error = 0;
	user_ssize_t orig_resid, len, total_size, resid = 0;
	off_t orig_offset;
    
    /* assume we can write it all in one request */
	total_size = orig_resid = uio_resid(writep->auio);
	orig_offset = uio_offset(writep->auio);
	
    SMB_LOG_KTRACE(SMB_DBG_SMB_WRITE | DBG_FUNC_START,
                   uio_offset(writep->auio),
                   uio_resid(writep->auio), 0, 0, 0);
	
	while (total_size > 0) {
        /* write "len" amount of data */
		len = total_size;
        
		error = smb2_smb_read_write_async(share, writep, &len, &resid, 0,
                                          allow_compressionp, context);
		if (error) {
			SMBERROR("smb2_smb_read_write_async failed with an error %d\n", error);
			break;
        }
        
        /* did we fail to write out the data? */
		if (resid < len) {
			SMBERROR("Unexpected partial write! resid %lld, len %lld\n", resid, len);
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
    
    SMB_LOG_KTRACE(SMB_DBG_SMB_WRITE | DBG_FUNC_END, error, 0, 0, 0, 0);
	return error;
}

/*
 * "*len" is amount of data requested (updated with actual size attempted)
 * "*rresid" is actual amount of data written
 */
int
smb2_smb_write_one(struct smb_share *share,
                   struct smb2_rw_rq *writep,
                   user_ssize_t *len,
                   user_ssize_t *rresid,
                   struct smb_rq **compound_rqp,
                   struct smbiod *iod,
                   uint32_t *allow_compressionp,
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
    uio_t auio = NULL;

	len32 = (uint32_t) MIN(SS_TO_SESSION(share)->session_wxmax, *len);

resend:
    if (iod) {
        error = smb_iod_ref(iod, __FUNCTION__);
    } else {
        // Multichannel: use any iod to send
        error = smb_iod_get_any_iod(SS_TO_SESSION(share), &iod, __FUNCTION__);
    }
    if (error) {
        return error;
    }

    /*
     * Allocate request and header for a Write
     * Available credits may reduce the write size
     */
    error = smb2_rq_alloc(SSTOCP(share), SMB2_WRITE, &len32, context, iod, &rqp);
    if (error) {
        smb_iod_rel(iod, NULL, __FUNCTION__);
        return error;
    }

    if (compound_rqp == NULL) {
        /* ktrace only if doing actual read and not building part of a cmpd */
        SMB_LOG_KTRACE(SMB_DBG_SMB_WRITE_ONE | DBG_FUNC_START, smb_hideaddr(rqp), uio_offset(writep->auio), len32, 0, 0);
    }

    SMB_LOG_MC_REF("id %u function %s rqp 0x%lx cmd %d ref_cnt %u.\n",
                   iod->iod_id, __FUNCTION__, smb_hideaddr(rqp), rqp->sr_command,
                   iod->iod_ref_cnt);

    *len = len32;
    
    if (remaining > len32) {
        remaining -= len32;
    }
    else {
        remaining = 0;
    }

    smb_rq_getrequest(rqp, &mbp);
    
    /* Make a copy of uio in case we need to resend */
    auio = uio_duplicate(writep->auio);
    if (auio == NULL) {
        SMBERROR("uio_duplicate failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    /*
     * Build the SMB 2/3 Write Request
     */
    mb_put_uint16le(mbp, 49);                       /* Struct size */
    data_offset = SMB2_HDRLEN;
    data_offset += 48;
    mb_put_uint16le(mbp, data_offset);              /* Data Offset */
    mb_put_uint32le(mbp, (uint32_t) *len);          /* Length of write */
	mb_put_uint64le(mbp, uio_offset(auio));         /* Offset */

    /* map fid to SMB 2/3 fid */
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

    error = mb_put_uio(mbp, auio, (int) *len);       /* Write data */
    if (error) {
        goto bad;
    }

    *rqp->sr_flagsp |= (writep->mc_flags & SMB2_MC_REPLAY_FLAG)?(SMB2_FLAGS_REPLAY_OPERATIONS):0;

    if (*allow_compressionp == 0) {
        /* Dont compress this write */
        rqp->sr_extflags |= SMB2_NO_COMPRESS_WRITE;
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

        if (auio != NULL) {
            /* ASSUME write will work and write entire amount */
            smbfs_uio_update(writep->auio, *len);

            uio_free(auio);
            auio = NULL;
        }
        
        return (0);
    }

    error = smb_rq_simple_timed(rqp, SMBWRTTIMO);
    writep->ret_ntstatus = rqp->sr_ntstatus;
    if (error) {
        if (rqp->sr_flags & SMBR_RECONNECTED) {
            SMB_LOG_MC("resending messageid %llu cmd %u.\n", rqp->sr_messageid, rqp->sr_command);
            if (rqp->sr_flags & SMBR_ALT_CH_DISCON) {
                /* An alternate channel got disconnected. Resend with the REPLAY flag set */
                writep->mc_flags |= SMB2_MC_REPLAY_FLAG;
            }

            /* Rebuild and try sending again */
            if (auio != NULL) {
                uio_free(auio);
                auio = NULL;
            }

            remaining += len32;
            
            smb_rq_done(rqp);
            rqp = NULL;
            iod = NULL;
            
            SMB_LOG_KTRACE(SMB_DBG_SMB_WRITE_ONE | DBG_FUNC_END, error, 0xabc001, 0, 0, 0);
            goto resend;
        }
        
        goto bad;
    }
    else {
        if (auio != NULL) {
            /* Update uio with amount of data written */
            smbfs_uio_update(writep->auio, *len);
        }
    }
    
    /* Now get pointer to response data */
    smb_rq_getreply(rqp, &mdp);
    
    error = smb2_smb_parse_write_one(mdp, rresid, writep);
    if (error) {
        goto bad;
    }
    
bad:    
    if (auio != NULL) {
        uio_free(auio);
    }

    smb_rq_done(rqp);
    SMB_LOG_KTRACE(SMB_DBG_SMB_WRITE_ONE | DBG_FUNC_END, error, 0, 0, 0, 0);
    return error;
}

static int
smb2_smb_write_uio(struct smb_share *share, SMBFID fid, uio_t uio, int ioflag,
                   uint32_t *allow_compressionp, vfs_context_t context)
{
#pragma unused(ioflag)
    int error;
	uint32_t write_mode = 0;
 	struct smb2_rw_rq *writep = NULL;
    int attempts = 0;
    uio_t temp_uio = NULL;
    user_size_t write_count = 0;
    
    temp_uio = uio_duplicate(uio);
    if (temp_uio == NULL) {
        SMBERROR("uio_duplicate failed\n");
        error = ENOMEM;
        return error;
    }
    
    write_count = uio_resid(uio);
    
    SMB_MALLOC_TYPE(writep, struct smb2_rw_rq, Z_WAITOK_ZERO);
    if (writep == NULL) {
        SMBERROR("SMB_MALLOC_TYPE failed\n");
        error = ENOMEM;
        goto done;
    }
    
again:
    writep->flags = 0;
    writep->remaining = 0;
    writep->write_flags = write_mode;
    writep->fid = fid;
    writep->auio = temp_uio;
    writep->mc_flags = 0;
    
    error = smb2_smb_write(share, writep, allow_compressionp, context);
	
    /* Handle servers that dislike write through mode */
    if ((error == EINVAL) &&
        (writep->ret_ntstatus == STATUS_INVALID_PARAMETER) &&
        (write_mode != 0) &&
        !((SS_TO_SESSION(share)->session_misc_flags) & SMBV_NO_WRITE_THRU) &&
        (attempts == 0)) {
        SMBWARNING("SMB 2/3 server cant handle write through. Disabling write through.\n");
        SS_TO_SESSION(share)->session_misc_flags |= SMBV_NO_WRITE_THRU;
        attempts += 1;
        
        /* Reset the temp_uio back to original values */
        if (temp_uio != NULL) {
            uio_free(temp_uio);
            temp_uio = NULL;
        }
        
        temp_uio = uio_duplicate(uio);
        if (temp_uio == NULL) {
            SMBERROR("uio_duplicate failed\n");
            error = ENOMEM;
            goto done;
        }
        
        write_mode = 0;
        bzero(writep, sizeof(struct smb2_rw_rq));
        
        goto again;
    }
    else {
        /* Update the actual uio */
        write_count -= uio_resid(temp_uio);
        smbfs_uio_update(uio, write_count);
    }
    
done:
    if (writep != NULL) {
        SMB_FREE_TYPE(struct smb2_rw_rq, writep);
    }
    
    if (temp_uio != NULL) {
        uio_free(temp_uio);
    }
    
    return error;
}

/*
 * The calling routine must hold a reference on the share
 */
int
smb_smb_write(struct smb_share *share, SMBFID fid, uio_t uio, int ioflag,
              uint32_t *allow_compressionp, vfs_context_t context)
{
    int error;
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2_smb_write_uio(share, fid, uio, ioflag,
                                   allow_compressionp, context);
    }
    else {
        error = smb1_write(share, fid, uio, ioflag, context);
    }
    
    return (error);
}


uint32_t
smb2_session_maxread(struct smb_session *sessionp, uint32_t max_read)
{
	uint32_t socksize = sessionp->session_sopt.sv_maxread;
	uint32_t srvr_max_read = sessionp->session_sopt.sv_maxread;
	uint32_t maxmsgsize = 0;
    struct smbiod *iod = sessionp->session_iod;

	/* Make sure we never use a size bigger than the socket can support */
	SMB_TRAN_GETPARAM(iod, SMBTP_RCVSZ, &socksize);
	
	maxmsgsize = MIN(srvr_max_read, socksize);
	
	/* Now make it page aligned */
	if (maxmsgsize > PAGE_SIZE) {
		maxmsgsize = (maxmsgsize / PAGE_SIZE) * PAGE_SIZE;
	}
	
	if (maxmsgsize > max_read) {
		maxmsgsize = max_read;
	}

	SMB_LOG_IO("srvr_max_read = <%u>, socksize = <%u>, max_read = <%u>, maxmsgsize = <%d> \n",
			   srvr_max_read, socksize, max_read, maxmsgsize);

    /* Paranoid checks */
    lck_mtx_lock(&sessionp->iod_quantum_lock);

    if (sessionp->iod_readSizes[2] > maxmsgsize) {
        SMBERROR("Limit max read quantum size to maxmsgsize <%d> \n", maxmsgsize);
        sessionp->iod_readSizes[2] = maxmsgsize;
    }

    if (sessionp->iod_readSizes[1] > maxmsgsize) {
        SMBERROR("Limit med read quantum size to maxmsgsize <%d> \n", maxmsgsize);
        sessionp->iod_readSizes[1] = maxmsgsize;
    }

    if (sessionp->iod_readSizes[0] > maxmsgsize) {
        SMBERROR("Limit min read quantum size to maxmsgsize <%d> \n", maxmsgsize);
        sessionp->iod_readSizes[0] = maxmsgsize;
    }

    if ((uint32_t) sessionp->iod_readQuantumSize > maxmsgsize) {
        SMBERROR("Limit read quantum size to maxmsgsize <%d> \n", maxmsgsize);
        sessionp->iod_readQuantumSize = maxmsgsize;
    }
    
    lck_mtx_unlock(&sessionp->iod_quantum_lock);

    return maxmsgsize;
}

static uint32_t
smb2_session_maxtransact(struct smb_session *sessionp)
{
	uint32_t socksize = sessionp->session_sopt.sv_maxtransact;
	uint32_t maxmsgsize = 0;
    struct smbiod *iod = sessionp->session_iod;

	/* Make sure we never use a size bigger than the socket can support */
	SMB_TRAN_GETPARAM(iod, SMBTP_SNDSZ, &socksize);
    
    maxmsgsize = MIN(sessionp->session_sopt.sv_maxtransact, socksize);

    return (maxmsgsize);
}

/*
 * Figure out the largest write we can do to the server.
 */
uint32_t
smb2_session_maxwrite(struct smb_session *sessionp, uint32_t max_write)
{
	uint32_t socksize = sessionp->session_sopt.sv_maxwrite;
	uint32_t srvr_max_write = sessionp->session_sopt.sv_maxwrite;
	uint32_t maxmsgsize;
    struct smbiod *iod = sessionp->session_iod;

	/* Make sure we never use a size bigger than the socket can support */
	SMB_TRAN_GETPARAM(iod, SMBTP_SNDSZ, &socksize);
	
	maxmsgsize = MIN(srvr_max_write, socksize);
    
    /* Now make it page aligned */
	if (maxmsgsize > PAGE_SIZE) {
        maxmsgsize = (maxmsgsize / PAGE_SIZE) * PAGE_SIZE;
	}

	if (maxmsgsize > max_write) {
		maxmsgsize = max_write;
	}

	SMB_LOG_IO("srvr_max_write = <%u>, socksize = <%u>, max_write = <%u>, maxmsgsize = <%d> \n",
			   srvr_max_write, socksize, max_write, maxmsgsize);

    /* Paranoid checks */
    lck_mtx_lock(&sessionp->iod_quantum_lock);

    if (sessionp->iod_writeSizes[2] > maxmsgsize) {
        SMBERROR("Limit max write quantum size to maxmsgsize <%d> \n", maxmsgsize);
        sessionp->iod_writeSizes[2] = maxmsgsize;
    }
    
    if (sessionp->iod_writeSizes[1] > maxmsgsize) {
        SMBERROR("Limit med write quantum size to maxmsgsize <%d> \n", maxmsgsize);
        sessionp->iod_writeSizes[1] = maxmsgsize;
    }

    if (sessionp->iod_writeSizes[0] > maxmsgsize) {
        SMBERROR("Limit min write quantum size to maxmsgsize <%d> \n", maxmsgsize);
        sessionp->iod_writeSizes[0] = maxmsgsize;
    }
    
    if ((uint32_t) sessionp->iod_writeQuantumSize > maxmsgsize) {
        SMBERROR("Limit write quantum size to maxmsgsize <%d> \n", maxmsgsize);
        sessionp->iod_writeQuantumSize = maxmsgsize;
    }

    lck_mtx_unlock(&sessionp->iod_quantum_lock);

    return maxmsgsize;
}

uint32_t
smb2_session_max_io_size(struct smb_session *sessionp, int io_type)
{
    uint32_t max_io_size = 0;
    if (io_type == SMB2_READ) {
        max_io_size = MIN(sessionp->session_rxmax, kMaxSingleIO);
    } else if (io_type == SMB2_WRITE) {
        max_io_size = MIN(sessionp->session_wxmax, kMaxSingleIO);
    } else {
        SMBERROR("Unexpected io_type %x", io_type);
    }
    return max_io_size;

}
