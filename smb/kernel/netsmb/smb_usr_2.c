/*
 * Copyright (c) 2011-2012  Apple Inc. All rights reserved.
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
#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_conn_2.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_dev.h>
#include <netsmb/smb_dev_2.h>
#include <netsmb/smb_converter.h>
#include <smbfs/smbfs_subr.h>
#include <smbfs/smbfs_subr_2.h>
#include <smbclient/ntstatus.h>
#include <sys/msfscc.h>

/*
 * Note:  The _usr_ in the function name indicates that these functions are 
 * called by from user land, typically via ioctls, to do work in the kernel.
 * This way packets are only marshalled in one spot and that is in the kernel.
 *
 * These take in the user space arguments, put them into a _smb_ call structure,
 * call the _smb_ function, then return the results back in the user space
 * arguments.
 */


/*
 * Called from user land so we always have a reference on the share.
 */
int 
smb_usr_check_dir(struct smb_share *share, struct smb_vc *vcp,
                  struct smb2ioc_check_dir *check_dir_ioc,
                  vfs_context_t context)
{
	int error;
    char *local_pathp = NULL;
    uint32_t local_path_len = check_dir_ioc->ioc_path_len;
    uint32_t desired_access = SMB2_FILE_READ_ATTRIBUTES | SMB2_SYNCHRONIZE;
    uint32_t share_access = NTCREATEX_SHARE_ACCESS_ALL;
    /* Tell create, the namep is a path to an item */
    uint64_t create_flags = SMB2_CREATE_NAME_IS_PATH;
    struct smbfattr *fap = NULL;
	size_t network_path_len = PATH_MAX + 1;
	char *network_pathp = NULL;
    
    /* Assume no error */
    check_dir_ioc->ioc_ret_ntstatus = 0;
        
    /* 
     * Compound Create/Close call should be sufficient. 
     * If item exists, verify it is a dir.
     */
    
	/* Take the 32 bit world pointers and convert them to user_addr_t. */
	if (!vfs_context_is64bit(context)) {
		check_dir_ioc->ioc_kern_path = CAST_USER_ADDR_T(check_dir_ioc->ioc_path);
	}
	
    if (!(check_dir_ioc->ioc_kern_path)) {
        error = EINVAL;
        goto bad;
    }

    /* local_path_len includes the null byte, ioc_kern_path is a c-style string */
	if (check_dir_ioc->ioc_kern_path && local_path_len) {
		local_pathp = smb_memdupin(check_dir_ioc->ioc_kern_path,
                                   local_path_len);
	}

    if (local_pathp == NULL) {
        SMBERROR("smb_memdupin failed\n");
        error = ENOMEM;
        goto bad;
    }

    /*
     * Need to convert from local path to a network path 
     */
	SMB_MALLOC(network_pathp, char *, network_path_len, M_TEMP, M_WAITOK | M_ZERO);
	if (network_pathp == NULL) {
		error = ENOMEM;
		goto bad;		
	}

	error = smb_convert_path_to_network(local_pathp, local_path_len,
                                        network_pathp, &network_path_len,
										'\\', SMB_UTF_SFM_CONVERSIONS,
                                        SMB_UNICODE_STRINGS(vcp));
	if (error) {
		SMBERROR("smb_convert_path_to_network failed : %d\n", error);
		goto bad;
	}
    
    /* 
     * Set up for Compound Create/Close call 
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

    /* Send a Create/Close */
    error = smb2fs_smb_cmpd_create(share, NULL,
                                   network_pathp, network_path_len,
                                   NULL, 0,
                                   desired_access, VDIR,
                                   share_access, FILE_OPEN,
                                   create_flags, &check_dir_ioc->ioc_ret_ntstatus,
                                   NULL, fap,
                                   NULL, context);
	if (error) {
		goto bad;
	}
    
    /* found something, verify its a dir */
    if (!(fap->fa_attr & SMB_EFA_DIRECTORY)) {
        error = ENOTDIR;
        check_dir_ioc->ioc_ret_ntstatus = STATUS_NOT_A_DIRECTORY;
    }

bad:    
    if (local_pathp) {
        SMB_FREE(local_pathp, M_SMBSTR);
    }
    
    if (network_pathp) {
        SMB_FREE(network_pathp, M_SMBSTR);
    }

    if (fap) {
        SMB_FREE(fap, M_SMBTEMP);
    }

	return error;
}

