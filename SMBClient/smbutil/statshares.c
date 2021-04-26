/*
 * Copyright (c) 2012 - 2020 Apple Inc. All rights reserved
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
#include <sys/types.h>
#include <sys/socket.h>
#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <sysexits.h>
#include <net/if.h>


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

#include <json_support.h>


CFMutableArrayRef smbShares = NULL;


/*
 * Make a copy of the name and if request unpercent escape the name
 */
char *
get_share_name(const char *name)
{
	char *newName = strdup(name);
	CFStringRef nameRef = NULL;
	CFStringRef newNameRef = NULL;
	
	/* strdup failed */
	if (!newName) {
		return newName;
	}
	
	/* Get a CFString */
	nameRef = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
	
	/* unpercent escape out the CFString */
	if (nameRef) {
		newNameRef = CFURLCreateStringByReplacingPercentEscapes(kCFAllocatorDefault,
																nameRef, CFSTR(""));
	}
	/* Now create an unpercent escape out c style string */
	if (newNameRef) {
		int maxlen = (int)CFStringGetLength(newNameRef)+1;
		char *tempName = malloc(maxlen);
		
		if (tempName) {
			free(newName);
			newName = tempName;
			CFStringGetCString(newNameRef, newName, maxlen, kCFStringEncodingUTF8);
		}
	}
	
	if (nameRef) {
		CFRelease(nameRef);
	}
	if (newNameRef) {
		CFRelease(newNameRef);
	}
	return newName;
}

static void
print_header(FILE *fp, enum OutputFormat format)
{
    if (format == None) {
        fprintf(fp, "\n================================================");
        fprintf(fp, "==================================================\n");
        fprintf(fp, "%-30s%-30s%s\n", "SHARE", "ATTRIBUTE TYPE", "VALUE");
        fprintf(fp, "==================================================");
        fprintf(fp, "================================================\n");
    }
}

