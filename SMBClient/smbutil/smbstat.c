/*
 * Copyright (c) 2021 Apple Inc. All rights reserved
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Apple Inc.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/errno.h>
#include <stdio.h>
#include <sys/vnode.h>

#include <netsmb/smb_2.h>
#include <smbclient/smbclient_internal.h>
#include <netsmb/smbio_2.h>
#include "common.h"
#include <smbfs/smbfs.h>
#include <json_support.h>


CFMutableDictionaryRef smbStats = NULL;
char objType[10][10] = { "none", "VREG", "VDIR", "VBLK", "VCHR", "VLNK", "VSOCK", "VFIFO" };
char leaseState[10][25] = { "none", "read", "handle", "read/handle", "write", "read/write", "write/handle", "read/write/handle" };
char fileFlags[10][25] = { "none", "UBC cached" };
char dirFlags[10][25] = { "none", "enum cached" };
char enumFlags[10][25] = { "none", "completed", "partial", "completed/partial", "dirty"};

static int
get_acces_mode_str(uint32_t access_mode, char *out_str, size_t str_buf_size) {
    int error = 0;
    
    switch(access_mode) {
        case 0:
            snprintf(out_str, str_buf_size, "none");
            return(error);
        case 1:
            snprintf(out_str, str_buf_size, "read");
            return(error);
        case 2:
            snprintf(out_str, str_buf_size, "write");
            return(error);
        case 3:
            snprintf(out_str, str_buf_size, "read/write");
            return(error);
        case 0x11:
            snprintf(out_str, str_buf_size, "read/denyRead");
            return(error);
        case 0x21:
            snprintf(out_str, str_buf_size, "read/denyWrite");
            return(error);
        case 0x31:
            snprintf(out_str, str_buf_size, "read/denyRead/denyWrite");
            return(error);
        case 0x12:
            snprintf(out_str, str_buf_size, "write/denyRead");
            return(error);
        case 0x22:
            snprintf(out_str, str_buf_size, "write/denyWrite");
            return(error);
        case 0x32:
            snprintf(out_str, str_buf_size, "write/denyRead/denyWrite");
            return(error);
        case 0x13:
            snprintf(out_str, str_buf_size, "read/write/denyRead");
            return(error);
        case 0x23:
            snprintf(out_str, str_buf_size, "read/write/denyWrite");
            return(error);
        case 0x33:
            snprintf(out_str, str_buf_size, "read/write/denyRead/denyWrite");
            return(error);
        default:
            snprintf(out_str, str_buf_size, "unknown");
            return(error);
    }
}

static void
add_str(const char *add_str, char *out_str, size_t str_buf_size) {
    /* Are we the first string to be added? */
    if (strlen(out_str) == 0) {
        /* Yep */
        strlcat(out_str, add_str, str_buf_size);
    }
    else {
        /* Nope, add a / and then string */
        strlcat(out_str, "/", str_buf_size);
        strlcat(out_str, add_str, str_buf_size);
    }
}

static int
get_lease_flags_str(uint64_t lease_flags, char *out_str, size_t str_buf_size) {
    int error = 0;
    
    if (lease_flags == 0) {
        snprintf(out_str, str_buf_size, "none");
        return(error);
    }
    
    snprintf(out_str, str_buf_size, "");

    if (lease_flags & SMB2_DURABLE_HANDLE_REQUEST) {
        add_str("DurHndlRequest", out_str, str_buf_size);
    }
    
    if (lease_flags & SMB2_DURABLE_HANDLE_RECONNECT) {
        add_str("DurHndlReconnect", out_str, str_buf_size);
    }

    if (lease_flags & SMB2_DURABLE_HANDLE_GRANTED) {
        add_str("DurHndlGranted", out_str, str_buf_size);
    }
    
    if (lease_flags & SMB2_LEASE_GRANTED) {
        add_str("LeaseGranted", out_str, str_buf_size);
    }
    
    if (lease_flags & SMB2_LEASE_PARENT_LEASE_KEY_SET) {
        add_str("ParLeaseKeySet", out_str, str_buf_size);
    }
    
    if (lease_flags & SMB2_PERSISTENT_HANDLE_REQUEST) {
        add_str("PersistHndlRequest", out_str, str_buf_size);
    }
    
    if (lease_flags & SMB2_PERSISTENT_HANDLE_RECONNECT) {
        add_str("PersistHndlReconnect", out_str, str_buf_size);
    }
    
    if (lease_flags & SMB2_PERSISTENT_HANDLE_GRANTED) {
        add_str("PersistHndlGranted", out_str, str_buf_size);
    }
    
    if (lease_flags & SMB2_DURABLE_HANDLE_V2) {
        add_str("DurHndlV2", out_str, str_buf_size);
    }

    if (lease_flags & SMB2_LEASE_V2) {
        add_str("LeaseV2", out_str, str_buf_size);
    }

    if (lease_flags & SMB2_DURABLE_HANDLE_V2_CHECK) {
        add_str("DurHndlV2Check", out_str, str_buf_size);
    }

    if (lease_flags & SMB2_DURABLE_HANDLE_FAIL) {
        add_str("DurHndlFail", out_str, str_buf_size);
    }

    if (lease_flags & SMB2_LEASE_BROKEN) {
        add_str("LeaseBroken", out_str, str_buf_size);
    }

    if (lease_flags & SMB2_NEW_LEASE_KEY) {
        add_str("NewLeaseKey", out_str, str_buf_size);
    }

    if (lease_flags & SMB2_DEFERRED_CLOSE) {
        add_str("DefClose", out_str, str_buf_size);
    }

    return(error);
}