/*
 * Called from user land so we always have a reference on the share.
 */
int 
smb_usr_close(struct smb_share *share, struct smb2ioc_close *close_ioc, vfs_context_t context)
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
    closep->flags = close_ioc->ioc_flags;
    closep->fid = close_ioc->ioc_fid;
    
	/* Now do the real work */
	error = smb2_smb_close(share, closep, NULL, context);
    
    /* always return the ntstatus error */
    close_ioc->ioc_ret_ntstatus = closep->ret_ntstatus;
	if (error) {
		goto bad;
	}
    
    /* Fill in return parameters if they wanted them */
    if (close_ioc->ioc_flags & SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB) {
        close_ioc->ioc_ret_attributes = closep->ret_attributes;
        close_ioc->ioc_ret_create_time = closep->ret_create_time;
        close_ioc->ioc_ret_access_time = closep->ret_access_time;
        close_ioc->ioc_ret_write_time = closep->ret_write_time;
        close_ioc->ioc_ret_change_time = closep->ret_change_time;
        close_ioc->ioc_ret_alloc_size = closep->ret_alloc_size;
        close_ioc->ioc_ret_eof = closep->ret_eof;
    }
    
bad:    
    if (closep != NULL) {
        SMB_FREE(closep, M_SMBTEMP);
    }
    
	return error;
}

/*
 * Called from user land so we always have a reference on the share.
 */
int 
smb_usr_create(struct smb_share *share, struct smb2ioc_create *create_ioc,
               vfs_context_t context)
{
	int error;
 	struct smb2_create_rq *createp = NULL;
    
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
    
	/* Take the 32 bit world pointers and convert them to user_addr_t. */
	if (!vfs_context_is64bit (context)) {
		create_ioc->ioc_kern_name = CAST_USER_ADDR_T(create_ioc->ioc_name);
	}
	
	/* ioc_name_len includes the null byte, ioc_kern_name is a c-style string */
	if (create_ioc->ioc_kern_name && create_ioc->ioc_name_len) {
        createp->name_len = create_ioc->ioc_name_len;
		createp->namep = smb_memdupin(create_ioc->ioc_kern_name, 
                                   create_ioc->ioc_name_len);
		if (createp->namep == NULL) {
            SMBERROR("smb_memdupin failed\n");
			error = ENOMEM;
			goto bad;
		}
	}
    
    createp->flags = SMB2_CREATE_GET_MAX_ACCESS;
    createp->oplock_level = create_ioc->ioc_oplock_level;
    createp->impersonate_level = create_ioc->ioc_impersonate_level;
    createp->desired_access = create_ioc->ioc_desired_access;
    createp->file_attributes = create_ioc->ioc_file_attributes;
    createp->share_access = create_ioc->ioc_share_access;
    createp->disposition = create_ioc->ioc_disposition;
    createp->create_options = create_ioc->ioc_create_options;
    createp->dnp = NULL;
    
	/* Now do the real work */
	error = smb2_smb_create(share, createp, NULL, context);
    
    /* always return the ntstatus error */
    create_ioc->ioc_ret_ntstatus = createp->ret_ntstatus;
	if (error) {
		goto bad;
	}
    
    /* Fill in return parameters */
    create_ioc->ioc_ret_attributes = createp->ret_attributes;
    create_ioc->ioc_ret_oplock_level = createp->ret_oplock_level;
    create_ioc->ioc_ret_create_action = createp->ret_create_action;
    create_ioc->ioc_ret_create_time = createp->ret_create_time;
    create_ioc->ioc_ret_access_time = createp->ret_access_time;
    create_ioc->ioc_ret_write_time = createp->ret_write_time;
    create_ioc->ioc_ret_change_time = createp->ret_change_time;
    create_ioc->ioc_ret_alloc_size = createp->ret_alloc_size;
    create_ioc->ioc_ret_eof = createp->ret_eof;
    create_ioc->ioc_ret_fid = createp->ret_fid;
    create_ioc->ioc_ret_max_access = createp->ret_max_access;
    
bad:    
    if (createp != NULL) {
        if (createp->namep)
            SMB_FREE(createp->namep, M_SMBSTR);
        SMB_FREE(createp, M_SMBTEMP);
    }
    
	return error;
}