static void
print_footer(FILE *fp, enum OutputFormat format)
{
    if (format == None) {
        fprintf(fp, "\n------------------------------------------------");
        fprintf(fp, "--------------------------------------------------\n"); \
    }
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
print_delimiter(FILE *fp, enum OutputFormat format)
{
    if (format == None) {
        fprintf(fp, "\n------------------------------------------------");
        fprintf(fp, "--------------------------------------------------\n");
    }
}

static void
interpret_and_display(char *share, SMBShareAttributes *sattrs)
{
    int ret = 0;
    time_t local_time;

    /* share name and server */
    fprintf(stdout, "%-30s\n", share);
    fprintf(stdout, "%-30s%-30s%s\n", "", "SERVER_NAME", sattrs->server_name);
    
    /* user who mounted this share */
    fprintf(stdout, "%-30s%-30s%d\n", "","USER_ID", sattrs->session_uid);

    if (sattrs->session_misc_flags & SMBV_MNT_SNAPSHOT) {
        fprintf(stdout, "%-30s%-30s%s\n", "", "SNAPSHOT_TIME", sattrs->snapshot_time);

        /* Convert the @GMT token to local time */
        local_time = SMBConvertGMT(sattrs->snapshot_time);
        if (local_time != 0) {
            fprintf(stdout, "%-30s%-30s%s", "", "SNAPSHOT_TIME_LOCAL", ctime(&local_time));
        }
    }

    /* smb negotiate */
    print_if_attr(stdout, sattrs->session_misc_flags,
                  SMBV_NEG_SMB1_ENABLED, "SMB_NEGOTIATE",
                  "SMBV_NEG_SMB1_ENABLED", &ret);
    print_if_attr(stdout, sattrs->session_misc_flags,
                  SMBV_NEG_SMB2_ENABLED, "SMB_NEGOTIATE",
                  "SMBV_NEG_SMB2_ENABLED", &ret);
    print_if_attr(stdout, sattrs->session_misc_flags,
                  SMBV_NEG_SMB3_ENABLED, "SMB_NEGOTIATE",
                  "SMBV_NEG_SMB3_ENABLED", &ret);
    print_if_attr_chk_ret(stdout, 0,
                  0, "SMB_NEGOTIATE",
                  "AUTO_NEGOTIATE", &ret);
    
    /* smb version */
    print_if_attr(stdout, sattrs->session_flags,
                  SMBV_SMB311, "SMB_VERSION",
                  "SMB_3.1.1", &ret);
    print_if_attr(stdout, sattrs->session_flags,
                  SMBV_SMB302, "SMB_VERSION",
                  "SMB_3.0.2", &ret);
    print_if_attr(stdout, sattrs->session_flags,
                  SMBV_SMB30, "SMB_VERSION",
                  "SMB_3.0", &ret);
    print_if_attr(stdout, sattrs->session_flags,
                  SMBV_SMB2002, "SMB_VERSION",
                  "SMB_2.002", &ret);
    print_if_attr(stdout, sattrs->session_flags,
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
    print_if_attr(stdout, sattrs->session_flags,
                  SMBV_SIGNING, "SIGNING_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->session_flags,
                  SMBV_SIGNING_REQUIRED, "SIGNING_REQUIRED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->session_smb1_caps,
                  SMB_CAP_EXT_SECURITY, "EXTENDED_SECURITY_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->session_smb1_caps,
                  SMB_CAP_UNIX, "UNIX_SUPPORT",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->session_smb1_caps,
                  SMB_CAP_LARGE_FILES, "LARGE_FILE_SUPPORTED",
                  "TRUE", &ret);
    
    /* SMB 2/3 capabilities */
    print_if_attr(stdout, sattrs->session_misc_flags,
                  SMBV_OSX_SERVER, "OS_X_SERVER",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->session_misc_flags,
                  SMBV_CLIENT_SIGNING_REQUIRED, "CLIENT_REQUIRES_SIGNING",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->session_misc_flags,
                  SMBV_HAS_FILEIDS, "FILE_IDS_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->session_misc_flags,
                  SMBV_NO_QUERYINFO, "QUERYINFO_NOT_SUPPORTED",
                  "TRUE", &ret);

    print_if_attr(stdout, sattrs->session_smb2_caps,
                  SMB2_GLOBAL_CAP_DFS, "DFS_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->session_smb2_caps,
                  SMB2_GLOBAL_CAP_LEASING, "FILE_LEASING_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->session_smb2_caps,
                  SMB2_GLOBAL_CAP_LARGE_MTU, "MULTI_CREDIT_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->session_smb2_caps,
                  SMB2_GLOBAL_CAP_MULTI_CHANNEL, "MULTI_CHANNEL_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->session_smb2_caps,
                  SMB2_GLOBAL_CAP_PERSISTENT_HANDLES, "PERSISTENT_HANDLES_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->session_smb2_caps,
                  SMB2_GLOBAL_CAP_DIRECTORY_LEASING, "DIR_LEASING_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->session_smb2_caps,
                  SMB2_GLOBAL_CAP_ENCRYPTION, "ENCRYPTION_SUPPORTED",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->session_misc_flags,
                  SMBV_MNT_HIGH_FIDELITY, "HIGH_FIDELITY",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->session_misc_flags,
                  SMBV_MNT_DATACACHE_OFF, "DATA_CACHING_OFF",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->session_misc_flags,
                  SMBV_MNT_MDATACACHE_OFF, "META_DATA_CACHING_OFF",
                  "TRUE", &ret);
    print_if_attr(stdout, sattrs->session_misc_flags,
                  SMBV_MNT_SNAPSHOT, "SNAPSHOT_MOUNT",
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
    /* Sealing current status */
    print_if_attr(stdout, sattrs->ss_flags,
                  SMB2_SHAREFLAG_ENCRYPT_DATA, "ENCRYPTION_REQUIRED",
                  "TRUE", &ret);

    /* Signing current status */
    print_if_attr(stdout, sattrs->session_hflags2,
                  SMB_FLAGS2_SECURITY_SIGNATURE, "SIGNING_ON",
                  "TRUE", &ret);

	if (verbose) {
        fprintf(stdout, "session_flags: 0x%x\n", sattrs->session_flags);
        fprintf(stdout, "session_hflags: 0x%x\n", sattrs->session_hflags);
        fprintf(stdout, "session_hflags2: 0x%x\n", sattrs->session_hflags2);
        fprintf(stdout, "session_misc_flags: 0x%llx\n", sattrs->session_misc_flags);
        fprintf(stdout, "session_smb1_caps: 0x%x\n", sattrs->session_smb1_caps);
        fprintf(stdout, "session_smb2_caps: 0x%x\n", sattrs->session_smb2_caps);
        fprintf(stdout, "session_uid: 0x%x\n", sattrs->session_uid);

		fprintf(stdout, "ss_attrs: 0x%x\n", sattrs->ss_attrs);
        fprintf(stdout, "ss_caps: 0x%x\n", sattrs->ss_caps);
        fprintf(stdout, "ss_flags: 0x%x\n", sattrs->ss_flags);
        fprintf(stdout, "ss_fstype: 0x%x\n", sattrs->ss_fstype);
        fprintf(stdout, "ss_type: 0x%x\n", sattrs->ss_type);
    }
}

static CFMutableDictionaryRef
display_json(char *share, SMBShareAttributes *sattrs)
{
    int res = 0;
    char buf[32];
    time_t local_time;

    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (dict == NULL) {
        fprintf(stderr, "CFDictionaryCreateMutable failed\n");
        return NULL;
    }
    CFMutableArrayRef arr =  CFArrayCreateMutable(NULL, 3, &kCFTypeArrayCallBacks);
    if (arr == NULL) {
        fprintf(stderr, "CFArrayCreateMutable failed\n");
        return NULL;
    }

    /* share name and server */
    json_add_str(dict, "share_name", share);
    json_add_str(dict, "SERVER_NAME", sattrs->server_name);
    
    /* user who mounted this share */
    sprintf(buf, "%d", sattrs->session_uid);
    json_add_str(dict, "USER_ID", buf);
    
    if (sattrs->session_misc_flags & SMBV_MNT_SNAPSHOT) {
        json_add_str(dict, "SNAPSHOT_TIME", sattrs->snapshot_time);

        /* Convert the @GMT token to local time */
        local_time = SMBConvertGMT(sattrs->snapshot_time);
        if (local_time != 0) {
            sprintf(buf, "%s", ctime(&local_time));
            buf[strlen(buf) - 1] = 0x00; /* strip off the newline at end */
            json_add_str(dict, "SNAPSHOT_TIME_LOCAL", buf);
        }
    }

    /* smb negotiate */
    if (sattrs->session_misc_flags & SMBV_NEG_SMB1_ENABLED) {
        CFArrayAppendValue(arr, CFSTR("SMBV_NEG_SMB1_ENABLED"));
    }
    if (sattrs->session_misc_flags & SMBV_NEG_SMB2_ENABLED) {
        CFArrayAppendValue(arr, CFSTR("SMBV_NEG_SMB2_ENABLED"));
    }
    if (sattrs->session_misc_flags & SMBV_NEG_SMB3_ENABLED) {
        CFArrayAppendValue(arr, CFSTR("SMBV_NEG_SMB3_ENABLED"));
    }
    if (CFArrayGetCount(arr) == 0) {
        CFArrayAppendValue(arr, CFSTR("AUTO_NEGOTIATE"));
    }
    CFDictionarySetValue(dict, CFSTR("SMB_NEGOTIATE"), arr);

    /* smb version */
    res = 0;
    if (sattrs->session_flags & SMBV_SMB311) {
        CFDictionarySetValue(dict, CFSTR("SMB_VERSION"), CFSTR("SMB_3.1.1"));
        res = 1;
    }
    if (!res && (sattrs->session_flags & SMBV_SMB302)) {
        CFDictionarySetValue(dict, CFSTR("SMB_VERSION"), CFSTR("SMB_3.0.2"));
        res = 1;
    }
    if (!res && (sattrs->session_flags & SMBV_SMB30)) {
        CFDictionarySetValue(dict, CFSTR("SMB_VERSION"), CFSTR("SMB_3.0"));
        res = 1;
    }
    if (!res && (sattrs->session_flags & SMBV_SMB2002)) {
        CFDictionarySetValue(dict, CFSTR("SMB_VERSION"), CFSTR("SMB_2.002"));
        res = 1;
    }
    if (!res && (sattrs->session_flags & SMBV_SMB21)) {
        CFDictionarySetValue(dict, CFSTR("SMB_VERSION"), CFSTR("SMB_2.1"));
        res = 1;
    }
    if (!res) {
        CFDictionarySetValue(dict, CFSTR("SMB_VERSION"), CFSTR("SMB_1"));
    }
    
    /*
     * Note: No way to get file system type since the type is determined at
     * mount time and not just by a Tree Connect.  If we ever wanted to display
     * the file system type, we would probably need an IOCTL to the mount
     * point to get the information.
     */
    
    /* Share type */
    switch (sattrs->ss_type) {
        case SMB2_SHARE_TYPE_DISK:
            CFDictionarySetValue(dict, CFSTR("SMB_SHARE_TYPE"), CFSTR("DISK"));
            break;
        
        case SMB2_SHARE_TYPE_PIPE:
            CFDictionarySetValue(dict, CFSTR("SMB_SHARE_TYPE"), CFSTR("PIPE"));
            break;
        
        case SMB2_SHARE_TYPE_PRINT:
            CFDictionarySetValue(dict, CFSTR("SMB_SHARE_TYPE"), CFSTR("PRINT"));
            break;
        
        default:
            CFDictionarySetValue(dict, CFSTR("SMB_SHARE_TYPE"), CFSTR("UNKNOWN"));
            break;
    }

    /* smb server capabilities */
    if (sattrs->session_flags & SMBV_SIGNING) {
        CFDictionarySetValue(dict, CFSTR("SIGNING_SUPPORTED"), CFSTR("TRUE"));
    }
    if (sattrs->session_flags & SMBV_SIGNING_REQUIRED) {
        CFDictionarySetValue(dict, CFSTR("SIGNING_REQUIRED"), CFSTR("TRUE"));
    }
    if (sattrs->session_smb1_caps & SMB_CAP_EXT_SECURITY) {
        CFDictionarySetValue(dict, CFSTR("EXTENDED_SECURITY_SUPPORTED"), CFSTR("TRUE"));
    }
    if (sattrs->session_smb1_caps & SMB_CAP_UNIX) {
        CFDictionarySetValue(dict, CFSTR("UNIX_SUPPORT"), CFSTR("TRUE"));
    }
    if (sattrs->session_smb1_caps & SMB_CAP_LARGE_FILES) {
        CFDictionarySetValue(dict, CFSTR("LARGE_FILE_SUPPORTED"), CFSTR("TRUE"));
    }
    
    /* SMB 2/3 capabilities */
    res = 0;
    if (sattrs->session_misc_flags & SMBV_OSX_SERVER) {
        CFDictionarySetValue(dict, CFSTR("OS_X_SERVER"), CFSTR("TRUE"));
        res = 1;
    }
    if (sattrs->session_misc_flags & SMBV_CLIENT_SIGNING_REQUIRED) {
        CFDictionarySetValue(dict, CFSTR("CLIENT_REQUIRES_SIGNING"), CFSTR("TRUE"));
        res = 1;
    }
    if (sattrs->session_misc_flags & SMBV_HAS_FILEIDS) {
        CFDictionarySetValue(dict, CFSTR("FILE_IDS_SUPPORTED"), CFSTR("TRUE"));
        res = 1;
    }
    if (sattrs->session_misc_flags & SMBV_NO_QUERYINFO) {
        CFDictionarySetValue(dict, CFSTR("QUERYINFO_NOT_SUPPORTED"), CFSTR("TRUE"));
        res = 1;
    }

    if (sattrs->session_smb2_caps & SMB2_GLOBAL_CAP_DFS) {
        CFDictionarySetValue(dict, CFSTR("DFS_SUPPORTED"), CFSTR("TRUE"));
        res = 1;
    }
    if (sattrs->session_smb2_caps & SMB2_GLOBAL_CAP_LEASING) {
        CFDictionarySetValue(dict, CFSTR("FILE_LEASING_SUPPORTED"), CFSTR("TRUE"));
        res = 1;
    }
    if (sattrs->session_smb2_caps & SMB2_GLOBAL_CAP_LARGE_MTU) {
        CFDictionarySetValue(dict, CFSTR("MULTI_CREDIT_SUPPORTED"), CFSTR("TRUE"));
        res = 1;
    }
    if (sattrs->session_smb2_caps & SMB2_GLOBAL_CAP_MULTI_CHANNEL) {
        CFDictionarySetValue(dict, CFSTR("MULTI_CHANNEL_SUPPORTED"), CFSTR("TRUE"));
        res = 1;
    }
    if (sattrs->session_smb2_caps & SMB2_GLOBAL_CAP_PERSISTENT_HANDLES) {
        CFDictionarySetValue(dict, CFSTR("PERSISTENT_HANDLES_SUPPORTED"), CFSTR("TRUE"));
        res = 1;
    }
    if (sattrs->session_smb2_caps & SMB2_GLOBAL_CAP_DIRECTORY_LEASING) {
        CFDictionarySetValue(dict, CFSTR("DIR_LEASING_SUPPORTED"), CFSTR("TRUE"));
        res = 1;
    }
    if (sattrs->session_smb2_caps & SMB2_GLOBAL_CAP_ENCRYPTION) {
        CFDictionarySetValue(dict, CFSTR("ENCRYPTION_SUPPORTED"), CFSTR("TRUE"));
        res = 1;
    }
    if (sattrs->session_misc_flags & SMBV_MNT_HIGH_FIDELITY) {
        CFDictionarySetValue(dict, CFSTR("HIGH_FIDELITY"), CFSTR("TRUE"));
        res = 1;
    }
    if (sattrs->session_misc_flags & SMBV_MNT_DATACACHE_OFF) {
        CFDictionarySetValue(dict, CFSTR("DATA_CACHING_OFF"), CFSTR("TRUE"));
        res = 1;
    }
    if (sattrs->session_misc_flags & SMBV_MNT_MDATACACHE_OFF) {
        CFDictionarySetValue(dict, CFSTR("META_DATA_CACHING_OFF"), CFSTR("TRUE"));
        res = 1;
    }
    if (sattrs->session_misc_flags & SMBV_MNT_SNAPSHOT) {
        CFDictionarySetValue(dict, CFSTR("SNAPSHOT_MOUNT"), CFSTR("TRUE"));
        res = 1;
    }

    if (res == 0) {
        CFDictionarySetValue(dict, CFSTR("SERVER_CAPS"), CFSTR("UNKNOWN"));
    }
    
    /* other share attributes */
    if (sattrs->ss_attrs & FILE_READ_ONLY_VOLUME) {
        CFDictionarySetValue(dict, CFSTR("VOLUME_RDONLY"), CFSTR("TRUE"));
    }
    if (sattrs->ss_attrs & SMB2_SHARE_CAP_DFS) {
        CFDictionarySetValue(dict, CFSTR("DFS_SHARE"), CFSTR("TRUE"));
    }

    /* Sealing current status */
    if (sattrs->ss_flags & SMB2_SHAREFLAG_ENCRYPT_DATA) {
        CFDictionarySetValue(dict, CFSTR("ENCRYPTION_REQUIRED"), CFSTR("TRUE"));
    }
    
    /* Signing current status */
    if (sattrs->session_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE) {
        CFDictionarySetValue(dict, CFSTR("SIGNING_ON"), CFSTR("TRUE"));
    }
    
    if (verbose) {
        sprintf(buf, "0x%x", sattrs->session_flags);
        json_add_str(dict, "session_flags", buf);
        
        sprintf(buf, "0x%x", sattrs->session_hflags);
        json_add_str(dict, "session_hflags", buf);
        
        sprintf(buf, "0x%x", sattrs->session_hflags2);
        json_add_str(dict, "session_hflags2", buf);
        
        sprintf(buf, "0x%llx", sattrs->session_misc_flags);
        json_add_str(dict, "session_misc_flags", buf);
        
        sprintf(buf, "0x%x", sattrs->session_smb1_caps);
        json_add_str(dict, "session_smb1_caps", buf);
        
        sprintf(buf, "0x%x", sattrs->session_smb2_caps);
        json_add_str(dict, "session_smb2_caps", buf);
        
        sprintf(buf, "0x%x", sattrs->session_uid);
        json_add_str(dict, "session_uid", buf);
        
        sprintf(buf, "0x%x", sattrs->ss_attrs);
        json_add_str(dict, "ss_attrs", buf);
        
        sprintf(buf, "0x%x", sattrs->ss_caps);
        json_add_str(dict, "ss_caps", buf);
        
        sprintf(buf, "0x%x", sattrs->ss_flags);
        json_add_str(dict, "ss_flags", buf);
        
        sprintf(buf, "0x%x", sattrs->ss_fstype);
        json_add_str(dict, "ss_fstype", buf);
        
        sprintf(buf, "0x%x", sattrs->ss_type);
        json_add_str(dict, "ss_type", buf);
    }

    return dict;
}

static NTSTATUS
stat_share(char *share_mp, enum OutputFormat format)
{
    SMBHANDLE inConnection = NULL;
    NTSTATUS status = STATUS_SUCCESS;
    struct statfs statbuf;
    char tmp_name[MNAMELEN];
    char *share_name = NULL, *end = NULL;
	char *unescaped_share_name = NULL;
    
    if ((statfs((const char*)share_mp, &statbuf) == -1) || (strncmp(statbuf.f_fstypename, "smbfs", 5) != 0)) {
        status = STATUS_INVALID_PARAMETER;
        errno = EINVAL;
        return  status;
	}
    
    /* If root user, change to the owner who mounted the share */
    if (getuid() == 0) {
        setuid(statbuf.f_owner);
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
    
	unescaped_share_name = get_share_name(share_name);

	if (unescaped_share_name == NULL) {
		fprintf(stderr, "%s : Failed to unescape share name <%s>\n",
				__FUNCTION__, share_name);
		status = STATUS_INVALID_PARAMETER;
		errno = EINVAL;
		return  status;
	}
	
	status = SMBOpenServerWithMountPoint(share_mp,
                                         unescaped_share_name,
                                         &inConnection,
                                         0);
    if (!NT_SUCCESS(status)) {
        fprintf(stderr, "%s : SMBOpenServerWithMountPoint() failed for %s <%s>\n",
                __FUNCTION__, share_mp, unescaped_share_name);
    }
    else {
        SMBShareAttributes sattrs;
        
        status = SMBGetShareAttributes(inConnection, &sattrs);
        if (!NT_SUCCESS(status)) {
            fprintf(stderr, "%s : SMBGetShareAttributes() failed for %s <%s>\n",
                    __FUNCTION__, share_mp, unescaped_share_name);
        }
        else {
            if (format == None) {
                interpret_and_display(unescaped_share_name, &sattrs);
            }
            if (format == Json) {
                CFMutableDictionaryRef dict = display_json(unescaped_share_name, &sattrs);
                CFArrayAppendValue(smbShares, dict);
            }
        }
        SMBReleaseServer(inConnection);
    }
    
	if (unescaped_share_name != NULL) {
		free(unescaped_share_name);
	}

	return status;
}

static NTSTATUS
stat_all_shares(enum OutputFormat format)
{
    NTSTATUS error = STATUS_SUCCESS;
    struct statfs *fs = NULL;
    int fs_cnt = 0;
    int i = 0;
    bool first = true;

    fs = smb_getfsstat(&fs_cnt);
    if (!fs || fs_cnt < 0)
        return ENOENT;
    print_header(stdout, format);
    for (i = 0; i < fs_cnt; i++, fs++) {
        NTSTATUS status;
        
        if (strncmp(fs->f_fstypename, "smbfs", 5) != 0)
			continue;
		if (fs->f_flags & MNT_AUTOMOUNTED)
            continue;

        if (!first) {
            print_delimiter(stdout, format);
        }

        status = stat_share(fs->f_mntonname, format) ;
        if (!NT_SUCCESS(status)) {
            fprintf(stderr, "%s : stat_share() failed for %s\n",
                    __FUNCTION__, fs->f_mntonname);
            error = status;
        }

        first = false;
    }

    print_footer(stdout, format);

    return error;
}

int
cmd_statshares(int argc, char *argv[])
{
    NTSTATUS status = STATUS_SUCCESS;
    int opt;
    enum OutputFormat format = None;
    bool printShare = false;
    bool printAll = false;
    char *mountPath = NULL;
    
    while ((opt = getopt(argc, argv, "am:f:")) != EOF) {
		switch(opt) {
            case 'f':
                if (strcasecmp(optarg, "json") == 0) {
                    format = Json;

                    /* Init smbShares array */
                    smbShares = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
                    if (smbShares == NULL) {
                        fprintf(stderr, "CFArrayCreateMutable failed\n");
                        return EINVAL;
                    }
                }
                else {
                    statshares_usage();
                }
                break;
			case 'a':
                printAll = true;
                if (printShare) {
                    statshares_usage();
                }
                break;
            case 'm':
                printShare = true;
                if (printAll) {
                    statshares_usage();
                }
                mountPath = optarg;
                break;
            default:
                statshares_usage();
                break;
        }
    }

    if (!printShare && !printAll) {
        statshares_usage();
    }
    
    if (printShare) {
        print_header(stdout, format);
        status = stat_share(mountPath, format);
        print_footer(stdout, format);
    }

    if (printAll) {
        status = stat_all_shares(format);
    }
    
    if (format == Json) {
        json_print_cf_object(smbShares, NULL);
    }

    if (!NT_SUCCESS(status))
        ntstatus_to_err(status);
    
    return 0;
}

void
statshares_usage(void)
{
	fprintf(stderr, "usage : smbutil statshares [-m <mount_path> | -a] [-f <format>]\n");
    fprintf(stderr, "\
            [\n \
            description :\n \
            -a : attributes of all mounted shares\n \
            -m <mount_path> : attributes of share mounted at mount_path\n \
            -f <format> : print info in the provided format. Supported formats: JSON\n \
            ]\n");
    exit(1);
}