static int
do_smbstat(char *path, enum OutputFormat output_format)
{
    struct statfs statbuf;
    int error = 0;
    struct smbStatPB pb = {0};
    int i, j;
    CFMutableDictionaryRef lock = NULL;
    CFMutableDictionaryRef lockEntry = NULL;
    char buf[120];

    if ((statfs((const char*)path, &statbuf) == -1) || (strncmp(statbuf.f_fstypename, "smbfs", 5) != 0)) {
        return EINVAL;
	}
    
    /* If root user, change to the owner who mounted the share */
    if (getuid() == 0) {
        error = setuid(statbuf.f_owner);
        if (error) {
            fprintf(stderr, "%s : setuid failed %d (%s)\n\n",
                     __FUNCTION__, errno, strerror (errno));
            return(errno);
        }
    }    

    error = fsctl(path, smbfsStatFSCTL, &pb, 0);
    if (error != 0) {
        fprintf(stderr, "%s : fsctl failed %d (%s)\n\n",
                 __FUNCTION__, errno, strerror (errno));
        return(errno);
    }

    if (output_format == Json) {
        json_add_num(smbStats, "vnode_type",
                     &pb.vnode_type, sizeof(pb.vnode_type));
        if (pb.vnode_type == VDIR) {
            json_add_num(smbStats, "flags",
                         &pb.dir.flags, sizeof(pb.dir.flags));
            json_add_num(smbStats, "refcnt",
                         &pb.dir.refcnt, sizeof(pb.dir.refcnt));
            sprintf(buf, "0x%llx-0x%llx",
                   pb.dir.fid.fid_persistent, pb.dir.fid.fid_volatile);
            json_add_str(smbStats, "fid", buf);
            json_add_num(smbStats, "enum_flags",
                         &pb.dir.enum_flags, sizeof(pb.dir.enum_flags));
            json_add_num(smbStats, "enum_count",
                         &pb.dir.enum_count, sizeof(pb.dir.enum_count));
            json_add_num(smbStats, "enum_timer",
                         &pb.dir.enum_timer, sizeof(pb.dir.enum_timer));
        }
        else {
            json_add_num(smbStats, "flags",
                         &pb.file.flags, sizeof(pb.file.flags));
            json_add_num(smbStats, "sharedFID_refcnt",
                         &pb.file.sharedFID_refcnt, sizeof(pb.file.sharedFID_refcnt));
            json_add_num(smbStats, "sharedFID_mmapped",
                         &pb.file.sharedFID_mmapped, sizeof(pb.file.sharedFID_mmapped));
            sprintf(buf, "0x%llx-0x%llx",
                   pb.file.sharedFID_fid.fid_persistent, pb.file.sharedFID_fid.fid_volatile);
            json_add_str(smbStats, "sharedFID_fid", buf);
            json_add_num(smbStats, "shared_access_mode",
                         &pb.file.sharedFID_access_mode, sizeof(pb.file.sharedFID_access_mode));
            json_add_num(smbStats, "sharedFID_mmap_mode",
                         &pb.file.sharedFID_mmap_mode, sizeof(pb.file.sharedFID_mmap_mode));
            json_add_num(smbStats, "sharedFID_rights",
                         &pb.file.sharedFID_rights, sizeof(pb.file.sharedFID_rights));
            json_add_num(smbStats, "sharedFID_rw_refcnt",
                         &pb.file.sharedFID_rw_refcnt, sizeof(pb.file.sharedFID_rw_refcnt));
            json_add_num(smbStats, "sharedFID_r_refcnt",
                         &pb.file.sharedFID_r_refcnt, sizeof(pb.file.sharedFID_r_refcnt));
            json_add_num(smbStats, "sharedFID_w_refcnt",
                         &pb.file.sharedFID_w_refcnt, sizeof(pb.file.sharedFID_w_refcnt));
            json_add_num(smbStats, "sharedFID_is_EXLOCK",
                         &pb.file.sharedFID_is_EXLOCK, sizeof(pb.file.sharedFID_is_EXLOCK));
            json_add_num(smbStats, "sharedFID_is_SHLOCK",
                         &pb.file.sharedFID_is_SHLOCK, sizeof(pb.file.sharedFID_is_SHLOCK));

            json_add_num(smbStats, "sharedFID_dur_handle_flags",
                         &pb.file.sharedFID_dur_handle.flags, sizeof(pb.file.sharedFID_dur_handle.flags));
            sprintf(buf, "0x%llx-0x%llx",
                   pb.file.sharedFID_dur_handle.fid.fid_persistent, pb.file.sharedFID_dur_handle.fid.fid_volatile);
            json_add_str(smbStats, "sharedFID_dur_handle_fid", buf);
            json_add_num(smbStats, "sharedFID_dur_handle_timeout",
                         &pb.file.sharedFID_dur_handle.timeout, sizeof(pb.file.sharedFID_dur_handle.timeout));
            sprintf(buf, "0x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x",
                    pb.file.sharedFID_dur_handle.create_guid[0], pb.file.sharedFID_dur_handle.create_guid[1],
                    pb.file.sharedFID_dur_handle.create_guid[2], pb.file.sharedFID_dur_handle.create_guid[3],
                    pb.file.sharedFID_dur_handle.create_guid[4], pb.file.sharedFID_dur_handle.create_guid[5],
                    pb.file.sharedFID_dur_handle.create_guid[6], pb.file.sharedFID_dur_handle.create_guid[7],
                    pb.file.sharedFID_dur_handle.create_guid[8], pb.file.sharedFID_dur_handle.create_guid[9],
                    pb.file.sharedFID_dur_handle.create_guid[10], pb.file.sharedFID_dur_handle.create_guid[11],
                    pb.file.sharedFID_dur_handle.create_guid[12], pb.file.sharedFID_dur_handle.create_guid[13],
                    pb.file.sharedFID_dur_handle.create_guid[14], pb.file.sharedFID_dur_handle.create_guid[15]
                    );
            json_add_str(smbStats, "sharedFID_dur_handle_create_guid", buf);

            /* sharedFID BRL entries */
            for (i = 0; i < 3; i++) {
                lockEntry = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                      0,
                                                      &kCFTypeDictionaryKeyCallBacks,
                                                      &kCFTypeDictionaryValueCallBacks);
                json_add_num(lockEntry, "refcnt",
                             &pb.file.sharedFID_lockEntries[i].refcnt,
                             sizeof(pb.file.sharedFID_lockEntries[i].refcnt));
                sprintf(buf, "0x%llx-0x%llx",
                        pb.file.sharedFID_lockEntries[i].fid.fid_persistent,
                        pb.file.sharedFID_lockEntries[i].fid.fid_volatile);
                json_add_str(lockEntry, "fid", buf);
                json_add_num(lockEntry, "accessMode",
                             &pb.file.sharedFID_lockEntries[i].accessMode,
                             sizeof(pb.file.sharedFID_lockEntries[i].accessMode));
                json_add_num(lockEntry, "rights",
                             &pb.file.sharedFID_lockEntries[i].rights,
                             sizeof(pb.file.sharedFID_lockEntries[i].rights));
                
                for (j = 0; j < SMB_MAX_LOCKS_RETURNED; j++) {
                    /* If lock length is 0, then end of brls */
                    if (pb.file.sharedFID_lockEntries[i].brl_locks[j].length == 0) {
                        break;
                    }
                    
                    lock = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                     0,
                                                     &kCFTypeDictionaryKeyCallBacks,
                                                     &kCFTypeDictionaryValueCallBacks);
                    
                    json_add_num(lock, "offset",
                                 &pb.file.sharedFID_lockEntries[i].brl_locks[j].offset,
                                 sizeof(pb.file.sharedFID_lockEntries[i].brl_locks[j].offset));
                    json_add_num(lock, "length",
                                 &pb.file.sharedFID_lockEntries[i].brl_locks[j].length,
                                 sizeof(pb.file.sharedFID_lockEntries[i].brl_locks[j].length));
                    json_add_num(lock, "lock_pid",
                                 &pb.file.sharedFID_lockEntries[i].brl_locks[j].lock_pid,
                                 sizeof(pb.file.sharedFID_lockEntries[i].brl_locks[j].lock_pid));
                    json_add_num(lock, "p_pid",
                                 &pb.file.sharedFID_lockEntries[i].brl_locks[j].p_pid,
                                 sizeof(pb.file.sharedFID_lockEntries[i].brl_locks[j].p_pid));

                    /* Add in the durable handle */
                    json_add_num(smbStats, "dur_handle_flags",
                                 &pb.file.sharedFID_lockEntries[i].dur_handle.flags,
                                 sizeof(pb.file.sharedFID_lockEntries[i].dur_handle.flags));
                    sprintf(buf, "0x%llx-0x%llx",
                            pb.file.sharedFID_lockEntries[i].dur_handle.fid.fid_persistent,
                            pb.file.sharedFID_lockEntries[i].dur_handle.fid.fid_volatile);
                    json_add_str(smbStats, "dur_handle_fid", buf);
                    json_add_num(smbStats, "dur_handle_timeout",
                                 &pb.file.sharedFID_lockEntries[i].dur_handle.timeout, sizeof(pb.file.sharedFID_lockEntries[i].dur_handle.timeout));
                    sprintf(buf, "0x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x",
                            pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[0], pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[1],
                            pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[2], pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[3],
                            pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[4], pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[5],
                            pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[6], pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[7],
                            pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[8], pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[9],
                            pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[10], pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[11],
                            pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[12], pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[13],
                            pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[14], pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[15]
                            );
                    json_add_str(smbStats, "dur_handle_create_guid", buf);

                    
                    sprintf(buf, "byte_range_locks");
                    json_add_dict(lockEntry, buf, lock);
                }
                
                sprintf(buf, "sharedFID_lockEntries[%d]", i);
                json_add_dict(smbStats, buf, lockEntry);
            }

            /*
             * LockFID info
             */
            json_add_num(smbStats, "lockFID_refcnt",
                         &pb.file.lockFID_refcnt, sizeof(pb.file.lockFID_refcnt));
            json_add_num(smbStats, "lockFID_mmapped",
                         &pb.file.lockFID_mmapped, sizeof(pb.file.lockFID_mmapped));
           sprintf(buf, "0x%llx-0x%llx",
                   pb.file.lockFID_fid.fid_persistent, pb.file.lockFID_fid.fid_volatile);
            json_add_str(smbStats, "lockFID_fid", buf);
            json_add_num(smbStats, "lockFID_access_mode",
                         &pb.file.lockFID_access_mode, sizeof(pb.file.lockFID_access_mode));
            json_add_num(smbStats, "lockFID_mmap_mode",
                         &pb.file.lockFID_mmap_mode, sizeof(pb.file.lockFID_mmap_mode));
            json_add_num(smbStats, "lockFID_rights",
                         &pb.file.lockFID_rights, sizeof(pb.file.lockFID_rights));
            json_add_num(smbStats, "lockFID_rw_refcnt",
                         &pb.file.lockFID_rw_refcnt, sizeof(pb.file.lockFID_rw_refcnt));
            json_add_num(smbStats, "lockFID_r_refcnt",
                         &pb.file.lockFID_r_refcnt, sizeof(pb.file.lockFID_r_refcnt));
            json_add_num(smbStats, "lockFID_w_refcnt",
                         &pb.file.lockFID_w_refcnt, sizeof(pb.file.lockFID_w_refcnt));
            json_add_num(smbStats, "lockFID_is_EXLOCK",
                         &pb.file.lockFID_is_EXLOCK, sizeof(pb.file.lockFID_is_EXLOCK));
            json_add_num(smbStats, "lockFID_is_SHLOCK",
                         &pb.file.lockFID_is_SHLOCK, sizeof(pb.file.lockFID_is_SHLOCK));

            json_add_num(smbStats, "lockFID_dur_hndl_flags",
                         &pb.file.lockFID_dur_handle.flags, sizeof(pb.file.lockFID_dur_handle.flags));
            sprintf(buf, "0x%llx-0x%llx",
                   pb.file.lockFID_dur_handle.fid.fid_persistent, pb.file.lockFID_dur_handle.fid.fid_volatile);
            json_add_str(smbStats, "lockFID_dur_hndl_fid", buf);
            json_add_num(smbStats, "lockFID_dur_hndl_timeout",
                         &pb.file.lockFID_dur_handle.timeout, sizeof(pb.file.lockFID_dur_handle.timeout));
            sprintf(buf, "0x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x",
                    pb.file.lockFID_dur_handle.create_guid[0], pb.file.lockFID_dur_handle.create_guid[1],
                    pb.file.lockFID_dur_handle.create_guid[2], pb.file.lockFID_dur_handle.create_guid[3],
                    pb.file.lockFID_dur_handle.create_guid[4], pb.file.lockFID_dur_handle.create_guid[5],
                    pb.file.lockFID_dur_handle.create_guid[6], pb.file.lockFID_dur_handle.create_guid[7],
                    pb.file.lockFID_dur_handle.create_guid[8], pb.file.lockFID_dur_handle.create_guid[9],
                    pb.file.lockFID_dur_handle.create_guid[10], pb.file.lockFID_dur_handle.create_guid[11],
                    pb.file.lockFID_dur_handle.create_guid[12], pb.file.lockFID_dur_handle.create_guid[13],
                    pb.file.lockFID_dur_handle.create_guid[14], pb.file.lockFID_dur_handle.create_guid[15]
                    );
            json_add_str(smbStats, "lockFID_dur_hndl_create_guid", buf);

            for (i = 0; i < SMB_MAX_LOCKS_RETURNED; i++) {
                /* If lock length is 0, then end of brls */
                if (pb.file.lockFID_brl_locks[i].length == 0) {
                    break;
                }
                
                lock = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                 0,
                                                 &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
                
                json_add_num(lock, "offset",
                             &pb.file.lockFID_brl_locks[i].offset,
                             sizeof(pb.file.lockFID_brl_locks[i].offset));
                json_add_num(lock, "length",
                             &pb.file.lockFID_brl_locks[i].length,
                             sizeof(pb.file.lockFID_brl_locks[i].length));
                json_add_num(lock, "lock_pid",
                             &pb.file.lockFID_brl_locks[i].lock_pid,
                             sizeof(pb.file.lockFID_brl_locks[i].lock_pid));
                json_add_num(lock, "p_pid",
                             &pb.file.lockFID_brl_locks[i].p_pid,
                             sizeof(pb.file.lockFID_brl_locks[i].p_pid));

                sprintf(buf, "lockFID_brl[%d]", i);
                json_add_dict(smbStats, buf, lock);
            }

            json_add_num(smbStats, "rsrc_fork_timer",
                         &pb.file.rsrc_fork_timer, sizeof(pb.file.rsrc_fork_timer));
            json_add_num(smbStats, "symlink_timer",
                         &pb.file.symlink_timer, sizeof(pb.file.symlink_timer));
        }

        json_add_num(smbStats, "curr_time",
                     &pb.curr_time, sizeof(pb.curr_time));
        json_add_num(smbStats, "meta_data_timer",
                     &pb.meta_data_timer, sizeof(pb.meta_data_timer));
        json_add_num(smbStats, "finfo_timer",
                     &pb.finfo_timer, sizeof(pb.finfo_timer));
        json_add_num(smbStats, "acl_cache_timer",
                     &pb.acl_cache_timer, sizeof(pb.acl_cache_timer));
        json_add_num(smbStats, "lease_flags",
                     &pb.lease_flags, sizeof(pb.lease_flags));
        sprintf(buf, "0x%llx", pb.lease_key_hi);
        json_add_str(smbStats, "lease_key_hi", buf);
        sprintf(buf, "0x%llx", pb.lease_key_low);
        json_add_str(smbStats, "lease_key_low", buf);
        json_add_num(smbStats, "lease_req_state",
                     &pb.lease_req_state, sizeof(pb.lease_req_state));
        json_add_num(smbStats, "lease_curr_state",
                     &pb.lease_curr_state, sizeof(pb.lease_curr_state));
        sprintf(buf, "0x%llx", pb.lease_par_key_hi);
        json_add_str(smbStats, "lease_par_key_hi", buf);
        sprintf(buf, "0x%llx", pb.lease_par_key_low);
        json_add_str(smbStats, "lease_par_key_low", buf);
        json_add_num(smbStats, "lease_epoch",
                     &pb.lease_epoch, sizeof(pb.lease_epoch));
        json_add_num(smbStats, "lease_def_close_reuse_cnt",
                     &pb.lease_def_close_reuse_cnt, sizeof(pb.lease_def_close_reuse_cnt));
        json_add_num(smbStats, "lease_def_close_timer",
                     &pb.lease_def_close_timer, sizeof(pb.lease_def_close_timer));
    }
    else {
        printf("Object Type: %s \n", objType[pb.vnode_type]);
        if (pb.vnode_type == VDIR) {
            printf("   flags: 0x%llx (%s)\n", pb.dir.flags, dirFlags[pb.dir.flags]);
            printf("   refcnt: %d \n", pb.dir.refcnt);
            printf("   fid: 0x%llx-0x%llx \n",
                   pb.dir.fid.fid_persistent, pb.dir.fid.fid_volatile);
            printf("   enum flags: 0x%llx (%s)\n", pb.dir.enum_flags, enumFlags[pb.dir.enum_flags]);
            printf("   enum count: %lld \n", pb.dir.enum_count);
            printf("   enum timer: %ld \n", pb.dir.enum_timer);
        }
        else {
            printf("   flags: 0x%llx (%s)\n", pb.file.flags, fileFlags[pb.file.flags]);
            printf("   sharedFID_refcnt: %d \n", pb.file.sharedFID_refcnt);
            printf("   sharedFID_mmapped: %d \n", pb.file.sharedFID_mmapped);
            printf("   sharedFID_fid: 0x%llx-0x%llx \n",
                   pb.file.sharedFID_fid.fid_persistent, pb.file.sharedFID_fid.fid_volatile);
            get_acces_mode_str(pb.file.sharedFID_access_mode, buf, sizeof(buf));
            printf("   sharedFID_access_mode: 0x%x (%s) \n",
                   pb.file.sharedFID_access_mode, buf);
            get_acces_mode_str(pb.file.sharedFID_mmap_mode, buf, sizeof(buf));
            printf("   sharedFID_mmap_mode: 0x%x (%s) \n",
                   pb.file.sharedFID_mmap_mode, buf);
            printf("   sharedFID_rights: 0x%x \n", pb.file.sharedFID_rights);
            printf("   sharedFID_rw_refcnt: %d \n", pb.file.sharedFID_rw_refcnt);
            printf("   sharedFID_r_refcnt: %d \n", pb.file.sharedFID_r_refcnt);
            printf("   sharedFID_w_refcnt: %d \n", pb.file.sharedFID_w_refcnt);
            printf("   sharedFID_is_EXLOCK: %d \n", pb.file.sharedFID_is_EXLOCK);
            printf("   sharedFID_is_SHLOCK: %d \n", pb.file.sharedFID_is_SHLOCK);

            printf("   sharedFID_dur_handle.flags: 0x%llx \n", pb.file.sharedFID_dur_handle.flags);
            printf("   sharedFID_dur_handle.fid: 0x%llx-0x%llx \n",
                   pb.file.sharedFID_dur_handle.fid.fid_persistent, pb.file.sharedFID_dur_handle.fid.fid_volatile);
            printf("   sharedFID_dur_handle.timeout: %d \n", pb.file.sharedFID_dur_handle.timeout);
            printf("   sharedFID_dur_handle.create_guid: 0x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x \n",
                   pb.file.sharedFID_dur_handle.create_guid[0], pb.file.sharedFID_dur_handle.create_guid[1],
                   pb.file.sharedFID_dur_handle.create_guid[2], pb.file.sharedFID_dur_handle.create_guid[3],
                   pb.file.sharedFID_dur_handle.create_guid[4], pb.file.sharedFID_dur_handle.create_guid[5],
                   pb.file.sharedFID_dur_handle.create_guid[6], pb.file.sharedFID_dur_handle.create_guid[7],
                   pb.file.sharedFID_dur_handle.create_guid[8], pb.file.sharedFID_dur_handle.create_guid[9],
                   pb.file.sharedFID_dur_handle.create_guid[10], pb.file.sharedFID_dur_handle.create_guid[11],
                   pb.file.sharedFID_dur_handle.create_guid[12], pb.file.sharedFID_dur_handle.create_guid[13],
                   pb.file.sharedFID_dur_handle.create_guid[14], pb.file.sharedFID_dur_handle.create_guid[15]
                   );

            for (i = 0; i < 3; i++) {
                printf("   LockEntry: %d \n", i);
                printf("      refcnt: 0x%x \n",
                       pb.file.sharedFID_lockEntries[i].refcnt);
                printf("      fid: 0x%llx-0x%llx \n",
                       pb.file.sharedFID_lockEntries[i].fid.fid_persistent,
                       pb.file.sharedFID_lockEntries[i].fid.fid_volatile);
                printf("      accessMode: 0x%x \n",
                       pb.file.sharedFID_lockEntries[i].accessMode);
                printf("      rights: 0x%x \n",
                       pb.file.sharedFID_lockEntries[i].rights);
                
                printf("      dur_handle_flags: 0x%llx \n",
                       pb.file.sharedFID_lockEntries[i].dur_handle.flags);
                printf("      dur_handle_fid: 0x%llx-0x%llx \n",
                       pb.file.sharedFID_lockEntries[i].dur_handle.fid.fid_persistent,
                       pb.file.sharedFID_lockEntries[i].dur_handle.fid.fid_volatile);
                printf("      dur_handle_timeout: %d \n",
                       pb.file.sharedFID_lockEntries[i].dur_handle.timeout);
                printf("      dur_handle_create_guid: 0x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x \n",
                       pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[0], pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[1],
                       pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[2], pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[3],
                       pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[4], pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[5],
                       pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[6], pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[7],
                       pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[8], pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[9],
                       pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[10], pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[11],
                       pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[12], pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[13],
                       pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[14], pb.file.sharedFID_lockEntries[i].dur_handle.create_guid[15]
                       );

                for (j = 0; j < SMB_MAX_LOCKS_RETURNED; j++) {
                    /* If lock length is 0, then end of brls */
                    if (pb.file.sharedFID_lockEntries[i].brl_locks[j].length == 0) {
                        break;
                    }
                    printf("      BRL: %d \n", j);
                    printf("         offset: %lld \n", pb.file.sharedFID_lockEntries[i].brl_locks[j].offset);
                    printf("         length: %lld \n", pb.file.sharedFID_lockEntries[i].brl_locks[j].length);
                    printf("         lock_pid: %d \n", pb.file.sharedFID_lockEntries[i].brl_locks[j].lock_pid);
                    printf("         p_pid: %d \n", pb.file.sharedFID_lockEntries[i].brl_locks[j].p_pid);

                }
           }

            
            printf("\n");
            
            
            printf("   lockFID_refcnt: %d \n", pb.file.lockFID_refcnt);
            printf("   lockFID_mmapped: %d \n", pb.file.lockFID_mmapped);
            printf("   lockFID_fid: 0x%llx-0x%llx \n",
                   pb.file.lockFID_fid.fid_persistent, pb.file.lockFID_fid.fid_volatile);
            get_acces_mode_str(pb.file.lockFID_access_mode, buf, sizeof(buf));
            printf("   lockFID_access_mode: 0x%x (%s) \n",
                   pb.file.lockFID_access_mode, buf);
            get_acces_mode_str(pb.file.lockFID_mmap_mode, buf, sizeof(buf));
            printf("   lockFID_mmap_mode: 0x%x (%s) \n",
                   pb.file.lockFID_mmap_mode, buf);
            printf("   lockFID_rights: 0x%x \n", pb.file.lockFID_rights);
            printf("   lockFID_rw_refcnt: %d \n", pb.file.lockFID_rw_refcnt);
            printf("   lockFID_r_refcnt: %d \n", pb.file.lockFID_r_refcnt);
            printf("   lockFID_w_refcnt: %d \n", pb.file.lockFID_w_refcnt);
            printf("   lockFID_is_EXLOCK: %d \n", pb.file.lockFID_is_EXLOCK);
            printf("   lockFID_is_SHLOCK: %d \n", pb.file.lockFID_is_SHLOCK);

            printf("   lockFID_dur_handle.flags: 0x%llx \n", pb.file.lockFID_dur_handle.flags);
            printf("   lockFID_dur_handle.fid: 0x%llx-0x%llx \n",
                   pb.file.lockFID_dur_handle.fid.fid_persistent, pb.file.lockFID_dur_handle.fid.fid_volatile);
            printf("   lockFID_dur_handle.timeout: %d \n", pb.file.lockFID_dur_handle.timeout);
            printf("   lockFID_dur_handle.create_guid: 0x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x \n",
                   pb.file.lockFID_dur_handle.create_guid[0], pb.file.lockFID_dur_handle.create_guid[1],
                   pb.file.lockFID_dur_handle.create_guid[2], pb.file.lockFID_dur_handle.create_guid[3],
                   pb.file.lockFID_dur_handle.create_guid[4], pb.file.lockFID_dur_handle.create_guid[5],
                   pb.file.lockFID_dur_handle.create_guid[6], pb.file.lockFID_dur_handle.create_guid[7],
                   pb.file.lockFID_dur_handle.create_guid[8], pb.file.lockFID_dur_handle.create_guid[9],
                   pb.file.lockFID_dur_handle.create_guid[10], pb.file.lockFID_dur_handle.create_guid[11],
                   pb.file.lockFID_dur_handle.create_guid[12], pb.file.lockFID_dur_handle.create_guid[13],
                   pb.file.lockFID_dur_handle.create_guid[14], pb.file.lockFID_dur_handle.create_guid[15]
                   );

            printf("   lockFID_brl_locks:  \n");
            for (i = 0; i < SMB_MAX_LOCKS_RETURNED; i++) {
                if (pb.file.lockFID_brl_locks[i].length == 0) {
                    break;
                }
                
                printf("      offset: %lld \n", pb.file.lockFID_brl_locks[i].offset);
                printf("      length: %lld \n", pb.file.lockFID_brl_locks[i].length);
                printf("      lock_pid: %d \n", pb.file.lockFID_brl_locks[i].lock_pid);
                printf("      p_pid: %d \n", pb.file.lockFID_brl_locks[i].p_pid);
            }

            
            printf("   rsrc fork timer: %ld \n", pb.file.rsrc_fork_timer);
            printf("   symlink timer: %ld \n", pb.file.symlink_timer);
        }
        
        printf("current time: %ld \n", pb.curr_time);
        printf("meta data timer: %ld \n", pb.meta_data_timer);
        printf("finder info timer: %ld \n", pb.finfo_timer);
        printf("ACL timer: %ld \n", pb.acl_cache_timer);
        printf("\n");
        get_lease_flags_str(pb.lease_flags, buf, sizeof(buf));
        printf("lease flags: 0x%llx (%s)\n", pb.lease_flags, buf);
        printf("lease key hi: 0x%llx \n", pb.lease_key_hi);
        printf("lease key low: 0x%llx \n", pb.lease_key_low);
        printf("lease requested state: 0x%x (%s)\n",
               pb.lease_req_state, leaseState[pb.lease_req_state]);
        printf("lease current state: 0x%x (%s) \n",
               pb.lease_curr_state, leaseState[pb.lease_curr_state]);
        printf("lease parent key hi: 0x%llx \n", pb.lease_par_key_hi);
        printf("lease parent key low: 0x%llx \n", pb.lease_par_key_low);
        printf("lease epoch: %d \n", pb.lease_epoch);
        printf("lease def close reuse count: %d \n", pb.lease_def_close_reuse_cnt);
        printf("lease def close timer: %ld \n", pb.lease_def_close_timer);
        printf("\n");
    }

	return(error);
}