int
smb_usr_get_dfs_referral(struct smb_share *share, struct smb_vc *vcp,
                         struct smb2ioc_get_dfs_referral *get_dfs_refer_ioc,
                         vfs_context_t context)
{
	int error;
 	struct smb2_ioctl_rq *ioctlp = NULL;
 	struct smb2_get_dfs_referral dfs_referral;
    char *local_pathp = NULL;
    uint32_t local_path_len = get_dfs_refer_ioc->ioc_file_name_len;
	size_t network_path_len = PATH_MAX + 1;
	char *network_pathp = NULL;
    
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

again:
	/* Take the 32 bit world pointers and convert them to user_addr_t. */
    bzero(&dfs_referral, sizeof(dfs_referral));

    dfs_referral.file_namep = NULL;
    dfs_referral.max_referral_level = get_dfs_refer_ioc->ioc_max_referral_level;
    
	if (!vfs_context_is64bit (context)) {
		get_dfs_refer_ioc->ioc_kern_file_name = CAST_USER_ADDR_T(get_dfs_refer_ioc->ioc_file_name);
	}
	
    if (!(get_dfs_refer_ioc->ioc_kern_file_name)) {
        error = EINVAL;
        goto bad;
    }

	/* ioc_file_name_len includes the null byte, ioc_kern_file_name is a c-style string */
	if (get_dfs_refer_ioc->ioc_kern_file_name && get_dfs_refer_ioc->ioc_file_name_len) {
		local_pathp = smb_memdupin(get_dfs_refer_ioc->ioc_kern_file_name,
                                   local_path_len);
	}

    if (local_pathp == NULL) {
        SMBERROR("smb_memdupin failed\n");
        error = ENOMEM;
        goto bad;
    }

    /*
     * Need to convert from local path to a network path 
     */
	SMB_MALLOC(network_pathp, char *, network_path_len, M_TEMP, M_WAITOK | M_ZERO);
	if (network_pathp == NULL) {
		error = ENOMEM;
		goto bad;		
	}
    
	error = smb_convert_path_to_network(local_pathp, local_path_len,
                                        network_pathp, &network_path_len,
										'\\', SMB_UTF_SFM_CONVERSIONS,
                                        SMB_UNICODE_STRINGS(vcp));
	if (error) {
		SMBERROR("smb_convert_path_to_network failed : %d\n", error);
		goto bad;
	}

    dfs_referral.file_namep = network_pathp;
    dfs_referral.file_name_len = (uint32_t) network_path_len;
    
    /* Take 32 bit world pointers and convert them to user_addr_t. */
    if (get_dfs_refer_ioc->ioc_rcv_output_len > 0) {
        if (vfs_context_is64bit(context)) {
            ioctlp->rcv_output_uio = uio_create(1, 0, UIO_USERSPACE64, UIO_READ);
        }
        else {
            get_dfs_refer_ioc->ioc_kern_rcv_output = CAST_USER_ADDR_T(get_dfs_refer_ioc->ioc_rcv_output);
            ioctlp->rcv_output_uio = uio_create(1, 0, UIO_USERSPACE32, UIO_READ);
        }
        
        if (ioctlp->rcv_output_uio) {
            uio_addiov(ioctlp->rcv_output_uio,
                       get_dfs_refer_ioc->ioc_kern_rcv_output, 
                       get_dfs_refer_ioc->ioc_rcv_output_len);
        } 
        else {
            error = ENOMEM;
            SMBERROR("uio_create failed\n");
            goto bad;
        }
    }

    ioctlp->share = share;
    ioctlp->ctl_code = FSCTL_DFS_GET_REFERRALS;
    ioctlp->fid = -1;
    
	ioctlp->snd_input_buffer = (uint8_t *) &dfs_referral;
	ioctlp->snd_input_len = sizeof(struct smb2_get_dfs_referral);
	ioctlp->snd_output_len = 0;
    
	ioctlp->rcv_input_len = 0;
    
    /* Handle servers that dislike large output buffer lengths */
    if (vcp->vc_misc_flags & SMBV_63K_IOCTL) {
        ioctlp->rcv_output_len = kSMB_63K;
    }
    else {
        ioctlp->rcv_output_len = get_dfs_refer_ioc->ioc_rcv_output_len;
    }

    /* Now do the real work */
	error = smb2_smb_ioctl(share, ioctlp, NULL, context);
    
    if ((error) &&
        (ioctlp->ret_ntstatus == STATUS_INVALID_PARAMETER) &&
        !(vcp->vc_misc_flags & SMBV_63K_IOCTL)) {
        /*
         * <14281932> Could this be a server that can not handle
         * larger than 65535 bytes in an IOCTL? 
         */
        SMBWARNING("SMB 2.x server cant handle large OutputBufferLength in DFS Referral. Reducing to 63Kb.\n");
        vcp->vc_misc_flags |= SMBV_63K_IOCTL;
        
        ioctlp->ret_ntstatus = 0;
        
        if (ioctlp->snd_input_uio != NULL) {
            uio_free(ioctlp->snd_input_uio);
            ioctlp->snd_input_uio = NULL;
        }
        if (ioctlp->snd_output_uio != NULL) {
            uio_free(ioctlp->snd_output_uio);
            ioctlp->snd_output_uio = NULL;
        }
        if (ioctlp->rcv_input_uio != NULL) {
            uio_free(ioctlp->rcv_input_uio);
            ioctlp->rcv_input_uio = NULL;
        }
        if (ioctlp->rcv_output_uio != NULL) {
            uio_free(ioctlp->rcv_output_uio);
            ioctlp->rcv_output_uio = NULL;
        }

        goto again;
    }    
    
    /* always return the ntstatus error */
    get_dfs_refer_ioc->ioc_ret_ntstatus = ioctlp->ret_ntstatus;
	if (error) {
		goto bad;
	}
    
    /* Fill in actual bytes returned */
    get_dfs_refer_ioc->ioc_ret_output_len = ioctlp->ret_output_len;
    
bad:    
    if (ioctlp != NULL) {
        if (ioctlp->snd_input_uio != NULL) {
            uio_free(ioctlp->snd_input_uio);
        }
        if (ioctlp->snd_output_uio != NULL) {
            uio_free(ioctlp->snd_output_uio);
        }
        if (ioctlp->rcv_input_uio != NULL) {
            uio_free(ioctlp->rcv_input_uio);
        }
        if (ioctlp->rcv_output_uio != NULL) {
            uio_free(ioctlp->rcv_output_uio);
        }
        SMB_FREE(ioctlp, M_SMBTEMP);
    }
    
    if (local_pathp) {
        SMB_FREE(local_pathp, M_SMBSTR);
    }
    
    if (network_pathp) {
        SMB_FREE(network_pathp, M_SMBSTR);
    }

    return error;
}

