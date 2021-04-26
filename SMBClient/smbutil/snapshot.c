/*
 * Copyright (c) 2020 Apple Inc. All rights reserved
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

#include <stdio.h>
#include <time.h>

#include <smbclient/smbclient.h>
#include <smbclient/smbclient_internal.h>
#include <smbclient/smbclient_private.h>
#include <smbclient/smbclient_netfs.h>
#include <smbclient/ntstatus.h>

#include <netsmb/smb_dev.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb2_mc.h>

#include <net/if_media.h>

#include "common.h"

#include <json_support.h>

CFMutableDictionaryRef snapshotShares = NULL;


static void
print_header(FILE *fp, enum OutputFormat output_format)
{
    if (output_format == None) {
        fprintf(fp, "\n================================================");
        fprintf(fp, "==================================================\n");
        fprintf(fp, "%-30s%-30s%s\n", "MOUNT PATH", "@GMT Token", "LOCAL TIME");
        fprintf(fp, "==================================================");
        fprintf(fp, "================================================\n");
    }
}

static void
print_delimiter(FILE *fp, enum OutputFormat output_format)
{
    if (output_format == None) {
        fprintf(fp, "\n------------------------------------------------");
        fprintf(fp, "--------------------------------------------------\n"); \
    }
}

static NTSTATUS
snapshot_share(char *mount_path, enum OutputFormat output_format)
{
    CFMutableDictionaryRef snapshot_dict = NULL;
    CFMutableDictionaryRef time_dict = NULL;
    SMBHANDLE inConnection = NULL;
    NTSTATUS status = STATUS_SUCCESS;
    struct statfs statbuf;
    char tmp_name[MNAMELEN];
    char *share_name = NULL, *end = NULL;
    char *unescaped_share_name = NULL;
    char path[MNAMELEN] = {0};
    uint8_t buffer[64 * 1024] = {0};
    size_t buffer_size = sizeof(buffer);
    size_t ret_size = 0;
    uint32_t nbr_snapshots = 0, nbr_snapshots_ret = 0, array_size = 0;
    uint32_t *lptr, i;
    uint16_t *tptr;
    char *cStr = NULL;
    size_t maxLen = 50;
    time_t local_time;
    void *ptr;
    char buf[64];

    if (mount_path == NULL) {
        snapshot_usage();
    }

    if (output_format == Json) {
        snapshot_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                  &kCFTypeDictionaryKeyCallBacks,
                                                  &kCFTypeDictionaryValueCallBacks);
        if (snapshot_dict == NULL) {
            fprintf(stderr, "CFDictionaryCreateMutable failed\n");
            status = STATUS_NO_MEMORY;
            return EINVAL;
        }
    }
    else {
        fprintf(stdout, "%-30s\n", mount_path);
    }

    if ((statfs((const char*)mount_path, &statbuf) == -1) ||
        (strncmp(statbuf.f_fstypename, "smbfs", 5) != 0)) {
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
        return status;
    }

    end = mount_path;
    end += strlen(statbuf.f_mntonname);

    /* There might not be any path or we might be pointing at ending / */
    if (end != NULL) {
        if (*end == '/') {
            /* skip over the / between share and path */
            end += 1;

            if (end != NULL) {
                strlcpy(path, end, sizeof(path));
            }
        }
    }

    /* Use the f_mntonname for the mountpoint */
    status = SMBOpenServerWithMountPoint(statbuf.f_mntonname,
                                         unescaped_share_name,
                                         &inConnection,
                                         0);
    if (!NT_SUCCESS(status)) {
        fprintf(stderr, "%s : SMBOpenServerWithMountPoint() failed for %s <%s>\n",
                __FUNCTION__, statbuf.f_mntonname, unescaped_share_name);
    }
    else {
        status = SMBListSnapshots(inConnection,
                                  path,
                                  &buffer,
                                  buffer_size,
                                  &ret_size);
        if (!NT_SUCCESS(status)) {
            fprintf(stderr, "%s : SMBListSnapshots() failed for <%s> share <%s> path <%s> \n",
                    __FUNCTION__, statbuf.f_mntonname, unescaped_share_name,
                    path);
        }
        else {
            ptr = &buffer[0];
            lptr = ptr;
            nbr_snapshots = OSSwapLittleToHostInt32(*lptr);

            lptr++;
            nbr_snapshots_ret = OSSwapLittleToHostInt32(*lptr);

            lptr++;
            array_size = OSSwapLittleToHostInt32(*lptr);

            if (output_format == Json) {
                json_add_num(snapshot_dict, "NumberOfSnapshots",
                             &nbr_snapshots, sizeof(nbr_snapshots));
                json_add_num(snapshot_dict, "NumberOfSnapshotsReturned",
                             &nbr_snapshots_ret, sizeof(nbr_snapshots_ret));
                json_add_num(snapshot_dict, "SnapShotArraySize",
                             &array_size, sizeof(array_size));
            }
            else {
                fprintf(stdout, "     NumberOfSnapshots: %d \n",
                        nbr_snapshots);
                fprintf(stdout, "     NumberOfSnapshotsReturned: %d \n",
                        nbr_snapshots_ret);
                fprintf(stdout, "     SnapShotArraySize: %d bytes \n",
                        array_size);
            }

            lptr++;
            tptr = (uint16_t *) lptr;

            for (i = 0; i < nbr_snapshots_ret; i++) {
                cStr = SMBConvertFromUTF16ToUTF8(tptr, maxLen, 1);

                if (cStr != NULL) {
                    /* Convert the @GMT token to local time */
                    local_time = SMBConvertGMT(cStr);

                    if (local_time != 0) {
                        if (output_format == Json) {
                            time_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                                  &kCFTypeDictionaryKeyCallBacks,
                                                                  &kCFTypeDictionaryValueCallBacks);
                            if (time_dict == NULL) {
                                fprintf(stderr, "CFDictionaryCreateMutable failed\n");
                                break;
                            }

                            json_add_str(time_dict, "GMT", cStr);
                            json_add_str(time_dict, "Local", ctime(&local_time));

                            sprintf(buf, "%d", i);
                            json_add_dict(snapshot_dict, buf, time_dict);

                            if (time_dict != NULL) {
                                CFRelease(time_dict);
                                time_dict = NULL;
                            }
                        }
                        else {
                            fprintf(stdout, "%-30s%-30s%s", "", cStr, ctime(&local_time));
                        }
                    }
                    else {
                        fprintf(stderr, "%s : SMBConvertGMT() Failed to convert snapshot string \n",
                            __FUNCTION__);
                        break;
                    }

                    free(cStr);
                    cStr = NULL;
                }
                else {
                    fprintf(stderr, "%s : SMBConvertFromUTF16ToUTF8() Failed to convert snapshot string \n",
                        __FUNCTION__);
                    break;
                }

                tptr += 25;
            }
        }

        SMBReleaseServer(inConnection);
    }
    
    if (output_format == Json) {
        json_add_dict(snapshotShares, mount_path, snapshot_dict);
    }

    if (unescaped_share_name != NULL) {
        free(unescaped_share_name);
    }

    if (time_dict != NULL) {
        CFRelease(time_dict);
    }

    if (snapshot_dict != NULL) {
        CFRelease(snapshot_dict);
    }

    return status;
}

