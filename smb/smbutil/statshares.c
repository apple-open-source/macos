/*
 * Copyright (c) 2012 - 2013 Apple Inc. All rights reserved
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

#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <sysexits.h>

#include <smbclient/smbclient.h>
#include <smbclient/smbclient_internal.h>
#include <smbclient/smbclient_private.h>
#include <smbclient/smbclient_netfs.h>
#include <smbclient/ntstatus.h>

#include <netsmb/smb_lib.h>
#include <netsmb/smb_dev.h>
#include <netsmb/smbio.h>
#include <netsmb/smbio_2.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_conn.h>

#include "common.h"
#include "netshareenum.h"

static void
print_header(FILE *fp)
{
    fprintf(fp, "\n================================================");
    fprintf(fp, "==================================================\n");
    fprintf(fp, "%-30s%-30s%s\n", "SHARE", "ATTRIBUTE TYPE", "VALUE");
    fprintf(fp, "==================================================");
    fprintf(fp, "================================================\n");
}

static void
print_if_attr(FILE *fp, uint64_t flag, uint64_t of_type,
              const char *attr, const char *attr_val, int *isattr)
{
    if (*isattr == -1)
        fprintf(fp, "%-30s%-30s%s\n", "", attr, attr_val);
    else if (flag & of_type) {
        fprintf(fp, "%-30s%-30s%s\n", "", attr, attr_val);
        *isattr = 1;
    }
}

static void
print_if_attr_chk_ret(FILE *fp, uint64_t flag, uint64_t of_type,
              const char *attr, const char *attr_val, int *ret)
{
    if (!(*ret)) {
        *ret = -1;
        print_if_attr(fp, flag, of_type, attr, attr_val, ret);
    }
    *ret = 0;
}

static void
print_delimeter(FILE *fp)
{
    fprintf(fp, "\n------------------------------------------------");
    fprintf(fp, "--------------------------------------------------\n"); \
}

static void
interpret_and_display(char *share, SMBShareAttributes *sattrs)
{
    int ret = 0;
    
    /* share name and server */
    fprintf(stdout, "%-30s\n", share);
    fprintf(stdout, "%-30s%-30s%s\n", "", "SERVER_NAME", sattrs->server_name);
    
    /* user who mounted this share */
    fprintf(stdout, "%-30s%-30s%d\n", "","USER_ID", sattrs->vc_uid);
    
    /* smb negotiate */
    print_if_attr(stdout, sattrs->vc_misc_flags,
                  SMBV_NEG_SMB1_ONLY, "SMB_NEGOTIATE",
                  "SMBV_NEG_SMB1_ONLY", &ret);
    print_if_attr(stdout, sattrs->vc_misc_flags,
                  SMBV_NEG_SMB2_ONLY, "SMB_NEGOTIATE",
                  "SMBV_NEG_SMB2_ONLY", &ret);
    print_if_attr_chk_ret(stdout, 0,
                  0, "SMB_NEGOTIATE",
                  "AUTO_NEGOTIATE", &ret);
    
    /* smb version */
    print_if_attr(stdout, sattrs->vc_flags,
                  SMBV_SMB2002, "SMB_VERSION",
                  "SMB_2.002", &ret);
    print_if_attr(stdout, sattrs->vc_flags,
                  SMBV_SMB21, "SMB_VERSION",
                  "SMB_2.1", &ret);
    print_if_attr_chk_ret(stdout, 0,
                  0, "SMB_VERSION",
                  "SMB_1", &ret);
    
    /*
     * Note: No way to get file system type since the type is determined at
     * mount time and not just by a Tree Connect.  If we ever wanted to display
     * the file system type, we would probably need an IOCTL to the mount 
     * point to get the information.
     */
    
    /* Share type */
    switch (sattrs->ss_type) {
        case SMB2_SHARE_TYPE_DISK:
            print_if_attr(stdout, 1, 1, "SMB_SHARE_TYPE", "DISK", &ret);
            break;
            
        case SMB2_SHARE_TYPE_PIPE:
            print_if_attr(stdout, 1, 1, "SMB_SHARE_TYPE", "PIPE", &ret);
            break;

        case SMB2_SHARE_TYPE_PRINT:
            print_if_attr(stdout, 1, 1, "SMB_SHARE_TYPE", "PRINT", &ret);
            break;
            
        default:
            print_if_attr_chk_ret(stdout, 0, 0, "SMB_SHARE_TYPE", "UNKNOWN",
                                  &ret);
            break;
    }
    
    /* smb server capabilities */
    print_if_attr(stdout, sattrs->vc_flags,
                  SMBV_SIGNING, "SIGNING_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->vc_flags,
                  SMBV_SIGNING_REQUIRED, "SIGNING_REQUIRED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->vc_smb1_caps,
                  SMB_CAP_EXT_SECURITY, "EXTENDED_SECURITY_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->vc_smb1_caps,
                  SMB_CAP_UNIX, "UNIX_SUPPORT",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->vc_smb1_caps,
                  SMB_CAP_LARGE_FILES, "LARGE_FILE_SUPPORTED",
                  "TRUE", &ret);
    
    /* SMB 2.x capabilities */
    print_if_attr(stdout, sattrs->vc_misc_flags,
                  SMBV_OSX_SERVER, "OS_X_SERVER",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->vc_misc_flags,
                  SMBV_CLIENT_SIGNING_REQUIRED, "CLIENT_REQUIRES_SIGNING",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->vc_misc_flags,
                  SMBV_HAS_FILEIDS, "FILE_IDS_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->vc_misc_flags,
                  SMBV_NO_QUERYINFO, "QUERYINFO_NOT_SUPPORTED",
                  "TRUE", &ret);

    print_if_attr(stdout, sattrs->vc_smb2_caps,
                  SMB2_GLOBAL_CAP_DFS, "DFS_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->vc_smb2_caps,
                  SMB2_GLOBAL_CAP_LEASING, "FILE_LEASING_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->vc_smb2_caps,
                  SMB2_GLOBAL_CAP_LARGE_MTU, "MULTI_CREDIT_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->vc_smb2_caps,
                  SMB2_GLOBAL_CAP_MULTI_CHANNEL, "MULTI_CHANNEL_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->vc_smb2_caps,
                  SMB2_GLOBAL_CAP_PERSISTENT_HANDLES, "PERSISTENT_HANDLES_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->vc_smb2_caps,
                  SMB2_GLOBAL_CAP_DIRECTORY_LEASING, "DIR_LEASING_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->vc_smb2_caps,
                  SMB2_GLOBAL_CAP_ENCRYPTION, "ENCRYPTION_SUPPORTED",
                  "TRUE", &ret);

    print_if_attr_chk_ret(stdout, 0,
                          0, "SERVER_CAPS",
                          "UNKNOWN", &ret);
    
    /* other share attributes */
    print_if_attr(stdout, sattrs->ss_attrs,
                  FILE_READ_ONLY_VOLUME, "VOLUME_RDONLY",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->ss_caps,
                  SMB2_SHARE_CAP_DFS, "DFS_SHARE",
                  "TRUE", &ret);

	if (verbose) {
        fprintf(stdout, "vc_flags: 0x%x\n", sattrs->vc_flags);
        fprintf(stdout, "vc_hflags: 0x%x\n", sattrs->vc_hflags);
        fprintf(stdout, "vc_hflags2: 0x%x\n", sattrs->vc_hflags2);
        fprintf(stdout, "vc_misc_flags: 0x%llx\n", sattrs->vc_misc_flags);
        fprintf(stdout, "vc_smb1_caps: 0x%x\n", sattrs->vc_smb1_caps);
        fprintf(stdout, "vc_smb2_caps: 0x%x\n", sattrs->vc_smb2_caps);
        fprintf(stdout, "vc_uid: 0x%x\n", sattrs->vc_uid);
        
        fprintf(stdout, "ss_attrs: 0x%x\n", sattrs->ss_attrs);
        fprintf(stdout, "ss_caps: 0x%x\n", sattrs->ss_caps);
        fprintf(stdout, "ss_flags: 0x%x\n", sattrs->ss_flags);
        fprintf(stdout, "ss_fstype: 0x%x\n", sattrs->ss_fstype);
        fprintf(stdout, "ss_type: 0x%x\n", sattrs->ss_type);
    }
}