int
smb_usr_ioctl(struct smb_share *share, struct smb_vc *vcp,
              struct smb2ioc_ioctl *ioctl_ioc, vfs_context_t context)
{
	int error;
 	struct smb2_ioctl_rq *ioctlp = NULL;
    
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
    
again:
    ioctlp->share = share;
    ioctlp->ctl_code = ioctl_ioc->ioc_ctl_code;
    ioctlp->fid = ioctl_ioc->ioc_fid;
    
	ioctlp->snd_input_len = ioctl_ioc->ioc_snd_input_len;
	ioctlp->snd_output_len = ioctl_ioc->ioc_snd_output_len;
	ioctlp->rcv_input_len = ioctl_ioc->ioc_rcv_input_len;

    /* Handle servers that dislike large output buffer lengths */
    if (vcp->vc_misc_flags & SMBV_63K_IOCTL) {
        ioctlp->rcv_output_len = kSMB_63K;
    }
    else {
        ioctlp->rcv_output_len = ioctl_ioc->ioc_rcv_output_len;
    }

    /* Take 32 bit world pointers and convert them to user_addr_t. */
    if (ioctl_ioc->ioc_snd_input_len > 0) {
        if (vfs_context_is64bit(context)) {
            ioctlp->snd_input_uio = uio_create(1, 0, UIO_USERSPACE64, UIO_WRITE);
        }
        else {
            ioctl_ioc->ioc_kern_snd_input = 
                CAST_USER_ADDR_T(ioctl_ioc->ioc_snd_input);
            ioctlp->snd_input_uio = uio_create(1, 0, UIO_USERSPACE32, UIO_WRITE);
        }
        
        if (ioctlp->snd_input_uio) {
            uio_addiov(ioctlp->snd_input_uio,
                       ioctl_ioc->ioc_kern_snd_input, 
                       ioctl_ioc->ioc_snd_input_len);
        } 
        else {
            SMBERROR("uio_create failed\n");
            error = ENOMEM;
            goto bad;
        }
    }
    