static NTSTATUS
snapshot_all(enum OutputFormat output_format)
{
    NTSTATUS error = STATUS_SUCCESS;
    NTSTATUS status;
    struct statfs *fs = NULL;
    int fs_cnt = 0;
    int i = 0;

    fs = smb_getfsstat(&fs_cnt);
    if (!fs || (fs_cnt < 0)) {
        return ENOENT;
    }

    print_header(stdout, output_format);
    for (i = 0; i < fs_cnt; i++, fs++) {
        if (strncmp(fs->f_fstypename, "smbfs", 5) != 0) {
            continue;
        }

        if (fs->f_flags & MNT_AUTOMOUNTED) {
            continue;
        }

        status = snapshot_share(fs->f_mntonname, output_format);
        if (!NT_SUCCESS(status)) {
            if (status != STATUS_NOT_SUPPORTED) {
                fprintf(stderr, "%s : snapshot_share() failed 0x%x for %s\n",
                        __FUNCTION__, status, fs->f_mntonname);
                error = status;
            }
            else {
                fprintf(stderr, "%s : Snapshots not supported for %s\n",
                        __FUNCTION__, fs->f_mntonname);
            }
        }

        print_delimiter(stderr, output_format);
    }

    return error;
}

int
cmd_snapshot(int argc, char *argv[])
{
    NTSTATUS status = STATUS_SUCCESS;
    int opt;
    enum OutputFormat output_format = None;
    bool printShare = false;
    bool printAll = false;
    char *mountPath = NULL;
    
    while ((opt = getopt(argc, argv, "af:m:")) != EOF) {
        switch(opt) {
            case 'a':
                printAll = true;
                if (printShare) {
                    snapshot_usage();
                }
                break;
            case 'f':
                if (strcasecmp(optarg, "json") == 0) {
                    output_format = Json;

                    /* Init snapshotShares dictionary */
                    snapshotShares = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                               &kCFTypeDictionaryKeyCallBacks,
                                                               &kCFTypeDictionaryValueCallBacks);
                    if (snapshotShares == NULL) {
                        fprintf(stderr, "CFDictionaryCreateMutable failed\n");
                        return EINVAL;
                    }
                }
                else {
                    statshares_usage();
                }
                break;
            case 'm':
                printShare = true;
                if (printAll) {
                    snapshot_usage();
                }
                mountPath = optarg;
                break;
            default:
                snapshot_usage();
                break;
        }
    }

    if (!printShare && !printAll) {
        snapshot_usage();
    }

    if (printAll) {
        status = snapshot_all(output_format);
    }

    if (printShare) {
        print_header(stdout, output_format);
        status = snapshot_share(mountPath, output_format);
    }

    if (output_format == Json) {
        json_print_cf_object(snapshotShares, NULL);
        printf("\n");
    }

    if (!NT_SUCCESS(status)) {
        ntstatus_to_err(status);
    }

    return 0;
}

void
snapshot_usage(void)
{
    fprintf(stderr, "usage: smbutil snapshot [-m <mount_path> | -a] [-f <format>] \n");
    fprintf(stderr, "\
            [\n \
            description :\n \
            -a : do snapshot command on root of all mounted shares\n \
            -m <mount_path> : do snapshot command on mount_path (can be root or item in mount_path) \n \
            -f <format> : print info in the provided format. Supported formats: JSON\n \
            ]\n");
    exit(1);
}