static NTSTATUS
stat_share(char *share_mp, bool disablePrintingHeader)
{
    SMBHANDLE inConnection = NULL;
    NTSTATUS status = STATUS_SUCCESS;
    struct statfs statbuf;
    char tmp_name[MNAMELEN];
    char *share_name = NULL, *end = NULL;
    
    if ((statfs((const char*)share_mp, &statbuf) == -1) || (strncmp(statbuf.f_fstypename, "smbfs", 5) != 0)) {
        status = STATUS_INVALID_PARAMETER;
        errno = EINVAL;
        return  status;
	}
    
    /* 
     * Need to specify a share name, else you get IPC$
     * Use the f_mntfromname so we dont have to worry about -1, -2 on the 
     * mountpath and skip the initial "//"
     */
    strlcpy(tmp_name, &statbuf.f_mntfromname[2], sizeof(tmp_name));
    share_name = strchr(tmp_name, '/');
    if (share_name != NULL) {
        /* skip over the / to point at share name */
        share_name += 1;
        
        /* Check for submount and if found, strip it off */
        end = strchr(share_name, '/');
        if (end != NULL) {
            /* Found submount, just null it out as we only want sharepoint */
            *end = 0x00;
        }
    }
    else {
        fprintf(stderr, "%s : Failed to find share name in %s\n",
                __FUNCTION__, statbuf.f_mntfromname);
        status = STATUS_INVALID_PARAMETER;
        errno = EINVAL;
        return  status;
    }
    
    status = SMBOpenServerWithMountPoint(share_mp,
                                         share_name,
                                         &inConnection,
                                         0);
    if (!NT_SUCCESS(status)) {
        fprintf(stderr, "%s : SMBOpenServerWithMountPoint() failed for %s <%s>\n",
                __FUNCTION__, share_mp, share_name);
    }
    else {
        SMBShareAttributes sattrs;
        
        status = SMBGetShareAttributes(inConnection, &sattrs);
        if (!NT_SUCCESS(status)) {
            fprintf(stderr, "%s : SMBGetShareAttributes() failed for %s <%s>\n",
                    __FUNCTION__, share_mp, share_name);
        }
        else {
            if (!disablePrintingHeader)
                print_header(stdout);

            interpret_and_display(share_name, &sattrs);
            print_delimeter(stdout);
        }
        SMBReleaseServer(inConnection);
    }
    
    return status;
}