    /* Take 32 bit world pointers and convert them to user_addr_t. */
    if (ioctl_ioc->ioc_snd_output_len > 0) {
        if (vfs_context_is64bit(context)) {
            ioctlp->snd_output_uio = uio_create(1, 0, UIO_USERSPACE64, UIO_WRITE);
        }
        else {
            ioctl_ioc->ioc_kern_snd_output =  
                CAST_USER_ADDR_T(ioctl_ioc->ioc_snd_output);
            ioctlp->snd_output_uio = uio_create(1, 0, UIO_USERSPACE32, UIO_WRITE);
        }
        
        if (ioctlp->snd_output_uio) {
            uio_addiov(ioctlp->snd_output_uio,
                       ioctl_ioc->ioc_kern_snd_output, 
                       ioctl_ioc->ioc_snd_output_len);
        } 
        else {
            SMBERROR("uio_create failed\n");
            error = ENOMEM;
            goto bad;
        }
    }
    
    /* Take 32 bit world pointers and convert them to user_addr_t. */
    if (ioctl_ioc->ioc_rcv_input_len > 0) {
        if (vfs_context_is64bit(context)) {
            ioctlp->rcv_input_uio = uio_create(1, 0, UIO_USERSPACE64, UIO_READ);
        }
        else {
            ioctl_ioc->ioc_kern_rcv_input = 
                CAST_USER_ADDR_T(ioctl_ioc->ioc_rcv_input);
            ioctlp->rcv_input_uio = uio_create(1, 0, UIO_USERSPACE32, UIO_READ);
        }
        
        if (ioctlp->rcv_input_uio) {
            uio_addiov(ioctlp->rcv_input_uio,
                       ioctl_ioc->ioc_kern_rcv_input, 
                       ioctl_ioc->ioc_rcv_input_len);
        } 
        else {
            SMBERROR("uio_create failed\n");
            error = ENOMEM;
            goto bad;
        }
    }

    /* Take 32 bit world pointers and convert them to user_addr_t. */
    if (ioctl_ioc->ioc_rcv_output_len > 0) {
        if (vfs_context_is64bit(context)) {
            ioctlp->rcv_output_uio = uio_create(1, 0, UIO_USERSPACE64, UIO_READ);
        }
        else {
            ioctl_ioc->ioc_kern_rcv_output = 
                CAST_USER_ADDR_T(ioctl_ioc->ioc_rcv_output);
            ioctlp->rcv_output_uio = uio_create(1, 0, UIO_USERSPACE32, UIO_READ);
        }
        
        if (ioctlp->rcv_output_uio) {
            uio_addiov(ioctlp->rcv_output_uio,
                       ioctl_ioc->ioc_kern_rcv_output, 
                       ioctl_ioc->ioc_rcv_output_len);
        } 
        else {
            SMBERROR("uio_create failed\n");
            error = ENOMEM;
            goto bad;
        }
    }
    
    /* Now do the real work */
	error = smb2_smb_ioctl(share, ioctlp, NULL, context);

