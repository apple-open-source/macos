/*
 * Copyright (c) 2011  Apple Inc. All rights reserved.
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
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <smbfs/smbfs_node.h>
#include <smbfs/smbfs_subr_2.h>
#include <smbfs/smbfs.h>


void
smb2fs_smb_file_id_check(struct smb_share *share, uint64_t ino,
                         char *network_name, uint32_t network_name_len)
{
    uint32_t no_file_ids = 0;
    
    /*
     * Check to see if server supports File IDs or not
     * Watch out because the ".." in every Query Dir response has File ID of 0
     * which is supposed to be illegal. Sigh.
     */
    if (SSTOVC(share)->vc_misc_flags & SMBV_HAS_FILEIDS) {
        if (ino == 0) {
            no_file_ids = 1;
            
            if ((network_name != NULL) && (network_name_len > 0)) {
                if ((network_name_len == 2 &&
                     letohs(*(uint16_t * ) network_name) == 0x002e) ||
                    (network_name_len == 4 &&
                     letohl(*(uint32_t *) network_name) == 0x002e002e)) {
                        /*
                         * Its the ".." dir so allow the File ID of 0. "." and ".."
                         * dirs are ignored by smbfs_findnext so we can safely leave
                         * their fa_ino to be 0
                         */
                        no_file_ids = 0;
                    }
            }
        }
    }
    
    if (no_file_ids == 1) {
        SMBDEBUG("Server does not support File IDs \n");
        SSTOVC(share)->vc_misc_flags &= ~SMBV_HAS_FILEIDS;
    }
}

uint64_t
smb2fs_smb_file_id_get(struct smbmount *smp, uint64_t ino, char *name)
{
    uint64_t ret_ino;
    
    if (ino == smp->sm_root_ino) {
        /* If its the root File ID, then return SMBFS_ROOT_INO */
        ret_ino = SMBFS_ROOT_INO;
    }
    else {
        /*
         * If actual File ID is SMBFS_ROOT_INO, then return the root File ID
         * instead.
         */
        if (ino == SMBFS_ROOT_INO) {
            ret_ino = smp->sm_root_ino;
        }
        else {
            if (ino == 0) {
                /* This should never happen */
                SMBERROR("File ID of 0 in <%s>? \n",
                         ((name != NULL) ? name : "unknown name"));
                ret_ino = SMBFS_ROOT_INO;
            }
            else {
                ret_ino = ino;
            }
        }
    }
    
    return (ret_ino);
}

/*
 * This differs from smbfs_fullpath in
 * 1) no pad byte
 * 2) Unicode is always used
 * 3) no end null bytes
 */
int
smb2fs_fullpath(struct mbchain *mbp, struct smbnode *dnp, 
                const char *namep, size_t in_name_len, 
                const char *strm_namep, size_t in_strm_name_len,
                int name_flags, uint8_t sep_char)
{
	int error = 0; 
	const char *name = (namep ? namep : NULL);
	const char *strm_name = (strm_namep ? strm_namep : NULL);
	size_t name_len = in_name_len;
	size_t strm_name_len = in_strm_name_len;
    size_t len = 0;
    uint8_t stream_sep_char = ':';
    
	if (dnp != NULL) {
		struct smbmount *smp = dnp->n_mount;
		
		error = smb_fphelp(smp, mbp, dnp, TRUE, &len);
		if (error) {
			return error;
        }
	}
    
	if (name) {
        /* Add separator char only if we added a path from above */
        if (len > 0) {
            error = mb_put_uint16le(mbp, sep_char);
            if (error) {
                return error;
            }
        }
        
		error = smb_put_dmem(mbp, name, name_len, name_flags, TRUE, NULL);
		if (error) {
			return error;
        }
	}
    
    /* Add Stream Name */
	if (strm_name) {
        /* Add separator char */
        error = mb_put_uint16le(mbp, stream_sep_char);
        if (error) {
            return error;
        }
        
		error = smb_put_dmem(mbp, strm_name, strm_name_len, name_flags, TRUE, NULL);
		if (error) {
			return error;
        }
	}

	return error;
}