static NTSTATUS
stat_all_shares()
{
    NTSTATUS error = STATUS_SUCCESS;
    struct statfs *fs = NULL;
    int fs_cnt = 0;
    int i = 0;
    int disablePrintingHeader = 1;
    
    fs = smb_getfsstat(&fs_cnt);
    if (!fs || fs_cnt < 0)
        return ENOENT;
    print_header(stdout);
    for (i = 0; i < fs_cnt; i++, fs++) {
        NTSTATUS status;
        
        if (strncmp(fs->f_fstypename, "smbfs", 5) != 0)
			continue;
		if (fs->f_flags & MNT_AUTOMOUNTED)
            continue;
        
        status = stat_share(fs->f_mntonname, disablePrintingHeader) ;
        if (!NT_SUCCESS(status)) {
            fprintf(stderr, "%s : stat_share() failed for %s\n",
                    __FUNCTION__, fs->f_mntonname);
            print_delimeter(stderr);
            error = status;
        }
    }
    
    return error;
}

int
cmd_statshares(int argc, char *argv[])
{
    NTSTATUS status = STATUS_SUCCESS;
    int opt;
    bool disablePrintingHeader = 0;
    
    if ((opt = getopt(argc, argv, "am:")) != EOF) {
		switch(opt) {
			case 'a':
                if (argc != 2)
                    statshares_usage();
                status = stat_all_shares();
                break;
            case 'm':
                if (argc != 3)
                    statshares_usage();
                status = stat_share(optarg, disablePrintingHeader);
                break;
            default:
                break;
        }
    }
    else
        statshares_usage();
    
    if (!NT_SUCCESS(status))
        ntstatus_to_err(status);
    
    return 0;
}

void
statshares_usage(void)
{
	fprintf(stderr, "usage : smbutil statshare [-m <mount_path>] | [-a]\n");
    fprintf(stderr, "\
            [\n \
            description :\n \
            -a : attributes of all mounted shares\n \
            -m <mount_path> : attributes of share mounted at mount_path\n \
            ]\n");
    exit(1);
}