    if ((error) &&
        (ioctlp->ret_ntstatus == STATUS_INVALID_PARAMETER) &&
        !(vcp->vc_misc_flags & SMBV_63K_IOCTL)) {
        /*
         * <14281932> Could this be a server that can not handle
         * larger than 65535 bytes in an IOCTL?
         */
        SMBWARNING("SMB 2.x server cant handle large OutputBufferLength in IOCTL. Reducing to 63Kb.\n");
        vcp->vc_misc_flags |= SMBV_63K_IOCTL;
        
        ioctlp->ret_ntstatus = 0;
        
        if (ioctlp->snd_input_uio != NULL) {
            uio_free(ioctlp->snd_input_uio);
            ioctlp->snd_input_uio = NULL;
        }
        if (ioctlp->snd_output_uio != NULL) {
            uio_free(ioctlp->snd_output_uio);
            ioctlp->snd_output_uio = NULL;
        }
        if (ioctlp->rcv_input_uio != NULL) {
            uio_free(ioctlp->rcv_input_uio);
            ioctlp->rcv_input_uio = NULL;
        }
        if (ioctlp->rcv_output_uio != NULL) {
            uio_free(ioctlp->rcv_output_uio);
            ioctlp->rcv_output_uio = NULL;
        }

        goto again;
    }

    /* always return the ntstatus error */
    ioctl_ioc->ioc_ret_ntstatus = ioctlp->ret_ntstatus;
	if (error) {
		goto bad;
	}
    
    /* Fill in actual bytes returned */
    ioctl_ioc->ioc_ret_input_len = ioctlp->ret_input_len;
    ioctl_ioc->ioc_ret_output_len = ioctlp->ret_output_len;
    
bad:    
    if (ioctlp != NULL) {
        if (ioctlp->snd_input_uio != NULL) {
            uio_free(ioctlp->snd_input_uio);
        }
        if (ioctlp->snd_output_uio != NULL) {
            uio_free(ioctlp->snd_output_uio);
        }
        if (ioctlp->rcv_input_uio != NULL) {
            uio_free(ioctlp->rcv_input_uio);
        }
        if (ioctlp->rcv_output_uio != NULL) {
            uio_free(ioctlp->rcv_output_uio);
        }
        SMB_FREE(ioctlp, M_SMBTEMP);
    }
    
    return error;
}

/*
 * Called from user land so we always have a reference on the share.
 */
int 
smb_usr_read_write(struct smb_share *share, u_long cmd, struct smb2ioc_rw *rw_ioc, vfs_context_t context)
{
	int error;
 	struct smb2_rw_rq *read_writep = NULL;
    
    SMB_MALLOC(read_writep,
               struct smb2_rw_rq *, 
               sizeof(struct smb2_rw_rq), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (read_writep == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    /* Take 32 bit world pointers and convert them to user_addr_t. */
    if (vfs_context_is64bit(context)) {
        read_writep->auio = uio_create(1,
                            rw_ioc->ioc_offset,
                            UIO_USERSPACE64,
                            (cmd == SMB2IOC_READ) ? UIO_READ : UIO_WRITE);
    }
    else {
        rw_ioc->ioc_kern_base = CAST_USER_ADDR_T(rw_ioc->ioc_base);
        read_writep->auio = uio_create(1,
                            rw_ioc->ioc_offset,
                            UIO_USERSPACE32,
                            (cmd == SMB2IOC_READ) ? UIO_READ : UIO_WRITE);
    }
    
    if (read_writep->auio) {
        /* <14516550> All IO requests from user space are done synchronously */
        read_writep->flags |= SMB2_SYNC_IO;

        uio_addiov(read_writep->auio, rw_ioc->ioc_kern_base, rw_ioc->ioc_len);
        
        read_writep->remaining = rw_ioc->ioc_remaining;
        read_writep->write_flags = rw_ioc->ioc_write_flags;
        read_writep->fid = rw_ioc->ioc_fid;
        
        /* Now do the real work */
        if (cmd == SMB2IOC_READ) {
            error = smb2_smb_read(share, read_writep, context);
        } 
        else {
            error = smb2_smb_write(share, read_writep, context);
        }
        
        /* always return the ntstatus error */
        rw_ioc->ioc_ret_ntstatus = read_writep->ret_ntstatus;
        if (error) {
            goto bad;
        }
        
        /* Fill in actual bytes read or written */
        rw_ioc->ioc_ret_len = read_writep->ret_len;
    } 
    else {
        error = ENOMEM;
    }
    
bad:    
    if (read_writep != NULL) {
        if (read_writep->auio != NULL) {
            uio_free(read_writep->auio);
        }
        SMB_FREE(read_writep, M_SMBTEMP);
    }
    
	return error;
}