int
cmd_smbstat(int argc, char *argv[])
{
    int opt;
    enum OutputFormat format = None;
    char *path = NULL;
    int error = 0;
    
    if (argc < 2) {
        smbstat_usage();
    }

    while ((opt = getopt(argc, argv, "f:")) != EOF) {
		switch(opt) {
            case 'f':
                if (strcasecmp(optarg, "json") == 0) {
                    format = Json;

                    /* Init smbShares array */
                    smbStats = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                         0,
                                                         &kCFTypeDictionaryKeyCallBacks,
                                                         &kCFTypeDictionaryValueCallBacks);
                    if (smbStats == NULL) {
                        fprintf(stderr, "CFDictionaryCreateMutable failed\n");
                        return EINVAL;
                    }
                }
                else {
                    smbstat_usage();
                }
                break;
            default:
                smbstat_usage();
                break;
        }
    }

    path = argv[argc - 1];

    error = do_smbstat(path, format);
    if (error) {
        fprintf(stderr, "%s : do_smbstat failed %d (%s)\n\n",
                 __FUNCTION__, error, strerror (error));
    }
    else {
        if (format == Json) {
            json_print_cf_object(smbStats, NULL);
        }
    }

    return 0;
}

void
smbstat_usage(void)
{
	fprintf(stderr, "usage : smbutil smbstat [-f <format>] pathToItem\n");
    fprintf(stderr, "\
            [\n \
            description :\n \
            -f <format> : print info in the provided format. Supported formats: JSON\n \
            ]\n");
    exit(1);
}
