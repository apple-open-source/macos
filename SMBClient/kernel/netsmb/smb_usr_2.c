/*
 * Copyright (c) 2011-2023  Apple Inc. All rights reserved.
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
smb_usr_check_dir(struct smb_share *share, struct smb_session *sessionp,
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
    size_t network_allocsize = 0;
	char *network_pathp = NULL;
    
    /* Assume no error */
    check_dir_ioc->ioc_ret_ntstatus = 0;
	
	/* Do they want the DFS flag set in the header? */
	if (check_dir_ioc->ioc_flags & SMB_FLAGS2_DFS) {
		create_flags |= SMB2_CREATE_SET_DFS_FLAG;
	}
	
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
        
        /* Validate we get an acceptable name-length from userland */
        if (local_path_len >= PATH_MAX) {
            error = EINVAL;
            goto bad;
        }

        local_pathp = smb_str_memdupin(check_dir_ioc->ioc_kern_path,
                                       local_path_len,
                                       &error);

        if (error) {
            goto bad;
        }
	}

    /*
     * Need to convert from local path to a network path 
     */
    network_allocsize = network_path_len;
    SMB_MALLOC_DATA(network_pathp, network_allocsize, Z_WAITOK_ZERO);
	if (network_pathp == NULL) {
		error = ENOMEM;
		goto bad;		
	}

	error = smb_convert_path_to_network(local_pathp, local_path_len,
                                        network_pathp, &network_path_len,
										'\\', SMB_UTF_SFM_CONVERSIONS,
                                        SMB_UNICODE_STRINGS(sessionp));
	if (error) {
		SMBERROR("smb_convert_path_to_network failed : %d\n", error);
		goto bad;
	}
    
    /* 
     * Set up for Compound Create/Close call 
     */
    SMB_MALLOC_TYPE(fap, struct smbfattr, Z_WAITOK_ZERO);
    if (fap == NULL) {
        SMBERROR("SMB_MALLOC_TYPE failed\n");
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
                                   NULL, NULL,
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
        SMB_FREE_DATA(local_pathp, local_path_len);
    }
    
    if (network_pathp) {
        SMB_FREE_DATA(network_pathp, network_allocsize);
    }

    if (fap) {
        SMB_FREE_TYPE(struct smbfattr, fap);
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
    
    SMB_MALLOC_TYPE(closep, struct smb2_close_rq, Z_WAITOK_ZERO);
    if (closep == NULL) {
		SMBERROR("SMB_MALLOC_TYPE failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    closep->share = share;
    closep->flags = close_ioc->ioc_flags;
    closep->fid = close_ioc->ioc_fid;
    closep->mc_flags = 0;

	/* Now do the real work */
	error = smb2_smb_close(share, closep, NULL, NULL, context);
    
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
        SMB_FREE_TYPE(struct smb2_close_rq, closep);
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
    size_t local_path_len = 0;
    size_t network_path_len = PATH_MAX + 1;
    size_t network_allocsize = 0;
    char *network_pathp = NULL;
    char *local_pathp = NULL;
	
    SMB_MALLOC_TYPE(createp, struct smb2_create_rq, Z_WAITOK_ZERO);
    if (createp == NULL) {
        SMBERROR("SMB_MALLOC_TYPE failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    /* Take the 32 bit world pointers and convert them to user_addr_t. */
    if (!vfs_context_is64bit (context)) {
        create_ioc->ioc_kern_name = CAST_USER_ADDR_T(create_ioc->ioc_name);
    }

    local_path_len = create_ioc->ioc_name_len;
    /* ioc_name_len includes the null byte, ioc_kern_name is a c-style string */
    if (create_ioc->ioc_kern_name && local_path_len) {

        if (local_path_len >= PATH_MAX) {
            error = EINVAL;
            goto bad;
        }
        
        local_pathp = smb_str_memdupin(create_ioc->ioc_kern_name,
                                       create_ioc->ioc_name_len,
                                       &error);

        if (error) {
            goto bad;
        }

        /*
         * Need to convert from local path to a network path
         */
        network_allocsize = network_path_len;
        SMB_MALLOC_DATA(network_pathp, network_allocsize, Z_WAITOK_ZERO);
        if (network_pathp == NULL) {
            error = ENOMEM;
            goto bad;
        }
        
        error = smb_convert_path_to_network(local_pathp, local_path_len,
                                            network_pathp, &network_path_len,
                                            '\\', SMB_UTF_SFM_CONVERSIONS,
                                            1);
        if (error) {
            SMBERROR("smb_convert_path_to_network failed : %d\n", error);
            goto bad;
        }
        
        createp->namep = network_pathp;
        createp->name_len = (uint32_t)network_path_len;
    }
	
#if 0
    /* Security check if its a named pipe being opened */
	if ((local_pathp != NULL) && (local_path_len != 0)) {
		error = smb_check_named_pipe(share, local_pathp, local_path_len);
		if (error) {
			goto bad;
		}
	}
#endif
    
    createp->flags = SMB2_CREATE_GET_MAX_ACCESS | SMB2_CREATE_NAME_IS_PATH;
    createp->oplock_level = create_ioc->ioc_oplock_level;
    createp->impersonate_level = create_ioc->ioc_impersonate_level;
    createp->desired_access = create_ioc->ioc_desired_access;
    createp->file_attributes = create_ioc->ioc_file_attributes;
    createp->share_access = create_ioc->ioc_share_access;
    createp->disposition = create_ioc->ioc_disposition;
    createp->create_options = create_ioc->ioc_create_options;
    createp->dnp = NULL;
    createp->mc_flags = 0;
    
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
        SMB_FREE_TYPE(struct smb2_create_rq, createp);
    }
    
    if (network_pathp) {
        SMB_FREE_DATA(network_pathp, network_allocsize);
    }
    
    if (local_pathp) {
        SMB_FREE_DATA(local_pathp, local_path_len);
    }
    
    return error;
}

int
smb_usr_get_dfs_referral(struct smb_share *share, struct smb_session *sessionp,
                         struct smb2ioc_get_dfs_referral *get_dfs_refer_ioc,
                         vfs_context_t context)
{
	int error;
 	struct smb2_ioctl_rq *ioctlp = NULL;
 	struct smb2_get_dfs_referral dfs_referral;
    char *local_pathp = NULL;
    uint32_t local_path_len = get_dfs_refer_ioc->ioc_file_name_len;
	size_t network_path_len = PATH_MAX + 1;
    size_t network_allocsize = 0;
	char *network_pathp = NULL;

    SMB_MALLOC_TYPE(ioctlp, struct smb2_ioctl_rq, Z_WAITOK_ZERO);
    if (ioctlp == NULL) {
		SMBERROR("SMB_MALLOC_TYPE failed\n");
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

	/* ioc_file_name_len includes the null byte, ioc_kern_file_name
     * is a c-style string
     */
	if (get_dfs_refer_ioc->ioc_kern_file_name && local_path_len) {

        if (local_path_len >= PATH_MAX) {
            error = EINVAL;
            goto bad;
        }

		local_pathp = smb_str_memdupin(get_dfs_refer_ioc->ioc_kern_file_name,
                                       local_path_len,
                                       &error);

        if (error) {
            goto bad;
        }
	}

    /*
     * Need to convert from local path to a network path 
     */
    network_allocsize = network_path_len;
    SMB_MALLOC_DATA(network_pathp, network_allocsize, Z_WAITOK_ZERO);
	if (network_pathp == NULL) {
		error = ENOMEM;
		goto bad;		
	}
    
	error = smb_convert_path_to_network(local_pathp, local_path_len,
                                        network_pathp, &network_path_len,
										'\\', SMB_UTF_SFM_CONVERSIONS,
                                        SMB_UNICODE_STRINGS(sessionp));
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
    ioctlp->mc_flags = 0;

	ioctlp->snd_input_buffer = (uint8_t *) &dfs_referral;
	ioctlp->snd_input_len = sizeof(struct smb2_get_dfs_referral);
	ioctlp->snd_output_len = 0;
    
	ioctlp->rcv_input_len = 0;
    
    /* Handle servers that dislike large output buffer lengths */
    if (sessionp->session_misc_flags & SMBV_63K_IOCTL) {
        ioctlp->rcv_output_len = kSMB_63K;
    }
    else {
        ioctlp->rcv_output_len = get_dfs_refer_ioc->ioc_rcv_output_len;
    }

    /* Now do the real work */
	error = smb2_smb_ioctl(share, NULL, ioctlp, NULL, context);
    
    if ((error) &&
        (ioctlp->ret_ntstatus == STATUS_INVALID_PARAMETER) &&
        !(sessionp->session_misc_flags & SMBV_63K_IOCTL)) {
        /*
         * <14281932> Could this be a server that can not handle
         * larger than 65535 bytes in an IOCTL? 
         */
        SMBWARNING("SMB 2/3 server cant handle large OutputBufferLength in DFS Referral. Reducing to 63Kb.\n");
        sessionp->session_misc_flags |= SMBV_63K_IOCTL;
        
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
        SMB_FREE_TYPE(struct smb2_ioctl_rq, ioctlp);
    }
    
    if (local_pathp) {
        SMB_FREE_DATA(local_pathp, local_path_len);
    }
    
    if (network_pathp) {
        SMB_FREE_DATA(network_pathp, network_allocsize);
    }
    return error;
}

int
smb_usr_ioctl(struct smb_share *share, struct smb_session *sessionp,
              struct smb2ioc_ioctl *ioctl_ioc, vfs_context_t context)
{
	int error;
 	struct smb2_ioctl_rq *ioctlp = NULL;

    SMB_MALLOC_TYPE(ioctlp, struct smb2_ioctl_rq, Z_WAITOK_ZERO);
    if (ioctlp == NULL) {
		SMBERROR("SMB_MALLOC_TYPE failed\n");
        error = ENOMEM;
        goto bad;
    }
    
again:
    ioctlp->share = share;
    ioctlp->ctl_code = ioctl_ioc->ioc_ctl_code;
    ioctlp->fid = ioctl_ioc->ioc_fid;
    ioctlp->mc_flags = 0;

	ioctlp->snd_input_len = ioctl_ioc->ioc_snd_input_len;
	ioctlp->snd_output_len = ioctl_ioc->ioc_snd_output_len;
	ioctlp->rcv_input_len = ioctl_ioc->ioc_rcv_input_len;

    /* Handle servers that dislike large output buffer lengths */
    if (sessionp->session_misc_flags & SMBV_63K_IOCTL) {
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
	error = smb2_smb_ioctl(share, NULL, ioctlp, NULL, context);

    if ((error) &&
        (ioctlp->ret_ntstatus == STATUS_INVALID_PARAMETER) &&
        !(sessionp->session_misc_flags & SMBV_63K_IOCTL)) {
        /*
         * <14281932> Could this be a server that can not handle
         * larger than 65535 bytes in an IOCTL?
         */
        SMBWARNING("SMB 2/3 server cant handle large OutputBufferLength in IOCTL. Reducing to 63Kb.\n");
        sessionp->session_misc_flags |= SMBV_63K_IOCTL;
        
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
        SMB_FREE_TYPE(struct smb2_ioctl_rq, ioctlp);
    }
    return error;
}

/*
 * Called from user land so we always have a reference on the share.
 */
int
smb_usr_query_dir(struct smb_share *share,
                  struct smb2ioc_query_dir *query_dir_ioc,
                  vfs_context_t context)
{
	int error;
 	struct smb2_query_dir_rq *queryp = NULL;

    SMB_MALLOC_TYPE(queryp, struct smb2_query_dir_rq, Z_WAITOK_ZERO);
    if (queryp == NULL) {
		SMBERROR("SMB_MALLOC_TYPE failed\n");
        error = ENOMEM;
        goto bad;
    }
    
    /* Take 32 bit world pointers and convert them to user_addr_t. */
    if (query_dir_ioc->ioc_rcv_output_len > 0) {
        if (vfs_context_is64bit(context)) {
            queryp->rcv_output_uio = uio_create(1, 0, UIO_USERSPACE64, UIO_READ);
        }
        else {
            query_dir_ioc->ioc_kern_rcv_output =
            CAST_USER_ADDR_T(query_dir_ioc->ioc_rcv_output);
            queryp->rcv_output_uio = uio_create(1, 0, UIO_USERSPACE32, UIO_READ);
        }
        
        if (queryp->rcv_output_uio) {
            uio_addiov(queryp->rcv_output_uio,
                       query_dir_ioc->ioc_kern_rcv_output,
                       query_dir_ioc->ioc_rcv_output_len);
        }
        else {
            SMBERROR("uio_create failed\n");
            error = ENOMEM;
            goto bad;
        }
    }

	/* Take the 32 bit world pointers and convert them to user_addr_t. */
	if (!vfs_context_is64bit (context)) {
		query_dir_ioc->ioc_kern_name = CAST_USER_ADDR_T(query_dir_ioc->ioc_name);
	}

    queryp->name_len = query_dir_ioc->ioc_name_len;
	/* ioc_name_len includes the null byte, ioc_kern_name is a c-style string */
	if (query_dir_ioc->ioc_kern_name && queryp->name_len) {

        if (queryp->name_len >= PATH_MAX) {
            error = EINVAL;
            goto bad;
        }

		queryp->namep = smb_str_memdupin(query_dir_ioc->ioc_kern_name,
                                         queryp->name_len,
                                         &error);
        queryp->name_allocsize = queryp->name_len;

		if (error) {
			goto bad;
		}
	}

	queryp->file_info_class = query_dir_ioc->ioc_file_info_class;
	queryp->flags = query_dir_ioc->ioc_flags;
	queryp->file_index = query_dir_ioc->ioc_file_index;
	queryp->output_buffer_len = query_dir_ioc->ioc_rcv_output_len;
	queryp->fid = query_dir_ioc->ioc_fid;
	queryp->name_len = query_dir_ioc->ioc_name_len;
	queryp->name_flags = query_dir_ioc->ioc_name_flags;
    queryp->mc_flags = 0;
    /* 
     * Never used for user ioctl query dir. User must have already opened
     * the dir to be searched.
     */
	queryp->dnp = NULL; 
    
    /* 
     * Since this is from user space, there is no mounted file system, so 
     * there are no vnodes and thus no queryp->dnp. This means that namep 
     * must be non NULL.
     *
     * If ioc_rcv_output_len is not 0, then copy results directly to user 
     * buffer and let them parse it.
     */
    if ((queryp->namep == NULL) || (queryp->name_len == 0)) {
        SMBERROR("missing name \n");
        error = EINVAL;
        goto bad;
    }
    
	/* Now do the real work */
	error = smb2_smb_query_dir(share, queryp, NULL, NULL, context);
    
    /* always return the ntstatus error */
    query_dir_ioc->ioc_ret_ntstatus = queryp->ret_ntstatus;
	if (error) {
		goto bad;
	}
    
    /* Fill in amount of data returned in Query Dir reply */
    query_dir_ioc->ioc_ret_output_len = queryp->ret_buffer_len;
    
    /* Fill in actual amount of data returned */
    query_dir_ioc->ioc_rcv_output_len = queryp->output_buffer_len;
    
bad:
    if (queryp != NULL) {
        if (queryp->ret_rqp != NULL) {
            smb_rq_done(queryp->ret_rqp);
        }
        if (queryp->namep)
            SMB_FREE_DATA(queryp->namep, queryp->name_allocsize);
        if (queryp->rcv_output_uio != NULL) {
            uio_free(queryp->rcv_output_uio);
        }
        SMB_FREE_TYPE(struct smb2_query_dir_rq, queryp);
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
    uint32_t allow_compression = 0;
    
    SMB_MALLOC_TYPE(read_writep, struct smb2_rw_rq, Z_WAITOK_ZERO);
    if (read_writep == NULL) {
        SMBERROR("SMB_MALLOC_TYPE failed\n");
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
        read_writep->mc_flags = 0;
        
        /*
         * Now do the real work
         * Do not allow read/write compression from user space since I have
         * no idea the type of file being accessed.
         */
        if (cmd == SMB2IOC_READ) {
            error = smb2_smb_read(share, read_writep, allow_compression, context);
        }
        else {
            error = smb2_smb_write(share, read_writep, &allow_compression, context);
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
        SMB_FREE_TYPE(struct smb2_rw_rq, read_writep);
    }
    
	return error;
}

