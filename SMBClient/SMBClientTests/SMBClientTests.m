/*
 * Copyright (c) 2017 - 2018 Apple Inc. All rights reserved.
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

/*
 * URL1 - used for tests that only need a single user connection to server
 * URL2 - used for multi user testing to the same server
 * URL3 - used for tests that only test SMBv1
 */


#ifndef FAKE_XC_TEST
    #import <XCTest/XCTest.h>

    #if 0
        char g_test_url1[1024] = "smb://smbtest:storageSW0@127.0.0.1/SMBBasic";
        char g_test_url2[1024] = "smb://smbtest2:storageSW0@127.0.0.1/SMBBasic";
        char g_test_url3[1024] = "cifs://smbtest2:storageSW0@127.0.0.1/SMBBasic";
    #else
        #if 1
            char g_test_url1[1024] = "smb://Administrator:Password01!@192.168.1.30/SMBBasic";
            char g_test_url2[1024] = "smb://adbrad:Password01!@192.168.1.30/SMBBasic";
            char g_test_url3[1024] = "cifs://adbrad:Password01!@192.168.1.30/SMBBasic";
        #else
            char g_test_url1[1024] = "smb://usr1:passWORD1@192.168.0.51/smb_share_usr1_1/";
            char g_test_url2[1024] = "smb://usr2:passWORD2@192.168.0.51/smb_share_usr1_1/";
            char g_test_url3[1024] = "cifs://usr2:passWORD2@192.168.0.51/smb_share_usr1_1/";
        #endif
    #endif
#else
    /* BATS support */
    #import "FakeXCTest.h"
    #import "json_support.h"

    extern CFMutableDictionaryRef test_mdata_dict;

    char g_test_url1[1024] = {0};
    char g_test_url2[1024] = {0};
    char g_test_url3[1024] = {0};
#endif

int list_tests_with_mdata = 0;

#include <CoreFoundation/CoreFoundation.h>
#include <smbclient/smbclient.h>
#include <smbclient/smbclient_netfs.h>
#import "smb_dev.h"
#import "smb_dev_2.h"
#include <NetFS/NetFS.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <sys/paths.h>

#include "netshareenum.h"
#include "smb_lib.h"
#include "parse_url.h"
#include "upi_mbuf.h"
#include "mchain.h"
#include "rq.h"
#include "smb_converter.h"
#include "ntstatus.h"
#include "smbclient_internal.h"
#include "LsarLookup.h"
#include "ftw.h"
#include <smbclient/netbios.h>
#include <smbfs/smbfs.h>

#include <stdio.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/attr.h>
#include <sys/vnode.h>
#include <sys/xattr.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <unistd.h>
#include <copyfile.h>
#include <time.h>

char default_test_filename[] = "testfile";

static const char* ROOT_LONG_NAME_PREFIX = "PR";
static const char* TEST_LONG_NAME_PREFIX = "PT";

static uint32_t sNextTestDirectoryIndex = 0;

char root_test_dir[PATH_MAX];
char cur_test_dir[PATH_MAX];

int gVerbose = 1;
int gInitialized = 0;

/* URLRefs to do the mounts with */
CFURLRef urlRef1 = NULL;
CFURLRef urlRef2 = NULL;
CFURLRef urlRef3 = NULL;

/* Data for UBC Cache tests */
const char data1[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
const char data2[] = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz0123456789";
const char data3[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+2";



@interface SMBClientBasicFunctionalTests : XCTestCase

@end


@implementation SMBClientBasicFunctionalTests

- (void)setUp {
    char buffer[PATH_MAX];
    const char *url1, *url2, *url3;
    CFStringRef urlStringRef1 = NULL;
    CFStringRef urlStringRef2 = NULL;
    CFStringRef urlStringRef3 = NULL;

    // Put setup code here. This method is called before the invocation of each test method in the class.

    [super setUp];

    if (gInitialized == 0) {
        /* Only do once */
        gInitialized = 1;

        /* Create root test dir name */
        sprintf(root_test_dir, "%s_%08lX_%08X",
                ROOT_LONG_NAME_PREFIX, time(NULL), getpid());
    }

    /* Create test dir name inside of root dir */
    sprintf(buffer, "%s_%010lu", TEST_LONG_NAME_PREFIX,
            (long unsigned)sNextTestDirectoryIndex++);
    strlcpy(cur_test_dir, root_test_dir, sizeof(cur_test_dir));
    strlcat(cur_test_dir, "/", sizeof(cur_test_dir));
    strlcat(cur_test_dir, buffer, sizeof(cur_test_dir));


    url1 = g_test_url1;
    url2 = g_test_url2;
    url3 = g_test_url3;

    /* Convert char* to a CFString */
    urlStringRef1 = CFStringCreateWithCString(kCFAllocatorDefault, url1, kCFStringEncodingUTF8);
    if (urlStringRef1 == NULL) {
        XCTFail("CFStringCreateWithCString failed with <%s> \n", url1);
        return;
    }

    /* Convert CFStrings to CFURLRefs */
    urlRef1 = CFURLCreateWithString (NULL, urlStringRef1, NULL);
    CFRelease(urlStringRef1);
    if (urlRef1 == NULL) {
        XCTFail("CFURLCreateWithString failed with <%s> \n", url1);
        return;
    }

    /* Convert char* to a CFString */
    urlStringRef2 = CFStringCreateWithCString(kCFAllocatorDefault, url2, kCFStringEncodingUTF8);
    if (urlStringRef2 == NULL) {
        XCTFail("CFStringCreateWithCString failed with <%s> \n", url2);
        CFRelease(urlRef1);
        return;
    }

    /* Convert CFStrings to CFURLRefs */
    urlRef2 = CFURLCreateWithString (NULL, urlStringRef2, NULL);
    CFRelease(urlStringRef2);
    if (urlRef2 == NULL) {
        XCTFail("CFURLCreateWithString failed with <%s> \n", url2);
        CFRelease(urlRef1);
        return;
    }

    /* Convert char* to a CFString */
    urlStringRef3 = CFStringCreateWithCString(kCFAllocatorDefault, url3, kCFStringEncodingUTF8);
    if (urlStringRef3 == NULL) {
        XCTFail("CFStringCreateWithCString failed with <%s> \n", url3);
        CFRelease(urlRef1);
        CFRelease(urlRef2);
        return;
    }

    /* Convert CFStrings to CFURLRefs */
    urlRef3 = CFURLCreateWithString (NULL, urlStringRef3, NULL);
    CFRelease(urlStringRef3);
    if (urlRef3 == NULL) {
        XCTFail("CFURLCreateWithString failed with <%s> \n", url3);
        CFRelease(urlRef1);
        CFRelease(urlRef2);
        return;
    }
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.

    if (urlRef1) {
        CFRelease(urlRef1);
    }

    if (urlRef2) {
        CFRelease(urlRef2);
    }

    if (urlRef3) {
        CFRelease(urlRef3);
    }
}

/*
 * desc - short description of what the unit tests does
 * tags - comma seperated (no spaces) keywords of what the unit tests uses,
 *        like open, close, read, write, UBC, reparse_point, symlink, etc
 * smb_version - what SMB versions are valid for this unit test (1, 2, 3)
 * radars - comma seperated (no spaces) list of related radars, may be NULL
 * server_exclusive - comma seperated (no spaces) list of servers that this
 *        unit test is valid for. If NULL, then valid for all servers. Valid
 *        values are "apple,samba,windows,no_server_required"
 *
 * Note: If no server is needed for the unit test, then set smb_Version to NULL,
 *       and set server_exclusive to "no_server_required"
 */
void do_list_test_meta_data(const char *desc,
                            const char *tags,
                            const char *smb_versions,
                            const char *radars,
                            const char *server_exclusive
                            )
{
    if ((desc == NULL) ||
        (tags == NULL)) {
        fprintf(stderr, "*** %s: one of the arguments are null \n", __FUNCTION__);
        return;
    }

#ifdef FAKE_XC_TEST
    test_mdata_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks);
    if (test_mdata_dict == NULL) {
        fprintf(stderr, "*** %s: CFDictionaryCreateMutable failed\n",
                __FUNCTION__);
        return;
    }
    
    /* description is a short description of what the test does */
    json_add_str(test_mdata_dict, "description", desc);
    
    /* tags are keywords to help sort the tests */
    json_add_array(test_mdata_dict, "tags", tags);
    
    /* If smb_versions is NULL, that means no server is needed for this test */
    json_add_array(test_mdata_dict, "versions", smb_versions);

    /* radars is a list of related radars if any */
    json_add_array(test_mdata_dict, "radars", radars);
    
    /*
     * server_exclusive is for tests that only work against those servers in
     * the list. NULL means all servers supported.
     */
    json_add_array(test_mdata_dict, "server_exclusive", server_exclusive);
#endif
    
    return;
}


int do_create_test_dirs(const char *mp) {
    int error = 0;
    char dir_path[PATH_MAX];

    /* Set up path on mp to root dir */
    strlcpy(dir_path, mp, sizeof(dir_path));
    strlcat(dir_path, "/", sizeof(dir_path));
    strlcat(dir_path, root_test_dir, sizeof(dir_path));

    /* Create root test dir, ok for this dir to already exist */
    error = mkdir(dir_path, S_IRWXU);
    if ((error) && (errno != EEXIST)) {
        fprintf(stderr, "mkdir on <%s> failed %d:%s \n",
                dir_path, errno, strerror(errno));
        error = errno;
        goto done;
    }

    /* Make sure the dir is read/write */
    error = chmod(dir_path, S_IRWXU | S_IRWXG | S_IRWXO);
    if (error) {
        fprintf(stderr, "chmod on <%s> failed %d:%s \n",
                dir_path, errno, strerror(errno));
        error = errno;
        goto done;
    }

    /* Create test dir */
    strlcpy(dir_path, mp, sizeof(dir_path));
    strlcat(dir_path, "/", sizeof(dir_path));
    strlcat(dir_path, cur_test_dir, sizeof(dir_path));

    error = mkdir(dir_path, S_IRWXU);
    if (error) {
        fprintf(stderr, "mkdir on <%s> failed %d:%s \n",
                dir_path, errno, strerror(errno));
        error = errno;
        goto done;
    }

    /* Make sure the dir is read/write */
    error = chmod(dir_path, S_IRWXU | S_IRWXG | S_IRWXO);
    if (error) {
        fprintf(stderr, "chmod on <%s> failed %d:%s \n",
                dir_path, errno, strerror(errno));
        error = errno;
        goto done;
    }

    printf("Test dir <%s> \n", dir_path);

done:
    return(error);
}

int do_delete_test_dirs(const char *mp) {
    int error = 0;
    char dir_path[PATH_MAX];

    /* Set up path on mp to test dir */
    strlcpy(dir_path, mp, sizeof(dir_path));
    strlcat(dir_path, "/", sizeof(dir_path));
    strlcat(dir_path, cur_test_dir, sizeof(dir_path));

    /* Do the Delete on test dir */
    error = rmdir(dir_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)> \n",
                dir_path, strerror(errno), errno);
        error = errno;
        goto done;
    }

    /* Set up path on mp to root dir */
    strlcpy(dir_path, mp, sizeof(dir_path));
    strlcat(dir_path, "/", sizeof(dir_path));
    strlcat(dir_path, root_test_dir, sizeof(dir_path));

    /* Do the Delete on root dir */
    error = rmdir(dir_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)> \n",
                dir_path, strerror(errno), errno);
        error = errno;
        return(error);
    }

done:
    return(error);
}

static void do_create_mount_path(char *mp, size_t mp_len, const char *test_name)
{
    UInt8 uuid[16] = {0};
    uuid_generate(uuid);
    
    snprintf(mp, mp_len, "/private/tmp/%s_%x%x%x%x-%x%x-%x%x-%x%x-%x%x%x%x%x%x",
             test_name,
             uuid[0], uuid[1], uuid[2], uuid[3],
             uuid[4], uuid[5],
             uuid[6], uuid[7],
             uuid[8], uuid[9],
             uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
    printf("Created mountpoint: <%s> \n", mp);
}

static int do_mount(const char *mp, CFURLRef url, CFDictionaryRef openOptions, int mntflags)
{
    SMBHANDLE theConnection = NULL;
    CFStringRef mountPoint = NULL;
    CFDictionaryRef mountInfo = NULL;
    int error;
    CFNumberRef numRef = NULL;
    CFMutableDictionaryRef mountOptions = NULL;

    /* Make the dir to mount on */
    if ((mkdir(mp, S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST)) {
        fprintf(stderr, "mkdir failed %d:%s for <%s>\n", errno, strerror(errno), mp);
        return errno;
    }

    /* Create the session ref */
    error = SMBNetFsCreateSessionRef(&theConnection);
    if (error) {
        fprintf(stderr, "SMBNetFsCreateSessionRef failed %d:%s for <%s>\n",
                error, strerror(error), mp);
        return error;
    }

    /* Attempt to log in to the server */
    error = SMBNetFsOpenSession(url, theConnection, openOptions, NULL);
    if (error) {
        fprintf(stderr, "SMBNetFsOpenSession failed %d:%s for <%s>\n",
                error, strerror(error), mp);
        (void)SMBNetFsCloseSession(theConnection);
        return error;
    }

    /* Create mount options dictionary */
    mountOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!mountOptions) {
        fprintf(stderr, "CFDictionaryCreateMutable failed for <%s>\n", mp);
        (void)SMBNetFsCloseSession(theConnection);
        return error;
    }

    /*  Add mount flags into the mount options dictionary */
    numRef = CFNumberCreate( nil, kCFNumberIntType, &mntflags);
    if (numRef != NULL) {
        CFDictionarySetValue( mountOptions, kNetFSMountFlagsKey, numRef);
        CFRelease(numRef);
    }

    /* Always force a new session even in the mount */
    CFDictionarySetValue(mountOptions, kNetFSForceNewSessionKey, kCFBooleanTrue);

    /* Convert mount point into a CFStringRef */
    mountPoint = CFStringCreateWithCString(kCFAllocatorDefault, mp, kCFStringEncodingUTF8);
    if (mountPoint) {
        /* Attempt the mount */
        error = SMBNetFsMount(theConnection, url, mountPoint, mountOptions, &mountInfo, NULL, NULL);
        if (error) {
            fprintf(stderr, "SMBNetFsMount failed %d:%s for <%s>\n",
                    error, strerror(error), mp);
            /* Continue on to clean up */
        }

        CFRelease(mountPoint);
        if (mountInfo) {
            CFRelease(mountInfo);
        }
    }
    else {
        fprintf(stderr, "CFStringCreateWithCString failed for <%s>\n", mp);
        error = ENOMEM;
    }

    CFRelease(mountOptions);
    (void)SMBNetFsCloseSession(theConnection);

    return error;
}

int mount_two_sessions(const char *mp1, const char *mp2, int useSMB1)
{
    int error = 0;

    CFMutableDictionaryRef openOptions = NULL;

    /* We want to force new sessions for each mount */
    openOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (openOptions == NULL) {
        error = ENOMEM;
        goto done;
    }
    CFDictionarySetValue(openOptions, kNetFSForceNewSessionKey, kCFBooleanTrue);

    /*
     * Mount are done with MNT_DONTBROWSE so that Finder does not see them
     *
     * We will need two mounts to start with
     */
    if (useSMB1 == 0) {
        /* Use SMB 2/3 */
        error = do_mount(mp1, urlRef1, openOptions, MNT_DONTBROWSE);
    }
    else {
        /* Force using SMB 1 */
        error = do_mount(mp1, urlRef3, openOptions, MNT_DONTBROWSE);
    }
    if (error) {
        fprintf(stderr, "do_mount failed for first url %d\n", error);
        goto done;
    }

    /* Do they want a second mount \? */
    if (mp2 != NULL) {
        error = do_mount(mp2, urlRef2, openOptions, MNT_DONTBROWSE);
        if (error) {
            fprintf(stderr, "do_mount failed for second url %d\n", error);
            goto done;
        }
    }

done:
    if (openOptions) {
        CFRelease(openOptions);
    }

    return (error);
}

int setup_file_paths(const char *mp1, const char *mp2,
                     const char *filename,
                     char *file_path1, size_t file_len1,
                     char *file_path2, size_t file_len2)
{
    int error = 0;
    int fd1 = -1;

    /* Set up file paths on both mounts */
    strlcpy(file_path1, mp1, file_len1);
    strlcat(file_path1, "/", file_len1);
    strlcat(file_path1, cur_test_dir, file_len1);
    strlcat(file_path1, "/", file_len1);
    strlcat(file_path1, filename, file_len1);

    if (file_path2) {
        strlcpy(file_path2, mp2,file_len2);
        strlcat(file_path2, "/",file_len2);
        strlcat(file_path2, cur_test_dir,file_len2);
        strlcat(file_path2, "/",file_len2);
        strlcat(file_path2, filename,file_len2);
    }

    /* Create the test dirs */
    error = do_create_test_dirs(mp1);
    if (error) {
        fprintf(stderr, "do_create_test_dirs on <%s> failed %d:%s \n", mp1,
                error, strerror(error));
        goto done;
    }

    /*
     * Open the file for the writer on mp1
     * If file exists, open it and truncate it
     * If file does not exist, create it
     *
     * Note umask prevents me from also allowing RWX to group/other so
     * have to do an extra call to set those permissions
     */
    fd1 = open(file_path1, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
    if (fd1 == -1) {
        fprintf(stderr, "open on <%s> failed %d:%s \n", file_path1,
                errno,strerror(errno));
        error = errno;
        goto done;
    }

    /* Make sure the file is read/write */
    error = chmod(file_path1, S_IRWXU | S_IRWXG | S_IRWXO);
    if (error) {
        fprintf(stderr, "chmod on <%s> failed %d:%s \n", file_path1,
                error, strerror(error));
        goto done;
    }

done:
    if (fd1 != -1) {
        /* Close file on mp1 */
        error = close(fd1);
        if (error) {
            fprintf(stderr, "close on fd1 failed %d:%s \n", error, strerror(error));
            goto done;
        }
    }

    return(error);
}

int verify_read(const char *expected_data, const char *read_buffer, size_t data_len)
{
    int error = 0;
    int i;
    const char *cptr;

    if (memcmp(expected_data, read_buffer, data_len)) {
        fprintf(stderr, "memcmp failed \n");
        
        fprintf(stderr, "Correct data \n");
        cptr = (char *) expected_data;
        for (i = 0; i < data_len; i++) {
            fprintf(stderr, "%c (0x%x) ", *cptr, *cptr);
            cptr++;
        }
        fprintf(stderr, "\n");

        fprintf(stderr, "Read data \n");
        cptr = read_buffer;
        for (i = 0; i < data_len; i++) {
            fprintf(stderr, "%c (0x%x) ", *cptr, *cptr);
            cptr++;
        }
        fprintf(stderr, "\n");
        
        error = EINVAL;
    }
    
    return error;
}

int read_and_verify(int fd, const char *data, size_t data_len, int no_verify)
{
    int error = 0;
    ssize_t read_size = 0;
    char *read_buffer = NULL;
    size_t read_buf_len = data_len * 2;

    /* Read and Verify the data */
    read_buffer = malloc(read_buf_len);
    if (read_buffer == NULL) {
        fprintf(stderr, "malloc failed for read buffer \n");
        error = ENOMEM;
        goto done;
    }
    bzero(read_buffer, read_buf_len);

    read_size = pread(fd, read_buffer, read_buf_len, 0);
    if (read_size != data_len) {
        fprintf(stderr, "read verify failed %zd != %zd \n", read_size, data_len);
        error = EINVAL;
        goto done;
    }

    if (no_verify == 0) {
        error = verify_read(data, read_buffer, data_len);
    }

done:
    if (read_buffer != NULL) {
        free(read_buffer);
    }
    
    return (error);
}

int write_and_verify(int fd, const char *data, size_t data_len, int no_verify)
{
    int error = 0;
    ssize_t write_size;

    /* Write out data */
    write_size = pwrite(fd, data, data_len, 0);
    if (write_size != data_len) {
        fprintf(stderr, "data write failed %zd != %zd \n",  write_size, data_len);
        error = EINVAL;
        goto done;
    }

    if (no_verify == 0) {
        /* Probably write only mode */
        error = read_and_verify(fd, data, data_len, no_verify);
        if (error) {
            goto done;
        }
    }

done:
    return (error);
}

int read_offset(int fd, off_t offset, size_t data_len)
{
    int error = 0;
    ssize_t read_size = 0;
    char *read_buffer = NULL;
    size_t read_buf_len = data_len; /* read exact length */

    /* Read and Verify the data */
    read_buffer = malloc(read_buf_len);
    if (read_buffer == NULL) {
        fprintf(stderr, "malloc failed for read buffer \n");
        error = ENOMEM;
        goto done;
    }
    bzero(read_buffer, read_buf_len);

    read_size = pread(fd, read_buffer, read_buf_len, offset);
    if (read_size != data_len) {
        fprintf(stderr, "read offset failed %zd != %zd errno %d (%s) \n",
                read_size, data_len, errno, strerror (errno));
        error = errno;
        goto done;
    }

done:
    if (read_buffer != NULL) {
        free(read_buffer);
    }

    return (error);
}

int write_offset(int fd, const char *data, off_t offset, size_t data_len)
{
    int error = 0;
    ssize_t write_size;

    /* Write out data */
    write_size = pwrite(fd, data, data_len, offset);
    if (write_size != data_len) {
        fprintf(stderr, "write offset failed %zd != %zd errno %d (%s) \n",
                write_size, data_len, errno, strerror (errno));
        error = errno;
        goto done;
    }

done:
    return (error);
}

#define kBRL_Lock 0
#define kBRL_UnLock 1
#define kBRL_FromStart 0
#define kBRL_FromEnd 1

int byte_range_lock(char *path, int fd, int64_t offset, int64_t len,
                    uint8 lock_flag, uint8 start_end_flag)
{
    int error = 0;
    struct ByteRangeLockPB2 pb = {0};
    
    pb.offset = offset;
    pb.length = len;
    pb.unLockFlag = lock_flag;
    pb.startEndFlag = start_end_flag;
    pb.fd = fd;
    
    error = fsctl(path, smbfsByteRangeLock2FSCTL, &pb, 0);
    if (error != 0) {
        fprintf(stderr,"byte range lock fsctl failed %d (%s) \n", errno, strerror (errno));
        error = errno;
        goto done;
    }

done:
    return (error);
}

static int do_GetServerInfo(CFURLRef url,
                            SMBHANDLE inConnection,
                            CFDictionaryRef openOptions,
                            CFDictionaryRef *serverParms)
{
    int error;

    if ((inConnection == NULL) || (url == NULL)) {
        fprintf(stderr, "inConnection or url is null \n");
        return EINVAL;
    }
    
    error = SMBNetFsGetServerInfo(url, inConnection, openOptions, serverParms);
    if (error) {
        fprintf(stderr, "SMBNetFsGetServerInfo failed %d:%s \n",
                error, strerror(error));
        return error;
    }
    
    return error;
}

static int do_OpenSession(CFURLRef url,
                          SMBHANDLE inConnection,
                          CFDictionaryRef openOptions,
                          CFDictionaryRef *sessionInfo)
{
    int error;
    
    if ((inConnection == NULL) || (url == NULL)) {
        fprintf(stderr, "inConnection or url is null \n");
        return EINVAL;
    }
    
    error = SMBNetFsOpenSession(url, inConnection, openOptions, sessionInfo);
    if (error) {
        fprintf(stderr, "SMBNetFsOpenSession failed %d:%s \n",
                error, strerror(error));
        return error;
    }
    
    return error;
}

static int do_CloseSession(SMBHANDLE inConnection)
{
    int error;
    
    if (inConnection == NULL) {
        fprintf(stderr, "inConnection is null \n");
        return EINVAL;
    }
    
    error = SMBNetFsCloseSession(inConnection);
    if (error) {
        fprintf(stderr, "SMBNetFsCloseSession failed %d:%s \n",
                error, strerror(error));
        return error;
    }
    
    return error;
}

static int do_EnumerateShares(SMBHANDLE inConnection,
                              CFDictionaryRef *sharePoints)
{
    int error;
    
    if (inConnection == NULL) {
        fprintf(stderr, "inConnection is null \n");
        return EINVAL;
    }
    
    error = smb_netshareenum(inConnection, sharePoints, TRUE);
    if (error) {
        fprintf(stderr, "smb_netshareenum failed %d:%s \n",
               error, strerror(error));
        return error;
    }
    
    return error;
}

/*
 * testUBCOpenWriteClose - Verify that open/close invalidates UBC
 *
 * 1.   Open testfile on mount 1, write data, read/verify data, close file
 *      Open testfile on mount 2, read/verify data, close file
 * 2.   Open testfile on mount 1, change data but not length, read/verify data, close file
 *      Open testfile on mount 2, read/verify data, close file
 * 3.   Open testfile on mount 1, change data including length, read/verify data, close file
 *      Open testfile on mount 2, read/verify data, close file
 */
- (void)testUBCOpenWriteClose
{
    int error = 0;
    char file_path1[PATH_MAX];
    char file_path2[PATH_MAX];
    int fd1 = -1, fd2 = -1;
    int mode1 = O_RDWR;
    int mode2 = O_RDONLY;
    int iteration = 0;
    char mp1[PATH_MAX];
    char mp2[PATH_MAX];

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Verify that open/close invalidates UBC",
                               "open,close,read,write,UBC",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }
    
    /*
     * We will need two mounts to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testUBCOpenWriteCloseMp1");
    do_create_mount_path(mp2, sizeof(mp2), "testUBCOpenWriteCloseMp2");
    
    error = mount_two_sessions(mp1, mp2, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

again:

    /* Set up file paths on both mounts and create test file */
    error = setup_file_paths(mp1, mp2, default_test_filename,
                             file_path1, sizeof(file_path1),
                             file_path2, sizeof(file_path2));
    if (error) {
        XCTFail("setup_file_paths failed %d \n", error);
        goto done;
    }

    if (iteration == 1) {
        printf("Repeat tests with O_SHLOCK \n");
        mode1 |= O_SHLOCK;
        mode2 |= O_SHLOCK;
    }

    /*
     * Open the file for the writer on mp1
     */
    fd1 = open(file_path1, mode1);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path1,
                errno, strerror(errno));
        goto done;
    }

    /* Write out and verify initial data on mp1 */
    error = write_and_verify(fd1, data1, sizeof(data1), 0);
    if (error) {
        XCTFail("initial write_and_verify failed %d \n", error);
        goto done;
    }

    /* Close file on mp1 which should write the data out to the server */
    error = close(fd1);
    if (error) {
        XCTFail("close on fd1 failed %d:%s \n",
                error, strerror(error));
        goto done;
    }
    else {
        fd1 = -1;
    }

    /* smbx time granularity is 1 second so wait after each write/truncate */
    sleep(1);

    /* Switch to second connection, mp2 and open the file */
    fd2 = open(file_path2, mode2);
    if (fd2 == -1) {
        XCTFail("open on <%s> failed %d:%s \n",
                file_path2, errno, strerror(errno));
        goto done;
    }

    /* Read and verify initial data on mp2 */
    error = read_and_verify(fd2, data1, sizeof(data1), 0);
    if (error) {
        XCTFail("initial read_and_verify failed %d:%s \n",
                error, strerror(error));
        goto done;
    }

    /* Close file on mp2 */
    error = close(fd2);
    if (error) {
        XCTFail("close on fd2 failed %d:%s \n",
                error, strerror(error));
        goto done;
    }
    else {
        fd2 = -1;
    }

    printf("Initial data passes \n");


    /*
     * Open the file again on mp1 and change the data but not data len
     */
    fd1 = open(file_path1, mode1);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n",
                file_path1, errno, strerror(errno));
        goto done;
    }

    /* Write out and verify new data, but same length */
    error = write_and_verify(fd1, data2, sizeof(data2), 0);
    if (error) {
        XCTFail("second write_and_verify failed %d \n", error);
        goto done;
    }

    /* Close file on mp1 which should write the data out to the server */
    error = close(fd1);
    if (error) {
        XCTFail("close on fd1 failed %d:%s \n",
                error, strerror(error));
        goto done;
    }
    else {
        fd1 = -1;
    }

    /* smbx time granularity is 1 second so wait after each write/truncate */
    sleep(1);

    /* Switch to second connection, mp2 and open the file */
    fd2 = open(file_path2, mode2);
    if (fd2 == -1) {
        XCTFail("open on <%s> failed %d:%s \n",
                file_path2, errno, strerror(errno));
        goto done;
    }

    /* Read data into UBC and verify the second data */
    error = read_and_verify(fd2, data2, sizeof(data2), 0);
    if (error) {
        XCTFail("second read_and_verify failed %d:%s \n",
                error, strerror(error));
        goto done;
    }

    /* Close file on mp2 */
    error = close(fd2);
    if (error) {
        XCTFail("close on fd2 failed %d:%s \n",
                error, strerror(error));
        goto done;
    }
    else {
        fd2 = -1;
    }

    printf("Change data but NOT length passes \n");


    /*
     * Open the file again on mp1 and change the data and data len
     */
    fd1 = open(file_path1, mode1);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n",
                file_path1, errno, strerror(errno));
        goto done;
    }

    /* Write out new data, but different length */
    error = write_and_verify(fd1, data3, sizeof(data3), 0);
    if (error) {
        XCTFail("third write_and_verify failed %d \n", error);
        goto done;
    }

    /* Close file on mp1 which should write the data out to the server */
    error = close(fd1);
    if (error) {
        XCTFail("close on fd1 failed %d:%s \n",
                error, strerror(error));
        goto done;
    }
    else {
        fd1 = -1;
    }

    /* smbx time granularity is 1 second so wait after each write/truncate */
    sleep(1);

    /* Switch to second connection, mp2 and open the file */
    fd2 = open(file_path2, mode2);
    if (fd2 == -1) {
        XCTFail("open on <%s> failed %d:%s \n",
                file_path2, errno, strerror(errno));
        goto done;
    }

    /* Read data into UBC and verify the third data */
    error = read_and_verify(fd2, data3, sizeof(data3), 0);
    if (error) {
        XCTFail("third read_and_verify failed %d:%s \n",
                error, strerror(error));
        goto done;
    }

    /* Close file on mp2 */
    error = close(fd2);
    if (error) {
        XCTFail("close on fd2 failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    else {
        fd2 = -1;
    }

    printf("Change data AND length passes \n");

    /* Do the Delete on test file */
    error = remove(file_path1);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path1, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

    if (iteration == 0) {
        iteration += 1;
        goto again;
    }

done:
    if (fd1 != -1) {
        /* Close file on mp1 */
        error = close(fd1);
        if (error) {
            XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (fd2 != -1) {
        /* Close file on mp2 */
        error = close(fd2);
        if (error) {
            XCTFail("close on fd2 failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    if (unmount(mp2, MNT_FORCE) == -1) {
        XCTFail("unmount failed for second url %d\n", errno);
    }

    rmdir(mp1);
    rmdir(mp2);
}

/*
 * testUBCOpenWriteFsyncClose - Verify that open/write/fsync pushes data to
 *      server while leaving file open on mp1, but open/close file on mp2
 *
 * 1.   Open testfile on mount 1, write data, read/verify data, fsync data
 *      Open testfile on mount 2, read/verify data, close file
 * 2.   On mount 1, change data but not length, read/verify data, fsync data
 *      Open testfile on mount 2, read/verify data, close file
 * 3.   On mount 1, change data including length, read/verify data, fsync data
 *      Open testfile on mount 2, read/verify data, close file
 * 4.   Close testfile on mount 1
 */
- (void)testUBCOpenWriteFsyncClose
{
    int error = 0;
    char file_path1[PATH_MAX];
    char file_path2[PATH_MAX];
    int fd1 = -1, fd2 = -1;
    char mp1[PATH_MAX];
    char mp2[PATH_MAX];

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Verify that open/write/fsync pushes data to server while leaving file open on mp1, but open/close file on mp2",
                               "open,close,read,write,fsync,UBC",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need two mounts to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testUBCOpenWriteFsyncCloseMp1");
    do_create_mount_path(mp2, sizeof(mp2), "testUBCOpenWriteFsyncCloseMp2");

    error = mount_two_sessions(mp1, mp2, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Set up file paths on both mounts and create test file */
    error = setup_file_paths(mp1, mp2, default_test_filename,
                             file_path1, sizeof(file_path1),
                             file_path2, sizeof(file_path2));
    if (error) {
        XCTFail("setup_file_paths failed %d \n", error);
        goto done;
    }

    /*
     * Open the file for the writer on mp1
     */
    fd1 = open(file_path1, O_RDWR);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path1,
                errno, strerror(errno));
        goto done;
    }

    /* Write out and verify initial data on mp1 */
    error = write_and_verify(fd1, data1, sizeof(data1), 0);
    if (error) {
        XCTFail("initial write_and_verify failed %d \n", error);
        goto done;
    }

    /* Call fsync on mp1 which should write the data out to the server */
    error = fsync(fd1);
    if (error) {
        XCTFail("fsync on fd1 failed %d:%s \n", error, strerror(error));
        goto done;
    }

    /* smbx time granularity is 1 second so wait after each write/truncate */
    sleep(1);

    /* Switch to second connection, mp2 and open the file */
    fd2 = open(file_path2, O_RDONLY);
    if (fd2 == -1) {
        XCTFail("open on <%s> failed %d:%s \n",
                file_path2, errno, strerror(errno));
        goto done;
    }

    /* Read data into UBC and verify the initial data */
    error = read_and_verify(fd2, data1, sizeof(data1), 0);
    if (error) {
        XCTFail("initial read_and_verify failed %d:%s \n", error, strerror(error));
        goto done;
    }

    /* Close file on mp2 */
    error = close(fd2);
    if (error) {
        XCTFail("close on fd2 failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    else {
        fd2 = -1;
    }

    printf("Initial data passes \n");


    /*
     * File on mp1 is still open. Change the data but not data len
     */

    /* Write out and verify new data, but same length */
    error = write_and_verify(fd1, data2, sizeof(data2), 0);
    if (error) {
        XCTFail("second write_and_verify failed %d \n", error);
        goto done;
    }

    /* Call fsync on mp1 which should write the data out to the server */
    error = fsync(fd1);
    if (error) {
        XCTFail("fsync on fd1 failed %d:%s \n", error, strerror(error));
        goto done;
    }

    /* smbx time granularity is 1 second so wait after each write/truncate */
    sleep(1);

    /* Switch to second connection, mp2 and open the file */
    fd2 = open(file_path2, O_RDONLY);
    if (fd2 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path2, errno, strerror(errno));
        goto done;
    }

    /* Read data into UBC and verify the second data */
    error = read_and_verify(fd2, data2, sizeof(data2), 0);
    if (error) {
        XCTFail("second read_and_verify failed %d:%s \n",
                error, strerror(error));
        goto done;
    }

    /* Close file on mp2 */
    error = close(fd2);
    if (error) {
        XCTFail("close on fd2 failed %d:%s \n",
                error, strerror(error));
        goto done;
    }
    else {
        fd2 = -1;
    }

    printf("Change data but NOT length passes \n");


    /*
     * File on mp1 is still open. Change the data and data len
     */

    /* Write out new data, but different length */
    error = write_and_verify(fd1, data3, sizeof(data3), 0);
    if (error) {
        XCTFail("third write_and_verify failed %d \n", error);
        goto done;
    }

    /* Call fsync on mp1 which should write the data out to the server */
    error = fsync(fd1);
    if (error) {
        XCTFail("fsync on fd1 failed %d:%s \n", error, strerror(error));
        goto done;
    }

    /* smbx time granularity is 1 second so wait after each write/truncate */
    sleep(1);

    /* Switch to second connection, mp2 and open the file */
    fd2 = open(file_path2, O_RDONLY);
    if (fd2 == -1) {
        XCTFail("open on <%s> failed %d:%s \n",
                file_path2, errno, strerror(errno));
        goto done;
    }

    /* Read data into UBC and verify the third data */
    error = read_and_verify(fd2, data3, sizeof(data3), 0);
    if (error) {
        XCTFail("third read_and_verify failed %d:%s \n",
                error, strerror(error));
        goto done;
    }

    /* Close file on mp2 */
    error = close(fd2);
    if (error) {
        XCTFail("close on fd2 failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    else {
        fd2 = -1;
    }

    printf("Change data AND length passes \n");

    /* Close file on mp1 */
    error = close(fd1);
    if (error) {
        XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    else {
        fd1 = -1;
    }

    /* Do the Delete on test file */
    error = remove(file_path1);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path1, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (fd1 != -1) {
        /* Close file on mp1 */
        error = close(fd1);
        if (error) {
            XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (fd2 != -1) {
        /* Close file on mp2 */
        error = close(fd2);
        if (error) {
            XCTFail("close on fd2 failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    if (unmount(mp2, MNT_FORCE) == -1) {
        XCTFail("unmount failed for second url %d\n", errno);
    }

    rmdir(mp1);
    rmdir(mp2);
}


/*
 * testUBCOpenWriteFsync - Verify that open/write/fsync pushes data to server
 *                         even when the file is left open on mp1 and mp2
 *
 * 1.   Open testfile on mount 1, write data, read/verify data, fsync data
 *      Open testfile on mount 2, read/verify data
 * 2.   On mount 1, change data but not length, read/verify data, fsync data
 *      On mount 2, wait 5 seconds for meta data cache to expire, do fstat to
 *      get latest meta data, read/verify data
 * 3.   On mount 1, change data including length, read/verify data, fsync data
 *      On mount 2, wait 5 seconds for meta data cache to expire, do fstat to
 *      get latest meta data, read/verify data
 * 4.   Close testfile on mount 1. Close testfile on mount 2
 */
- (void)testUBCOpenWriteFsync
{
    int error = 0;
    char file_path1[PATH_MAX];
    char file_path2[PATH_MAX];
    int fd1 = -1, fd2 = -1;
    struct stat stat_buffer = {0};
    struct stat saved_stat_buffer = {0};
    char mp1[PATH_MAX];
    char mp2[PATH_MAX];
    int max_wait = 5, i;

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Verify that open/write/fsync pushes data to server even when the file is left open on mp1 and mp2",
                               "open,close,read,write,fsync,UBC",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need two mounts to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testUBCOpenWriteFsyncMp1");
    do_create_mount_path(mp2, sizeof(mp2), "testUBCOpenWriteFsyncMp2");

    error = mount_two_sessions(mp1, mp2, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Set up file paths on both mounts and create test file */
    error = setup_file_paths(mp1, mp2, default_test_filename,
                             file_path1, sizeof(file_path1),
                             file_path2, sizeof(file_path2));
    if (error) {
        XCTFail("setup_file_paths failed %d \n", error);
        goto done;
    }

    /*
     * Open the file for the writer on mp1
     */
    fd1 = open(file_path1, O_RDWR);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path1,
                errno, strerror(errno));
        goto done;
    }

    /* Write out and verify initial data on mp1 */
    error = write_and_verify(fd1, data1, sizeof(data1), 0);
    if (error) {
        XCTFail("initial write_and_verify failed %d \n", error);
        goto done;
    }

    /* Call fsync on mp1 which should write the data out to the server */
    error = fsync(fd1);
    if (error) {
        XCTFail("fsync on fd1 failed %d:%s \n", error, strerror(error));
        goto done;
    }

    /* smbx time granularity is 1 second so wait after each write/truncate */
    sleep(1);

    /* Switch to second connection, mp2 and open the file */
    fd2 = open(file_path2, O_RDONLY);
    if (fd2 == -1) {
        XCTFail("open on <%s> failed %d:%s \n",
                file_path2, errno, strerror(errno));
        goto done;
    }

    /* Read and verify initial data on mp2 */
    error = read_and_verify(fd2, data1, sizeof(data1), 0);
    if (error) {
        XCTFail("initial read_and_verify failed %d:%s \n",
                error, strerror(error));
        goto done;
    }

    printf("Initial data passes \n");

    /*
     * Save current mod date on mp2 so we know when its been updated
     */
    error = fstat(fd2, &saved_stat_buffer);
    if (error) {
        XCTFail("Save fstat failed %d:%s \n",
                errno, strerror(errno));
        goto done;
    }

    /*
     * File on mp1 is still open. Change the data but not data len
     */

    /* Write out and verify new data, but same length */
    error = write_and_verify(fd1, data2, sizeof(data2), 0);
    if (error) {
        XCTFail("second write_and_verify failed %d \n", error);
        goto done;
    }

    /* Call fsync on mp1 which should write the data out to the server */
    error = fsync(fd1);
    if (error) {
        XCTFail("fsync on fd1 failed %d:%s \n", error, strerror(error));
        goto done;
    }

    /* Switch to second connection, mp2 where the file is still open */

    /*
     * Wait for lease break on mp2 or for meta data cache to expire
     *
     * 1. If lease breaks are supported by this server, then when lease break
     * arrives, it will invalidate the meta data cache and the fstat() will
     * get the newer mod date. Then we know its ok to do the read on mp2.
     *
     * 2. If lease breaks are not supported, then we just have to wait
     * at least 5 seconds to let the meta data cache expire.
     * Reads get the vnode directly so need to force the meta data cache to
     * be checked by using fstat(). The meta data cache has expired by now
     * so fstat() will update the meta data cache where it should find
     * the modification data AND file size has changed and flush the UBC and
     * invalidate it.
     *
     *
     */
    for (i = 0; i < max_wait; i++) {
        error = fstat(fd2, &stat_buffer);
        if (error) {
            XCTFail("Waiting fstat failed %d:%s \n",
                    errno, strerror(errno));
            goto done;
        }

        if ((saved_stat_buffer.st_mtimespec.tv_sec == stat_buffer.st_mtimespec.tv_sec) &&
            (saved_stat_buffer.st_mtimespec.tv_nsec == stat_buffer.st_mtimespec.tv_nsec)) {
            /* mod time the same, so keep waiting */
            sleep(1);
            printf("Waiting %d secs \n", i);
        }
        else {
            printf("Done waiting after %d secs \n", i);
            break;
        }
    }

    /* Read data into UBC and verify the second data */
    error = read_and_verify(fd2, data2, sizeof(data2), 0);
    if (error) {
        XCTFail("second read_and_verify failed %d:%s \n",
                error, strerror(error));
        goto done;
    }

    printf("Change data but NOT length passes \n");

    /*
     * Save current mod date on mp2 so we know when its been updated
     */
    error = fstat(fd2, &saved_stat_buffer);
    if (error) {
        XCTFail("Save fstat failed %d:%s \n",
                errno, strerror(errno));
        goto done;
    }

    /*
     * File on mp1 is still open. Change the data and data len
     */

    /* Write out new data, but different length */
    error = write_and_verify(fd1, data3, sizeof(data3), 0);
    if (error) {
        XCTFail("third write_and_verify failed %d \n", error);
        goto done;
    }

    /* Call fsync on mp1 which should write the data out to the server */
    error = fsync(fd1);
    if (error) {
        XCTFail("fsync on fd1 failed %d:%s \n", error, strerror(error));
        goto done;
    }

    /* Switch to second connection, mp2 where the file is still open */

    /*
     * Wait for lease break on mp2 or for meta data cache to expire
     *
     * 1. If lease breaks are supported by this server, then when lease break
     * arrives, it will invalidate the meta data cache and the fstat() will
     * get the newer mod date. Then we know its ok to do the read on mp2.
     *
     * 2. If lease breaks are not supported, then we just have to wait
     * at least 5 seconds to let the meta data cache expire.
     * Reads get the vnode directly so need to force the meta data cache to
     * be checked by using fstat(). The meta data cache has expired by now
     * so fstat() will update the meta data cache where it should find
     * the modification data AND file size has changed and flush the UBC and
     * invalidate it.
     *
     *
     */
    for (i = 0; i < max_wait; i++) {
        error = fstat(fd2, &stat_buffer);
        if (error) {
            XCTFail("Waiting fstat failed %d:%s \n",
                    errno, strerror(errno));
            goto done;
        }

        if ((saved_stat_buffer.st_mtimespec.tv_sec == stat_buffer.st_mtimespec.tv_sec) &&
            (saved_stat_buffer.st_mtimespec.tv_nsec == stat_buffer.st_mtimespec.tv_nsec)) {
            /* mod time the same, so keep waiting */
            sleep(1);
            printf("Waiting %d secs \n", i);
        }
        else {
            printf("Done waiting after %d secs \n", i);
            break;
        }
    }

    /*
     * Reads get the vnode directly so need to force the meta data cache to
     * be checked by using fstat(). The meta data cache has expired by now
     * so fstat() will update the meta data cache where it should find
     * the modification data AND file size has changed and flush the UBC and
     * invalidate it.
     */
    error = fstat(fd2, &stat_buffer);
    if (error) {
        XCTFail("third fstat failed %d:%s \n",
                errno, strerror(errno));
        goto done;
    }

    /* Read data into UBC and verify the third data */
    error = read_and_verify(fd2, data3, sizeof(data3), 0);
    if (error) {
        XCTFail("third read_and_verify failed %d:%s \n",
                error, strerror(error));
        goto done;
    }

    printf("Change data AND length passes \n");

    /* Close file on mp2 */
    error = close(fd2);
    if (error) {
        XCTFail("close on fd2 failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    else {
        fd2 = -1;
    }

    /* Close file on mp1 */
    error = close(fd1);
    if (error) {
        XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    else {
        fd1 = -1;
    }

    /* Do the Delete on test file */
    error = remove(file_path1);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path1, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (fd1 != -1) {
        /* Close file on mp1 */
        error = close(fd1);
        if (error) {
            XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (fd2 != -1) {
        /* Close file on mp2 */
        error = close(fd2);
        if (error) {
            XCTFail("close on fd2 failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    if (unmount(mp2, MNT_FORCE) == -1) {
        XCTFail("unmount failed for second url %d\n", errno);
    }

    rmdir(mp1);
    rmdir(mp2);
}

/*
 * testUBCOpenWriteNoCache - Verify that open/write with no cache pushes data to
 *      server immediately even when the file is left open on mp2
 *
 * 1.   Open testfile on mount 1, disable caching, write data, read/verify data
 *      Open testfile on mount 2, disable caching, read/verify data
 * 2.   On mount 1, change data but not length, read/verify data
 *      On mount 2, immediately read/verify data
 * 3.   On mount 1, change data including length, read/verify data
 *      On mount 2, immediately read/verify data
 * 4.   Close testfile on mount 1. Close testfile on mount 2
 */
-(void)testUBCOpenWriteNoCache
{
    int error = 0;
    char file_path1[PATH_MAX];
    char file_path2[PATH_MAX];
    int fd1 = -1, fd2 = -1;
    char mp1[PATH_MAX];
    char mp2[PATH_MAX];

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Verify that open/write with no cache pushes data to server immediately even when the file is left open on mp2",
                               "open,close,read,write,UBC",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need two mounts to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testUBCOpenWriteNoCacheMp1");
    do_create_mount_path(mp2, sizeof(mp2), "testUBCOpenWriteNoCacheMp2");

    error = mount_two_sessions(mp1, mp2, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Set up file paths on both mounts and create test file */
    error = setup_file_paths(mp1, mp2, default_test_filename,
                             file_path1, sizeof(file_path1),
                             file_path2, sizeof(file_path2));
    if (error) {
        XCTFail("setup_file_paths failed %d \n", error);
        goto done;
    }

    /*
     * Open the file for the writer on mp1
     */
    fd1 = open(file_path1, O_RDWR);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path1,
                errno, strerror(errno));
        goto done;
    }

    /* Turn off data caching. All IO should go out immediately */
    error = fcntl(fd1, F_NOCACHE, 1);
    if (error) {
        XCTFail("fcntl on fd1 failed %d:%s \n", error, strerror(error));
        goto done;
    }

    /* Write out and verify initial data on mp1 */
    error = write_and_verify(fd1, data1, sizeof(data1), 0);
    if (error) {
        XCTFail("initial write_and_verify failed %d \n", error);
        goto done;
    }

    /* Switch to second connection, mp2 and open the file */
    fd2 = open(file_path2, O_RDONLY);
    if (fd2 == -1) {
        XCTFail("open on <%s> failed %d:%s \n",
                file_path2, errno, strerror(errno));
        goto done;
    }

    /* Turn off data caching */
    error = fcntl(fd2, F_NOCACHE, 1);
    if (error) {
        XCTFail("fcntl on fd2 failed %d:%s \n", error, strerror(error));
        goto done;
    }

    /* Read and verify initial data on mp2 */
    error = read_and_verify(fd2, data1, sizeof(data1), 0);
    if (error) {
        XCTFail("initial read_and_verify failed %d:%s \n",
                error, strerror(error));
        goto done;
    }

    printf("Initial data passes \n");

    /*
     * File on mp1 is still open. Change the data but not data len
     */

    /* Write out and verify new data, but same length */
    error = write_and_verify(fd1, data2, sizeof(data2), 0);
    if (error) {
        XCTFail("second write_and_verify failed %d \n", error);
        goto done;
    }

    /* Switch to second connection, mp2 where the file is still open */

    /* Read data into UBC and verify the second data */
    error = read_and_verify(fd2, data2, sizeof(data2), 0);
    if (error) {
        XCTFail("second read_and_verify failed %d:%s \n",
                error, strerror(error));
        goto done;
    }

    printf("Change data but NOT length passes \n");

    /*
     * File on mp1 is still open. Change the data and data len
     */

    /* Write out new data, but different length */
    error = write_and_verify(fd1, data3, sizeof(data3), 0);
    if (error) {
        XCTFail("third write_and_verify failed %d \n", error);
        goto done;
    }

    /* Switch to second connection, mp2 where the file is still open */

    /* Read data into UBC and verify the third data */
    error = read_and_verify(fd2, data3, sizeof(data3), 0);
    if (error) {
        XCTFail("third read_and_verify failed %d:%s \n",
                error, strerror(error));
        goto done;
    }

    printf("Change data AND length passes \n");

    /* Close file on mp2 */
    error = close(fd2);
    if (error) {
        XCTFail("close on fd2 failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    else {
        fd2 = -1;
    }

    /* Close file on mp1 */
    error = close(fd1);
    if (error) {
        XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    else {
        fd1 = -1;
    }

    /* Do the Delete on test file */
    error = remove(file_path1);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path1, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (fd1 != -1) {
        /* Close file on mp1 */
        error = close(fd1);
        if (error) {
            XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (fd2 != -1) {
        /* Close file on mp2 */
        error = close(fd2);
        if (error) {
            XCTFail("close on fd2 failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    if (unmount(mp2, MNT_FORCE) == -1) {
        XCTFail("unmount failed for second url %d\n", errno);
    }

    rmdir(mp1);
    rmdir(mp2);
}

/*
 * This test is only valid for servers that support AAPL create context and
 * support the NFS ACE for setting/getting Posix mode bits.
 */
-(void)testFileCreateInitialMode
{
    int error = 0;
    char file_path[PATH_MAX];
    int fd = -1;
    int oflag = O_EXCL | O_CREAT | O_SHLOCK | O_NONBLOCK | O_RDWR;
    int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH; /* -rw-rw-rw- */
    struct stat stat_buffer = {0};
    int i;
    char mp1[PATH_MAX];

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("For AAPL servers, verify that setting inital create modes via NFS ACE works",
                               "open,close,fstat,posix_permissions,nfs_ace",
                               "1,2,3",
                               NULL,
                               "apple");
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testFileCreateInitialModeMp1");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create the test dirs */
    error = do_create_test_dirs(mp1);
    if (error) {
        XCTFail("do_create_test_dirs on <%s> failed %d:%s \n", mp1,
                error, strerror(error));
        goto done;
    }

    /* Set up file path */
    strlcpy(file_path, mp1, sizeof(file_path));
    strlcat(file_path, "/", sizeof(file_path));
    strlcat(file_path, cur_test_dir, sizeof(file_path));
    strlcat(file_path, "/", sizeof(file_path));
    strlcat(file_path, default_test_filename, sizeof(file_path));

    for (i = 0; i < 3; i++) {
        switch(i) {
            case 0:
                printf("Test with plain open \n");
                oflag = O_EXCL | O_CREAT | O_NONBLOCK | O_RDWR;
                break;
            case 1:
                printf("Test with O_SHLOCK \n");
                oflag = O_EXCL | O_CREAT | O_NONBLOCK | O_RDWR | O_SHLOCK;
                break;
            case 2:
                printf("Test with O_EXLOCK \n");
                oflag = O_EXCL | O_CREAT | O_NONBLOCK | O_RDWR | O_EXLOCK;
                break;
        }

        /*
         * Open the file for the writer on mp1
         */
        fd = open(file_path, oflag, mode);
        if (fd == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        error = fstat(fd, &stat_buffer);
        if (error) {
            XCTFail("fstat on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        /*
         * Note the umask will probably strip out write access so instead of
         * rw-rw-rw-, we end up with rw-r--r--
         */
        printf("Got initial mode <0o%o> \n", stat_buffer.st_mode);

        if ((stat_buffer.st_mode & ACCESSPERMS) == S_IRWXU) {
            /* Default mode, thus setting create mode must have failed */
            XCTFail("Initial create mode failed to set \n");
            goto done;
        }

        /* Close file */
        error = close(fd);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            fd = -1;
        }

        /* Do the Delete on test file */
        error = remove(file_path);
        if (error) {
            fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                    file_path, strerror(errno), errno);
            goto done;
        }
    }


    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (fd != -1) {
        /* Close file on mp1 */
        error = close(fd);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

/* Unit test for 42944162 which only occurs with SMB v1 */
-(void)testFileCreateSMBv1
{
    int error = 0;
    char file_path[PATH_MAX];
    int fd = -1;
    int oflag = O_EXCL | O_CREAT | O_RDWR;
    char mp1[PATH_MAX];

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Unit test for 42944162 which only occurs with SMB v1",
                               "open,close,posix_permissions,nfs_ace",
                               "1",
                               "42944162",
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with using SMB 1
     */
    do_create_mount_path(mp1, sizeof(mp1), "testFileCreateSMBv1Mp1");

    error = mount_two_sessions(mp1, NULL, 1);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create the test dirs */
    error = do_create_test_dirs(mp1);
    if (error) {
        XCTFail("do_create_test_dirs on <%s> failed %d:%s \n", mp1,
                error, strerror(error));
        goto done;
    }

    /* Set up file path */
    strlcpy(file_path, mp1, sizeof(file_path));
    strlcat(file_path, "/", sizeof(file_path));
    strlcat(file_path, cur_test_dir, sizeof(file_path));
    strlcat(file_path, "/", sizeof(file_path));
    strlcat(file_path, default_test_filename, sizeof(file_path));

    /*
     * Open the file on mp1
     */
    fd = open(file_path, oflag, S_IRWXU | S_IRWXG | S_IRWXO);
    if (fd == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }

    /* Close file */
    error = close(fd);
    if (error) {
        XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    else {
        fd = -1;
    }

    /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (fd != -1) {
        /* Close file on mp1 */
        error = close(fd);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testGetSrvrInfoPerf
{
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test performance of SMBNetFsGetServerInfo",
                               "performance,netfs_apis",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    [self measureBlock:^{
        int error;
        SMBHANDLE theConnection = NULL;
        CFDictionaryRef serverParms = NULL;
        CFMutableDictionaryRef openOptions = NULL;
        
        /* We want to force new sessions for each mount */
        openOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks);
        if (openOptions == NULL) {
            XCTFail("CFDictionaryCreateMutable failed for open options \n");
            goto done;
        }
        CFDictionarySetValue(openOptions, kNetFSForceNewSessionKey, kCFBooleanTrue);

        /* Create the session ref */
        error = SMBNetFsCreateSessionRef(&theConnection);
        if (error) {
            XCTFail("SMBNetFsCreateSessionRef failed %d:%s \n",
                    error, strerror(error));
            goto done;
        }
        
        error = do_GetServerInfo(urlRef1, theConnection,
                                 openOptions, &serverParms);
        if (error) {
            XCTFail("do_GetServerInfo failed %d:%s \n",
                    error, strerror(error));
            goto done;
        }
        
        error = do_CloseSession(theConnection);
        if (error) {
            XCTFail("do_CloseSession failed %d:%s \n",
                    error, strerror(error));
            goto done;
        }

    done:
        return;
    }];
}

-(void)testOpenSessionPerf
{
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test performance of SMBNetFsOpenSession",
                               "performance,netfs_apis",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    [self measureBlock:^{
        int error;
        SMBHANDLE theConnection = NULL;
        CFDictionaryRef sessionInfo = NULL;
        CFMutableDictionaryRef openOptions = NULL;
        
        /* We want to force new sessions for each mount */
        openOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks);
        if (openOptions == NULL) {
            XCTFail("CFDictionaryCreateMutable failed for open options \n");
            goto done;
        }
        CFDictionarySetValue(openOptions, kNetFSForceNewSessionKey, kCFBooleanTrue);
        
        /* Create the session ref */
        error = SMBNetFsCreateSessionRef(&theConnection);
        if (error) {
            XCTFail("SMBNetFsCreateSessionRef failed %d:%s \n",
                    error, strerror(error));
            goto done;
        }
        
        error = do_OpenSession(urlRef1, theConnection,
                               openOptions, &sessionInfo);
        if (error) {
            XCTFail("do_OpenSession failed %d:%s \n",
                    error, strerror(error));
            goto done;
        }
        
        error = do_CloseSession(theConnection);
        if (error) {
            XCTFail("do_CloseSession failed %d:%s \n",
                    error, strerror(error));
            goto done;
        }

    done:
        return;
    }];
}

/*
 * This test mimics what Finder does in Connect To and when checking an
 * already mounted server/share in the Finder Sidebar. These are the same
 * calls that the NetFS SMB Plugin uses
 */
-(void)testGetSrvrInfoEnumPerf
{
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test performance of SMBNetFsGetServerInfo/SMBNetFsOpenSession/smb_netshareenum",
                               "performance,netfs_apis,srvsvc",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    [self measureBlock:^{
        int error;
        SMBHANDLE theConnection = NULL;
        CFDictionaryRef serverParms = NULL;
        CFDictionaryRef sessionInfo = NULL;
        CFDictionaryRef sharePoints = NULL;
        CFMutableDictionaryRef openOptions = NULL;
        
        /* We want to force new sessions for each mount */
        openOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks);
        if (openOptions == NULL) {
            XCTFail("CFDictionaryCreateMutable failed for open options \n");
            goto done;
        }
        CFDictionarySetValue(openOptions, kNetFSForceNewSessionKey, kCFBooleanTrue);

        /* Create the session ref */
        error = SMBNetFsCreateSessionRef(&theConnection);
        if (error) {
            XCTFail("SMBNetFsCreateSessionRef failed %d:%s \n",
                    error, strerror(error));
            goto done;
        }
        
        error = do_GetServerInfo(urlRef1, theConnection,
                                 openOptions, &serverParms);
        if (error) {
            XCTFail("do_GetServerInfo failed %d:%s \n",
                    error, strerror(error));
            goto done;
        }
        
        error = do_OpenSession(urlRef1, theConnection,
                               openOptions, &sessionInfo);
        if (error) {
            XCTFail("do_OpenSession failed %d:%s \n",
                    error, strerror(error));
            goto done;
        }

        error = do_EnumerateShares(theConnection, &sharePoints);
        if (error) {
            XCTFail("do_EnumerateShares failed %d:%s \n",
                    error, strerror(error));
            goto done;
        }

        error = do_CloseSession(theConnection);
        if (error) {
            XCTFail("do_CloseSession failed %d:%s \n",
                    error, strerror(error));
            goto done;
        }

    done:
        return;
    }];
}

int createAttrList (struct attrlist *alist)
{
    memset(alist, 0, sizeof(struct attrlist));
    
    alist->bitmapcount = ATTR_BIT_MAP_COUNT;
    
    alist->commonattr = ATTR_CMN_NAME |
                        ATTR_CMN_DEVID |
                        ATTR_CMN_FSID |
                        ATTR_CMN_OBJTYPE |
                        ATTR_CMN_OBJTAG |
                        ATTR_CMN_OBJID |
                        ATTR_CMN_OBJPERMANENTID |
                        ATTR_CMN_PAROBJID |
                        ATTR_CMN_SCRIPT |
                        ATTR_CMN_CRTIME |
                        ATTR_CMN_MODTIME |
                        ATTR_CMN_CHGTIME |
                        ATTR_CMN_ACCTIME |
                        ATTR_CMN_BKUPTIME |
                        ATTR_CMN_FNDRINFO |
                        ATTR_CMN_OWNERID |
                        ATTR_CMN_GRPID |
                        ATTR_CMN_ACCESSMASK |
                        ATTR_CMN_FLAGS |
                        ATTR_CMN_USERACCESS |
                        ATTR_CMN_FILEID |
                        ATTR_CMN_PARENTID;
    
    alist->fileattr =   ATTR_FILE_LINKCOUNT |
                        ATTR_FILE_TOTALSIZE |
                        ATTR_FILE_ALLOCSIZE |
                        ATTR_FILE_IOBLOCKSIZE |
                        ATTR_FILE_DEVTYPE |
                        ATTR_FILE_DATALENGTH |
                        ATTR_FILE_DATAALLOCSIZE |
                        ATTR_FILE_RSRCLENGTH |
                        ATTR_FILE_RSRCALLOCSIZE;
    
    alist->dirattr =    ATTR_DIR_LINKCOUNT |
                        ATTR_DIR_ENTRYCOUNT |
                        ATTR_DIR_MOUNTSTATUS;
    
    alist->commonattr |=  ATTR_CMN_RETURNED_ATTRS;
        
    return 0;
}

int getAttrListBulk (int dirfd, int memSize, char *replyPtr,
                     attribute_set_t *requested_attributes)
{
    int returnCode = 0;
    struct attrlist alist;
    uint64_t bulkFlags = 0;

    returnCode = createAttrList(&alist);
    if (returnCode != 0) {
        fprintf(stderr, "Error in creating attribute list for getattrlistbulk. \n");
        return (-1);
    }
    
    memset(requested_attributes, 0, sizeof(attribute_set_t));
    requested_attributes->commonattr = alist.commonattr;
    requested_attributes->dirattr = alist.dirattr;
    requested_attributes->fileattr = alist.fileattr;
    
    bulkFlags |= FSOPT_NOFOLLOW;
    
//    if (gSetInvalOpt) {
//        bulkFlags |= FSOPT_PACK_INVAL_ATTRS;
//    }
    
    returnCode = getattrlistbulk(dirfd, &alist, replyPtr, memSize, bulkFlags);
    if (returnCode == -1) {
        fprintf(stderr, "getattrlistbulk failed %d (%s)\n",
                errno, strerror(errno));
        return  (-1);
    }
    
    if (returnCode != 0) {
        if (gVerbose) {
            printf("Number of Entries in Directory (Bulk): %d. \n", returnCode);
        }
    }
    
    return returnCode;
}

struct replyData {
    u_int32_t attr_length;
    attribute_set_t attrs_returned;
    char name[MAXPATHLEN];
    dev_t devid;
    fsid_t fsid;
    fsobj_type_t fsobj_type;
    fsobj_tag_t fsobj_tag;
    fsobj_id_t fsobj_id;
    fsobj_id_t fsobj_id_perm;
    fsobj_id_t fsobj_par_id;
    text_encoding_t script;
    struct timespec create_time;
    struct timespec mod_time;
    struct timespec change_time;
    struct timespec access_time;
    struct timespec backup_time;
    FileInfo finfo;
    FolderInfo folderInfo;
    ExtendedFileInfo exFinfo;
    ExtendedFolderInfo exFolderInfo;
    uid_t uid;
    gid_t gid;
    u_int32_t common_access;
    uint32_t common_flags;
    uint32_t user_access;
    guid_t guid;
    guid_t uuid;
    struct kauth_filesec file_sec;
    int fileSecLength;
    uint64_t common_file_id;
    uint64_t parent_id;
    struct timespec add_time;
    uint64_t file_link_count;
    off_t file_total_size;
    off_t file_alloc_size;
    u_int32_t file_io_size;
    dev_t dev_type;
    off_t file_data_length;
    off_t file_data_logic_size;
    off_t file_rsrc_fork;
    off_t file_rsrc_alloc_size;
    uint32_t dir_link_count;
    uint32_t dir_entry_count;
    uint32_t dir_mount_status;
    
};

/* Setting this value will change how many getdirentriesattr() will do only if
 * getattrlistbulk() is NOT being used. Bulk fills all the space it is given, so
 * in most cases it will get more than the number specified here, unless, of
 * course, there are less than this many entries in the directory. */
enum {
    kEntriesPerCall = 150
};

char objType[10][10] = { "none", "VREG", "VDIR", "VBLK", "VCHR", "VLNK", "VSOCK", "VFIFO" };
char cmnAccess[10][10] = { "---", "--X", "-W-", "-WX", "R--", "R-X", "RW-", "RWX" };
char objTag[30][10] = { "VT_NON", "VT_UFS", "VT_NFS", "VT_MFS", "VT_MSDOSFS", "VT_LFS", "VT_LOFS", "VT_FDESC",
    "VT_PORTAL", "VT_NULL", "VT_UMAP", "VT_KERNFS", "VT_PROCFS", "VT_AFS", "VT_ISOFS",
    "VT_MOCKFS", "VT_HFS", "VT_ZFS", "VT_DEVFS", "VT_WEBDAV", "VT_UDF", "VT_AFP",
    "VT_CDDA", "VT_CIFS", "VT_OTHER"};

void PrintDirEntriesBulk(attribute_set_t requested_attributes, int count, char *attrBuf, long *item_cnt)
{
    u_int32_t *attr_length;
    attribute_set_t *attrs_returned;
    unsigned long index = 0;
    char *tptr;
    FileInfo *finfo = NULL;
    FolderInfo *folderInfo = NULL;
    ExtendedFileInfo *exFinfo = NULL;
    ExtendedFolderInfo *exFolderInfo = NULL;
    char mode_str[20];
    fsobj_type_t obj_type = VREG;
    char *curr, *start;
    attrreference_t *attref_ptr;
    dev_t *dev_ptr;
    fsid_t *fsid_ptr;
    fsobj_type_t *fsobj_type_ptr;
    fsobj_tag_t *fsobj_tag_ptr;
    fsobj_id_t *fsobj_id_ptr;
    text_encoding_t *encoding_ptr;
    struct timespec *time_ptr;
    uid_t *uid_ptr;
    gid_t *gid_ptr;
    u_int32_t *uint32_ptr;
    u_int64_t *uint64_ptr;
    off_t *off_t_ptr;
    guid_t *guid_ptr;
    //struct kauth_filesec *file_sec_ptr;
    
    curr = (char *) attrBuf;
    
    for (index = 0; index < count; index++) {
        *item_cnt += 1;
        
        /* Save length of this attribute entry */
        start = curr;
        attr_length = (u_int32_t *) curr;
        curr += sizeof(u_int32_t);
        
        /* Save returned attributes for this attribute entry */
        attrs_returned = (attribute_set_t *) curr;
        curr += sizeof(attribute_set_t);
        
#if 0
        if (gSetInvalOpt) {
            /*
             * if FSOPT_PACK_INVAL_ATTRS was used, then all the attributes
             * we requested were returned, but some may be filled with
             * default values. Thus set attrs_returned to be the same as
             * what we requested.
             *
             * ATTR_CMN_RETURNED_ATTRS just tells us which attributes were
             * actually filled in with real values in this case. Not used for
             * now.
             */
            attrs_returned = &requested_attributes;
        }
#endif
        if (attrs_returned->commonattr & ATTR_CMN_NAME) {
            attref_ptr = (attrreference_t *) curr;
            printf("Name: %s\n", ((char *) attref_ptr) + attref_ptr->attr_dataoffset);
            curr += sizeof(attrreference_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_NAME) {
                fprintf(stderr, "Name not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_DEVID) {
            dev_ptr = (dev_t *) curr;
            printf("Dev ID: %d (0x%x)\n", *dev_ptr, *dev_ptr);
            curr += sizeof(dev_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_DEVID) {
                fprintf(stderr, "Dev ID not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_FSID) {
            fsid_ptr = (fsid_t *) curr;
            printf("FS ID: %d:%d (0x%x:0x%x)\n",
                   fsid_ptr->val[0], fsid_ptr->val[1],
                   fsid_ptr->val[0], fsid_ptr->val[1]);
            curr += sizeof(fsid_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_FSID) {
                fprintf(stderr, "FS ID not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_OBJTYPE) {
            fsobj_type_ptr = (fsobj_type_t *) curr;
            obj_type = *fsobj_type_ptr;
            printf("Object Type: %d (%s) \n", *fsobj_type_ptr, objType[*fsobj_type_ptr]);
            curr += sizeof(fsobj_type_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_OBJTYPE) {
                fprintf(stderr, "Object Type not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_OBJTAG) {
            fsobj_tag_ptr = (fsobj_tag_t *) curr;
            printf("Object Tag: %d (%s)\n", *fsobj_tag_ptr, objTag[*fsobj_tag_ptr]);
            curr += sizeof(fsobj_tag_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_OBJTAG) {
                fprintf(stderr, "Object Tag not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_OBJID) {
            fsobj_id_ptr = (fsobj_id_t *) curr;
            printf("Object ID: %d:%d (0x%x:0x%x)\n",
                   fsobj_id_ptr->fid_objno, fsobj_id_ptr->fid_generation,
                   fsobj_id_ptr->fid_objno, fsobj_id_ptr->fid_generation);
            curr += sizeof(fsobj_id_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_OBJID) {
                fprintf(stderr, "Object ID not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_OBJPERMANENTID) {
            fsobj_id_ptr = (fsobj_id_t *) curr;
            printf("Object Permanent ID: %d:%d (0x%x:0x%x)\n",
                   fsobj_id_ptr->fid_objno, fsobj_id_ptr->fid_generation,
                   fsobj_id_ptr->fid_objno, fsobj_id_ptr->fid_generation);
            curr += sizeof(fsobj_id_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_OBJPERMANENTID) {
                fprintf(stderr, "Permanent ID not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_PAROBJID) {
            fsobj_id_ptr = (fsobj_id_t *) curr;
            printf("Parent Object ID: %d:%d (0x%x:0x%x)\n",
                   fsobj_id_ptr->fid_objno, fsobj_id_ptr->fid_generation,
                   fsobj_id_ptr->fid_objno, fsobj_id_ptr->fid_generation);
            curr += sizeof(fsobj_id_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_PAROBJID) {
                fprintf(stderr, "Parent Object ID not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_SCRIPT) {
            encoding_ptr = (text_encoding_t *) curr;
            printf("Script: %d (0x%x)\n", *encoding_ptr, *encoding_ptr);
            curr += sizeof(text_encoding_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_SCRIPT) {
                fprintf(stderr, "Script not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_CRTIME) {
            time_ptr = (struct timespec *) curr;
            printf("Create Time: %s", ctime(&time_ptr->tv_sec));
            curr += sizeof(struct timespec);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_CRTIME) {
                fprintf(stderr, "Create Time not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_MODTIME) {
            time_ptr = (struct timespec *) curr;
            printf("Mod Time: %s", ctime(&time_ptr->tv_sec));
            curr += sizeof(struct timespec);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_MODTIME) {
                fprintf(stderr, "Mod Time not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_CHGTIME) {
            time_ptr = (struct timespec *) curr;
            printf("Change Time: %s", ctime(&time_ptr->tv_sec));
            curr += sizeof(struct timespec);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_CHGTIME) {
                fprintf(stderr, "Change Time not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_ACCTIME) {
            time_ptr = (struct timespec *) curr;
            printf("Access Time: %s", ctime(&time_ptr->tv_sec));
            curr += sizeof(struct timespec);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_ACCTIME) {
                fprintf(stderr, "Access Time not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_BKUPTIME) {
            time_ptr = (struct timespec *) curr;
            printf("Backup Time: %s", ctime(&time_ptr->tv_sec));
            curr += sizeof(struct timespec);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_BKUPTIME) {
                fprintf(stderr, "Backup Time not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_FNDRINFO) {
            /* print this out later */
            finfo = (FileInfo *) curr;
            folderInfo = (FolderInfo *) curr;
            curr += 16;
            
            exFinfo = (ExtendedFileInfo *) curr;
            exFolderInfo = (ExtendedFolderInfo *) curr;
            curr += 16;
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_FNDRINFO) {
                fprintf(stderr, "Finder Info not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_OWNERID) {
            uid_ptr = (uid_t *) curr;
            printf("Common Owner ID: %d (0x%x) \n", *uid_ptr, *uid_ptr);
            curr += sizeof(uid_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_OWNERID) {
                fprintf(stderr, "Owner ID not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_GRPID) {
            gid_ptr = (gid_t *) curr;
            printf("Common Group ID: %ud (0x%x) \n", *gid_ptr, *gid_ptr);
            curr += sizeof(gid_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_GRPID) {
                fprintf(stderr, "Group ID not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_ACCESSMASK) {
            uint32_ptr = (u_int32_t *) curr;
            strmode(*uint32_ptr, mode_str);
            printf("Common Access Mask: 0x%x (%s) \n", *uint32_ptr, mode_str);
            curr += sizeof(u_int32_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_ACCESSMASK) {
                fprintf(stderr, "Access Mask not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_FLAGS) {
            uint32_ptr = (u_int32_t *) curr;
            printf("Common Flags: 0x%x \n", *uint32_ptr);
            curr += sizeof(u_int32_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_FLAGS) {
                fprintf(stderr, "Flags not returned\n");
            }
        }
        
#if 0
        if (attrs_returned->commonattr & ATTR_CMN_GEN_COUNT) {
            uint32_ptr = (u_int32_t *) curr;
            printf("Common Generation Count: 0x%x \n", *uint32_ptr);
            curr += sizeof(u_int32_t);
        }
        else {
            fprintf(stderr, "Common Generation Count not returned\n");
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_DOCUMENT_ID) {
            uint32_ptr = (u_int32_t *) curr;
            printf("Common Document ID: 0x%x \n", *uint32_ptr);
            curr += sizeof(u_int32_t);
        }
        else {
            fprintf(stderr, "Common Document ID not returned\n");
        }
#endif
        
        if (attrs_returned->commonattr & ATTR_CMN_USERACCESS) {
            uint32_ptr = (u_int32_t *) curr;
            printf("Common UserAccess: 0x%x (%s)\n", *uint32_ptr, cmnAccess[*uint32_ptr]);
            curr += sizeof(u_int32_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_USERACCESS) {
                fprintf(stderr, "User Access not returned\n");
            }
        }
        
#if 0
        if (attrs_returned->commonattr & ATTR_CMN_EXTENDED_SECURITY) {
            attref_ptr = (attrreference_t *) curr;
            printf("length %d, offset %d\n", attref_ptr->attr_length, attref_ptr->attr_dataoffset);
            
            tptr = curr;
            tptr += attref_ptr->attr_dataoffset;
            
            
            file_sec_ptr = (struct kauth_filesec *) tptr;
            
            printf("magic 0x%x acl_entrycount %d acl_flags 0x%x\n",
                   file_sec_ptr->fsec_magic,
                   file_sec_ptr->fsec_acl.acl_entrycount,
                   file_sec_ptr->fsec_acl.acl_flags);
            smb_print_acl(&file_sec_ptr->fsec_acl);
            curr += sizeof(attrreference_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_EXTENDED_SECURITY) {
                fprintf(stderr, "Extended Security not returned\n");
            }
        }
#endif
        if (attrs_returned->commonattr & ATTR_CMN_UUID) {
            guid_ptr = (guid_t *) curr;
            printf("UUID: 0x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x\n",
                   guid_ptr->g_guid[0], guid_ptr->g_guid[1], guid_ptr->g_guid[2], guid_ptr->g_guid[3],
                   guid_ptr->g_guid[4], guid_ptr->g_guid[5], guid_ptr->g_guid[6], guid_ptr->g_guid[7],
                   guid_ptr->g_guid[8], guid_ptr->g_guid[9], guid_ptr->g_guid[10], guid_ptr->g_guid[11],
                   guid_ptr->g_guid[12], guid_ptr->g_guid[13], guid_ptr->g_guid[14], guid_ptr->g_guid[15]
                   );
            curr += sizeof(guid_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_UUID) {
                fprintf(stderr, "UUID not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_GRPUUID) {
            guid_ptr = (guid_t *) curr;
            printf("Group UUID: 0x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x\n",
                   guid_ptr->g_guid[0], guid_ptr->g_guid[1], guid_ptr->g_guid[2], guid_ptr->g_guid[3],
                   guid_ptr->g_guid[4], guid_ptr->g_guid[5], guid_ptr->g_guid[6], guid_ptr->g_guid[7],
                   guid_ptr->g_guid[8], guid_ptr->g_guid[9], guid_ptr->g_guid[10], guid_ptr->g_guid[11],
                   guid_ptr->g_guid[12], guid_ptr->g_guid[13], guid_ptr->g_guid[14], guid_ptr->g_guid[15]
                   );
            curr += sizeof(guid_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_GRPUUID) {
                fprintf(stderr, "Group UUID not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_FILEID) {
            uint64_ptr = (u_int64_t *) curr;
            printf("Common File ID: %lld (0x%llx) \n", *uint64_ptr, *uint64_ptr);
            curr += sizeof(u_int64_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_FILEID) {
                fprintf(stderr, "File ID not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_PARENTID) {
            uint64_ptr = (u_int64_t *) curr;
            printf("Common Parent ID: %lld (0x%llx) \n", *uint64_ptr, *uint64_ptr);
            curr += sizeof(u_int64_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_PARENTID) {
                fprintf(stderr, "Parent ID not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_FULLPATH) {
            attref_ptr = (attrreference_t *) curr;
            printf("Common Full Path: %s\n", ((char *) attref_ptr) + attref_ptr->attr_dataoffset);
            curr += sizeof(attrreference_t);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_FULLPATH) {
                fprintf(stderr, "Common Full Path not returned\n");
            }
        }
        
        if (attrs_returned->commonattr & ATTR_CMN_ADDEDTIME) {
            time_ptr = (struct timespec *) curr;
            printf("Added Time: %s", ctime(&time_ptr->tv_sec));
            curr += sizeof(struct timespec);
        }
        else {
            if (requested_attributes.commonattr & ATTR_CMN_ADDEDTIME) {
                fprintf(stderr, "Added Time not returned\n");
            }
        }
        
#if 0
        if (attrs_returned->commonattr & ATTR_CMN_DATA_PROTECT_FLAGS) {
            uint32_ptr = (u_int32_t *) curr;
            printf("Common Data Protect Flags: 0x%x (%s)\n", *uint32_ptr, cmnAccess[*uint32_ptr]);
            curr += sizeof(u_int32_t);
        }
        else {
            fprintf(stderr, "User Data Protect Flags not returned\n");
        }
#endif
        
        switch (obj_type) {
            case VLNK:
            case VREG:
                if (attrs_returned->commonattr & ATTR_CMN_FNDRINFO) {
                    if (finfo == NULL) {
                        fprintf(stderr, "Common Finder Info: finfo is NULL \n");
                    }
                    else {
                        tptr = (char*) &finfo->fileType;
                        printf("Common Finder Info: fileType <%c%c%c%c> 0x%x:0x%x:0x%x:0x%x\n",
                               tptr[0], tptr[1], tptr[2], tptr[3],
                               tptr[0], tptr[1], tptr[2], tptr[3]
                               );

                        tptr = (char*) &finfo->fileCreator;
                        printf("Common Finder Info: fileCreator <%c%c%c%c> 0x%x:0x%x:0x%x:0x%x\n",
                               tptr[0], tptr[1], tptr[2], tptr[3],
                               tptr[0], tptr[1], tptr[2], tptr[3]
                               );

                        printf("Common Finder Info: finderFlags %d\n", finfo->finderFlags);
                        printf("Common Finder Info: location v %d h %d\n",
                               finfo->location.v,
                               finfo->location.h);
                        printf("Common Finder Info: reservedField %d\n", finfo->reservedField);
                        printf("Common Finder Info: reserved %d %d %d %d\n",
                               exFinfo->reserved1[0],
                               exFinfo->reserved1[1],
                               exFinfo->reserved1[2],
                               exFinfo->reserved1[3]);
                        printf("Common Finder Info: extendedFinderFlags %d\n", exFinfo->extendedFinderFlags);
                        printf("Common Finder Info: reserved2 %d\n", exFinfo->reserved2);
                        printf("Common Finder Info: putAwayFolderID %d\n", exFinfo->putAwayFolderID);
                    }
                }
                
                if (attrs_returned->fileattr & ATTR_FILE_LINKCOUNT) {
                    uint32_ptr = (u_int32_t *) curr;
                    printf("File Link Count: %d \n", *uint32_ptr);
                    curr += sizeof(u_int32_t);
                }
                else {
                    if (requested_attributes.fileattr & ATTR_FILE_LINKCOUNT) {
                        fprintf(stderr, "File Link Count not returned\n");
                    }
                }
                
                if (attrs_returned->fileattr & ATTR_FILE_TOTALSIZE) {
                    off_t_ptr = (off_t *) curr;
                    printf("File Total Size: %lld (0x%llx)\n", *off_t_ptr, *off_t_ptr);
                    curr += sizeof(off_t);
                }
                else {
                    if (requested_attributes.fileattr & ATTR_FILE_TOTALSIZE) {
                        fprintf(stderr, "File Total Size not returned\n");
                    }
                }
                
                if (attrs_returned->fileattr & ATTR_FILE_ALLOCSIZE) {
                    off_t_ptr = (off_t *) curr;
                    printf("File Alloc Size: %lld (0x%llx)\n", *off_t_ptr, *off_t_ptr);
                    curr += sizeof(off_t);
                }
                else {
                    if (requested_attributes.fileattr & ATTR_FILE_ALLOCSIZE) {
                        fprintf(stderr, "File Alloc Size not returned\n");
                    }
                }
                
                if (attrs_returned->fileattr & ATTR_FILE_IOBLOCKSIZE) {
                    uint32_ptr = (u_int32_t *) curr;
                    printf("File IO Size: %d (0x%x)\n", *uint32_ptr, *uint32_ptr);
                    curr += sizeof(u_int32_t);
                }
                else {
                    if (requested_attributes.fileattr & ATTR_FILE_IOBLOCKSIZE) {
                        fprintf(stderr, "File IO Block Size not returned\n");
                    }
                }
                
                if (attrs_returned->fileattr & ATTR_FILE_DEVTYPE) {
                    dev_ptr = (dev_t *) curr;
                    printf("File Dev Type: %d \n", *dev_ptr);
                    curr += sizeof(dev_t);
                }
                else {
                    if (requested_attributes.fileattr & ATTR_FILE_DEVTYPE) {
                        fprintf(stderr, "File Dev Type not returned\n");
                    }
                }
                
                if (attrs_returned->fileattr & ATTR_FILE_DATALENGTH) {
                    off_t_ptr = (off_t *) curr;
                    printf("File Data Fork Logical Length: %lld (0x%llx)\n", *off_t_ptr, *off_t_ptr);
                    curr += sizeof(off_t);
                }
                else {
                    if (requested_attributes.fileattr & ATTR_FILE_DATALENGTH) {
                        fprintf(stderr, "File Data Length not returned\n");
                    }
                }
                
                if (attrs_returned->fileattr & ATTR_FILE_DATAALLOCSIZE) {
                    off_t_ptr = (off_t *) curr;
                    printf("File Data Fork Physical Length: %lld (0x%llx)\n", *off_t_ptr, *off_t_ptr);
                    curr += sizeof(off_t);
                }
                else {
                    if (requested_attributes.fileattr & ATTR_FILE_DATAALLOCSIZE) {
                        fprintf(stderr, "File Data Alloc Size not returned\n");
                    }
                }
                
                if (attrs_returned->fileattr & ATTR_FILE_RSRCLENGTH) {
                    off_t_ptr = (off_t *) curr;
                    printf("File Rsrc Fork Logical Length: %lld (0x%llx)\n", *off_t_ptr, *off_t_ptr);
                    curr += sizeof(off_t);
                }
                else {
                    if (requested_attributes.fileattr & ATTR_FILE_RSRCLENGTH) {
                        fprintf(stderr, "File Resource Length not returned\n");
                    }
                }
                
                if (attrs_returned->fileattr & ATTR_FILE_RSRCALLOCSIZE) {
                    off_t_ptr = (off_t *) curr;
                    printf("File Rsrc Fork Physical Length: %lld (0x%llx)\n", *off_t_ptr, *off_t_ptr);
                    //curr += sizeof(off_t);
                }
                else {
                    if (requested_attributes.fileattr & ATTR_FILE_RSRCALLOCSIZE) {
                        fprintf(stderr, "File Resource Alloc Size not returned\n");
                    }
                }
                
                printf("\n");
                break;
            case VDIR:
                if (attrs_returned->commonattr & ATTR_CMN_FNDRINFO) {
                    if (folderInfo == NULL) {
                        fprintf(stderr, "Common Finder Info: folderInfo is NULL \n");
                    }
                    else {
                        printf("Common Finder Info: windowBounds top %d left %d bottom %d right %d\n",
                               folderInfo->windowBounds.top,
                               folderInfo->windowBounds.left,
                               folderInfo->windowBounds.bottom,
                               folderInfo->windowBounds.right);
                        printf("Common Finder Info: finderFlags %d\n", folderInfo->finderFlags);
                        printf("Common Finder Info: location v %d h %d\n",
                               folderInfo->location.v,
                               folderInfo->location.h);
                        printf("Common Finder Info: reservedField %d\n", folderInfo->reservedField);
                        printf("Common Finder Info: scrollPosition v %d h %d\n",
                               exFolderInfo->scrollPosition.v,
                               exFolderInfo->scrollPosition.h);
                        printf("Common Finder Info: reserved1 %d\n", exFolderInfo->reserved1);
                        printf("Common Finder Info: extendedFinderFlags %d\n", exFolderInfo->extendedFinderFlags);
                        printf("Common Finder Info: reserved2 %d\n", exFolderInfo->reserved2);
                        printf("Common Finder Info: putAwayFolderID %d\n", exFolderInfo->putAwayFolderID);
                    }
                }
                
                if (attrs_returned->dirattr & ATTR_DIR_LINKCOUNT) {
                    uint32_ptr = (u_int32_t *) curr;
                    printf("Dir Link Count: %d (0x%x)\n", *uint32_ptr, *uint32_ptr);
                    curr += sizeof(u_int32_t);
                }
                else {
                    if (requested_attributes.dirattr & ATTR_DIR_LINKCOUNT) {
                        fprintf(stderr, "Dir Link Count not returned\n");
                    }
                }
                
                if (attrs_returned->dirattr & ATTR_DIR_ENTRYCOUNT) {
                    uint32_ptr = (u_int32_t *) curr;
                    printf("Dir Entry Count: %d (0x%x)\n", *uint32_ptr, *uint32_ptr);
                    curr += sizeof(u_int32_t);
                }
                else {
                    if (requested_attributes.dirattr & ATTR_DIR_ENTRYCOUNT) {
                        fprintf(stderr, "Dir Entry Count not returned\n");
                    }
                }
                
                if (attrs_returned->dirattr & ATTR_DIR_MOUNTSTATUS) {
                    uint32_ptr = (u_int32_t *) curr;
                    printf("Dir Mount Status:  %d (0x%x)", *uint32_ptr, *uint32_ptr);
                    if (*uint32_ptr) {
                        if (*uint32_ptr & DIR_MNTSTATUS_MNTPOINT) {
                            printf(", mount point");
                        }
                        if (*uint32_ptr & DIR_MNTSTATUS_TRIGGER) {
                            printf(", trigger point");
                        }
                    }
                    printf("\n");
                    //curr += sizeof(u_int32_t);
                }
                else {
                    if (requested_attributes.dirattr & ATTR_DIR_MOUNTSTATUS) {
                        fprintf(stderr, "Dir Mount Status not returned\n");
                    }
                }
                
                printf("\n");
                break;
            default:
                break;
        }
        
        // Advance to the next entry.
        curr = start + *attr_length;
    } /* End of for loop */
}

-(void)testNoXattrSupportWithNoXattrs
{
    __block int error = 0;
    char base_file_path[PATH_MAX];
    char file_path[PATH_MAX];
    int fd = -1;
    __block int i;
    __block int test_file_cnt = 1000;
    __block int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH; /* -rw-rw-rw- */
    char mp1[PATH_MAX];
    char *mount_path = mp1;

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test enumeration performance of a dir with empty files with no xattrs using getAttrListBulk()",
                               "performance,enumeration",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testNoXattrSupportWithNoXattrsMp1");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto err_out;
    }
    
    /* Create the test dirs */
    error = do_create_test_dirs(mp1);
    if (error) {
        XCTFail("do_create_test_dirs on <%s> failed %d:%s \n", mp1,
               error, strerror(error));
        goto err_out;
    }
    
    /* Set up file path */
    strlcpy(base_file_path, mp1, sizeof(base_file_path));
    strlcat(base_file_path, "/", sizeof(base_file_path));
    strlcat(base_file_path, cur_test_dir, sizeof(base_file_path));
    

    /* Create a bunch of test files to enumerate without xattrs */
    printf("Creating %d test files \n", test_file_cnt);
    for (i = 0; i < test_file_cnt; i++) {
        sprintf(file_path, "%s/%s_%d",
                base_file_path, default_test_filename, i);
        
        /* Create the test files on mp1 */
        fd = open(file_path, O_RDONLY | O_CREAT, mode);
        if (fd == -1) {
            XCTFail("Create on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            error = errno;
            goto err_out;
        }
        else {
            error = close(fd);
            if (error) {
                XCTFail("close on <%s> failed %d:%s \n", file_path,
                        errno, strerror(errno));
                error = errno;
                goto err_out;
            }
        }
    }
    
err_out:
    if (error) {
        if (unmount(mp1, MNT_FORCE) == -1) {
            XCTFail("unmount failed for first url %d\n", errno);
        }
        
        rmdir(mp1);

        return;
    }

    /* Turn down verbosity before the performance test */
    gVerbose = 0;
    
    printf("Starting performance part of test \n");

    [self measureBlock:^{
        int numReturnedBulk = 0;
        int total_returned = 0;
        int dirBulkReplySize = 0;
        char *dirBulkReplyPtr = NULL;
        attribute_set_t req_attrs = {0};
        char base_file_path[PATH_MAX];
        char file_path[PATH_MAX];
        ssize_t xattr_size = 0;
        int xattr_options = XATTR_NOFOLLOW;
        char dir_path[PATH_MAX];
        int fd = -1;
        int dir_fd = -1;
        long item_cnt = 0;

        strlcpy(base_file_path, mount_path, sizeof(base_file_path));
        strlcat(base_file_path, "/", sizeof(base_file_path));
        strlcat(base_file_path, cur_test_dir, sizeof(base_file_path));

        /* Save path to test dir */
        strlcpy(dir_path, base_file_path, sizeof(dir_path));

        /* Create a file, then delete it to invalidate any dir enum caching */
        sprintf(file_path, "%s/%s",
                base_file_path, default_test_filename);

        /* Create the test file on mp1 */
        fd = open(file_path, O_RDONLY | O_CREAT, mode);
        if (fd == -1) {
            XCTFail("Create on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            error = errno;
            goto done;
        }
        else {
            error = close(fd);
            if (error) {
                XCTFail("close on <%s> failed %d:%s \n", file_path,
                        errno, strerror(errno));
                error = errno;
                goto done;
            }
            //fd = -1;
            
            error = unlink(file_path);
            if (error) {
                XCTFail("unlink on <%s> failed %d:%s \n", file_path,
                        errno, strerror(errno));
                error = errno;
                goto done;
            }
        }

        /*
         * Open the test dir on mp1
         */
        dir_fd = open(dir_path, O_RDONLY);
        if (dir_fd == -1) {
            XCTFail("open on <%s> failed %d:%s \n", dir_path,
                    errno, strerror(errno));
            error = errno;
            goto done;
        }

        dirBulkReplySize = kEntriesPerCall * (sizeof(struct replyData)) + 1024;
        dirBulkReplyPtr = malloc(dirBulkReplySize);
        total_returned = 0;
        
        do {
            numReturnedBulk = getAttrListBulk(dir_fd, dirBulkReplySize,
                                              dirBulkReplyPtr, &req_attrs);
            if (numReturnedBulk < 0) {
                XCTFail("getAttrListBulk failed %d:%s \n",
                        errno, strerror(errno));
                error = errno;
                goto done;
            }
            
            total_returned += numReturnedBulk;
            if (gVerbose) {
                printf("Got %d entries. Total %d \n",
                       numReturnedBulk, total_returned);
                PrintDirEntriesBulk(req_attrs, numReturnedBulk,
                                    dirBulkReplyPtr, &item_cnt);
            }
        } while (numReturnedBulk != 0);
        
        if (total_returned < test_file_cnt) {
            XCTFail("Did not enumerate all the files %d != %d \n",
                    total_returned,test_file_cnt);
            error = EINVAL;
            goto done;
        }

        /* Do listxattr on all the files on mp1 */
        for (i = 0; i < test_file_cnt; i++) {
            sprintf(file_path, "%s/%s_%d",
                    base_file_path, default_test_filename, i);

            /* Do listxattr */
            xattr_size = listxattr(file_path, NULL, 0, xattr_options);
            if (xattr_size != 0) {
                XCTFail("listxattr on <%s> with no xattrs failed %zd %d:%s \n",
                        file_path, xattr_size, errno, strerror(errno));
                error = errno;
                goto done;
            }
        }
        
    done:
        if (dirBulkReplyPtr != NULL) {
            free(dirBulkReplyPtr);
            dirBulkReplyPtr = NULL;
        }
        
        /* Close test dir */
        if (dir_fd != -1) {
            error = close(dir_fd);
            if (error) {
                XCTFail("close on dir_fd failed %d:%s \n", errno, strerror(errno));
                error = errno;
            }
        }

        return;
    }];
    
    /* Delete all the files on mp1 */
    for (i = 0; i < test_file_cnt; i++) {
        sprintf(file_path, "%s/%s_%d",
                base_file_path, default_test_filename, i);
        
        /* Do delete */
        error = unlink(file_path);
        if (error) {
            fprintf(stderr, "cleanup unlink on <%s> failed %d:%s \n",
                    file_path, errno, strerror(errno));
        }
    }
    
    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);
    
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }
    
    rmdir(mp1);
}

-(void)testNoXattrSupportWithXattrs
{
    __block int error = 0;
    char base_file_path[PATH_MAX];
    char file_path[PATH_MAX];
    int fd = -1;
    __block int i;
    __block int test_file_cnt = 1000;
    __block int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH; /* -rw-rw-rw- */
    char mp1[PATH_MAX];
    char *mount_path = mp1;
#define kXattrData "test_value"
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test enumeration performance of a dir with empty files with xattrs using getAttrListBulk()",
                               "performance,enumeration",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testNoXattrSupportWithXattrsMp1");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto err_out;
    }
    
    /* Create the test dirs */
    error = do_create_test_dirs(mp1);
    if (error) {
        XCTFail("do_create_test_dirs on <%s> failed %d:%s \n", mp1,
               error, strerror(error));
        goto err_out;
    }
    
    /* Set up file path */
    strlcpy(base_file_path, mp1, sizeof(base_file_path));
    strlcat(base_file_path, "/", sizeof(base_file_path));
    strlcat(base_file_path, root_test_dir, sizeof(base_file_path));
    
    /* Add an xattr to root test dir */
    error = setxattr(base_file_path, "test_xattr", kXattrData,
                     sizeof(kXattrData), 0, XATTR_NOFOLLOW);
    if (error) {
        XCTFail("setxattr on <%s> failed %d:%s \n", base_file_path,
                errno, strerror(errno));
        error = errno;
        goto err_out;
    }

    strlcpy(base_file_path, mp1, sizeof(base_file_path));
    strlcat(base_file_path, "/", sizeof(base_file_path));
    strlcat(base_file_path, cur_test_dir, sizeof(base_file_path));

    /* Add an xattr to test dir */
    error = setxattr(base_file_path, "test_xattr", kXattrData,
                     sizeof(kXattrData), 0, XATTR_NOFOLLOW);
    if (error) {
        XCTFail("setxattr on <%s> failed %d:%s \n", base_file_path,
                errno, strerror(errno));
        error = errno;
        goto err_out;
    }

    /* Create a bunch of test files to enumerate with xattrs */
    printf("Creating %d test files \n", test_file_cnt);
    for (i = 0; i < test_file_cnt; i++) {
        sprintf(file_path, "%s/%s_%d",
                base_file_path, default_test_filename, i);
        
        /* Create the test files on mp1 */
        fd = open(file_path, O_RDONLY | O_CREAT, mode);
        if (fd == -1) {
            XCTFail("Create on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            error = errno;
            goto err_out;
        }
        else {
            error = close(fd);
            if (error) {
                XCTFail("close on <%s> failed %d:%s \n", file_path,
                        errno, strerror(errno));
                error = errno;
                goto err_out;
            }
            
            /* Add an xattr */
            error = setxattr(file_path, "test_xattr", kXattrData,
                             sizeof(kXattrData), 0, XATTR_NOFOLLOW);
            if (error) {
                XCTFail("setxattr on <%s> failed %d:%s \n", file_path,
                        errno, strerror(errno));
                error = errno;
                goto err_out;
            }
        }
    }
    
err_out:
    if (error) {
        if (unmount(mp1, MNT_FORCE) == -1) {
            XCTFail("unmount failed for first url %d\n", errno);
        }
        
        rmdir(mp1);
        
        return;
    }
    
    /* Turn down verbosity before the performance test */
    gVerbose = 0;
    
    printf("Starting performance part of test \n");

    [self measureBlock:^{
        int numReturnedBulk = 0;
        int total_returned = 0;
        int dirBulkReplySize = 0;
        char *dirBulkReplyPtr = NULL;
        attribute_set_t req_attrs = {0};
        char base_file_path[PATH_MAX];
        char file_path[PATH_MAX];
        ssize_t xattr_size = 0;
        int xattr_options = XATTR_NOFOLLOW;
        char dir_path[PATH_MAX];
        int fd = -1;
        int dir_fd = -1;
        long item_cnt = 0;
        
        strlcpy(base_file_path, mount_path, sizeof(base_file_path));
        strlcat(base_file_path, "/", sizeof(base_file_path));
        strlcat(base_file_path, cur_test_dir, sizeof(base_file_path));
        
        /* Save path to test dir */
        strlcpy(dir_path, base_file_path, sizeof(dir_path));
        
        /* Create a file, then delete it to invalidate any dir enum caching */
        sprintf(file_path, "%s/%s",
                base_file_path, default_test_filename);
        
        /* Create the test file on mp1 */
        fd = open(file_path, O_RDONLY | O_CREAT, mode);
        if (fd == -1) {
            XCTFail("Create on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            error = errno;
            goto done;
        }
        else {
            error = close(fd);
            if (error) {
                XCTFail("close on <%s> failed %d:%s \n", file_path,
                        errno, strerror(errno));
                error = errno;
                goto done;
            }
            
            error = unlink(file_path);
            if (error) {
                XCTFail("unlink on <%s> failed %d:%s \n", file_path,
                        errno, strerror(errno));
                error = errno;
                goto done;
            }
        }
        
        /*
         * Open the test dir on mp1
         */
        dir_fd = open(dir_path, O_RDONLY);
        if (dir_fd == -1) {
            XCTFail("open on <%s> failed %d:%s \n", dir_path,
                    errno, strerror(errno));
            error = errno;
            goto done;
        }
        
        dirBulkReplySize = kEntriesPerCall * (sizeof(struct replyData)) + 1024;
        dirBulkReplyPtr = malloc(dirBulkReplySize);
        total_returned = 0;

        do {
            numReturnedBulk = getAttrListBulk(dir_fd, dirBulkReplySize,
                                              dirBulkReplyPtr, &req_attrs);
            if (numReturnedBulk < 0) {
                XCTFail("getAttrListBulk failed %d:%s \n",
                        errno, strerror(errno));
                error = errno;
                goto done;
            }
            
            total_returned += numReturnedBulk;
            if (gVerbose) {
                printf("Got %d entries. Total %d \n",
                       numReturnedBulk, total_returned);
                PrintDirEntriesBulk(req_attrs, numReturnedBulk,
                                    dirBulkReplyPtr, &item_cnt);
            }
        } while (numReturnedBulk != 0);
        
        if (total_returned < test_file_cnt) {
            XCTFail("Did not enumerate all the files %d != %d \n",
                    total_returned,test_file_cnt);
            error = EINVAL;
            goto done;
        }

        /* Do listxattr on all the files on mp1 */
        for (i = 0; i < test_file_cnt; i++) {
            sprintf(file_path, "%s/%s_%d",
                    base_file_path, default_test_filename, i);
            
            /* Do listxattr */
            xattr_size = listxattr(file_path, NULL, 0, xattr_options);
            if (xattr_size != sizeof(kXattrData)) {
                XCTFail("listxattr on <%s> with no xattrs failed %zd != %lu\n",
                        file_path, xattr_size, sizeof(kXattrData));
                error = errno;
                goto done;
            }
        }
        
    done:
        if (dirBulkReplyPtr != NULL) {
            free(dirBulkReplyPtr);
            dirBulkReplyPtr = NULL;
        }

        /* Close test dir */
        if (dir_fd != -1) {
            error = close(dir_fd);
            if (error) {
                XCTFail("close on dir_fd failed %d:%s \n", errno, strerror(errno));
                error = errno;
            }
        }

        return;
    }];
    
    /* Delete all the files on mp1 */
    for (i = 0; i < test_file_cnt; i++) {
        sprintf(file_path, "%s/%s_%d",
                base_file_path, default_test_filename, i);
        
        /* Do delete */
        error = unlink(file_path);
        if (error) {
            fprintf(stderr, "cleanup unlink on <%s> failed %d:%s \n",
                    file_path, errno, strerror(errno));
        }
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);
    
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }
    
    rmdir(mp1);
}

/* Unit test for 59146570 */
-(void)testReadDirOfSymLink
{
    int error = 0;
    char dir_path[PATH_MAX];
    char test_dir_path[PATH_MAX];
    char test_file_path[PATH_MAX];
    char symlink_path[PATH_MAX];
    char stat_path[PATH_MAX];
    int fd = -1;
    int oflag = O_CREAT | O_RDWR;
    struct dirent *dp;
    DIR * dirp;
    struct stat sb;
    int item_count = 0;
    char mp1[PATH_MAX];

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test enumeration performance of a dir with files with no xattrs using getAttrListBulk()",
                               "symlink,reparse_point,readdir",
                               "1,2,3",
                               "59146570",
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with using SMB 2
     */
    do_create_mount_path(mp1, sizeof(mp1), "testReadDirOfSymLinkMp1");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create the test dirs */
    error = do_create_test_dirs(mp1);
    if (error) {
        XCTFail("do_create_test_dirs on <%s> failed %d:%s \n", mp1,
                error, strerror(error));
        goto done;
    }

    /* Set up file paths */
    strlcpy(dir_path, mp1, sizeof(dir_path));
    strlcat(dir_path, "/", sizeof(dir_path));
    strlcat(dir_path, cur_test_dir, sizeof(dir_path));

    strlcpy(test_dir_path, dir_path, sizeof(test_dir_path));
    strlcat(test_dir_path, "/", sizeof(test_dir_path));
    strlcat(test_dir_path, "test_directory", sizeof(test_dir_path));

    strlcpy(test_file_path, dir_path, sizeof(test_file_path));
    strlcat(test_file_path, "/", sizeof(test_file_path));
    strlcat(test_file_path, default_test_filename, sizeof(test_file_path));

    strlcpy(symlink_path, dir_path, sizeof(symlink_path));
    strlcat(symlink_path, "/", sizeof(symlink_path));
    strlcat(symlink_path, "test_sym_link", sizeof(symlink_path));

    /*
     * In mp1, we will create
     * 1. Directory, "test_directory"
     * 2. File, "testfile"
     * 3. Symbolic link to the file, "test_sym_link"
     */
    if (mkdir(test_dir_path, ALLPERMS) != 0) {
        XCTFail("mkdir on <%s> failed %d:%s \n", test_dir_path,
                errno, strerror(errno));
        goto done;
    }

    fd = open(test_file_path, oflag, S_IRWXU | S_IRWXG | S_IRWXO);
    if (fd == -1) {
        XCTFail("open on <%s> failed %d:%s \n", test_file_path,
                errno, strerror(errno));
        goto done;
    }

    /* Close file */
    error = close(fd);
    if (error) {
        XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    else {
        fd = -1;
    }

    /* Create the sym link to the file */
    error = symlink(test_file_path, symlink_path);
    if (error) {
        XCTFail("symlink on <%s> -> <%s> failed %d:%s \n",
                symlink_path, test_file_path, errno, strerror(errno));
        goto done;
    }

    /* Open the dir to be enumerated*/
    dirp = opendir(dir_path);
    if (dirp == NULL) {
        XCTFail("opendir on <%s> failed %d:%s \n",
                dir_path, errno, strerror(errno));
        goto done;
    }

    while ((dp = readdir(dirp)) != NULL) {
        if ((strcmp(dp->d_name, ".") == 0) || (strcmp(dp->d_name, "..") == 0)) {
            /* Skip . and .. */
            continue;
        }

        strlcpy(stat_path, mp1, sizeof(stat_path));
        strlcat(stat_path, "/", sizeof(stat_path));
        strlcat(stat_path, cur_test_dir, sizeof(stat_path));
        strlcat(stat_path, "/", sizeof(stat_path));
        strlcat(stat_path, dp->d_name, sizeof(stat_path));

        if (lstat(stat_path, &sb) != 0) {
            XCTFail("lstat on <%s> failed %d:%s \n", stat_path,
                    errno, strerror(errno));
            goto done;
        }

        printf("Checking <%s> \n", dp->d_name);
        item_count += 1;

        switch (sb.st_mode & S_IFMT) {
            case S_IFDIR:
                if (dp->d_type != DT_DIR) {
                    XCTFail("readdir type <octal %o> != lstat type <octal %o> for <%s> \n",
                            dp->d_type, sb.st_mode & S_IFMT, dp->d_name);
                    goto done;
                }
                break;

            case S_IFREG:
                if (dp->d_type != DT_REG) {
                    XCTFail("readdir type <octal %o> != lstat type <octal %o> for <%s> \n",
                            dp->d_type, sb.st_mode & S_IFMT, dp->d_name);
                    goto done;
                }
                break;

            case S_IFLNK:
                if (dp->d_type != DT_LNK) {
                    XCTFail("readdir type <octal %o> != lstat type <octal %o> for <%s> \n",
                            dp->d_type, sb.st_mode & S_IFMT, dp->d_name);
                    goto done;
                }
                break;

            default:
                XCTFail("Unexpected type for readdir type <octal %o>, lstat type <octal %o> for <%s> \n",
                        dp->d_type, sb.st_mode & S_IFMT, dp->d_name);
                goto done;
            }
    }
    (void)closedir(dirp);

    printf("Entry count <%d>\n", item_count);

    if (item_count != 3) {
        XCTFail("readdir did not find all three items, found only <%d> items \n",
                item_count);
        goto done;
    }

    /* Do the Delete on test dir */
    error = remove(test_dir_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                test_dir_path, strerror(errno), errno);
        goto done;
    }

    /* Do the Delete on test file */
    error = remove(test_file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                test_file_path, strerror(errno), errno);
        goto done;
    }
    /* Do the Delete on sym link */
    error = remove(symlink_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                symlink_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (fd != -1) {
        /* Close file on mp1 */
        error = close(fd);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

/*
 * Unit test for 67724462
 * 1. Open the file, fd1
 * 2. Write some data on fd1
 * 3. Open the file again, fd2
 * 4. Read the file and verify the data on fd2
 * 5. Close the file on fd2
 * 6. Close the file on fd1
 * 7. Open the file again, fd2
 * 8. Read the file and verify the data on fd2
 * 9. Close the file on fd2
 */
int openReadClose(const char *file_path, char *buffer, ssize_t buf_len)
{
    int error = 0, error2;
    int fd = -1;
    int oflag = O_RDONLY | O_NOFOLLOW;

    /*
     * Open the file on mp1
     */
    fd = open(file_path, oflag);
    if (fd == -1) {
        fprintf(stderr, "open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        error = errno;
        goto done;
    }
    printf("File opened for read \n");

    error = read_and_verify(fd, buffer, buf_len, 0);
    if (error) {
        goto done;
    }
    printf("Read verification passed \n");

done:
    if (fd != -1) {
        /* Close file on mp1 */
        error2 = close(fd);
        if (error2) {
            fprintf(stderr, "close on fd failed %d:%s \n",
                    error2, strerror(error2));
            if (error == 0) {
                error = error2;
            }
        }
        else {
            printf("File closed \n");
        }
    }

    return(error);
}

-(void)testUBCOpenWriteOpenReadClose
{
    int error = 0;
    char file_path[PATH_MAX];
    int fd = -1;
    int oflag = O_CREAT | O_RDWR | O_EXCL | O_TRUNC;
    char buffer[] = "abcd";
    ssize_t write_size;
    char mp1[PATH_MAX];

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Verify that Open/Write, Open/Read/Close, Close, Open/Read/Close works",
                               "open,close,read,write,UBC",
                               "1,2,3",
                               "67724462",
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with using SMB 2
     */
    do_create_mount_path(mp1, sizeof(mp1), "testUBCOpenWriteOpenReadCloseMp1");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create the test dirs */
    error = do_create_test_dirs(mp1);
    if (error) {
        XCTFail("do_create_test_dirs on <%s> failed %d:%s \n", mp1,
                error, strerror(error));
        goto done;
    }

    /* Set up file path */
    strlcpy(file_path, mp1, sizeof(file_path));
    strlcat(file_path, "/", sizeof(file_path));
    strlcat(file_path, cur_test_dir, sizeof(file_path));
    strlcat(file_path, "/", sizeof(file_path));
    strlcat(file_path, default_test_filename, sizeof(file_path));

    /*
     * Open the file on mp1
     */
    fd = open(file_path, oflag, S_IRWXU | S_IRWXG | S_IRWXO);
    if (fd == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }
    printf("File opened for read/write \n");

    /*
     * Write data to file on mp1
     */
    write_size = pwrite(fd, buffer, sizeof(buffer), 0);
    if (write_size != sizeof(buffer)) {
        XCTFail("data write failed %zd != %zd \n",
                write_size, sizeof(buffer));
        //error = EINVAL;
        goto done;
    }
    printf("Initial write passes \n");

    error = openReadClose(file_path, buffer, sizeof(buffer));
    if (error) {
        XCTFail("openReadClose on <%s> failed %d:%s \n", mp1,
                error, strerror(error));
        goto done;
    }
    printf("First openReadClose passes \n");

    /* Close file */
    error = close(fd);
    if (error) {
        XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    else {
        fd = -1;
        printf("File closed \n");
    }

    error = openReadClose(file_path, buffer, sizeof(buffer));
    if (error) {
        XCTFail("openReadClose on <%s> failed %d:%s \n", mp1,
                error, strerror(error));
        goto done;
    }
    printf("Second openReadClose passes \n");

    /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (fd != -1) {
        /* Close file on mp1 */
        error = close(fd);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

/*
 * Another unit test for 67724462
 * 1. Open the file, fd1
 * 2. Write some data on fd1
 * 3. Close fd1 to save the last close mod date and data size
 * 4. Open the file again, fd1. This should check the last close date and size.
 * 5. Write some data on fd1 to change the size in UBC
 * 6. Open the file again, fd2. This should NOT check the last close date and size.
 * 7. Read the file and verify the data on fd2
 * 8. Close the file on fd2
 * 9. Close the file on fd1
 * 10. Open the file again, fd2
 * 11. Read the file and verify the data on fd2
 * 12. Close the file on fd2
 */
-(void)testUBCOpenWriteCloseOpenWriteOpenRead
{
    int error = 0;
    char file_path[PATH_MAX];
    int fd = -1;
    char buffer[] = "abcd";
    char buffer2[] = "efghijkl";
    ssize_t write_size;
    char mp1[PATH_MAX];

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Verify that Open/Write/Close, Open/Write, Open/Read/Close, Close, Open/Read/Close works",
                               "open,close,read,write,UBC",
                               "1,2,3",
                               "67724462",
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with using SMB 2
     */
    do_create_mount_path(mp1, sizeof(mp1), "testUBCOpenWriteCloseOpenWriteOpenReadMp1");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create the test dirs */
    error = do_create_test_dirs(mp1);
    if (error) {
        XCTFail("do_create_test_dirs on <%s> failed %d:%s \n", mp1,
                error, strerror(error));
        goto done;
    }

    /* Set up file path */
    strlcpy(file_path, mp1, sizeof(file_path));
    strlcat(file_path, "/", sizeof(file_path));
    strlcat(file_path, cur_test_dir, sizeof(file_path));
    strlcat(file_path, "/", sizeof(file_path));
    strlcat(file_path, default_test_filename, sizeof(file_path));

    /*
     * Open the file on mp1
     */
    fd = open(file_path, O_CREAT | O_RDWR | O_EXCL | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
    if (fd == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }
    printf("File opened for read/write \n");

    /*
     * Write data to file on mp1
     */
    write_size = pwrite(fd, buffer, sizeof(buffer), 0);
    if (write_size != sizeof(buffer)) {
        XCTFail("data write failed %zd != %zd \n",
                write_size, sizeof(buffer));
        //error = EINVAL;
        goto done;
    }
    printf("Initial write passes \n");
    
    /* Close file */
    error = close(fd);
    if (error) {
        XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    else {
        //fd = -1;
        printf("File closed \n");
    }

    /* At this point the last close mod date and data size are saved */
    
    /*
     * Open the file on mp1 again. The last close mod date and data size
     * will get checked and nothing has changed so UBC stays valid.
     */
    fd = open(file_path, O_RDWR);
    if (fd == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }
    printf("File opened again for read/write \n");

    /*
     * Write different data to file on mp1
     */
    write_size = pwrite(fd, buffer2, sizeof(buffer2), 0);
    if (write_size != sizeof(buffer2)) {
        XCTFail("data write failed %zd != %zd \n",
                write_size, sizeof(buffer2));
        //error = EINVAL;
        goto done;
    }
    printf("Second write passes \n");

    /*
     * This next Open should NOT check the mod date and data size since its
     * not the first open call on the file.
     */
    error = openReadClose(file_path, buffer2, sizeof(buffer2));
    if (error) {
        XCTFail("openReadClose on <%s> failed %d:%s \n", mp1,
                error, strerror(error));
        goto done;
    }
    printf("First openReadClose passes \n");

    /* Close file */
    error = close(fd);
    if (error) {
        XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    else {
        fd = -1;
        printf("File closed \n");
    }

    error = openReadClose(file_path, buffer2, sizeof(buffer2));
    if (error) {
        XCTFail("openReadClose on <%s> failed %d:%s \n", mp1,
                error, strerror(error));
        goto done;
    }
    printf("Second openReadClose passes \n");

    /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (fd != -1) {
        /* Close file on mp1 */
        error = close(fd);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}


#define MAX_URL_TO_DICT_TO_URL_TEST 12

struct urlToDictToURLStrings {
    const char *url;
    const CFStringRef EscapedHost;
    const CFStringRef EscapedShare;
};

struct urlToDictToURLStrings urlToDictToURL[] = {
    /* Make sure IPv6 addresses are handled in host */
    { "smb://local1:local@[fe80::d9b6:f149:a17c:8307%25en1]/Vista-Share", CFSTR("[fe80::d9b6:f149:a17c:8307%en1]"), CFSTR("Vista-Share") },

    /* Make sure escaped chars in host are handled */
    { "smb://local:local@colley%5B3%5D._smb._tcp.local/local", CFSTR("colley[3]._smb._tcp.local"), CFSTR("local") },

    /* Make sure escaped chars in user name are handled */
    { "smb://BAD%3A%3B%2FBAD@colley2/badbad", CFSTR("colley2"), CFSTR("badbad") },

    /* Make sure escaped chars in host are handled */
    { "smb://user:password@%7e%21%40%24%3b%27%28%29/share", CFSTR("~!@$;'()"), CFSTR("share") },

    /* Make sure we handle a "/" in share name */
    { "smb://server.local/Open%2fSpace", CFSTR("server.local"), CFSTR("Open%2FSpace") },

    /* Make sure we handle "%" in share name */
    { "smb://server.local/Odd%25ShareName", CFSTR("server.local"), CFSTR("Odd%ShareName") },
    
    /* Make sure we handle share name of just "/" */
    { "smb://server.local/%2f", CFSTR("server.local"), CFSTR("%2F") },
    
    /* Make sure we handle a "/" and a "%" in share name */
    { "smb://server.local/Odd%2fShare%25Name", CFSTR("server.local"), CFSTR("Odd%2FShare%Name") },
    
    /* Make sure we handle all URL reserved chars of " !#$%&'()*+,:;<=>?@[]|" in share name */
    { "smb://server.local/%20%21%23%24%25%26%27%28%29%2a%2b%2c%3a%3b%3c%3d%3e%3f%40%5b%5d%7c", CFSTR("server.local"), CFSTR(" !#$%&'()*+,:;<=>?@[]|") },
    
    /* Make sure we handle all URL reserved chars of " !#$%&'()*+,:;<=>?@[]|" in path */
    { "smb://server.local/share/%20%21%23%24%25%26%27%28%29%2a%2b%2c%3a%3b%3c%3d%3e%3f%40%5b%5d%7c", CFSTR("server.local"), CFSTR("share/ !#$%&'()*+,:;<=>?@[]|") },
    
    /* Make sure workgroup;user round trips correctly */
    { "smb://foo;bar@server.local/share", CFSTR("server.local"), CFSTR("share") },

    /* Make sure alternate port round trips correctly */
    { "smb://foo@server.local:446/share", CFSTR("server.local"), CFSTR("share") },

    { NULL, NULL, NULL }
};

-(void)testURLToDictToURL
{
    /*
     * This tests round tripping from smb_url_to_dictionary()
     * and smb_dictionary_to_url()
     *
     * It also checks that host and share are escaped properly
     */
    CFURLRef startURL, endURL;
    CFDictionaryRef dict;
    int ii, error = 0;
    CFStringRef host = NULL;
    CFStringRef share = NULL;

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("This tests round tripping from smb_url_to_dictionary() and smb_dictionary_to_url()",
                               "smb_url_to_dictionary,smb_dictionary_to_url,url",
                               NULL,
                               NULL,
                               "no_server_required");
        return;
    }

    for (ii = 0; ii < MAX_URL_TO_DICT_TO_URL_TEST; ii++) {
        startURL = CreateSMBURL(urlToDictToURL[ii].url);
        if (!startURL) {
            error = errno;
            XCTFail("CreateSMBURL with <%s> failed, %d\n",
                    urlToDictToURL[ii].url, error);
            break;
        }
        
        /* Convert CFURL to a dictionary */
        error = smb_url_to_dictionary(startURL, &dict);
        if (error) {
            XCTFail("smb_url_to_dictionary with <%s> failed, %d\n",
                    urlToDictToURL[ii].url, error);

            CFRelease(startURL);
            break;
        }
        
        /* Get the host from the dictionary */
        host = CFDictionaryGetValue(dict, kNetFSHostKey);
        if (host == NULL) {
            XCTFail("Failed to find host in dictionary for <%s> \n",
                    urlToDictToURL[ii].url);
            CFShow(dict);
            CFShow(startURL);

            CFRelease(dict);
            CFRelease(startURL);
            break;
        }
        
        /* Verify the host is escaped out correctly */
        if (kCFCompareEqualTo == CFStringCompare (host,
                                                  urlToDictToURL[ii].EscapedHost,
                                                  kCFCompareCaseInsensitive)) {
            printf("Host in dictionary is correct for <%s> \n",
                   urlToDictToURL[ii].url);
        }
        else {
            XCTFail("Host in dictionary did not match for <%s> \n",
                    urlToDictToURL[ii].url);
            CFShow(dict);
            CFShow(urlToDictToURL[ii].EscapedHost);

            CFRelease(dict);
            CFRelease(startURL);
            break;
        }

        /* Get the share from the dictionary */
        share = CFDictionaryGetValue(dict, kNetFSPathKey);
        if (share == NULL) {
            XCTFail("Failed to find share in dictionary for <%s> \n",
                    urlToDictToURL[ii].url);
            CFShow(dict);
            CFShow(startURL);

            CFRelease(dict);
            CFRelease(startURL);
            break;
        }
        
        /* Verify the share is escaped out correctly */
        if (kCFCompareEqualTo == CFStringCompare (share, urlToDictToURL[ii].EscapedShare,
                                                  kCFCompareCaseInsensitive)) {
            printf("Share in dictionary is correct for <%s> \n",
                   urlToDictToURL[ii].url);
        }
        else {
            XCTFail("Share in dictionary did not match for <%s> \n",
                    urlToDictToURL[ii].url);
            CFShow(dict);
            CFShow(urlToDictToURL[ii].EscapedShare);

            CFRelease(dict);
            CFRelease(startURL);
            break;
        }
        
        error = smb_dictionary_to_url(dict, &endURL);
        if (error) {
            XCTFail("smb_dictionary_to_url with <%s> failed, %d\n", urlToDictToURL[ii].url,  error);
            CFShow(dict);
            CFShow(startURL);

            CFRelease(dict);
            CFRelease(startURL);
            break;
        }
        /* We only want to compare the URL string currently, may want to add more later */
        if (CFStringCompare(CFURLGetString(startURL), CFURLGetString(endURL),
                            kCFCompareCaseInsensitive) != kCFCompareEqualTo) {
            XCTFail("Round trip failed for <%s>\n", urlToDictToURL[ii].url);
            CFShow(dict);
            CFShow(startURL);
            CFShow(endURL);
            
            CFRelease(dict);
            CFRelease(startURL);
            CFRelease(endURL);
            break;
        }

        printf("Round trip was successful for <%s> \n", urlToDictToURL[ii].url);

        CFRelease(dict);
        CFRelease(startURL);
        CFRelease(endURL);
    }
}


#define MAX_URL_TO_DICT_TEST 8

struct urlToDictStrings {
    const char *url;
    const CFStringRef userAndDomain;
    const CFStringRef password;
    const CFStringRef host;
    const CFStringRef port;
    const CFStringRef path;
};

struct urlToDictStrings urlToDict[] = {
    /* Just server, .local server name */
    { "smb://server.local", NULL, NULL, CFSTR("server.local"), NULL, NULL },

    /* Empty user name, dns server name */
    { "smb://@netfs-13.apple.com", CFSTR(""), NULL, CFSTR("netfs-13.apple.com"), NULL, NULL },

    /* Empty domain and username, netbios server name */
    { "smb://;@Windoze2016", CFSTR(""), NULL, CFSTR("Windoze2016"), NULL, NULL },

    /* Empty domain, username and password, Bonjour server name */
    { "smb://;:@User's%20Macintosh._smb._tcp.local", CFSTR(""), CFSTR(""), CFSTR("User's Macintosh._smb._tcp.local"), NULL, NULL },

    /* Guest login with no password, IPv4 server name */
    { "smb://guest:@192.168.1.30/", CFSTR("guest"), CFSTR(""), CFSTR("192.168.1.30"), NULL, NULL },

    /* Workgroup, user and password. Note the "\\" instead of the ";", IPv6 server */
    { "smb://workgroup;foo:bar@[fe80::d9b6:f149:a17c:8307%25en1]", CFSTR("workgroup\\foo"), CFSTR("bar"), CFSTR("[fe80::d9b6:f149:a17c:8307%en1]"), NULL, NULL },

    /* Check for alternate port, dns server name with port */
    { "smb://workgroup;foo:bar@server.local:446", CFSTR("workgroup\\foo"), CFSTR("bar"), CFSTR("server.local"), CFSTR("446"), NULL },

    /* Check for path, IPv4 server with port */
    { "smb://workgroup;foo:bar@192.168.1.30:446/share", CFSTR("workgroup\\foo"), CFSTR("bar"), CFSTR("192.168.1.30"), CFSTR("446"), CFSTR("share") },

    { NULL, NULL, NULL }
};

-(void)testURLToDict
{
    /*
     * This tests smb_url_to_dictionary() fills in the dictionary correctly
     */
    CFURLRef startURL;
    CFDictionaryRef dict;
    int ii, error = 0;
    CFStringRef userAndDomain = NULL;
    CFStringRef password = NULL;
    CFStringRef host = NULL;
    CFStringRef port = NULL;
    CFStringRef path = NULL;

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("This tests smb_url_to_dictionary() fills in the dictionary correctly",
                               "smb_url_to_dictionary,url",
                               NULL,
                               NULL,
                               "no_server_required");
        return;
    }

    for (ii = 0; ii < MAX_URL_TO_DICT_TEST; ii++) {
        startURL = CreateSMBURL(urlToDict[ii].url);
        if (!startURL) {
            error = errno;
            XCTFail("CreateSMBURL with <%s> failed, %d\n",
                    urlToDict[ii].url, error);
            break;
        }
        
        /* Convert CFURL to a dictionary */
        error = smb_url_to_dictionary(startURL, &dict);
        if (error) {
            XCTFail("smb_url_to_dictionary with <%s> failed, %d\n",
                    urlToDict[ii].url, error);

            CFRelease(startURL);
            break;
        }
        
        /* Get the domain and user from the dictionary */
        userAndDomain = CFDictionaryGetValue(dict, kNetFSUserNameKey);
        if (urlToDict[ii].userAndDomain == NULL) {
            /* We expect no user name to be found */
            if (userAndDomain != NULL) {
                XCTFail("Username should have been NULL with <%s> \n",
                        urlToDict[ii].url);
                CFShow(dict);
                CFShow(userAndDomain);
                
                CFRelease(startURL);
                CFRelease(dict);
                break;
            }
            else {
                printf("Username in dictionary is NULL as expected for <%s> \n",
                       urlToDict[ii].url);
            }
        }
        else {
            /* Verify that user name matches */
            if (userAndDomain == NULL) {
                XCTFail("Failed to find Username in dictionary for <%s> \n",
                        urlToDict[ii].url);
                CFShow(dict);

                CFRelease(dict);
                CFRelease(startURL);
                break;
            }

            if (kCFCompareEqualTo == CFStringCompare (userAndDomain,
                                                      urlToDict[ii].userAndDomain,
                                                      kCFCompareCaseInsensitive)) {
                printf("Username in dictionary is correct for <%s> \n",
                       urlToDict[ii].url);
            }
            else {
                XCTFail("Username in dictionary did not match for <%s> \n",
                        urlToDict[ii].url);
                CFShow(dict);
                CFShow(urlToDict[ii].userAndDomain);

                CFRelease(dict);
                CFRelease(startURL);
                break;
            }
        }
        
        /* Get the password from the dictionary */
        password = CFDictionaryGetValue(dict, kNetFSPasswordKey);
        if (urlToDict[ii].password == NULL) {
            /* We expect no password to be found */
            if (password != NULL) {
                XCTFail("Password should have been NULL with <%s> \n",
                        urlToDict[ii].url);
                CFShow(dict);
                CFShow(password);
                
                CFRelease(startURL);
                CFRelease(dict);
                break;
            }
            else {
                printf("Password in dictionary is NULL as expected for <%s> \n",
                       urlToDict[ii].url);
            }
        }
        else {
            /* Verify that password matches */
            if (password == NULL) {
                XCTFail("Failed to find Password in dictionary for <%s> \n",
                        urlToDict[ii].url);
                CFShow(dict);

                CFRelease(dict);
                CFRelease(startURL);
                break;
            }

            if (kCFCompareEqualTo == CFStringCompare (password,
                                                      urlToDict[ii].password,
                                                      kCFCompareCaseInsensitive)) {
                printf("Password in dictionary is correct for <%s> \n",
                       urlToDict[ii].url);
            }
            else {
                XCTFail("Password in dictionary did not match for <%s> \n",
                        urlToDict[ii].url);
                CFShow(dict);
                CFShow(urlToDict[ii].password);

                CFRelease(dict);
                CFRelease(startURL);
                break;
            }
        }

        /* Get the host from the dictionary */
        host = CFDictionaryGetValue(dict, kNetFSHostKey);
        if (host == NULL) {
            XCTFail("Failed to find host in dictionary for <%s> \n",
                    urlToDict[ii].url);
            CFShow(dict);

            CFRelease(dict);
            CFRelease(startURL);
            break;
        }
        
        /* Verify that host matches */
        if (kCFCompareEqualTo == CFStringCompare (host,
                                                  urlToDict[ii].host,
                                                  kCFCompareCaseInsensitive)) {
            printf("Host in dictionary is correct for <%s> \n",
                   urlToDict[ii].url);
        }
        else {
            XCTFail("Host in dictionary did not match for <%s> \n",
                    urlToDict[ii].url);
            CFShow(dict);
            CFShow(urlToDict[ii].host);

            CFRelease(dict);
            CFRelease(startURL);
            break;
        }

        /* Get the port from the dictionary */
        port = CFDictionaryGetValue(dict, kNetFSAlternatePortKey);
        if (urlToDict[ii].port == NULL) {
            /* We expect no port to be found */
            if (port != NULL) {
                XCTFail("Port should have been NULL with <%s> \n",
                        urlToDict[ii].url);
                CFShow(dict);
                CFShow(port);
                
                CFRelease(startURL);
                CFRelease(dict);
                break;
            }
            else {
                printf("Port in dictionary is NULL as expected for <%s> \n",
                       urlToDict[ii].url);
            }
        }
        else {
            /* Verify that port matches */
            if (port == NULL) {
                XCTFail("Failed to find Port in dictionary for <%s> \n",
                        urlToDict[ii].url);
                CFShow(dict);

                CFRelease(dict);
                CFRelease(startURL);
                break;
            }

            if (kCFCompareEqualTo == CFStringCompare (port,
                                                      urlToDict[ii].port,
                                                      kCFCompareCaseInsensitive)) {
                printf("Port in dictionary is correct for <%s> \n",
                       urlToDict[ii].url);
            }
            else {
                XCTFail("Port in dictionary did not match for <%s> \n",
                        urlToDict[ii].url);
                CFShow(dict);
                CFShow(urlToDict[ii].port);

                CFRelease(dict);
                CFRelease(startURL);
                break;
            }
        }

        /* Get the path from the dictionary */
        path = CFDictionaryGetValue(dict, kNetFSPathKey);
        if (urlToDict[ii].path == NULL) {
            /* We expect no path to be found */
            if (path != NULL) {
                XCTFail("Path should have been NULL with <%s> \n",
                        urlToDict[ii].url);
                CFShow(dict);
                CFShow(path);
                
                CFRelease(startURL);
                CFRelease(dict);
                break;
            }
            else {
                printf("Path in dictionary is NULL as expected for <%s> \n",
                       urlToDict[ii].url);
            }
        }
        else {
            /* Verify that port matches */
            if (path == NULL) {
                XCTFail("Failed to find Path in dictionary for <%s> \n",
                        urlToDict[ii].url);
                CFShow(dict);

                CFRelease(dict);
                CFRelease(startURL);
                break;
            }

            if (kCFCompareEqualTo == CFStringCompare (path,
                                                      urlToDict[ii].path,
                                                      kCFCompareCaseInsensitive)) {
                printf("Path in dictionary is correct for <%s> \n",
                       urlToDict[ii].url);
            }
            else {
                XCTFail("Path in dictionary did not match for <%s> \n",
                        urlToDict[ii].url);
                CFShow(dict);
                CFShow(urlToDict[ii].path);

                CFRelease(dict);
                CFRelease(startURL);
                break;
            }
        }
        
        printf("URL to dictionary was successful for <%s> \n", urlToDict[ii].url);

        CFRelease(dict);
        CFRelease(startURL);
    }
}

-(void)testListSharesOnce
{
    /*
     * This tests SMBNetFsCreateSessionRef, SMBNetFsOpenSession and
     * smb_netshareenum works.
     */
    CFURLRef url = NULL;
    SMBHANDLE serverConnection = NULL;
    int error;
    CFMutableDictionaryRef OpenOptions = NULL;
    CFDictionaryRef shares = NULL;
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("This tests SMBNetFsCreateSessionRef, SMBNetFsOpenSession and smb_netshareenum works",
                               "netfs_apis,srvsvc",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    error = SMBNetFsCreateSessionRef(&serverConnection);
    if (error) {
        XCTFail("SMBNetFsCreateSessionRef failed %d for <%s>\n",
                error, g_test_url1);
        goto done;
    }
    
    url = CreateSMBURL(g_test_url1);
    if (!url) {
        error = errno;
        XCTFail("CreateSMBURL failed %d for <%s>\n",
                error, g_test_url1);
        goto done;
    }
    
    OpenOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                            &kCFTypeDictionaryKeyCallBacks,
                                            &kCFTypeDictionaryValueCallBacks);
    if (OpenOptions == NULL) {
        error = errno;
        XCTFail("CFDictionaryCreateMutable failed %d for <%s>\n",
                error, g_test_url1);
        goto done;
    }
    
    error = SMBNetFsOpenSession(url, serverConnection, OpenOptions, NULL);
    if (error) {
        XCTFail("SMBNetFsOpenSession failed %d for <%s>\n",
                error, g_test_url1);
        goto done;
    }
    
    error = smb_netshareenum(serverConnection, &shares, FALSE);
    if (error) {
        XCTFail("smb_netshareenum failed %d for <%s>\n",
                error, g_test_url1);
        goto done;
    }

    /* We should have gotten at least 1 entry of IPC$ in dictionary */
    if (CFDictionaryGetCount(shares) < 1) {
        XCTFail("Found less than one share (%ld) for <%s>? \n",
                (long)CFDictionaryGetCount(shares), g_test_url1);
        goto done;
    }
    
    /* Verify that IPC$ was returned */
    if (!CFDictionaryContainsKey(shares, @"IPC$")) {
        XCTFail("IPC$ share not found for <%s>? \n", g_test_url1);
        goto done;
    }
    
    CFShow(shares);
    printf("List shares was successful for <%s> \n", g_test_url1);

done:
    if (url) {
        CFRelease(url);
    }
    
    if (OpenOptions) {
        CFRelease(OpenOptions);
    }
    
    if (shares) {
        CFRelease(shares);
    }
    
    if (serverConnection) {
        SMBNetFsCloseSession(serverConnection);
    }
}

-(void)testListSharesOnceSMBv1
{
    /*
     * This tests SMBNetFsCreateSessionRef, SMBNetFsOpenSession and
     * smb_netshareenum works.
     *
     * This version uses SMBv1 for its test
     */
    CFURLRef url = NULL;
    SMBHANDLE serverConnection = NULL;
    int error;
    CFMutableDictionaryRef OpenOptions = NULL;
    CFDictionaryRef shares = NULL;
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("This tests SMBNetFsCreateSessionRef, SMBNetFsOpenSession and smb_netshareenum works for SMBv1",
                               "netfs_apis,srvsvc",
                               "1",
                               NULL,
                               NULL);
        return;
    }

    error = SMBNetFsCreateSessionRef(&serverConnection);
    if (error) {
        XCTFail("SMBNetFsCreateSessionRef failed %d for <%s>\n",
                error, g_test_url3);
        goto done;
    }
    
    url = CreateSMBURL(g_test_url3);
    if (!url) {
        error = errno;
        XCTFail("CreateSMBURL failed %d for <%s>\n",
                error, g_test_url3);
        goto done;
    }
    
    OpenOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                            &kCFTypeDictionaryKeyCallBacks,
                                            &kCFTypeDictionaryValueCallBacks);
    if (OpenOptions == NULL) {
        error = errno;
        XCTFail("CFDictionaryCreateMutable failed %d for <%s>\n",
                error, g_test_url3);
        goto done;
    }
    
    error = SMBNetFsOpenSession(url, serverConnection, OpenOptions, NULL);
    if (error) {
        XCTFail("SMBNetFsOpenSession failed %d for <%s>\n",
                error, g_test_url3);
        goto done;
    }
    
    error = smb_netshareenum(serverConnection, &shares, FALSE);
    if (error) {
        XCTFail("smb_netshareenum failed %d for <%s>\n",
                error, g_test_url3);
        goto done;
    }

    CFShow(shares);
    printf("List shares was successful for <%s> \n", g_test_url3);

done:
    if (url) {
        CFRelease(url);
    }
    
    if (OpenOptions) {
        CFRelease(OpenOptions);
    }
    
    if (shares) {
        CFRelease(shares);
    }
    
    if (serverConnection) {
        SMBNetFsCloseSession(serverConnection);
    }
}


#define MAX_NUMBER_TESTSTRING 6

#define TESTSTRING1 "/George/J\\im/T:?.*est"
#define TESTSTRING2 "/George/J\\im/T:?.*est/"
#define TESTSTRING3 "George/J\\im/T:?.*est/"
#define TESTSTRING4 "George/J\\im/T:?.*est"
#define TESTSTRING5 "George"
#define TESTSTRING6 "/"
#define TESTSTRING7 "/George"

const char *teststr[MAX_NUMBER_TESTSTRING] = {TESTSTRING1, TESTSTRING2, TESTSTRING3, TESTSTRING4, TESTSTRING5, TESTSTRING6 };
const char *testslashstr[MAX_NUMBER_TESTSTRING] = {TESTSTRING1, TESTSTRING2, TESTSTRING2, TESTSTRING1, TESTSTRING7, TESTSTRING6 };

static int test_path(struct smb_ctx *ctx, const char *instring, const char *comparestring, uint32_t flags)
{
    char utf8str[1024];
    char ntwrkstr[1024];
    size_t ntwrk_len = 1024;
    size_t utf8_len = 1024;

    printf("\nTesting string  %s\n", instring);
    
    ntwrk_len = 1024;
    memset(ntwrkstr, 0, 1024);
    if (smb_localpath_to_ntwrkpath(ctx, instring, strlen(instring), ntwrkstr, &ntwrk_len, flags)) {
        printf("smb_localpath_to_ntwrkpath: %s failed %d\n", instring, errno);
        return -1;
    }

    //smb_ctx_hexdump(__FUNCTION__, "network name  =", (u_char *)ntwrkstr, ntwrk_len);
    
    memset(utf8str, 0, 1024);
    if (smb_ntwrkpath_to_localpath(ctx, ntwrkstr, ntwrk_len, utf8str, &utf8_len, flags)) {
        printf("smb_ntwrkpath_to_localpath: %s failed %d\n", instring, errno);
        return -1;
    }
    
    if (strcmp(comparestring, utf8str) != 0) {
        printf("UTF8 string didn't match: %s != %s\n", instring, utf8str);
        return -1;
    }

    printf("utf8str = %s len = %ld\n", utf8str, utf8_len);
    return 0;
}

static int test_path_conversion(struct smb_ctx *ctx)
{
    int ii, failcnt = 0, passcnt = 0;
    int maxcnt = MAX_NUMBER_TESTSTRING;
    uint32_t flags = SMB_UTF_SFM_CONVERSIONS;
    
    for (ii = 0; ii < maxcnt; ii++) {
        if (test_path(ctx, teststr[ii], teststr[ii], flags) == -1) {
            failcnt++;
        }
        else {
            passcnt++;
        }
    }
    
    flags |= SMB_FULLPATH_CONVERSIONS;
    for (ii = 0; ii < maxcnt; ii++) {
        if (test_path(ctx, teststr[ii], testslashstr[ii], flags) == -1) {
            failcnt++;
        }
        else {
            passcnt++;
        }
    }

    printf("test_path_conversion: Total = %d Passed = %d Failed = %d\n",
           (maxcnt * 2), passcnt, failcnt);

    return (failcnt) ? EIO : 0;
}

-(void)testNameConversion
{
    /*
     * This tests round tripping from smb_localpath_to_ntwrkpath()
     * and smb_ntwrkpath_to_localpath()
     */
    CFURLRef url = NULL;
    void *ref = NULL;
    int error = 0;
    CFMutableDictionaryRef OpenOptions = NULL;
    CFStringRef urlString = NULL;
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("This tests round tripping from smb_localpath_to_ntwrkpath() and smb_ntwrkpath_to_localpath()",
                               "smb_localpath_to_ntwrkpath,smb_ntwrkpath_to_localpath",
                               NULL,
                               NULL,
                               NULL);
        return;
    }

    url = CreateSMBURL(g_test_url1);
    if (!url) {
        error = errno;
        XCTFail("CreateSMBURL failed %d for <%s>\n",
                error, g_test_url1);
        goto done;
    }

    OpenOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                            &kCFTypeDictionaryKeyCallBacks,
                                            &kCFTypeDictionaryValueCallBacks);
    if (OpenOptions == NULL) {
        error = errno;
        XCTFail("CFDictionaryCreateMutable failed %d for <%s>\n",
                error, g_test_url1);
        goto done;
    }
    
    ref = create_smb_ctx();
    if (ref == NULL) {
        error = errno;
        XCTFail("create_smb_ctx failed %d for <%s>\n",
                error, g_test_url1);
        goto done;
    }
    
    error = smb_open_session(ref, url, OpenOptions, NULL);
    if (error) {
        XCTFail("smb_open_session failed %d for <%s>\n",
                error, g_test_url1);
        goto done;
    }
    
    error = test_path_conversion((struct smb_ctx *)ref);
    if (error) {
        XCTFail("test_path_conversion failed %d for <%s>\n",
                error, g_test_url1);
        goto done;
    }

done:
    if (OpenOptions)
        CFRelease(OpenOptions);
    if (urlString)
        CFRelease(urlString);
    if (url)
        CFRelease(url);

    smb_ctx_done(ref);
}

-(void)testSMBOpenServerWithMountPoint
{
    /*
     * This tests testSMBOpenServerWithMountPoint works.
     */
    SMBHANDLE mpConnection = NULL;
    SMBHANDLE shareConnection = NULL;
    int error = 0;
    uint32_t status = 0;
    CFDictionaryRef shares = NULL;
    char mp1[PATH_MAX];
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("This tests testSMBOpenServerWithMountPoint works.",
                               "SMBOpenServerWithMountPoint,srvsvc",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    do_create_mount_path(mp1, sizeof(mp1), "testSMBOpenServerWithMountPointMp1");

    /* First mount a volume */
    if ((mkdir(mp1, S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST)) {
        error = errno;
        XCTFail("mkdir failed %d for <%s>\n",
                error, g_test_url1);
        goto done;
    }
    
    status = SMBOpenServerEx(g_test_url1, &mpConnection,
                             kSMBOptionNoPrompt | kSMBOptionSessionOnly);
    if (!NT_SUCCESS(status)) {
        XCTFail("SMBOpenServerEx failed 0x%x for <%s>\n",
                status, g_test_url1);
        goto done;
    }
    
    status = SMBMountShareEx(mpConnection, NULL, mp1, 0, 0, 0, 0, NULL, NULL);
    if (!NT_SUCCESS(status)) {
        XCTFail("SMBMountShareEx failed 0x%x for <%s>\n",
                status, g_test_url1);
        goto done;
    }
    
    /* Now that we have a mounted volume we run the real test */
    status = SMBOpenServerWithMountPoint(mp1, "IPC$",
                                         &shareConnection, 0);
    if (!NT_SUCCESS(status)) {
        XCTFail("SMBMountShareEx failed 0x%x for <%s>\n",
                status, g_test_url1);
        goto done;
    }
    
    error = smb_netshareenum(shareConnection, &shares, FALSE);
    if (error == 0) {
        CFShow(shares);
        printf("SMBOpenServerWithMountPoint() and list shares was successful for <%s> \n",
               g_test_url1);
    }
    
    if (shares) {
        CFRelease(shares);
    }
 
done:
    /* Now cleanup everything */
    if (shareConnection) {
        SMBReleaseServer(shareConnection);
    }
    
    if (mpConnection) {
        SMBReleaseServer(mpConnection);
    }
    
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed %d\n", errno);
    }
}

#define MAX_SID_PRINTBUFFER    256    /* Used to print out the sid in case of an error */

static
void print_ntsid(ntsid_t *sidptr, const char *printstr)
{
    char sidprintbuf[MAX_SID_PRINTBUFFER];
    char *s = sidprintbuf;
    int subs;
    uint64_t auth = 0;
    unsigned i;
    uint32_t *ip;
    size_t len;
    
    bzero(sidprintbuf, MAX_SID_PRINTBUFFER);
    for (i = 0; i < sizeof(sidptr->sid_authority); i++) {
        auth = (auth << 8) | sidptr->sid_authority[i];
    }
    s += snprintf(s, MAX_SID_PRINTBUFFER, "S-%u-%llu", sidptr->sid_kind, auth);
    
    subs = sidptr->sid_authcount;
    
    for (ip = sidptr->sid_authorities; subs--; ip++)  {
        len = MAX_SID_PRINTBUFFER - (s - sidprintbuf);
        s += snprintf(s, len, "-%u", *ip);
    }
    
    fprintf(stdout, "%s: sid = %s \n", printstr, sidprintbuf);
}

-(void)testAccountNameSID
{
    /*
     * This tests that LSAR DCE/RPC works.
     */
    ntsid_t *sid = NULL;
    SMBHANDLE serverConnection = NULL;
    uint64_t options = kSMBOptionAllowGuestAuth | kSMBOptionAllowAnonymousAuth;
    uint32_t status = 0;
    SMBServerPropertiesV1 properties;
    char *AccountName = NULL, *DomainName = NULL;
    int error;

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("This tests that LSAR DCE/RPC works",
                               "lsarpc",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    status = SMBOpenServerEx(g_test_url1, &serverConnection, options);
    if (!NT_SUCCESS(status)) {
        error = errno;
        XCTFail("SMBOpenServerEx failed %d for <%s>\n",
                error, g_test_url1);
        return;
    }
    status = SMBGetServerProperties(serverConnection, &properties, kPropertiesVersion, sizeof(properties));
    if (!NT_SUCCESS(status)) {
        error = errno;
        XCTFail("SMBGetServerProperties failed %d for <%s>\n",
                error, g_test_url1);
        return;
    }

    status = GetNetworkAccountSID(properties.serverName, &AccountName, &DomainName, &sid);
    if (!NT_SUCCESS(status)) {
        error = errno;
        XCTFail("GetNetworkAccountSID failed %d for <%s>\n",
                error, g_test_url1);
        return;
    }
    
    if (serverConnection) {
        SMBReleaseServer(serverConnection);
    }

    printf("user = %s domain = %s\n", AccountName, DomainName);
    print_ntsid(sid, "Network Users ");

    if (sid) {
        free(sid);
    }
    
    if (AccountName) {
        free(AccountName);
    }
    
    if (DomainName) {
        free(DomainName);
    }
}

static int do_mount_URL(const char *mp, CFURLRef url, CFDictionaryRef openOptions, int mntflags)
{
    SMBHANDLE theConnection = NULL;
    CFStringRef mountPoint = NULL;
    CFDictionaryRef mountInfo = NULL;
    int error;
    CFNumberRef numRef = NULL;
    CFMutableDictionaryRef mountOptions = NULL;
    
    if ((mkdir(mp, S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST)) {
        return errno;
    }

    error = SMBNetFsCreateSessionRef(&theConnection);
    if (error) {
        return error;
    }
    
    error = SMBNetFsOpenSession(url, theConnection, openOptions, NULL);
    if (error) {
        (void)SMBNetFsCloseSession(theConnection);
        return error;
    }
    
    mountOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);
    if (!mountOptions) {
        (void)SMBNetFsCloseSession(theConnection);
        return error;
    }
    
    numRef = CFNumberCreate(nil, kCFNumberIntType, &mntflags);
    if (numRef != NULL) {
        CFDictionarySetValue( mountOptions, kNetFSMountFlagsKey, numRef);
        CFRelease(numRef);
    }
    
    mountPoint = CFStringCreateWithCString(kCFAllocatorDefault, mp,
                                           kCFStringEncodingUTF8);
    if (mountPoint) {
        error = SMBNetFsMount(theConnection, url, mountPoint, mountOptions, &mountInfo, NULL, NULL);
        CFRelease(mountPoint);
        if (mountInfo) {
            CFRelease(mountInfo);
        }
    }
    else {
        error = ENOMEM;
    }
    
    CFRelease(mountOptions);
    (void)SMBNetFsCloseSession(theConnection);
    return error;
}

-(void)testMountExists
{
    int error;
    int do_unmount1 = 0;
    int do_unmount2 = 0;
    char mp1[PATH_MAX];
    char mp2[PATH_MAX];

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("This tests various mount scenarios",
                               "mount,nobrowse,automount",
                               "1,2,3",
                               "80936007",
                               NULL);
        return;
    }

    /*
     * <80936007> Need to do forced unmounts because Spotlight tends to
     * get into the mount which causes a normal unmount to return EBUSY.
     */
    
    do_create_mount_path(mp1, sizeof(mp1), "testMountExistsMp1");
    do_create_mount_path(mp2, sizeof(mp2), "testMountExistsMp2");

    /* Test one: mount no browse twice */
    printf("Test 1: mount twice with MNT_DONTBROWSE. Second mount should get EEXIST error. \n");
    
    error = do_mount_URL(mp1, urlRef1, NULL, MNT_DONTBROWSE);
    if (error) {
        XCTFail("TEST 1 mntFlags = MNT_DONTBROWSE: Couldn't mount first volume: shouldn't happen %d\n", error);
        goto done;
    }
    do_unmount1 = 1;
    
    error = do_mount_URL(mp2, urlRef1, NULL, MNT_DONTBROWSE);
    if (error != EEXIST) {
        XCTFail("TEST 1, mntFlags = MNT_DONTBROWSE: Second volume returned wrong error: %d\n", error);
        
        /* If second mount somehow succeeded, try to unmount it now. */
        if ((error == 0) && (unmount(mp2, MNT_FORCE) == -1)) {
            XCTFail("TEST 1 unmount mp2 failed %d\n", errno);
            /* try force unmount on exit */
            do_unmount2 = 1;
        }
        goto done;
    }
    
    /* Try unmount of mp1 */
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("TEST 1 unmount mp1 failed %d\n", errno);
        goto done;
    }
    do_unmount1 = 0;
    
    
    /* Test two: mount first no browse second browse */
    printf("Test 2: mount first with MNT_DONTBROWSE then second mount with browse \n");

    error = do_mount_URL(mp1, urlRef1, NULL, MNT_DONTBROWSE);
    if (error) {
        XCTFail("TEST 2, mntFlags = MNT_DONTBROWSE: Couldn't mount first volume: shouldn't happen %d\n", error);
        goto done;
    }
    do_unmount1 = 1;

    error = do_mount_URL(mp2, urlRef1, NULL, 0);
    if (error != 0) {
        XCTFail("TEST 2, mntFlags = 0: Second volume return wrong error: %d\n", error);
        goto done;
    }
    do_unmount2 = 1;

    /* Try unmount of mp1 */
   if (unmount(mp1, MNT_FORCE) == -1) {
       XCTFail("TEST 2 unmount mp1 failed %d\n", errno);
       goto done;
    }
    do_unmount1 = 0;

    /* Try unmount of mp2 */
   if (unmount(mp2, MNT_FORCE) == -1) {
       XCTFail("TEST 2 unmount mp2 failed %d\n", errno);
       goto done;
    }
    do_unmount2 = 0;

    
    /* Test three: mount first browse second no browse */
    printf("Test 3: mount first with browse then second mount with MNT_DONTBROWSE \n");

    error = do_mount_URL(mp1, urlRef1, NULL, 0);
    if (error) {
        XCTFail("TEST 3, mntFlags = 0: Couldn't mount first volume: shouldn't happen %d\n", error);
        goto done;
    }
    do_unmount1 = 1;

    error = do_mount_URL(mp2, urlRef1, NULL, MNT_DONTBROWSE);
    if (error != 0) {
        XCTFail("TEST 3, mntFlags = MNT_DONTBROWSE: Second volume return wrong error: %d\n", error);
        goto done;
    }
    do_unmount2 = 1;

    /* Try unmount of mp1 */
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("TEST 3 unmount mp1 failed %d\n", errno);
        goto done;
    }
    do_unmount1 = 0;

    /* Try unmount of mp2 */
    if (unmount(mp2, MNT_FORCE) == -1) {
        XCTFail("TEST 3 unmount mp2 failed %d\n", errno);
        goto done;
    }
    do_unmount2 = 0;

    
    /* Test four: mount first browse second browse */
    printf("Test 4: mount first with browse then second mount with browse which should get EEXIST \n");

    error = do_mount_URL(mp1, urlRef1, NULL, 0);
    if (error) {
        XCTFail("TEST 4, mntFlags = 0: Couldn't mount first volume: shouldn't happen %d\n", error);
        goto done;
    }
    do_unmount1 = 1;

    error = do_mount_URL(mp2, urlRef1, NULL, 0);
    if (error != EEXIST) {
        XCTFail("TEST 4, mntFlags = 0: Second volume return wrong error: %d\n", error);
        
        /* If second mount somehow succeeded, try to unmount it now. */
        if ((error == 0) && (unmount(mp2, MNT_FORCE) == -1)) {
            XCTFail("TEST 4 unmount mp2 failed %d\n", errno);
            /* try force unmount on exit */
            do_unmount2 = 1;
        }
        goto done;
    }

    /* Try unmount of mp1 */
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("TEST 4 unmount mp1 failed %d\n", errno);
        goto done;
    }
    do_unmount1 = 0;


    /* Test five: mount first automount second no flags */
    printf("Test 5: mount first with automount then second mount with browse \n");

    error = do_mount_URL(mp1, urlRef1, NULL, MNT_AUTOMOUNTED);
    if (error) {
        XCTFail("TEST 5, mntFlags = MNT_AUTOMOUNTED: Couldn't mount first volume: shouldn't happen %d\n", error);
        goto done;
    }
    do_unmount1 = 1;

    error = do_mount_URL(mp2, urlRef1, NULL, 0);
    if (error != 0) {
        XCTFail("TEST 5, mntFlags = 0: Second volume return wrong error: %d\n", error);
        goto done;
    }
    do_unmount2 = 1;

    /* Try unmount of mp1 */
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("TEST 5 unmount mp1 failed %d\n", errno);
        goto done;
    }
    do_unmount1 = 0;

    /* Try unmount of mp2 */
    if (unmount(mp2, MNT_FORCE) == -1) {
        XCTFail("TEST 5 unmount mp2 failed %d\n", errno);
        goto done;
    }
    do_unmount2 = 0;


    /* Test six: mount first no flags second automount */
    printf("Test 6: mount first with browse then second mount with automount \n");

    error = do_mount_URL(mp1, urlRef1, NULL, 0);
    if (error) {
        XCTFail("TEST 6, mntFlags = 0: Couldn't mount first volume: shouldn't happen %d\n", error);
        goto done;
    }
    do_unmount1 = 1;

    error = do_mount_URL(mp2, urlRef1, NULL, MNT_AUTOMOUNTED);
    if (error != 0) {
        XCTFail("TEST 6, mntFlags = MNT_AUTOMOUNTED: Second volume return wrong error: %d\n", error);
        goto done;
    }
    do_unmount2 = 1;

    /* Try unmount of mp1 */
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("TEST 5 unmount mp1 failed %d\n", errno);
        goto done;
    }
    do_unmount1 = 0;

    /* Try unmount of mp2 */
    if (unmount(mp2, MNT_FORCE) == -1) {
        XCTFail("TEST 5 unmount mp2 failed %d\n", errno);
        goto done;
    }
    do_unmount2 = 0;

    printf("All mount exists tests pass \n");
    
done:
    if (do_unmount1) {
        if (unmount(mp1, MNT_FORCE) == -1) {
            XCTFail("unmount failed for first url %d\n", errno);
        }
    }

    if (do_unmount2) {
        if (unmount(mp2, MNT_FORCE) == -1) {
            XCTFail("unmount failed for second url %d\n", errno);
        }
    }

    rmdir(mp1);
    rmdir(mp2);

    return;
}

-(void)testNetBIOSNameConversion
{
    static const struct {
        CFStringRef    proposed;
        CFStringRef    expected;
    } test_names[] = {
        { NULL , NULL },
        { CFSTR(""), NULL },
        { CFSTR("james"), CFSTR("JAMES") },
        { CFSTR("colley-xp4"), CFSTR("COLLEY-XP4") },
        //{ CFSTR("jåmes"), CFSTR("JAMES") }, /* Unknown failure */
        //{ CFSTR("iPadæøåýÐ만돌"), CFSTR("IPADAYMANDOL") }, /* Unknown failure */
        { CFSTR("longnameshouldbetruncated"), CFSTR("LONGNAMESHOULDB") }
    };

    unsigned ii;
    unsigned count = (unsigned)(sizeof(test_names) / sizeof(test_names[0]));

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("This tests NetBIOS name conversions",
                               "netbios",
                               NULL,
                               NULL,
                               "no_server_required");
        return;
    }

    for (ii = 0; ii < sizeof(test_names) / sizeof(test_names[0]); ++ii) {
        CFStringRef converted;

        converted = SMBCreateNetBIOSName(test_names[ii].proposed);
        if (converted == NULL && test_names[ii].expected == NULL) {
            /* Expected this to fail ... good! */
            --count;
            continue;
        }

        if (!converted) {
            char buf[256];

            buf[0] = '\0';
            CFStringGetCString(test_names[ii].proposed, buf, sizeof(buf), kCFStringEncodingUTF8);

            XCTFail("failed to convert '%s'\n", buf);
            continue;
        }

        // CFShow(converted);

        if (CFEqual(converted, test_names[ii].expected)) {
            /* pass */
            --count;
        }
        else {
            char buf1[256];
            char buf2[256];

            buf1[0] = buf2[0] = '\0';
            CFStringGetCString(test_names[ii].expected, buf1, sizeof(buf1), kCFStringEncodingUTF8);
            CFStringGetCString(converted, buf2, sizeof(buf2), kCFStringEncodingUTF8);

            XCTFail("expected '%s' but received '%s'\n", buf1, buf2);
        }

        CFRelease(converted);
    }
    
    if (count == 0) {
        printf("All netbios names converted successfully \n");
    }
    
#if 0
    /*
     * %%% TO DO - SOMEDAY %%%
     * rdar://78620392 (Unit test of testNetBIOSNameConversion is failing for two odd names)
     * Debugging code to try to figure out why CFSTR("jåmes") and
     * CFSTR("iPadæøåýÐ만돌").
     *
     * This code is copied from SMBCreateNetBIOSName()
     *
     * Could not figure it out, something to do with
     * the code page translations, I think.
     */
    {
        /* This is the code page for 437, cp437 and should be the right one */
        CFStringEncoding codepage = kCFStringEncodingDOSLatinUS;
        CFMutableStringRef composedName;
        CFIndex nconverted = 0;
        uint8_t name_buffer[NetBIOS_NAME_LEN];
        CFIndex nused = 0;

        composedName = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
                                                 CFSTR("jåmes"));
        CFShow(composedName);
        
        CFStringUppercase(composedName, CFLocaleGetSystem());
        CFShow(composedName);
        
        printf("codepage %d \n", codepage);

        nconverted = CFStringGetBytes(composedName,
                                      CFRangeMake(0, MIN((CFIndex) sizeof(name_buffer), CFStringGetLength(composedName))),
                                      codepage, 0 /* loss byte */, false /* no BOM */,
                                      name_buffer, sizeof(name_buffer), &nused);

        /*
         * At this point, we are supposed to have CFSTR("JAMES"), but instead
         * we end up with "J\u00c5MES".
         */

        if (nconverted == 0) {
            char buf[256];

            buf[0] = '\0';
            CFStringGetCString(composedName, buf, sizeof(buf), kCFStringEncodingUTF8);
            printf("failed to compose a NetBIOS name string from '%s'", buf);
        }
        else {
            printf("nconverted %ld \n", (long) nconverted);
            CFShow(composedName);
        }

        CFRelease(composedName);

    }
#endif
}

-(void)testIOCRequestSMBv1
{
    /*
     * This tests SMBIOC_REQUEST works.
     */
    SMBHANDLE mpConnection = NULL;
    uint32_t status = 0;
    SMBFID hFile;

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("This tests SMBIOC_REQUEST works",
                               "open,close",
                               "1",
                               "84091372",
                               NULL);
        return;
    }

    status = SMBOpenServerEx(g_test_url3, &mpConnection,
                             kSMBOptionNoPrompt);

    if (!NT_SUCCESS(status)) {
        XCTFail("SMBOpenServerEx failed 0x%x for <%s>\n",
                status, g_test_url3);
        goto done;
    }

    status = SMBCreateFile(mpConnection, root_test_dir,
        0x10000, /* dwDesiredAccess: DELETE */
        0x0007, /* dwShareMode: FILE_SHARE_ALL */
        NULL,   /* lpSecurityAttributes */
        0x0003, /* dwCreateDisposition: FILE_CREATE */
        0x1001, /* dwFlagsAndAttributes  : DELETE_ON_CLOSE*/
        &hFile);

    if (!NT_SUCCESS(status)) {
        XCTFail("SMBCreateFile failed 0x%x for <%s>\n",
                status, cur_test_dir);
        goto done;
    }

    SMBCloseFile(mpConnection, hFile);

done:
    if (mpConnection) {
        SMBReleaseServer(mpConnection);
    }
}

-(void)testIOCRWSMBv1
{
    /*
     * This tests SMBIOC_READ and SMBIOC_WRITE works.
     */
    SMBHANDLE mpConnection = NULL;
    uint32_t status = 0;
    SMBFID hFile;
    SMBFID hTestDir;
    char *readBuffer = NULL;
    size_t nbyteWritten;
    char testFilePath[PATH_MAX];

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("This tests SMBIOC_READ and SMBIOC_WRITE works",
                               "open,close,read,write",
                               "1",
                               "82309348",
                               NULL);
        return;
    }

    status = SMBOpenServerEx(g_test_url3, &mpConnection,
                             kSMBOptionNoPrompt);

    if (!NT_SUCCESS(status)) {
        XCTFail("SMBOpenServerEx failed 0x%x for <%s>\n",
                status, g_test_url3);
        goto done;
    }

    status = SMBCreateFile(mpConnection, root_test_dir,
        0x10000, /* dwDesiredAccess: DELETE */
        0x0007, /* dwShareMode: FILE_SHARE_ALL */
        NULL,   /* lpSecurityAttributes */
        0x0003, /* dwCreateDisposition: FILE_OPEN_IF */
        0x1001, /* dwFlagsAndAttributes  : DELETE_ON_CLOSE*/
        &hTestDir);

    if (!NT_SUCCESS(status)) {
        XCTFail("SMBCreateFile failed 0x%x for <%s>\n",
                status, root_test_dir);
        goto done;
    }

    /* Create test file inside of root dir */
    strlcpy(testFilePath, root_test_dir, sizeof(testFilePath));
    strlcat(testFilePath, "/", sizeof(testFilePath));
    strlcat(testFilePath, default_test_filename, sizeof(testFilePath));

    status = SMBCreateFile(mpConnection, testFilePath,
        0x10003, /* dwDesiredAccess: DELETE, READ, WRITE*/
        0x0007, /* dwShareMode: FILE_SHARE_ALL */
        NULL,   /* lpSecurityAttributes */
        0x0003, /* dwCreateDisposition: FILE_OPEN_IF */
        0x1000, /* dwFlagsAndAttributes  : DELETE_ON_CLOSE*/
        &hFile);

    if (!NT_SUCCESS(status)) {
        XCTFail("SMBCreateFile failed 0x%x for <%s>\n",
                status, testFilePath);
        goto close_dir;
    }

    status = SMBWriteFile(mpConnection, hFile, data2, 0, sizeof(data2), &nbyteWritten);

    if (!NT_SUCCESS(status) || nbyteWritten != sizeof(data2)) {
        XCTFail("SMBWriteFile failed 0x%x for <%s> nbyteWritten %zu\n",
                status, testFilePath, nbyteWritten);
        goto close_file;
    }

    /* Read and Verify the data */
    readBuffer = malloc(sizeof(data2));

    if (readBuffer == NULL) {
        fprintf(stderr, "malloc failed for read buffer \n");
        goto close_file;
    }

    status = SMBReadFile(mpConnection, hFile, readBuffer, 0, sizeof(data2), &nbyteWritten);

    if (!NT_SUCCESS(status) || nbyteWritten != sizeof(data2)) {
        XCTFail("SMBReadFile failed 0x%x for <%s> nbyteWritten %zu\n",
                status, testFilePath, nbyteWritten);
        goto close_file;
    }

    if (verify_read(data2, readBuffer, nbyteWritten)) {
        XCTFail("verify_read failed\n");
    }

close_file:
    SMBCloseFile(mpConnection, hFile);
close_dir:
    SMBCloseFile(mpConnection, hTestDir);
done:
    if (mpConnection) {
        SMBReleaseServer(mpConnection);
    }
}

-(void)testIOCFsctlSMBv1
{
    /*
     * This tests SMBIOC_FSCTL works.
     */
    SMBHANDLE mpConnection = NULL;
    uint32_t status = 0;
    SMBFID hFile = 0;
    SMBFID hTestDir = 0;
    NSString *substituteSetName = NULL;
    NSString *substituteGetName = NULL;
    char testFilePath[PATH_MAX];
    char pathBuf[PATH_MAX];
    size_t nbytesReturned = 0;
    NSUInteger usedLength = 0;
    NSRange range;
    int error = 0;
    char mp1[PATH_MAX];

    struct reparseDataBuf {
        uint32_t reparse_tag;
        uint16_t reparse_data_len;
        uint16_t reserved;
        uint16_t substitute_name_offset;
        uint16_t substitute_name_len;
        uint16_t print_name_offset;
        uint16_t print_name_len;
        uint32_t flags;

        char substitute_name[1024];
    };

    struct reparseDataBuf request;
    struct reparseDataBuf response;

    if (list_tests_with_mdata == 1) {
        /* Not valid for Windows because it needs reparse points enabled */
        do_list_test_meta_data("This tests SMBIOC_FSCTL works",
                               "open,close,symlink,reparse_point,fsctl",
                               "1",
                               "82309348",
                               "apple");
        return;
    }

    status = SMBOpenServerEx(g_test_url3, &mpConnection,
                             kSMBOptionNoPrompt);

    if (!NT_SUCCESS(status)) {
        XCTFail("SMBOpenServerEx failed 0x%x for <%s>\n",
                status, g_test_url3);
        goto done;
    }

    status = SMBCreateFile(mpConnection, root_test_dir,
        0x10000, /* dwDesiredAccess: DELETE */
        0x0007, /* dwShareMode: FILE_SHARE_ALL */
        NULL,   /* lpSecurityAttributes */
        0x0003, /* dwCreateDisposition: FILE_OPEN_IF */
        0x0001, /* dwFlagsAndAttributes  : DIRECTORY */
        &hTestDir);

    if (!NT_SUCCESS(status)) {
        XCTFail("SMBCreateFile failed 0x%x for <%s>\n",
                status, root_test_dir);
        goto done;
    }

    /* Create test file inside of root dir */
    strlcpy(testFilePath, root_test_dir, sizeof(testFilePath));
    strlcat(testFilePath, "/", sizeof(testFilePath));
    strlcat(testFilePath, default_test_filename, sizeof(testFilePath));
    
    status = SMBCreateFile(mpConnection, testFilePath,
        0x13019b, /* dwDesiredAccess */
        0x0007, /* dwShareMode: FILE_SHARE_ALL */
        NULL,   /* lpSecurityAttributes */
        0x0003, /* dwCreateDisposition: FILE_OPEN_IF */
        0x200040, /* dwFlagsAndAttributes  : OPEN_REPARSE */
        &hFile);

    if (!NT_SUCCESS(status)) {
        XCTFail("SMBCreateFile failed 0x%x for <%s>", status, testFilePath);
        goto done;
    }

    memset(&request, 0, sizeof(request));
    request.reparse_tag = 0xA000000C; /* IO_REPARSE_TAG_SYMLINK */

    /* Create symlink path inside of root dir */
    strlcpy(pathBuf, "\\", sizeof(pathBuf));
    strlcat(pathBuf, root_test_dir, sizeof(pathBuf));
    strlcat(pathBuf, "\\", sizeof(pathBuf));
    strlcat(pathBuf, "testsymlink", sizeof(pathBuf));
    
    substituteSetName = [[NSString alloc] initWithCString:pathBuf
                            encoding:NSUTF8StringEncoding];
    range = NSMakeRange(0, [substituteSetName length]);

    /* Setup Substitute Name */
    [substituteSetName getBytes:&request.substitute_name[0]
        maxLength:sizeof(request.substitute_name)
        usedLength:&usedLength encoding:NSUTF16LittleEndianStringEncoding
        options:0 range:range remainingRange:NULL];

    request.substitute_name_offset = 0;
    request.substitute_name_len = usedLength;
    
    /* Setup Print Name */
    [substituteSetName getBytes:&request.substitute_name[usedLength]
        maxLength:sizeof(request.substitute_name)
        usedLength:&usedLength encoding:NSUTF16LittleEndianStringEncoding
        options:0 range:range remainingRange:NULL];

    request.print_name_offset = usedLength;
    request.print_name_len = usedLength;

    /* reparse_len starts from Substitute Name Offset, thus the 12 */
    request.reparse_data_len = request.substitute_name_len + request.print_name_len + 12;

    /* For setting a reparse point, response size must be 0 */
    status = SMBDeviceIoControl(mpConnection, hFile,
                0x900a4, /* FSCTL_SET_REPARSE_POINT */
                &request, 20 + request.substitute_name_len + request.print_name_len,
                &response, 0, &nbytesReturned);

    if (!NT_SUCCESS(status)) {
        XCTFail("SMBDeviceIoControl FSCTL_SET_REPARSE_POINT failed 0x%x", status);
        goto done;
    }

    /* reopen file to get reparse point*/
    status = SMBCloseFile(mpConnection, hFile);

    if (!NT_SUCCESS(status)) {
        XCTFail("SMBCloseFile failed 0x%x", status);
        goto done;
    }

    status = SMBCreateFile(mpConnection, testFilePath,
        0x13019b, /* dwDesiredAccess */
        0x0007, /* dwShareMode: FILE_SHARE_ALL */
        NULL,   /* lpSecurityAttributes */
        0x0003, /* dwCreateDisposition: FILE_OPEN_IF */
        0x200040, /* dwFlagsAndAttributes : OPEN_REPARSE */
        &hFile);

    if (!NT_SUCCESS(status)) {
        XCTFail("SMBCreateFile failed 0x%x for <%s>", status, testFilePath);
        goto done;
    }

    memset(&response, 0, sizeof(response));

    status = SMBDeviceIoControl(mpConnection, hFile,
                0x900a8, /* FSCTL_GET_REPARSE_POINT */
                NULL, 0,
                &response, sizeof(response), &nbytesReturned);

    if (!NT_SUCCESS(status)) {
        XCTFail("SMBDeviceIoControl FSCTL_GET_REPARSE_POINT failed 0x%x", status);
        goto done;
    }

    substituteGetName = [[NSString alloc]initWithBytes:response.substitute_name
                                    length:response.substitute_name_len
                                    encoding:NSUTF16LittleEndianStringEncoding];

    if ([substituteGetName isEqualToString:substituteSetName] == FALSE) {
        XCTFail("substituteGetName %s not equal to substituteSetName %s",
                [substituteGetName UTF8String], [substituteSetName UTF8String]);
        goto done;
    }

done:
    if (mpConnection) {
        if (hFile)
            SMBCloseFile(mpConnection, hFile);
        if (hTestDir)
            SMBCloseFile(mpConnection, hTestDir);

        SMBReleaseServer(mpConnection);

        if (hFile || hTestDir) {
            /*
             * cleanup the server
             */
            do_create_mount_path(mp1, sizeof(mp1), "testIOCFsctlSMBv1");
            error = mount_two_sessions(mp1, NULL, 1);
            if (error) {
                XCTFail("mount_two_sessions failed %d \n", error);
                return;
            }

            if (hFile) {
                strlcpy(pathBuf, mp1, sizeof(pathBuf));
                strlcat(pathBuf, "/", sizeof(pathBuf));
                strlcat(pathBuf, testFilePath, sizeof(pathBuf));
                error = remove(pathBuf);
                if (error) {
                    fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                            pathBuf, strerror(errno), errno);
                }
            }

            if (hTestDir) {
                strlcpy(pathBuf, mp1, sizeof(pathBuf));
                strlcat(pathBuf, "/", sizeof(pathBuf));
                strlcat(pathBuf, root_test_dir, sizeof(pathBuf));
                error = remove(pathBuf);
                if (error) {
                    fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                            pathBuf, strerror(errno), errno);
                }
            }

            if (unmount(mp1, MNT_FORCE) == -1) {
                XCTFail("unmount failed for first url %d\n", errno);
            }

            rmdir(mp1);
        }
    }
}

-(void)testNamedPipeWait
{
    char *pcWspNamedPipeName = "MsFteWds";
    char mp[PATH_MAX];
    int       error  = 0;
    uint32_t  status = 0;
    SMBHANDLE mpConnection    = NULL;
    SMBHANDLE shareConnection = NULL;
    char *pcIpStart = NULL;
    char *pcIpEnd = NULL;
    char *pcServerIpAddr = NULL;
    char *pcShareName = NULL;
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Verify that FSCTL_PIPE_WAIT works on a named pipe",
                               "open,fsctl",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    // mount
    do_create_mount_path(mp, sizeof(mp), "testNamedPipeWait");

    // Extract ServerIP address and share-point name
    pcIpStart = strstr(g_test_url1, "@");
    if (!pcIpStart || (strlen(pcIpStart) < 2) ) {
        XCTFail("failed to parse pcIpStart in g_test_url1 for <%s>\n", g_test_url1);
        goto done;
    }
    pcIpStart++; // skip @
    pcIpEnd = strstr(pcIpStart, "/");
    if (!pcIpEnd) {
        XCTFail("failed to parse pcIpEnd in g_test_url1 for <%s>\n", g_test_url1);
        goto done;
    }
    size_t uIpLen = pcIpEnd - pcIpStart; // '/' holds the '\0' position
    pcServerIpAddr = malloc(uIpLen);
    strncpy(pcServerIpAddr, pcIpStart, uIpLen);
    pcServerIpAddr[uIpLen] = '\0';

    pcIpEnd++; // skip '/'
    size_t uSharePointLen = strlen(pcIpEnd);
    pcShareName = malloc(uSharePointLen + 1);
    strncpy(pcShareName, pcIpEnd, uSharePointLen);
    pcShareName[uSharePointLen] = '\0';


    /* First mount a volume */
    if ((mkdir(mp, S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST)) {
        error = errno;
        XCTFail("mkdir failed %d for <%s>\n",
                error, g_test_url1);
    }
    status = SMBOpenServerEx(g_test_url1,
                             &mpConnection,
                             kSMBOptionNoPrompt | kSMBOptionSessionOnly);
    if (!NT_SUCCESS(status)) {
        XCTFail("SMBOpenServerEx failed 0x%x for <%s>\n",
                status, g_test_url1);
        goto done;
    }

    status = SMBMountShareEx(mpConnection, NULL, mp, 0, 0, 0, 0, NULL, NULL);
    if (!NT_SUCCESS(status)) {
        XCTFail("SMBMountShareEx failed 0x%x for <%s>\n",
                status, g_test_url1);
        goto done;
    }

    /* Now that we have a mounted volume we run the real test */
    status = SMBOpenServerWithMountPoint(mp, "IPC$",
                                         &shareConnection, 0);
    if (!NT_SUCCESS(status)) {
        XCTFail("SMBMountShareEx failed 0x%x for <%s>\n",
                status, g_test_url1);
        goto done;
    }

        // Send FSCTL_PIPE_WAIT
        {
            uint16_t *puWspNamedPipeName = SMBConvertFromUTF8ToUTF16(pcWspNamedPipeName,
                                                                     strlen(pcWspNamedPipeName)+1,
                                                                     1);

            uint32_t *puMsg = calloc(1, 1024);
            uint32_t uItr = 0;
            if (!puMsg) {
                XCTFail("Can't malloc puMsg for <%s>\n", g_test_url1);
                goto done;
            }

            // Timeout
            uint32_t uTimeout = 15 * 10; // 15 sec in units of 100mSes
            puMsg[uItr++] = uTimeout;
            puMsg[uItr++] = 0;

            uint32_t uNameLength = (uint32_t)strlen(pcWspNamedPipeName) * 2; // exclude '\0'
            puMsg[uItr++] = uNameLength;

            char *pcMsg = (void*)&puMsg[uItr];
            uint32_t uChrCnt = 0;
            uint8_t uTimeoutSpecified = 0;
            pcMsg[uChrCnt++] = uTimeoutSpecified;

            pcMsg[uChrCnt++] = 0; // padding

            memcpy(&pcMsg[uChrCnt], (void*)puWspNamedPipeName, uNameLength);

            uint32_t uMsgLen = uChrCnt + uNameLength + uItr * 4;

            size_t uBytesRcved = 0;
            if (@available(macOS 13.0.0, *)) {
                status = SMBNamedPipeWait(shareConnection,              /* inConnection */
                                          -1,                           /* hNamedPipe */
                                          puMsg,                        /* inBuffer */
                                          uMsgLen,                      /* inBufferSize */
                                          NULL,                         /* outBuffer */
                                          0,                            /* outBufferSize */
                                          &uBytesRcved);                /* bytesRead */
            }
            else {
                /* Skip this test */
                printf("skipping SMBNamedPipeWait since its only support on macOS 13 or later \n");
                status = 0;
            }
            if (!NT_SUCCESS(status)) {
                XCTFail("SMBNamedPipeWait failed 0x%x for <%s>\n",
                        status, g_test_url1);
                free(puMsg);
                goto done;
            }

            free(puWspNamedPipeName);
            free(puMsg);
        }
done:
    if (pcServerIpAddr != NULL) {
        free(pcServerIpAddr);
    }
    
    if (pcShareName != NULL) {
        free(pcShareName);
    }
    
    if (unmount(mp, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp);
}


int do_create_test_file(char *mp1, char *file_path) {
    int fd = -1;
    int error = 0;

    /* Create the file and write some initial data */
    printf("Creating file \n");

    fd = open(file_path, O_EXCL | O_CREAT | O_NONBLOCK | O_RDWR, S_IRWXU);
    if (fd == -1) {
        printf("open on <%s> failed %d:%s \n", file_path,
               errno, strerror(errno));
        return(errno);
    }

    /* Write out and verify initial data */
    printf("Writing initial data \n");
    error = write_and_verify(fd, data1, sizeof(data1), 0);
    if (error) {
        printf("initial write_and_verify failed %d \n", error);
        close(fd);
        return(error);
    }

    /* Close file */
    printf("Closing created file \n");
    error = close(fd);
    if (error) {
        printf("close on fd failed %d:%s \n", errno, strerror(errno));
        return(errno);
    }
    else {
        //fd = -1;
    }

    return(0);

}

int initialFileSetup(char *mp1, char *file_path, size_t file_path_size)
{
    int fd = -1;
    int error = 0;

    /* Create the test dirs */
    error = do_create_test_dirs(mp1);
    if (error) {
        printf("do_create_test_dirs on <%s> failed %d:%s \n", mp1,
               error, strerror(error));
        return(error);
    }

    /* Set up file path */
    strlcpy(file_path, mp1, file_path_size);
    strlcat(file_path, "/", file_path_size);
    strlcat(file_path, cur_test_dir, file_path_size);
    strlcat(file_path, "/", file_path_size);
    strlcat(file_path, default_test_filename, file_path_size);

    /* Create the file and write some initial data */
    printf("Creating file \n");

    fd = open(file_path, O_EXCL | O_CREAT | O_NONBLOCK | O_RDWR, S_IRWXU);
    if (fd == -1) {
        printf("open on <%s> failed %d:%s \n", file_path,
               errno, strerror(errno));
        return(errno);
    }

    /* Write out and verify initial data */
    printf("Writing initial data \n");
    error = write_and_verify(fd, data1, sizeof(data1), 0);
    if (error) {
        printf("initial write_and_verify failed %d \n", error);
        close(fd);
        return(error);
    }

    /* Close file */
    printf("Closing created file \n");
    error = close(fd);
    if (error) {
        printf("close on fd failed %d:%s \n", errno, strerror(errno));
        return(errno);
    }
    else {
        //fd = -1;
    }

    return(0);
}

int waitForChild(pid_t child_pid)
{
    int options = 0;
    int status = 0;
    int child_error = 0;

    if (child_pid == -1) {
        /* Failed to fork? */
        printf("fork failed %d:%s \n", errno, strerror(errno));
        child_error = errno;
        return(child_error);
    }

    while (waitpid(child_pid, &status, options) == -1 ) {
        if (errno == EINTR) {
            //printf("Parent interrrupted - restarting...\n");
            continue;
        }
        else {
            printf("waitpid() failed %d:%s \n",
                    errno, strerror(errno));
            child_error = errno;
            return(child_error);
        }
    }

    if (WIFEXITED(status)) {
        child_error = WEXITSTATUS(status);
        //printf("Exited normally with status %d\n", child_error);
    }
    else if (WIFSIGNALED(status)) {
        int signum = WTERMSIG(status);
        printf("Child exited due to receiving signal %d\n", signum);
        child_error = EIO;
    }
    else if (WIFSTOPPED(status)) {
        int signum = WSTOPSIG(status);
        printf("Child stopped due to receiving signal %d\n", signum);
        child_error = EIO;
    }
    else {
        printf("Unexpected child exit status.\n");
        child_error = EIO;
    }
    
    return(child_error);
}

-(void)testMultiProcessO_EXLOCK
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int oflag2 = 0;
    int fd1 = -1;
    int fd2 = -1;
    int i1 = 0, i2 = 0;
    int expect_fail = 0;
    int expect_read1 = 0 /*, expect_read2 = 0 */;
    int expect_write1 = 0 /*, expect_write2 = 0 */;
    pid_t child_pid = 0;
    int child_error = 0;
    
    /*
     * Test for correct O_EXLOCK behavior with other processes. For
     * SMB, a file can only be opened once with O_EXLOCK and all other
     * open attempts are denied.
     *
     * 1. Open file with O_EXLOCK and all 3 variants of O_RDONLY/O_WRONLY/O_RDWR
     * 2. Disable data caching (UBC)
     * 3. Verify read/write
     * 4. With a child process, try opening the same file with all 9 variants
     *    of none/O_SHLOCK/O_EXLOCK and O_RDONLY, O_WRONLY, O_RDWR
     * 5. After each child open attempt, parent verifies read/write again
     *
     * Total variants tested: 27
     */
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test for correct O_EXLOCK behavior with other processes",
                               "open,close,read,write,O_EXLOCK",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testMultiProcessO_EXLOCK");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }
    
    for (i1 = 0; i1 < 3; i1++) {
        switch(i1) {
            case 0:
                printf("Parent opening file with O_EXLOCK | O_RDONLY \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_RDONLY;
                expect_read1 = 1;
                expect_write1 = 0;
                break;
            case 1:
                printf("Parent opening file with O_EXLOCK | O_WRONLY \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_WRONLY;
                expect_read1 = 0;
                expect_write1 = 1;
                break;
            case 2:
                printf("Parent opening file with O_EXLOCK | O_RDWR \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_RDWR;
                expect_read1 = 1;
                expect_write1 = 1;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        /* turn data caching off */
        printf("Parent disabling UBC caching \n");
        if (fcntl(fd1, F_NOCACHE, 1) != 0) {
            XCTFail( "F_NOCACHE failed %d:%s \n",
                    errno, strerror(errno));
            goto done;
        }

        if (expect_read1) {
            printf("Parent verifying read access \n");
           error = read_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }
        
        if (expect_write1) {
            printf("Parent verifying write access \n");
            error = write_and_verify(fd1, data1, sizeof(data1),
                                     expect_read1 == 1 ? 0 : 1);
            if (error) {
                XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        /* Kick off the child process now */
        child_pid = fork();
        if (child_pid != 0) {
            /*
             * Parent Testing code continues here
             */
            child_error = waitForChild(child_pid);
            if (child_error != 0) {
                XCTFail("Parent - Child test failed %d:%s \n",
                        child_error, strerror(child_error));
                goto done;
            }
            else {
                printf("Child test passed \n");
            }
            
            if (expect_read1) {
                printf("Parent re-verifying read access \n");
               error = read_and_verify(fd1, data1, sizeof(data1), 0);
                if (error) {
                    XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }
            
            if (expect_write1) {
                printf("Parent re-verifying write access \n");
                error = write_and_verify(fd1, data1, sizeof(data1),
                                         expect_read1 == 1 ? 0 : 1);
                if (error) {
                    XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }

            /* Close file */
            printf("Parent closing file \n");
            if (fd1 != -1) {
                error = close(fd1);
                if (error) {
                    printf("close on fd2 failed %d:%s \n", errno, strerror(errno));
                    //error = errno;
                    goto done;
                }
                else {
                    fd1 = -1;
                }
            }
        }
        else {
            /*
             * Child Testing code starts here
             * Do not call XCTFail, just printf
             */
            for (i2 = 0; i2 < 9; i2++) {
                /*
                 * With the child process, try opening the same file with
                 * all the various read/write permissions.
                 */
                
                switch(i2) {
                    case 0:
                        printf("     Child opening file with O_RDONLY \n");
                        oflag2 = O_NONBLOCK | O_RDONLY;
                        //expect_read2 = 1;
                        //expect_write2 = 0;
                        expect_fail = 1;
                        break;
                    case 1:
                        printf("     Child opening file with O_WRONLY \n");
                        oflag2 = O_NONBLOCK | O_WRONLY;
                        //expect_read2 = 0;
                        //expect_write2 = 1;
                        expect_fail = 1;
                        break;
                    case 2:
                        printf("     Child opening file with O_RDWR \n");
                        oflag2 = O_NONBLOCK | O_RDWR;
                        //expect_read2 = 1;
                        //expect_write2 = 1;
                        expect_fail = 1;
                        break;
                    case 3:
                        printf("     Child opening file with O_SHLOCK | O_RDONLY \n");
                        oflag2 = O_SHLOCK | O_NONBLOCK | O_RDONLY;
                        //expect_read2 = 1;
                        //expect_write2 = 0;
                        expect_fail = 1;
                        break;
                    case 4:
                        printf("     Child opening file with O_SHLOCK | O_WRONLY \n");
                        oflag2 = O_SHLOCK | O_NONBLOCK | O_WRONLY;
                        //expect_read2 = 0;
                        //expect_write2 = 1;
                        expect_fail = 1;
                        break;
                    case 5:
                        printf("     Child opening file with O_SHLOCK | O_RDWR \n");
                        oflag2 = O_SHLOCK | O_NONBLOCK | O_RDWR;
                        //expect_read2 = 1;
                        //expect_write2 = 1;
                        expect_fail = 1;
                        break;
                    case 6:
                        printf("     Child opening file with O_EXLOCK | O_RDONLY \n");
                        oflag2 = O_EXLOCK | O_NONBLOCK | O_RDONLY;
                        //expect_read2 = 1;
                        //expect_write2 = 0;
                        expect_fail = 1;
                        break;
                    case 7:
                        printf("     Child opening file with O_EXLOCK | O_WRONLY \n");
                        oflag2 = O_EXLOCK | O_NONBLOCK | O_WRONLY;
                        //expect_read2 = 0;
                        //expect_write2 = 1;
                        expect_fail = 1;
                        break;
                    case 8:
                        printf("     Child opening file with O_EXLOCK | O_RDWR \n");
                        oflag2 = O_EXLOCK | O_NONBLOCK | O_RDWR;
                        //expect_read2 = 1;
                        //expect_write2 = 1;
                        expect_fail = 1;
                        break;
                    default:
                        printf("     Unknown selector for i2 %d \n", i2);
                        exit(EINVAL);
                }
                
                /*
                 * Open the testfile in child process
                 */
                fd2 = open(file_path, oflag2);
                if (fd2 == -1) {
                    if (expect_fail) {
                        /* Expected Failure */
                    }
                    else {
                        /* Unexpected failure */
                        printf("     Unexpected failure - open on <%s> failed %d:%s \n", file_path,
                                errno, strerror(errno));
                        exit(errno);
                    }
                }
                else {
                    if (expect_fail) {
                        /* Unexpected success */
                        printf("     Unexpected success - open on <%s> worked \n", file_path);
                        exit(EINVAL);
                    }
                    else {
                        /* Expected success */
                    }
                }

                /* Close file if needed */
                if (fd2 != -1) {
                    printf("     Child closing file \n");
                    error = close(fd2);
                    if (error) {
                        printf("     close on fd2 failed %d:%s \n", errno, strerror(errno));
                        exit(errno);
                    }
                    else {
                        //fd2 = -1;
                    }
                }
            } /* i2 loop */
        
            /*
             * Parents always have to clean up after this children,
             * so child just exits and leaves cleanup to parent process.
             */
            exit(0);
        } /* Child test code */
    } /* i1 loop */
        
    
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testMultiProcessO_SHLOCK
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int oflag2 = 0;
    int fd1 = -1;
    int fd2 = -1;
    int i1 = 0, i2 = 0;
    int expect_fail = 0;
    int expect_read1 = 0, expect_read2 = 0;
    int expect_write1 = 0, expect_write2 = 0;
    pid_t child_pid = 0;
    int child_error = 0;
    
    /*
     * Test for correct O_SHLOCK behavior with other processes. For
     * SMB, a file opened with O_SHLOCK will deny other open attempts
     * that request write access.
     *
     * 1. Open file with O_SHLOCK and all 3 variants of O_RDONLY/O_WRONLY/O_RDWR
     * 2. Disable data caching (UBC)
     * 3. Verify read/write
     * 4. With a child process, try opening the same file with all 9 variants
     *    of none/O_SHLOCK/O_EXLOCK and O_RDONLY, O_WRONLY, O_RDWR
     * 5. If the child is allowed to open the file, then verify read/write and
     *    close the file
     * 6. After each child open attempt, parent verifies read/write again
     *
     * Total variants tested: 27
     */
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test for correct O_SHLOCK behavior with other processes",
                               "open,close,read,write,O_SHLOCK",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testMultiProcessO_SHLOCK");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    for (i1 = 0; i1 < 3; i1++) {
        switch(i1) {
            case 0:
                printf("Parent opening file with O_SHLOCK | O_RDONLY \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDONLY;
                expect_read1 = 1;
                expect_write1 = 0;
                break;
            case 1:
                printf("Parent opening file with O_SHLOCK | O_WRONLY \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_WRONLY;
                expect_read1 = 0;
                expect_write1 = 1;
                break;
            case 2:
                printf("Parent opening file with O_SHLOCK | O_RDWR \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDWR;
                expect_read1 = 1;
                expect_write1 = 1;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        /* turn data caching off */
        printf("Parent disabling UBC caching \n");
        if (fcntl(fd1, F_NOCACHE, 1) != 0) {
            XCTFail( "F_NOCACHE failed %d:%s \n",
                    errno, strerror(errno));
            goto done;
        }

        if (expect_read1) {
            printf("Parent verifying read access \n");
           error = read_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }
        
        if (expect_write1) {
            printf("Parent verifying write access \n");
            error = write_and_verify(fd1, data1, sizeof(data1),
                                     expect_read1 == 1 ? 0 : 1);
            if (error) {
                XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        /* Kick off the child process now */
        child_pid = fork();
        if (child_pid != 0) {
            /*
             * Parent Testing code continues here
             */
            child_error = waitForChild(child_pid);
            if (child_error != 0) {
                XCTFail("Parent - Child test failed %d:%s \n",
                        child_error, strerror(child_error));
                goto done;
            }
            else {
                printf("Child test passed \n");
            }
            
            if (expect_read1) {
                printf("Parent re-verifying read access \n");
               error = read_and_verify(fd1, data1, sizeof(data1), 0);
                if (error) {
                    XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }
            
            if (expect_write1) {
                printf("Parent re-verifying write access \n");
                error = write_and_verify(fd1, data1, sizeof(data1),
                                         expect_read1 == 1 ? 0 : 1);
                if (error) {
                    XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }

            /* Close file */
            printf("Parent closing file \n");
            if (fd1 != -1) {
                error = close(fd1);
                if (error) {
                    printf("close on fd2 failed %d:%s \n", errno, strerror(errno));
                    //error = errno;
                    goto done;
                }
                else {
                    fd1 = -1;
                }
            }
        }
        else {
            /*
             * Child Testing code starts here
             * Do not call XCTFail, just printf
             */
            for (i2 = 0; i2 < 9; i2++) {
                /*
                 * With the child process, try opening the same file with
                 * all the various read/write permissions.
                 */
                
                switch(i2) {
                    case 0:
                        printf("     Child opening file with O_RDONLY \n");
                        oflag2 = O_NONBLOCK | O_RDONLY;
                        expect_read2 = 1;
                        expect_write2 = 0;
                        expect_fail = 0;
                        break;
                    case 1:
                        printf("     Child opening file with O_WRONLY \n");
                        oflag2 = O_NONBLOCK | O_WRONLY;
                        expect_read2 = 0;
                        expect_write2 = 1;
                        expect_fail = 1;
                        break;
                    case 2:
                        printf("     Child opening file with O_RDWR \n");
                        oflag2 = O_NONBLOCK | O_RDWR;
                        expect_read2 = 1;
                        expect_write2 = 1;
                        expect_fail = 1;
                        break;
                    case 3:
                        printf("     Child opening file with O_SHLOCK | O_RDONLY \n");
                        oflag2 = O_SHLOCK | O_NONBLOCK | O_RDONLY;
                        expect_read2 = 1;
                        expect_write2 = 0;
                        if (oflag1 & (O_WRONLY | O_RDWR)) {
                            /* First open has write access so O_SHLOCK will fail */
                            expect_fail = 1;
                        }
                        else {
                            expect_fail = 0;
                        }
                        break;
                    case 4:
                        printf("     Child opening file with O_SHLOCK | O_WRONLY \n");
                        oflag2 = O_SHLOCK | O_NONBLOCK | O_WRONLY;
                        expect_read2 = 0;
                        expect_write2 = 1;
                        expect_fail = 1;
                        break;
                    case 5:
                        printf("     Child opening file with O_SHLOCK | O_RDWR \n");
                        oflag2 = O_SHLOCK | O_NONBLOCK | O_RDWR;
                        expect_read2 = 1;
                        expect_write2 = 1;
                        expect_fail = 1;
                        break;
                    case 6:
                        printf("     Child opening file with O_EXLOCK | O_RDONLY \n");
                        oflag2 = O_EXLOCK | O_NONBLOCK | O_RDONLY;
                        expect_read2 = 1;
                        expect_write2 = 0;
                        expect_fail = 1;
                        break;
                    case 7:
                        printf("     Child opening file with O_EXLOCK | O_WRONLY \n");
                        oflag2 = O_EXLOCK | O_NONBLOCK | O_WRONLY;
                        expect_read2 = 0;
                        expect_write2 = 1;
                        expect_fail = 1;
                        break;
                    case 8:
                        printf("     Child opening file with O_EXLOCK | O_RDWR \n");
                        oflag2 = O_EXLOCK | O_NONBLOCK | O_RDWR;
                        expect_read2 = 1;
                        expect_write2 = 1;
                        expect_fail = 1;
                        break;
                    default:
                        printf("     Unknown selector for i2 %d \n", i2);
                        _Exit(EINVAL);
                }
                
                /*
                 * Open the testfile in child process
                 */
                fd2 = open(file_path, oflag2);
                if (fd2 == -1) {
                    if (expect_fail) {
                        /* Expected Failure */
                    }
                    else {
                        /* Unexpected failure */
                        printf("     Unexpected failure - open on <%s> failed %d:%s \n", file_path,
                                errno, strerror(errno));
                        _Exit(errno);
                    }
                }
                else {
                    if (expect_fail) {
                        /* Unexpected success */
                        printf("     Unexpected success - open on <%s> worked \n", file_path);
                        _Exit(EINVAL);
                    }
                    else {
                        /* Expected success */
                    }
                }

                if (expect_fail == 0) {
                    /* turn data caching off */
                    printf("     Child disabling UBC caching \n");
                    if (fcntl(fd2, F_NOCACHE, 1) != 0) {
                        printf( "     Child F_NOCACHE failed %d:%s \n",
                               errno, strerror(errno));
                        _Exit(errno);
                    }

                    if (expect_read2) {
                        printf("     Child verifying read access \n");
                        error = read_and_verify(fd2, data1, sizeof(data1), 0);
                        if (error) {
                            printf("     read_and_verify on <%s> failed %d:%s \n", file_path,
                                   error, strerror(error));
                            _Exit(error);
                        }
                    }
                    
                    if (expect_write2) {
                        printf("     Child verifying write access \n");
                        error = write_and_verify(fd2, data1, sizeof(data1),
                                                 expect_read1 == 1 ? 0 : 1);
                        if (error) {
                            printf("     write_and_verify on <%s> failed %d:%s \n", file_path,
                                   error, strerror(error));
                            _Exit(error);
                        }
                    }
                }
                
                /* Close file if needed */
                if (fd2 != -1) {
                    printf("     Child closing file \n");
                    error = close(fd2);
                    if (error) {
                        printf("     close on fd2 failed %d:%s \n", errno, strerror(errno));
                        _Exit(errno);
                    }
                    else {
                        //fd2 = -1;
                    }
                }
            } /* i2 loop */
        
            /*
             * Parents always have to clean up after this children,
             * so child just exits and leaves cleanup to parent process.
             */
            _Exit(0);
        } /* Child test code */
    } /* i1 loop */
        
    
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testSingleProcessO_EXLOCK
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int oflag2 = 0;
    int fd1 = -1;
    int fd2 = -1;
    int i1 = 0, i2 = 0;
    int expect_fail = 0;
    int expect_read1 = 0 /*, expect_read2 = 0 */;
    int expect_write1 = 0 /*, expect_write2 = 0*/;
    
    /*
     * Test for correct O_EXLOCK behavior all in a single process. For
     * SMB, a file can only be opened once with O_EXLOCK and all other
     * open attempts are denied.
     *
     * 1. Open file with O_EXLOCK and all 3 variants of O_RDONLY/O_WRONLY/O_RDWR
     * 2. Disable data caching (UBC)
     * 3. Verify read/write
     * 4. With same process, try opening the same file with all 9 variants
     *    of none/O_SHLOCK/O_EXLOCK and O_RDONLY, O_WRONLY, O_RDWR
     * 5. After each open attempt, verify read/write again
     *
     * Total variants tested: 27
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test for correct O_EXLOCK behavior all in a single process",
                               "open,close,read,write,O_EXLOCK",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testSingleProcessO_EXLOCK");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    for (i1 = 0; i1 < 3; i1++) {
        switch(i1) {
            case 0:
                printf("Parent opening file with O_EXLOCK | O_RDONLY \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_RDONLY;
                expect_read1 = 1;
                expect_write1 = 0;
                break;
            case 1:
                printf("Parent opening file with O_EXLOCK | O_WRONLY \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_WRONLY;
                expect_read1 = 0;
                expect_write1 = 1;
                break;
            case 2:
                printf("Parent opening file with O_EXLOCK | O_RDWR \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_RDWR;
                expect_read1 = 1;
                expect_write1 = 1;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        /* turn data caching off */
        printf("Parent disabling UBC caching \n");
        if (fcntl(fd1, F_NOCACHE, 1) != 0) {
            XCTFail( "F_NOCACHE failed %d:%s \n",
                    errno, strerror(errno));
            goto done;
        }

        if (expect_read1) {
            printf("Parent verifying read access \n");
           error = read_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }
        
        if (expect_write1) {
            printf("Parent verifying write access \n");
            error = write_and_verify(fd1, data1, sizeof(data1),
                                     expect_read1 == 1 ? 0 : 1);
            if (error) {
                XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        /*
         * Child Testing code starts here
         * Do not call XCTFail, just printf
         * Return test result in *child_errorp
         */
        for (i2 = 0; i2 < 9; i2++) {
            /*
             * With the child process, try opening the same file with
             * all the various read/write permissions.
             */
            
            switch(i2) {
                case 0:
                    printf("     Child opening file with O_RDONLY \n");
                    oflag2 = O_NONBLOCK | O_RDONLY;
                    //expect_read2 = 1;
                    //expect_write2 = 0;
                    expect_fail = 1;
                    break;
                case 1:
                    printf("     Child opening file with O_WRONLY \n");
                    oflag2 = O_NONBLOCK | O_WRONLY;
                    //expect_read2 = 0;
                    //expect_write2 = 1;
                    expect_fail = 1;
                    break;
                case 2:
                    printf("     Child opening file with O_RDWR \n");
                    oflag2 = O_NONBLOCK | O_RDWR;
                    //expect_read2 = 1;
                    //expect_write2 = 1;
                    expect_fail = 1;
                    break;
                case 3:
                    printf("     Child opening file with O_SHLOCK | O_RDONLY \n");
                    oflag2 = O_SHLOCK | O_NONBLOCK | O_RDONLY;
                    //expect_read2 = 1;
                    //expect_write2 = 0;
                    expect_fail = 1;
                    break;
                case 4:
                    printf("     Child opening file with O_SHLOCK | O_WRONLY \n");
                    oflag2 = O_SHLOCK | O_NONBLOCK | O_WRONLY;
                    //expect_read2 = 0;
                    //expect_write2 = 1;
                    expect_fail = 1;
                    break;
                case 5:
                    printf("     Child opening file with O_SHLOCK | O_RDWR \n");
                    oflag2 = O_SHLOCK | O_NONBLOCK | O_RDWR;
                    //expect_read2 = 1;
                    //expect_write2 = 1;
                    expect_fail = 1;
                    break;
                case 6:
                    printf("     Child opening file with O_EXLOCK | O_RDONLY \n");
                    oflag2 = O_EXLOCK | O_NONBLOCK | O_RDONLY;
                    //expect_read2 = 1;
                    //expect_write2 = 0;
                    expect_fail = 1;
                    break;
                case 7:
                    printf("     Child opening file with O_EXLOCK | O_WRONLY \n");
                    oflag2 = O_EXLOCK | O_NONBLOCK | O_WRONLY;
                    //expect_read2 = 0;
                    //expect_write2 = 1;
                    expect_fail = 1;
                    break;
                case 8:
                    printf("     Child opening file with O_EXLOCK | O_RDWR \n");
                    oflag2 = O_EXLOCK | O_NONBLOCK | O_RDWR;
                    //expect_read2 = 1;
                    //expect_write2 = 1;
                    expect_fail = 1;
                    break;
                default:
                    XCTFail("     Unknown selector for i2 %d \n", i2);
                    goto done;
            }
            
            /*
             * Open the testfile in child process
             */
            fd2 = open(file_path, oflag2);
            if (fd2 == -1) {
                if (expect_fail) {
                    /* Expected Failure */
                }
                else {
                    /* Unexpected failure */
                    XCTFail("     Unexpected failure - open on <%s> failed %d:%s \n", file_path,
                            errno, strerror(errno));
                    goto done;
                }
            }
            else {
                if (expect_fail) {
                    /* Unexpected success */
                    XCTFail("     Unexpected success - open on <%s> worked \n", file_path);
                    goto done;
                }
                else {
                    /* Expected success */
                }
            }

            /* Close file if needed */
            if (fd2 != -1) {
                printf("     Child closing file \n");
                error = close(fd2);
                if (error) {
                    XCTFail("     close on fd2 failed %d:%s \n", errno, strerror(errno));
                    goto done;
                }
                else {
                    //fd2 = -1;
                }
            }
        } /* i2 loop */
    
        printf("Child test passed \n");
            
        if (expect_read1) {
            printf("Parent re-verifying read access \n");
           error = read_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }
        
        if (expect_write1) {
            printf("Parent re-verifying write access \n");
            error = write_and_verify(fd1, data1, sizeof(data1),
                                     expect_read1 == 1 ? 0 : 1);
            if (error) {
                XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        /* Close file */
        printf("Parent closing file \n");
        if (fd1 != -1) {
            error = close(fd1);
            if (error) {
                printf("close on fd2 failed %d:%s \n", errno, strerror(errno));
                //error = errno;
                goto done;
            }
            else {
                fd1 = -1;
            }
        }
    } /* i1 loop */
        
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testSingleProcessO_SHLOCK
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int oflag2 = 0;
    int fd1 = -1;
    int fd2 = -1;
    int i1 = 0, i2 = 0;
    int expect_fail = 0;
    int expect_read1 = 0, expect_read2 = 0;
    int expect_write1 = 0, expect_write2 = 0;
    
    /*
     * Test for correct O_SHLOCK behavior all in a single process. For
     * SMB, a file opened with O_SHLOCK will deny other open attempts
     * that request write access.
     *
     * 1. Open file with O_SHLOCK and all 3 variants of O_RDONLY/O_WRONLY/O_RDWR
     * 2. Disable data caching (UBC)
     * 3. Verify read/write
     * 4. With same process, try opening the same file with all 9 variants
     *    of none/O_SHLOCK/O_EXLOCK and O_RDONLY, O_WRONLY, O_RDWR
     * 5. If process is allowed to open the file, then verify read/write and
     *    close the file
     * 6. After each open attempt, verify read/write again
     *
     * Total variants tested: 27
     */
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test for correct O_SHLOCK behavior all in a single process",
                               "open,close,read,write,O_SHLOCK",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testSingleProcessO_SHLOCK");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    for (i1 = 0; i1 < 3; i1++) {
        switch(i1) {
            case 0:
                printf("Parent opening file with O_SHLOCK | O_RDONLY \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDONLY;
                expect_read1 = 1;
                expect_write1 = 0;
                break;
            case 1:
                printf("Parent opening file with O_SHLOCK | O_WRONLY \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_WRONLY;
                expect_read1 = 0;
                expect_write1 = 1;
                break;
            case 2:
                printf("Parent opening file with O_SHLOCK | O_RDWR \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDWR;
                expect_read1 = 1;
                expect_write1 = 1;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        /* turn data caching off */
        printf("Parent disabling UBC caching \n");
        if (fcntl(fd1, F_NOCACHE, 1) != 0) {
            XCTFail( "F_NOCACHE failed %d:%s \n",
                    errno, strerror(errno));
            goto done;
        }

        if (expect_read1) {
            printf("Parent verifying read access \n");
           error = read_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }
        
        if (expect_write1) {
            printf("Parent verifying write access \n");
            error = write_and_verify(fd1, data1, sizeof(data1),
                                     expect_read1 == 1 ? 0 : 1);
            if (error) {
                XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        /*
         * Child Testing code starts here
         * Do not call XCTFail, just printf
         * Return test result in *child_errorp
         */
        for (i2 = 0; i2 < 9; i2++) {
            /*
             * With the child process, try opening the same file with
             * all the various read/write permissions.
             */
            
            switch(i2) {
                case 0:
                    printf("     Child opening file with O_RDONLY \n");
                    oflag2 = O_NONBLOCK | O_RDONLY;
                    expect_read2 = 1;
                    expect_write2 = 0;
                    expect_fail = 0;
                    break;
                case 1:
                    printf("     Child opening file with O_WRONLY \n");
                    oflag2 = O_NONBLOCK | O_WRONLY;
                    expect_read2 = 0;
                    expect_write2 = 1;
                    expect_fail = 1;
                    break;
                case 2:
                    printf("     Child opening file with O_RDWR \n");
                    oflag2 = O_NONBLOCK | O_RDWR;
                    expect_read2 = 1;
                    expect_write2 = 1;
                    expect_fail = 1;
                    break;
                case 3:
                    printf("     Child opening file with O_SHLOCK | O_RDONLY \n");
                    oflag2 = O_SHLOCK | O_NONBLOCK | O_RDONLY;
                    expect_read2 = 1;
                    expect_write2 = 0;
                    if (oflag1 & (O_WRONLY | O_RDWR)) {
                        /* First open has write access so O_SHLOCK will fail */
                        expect_fail = 1;
                    }
                    else {
                        expect_fail = 0;
                    }
                    break;
                case 4:
                    printf("     Child opening file with O_SHLOCK | O_WRONLY \n");
                    oflag2 = O_SHLOCK | O_NONBLOCK | O_WRONLY;
                    expect_read2 = 0;
                    expect_write2 = 1;
                    expect_fail = 1;
                    break;
                case 5:
                    printf("     Child opening file with O_SHLOCK | O_RDWR \n");
                    oflag2 = O_SHLOCK | O_NONBLOCK | O_RDWR;
                    expect_read2 = 1;
                    expect_write2 = 1;
                    expect_fail = 1;
                    break;
                case 6:
                    printf("     Child opening file with O_EXLOCK | O_RDONLY \n");
                    oflag2 = O_EXLOCK | O_NONBLOCK | O_RDONLY;
                    expect_read2 = 1;
                    expect_write2 = 0;
                    expect_fail = 1;
                    break;
                case 7:
                    printf("     Child opening file with O_EXLOCK | O_WRONLY \n");
                    oflag2 = O_EXLOCK | O_NONBLOCK | O_WRONLY;
                    expect_read2 = 0;
                    expect_write2 = 1;
                    expect_fail = 1;
                    break;
                case 8:
                    printf("     Child opening file with O_EXLOCK | O_RDWR \n");
                    oflag2 = O_EXLOCK | O_NONBLOCK | O_RDWR;
                    expect_read2 = 1;
                    expect_write2 = 1;
                    expect_fail = 1;
                    break;
                default:
                    XCTFail("     Unknown selector for i2 %d \n", i2);
                    goto done;
            }
            
            /*
             * Open the testfile in child process
             */
            fd2 = open(file_path, oflag2);
            if (fd2 == -1) {
                if (expect_fail) {
                    /* Expected Failure */
                }
                else {
                    /* Unexpected failure */
                    XCTFail("     Unexpected failure - open on <%s> failed %d:%s \n", file_path,
                            errno, strerror(errno));
                    goto done;
                }
            }
            else {
                if (expect_fail) {
                    /* Unexpected success */
                    XCTFail("     Unexpected success - open on <%s> worked \n", file_path);
                    goto done;
                }
                else {
                    /* Expected success */
                }
            }

            if (expect_fail == 0) {
                /* turn data caching off */
                printf("     Child disabling UBC caching \n");
                if (fcntl(fd2, F_NOCACHE, 1) != 0) {
                    XCTFail("     Child F_NOCACHE failed %d:%s \n",
                            errno, strerror(errno));
                    goto done;
                }

                if (expect_read2) {
                    printf("     Child verifying read access \n");
                   error = read_and_verify(fd2, data1, sizeof(data1), 0);
                    if (error) {
                        XCTFail("     read_and_verify on <%s> failed %d:%s \n", file_path,
                                error, strerror(error));
                        goto done;
                    }
                }
                
                if (expect_write2) {
                    printf("     Child verifying write access \n");
                    error = write_and_verify(fd2, data1, sizeof(data1),
                                             expect_read1 == 1 ? 0 : 1);
                    if (error) {
                        XCTFail("     write_and_verify on <%s> failed %d:%s \n", file_path,
                                error, strerror(error));
                        goto done;
                    }
                }
            }
            
            /* Close file if needed */
            if (fd2 != -1) {
                printf("     Child closing file \n");
                error = close(fd2);
                if (error) {
                    XCTFail("     close on fd2 failed %d:%s \n", errno, strerror(errno));
                    goto done;
                }
                else {
                    //fd2 = -1;
                }
            }
        } /* i2 loop */
    
        printf("Child test passed \n");
        
        if (expect_read1) {
            printf("Parent re-verifying read access \n");
           error = read_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }
        
        if (expect_write1) {
            printf("Parent re-verifying write access \n");
            error = write_and_verify(fd1, data1, sizeof(data1),
                                     expect_read1 == 1 ? 0 : 1);
            if (error) {
                XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        /* Close file */
        printf("Parent closing file \n");
        if (fd1 != -1) {
            error = close(fd1);
            if (error) {
                printf("close on fd2 failed %d:%s \n", errno, strerror(errno));
                //error = errno;
                goto done;
            }
            else {
                fd1 = -1;
            }
        }
    } /* i1 loop */
        
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testTransferFD
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int fd1 = -1;
    int i1 = 0;
    int expect_read1 = 0;
    int expect_write1 = 0;
    pid_t child_pid = 0;
    int child_error = 0;
    
    /*
     * Test that a SMB fd can be transferred to another process and still
     * work.
     *
     * 1. Open the file with all 9 variants of none/O_SHLOCK/O_EXLOCK and
     *    O_RDONLY, O_WRONLY, O_RDWR
     * 2. Disable data caching (UBC)
     * 3. Verify read/write
     * 4. With a child process, verify read/write on fd that was inherited
     * 5. Parent verifies read/write again
     *
     * Total variants tested: 9
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that a SMB fd can be transferred to another process and still work",
                               "open,close,read,write,O_EXLOCK,O_SHLOCK",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testTransferFD");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    for (i1 = 0; i1 < 9; i1++) {
        switch(i1) {
            case 0:
                printf("Parent opening file with O_RDONLY \n");
                oflag1 = O_NONBLOCK | O_RDONLY;
                expect_read1 = 1;
                expect_write1 = 0;
                break;
            case 1:
                printf("Parent opening file with O_WRONLY \n");
                oflag1 = O_NONBLOCK | O_WRONLY;
                expect_read1 = 0;
                expect_write1 = 1;
                break;
            case 2:
                printf("Parent opening file with O_RDWR \n");
                oflag1 = O_NONBLOCK | O_RDWR;
                expect_read1 = 1;
                expect_write1 = 1;
                break;
            case 3:
                printf("Parent opening file with O_SHLOCK | O_RDONLY \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDONLY;
                expect_read1 = 1;
                expect_write1 = 0;
                break;
            case 4:
                printf("Parent opening file with O_SHLOCK | O_WRONLY \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_WRONLY;
                expect_read1 = 0;
                expect_write1 = 1;
                break;
            case 5:
                printf("Parent opening file with O_SHLOCK | O_RDWR \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDWR;
                expect_read1 = 1;
                expect_write1 = 1;
                break;
            case 6:
                printf("Parent opening file with O_EXLOCK | O_RDONLY \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_RDONLY;
                expect_read1 = 1;
                expect_write1 = 0;
                break;
            case 7:
                printf("Parent opening file with O_EXLOCK | O_WRONLY \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_WRONLY;
                expect_read1 = 0;
                expect_write1 = 1;
                break;
            case 8:
                printf("Parent opening file with O_EXLOCK | O_RDWR \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_RDWR;
                expect_read1 = 1;
                expect_write1 = 1;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        /* turn data caching off */
        printf("Parent disabling UBC caching \n");
        if (fcntl(fd1, F_NOCACHE, 1) != 0) {
            XCTFail( "F_NOCACHE failed %d:%s \n",
                    errno, strerror(errno));
            goto done;
        }

        if (expect_read1) {
            printf("Parent verifying read access \n");
           error = read_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }
        
        if (expect_write1) {
            printf("Parent verifying write access \n");
            error = write_and_verify(fd1, data1, sizeof(data1),
                                     expect_read1 == 1 ? 0 : 1);
            if (error) {
                XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        /* Kick off the child process now */
        child_pid = fork();
        if (child_pid != 0) {
            /*
             * Parent Testing code continues here
             */
            child_error = waitForChild(child_pid);
            if (child_error != 0) {
                XCTFail("Parent - Child test failed %d:%s \n",
                        child_error, strerror(child_error));
                goto done;
            }
            else {
                printf("Child test passed \n");
            }
            
            if (expect_read1) {
                printf("Parent re-verifying read access \n");
               error = read_and_verify(fd1, data1, sizeof(data1), 0);
                if (error) {
                    XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }
            
            if (expect_write1) {
                printf("Parent re-verifying write access \n");
                error = write_and_verify(fd1, data1, sizeof(data1),
                                         expect_read1 == 1 ? 0 : 1);
                if (error) {
                    XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }

            /* Close file */
            printf("Parent closing file \n");
            if (fd1 != -1) {
                error = close(fd1);
                if (error) {
                    printf("close on fd2 failed %d:%s \n", errno, strerror(errno));
                    //error = errno;
                    goto done;
                }
                else {
                    fd1 = -1;
                }
            }
        }
        else {
            /*
             * Child Testing code starts here
             * Do not call XCTFail, just printf
             */
        
            /*
             * Test that another process can read/write on fd1 that came
             * from parent process
             */
            if (expect_read1) {
                printf("     Child verifying read access \n");
                error = read_and_verify(fd1, data1, sizeof(data1), 0);
                if (error) {
                    printf("     read_and_verify on <%s> failed %d:%s \n", file_path,
                           error, strerror(error));
                    exit(error);
                }
            }
            
            if (expect_write1) {
                printf("     Child verifying write access \n");
                error = write_and_verify(fd1, data1, sizeof(data1),
                                         expect_read1 == 1 ? 0 : 1);
                if (error) {
                    printf("     write_and_verify on <%s> failed %d:%s \n", file_path,
                           error, strerror(error));
                    exit(error);
                }
            }

            /*
             * Parents always have to clean up after this children,
             * so child just exits and leaves cleanup to parent process.
             */
            exit(0);
        } /* Child test code */
    } /* i1 loop */
        
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testTransferFlockFD
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int fd1 = -1;
    int i1 = 0;
    int expect_read1 = 0;
    int expect_write1 = 0;
    pid_t child_pid = 0;
    int child_error = 0;
    
    /*
     * Test that a SMB fd can be flock()'d and transferred to another process
     * and still work.
     *
     * 1. Open the file with all 3 variants of O_RDONLY, O_WRONLY, O_RDWR
     *    O_EXLOCK/O_SHLOCK not allowed to do a flock() so skip them.
     * 2. Call flock(LOCK_EX) on the open file
     * 3. Disable data caching (UBC)
     * 4. Verify read/write
     * 5. With a child process, verify read/write on fd that was inherited
     * 6. Parent verifies read/write again.
     *
     * Total variants tested: 3
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that a SMB fd can be flock()'d and transferred to another process and still work.",
                               "open,close,read,write,O_EXLOCK,O_SHLOCK",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testTransferFlockFD");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    for (i1 = 0; i1 < 3; i1++) {
        switch(i1) {
            case 0:
                printf("Parent opening file with O_RDONLY \n");
                oflag1 = O_NONBLOCK | O_RDONLY;
                expect_read1 = 1;
                expect_write1 = 0;
                break;
            case 1:
                printf("Parent opening file with O_WRONLY \n");
                oflag1 = O_NONBLOCK | O_WRONLY;
                expect_read1 = 0;
                expect_write1 = 1;
                break;
            case 2:
                printf("Parent opening file with O_RDWR \n");
                oflag1 = O_NONBLOCK | O_RDWR;
                expect_read1 = 1;
                expect_write1 = 1;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        printf("Parent flock on file \n");
        error = flock(fd1, LOCK_EX);
        if (error) {
            XCTFail("flock on <%s> failed %d:%s \n", file_path,
                    error, strerror(error));
            goto done;
        }

        /* turn data caching off */
        printf("Parent disabling UBC caching \n");
        if (fcntl(fd1, F_NOCACHE, 1) != 0) {
            XCTFail( "F_NOCACHE failed %d:%s \n",
                    errno, strerror(errno));
            goto done;
        }

        if (expect_read1) {
            printf("Parent verifying read access \n");
            error = read_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }
        
        if (expect_write1) {
            printf("Parent verifying write access \n");
            error = write_and_verify(fd1, data1, sizeof(data1),
                                     expect_read1 == 1 ? 0 : 1);
            if (error) {
                XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        /* Kick off the child process now */
        child_pid = fork();
        if (child_pid != 0) {
            /*
             * Parent Testing code continues here
             */
            child_error = waitForChild(child_pid);
            if (child_error != 0) {
                XCTFail("Parent - Child test failed %d:%s \n",
                        child_error, strerror(child_error));
                goto done;
            }
            else {
                printf("Child test passed \n");
            }
            
            if (expect_read1) {
                printf("Parent re-verifying read access \n");
               error = read_and_verify(fd1, data1, sizeof(data1), 0);
                if (error) {
                    XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }
            
            if (expect_write1) {
                printf("Parent re-verifying write access \n");
                error = write_and_verify(fd1, data1, sizeof(data1),
                                         expect_read1 == 1 ? 0 : 1);
                if (error) {
                    XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }

            printf("Parent removing flock \n");
            error = flock(fd1, LOCK_UN);
            if (error) {
                XCTFail("flock on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }

            /* Close file */
            printf("Parent closing file \n");
            if (fd1 != -1) {
                error = close(fd1);
                if (error) {
                    printf("close on fd2 failed %d:%s \n", errno, strerror(errno));
                    //error = errno;
                    goto done;
                }
                else {
                    fd1 = -1;
                }
            }
        }
        else {
            /*
             * Child Testing code starts here
             * Do not call XCTFail, just printf
             */
        
            /*
             * Test that another process can read/write on fd1 that came
             * from parent process
             */
            if (expect_read1) {
                printf("     Child verifying read access \n");
                error = read_and_verify(fd1, data1, sizeof(data1), 0);
                if (error) {
                    printf("     read_and_verify on <%s> failed %d:%s \n", file_path,
                           error, strerror(error));
                    exit(error);
                }
            }
            
            if (expect_write1) {
                printf("     Child verifying write access \n");
                error = write_and_verify(fd1, data1, sizeof(data1),
                                         expect_read1 == 1 ? 0 : 1);
                if (error) {
                    printf("     write_and_verify on <%s> failed %d:%s \n", file_path,
                           error, strerror(error));
                    exit(error);
                }
            }

            /*
             * Parents always have to clean up after this children,
             * so child just exits and leaves cleanup to parent process.
             */
            exit(0);
        } /* Child test code */
    } /* i1 loop */
        
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testDowngradeO_EXLOCK
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int oflag2 = 0;
    int fd1 = -1;
    int fd2 = -1;
    int i1 = 0, i2 = 0;
    int expect_fail = 0;
    pid_t child_pid = 0;
    int child_error = 0;
    
    /*
     * Test for that after a file is closed with O_EXLOCK, other open attempts
     * will work correctly.
     *
     * 1. Open file with O_EXLOCK and all 3 variants of O_RDONLY/O_WRONLY/O_RDWR
     * 2. With a child process, select one of the 9 variants
     *    of none/O_SHLOCK/O_EXLOCK and O_RDONLY, O_WRONLY, O_RDWR
     * 3. Child verifies the open attempt fails
     * 4. Parent now closes the file that has O_EXLOCK
     * 5. Child verifies the open attempt now works.
     *
     * Total variants tested: 27
     */

    /*
     * Note: no IO verification is done in this test, assumed other unit
     * tests cover that
     */
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test for that after a file is closed with O_EXLOCK, other open attempts will work correctly",
                               "open,close,O_EXLOCK",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testDowngradeO_EXLOCK");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    for (i1 = 0; i1 < 3; i1++) {
        switch(i1) {
            case 0:
                printf("Parent opening file with O_EXLOCK | O_RDONLY \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_RDONLY;
                break;
            case 1:
                printf("Parent opening file with O_EXLOCK | O_WRONLY \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_WRONLY;
                break;
            case 2:
                printf("Parent opening file with O_EXLOCK | O_RDWR \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_RDWR;
                break;
           default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        /*
         * Set up the child open mode to try
         */
        for (i2 = 0; i2 < 9; i2++) {
            /*
             * With the child process, try opening the same file with
             * all the various read/write permissions.
             */
            switch(i2) {
                case 0:
                    printf("     Child opening file with O_RDONLY \n");
                    oflag2 = O_NONBLOCK | O_RDONLY;
                    expect_fail = 1;
                    break;
                case 1:
                    printf("     Child opening file with O_WRONLY \n");
                    oflag2 = O_NONBLOCK | O_WRONLY;
                    expect_fail = 1;
                    break;
                case 2:
                    printf("     Child opening file with O_RDWR \n");
                    oflag2 = O_NONBLOCK | O_RDWR;
                    expect_fail = 1;
                    break;
                case 3:
                    printf("     Child opening file with O_SHLOCK | O_RDONLY \n");
                    oflag2 = O_SHLOCK | O_NONBLOCK | O_RDONLY;
                    expect_fail = 1;
                    break;
                case 4:
                    printf("     Child opening file with O_SHLOCK | O_WRONLY \n");
                    oflag2 = O_SHLOCK | O_NONBLOCK | O_WRONLY;
                    expect_fail = 1;
                    break;
                case 5:
                    printf("     Child opening file with O_SHLOCK | O_RDWR \n");
                    oflag2 = O_SHLOCK | O_NONBLOCK | O_RDWR;
                    expect_fail = 1;
                    break;
                case 6:
                    printf("     Child opening file with O_EXLOCK | O_RDONLY \n");
                    oflag2 = O_EXLOCK | O_NONBLOCK | O_RDONLY;
                    expect_fail = 1;
                    break;
                case 7:
                    printf("     Child opening file with O_EXLOCK | O_WRONLY \n");
                    oflag2 = O_EXLOCK | O_NONBLOCK | O_WRONLY;
                    expect_fail = 1;
                    break;
                case 8:
                    printf("     Child opening file with O_EXLOCK | O_RDWR \n");
                    oflag2 = O_EXLOCK | O_NONBLOCK | O_RDWR;
                    expect_fail = 1;
                    break;
                default:
                    printf("     Unknown selector for i2 %d \n", i2);
                    exit(EINVAL);
            }

            /*
             * Parent has the file open with O_EXLOCK, first verify that the
             * child process fails on attempting to open the same file.
             *
             * Kick off the child process now
             */
            child_pid = fork();
            if (child_pid != 0) {
                /*
                 * Parent Testing code continues here
                 */
                child_error = waitForChild(child_pid);
                if (child_error != 0) {
                    XCTFail("Parent - Child test 1 failed %d:%s \n",
                            child_error, strerror(child_error));
                    goto done;
                }
                else {
                    printf("Child test 1 (open expected to fail) passed \n");
                }
            }
            else {
                /*
                 * Child Testing code starts here
                 * Do not call XCTFail, just printf
                 */
                    
                /*
                 * Open the testfile in child process
                 */
                fd2 = open(file_path, oflag2);
                if (fd2 == -1) {
                    if (expect_fail) {
                        /* Expected Failure */
                    }
                    else {
                        /* Unexpected failure */
                        printf("     Unexpected failure - open on <%s> failed %d:%s \n", file_path,
                                errno, strerror(errno));
                        exit(errno);
                    }
                }
                else {
                    if (expect_fail) {
                        /* Unexpected success */
                        printf("     Unexpected success - open on <%s> worked \n", file_path);
                        exit(EINVAL);
                    }
                    else {
                        /* Expected success */
                    }
                }

                /* Close file if needed */
                if (fd2 != -1) {
                    printf("     Child closing file \n");
                    error = close(fd2);
                    if (error) {
                        printf("     close on fd2 failed %d:%s \n", errno, strerror(errno));
                        exit(errno);
                    }
                    else {
                        //fd2 = -1;
                    }
                }
        
                /*
                 * Parents always have to clean up after this children,
                 * so child just exits and leaves cleanup to parent process.
                 */
                exit(0);
            } /* Child test code */
            
            
            /*
             * Resuming parent code here
             */
            
            /*
             * Parent now closes the file so now verify that the
             * child process succeeds on attempting to open the same file.
             *
             * Kick off the child process now
             */
            
            expect_fail = 0;

            /* Close file */
            printf("Parent closing file \n");
            if (fd1 != -1) {
                error = close(fd1);
                if (error) {
                    printf("close on fd1 failed %d:%s \n", errno, strerror(errno));
                    //error = errno;
                    goto done;
                }
                else {
                    //fd1 = -1;
                }
            }

            /* Kick off the child process now for second attempt at open */
            child_pid = fork();
            if (child_pid != 0) {
                /*
                 * Parent Testing code continues here
                 */
                child_error = waitForChild(child_pid);
                if (child_error != 0) {
                    XCTFail("Parent - Child test 2 failed %d:%s \n",
                            child_error, strerror(child_error));
                    goto done;
                }
                else {
                    printf("Child test 2 (open expected to pass) passed \n");
                }
            }
            else {
                /*
                 * Child Testing code starts here
                 * Do not call XCTFail, just printf
                 */
                    
                /*
                 * Open the testfile in child process
                 */
                fd2 = open(file_path, oflag2);
                if (fd2 == -1) {
                    if (expect_fail) {
                        /* Expected Failure */
                    }
                    else {
                        /* Unexpected failure */
                        printf("     Unexpected failure - open on <%s> failed %d:%s \n", file_path,
                                errno, strerror(errno));
                        exit(errno);
                    }
                }
                else {
                    if (expect_fail) {
                        /* Unexpected success */
                        printf("     Unexpected success - open on <%s> worked \n", file_path);
                        exit(EINVAL);
                    }
                    else {
                        /* Expected success */
                    }
                }

                /* Close file if needed */
                if (fd2 != -1) {
                    error = close(fd2);
                    if (error) {
                        printf("     close on fd2 failed %d:%s \n", errno, strerror(errno));
                        exit(errno);
                    }
                    else {
                        //fd2 = -1;
                    }
                }
        
                /*
                 * Parents always have to clean up after this children,
                 * so child just exits and leaves cleanup to parent process.
                 */
                exit(0);
            } /* Child test code */
            
            
            /*
             * Resuming parent code here again
             */

            /*
             * Need to re-open the testfile in parent process for the
             * next test loop
             */
            printf("Parent reopening file \n");

            fd1 = open(file_path, oflag1);
            if (fd1 == -1) {
                XCTFail("open on <%s> failed %d:%s \n", file_path,
                        errno, strerror(errno));
                goto done;
            }
        } /* i2 loop */
        
        /* Need to close the file before next test loop */
        /* Close file */
        printf("Parent closing file \n");
        if (fd1 != -1) {
            error = close(fd1);
            if (error) {
                printf("close on fd2 failed %d:%s \n", errno, strerror(errno));
                //error = errno;
                goto done;
            }
            else {
                fd1 = -1;
            }
        }
    } /* i1 loop */
        
    
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testDowngradeO_SHLOCK
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int oflag2 = 0;
    int fd1 = -1;
    int fd2 = -1;
    int i1 = 0, i2 = 0;
    int expect_fail = 0;
    pid_t child_pid = 0;
    int child_error = 0;
    
    /*
     * Test for that after a file is closed with O_SHLOCK, other open attempts
     * will work correctly.
     *
     * 1. Open file with O_SHLOCK and all 3 variants of O_RDONLY/O_WRONLY/O_RDWR
     * 2. With a child process, select one of the 9 variants
     *    of none/O_SHLOCK/O_EXLOCK and O_RDONLY, O_WRONLY, O_RDWR
     * 3. Child verifies the open attempt fails for those variants which are
     *    expected to fail.
     * 4. Parent now closes the file that has O_SHLOCK
     * 5. Child verifies the open attempt now works.
     *
     * Total variants tested: 27
     */

    /*
     * Note: no IO verification is done in this test, assumed other unit
     * tests cover that
     */
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test for that after a file is closed with O_SHLOCK, other open attempts will work correctly",
                               "open,close,O_SHLOCK",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testDowngradeO_EXLOCK");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    for (i1 = 0; i1 < 3; i1++) {
        switch(i1) {
            case 0:
                printf("Parent opening file with O_SHLOCK | O_RDONLY \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDONLY;
                break;
            case 1:
                printf("Parent opening file with O_SHLOCK | O_WRONLY \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_WRONLY;
                break;
            case 2:
                printf("Parent opening file with O_SHLOCK | O_RDWR \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDWR;
                break;
           default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        /*
         * Set up the child open mode to try
         */
        for (i2 = 0; i2 < 9; i2++) {
            /*
             * With the child process, try opening the same file with
             * all the various read/write permissions.
             */
            switch(i2) {
                case 0:
                    printf("     Child opening file with O_RDONLY - skipped \n");
                    //oflag2 = O_NONBLOCK | O_RDONLY;
                    //expect_fail = 0;
                    continue;
                case 1:
                    printf("     Child opening file with O_WRONLY \n");
                    oflag2 = O_NONBLOCK | O_WRONLY;
                    expect_fail = 1;
                    break;
                case 2:
                    printf("     Child opening file with O_RDWR \n");
                    oflag2 = O_NONBLOCK | O_RDWR;
                    expect_fail = 1;
                    break;
                case 3:
                    oflag2 = O_SHLOCK | O_NONBLOCK | O_RDONLY;
                    if (oflag1 & (O_WRONLY | O_RDWR)) {
                        /* First open has write access so O_SHLOCK will fail */
                        printf("     Child opening file with O_SHLOCK | O_RDONLY \n");
                        expect_fail = 1;
                    }
                    else {
                        printf("     Child opening file with O_SHLOCK | O_RDONLY - skipped \n");
                        //expect_fail = 0;
                        continue;
                   }
                    break;
                case 4:
                    printf("     Child opening file with O_SHLOCK | O_WRONLY \n");
                    oflag2 = O_SHLOCK | O_NONBLOCK | O_WRONLY;
                    expect_fail = 1;
                    break;
                case 5:
                    printf("     Child opening file with O_SHLOCK | O_RDWR \n");
                    oflag2 = O_SHLOCK | O_NONBLOCK | O_RDWR;
                    expect_fail = 1;
                    break;
                case 6:
                    printf("     Child opening file with O_EXLOCK | O_RDONLY \n");
                    oflag2 = O_EXLOCK | O_NONBLOCK | O_RDONLY;
                    expect_fail = 1;
                    break;
                case 7:
                    printf("     Child opening file with O_EXLOCK | O_WRONLY \n");
                    oflag2 = O_EXLOCK | O_NONBLOCK | O_WRONLY;
                    expect_fail = 1;
                    break;
                case 8:
                    printf("     Child opening file with O_EXLOCK | O_RDWR \n");
                    oflag2 = O_EXLOCK | O_NONBLOCK | O_RDWR;
                    expect_fail = 1;
                    break;
                default:
                    printf("     Unknown selector for i2 %d \n", i2);
                    exit(EINVAL);
            }

            /*
             * Parent has the file open with O_EXLOCK, first verify that the
             * child process fails on attempting to open the same file.
             *
             * Kick off the child process now
             */
            child_pid = fork();
            if (child_pid != 0) {
                /*
                 * Parent Testing code continues here
                 */
                child_error = waitForChild(child_pid);
                if (child_error != 0) {
                    XCTFail("Parent - Child test 1 failed %d:%s \n",
                            child_error, strerror(child_error));
                    goto done;
                }
                else {
                    printf("Child test 1 (open expected to fail) passed \n");
                }
            }
            else {
                /*
                 * Child Testing code starts here
                 * Do not call XCTFail, just printf
                 */
                    
                /*
                 * Open the testfile in child process
                 */
                fd2 = open(file_path, oflag2);
                if (fd2 == -1) {
                    if (expect_fail) {
                        /* Expected Failure */
                    }
                    else {
                        /* Unexpected failure */
                        printf("     Unexpected failure - open on <%s> failed %d:%s \n", file_path,
                                errno, strerror(errno));
                        exit(errno);
                    }
                }
                else {
                    if (expect_fail) {
                        /* Unexpected success */
                        printf("     Unexpected success - open on <%s> worked \n", file_path);
                        exit(EINVAL);
                    }
                    else {
                        /* Expected success */
                    }
                }

                /* Close file if needed */
                if (fd2 != -1) {
                    printf("     Child closing file \n");
                    error = close(fd2);
                    if (error) {
                        printf("     close on fd2 failed %d:%s \n", errno, strerror(errno));
                        exit(errno);
                    }
                    else {
                        //fd2 = -1;
                    }
                }
        
                /*
                 * Parents always have to clean up after this children,
                 * so child just exits and leaves cleanup to parent process.
                 */
                exit(0);
            } /* Child test code */
            
            
            /*
             * Resuming parent code here
             */
            
            /*
             * Parent now closes the file so now verify that the
             * child process succeeds on attempting to open the same file.
             *
             * Kick off the child process now
             */
            
            expect_fail = 0;

            /* Close file */
            printf("Parent closing file \n");
            if (fd1 != -1) {
                error = close(fd1);
                if (error) {
                    printf("close on fd2 failed %d:%s \n", errno, strerror(errno));
                    //error = errno;
                    goto done;
                }
                else {
                    //fd1 = -1;
                }
            }

            /* Kick off the child process now for second attempt at open */
            child_pid = fork();
            if (child_pid != 0) {
                /*
                 * Parent Testing code continues here
                 */
                child_error = waitForChild(child_pid);
                if (child_error != 0) {
                    XCTFail("Parent - Child test 2 failed %d:%s \n",
                            child_error, strerror(child_error));
                    goto done;
                }
                else {
                    printf("Child test 2 (open expected to pass) passed \n");
                }
            }
            else {
                /*
                 * Child Testing code starts here
                 * Do not call XCTFail, just printf
                 */
                    
                /*
                 * Open the testfile in child process
                 */
                fd2 = open(file_path, oflag2);
                if (fd2 == -1) {
                    if (expect_fail) {
                        /* Expected Failure */
                    }
                    else {
                        /* Unexpected failure */
                        printf("     Unexpected failure - open on <%s> failed %d:%s \n", file_path,
                                errno, strerror(errno));
                        exit(errno);
                    }
                }
                else {
                    if (expect_fail) {
                        /* Unexpected success */
                        printf("     Unexpected success - open on <%s> worked \n", file_path);
                        exit(EINVAL);
                    }
                    else {
                        /* Expected success */
                    }
                }

                /* Close file if needed */
                if (fd2 != -1) {
                    error = close(fd2);
                    if (error) {
                        printf("     close on fd2 failed %d:%s \n", errno, strerror(errno));
                        exit(errno);
                    }
                    else {
                        //fd2 = -1;
                    }
                }
        
                /*
                 * Parents always have to clean up after this children,
                 * so child just exits and leaves cleanup to parent process.
                 */
                exit(0);
            } /* Child test code */
            
            
            /*
             * Resuming parent code here again
             */

            /*
             * Need to re-open the testfile in parent process for the
             * next test loop
             */
            printf("Parent reopening file \n");

            fd1 = open(file_path, oflag1);
            if (fd1 == -1) {
                XCTFail("open on <%s> failed %d:%s \n", file_path,
                        errno, strerror(errno));
                goto done;
            }
        } /* i2 loop */
        
        /* Need to close the file before next test loop */
        /* Close file */
        printf("Parent closing file \n");
        if (fd1 != -1) {
            error = close(fd1);
            if (error) {
                printf("close on fd2 failed %d:%s \n", errno, strerror(errno));
                //error = errno;
                goto done;
            }
            else {
                fd1 = -1;
            }
        }
    } /* i1 loop */
        
    
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testDowngradeSharedFID
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int oflag2 = 0;
    int fd1 = -1;
    int fd2 = -1;
    int expect_fail = 0;
    int expect_read1 = 0, expect_read2 = 0;
    int expect_write1 = 0, expect_write2 = 0;
    pid_t child_pid = 0;
    int child_error = 0;
    
    /*
     * Test opening a file for readOnly, then opening again for read/write,
     * then closing the read/write fid will allow another process to then
     * open the file for read/denyWrite. This will verify that the sharedFID
     * got sucessfully upgraded to read/write and then downgraded to read/write.
     *
     * 1. Open file with readOnly and verify read on fd1
     * 2. Open file again with read/write and verify read/write on fd2
     * 3. Close fd2
     * 4. With a child process, try opening the same file with read/write/denyWrite
     * 5. Verify read/write and close the file
     * 6. After each child open attempt, parent verifies read/write again
     *
     * Total variants tested: 1
     */
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test opening a file for readOnly, then opening again for read/write, then closing the read/write fid will allow another process to then open the file for read/denyWrite. This will verify that the sharedFID got sucessfully upgraded to read/write and then downgraded to read/write",
                               "open,close,read,write,O_SHLOCK",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testDowngradeSharedFID");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    printf("Parent opening file with O_RDONLY \n");
    oflag1 = O_NONBLOCK | O_RDONLY;
    expect_read1 = 1;
    //expect_write1 = 0;

    /*
     * Open the testfile in parent process with readOnly
     */
    fd1 = open(file_path, oflag1);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }

    /* turn data caching off */
    printf("Parent disabling UBC caching \n");
    if (fcntl(fd1, F_NOCACHE, 1) != 0) {
        XCTFail( "F_NOCACHE failed %d:%s \n",
                errno, strerror(errno));
        goto done;
    }

    if (expect_read1) {
        printf("Parent verifying read access \n");
       error = read_and_verify(fd1, data1, sizeof(data1), 0);
        if (error) {
            XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                    error, strerror(error));
            goto done;
        }
    }

    
    printf("Parent opening file with O_RDWR \n");
    oflag1 = O_NONBLOCK | O_RDWR;
    expect_read1 = 1;
    expect_write1 = 1;

    /*
     * Open the testfile in parent process but this time with read/write
     */
    fd2 = open(file_path, oflag1);
    if (fd2 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }

    /* turn data caching off */
    printf("Parent disabling UBC caching \n");
    if (fcntl(fd2, F_NOCACHE, 1) != 0) {
        XCTFail( "F_NOCACHE failed %d:%s \n",
                errno, strerror(errno));
        goto done;
    }

    if (expect_read1) {
        printf("Parent verifying read access \n");
       error = read_and_verify(fd2, data1, sizeof(data1), 0);
        if (error) {
            XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                    error, strerror(error));
            goto done;
        }
    }

    if (expect_write1) {
        printf("Parent verifying write access \n");
        error = write_and_verify(fd2, data1, sizeof(data1),
                                 expect_read1 == 1 ? 0 : 1);
        if (error) {
            XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                    error, strerror(error));
            goto done;
        }
    }

    /* Close file that was opened with read/write */
    printf("Parent closing file that has read/write \n");
    if (fd2 != -1) {
        error = close(fd2);
        if (error) {
            printf("     close on fd2 failed %d:%s \n", errno, strerror(errno));
            exit(errno);
        }
        else {
            //fd2 = -1;
        }
    }

    /* Reset parent flags for remaining readOnly file that is open */
    //oflag1 = O_NONBLOCK | O_RDONLY;
    expect_read1 = 1;
    expect_write1 = 0;


    /* Kick off the child process now */
    child_pid = fork();
    if (child_pid != 0) {
        /*
         * Parent Testing code continues here
         */
        child_error = waitForChild(child_pid);
        if (child_error != 0) {
            XCTFail("Parent - Child test failed %d:%s \n",
                    child_error, strerror(child_error));
            goto done;
        }
        else {
            printf("Child test passed \n");
        }
        
        if (expect_read1) {
            printf("Parent re-verifying read access \n");
           error = read_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }
        
        if (expect_write1) {
            printf("Parent re-verifying write access \n");
            error = write_and_verify(fd1, data1, sizeof(data1),
                                     expect_read1 == 1 ? 0 : 1);
            if (error) {
                XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        /* Close file */
        printf("Parent closing file \n");
        if (fd1 != -1) {
            error = close(fd1);
            if (error) {
                printf("close on fd2 failed %d:%s \n", errno, strerror(errno));
                //error = errno;
                goto done;
            }
            else {
                fd1 = -1;
            }
        }
    }
    else {
        /*
         * Child Testing code starts here
         * Do not call XCTFail, just printf
         */
        
        /* With child process, try opening file with read/write/denyWrite */
        printf("     Child opening file with O_SHLOCK | O_RDWR \n");
        oflag2 = O_SHLOCK | O_NONBLOCK | O_RDWR;
        expect_read2 = 1;
        expect_write2 = 1;
        expect_fail = 0;

        /*
         * Open the testfile in child process
         */
        fd2 = open(file_path, oflag2);
        if (fd2 == -1) {
            if (expect_fail) {
                /* Expected Failure */
            }
            else {
                /* Unexpected failure */
                printf("     Unexpected failure - open on <%s> failed %d:%s \n", file_path,
                        errno, strerror(errno));
                exit(errno);
            }
        }
        else {
            if (expect_fail) {
                /* Unexpected success */
                printf("     Unexpected success - open on <%s> worked \n", file_path);
                exit(EINVAL);
            }
            else {
                /* Expected success */
            }
        }

        /* turn data caching off */
        printf("     Child disabling UBC caching \n");
        if (fcntl(fd2, F_NOCACHE, 1) != 0) {
            printf( "     Child F_NOCACHE failed %d:%s \n",
                   errno, strerror(errno));
            exit(errno);
        }

        if (expect_read2) {
            printf("     Child verifying read access \n");
            error = read_and_verify(fd2, data1, sizeof(data1), 0);
            if (error) {
                printf("     read_and_verify on <%s> failed %d:%s \n", file_path,
                       error, strerror(error));
                exit(error);
            }
        }
        
        if (expect_write2) {
            printf("     Child verifying write access \n");
            error = write_and_verify(fd2, data1, sizeof(data1),
                                     expect_read1 == 1 ? 0 : 1);
            if (error) {
                printf("     write_and_verify on <%s> failed %d:%s \n", file_path,
                       error, strerror(error));
                exit(error);
            }
        }

        /* Close file if needed */
        if (fd2 != -1) {
            printf("     Child closing file \n");
            error = close(fd2);
            if (error) {
                printf("     close on fd2 failed %d:%s \n", errno, strerror(errno));
                exit(errno);
            }
            else {
                //fd2 = -1;
            }
        }
    
        /*
         * Parents always have to clean up after this children,
         * so child just exits and leaves cleanup to parent process.
         */
        exit(0);
    } /* Child test code */

    
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testDefCloseO_EXLOCK
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int fd1 = -1;
    int i1 = 0;
    struct smbStatPB pb = {0};

    /*
     * Test for correct deferred close behavior with file opened with O_EXLOCK
     *
     * 1. Open file with O_EXLOCK and all 3 variants of O_RDONLY/O_WRONLY/O_RDWR
     * 2. Verify got durable handle and lease
     * 3. Close the file
     * 4. Verify got deferred close
     * 5. Reopen file and verify deferred close got reused
     *
     * Total variants tested: 3
     *
     * Note that open(O_WRONLY) will end up with Read access added because the
     * SMBClient always tries to add Read so that UBC can be used. This will
     * result in open(O_RDWR) to end up with a defer use count of 3 instead
     * of 1.
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test for correct deferred close behavior with file opened with O_EXLOCK",
                               "open,close,O_EXLOCK,deferred_close",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testDefCloseO_EXLOCK");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    for (i1 = 0; i1 < 3; i1++) {
        switch(i1) {
            case 0:
                printf("Parent opening file with O_EXLOCK | O_RDONLY \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_RDONLY;
                break;
            case 1:
                printf("Parent opening file with O_EXLOCK | O_WRONLY \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_WRONLY;
                break;
            case 2:
                printf("Parent opening file with O_EXLOCK | O_RDWR \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_RDWR;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        printf("Parent verify got lease \n");
        bzero(&pb, sizeof(pb));
        error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
        if (error != 0) {
            XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
            goto done;
        }

        if (!(pb.lease_flags & SMB2_LEASE_GRANTED)) {
            XCTFail("Failed to get lease. lease_flags 0x%llx \n",
                    pb.lease_flags);
            goto done;
        }

        /* Close file */
        printf("Parent closing file \n");
        if (fd1 != -1) {
            error = close(fd1);
            if (error) {
                printf("close on fd1 failed %d:%s \n", errno, strerror(errno));
                //error = errno;
                goto done;
            }
            else {
                //fd1 = -1;
            }
        }

        printf("Parent verify got deferred close \n");
        bzero(&pb, sizeof(pb));
        error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
        if (error != 0) {
            XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
            goto done;
        }

        if (!(pb.lease_flags & SMB2_DEFERRED_CLOSE)) {
            XCTFail("Failed to get deferred close. lease_flags 0x%llx \n",
                    pb.lease_flags);
            goto done;
        }

        printf("Parent reopen file \n");
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        printf("Parent verify got deferred close \n");
        bzero(&pb, sizeof(pb));
        error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
        if (error != 0) {
            XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
            goto done;
        }

        if (pb.lease_def_close_reuse_cnt != 1) {
            if (pb.lease_def_close_reuse_cnt == 0) {
                XCTFail("Failed to get deferred close reuse. def_close_reuse_cnt %d \n",
                        pb.lease_def_close_reuse_cnt);
                goto done;
            }
            else {
                printf("File got reused more than once, def_close_reuse_cnt %d \n", pb.lease_def_close_reuse_cnt);
            }
        }

        /* Close file */
        printf("Parent closing file \n");
        if (fd1 != -1) {
            error = close(fd1);
            if (error) {
                printf("close on fd2 failed %d:%s \n", errno, strerror(errno));
                //error = errno;
                goto done;
            }
            else {
                fd1 = -1;
            }
        }
    } /* i1 loop */
        
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testDefCloseO_SHLOCK
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int fd1 = -1;
    int i1 = 0;
    struct smbStatPB pb = {0};

    /*
     * Test for correct deferred close behavior with file opened with O_SHLOCK
     *
     * 1. Open file with O_SHLOCK and all 3 variants of O_RDONLY/O_WRONLY/O_RDWR
     * 2. Verify got durable handle and lease
     * 3. Close the file
     * 4. Verify got deferred close
     * 5. Reopen file and verify deferred close got reused
     *
     * Total variants tested: 3
     *
     * Note that open(O_WRONLY) will end up with Read access added because the
     * SMBClient always tries to add Read so that UBC can be used. This will
     * result in open(O_RDWR) to end up with a defer use count of 3 instead
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test for correct deferred close behavior with file opened with O_SHLOCK",
                               "open,close,O_SHLOCK,deferred_close",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testDefCloseO_SHLOCK");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    for (i1 = 0; i1 < 3; i1++) {
        switch(i1) {
            case 0:
                printf("Parent opening file with O_SHLOCK | O_RDONLY \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDONLY;
                break;
            case 1:
                printf("Parent opening file with O_SHLOCK | O_WRONLY \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_WRONLY;
                break;
            case 2:
                printf("Parent opening file with O_SHLOCK | O_RDWR \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDWR;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        printf("Parent verify got lease \n");
        error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
        if (error != 0) {
            XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
            goto done;
        }

        if (!(pb.lease_flags & SMB2_LEASE_GRANTED)) {
            XCTFail("Failed to get lease. lease_flags 0x%llx \n",
                    pb.lease_flags);
            goto done;
        }

        /* Close file */
        printf("Parent closing file \n");
        if (fd1 != -1) {
            error = close(fd1);
            if (error) {
                printf("close on fd1 failed %d:%s \n", errno, strerror(errno));
                //error = errno;
                goto done;
            }
            else {
                //fd1 = -1;
            }
        }

        printf("Parent verify got deferred close \n");
        bzero(&pb, sizeof(pb));
        error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
        if (error != 0) {
            XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
            goto done;
        }

        if (!(pb.lease_flags & SMB2_DEFERRED_CLOSE)) {
            XCTFail("Failed to get deferred close. lease_flags 0x%llx \n",
                    pb.lease_flags);
            goto done;
        }

        printf("Parent reopen file \n");
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        printf("Parent verify got deferred close \n");
        bzero(&pb, sizeof(pb));
        error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
        if (error != 0) {
            XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
            goto done;
        }

        if (pb.lease_def_close_reuse_cnt != 1) {
            if (pb.lease_def_close_reuse_cnt == 0) {
                XCTFail("Failed to get deferred close reuse. def_close_reuse_cnt %d \n",
                        pb.lease_def_close_reuse_cnt);
                goto done;
            }
            else {
                printf("File got reused more than once, def_close_reuse_cnt %d \n", pb.lease_def_close_reuse_cnt);
            }
        }

        /* Close file */
        printf("Parent closing file \n");
        if (fd1 != -1) {
            error = close(fd1);
            if (error) {
                printf("close on fd2 failed %d:%s \n", errno, strerror(errno));
                //error = errno;
                goto done;
            }
            else {
                fd1 = -1;
            }
        }
    } /* i1 loop */
        
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

- (void)testDefCloseLeaseBreak
{
    int error = 0;
    char file_path1[PATH_MAX];
    char file_path2[PATH_MAX];
    int fd1 = -1, fd2 = -1;
    int mode1 = O_RDWR;
    int mode2 = O_RDONLY;
    int iteration = 0;
    char mp1[PATH_MAX];
    char mp2[PATH_MAX];
    struct smbStatPB pb = {0};

    /*
     * Test that a deferred close gets correctly closed by a lease break
     *
     * 1. Open file with O_RDWR and all 2 variants of O_EXLOCK/O_SHLOCK
     * 2. Verify got lease
     * 3. Close the file
     * 4. Verify got deferred close
     * 5. Open file on mp2 with same O_EXLOCK/O_SHLOCK
     * 6. If open works, then deferred close must have worked on mp1
     *
     * Total variants tested: 2
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that a deferred close gets correctly closed by a lease break",
                               "open,close,O_EXLOCK,O_SHLOCK,deferred_close,lease,lease_break",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need two mounts to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testDefCloseLeaseBreakMp1");
    do_create_mount_path(mp2, sizeof(mp2), "testDefCloseLeaseBreakMp2");
    
    error = mount_two_sessions(mp1, mp2, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Set up file paths on both mounts and create test file */
    error = setup_file_paths(mp1, mp2, default_test_filename,
                             file_path1, sizeof(file_path1),
                             file_path2, sizeof(file_path2));
    if (error) {
        XCTFail("setup_file_paths failed %d \n", error);
        goto done;
    }

again:
    if (iteration == 0) {
        printf("Test with O_EXLOCK \n");
        mode1 |= O_EXLOCK;
        mode2 |= O_EXLOCK;
    }
    else {
        printf("Repeat tests with O_SHLOCK \n");
        mode1 |= O_SHLOCK;
        mode2 |= O_SHLOCK;
    }

    /*
     * Open the file for the writer on mp1
     */
    printf("Opening file on mp1 \n");
    fd1 = open(file_path1, mode1);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path1,
                errno, strerror(errno));
        goto done;
    }

    printf("Verify got lease on mp1 \n");
    bzero(&pb, sizeof(pb));
    error = fsctl(file_path1, smbfsStatFSCTL, &pb, 0);
    if (error != 0) {
        XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
        goto done;
    }

    if (!(pb.lease_flags & SMB2_LEASE_GRANTED)) {
        XCTFail("Failed to get lease. lease_flags 0x%llx \n",
                pb.lease_flags);
        goto done;
    }

    /* Close file on mp1 which should write the data out to the server */
    printf("Closing file on mp1 \n");
    error = close(fd1);
    if (error) {
        XCTFail("close on fd1 failed %d:%s \n",
                error, strerror(error));
        goto done;
    }
    else {
        fd1 = -1;
    }

    printf("Verify got deferred close on mp1 \n");
    bzero(&pb, sizeof(pb));
    error = fsctl(file_path1, smbfsStatFSCTL, &pb, 0);
    if (error != 0) {
        XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
        goto done;
    }

    if (!(pb.lease_flags & SMB2_DEFERRED_CLOSE)) {
        XCTFail("Failed to get deferred close. lease_flags 0x%llx \n",
                pb.lease_flags);
        goto done;
    }


    /*
     * Switch to second connection, mp2 and open the file which should cause
     * a lease break to mp1 and mp1 should close the pending deferred close
     * so that mp2 can successfully open the file.
     */
    printf("Opening file on mp2 \n");
    fd2 = open(file_path2, mode2);
    if (fd2 == -1) {
        XCTFail("open on <%s> failed %d:%s \n",
                file_path2, errno, strerror(errno));
        goto done;
    }

    /*
     * No way to verify on mp1 that the file got a lease break except by
     * the fact that mp2 managed to open the file.
     */

    /* Close file on mp2 */
    printf("Closing file on mp2 \n");
    error = close(fd2);
    if (error) {
        XCTFail("close on fd2 failed %d:%s \n",
                error, strerror(error));
        goto done;
    }
    else {
        fd2 = -1;
    }

    if (iteration == 0) {
        iteration += 1;
        goto again;
    }

    /* Do the Delete on test file */
    error = remove(file_path1);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path1, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (fd1 != -1) {
        /* Close file on mp1 */
        error = close(fd1);
        if (error) {
            XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (fd2 != -1) {
        /* Close file on mp2 */
        error = close(fd2);
        if (error) {
            XCTFail("close on fd2 failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    if (unmount(mp2, MNT_FORCE) == -1) {
        XCTFail("unmount failed for second url %d\n", errno);
    }

    rmdir(mp1);
    rmdir(mp2);
}

- (void)testUpdateLease
{
    int error = 0;
    char file_path1[PATH_MAX];
    char file_path2[PATH_MAX];
    int fd1 = -1, fd2 = -1;
    int mode1 = O_RDWR;
    int mode2 = O_RDWR;
    char mp1[PATH_MAX];
    char mp2[PATH_MAX];
    struct smbStatPB pb = {0};
    struct smb_update_lease pb2 = {0};
    int max_wait = 5, i;

    /*
     * Test that a lease can get upgraded
     *
     * 1. Open file with O_RDWR on mp1
     * 2. Verify got RWH lease on mp1
     * 3. Open the file on mp2 which will break the W lease for mp1
     * 4. Write/read data on mp2 which will break the RH lease for mp1
     * 5. Verify lease is completely broken on mp1
     * 6. Close file on mp2 which becomes a deferred close
     * 7. Try to delete the file on mp2 which will do the actual close
     * 8. Do fsctl(smbfsUpdateLeaseFSCTL) to force lease update attempt on mp1
     * 9. Verify have RWH lease again on mp1
     *
     * Total variants tested: 1
     *
     * Note: fails against smbx which does not give dur handle or lease to user2, radar 84680950
     * Note: fails against smbx which does not grant upgraded lease, radar 84681694
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that a lease can get upgraded",
                               "open,read,write,close,deferred_close,lease,lease_break",
                               "1,2,3",
                               "84680950,84681694",
                               NULL);
        return;
    }

    /*
     * We will need two mounts to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testUpdateLease1");
    do_create_mount_path(mp2, sizeof(mp2), "testUpdateLease2");
    
    error = mount_two_sessions(mp1, mp2, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Set up file paths on both mounts and create test file */
    error = setup_file_paths(mp1, mp2, default_test_filename,
                             file_path1, sizeof(file_path1),
                             file_path2, sizeof(file_path2));
    if (error) {
        XCTFail("setup_file_paths failed %d \n", error);
        goto done;
    }

    /*
     * Open the file on mp1
     */
    printf("Opening file on mp1 with O_RDWR \n");
    fd1 = open(file_path1, mode1);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path1,
                errno, strerror(errno));
        goto done;
    }

    printf("Verify got RWH lease on mp1 \n");
    bzero(&pb, sizeof(pb));
    error = fsctl(file_path1, smbfsStatFSCTL, &pb, 0);
    if (error != 0) {
        XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
        goto done;
    }

    if (!(pb.lease_flags & SMB2_LEASE_GRANTED)) {
        XCTFail("Failed to get lease. lease_flags 0x%llx \n",
                pb.lease_flags);
        goto done;
    }

    /*
     * Servers that support NFS ace will have a Create/SetInfo/Close done
     * to set the unix mode bits, but that will cause the lease to break down
     * to 0x03.
     */
    if ((pb.lease_curr_state != 0x7) && (pb.lease_curr_state != 0x3)) {
        XCTFail("Failed to get lease state. lease_curr_state 0x%x != 0x07 or 0x03 \n",
                pb.lease_curr_state);
        goto done;
    }

    /*
     * Switch to second connection, mp2 and open the file which should cause
     * a lease break to mp1 to downgrade its current lease
     */
    printf("Opening file on mp2 with O_RDWR\n");
    fd2 = open(file_path2, mode2);
    if (fd2 == -1) {
        XCTFail("open on <%s> failed %d:%s \n",
                file_path2, errno, strerror(errno));
        goto done;
    }

    printf("Verify got RWH lease on mp2 \n");
    bzero(&pb, sizeof(pb));
    error = fsctl(file_path2, smbfsStatFSCTL, &pb, 0);
    if (error != 0) {
        XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
        goto done;
    }

    /*
     * smbx has odd bug of 84680950 where the second user does not get granted
     * a durable handle or lease. Work around it for now.
     */
    if (!(pb.lease_flags & SMB2_LEASE_GRANTED)) {
        printf("Failed to get lease. lease_flags 0x%llx. Assume this is a non fixed smbx server? \n",
                pb.lease_flags);
        
        /* turn data caching off */
        printf("Disabling UBC caching to work around smbx bug \n");
        if (fcntl(fd2, F_NOCACHE, 1) != 0) {
            XCTFail( "F_NOCACHE failed %d:%s \n",
                    errno, strerror(errno));
            goto done;
        }
    }

    /* Write/Read data on mp2 to completely break the lease on mp1 */
    printf("Write data on mp2 \n");
    error = write_and_verify(fd2, data1, sizeof(data1), 0);
    if (error) {
        XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path2,
                error, strerror(error));
        goto done;
    }
    
    /* Verify file on mp1 go its lease broken */

    /*
     * Wait for lease break on mp2 or for meta data cache to expire
     */
    for (i = 0; i < max_wait; i++) {
        bzero(&pb, sizeof(pb));
        error = fsctl(file_path1, smbfsStatFSCTL, &pb, 0);
        if (error) {
            XCTFail("Waiting fsctl failed %d:%s \n",
                    errno, strerror(errno));
            goto done;
        }

        if (pb.lease_flags & SMB2_LEASE_GRANTED) {
            /* Still have the lease, so keep waiting */
            printf("Still have the lease, sleep and try again \n");
            sleep(1);
        }
        else {
            printf("Lease is broken after waiting %d secs \n", i);
            break;
        }
    }
    
    if (i == max_wait) {
        XCTFail("Failed to lose lease. lease_flags 0x%llx lease_curr_state 0x%x \n",
                pb.lease_flags, pb.lease_curr_state);
        goto done;
    }

    /* Close file on mp2 */
    printf("Closing file on mp2 \n");
    error = close(fd2);
    if (error) {
        XCTFail("close on fd2 failed %d:%s \n",
                error, strerror(error));
        goto done;
    }
    else {
        fd2 = -1;
    }

    /*
     * Attempt the Delete on test file on mp2 to get def close to actually
     * close the file on mp2. Delete will fail because mp2 still has the file
     * open.
     */
    printf("Attempt delete on mp2 to get deferred close to close now \n");
    remove(file_path2);
    /* Dont care about an error */
    
    /* Give time for server state to settle down */
    sleep(1);
    
    /* Try a few times to get the server to update the lease */
    for (i = 0; i < max_wait; i++) {
        printf("Force lease update on mp1, attempt %d \n", i);
        bzero(&pb2, sizeof(pb2));
        error = fsctl(file_path1, smbfsUpdateLeaseFSCTL, &pb2, 0);
        if (error != 0) {
            XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
            goto done;
        }

        /* Verify we got full lease back on mp1 */
        printf("Verify got RWH lease again on mp1 \n");
        bzero(&pb, sizeof(pb));
        error = fsctl(file_path1, smbfsStatFSCTL, &pb, 0);
        if (error != 0) {
            XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
            goto done;
        }

        if (!(pb.lease_flags & SMB2_LEASE_GRANTED)) {
            printf("Still no lease granted, sleep and try again \n");
            sleep(1);
            continue;
        }

        if (pb.lease_curr_state != 0x7) {
            printf("Still wrong lease_curr_state 0x%x != 0x07, sleep and try again \n",
                   pb.lease_curr_state);
            sleep(1);
            continue;
        }
        else {
            printf("Lease succesfully updated after waiting %d secs \n", i);
            break;
        }
    }
    
    if (i == max_wait) {
        XCTFail("Failed to update lease. lease_flags 0x%llx lease_curr_state 0x%x \n",
                pb.lease_flags, pb.lease_curr_state);
        goto done;
    }

    /* Close file on mp1 */
    printf("Closing file on mp1 \n");
    error = close(fd1);
    if (error) {
        XCTFail("close on fd1 failed %d:%s \n",
                error, strerror(error));
        goto done;
    }
    else {
        fd1 = -1;
    }

    /* Do the Delete on test file */
    error = remove(file_path1);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path1, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (fd1 != -1) {
        /* Close file on mp1 */
        error = close(fd1);
        if (error) {
            XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (fd2 != -1) {
        /* Close file on mp2 */
        error = close(fd2);
        if (error) {
            XCTFail("close on fd2 failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    if (unmount(mp2, MNT_FORCE) == -1) {
        XCTFail("unmount failed for second url %d\n", errno);
    }

    rmdir(mp1);
    rmdir(mp2);
}

-(void)testFcopyfileLockConflict
{
    int error = 0;
    char mp1[PATH_MAX] = {0};
    char src_path[PATH_MAX] = {0}, dst_path[PATH_MAX] = {0};
    int src_fd = -1, dst_fd = -1;

    /*
     * Test that a fcopyfile() on exclusive locked files completes successfully
     *
     * 1. Open the source file with O_RDONLY
     * 2. Lock source file using LOCK_EX|LOCK_NB
     * 3. Open the destination file with O_CREAT|O_RDWR
     * 4. Lock destination file using LOCK_EX|LOCK_NB
     * 5. Copy the content of the source file to the destination file using fcopyfile()
     *
     * Total variants tested: 1
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test fcopyfile() behaviour of exclusively locked files",
                               "open,close,flock,LOCK_EX,fcopyfile",
                               "1,2,3",
                               "103792320,107648569",
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testFcopyfileLockConflict");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup source test file */
    error = initialFileSetup(mp1, src_path, sizeof(src_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(errno));
        goto done;
    }

    /* Open source file */
    src_fd = open(src_path, O_RDONLY, 0);
    if (src_fd == -1) {
        XCTFail("open on <%s> failed %d:%s \n", src_path, error, strerror(errno));
        goto done;
    }

    /* Lock source file */
    error = flock(src_fd, LOCK_EX | LOCK_NB);
    if (error) {
        XCTFail("flock on <%s> failed %d:%s \n", src_path, error, strerror(errno));
        goto done;
    }

    /* Set up destination file path */
    strlcpy(dst_path, mp1, sizeof(dst_path));
    strlcat(dst_path, "/", sizeof(dst_path));
    strlcat(dst_path, cur_test_dir, sizeof(dst_path));
    strlcat(dst_path, "/", sizeof(dst_path));
    strlcat(dst_path, "dest_file", sizeof(dst_path));

    /* Open destination file */
    dst_fd = open(dst_path, O_RDWR | O_CREAT, 0666);
    if (dst_fd == -1) {
        XCTFail("open on <%s> failed %d:%s \n", dst_path, error, strerror(errno));
        goto done;
    }

    /* Lock destination file */
    error = flock(dst_fd, LOCK_EX | LOCK_NB);
    if (error) {
        XCTFail("flock on <%s> failed %d:%s \n", dst_path, error, strerror(errno));
        goto done;
    }

    /* Execute fcopyfile on the newly created files */
    error = fcopyfile(src_fd, dst_fd, 0, COPYFILE_ALL);
    if (error) {
        XCTFail("fcopyfile from <%s> to <%s> failed %d:%s \n", src_path, dst_path, error, strerror(errno));
        goto done;
    }

    /* Unlock the destination file */
    error = flock(dst_fd, LOCK_UN);
    if (error) {
        XCTFail("flock on <%s> failed %d:%s \n", dst_path, error, strerror(errno));
        goto done;
    }

    /* Unlock the source file */
    error = flock(src_fd, LOCK_UN);
    if (error) {
        XCTFail("flock on <%s> failed %d:%s \n", src_path, error, strerror(errno));
        goto done;
    }

    /* Do the Delete on test files */
    error = remove(src_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>", src_path, strerror(errno), error);
        goto done;
    }

    error = remove(dst_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>", dst_path, strerror(errno), error);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (dst_fd != -1) {
        error = close(dst_fd);
        if (error) {
            XCTFail("close on dst_fd failed %d:%s \n", error, strerror(errno));
        }
    }

    if (src_fd != -1) {
        error = close(src_fd);
        if (error) {
            XCTFail("close on src_fd failed %d:%s \n", error, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", error);
    }

    rmdir(mp1);
}

-(void)testMultiProcessBRL
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int oflag2 = 0;
    int fd1 = -1;
    int fd2 = -1;
    int i1 = 0, i2 = 0;
    int expect_fail = 0;
    int expect_read1 = 0, expect_read2 = 0;
    int expect_write1 = 0, expect_write2 = 0;
    pid_t child_pid = 0;
    int child_error = 0;
    
    /*
     * Test for correct byte range locking behavior with other processes.
     * We do not test with O_EXLOCK, because brls would not be needed
     *
     * 1. Open file with O_RDONLY, O_RDWR, O_SHLOCK | O_RDONLY, O_SHLOCK | O_RDWR
     * 2. Lock 0-9 from start and then last 10 bytes from EOF
     * 3. Verify read/write
     * 4. Verify cant lock 4-6, cant unlock 5-9
     * 5. With a child process, try opening the same file with
     *    O_RDONLY, O_RDWR, O_SHLOCK | O_RDONLY, O_SHLOCK | O_RDWR
     * 5. If the child is allowed to open the file, then verify can lock 10-19,
     *    can not read/write 0-5, can read/write 10-15, can not lock 5-9, can
     *    not unlock 0-9. The brl of 10-19 is left for the close to free it.
     * 6. Child closes its file
     * 7. After all the child open attempts finish, parent verifies can
     *    lock 10-19 to verify the child close freed the brl, verifies
     *    read/write again, then closes its file
     *
     * Total variants tested: 16
     *
     * Note: fails against smbx, radar 84481913
     */
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test for correct byte range locking behavior with other processes. We do not test with O_EXLOCK, because brls would not be needed",
                               "open,close,read,write,O_SHLOCK,byte_range_lock",
                               "1,2,3",
                               "84481913",
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testMultiProcessBRL");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    for (i1 = 0; i1 < 4; i1++) {
        printf("----------\n");
        switch(i1) {
            case 0:
                printf("Parent opening file with O_RDONLY \n");
                oflag1 = O_NONBLOCK | O_RDONLY;
                expect_read1 = 1;
                expect_write1 = 0;
                break;
            case 1:
                printf("Parent opening file with O_RDWR \n");
                oflag1 = O_NONBLOCK | O_RDWR;
                expect_read1 = 1;
                expect_write1 = 1;
                break;
            case 2:
                printf("Parent opening file with O_SHLOCK | O_RDONLY \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDONLY;
                expect_read1 = 1;
                expect_write1 = 0;
                break;
            case 3:
                printf("Parent opening file with O_SHLOCK | O_RDWR \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDWR;
                expect_read1 = 1;
                expect_write1 = 1;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        printf("Parent locking 0-9 \n");
        error = byte_range_lock(file_path, fd1, 0, 10, kBRL_Lock, kBRL_FromStart);
        if (error) {
            XCTFail("lock of 0-9 on <%s> failed %d:%s \n", file_path,
                    error, strerror(error));
            goto done;
        }
        
        printf("Parent locking last ten bytes from EOF \n");
        error = byte_range_lock(file_path, fd1, -10, 10, kBRL_Lock, kBRL_FromEnd);
        if (error) {
            XCTFail("lock of 0-9 on <%s> failed %d:%s \n", file_path,
                    error, strerror(error));
            goto done;
        }

        if (expect_write1) {
            printf("Parent verifying write access \n");
            error = write_and_verify(fd1, data1, sizeof(data1),
                                     expect_read1 == 1 ? 0 : 1);
            if (error) {
                XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        if (expect_read1) {
            printf("Parent verifying read access \n");
            error = read_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        printf("Parent locking 4-6 and should fail with range already locked \n");
        error = byte_range_lock(file_path, fd1, 4, 3, kBRL_Lock, kBRL_FromStart);
        if (error != EACCES) {
            XCTFail("lock of 4-6 on <%s> did not fail as expected %d:%s \n", file_path,
                    error, strerror(error));
            if (error == 0) {
                //error = EINVAL; /* set to some error */
            }
            goto done;
        }

        printf("Parent unlocking 5-9 and should fail with matching lock not found \n");
        error = byte_range_lock(file_path, fd1, 5, 5, kBRL_UnLock, kBRL_FromStart);
        if (error != EAGAIN) {
            XCTFail("unlock of 5-9 on <%s> did not fail as expected %d:%s \n", file_path,
                    error, strerror(error));
            if (error == 0) {
                //error = EINVAL; /* set to some error */
            }
            goto done;
        }

        /* Kick off the child process now */
        child_pid = fork();
        if (child_pid != 0) {
            /*
             * Parent Testing code continues here
             */
            child_error = waitForChild(child_pid);
            if (child_error != 0) {
                XCTFail("Parent - Child test failed %d:%s \n",
                        child_error, strerror(child_error));
                goto done;
            }
            else {
                printf("Child test passed \n");
            }
            
            printf("Parent verifying child close freed lock on 10-19 \n");
            error = byte_range_lock(file_path, fd1, 10, 10, kBRL_Lock, kBRL_FromStart);
            if (error) {
                XCTFail("unlock of 0-9 on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }

            printf("Parent free lock on 10-19 \n");
            error = byte_range_lock(file_path, fd1, 10, 10, kBRL_UnLock, kBRL_FromStart);
            if (error) {
                XCTFail("unlock of 0-9 on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }

            /* Child changed the data so cant do verify on the read */
            if (expect_read1) {
                printf("Parent verifying read access \n");
               error = read_and_verify(fd1, data1, sizeof(data1), 1);
                if (error) {
                    XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }

            if (expect_write1) {
                printf("Parent verifying write access \n");
                error = write_and_verify(fd1, data1, sizeof(data1),
                                         expect_read1 == 1 ? 0 : 1);
                if (error) {
                    XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }

            printf("Parent unlocking 0-9 \n");
            error = byte_range_lock(file_path, fd1, 0, 10, kBRL_UnLock, kBRL_FromStart);
            if (error) {
                XCTFail("unlock of 0-9 on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }

            printf("Parent unlocking last ten bytes from EOF \n");
            error = byte_range_lock(file_path, fd1, -10, 10, kBRL_UnLock, kBRL_FromEnd);
            if (error) {
                XCTFail("unlock of last 10 bytes on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }

            /* Close file */
            printf("Parent closing file \n");
            if (fd1 != -1) {
                error = close(fd1);
                if (error) {
                    XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
                    //error = errno;
                    goto done;
                }
                else {
                    //fd1 = -1;
                }
            }
            
            /*
             * Since the child changed the data, need to reset it
             */
            printf("Parent resetting file to initial data \n");
            fd1 = open(file_path, O_NONBLOCK | O_RDWR);
            if (fd1 == -1) {
                XCTFail("open on <%s> failed %d:%s \n", file_path,
                       errno, strerror(errno));
                //error = errno;
                goto done;
            }

            /* Write out and verify initial data */
            printf("Writing initial data again \n");
            error = write_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("initial write_and_verify failed %d \n", error);
                close(fd1);
                goto done;
            }

            /* Close file */
            printf("Closing created file again \n");
            error = close(fd1);
            if (error) {
                XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
                //error = errno;
                goto done;
            }
            else {
                fd1 = -1;
            }
        }
        else {
            /*
             * Child Testing code starts here
             * Do not call XCTFail, just printf
             */
            for (i2 = 0; i2 < 4; i2++) {
                /*
                 * With the child process, try various operations
                 */
                printf("     ----------\n");
                switch(i2) {
                    case 0:
                        printf("     Child opening file with O_RDONLY \n");
                        oflag2 = O_NONBLOCK | O_RDONLY;
                        expect_read2 = 1;
                        expect_write2 = 0;
                        expect_fail = 0;
                        break;
                    case 1:
                        printf("     Child opening file with O_RDWR \n");
                        oflag2 = O_NONBLOCK | O_RDWR;
                        expect_read2 = 1;
                        expect_write2 = 1;
                        if (oflag1 & O_SHLOCK) {
                            /* First open has denyWrite access so read/write will fail */
                            expect_fail = 1;
                        }
                        else {
                            expect_fail = 0;
                        }
                        break;
                    case 2:
                        printf("     Child opening file with O_SHLOCK | O_RDONLY \n");
                        oflag2 = O_SHLOCK | O_NONBLOCK | O_RDONLY;
                        expect_read2 = 1;
                        expect_write2 = 0;
                        if (oflag1 & (O_WRONLY | O_RDWR)) {
                            /* First open has write access so O_SHLOCK will fail */
                            expect_fail = 1;
                        }
                        else {
                            expect_fail = 0;
                        }
                        break;
                    case 3:
                        printf("     Child opening file with O_SHLOCK | O_RDWR \n");
                        oflag2 = O_SHLOCK | O_NONBLOCK | O_RDWR;
                        expect_read2 = 1;
                        expect_write2 = 1;
                        if (oflag1 & (O_WRONLY | O_RDWR | O_SHLOCK)) {
                            /* First open has write access or O_SHLOCK, so open will fail */
                            expect_fail = 1;
                        }
                        else {
                            expect_fail = 0;
                        }
                        break;
                    default:
                        printf("     Unknown selector for i2 %d \n", i2);
                        _Exit(EINVAL);
                }
                
                /*
                 * Open the testfile in child process
                 */
                fd2 = open(file_path, oflag2);
                if (fd2 == -1) {
                    if (expect_fail) {
                        /* Expected Failure */
                    }
                    else {
                        /* Unexpected failure */
                        printf("     Unexpected failure - open on <%s> failed %d:%s \n", file_path,
                                errno, strerror(errno));
                        _Exit(errno);
                    }
                }
                else {
                    if (expect_fail) {
                        /* Unexpected success */
                        printf("     Unexpected success - open on <%s> worked \n", file_path);
                        _Exit(EINVAL);
                    }
                    else {
                        /* Expected success */
                    }
                }

                if (expect_fail == 0) {
                    printf("     Child verifying lock 10-19 \n");
                    error = byte_range_lock(file_path, fd2, 10, 10, kBRL_Lock, kBRL_FromStart);
                    if (error) {
                        printf("     lock of 10-19 on <%s> failed %d:%s \n", file_path,
                                error, strerror(error));
                        _Exit(error);
                    }

                   if (expect_read2) {
                       printf("     Child verifying fail to read bytes 0-5 \n");
                       error = read_offset(fd2, 0, 5);
                       
                       /* Older client returns EIO, newer returns EACCESS */
                       if ((error != EIO) && (error != EACCES)) {
                           printf("     read_offset on <%s> did not fail as expected %d:%s \n", file_path,
                                  error, strerror(error));
                           if (error == 0) {
                               error = EINVAL; /* set to some error */
                           }
                           _Exit(error);
                        }

                        printf("     Child verifying read bytes 10-15 \n");
                        error = read_offset(fd2, 10, 5);
                        if (error) {
                            printf("     read_offset on <%s> failed %d:%s \n", file_path,
                                   error, strerror(error));
                            _Exit(error);
                        }
                    }
                    
                    if (expect_write2) {
                        printf("     Child verifying fail to write 0-5 \n");
                        error = write_offset(fd2, data1, 0, 5);
                        /* Older client returns EIO, newer returns EACCESS */
                        if ((error != EIO) && (error != EACCES)) {
                            printf("     write_offset on <%s> did not fail as expected %d:%s \n", file_path,
                                   error, strerror(error));
                            if (error == 0) {
                                error = EINVAL; /* set to some error */
                            }
                            _Exit(error);
                        }
                        
                        printf("     Child verifying write bytes 10-15 \n");
                        error = write_offset(fd2, data1, 10, 5);
                        if (error) {
                            printf("     write_offset on <%s> failed %d:%s \n", file_path,
                                   error, strerror(error));
                            _Exit(error);
                        }
                    }
                    
                    printf("     Child verifying fail to lock 5-9 \n");
                    error = byte_range_lock(file_path, fd2, 5, 5, kBRL_Lock, kBRL_FromStart);
                    if (error != EACCES) {
                        printf("     lock of 5-9 on <%s> did not fail as expected %d:%s \n", file_path,
                                error, strerror(error));
                        if (error == 0) {
                            error = EINVAL; /* set to some error */
                        }
                        _Exit(error);
                    }

                    printf("     Child verifying fail to unlock 0-9 \n");
                    error = byte_range_lock(file_path, fd2, 0, 10, kBRL_UnLock, kBRL_FromStart);
                    if (error != EAGAIN) {
                        printf("     unlock of 0-9 on <%s> did not fail as expected %d:%s \n", file_path,
                                error, strerror(error));
                        if (error == 0) {
                            error = EINVAL; /* set to some error */
                        }
                        _Exit(error);
                    }

                    /*
                     * Leave the lock for 10-19 in place to test that the close
                     * frees the lock
                     */
                }
                
                /* Close file if needed */
                if (fd2 != -1) {
                    printf("     Child closing file \n");
                    error = close(fd2);
                    if (error) {
                        printf("     close on fd2 failed %d:%s \n", errno, strerror(errno));
                        _Exit(errno);
                    }
                    else {
                        //fd2 = -1;
                    }
                }
            } /* i2 loop */
        
            /*
             * Parents always have to clean up after this children,
             * so child just exits and leaves cleanup to parent process.
             */
            _Exit(0);
        } /* Child test code */
    } /* i1 loop */
        
    
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testMultiUserBRL
{
    int error = 0;
    char file_path1[PATH_MAX] = {0};
    char file_path2[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    char mp2[PATH_MAX];
    int oflag1 = 0;
    int oflag2 = 0;
    int fd1 = -1;
    int fd2 = -1;
    int i1 = 0, i2 = 0;
    int expect_fail = 0;
    int expect_read1 = 0, expect_read2 = 0;
    int expect_write1 = 0, expect_write2 = 0;
    pid_t child_pid = 0;
    int child_error = 0;
    
    /*
     * Test for correct byte range locking behavior with different users.
     * We do not test with O_EXLOCK, because brls would not be needed
     *
     * 1. Open file with O_RDONLY, O_RDWR, O_SHLOCK | O_RDONLY, O_SHLOCK | O_RDWR
     * 2. Lock 0-9 from start and then last 10 bytes from EOF
     * 3. Verify read/write
     * 4. Verify cant lock 4-6, cant unlock 5-9
     * 5. With a child process (as different user), try opening the same file with
     *    O_RDONLY, O_RDWR, O_SHLOCK | O_RDONLY, O_SHLOCK | O_RDWR
     * 5. If the child is allowed to open the file, then verify can lock 10-19,
     *    can not read/write 0-5, can read/write 10-15, can not lock 5-9, can
     *    not unlock 0-9. The brl of 10-19 is left for the close to free it.
     * 6. Child closes its file
     * 7. After all the child open attempts finish, parent verifies can
     *    lock 10-19 to verify the child close freed the brl, verifies
     *    read/write again, then closes its file
     *
     * Total variants tested: 16
     *
     * Note: fails against smbx, radar 84481913
     */
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test for correct byte range locking behavior with different users. We do not test with O_EXLOCK, because brls would not be needed",
                               "open,close,read,write,O_SHLOCK,byte_range_lock",
                               "1,2,3",
                               "84481913",
                               NULL);
        return;
    }

    /*
     * We will need just two mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testMultiUserBRL1");
    do_create_mount_path(mp2, sizeof(mp2), "testMultiUserBRL2");

    error = mount_two_sessions(mp1, mp2, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Set up file paths on both mounts and create test file */
    error = setup_file_paths(mp1, mp2, default_test_filename,
                             file_path1, sizeof(file_path1),
                             file_path2, sizeof(file_path2));
    if (error) {
        XCTFail("setup_file_paths failed %d \n", error);
        goto done;
    }

    /*
     * Write the initial data
     */
    printf("Parent setting file to initial data \n");
    fd1 = open(file_path1, O_NONBLOCK | O_RDWR);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path1,
               errno, strerror(errno));
        //error = errno;
        goto done;
    }

    /* Write out and verify initial data */
    printf("Writing initial data \n");
    error = write_and_verify(fd1, data1, sizeof(data1), 0);
    if (error) {
        XCTFail("initial write_and_verify failed %d \n", error);
        close(fd1);
        goto done;
    }

    /* Close file */
    printf("Closing created file \n");
    error = close(fd1);
    if (error) {
        XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        //error = errno;
        goto done;
    }
    else {
        fd1 = -1;
    }

    for (i1 = 0; i1 < 4; i1++) {
        printf("----------\n");
        switch(i1) {
            case 0:
                printf("Parent opening file with O_RDONLY \n");
                oflag1 = O_NONBLOCK | O_RDONLY;
                expect_read1 = 1;
                expect_write1 = 0;
                break;
            case 1:
                printf("Parent opening file with O_RDWR \n");
                oflag1 = O_NONBLOCK | O_RDWR;
                expect_read1 = 1;
                expect_write1 = 1;
                break;
            case 2:
                printf("Parent opening file with O_SHLOCK | O_RDONLY \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDONLY;
                expect_read1 = 1;
                expect_write1 = 0;
                break;
            case 3:
                printf("Parent opening file with O_SHLOCK | O_RDWR \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDWR;
                expect_read1 = 1;
                expect_write1 = 1;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path1, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path1,
                    errno, strerror(errno));
            goto done;
        }

        printf("Parent locking 0-9 \n");
        error = byte_range_lock(file_path1, fd1, 0, 10, kBRL_Lock, kBRL_FromStart);
        if (error) {
            XCTFail("lock of 0-9 on <%s> failed %d:%s \n", file_path1,
                    error, strerror(error));
            goto done;
        }
        
        printf("Parent locking last ten bytes from EOF \n");
        error = byte_range_lock(file_path1, fd1, -10, 10, kBRL_Lock, kBRL_FromEnd);
        if (error) {
            XCTFail("lock of 0-9 on <%s> failed %d:%s \n", file_path1,
                    error, strerror(error));
            goto done;
        }

        /* Do write to restore the data after child wrote over it */
        if (expect_write1) {
            printf("Parent verifying write access \n");
            error = write_and_verify(fd1, data1, sizeof(data1),
                                     expect_read1 == 1 ? 0 : 1);
            if (error) {
                XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path1,
                        error, strerror(error));
                goto done;
            }
        }

        if (expect_read1) {
            printf("Parent verifying read access \n");
            error = read_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path1,
                        error, strerror(error));
                goto done;
            }
        }

        printf("Parent locking 4-6 and should fail with range already locked \n");
        error = byte_range_lock(file_path1, fd1, 4, 3, kBRL_Lock, kBRL_FromStart);
        if (error != EACCES) {
            XCTFail("lock of 4-6 on <%s> did not fail as expected %d:%s \n", file_path1,
                    error, strerror(error));
            if (error == 0) {
                //error = EINVAL; /* set to some error */
            }
            goto done;
        }

        printf("Parent unlocking 5-9 and should fail with matching lock not found \n");
        error = byte_range_lock(file_path1, fd1, 5, 5, kBRL_UnLock, kBRL_FromStart);
        if (error != EAGAIN) {
            XCTFail("unlock of 5-9 on <%s> did not fail as expected %d:%s \n", file_path1,
                    error, strerror(error));
            if (error == 0) {
                //error = EINVAL; /* set to some error */
            }
            goto done;
        }

        /* Kick off the child process now */
        child_pid = fork();
        if (child_pid != 0) {
            /*
             * Parent Testing code continues here
             */
            child_error = waitForChild(child_pid);
            if (child_error != 0) {
                XCTFail("Parent - Child test failed %d:%s \n",
                        child_error, strerror(child_error));
                goto done;
            }
            else {
                printf("Child test passed \n");
            }
            
            printf("Parent verifying child close freed lock on 10-19 \n");
            error = byte_range_lock(file_path1, fd1, 10, 10, kBRL_Lock, kBRL_FromStart);
            if (error) {
                XCTFail("unlock of 0-9 on <%s> failed %d:%s \n", file_path1,
                        error, strerror(error));
                goto done;
            }

            printf("Parent free lock on 10-19 \n");
            error = byte_range_lock(file_path1, fd1, 10, 10, kBRL_UnLock, kBRL_FromStart);
            if (error) {
                XCTFail("unlock of 0-9 on <%s> failed %d:%s \n", file_path1,
                        error, strerror(error));
                goto done;
            }

            /* Child changed the data so cant do verify on the read */
            if (expect_read1) {
                printf("Parent verifying read access \n");
               error = read_and_verify(fd1, data1, sizeof(data1), 1);
                if (error) {
                    XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path1,
                            error, strerror(error));
                    goto done;
                }
            }

            if (expect_write1) {
                printf("Parent verifying write access \n");
                error = write_and_verify(fd1, data1, sizeof(data1),
                                         expect_read1 == 1 ? 0 : 1);
                if (error) {
                    XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path1,
                            error, strerror(error));
                    goto done;
                }
            }

            printf("Parent unlocking 0-9 \n");
            error = byte_range_lock(file_path1, fd1, 0, 10, kBRL_UnLock, kBRL_FromStart);
            if (error) {
                XCTFail("unlock of 0-9 on <%s> failed %d:%s \n", file_path1,
                        error, strerror(error));
                goto done;
            }

            printf("Parent unlocking last ten bytes from EOF \n");
            error = byte_range_lock(file_path1, fd1, -10, 10, kBRL_UnLock, kBRL_FromEnd);
            if (error) {
                XCTFail("unlock of last 10 bytes on <%s> failed %d:%s \n", file_path1,
                        error, strerror(error));
                goto done;
            }

            /* Close file */
            printf("Parent closing file \n");
            if (fd1 != -1) {
                error = close(fd1);
                if (error) {
                    XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
                    //error = errno;
                    goto done;
                }
                else {
                    //fd1 = -1;
                }
            }
            
            /*
             * Since the child changed the data, need to reset it
             */
            printf("Parent resetting file to initial data \n");
            fd1 = open(file_path1, O_NONBLOCK | O_RDWR);
            if (fd1 == -1) {
                XCTFail("open on <%s> failed %d:%s \n", file_path1,
                       errno, strerror(errno));
                //error = errno;
                goto done;
            }

            /* Write out and verify initial data */
            printf("Writing initial data again \n");
            error = write_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("initial write_and_verify failed %d \n", error);
                close(fd1);
                goto done;
            }

            /* Close file */
            printf("Closing created file again \n");
            error = close(fd1);
            if (error) {
                XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
                //error = errno;
                goto done;
            }
            else {
                fd1 = -1;
            }
        }
        else {
            /*
             * Child Testing code starts here
             * Do not call XCTFail, just printf
             */
            for (i2 = 0; i2 < 4; i2++) {
                /*
                 * With the child process, try various operations
                 */
                printf("     ----------\n");
                switch(i2) {
                    case 0:
                        printf("     Child opening file with O_RDONLY \n");
                        oflag2 = O_NONBLOCK | O_RDONLY;
                        expect_read2 = 1;
                        expect_write2 = 0;
                        expect_fail = 0;
                        break;
                    case 1:
                        printf("     Child opening file with O_RDWR \n");
                        oflag2 = O_NONBLOCK | O_RDWR;
                        expect_read2 = 1;
                        expect_write2 = 1;
                        if (oflag1 & O_SHLOCK) {
                            /* First open has denyWrite access so read/write will fail */
                            expect_fail = 1;
                        }
                        else {
                            expect_fail = 0;
                        }
                        break;
                    case 2:
                        printf("     Child opening file with O_SHLOCK | O_RDONLY \n");
                        oflag2 = O_SHLOCK | O_NONBLOCK | O_RDONLY;
                        expect_read2 = 1;
                        expect_write2 = 0;
                        if (oflag1 & (O_WRONLY | O_RDWR)) {
                            /* First open has write access so O_SHLOCK will fail */
                            expect_fail = 1;
                        }
                        else {
                            expect_fail = 0;
                        }
                        break;
                    case 3:
                        printf("     Child opening file with O_SHLOCK | O_RDWR \n");
                        oflag2 = O_SHLOCK | O_NONBLOCK | O_RDWR;
                        expect_read2 = 1;
                        expect_write2 = 1;
                        if (oflag1 & (O_WRONLY | O_RDWR | O_SHLOCK)) {
                            /* First open has write access or O_SHLOCK, so open will fail */
                            expect_fail = 1;
                        }
                        else {
                            expect_fail = 0;
                        }
                        break;
                    default:
                        printf("     Unknown selector for i2 %d \n", i2);
                        _Exit(EINVAL);
                }
                
                /*
                 * Open the testfile in child process
                 */
                fd2 = open(file_path2, oflag2);
                if (fd2 == -1) {
                    if (expect_fail) {
                        /* Expected Failure */
                    }
                    else {
                        /* Unexpected failure */
                        printf("     Unexpected failure - open on <%s> failed %d:%s \n", file_path2,
                                errno, strerror(errno));
                        _Exit(errno);
                    }
                }
                else {
                    if (expect_fail) {
                        /* Unexpected success */
                        printf("     Unexpected success - open on <%s> worked \n", file_path2);
                        _Exit(EINVAL);
                    }
                    else {
                        /* Expected success */
                    }
                }

                if (expect_fail == 0) {
                    printf("     Child verifying lock 10-19 \n");
                    error = byte_range_lock(file_path2, fd2, 10, 10, kBRL_Lock, kBRL_FromStart);
                    if (error) {
                        printf("     lock of 10-19 on <%s> failed %d:%s \n", file_path2,
                                error, strerror(error));
                        _Exit(error);
                    }

                   if (expect_read2) {
                        printf("     Child verifying fail to read bytes 0-5 \n");
                        error = read_offset(fd2, 0, 5);
                       /* Older client returns EIO, newer returns EACCESS */
                       if ((error != EIO) && (error != EACCES)) {
                            printf("     read_offset on <%s> did not fail as expected %d:%s \n", file_path2,
                                   error, strerror(error));
                            if (error == 0) {
                                error = EINVAL; /* set to some error */
                            }
                            _Exit(error);
                        }

                        printf("     Child verifying read bytes 10-15 \n");
                        error = read_offset(fd2, 10, 5);
                        if (error) {
                            printf("     read_offset on <%s> failed %d:%s \n", file_path2,
                                   error, strerror(error));
                            _Exit(error);
                        }
                    }
                    
                    if (expect_write2) {
                        printf("     Child verifying fail to write 0-5 \n");
                        error = write_offset(fd2, data1, 0, 5);
                        /* Older client returns EIO, newer returns EACCESS */
                        if ((error != EIO) && (error != EACCES)) {
                            printf("     write_offset on <%s> did not fail as expected %d:%s \n", file_path2,
                                   error, strerror(error));
                            if (error == 0) {
                                error = EINVAL; /* set to some error */
                            }
                            _Exit(error);
                        }
                        
                        printf("     Child verifying write bytes 10-15 \n");
                        error = write_offset(fd2, data1, 10, 5);
                        if (error) {
                            printf("     write_offset on <%s> failed %d:%s \n", file_path2,
                                   error, strerror(error));
                            _Exit(error);
                        }
                    }
                    
                    printf("     Child verifying fail to lock 5-9 \n");
                    error = byte_range_lock(file_path2, fd2, 5, 5, kBRL_Lock, kBRL_FromStart);
                    if (error != EACCES) {
                        printf("     lock of 5-9 on <%s> did not fail as expected %d:%s \n", file_path2,
                                error, strerror(error));
                        if (error == 0) {
                            error = EINVAL; /* set to some error */
                        }
                        _Exit(error);
                    }

                    printf("     Child verifying fail to unlock 0-9 \n");
                    error = byte_range_lock(file_path2, fd2, 0, 10, kBRL_UnLock, kBRL_FromStart);
                    if (error != EAGAIN) {
                        printf("     unlock of 0-9 on <%s> did not fail as expected %d:%s \n", file_path2,
                                error, strerror(error));
                        if (error == 0) {
                            error = EINVAL; /* set to some error */
                        }
                        _Exit(error);
                    }
                    /*
                     * Leave the lock for 10-19 in place to test that the close
                     * frees the lock
                     */
                }
                
                /* Close file if needed */
                if (fd2 != -1) {
                    printf("     Child closing file \n");
                    error = close(fd2);
                    if (error) {
                        printf("     close on fd2 failed %d:%s \n", errno, strerror(errno));
                        _Exit(errno);
                    }
                    else {
                        //fd2 = -1;
                    }
                }
            } /* i2 loop */
        
            /*
             * Parents always have to clean up after this children,
             * so child just exits and leaves cleanup to parent process.
             */
            _Exit(0);
        } /* Child test code */
    } /* i1 loop */
        
    
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path1);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path1, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    if (unmount(mp2, MNT_FORCE) == -1) {
        XCTFail("unmount failed for second url %d\n", errno);
    }

    rmdir(mp1);
    rmdir(mp2);
}

-(void)testMultiProcessFlock
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int oflag2 = 0;
    int fd1 = -1;
    int fd2 = -1;
    int i1 = 0, i2 = 0;
    int expect_fail = 0;
    int expect_read1 = 0, expect_read2 = 0;
    int expect_write1 = 0, expect_write2 = 0;
    pid_t child_pid = 0;
    int child_error = 0;
    
    /*
     * Test for correct flock() behavior with other processes.
     * We do not test with O_EXLOCK because flock() does not make sense.
     * We do not test with O_SHLOCK because
     *   1. If file is already open with O_SHLOCK, then flock() is a nop and
     *      returns no error
     *   2. If file is open and flock'd, then an open with O_SHLOCK will get
     *      an error
     *
     * 1. Open file with O_RDONLY, O_RDWR
     * 2. flock(LOCK_EX) the file
     * 3. Verify cant lock 0-9 due to flock()
     * 4. Verify read/write
     * 5. With a child process, try opening the same file with
     *    O_RDONLY, O_RDWR
     * 5. If the child is allowed to open the file, then verify can not lock 5-9,
     *    and CAN read/write. flock() DOES NOT prevent other process from
     *    read/write since the flock()'d fd may have been transferred to another
     *    process and that must be allowed.
     * 6. Child closes its file
     * 7. After all the child open attempts finish, parent verifies read/write
     *    again, then removes the flock
     * 8. With another child process, try opening the same file with
     *    O_RDONLY, O_RDWR
     * 9. If the child is allowed to open the file, then verify can lock 5-9,
     *    unlock 5-9, and can read/write
     * 10. Child closes its file
     * 11. After all the child open attempts finish again, the Parent closes
     *     its file, then open/write/close to reset back to initial data.
     *
     * Total variants tested: 4
     */
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test for correct flock() behavior with other processes. We do not test with O_EXLOCK/O_SHLOCK because flock() does make sense.",
                               "open,close,read,write,flock",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testMultiProcessFlock");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    for (i1 = 0; i1 < 2; i1++) {
        printf("----------\n");
        switch(i1) {
            case 0:
                printf("Parent opening file with O_RDONLY \n");
                oflag1 = O_NONBLOCK | O_RDONLY;
                expect_read1 = 1;
                expect_write1 = 0;
                break;
            case 1:
                printf("Parent opening file with O_RDWR \n");
                oflag1 = O_NONBLOCK | O_RDWR;
                expect_read1 = 1;
                expect_write1 = 1;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        printf("Parent flock on file \n");
        error = flock(fd1, LOCK_EX);
        if (error) {
            XCTFail("flock on <%s> failed %d:%s \n", file_path,
                    error, strerror(error));
            goto done;
        }
        
        printf("Parent verifying fail to lock 0-9 \n");
        error = byte_range_lock(file_path, fd1, 0, 10, kBRL_Lock, kBRL_FromStart);
        if (error != EINVAL) {
            XCTFail("lock of 0-9 on <%s> did not fail as expected %d:%s \n", file_path,
                    error, strerror(error));
            if (error == 0) {
                //error = EBADF; /* set to some error */
            }
            goto done;
        }

        if (expect_write1) {
            printf("Parent verifying write access \n");
            error = write_and_verify(fd1, data1, sizeof(data1),
                                     expect_read1 == 1 ? 0 : 1);
            if (error) {
                XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        if (expect_read1) {
            printf("Parent verifying read access \n");
            error = read_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        /* Kick off the child process now */
        child_pid = fork();
        if (child_pid != 0) {
            /*
             * Parent Testing code continues here
             */
            child_error = waitForChild(child_pid);
            if (child_error != 0) {
                XCTFail("Parent - Child test failed %d:%s \n",
                        child_error, strerror(child_error));
                goto done;
            }
            else {
                printf("Child test passed \n");
            }
            
            if (expect_read1) {
                printf("Parent verifying read access \n");
                error = read_and_verify(fd1, data1, sizeof(data1), 0);
                if (error) {
                    XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }

            if (expect_write1) {
                printf("Parent verifying write access \n");
                error = write_and_verify(fd1, data1, sizeof(data1),
                                         expect_read1 == 1 ? 0 : 1);
                if (error) {
                    XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }

            printf("Parent removing flock \n");
            error = flock(fd1, LOCK_UN);
            if (error) {
                XCTFail("flock on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
            
            /* Parent leaves the file open for next part of test */
        }
        else {
            /*
             * Child Testing code starts here
             * Do not call XCTFail, just printf
             */
            for (i2 = 0; i2 < 2; i2++) {
                /*
                 * With the child process, try various operations
                 */
                printf("     ----------\n");
                switch(i2) {
                    case 0:
                        printf("     Child opening file with O_RDONLY \n");
                        oflag2 = O_NONBLOCK | O_RDONLY;
                        
                        /*
                         * For flock and multi process, child will end up using
                         * SMB FID from parent and IO will work. Sigh.
                         */
                        expect_read2 = expect_read1 ? 1 : 0;
                        expect_write2 = 0;
                        expect_fail = 0;
                        break;
                    case 1:
                        printf("     Child opening file with O_RDWR \n");
                        oflag2 = O_NONBLOCK | O_RDWR;
                        
                        /*
                         * For flock and multi process, child will end up using
                         * SMB FID from parent and IO will work. Sigh.
                         */
                        expect_read2 = expect_read1 ? 1 : 0;
                        expect_write2 = expect_write1 ? 1 : 0;
                        if (oflag1 & O_SHLOCK) {
                            /* First open has denyWrite access so read/write will fail */
                            expect_fail = 1;
                        }
                        else {
                            expect_fail = 0;
                        }
                        break;
                    default:
                        printf("     Unknown selector for i2 %d \n", i2);
                        _Exit(EINVAL);
                }
                
                /*
                 * Open the testfile in child process
                 */
                fd2 = open(file_path, oflag2);
                if (fd2 == -1) {
                    if (expect_fail) {
                        /* Expected Failure */
                    }
                    else {
                        /* Unexpected failure */
                        printf("     Unexpected failure - open on <%s> failed %d:%s \n", file_path,
                                errno, strerror(errno));
                        _Exit(errno);
                    }
                }
                else {
                    if (expect_fail) {
                        /* Unexpected success */
                        printf("     Unexpected success - open on <%s> worked \n", file_path);
                        _Exit(EINVAL);
                    }
                    else {
                        /* Expected success */
                    }
                }

                if (expect_fail == 0) {
                    printf("     Child verifying fail to lock 5-9 \n");
                    error = byte_range_lock(file_path, fd2, 5, 5, kBRL_Lock, kBRL_FromStart);
                    if (error != EINVAL) {
                        printf("     lock of 5-9 on <%s> did not fail as expected %d:%s \n", file_path,
                                error, strerror(error));
                        if (error == 0) {
                            error = EINVAL; /* set to some error */
                        }
                        _Exit(error);
                    }

                   if (expect_read2) {
                        printf("     Child verifying read access \n");
                        error = read_and_verify(fd2, data1, sizeof(data1), 0);
                        if (error) {
                            printf("     read_and_verify on <%s> failed %d:%s \n", file_path,
                                   error, strerror(error));
                            _Exit(error);
                        }
                    }
                    
                    if (expect_write2) {
                        printf("     Child verifying write access \n");
                        error = write_and_verify(fd2, data1, sizeof(data1),
                                                 expect_read2 == 1 ? 0 : 1);
                        if (error) {
                            printf("     write_and_verify on <%s> failed %d:%s \n", file_path,
                                   error, strerror(error));
                            _Exit(error);
                        }
                    }
                }
                
                /* Close file if needed */
                if (fd2 != -1) {
                    printf("     Child closing file \n");
                    error = close(fd2);
                    if (error) {
                        printf("     close on fd2 failed %d:%s \n", errno, strerror(errno));
                        _Exit(errno);
                    }
                    else {
                        //fd2 = -1;
                    }
                }
            } /* i2 loop */
        
            /*
             * Parents always have to clean up after this children,
             * so child just exits and leaves cleanup to parent process.
             */
            _Exit(0);
        } /* Child test code */

        /*
         * Resuming parent code here
         */
        
        /*
         * Parent has now removed the flock()
         * child process verifies it can open/read/write the file now
         *
         * Kick off the child process again now
         */
        //expect_fail = 0;

        /* Kick off the child process now for second attempt at open */
        child_pid = fork();
        if (child_pid != 0) {
            /*
             * Parent Testing code continues here
             */
            child_error = waitForChild(child_pid);
            if (child_error != 0) {
                XCTFail("Parent - Child test 2 failed %d:%s \n",
                        child_error, strerror(child_error));
                goto done;
            }
            else {
                printf("Child test 2 passed \n");
            }
            
            /* Close file */
            printf("Parent closing file \n");
            if (fd1 != -1) {
                error = close(fd1);
                if (error) {
                    XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
                    //error = errno;
                    goto done;
                }
                else {
                    //fd1 = -1;
                }
            }
            
            /*
             * Since the child changed the data, need to reset it
             */
            printf("Parent resetting file to initial data \n");
            fd1 = open(file_path, O_NONBLOCK | O_RDWR);
            if (fd1 == -1) {
                XCTFail("open on <%s> failed %d:%s \n", file_path,
                       errno, strerror(errno));
                //error = errno;
                goto done;
            }

            /* Write out and verify initial data */
            printf("Writing initial data again \n");
            error = write_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("initial write_and_verify failed %d \n", error);
                close(fd1);
                goto done;
            }

            /* Close file */
            printf("Closing created file again \n");
            error = close(fd1);
            if (error) {
                XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
                //error = errno;
                goto done;
            }
            else {
                fd1 = -1;
            }
        }
        else {
            /*
             * Child Testing code starts here
             * Do not call XCTFail, just printf
             */
            for (i2 = 0; i2 < 2; i2++) {
                /*
                 * With the child process, try various operations
                 */
                printf("     ----------\n");
                switch(i2) {
                    case 0:
                        printf("     Child opening file with O_RDONLY \n");
                        oflag2 = O_NONBLOCK | O_RDONLY;
                        expect_read2 = 1;
                        expect_write2 = 0;
                        expect_fail = 0;
                        break;
                    case 1:
                        printf("     Child opening file with O_RDWR \n");
                        oflag2 = O_NONBLOCK | O_RDWR;
                        expect_read2 = 1;
                        expect_write2 = 1;
                        if (oflag1 & O_SHLOCK) {
                            /* First open has denyWrite access so read/write will fail */
                            expect_fail = 1;
                        }
                        else {
                            expect_fail = 0;
                        }
                        break;
                    default:
                        printf("     Unknown selector for i2 %d \n", i2);
                        _Exit(EINVAL);
                }
                
                /*
                 * Open the testfile in child process
                 */
                fd2 = open(file_path, oflag2);
                if (fd2 == -1) {
                    if (expect_fail) {
                        /* Expected Failure */
                    }
                    else {
                        /* Unexpected failure */
                        printf("     Unexpected failure - open on <%s> failed %d:%s \n", file_path,
                                errno, strerror(errno));
                        _Exit(errno);
                    }
                }
                else {
                    if (expect_fail) {
                        /* Unexpected success */
                        printf("     Unexpected success - open on <%s> worked \n", file_path);
                        _Exit(EINVAL);
                    }
                    else {
                        /* Expected success */
                    }
                }

                if (expect_fail == 0) {
                    printf("     Child verifying lock 0-9 \n");
                    error = byte_range_lock(file_path, fd2, 0, 10, kBRL_Lock, kBRL_FromStart);
                    if (error) {
                        printf("     lock of 0-9 on <%s> failed %d:%s \n", file_path,
                                error, strerror(error));
                        _Exit(error);
                    }

                    printf("     Child verifying unlock 0-9 \n");
                    error = byte_range_lock(file_path, fd2, 0, 10, kBRL_UnLock, kBRL_FromStart);
                    if (error) {
                        printf("     unlock of 0-9 on <%s> failed %d:%s \n", file_path,
                                error, strerror(error));
                        _Exit(error);
                    }

                    if (expect_read2) {
                        printf("     Child verifying read bytes 0-5 \n");
                        error = read_offset(fd2, 0, 5);
                        if (error) {
                            printf("     read_offset on <%s> failed %d:%s \n", file_path,
                                   error, strerror(error));
                            _Exit(error);
                        }
                    }
                    
                    if (expect_write2) {
                        printf("     Child verifying write 0-5 \n");
                        error = write_offset(fd2, data1, 0, 5);
                        if (error) {
                            printf("     write_offset on <%s> failed %d:%s \n", file_path,
                                   error, strerror(error));
                            _Exit(error);
                        }
                    }
                }
                
                /* Close file if needed */
                if (fd2 != -1) {
                    printf("     Child closing file \n");
                    error = close(fd2);
                    if (error) {
                        printf("     close on fd2 failed %d:%s \n", errno, strerror(errno));
                        _Exit(errno);
                    }
                    else {
                        //fd2 = -1;
                    }
                }
            } /* i2 loop */
        
            /*
             * Parents always have to clean up after this children,
             * so child just exits and leaves cleanup to parent process.
             */
            _Exit(0);
        } /* Child test code */
    } /* i1 loop */
        
    
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testMultiUserFlock
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char file_path2[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    char mp2[PATH_MAX] = {0};
    int oflag1 = 0;
    int oflag2 = 0;
    int fd1 = -1;
    int fd2 = -1;
    int i1 = 0, i2 = 0;
    int expect_fail = 0;
    int expect_read1 = 0, expect_read2 = 0;
    int expect_write1 = 0, expect_write2 = 0;
    pid_t child_pid = 0;
    int child_error = 0;
    
    /*
     * Test for correct flock() behavior with different users.
     * We do not test with O_EXLOCK, because flock() does make sense.
     * We do not test with O_SHLOCK because
     *   1. If file is already open with O_SHLOCK, then flock() is a nop and
     *      returns no error
     *   2. If file is open and flock'd, then an open with O_SHLOCK will get
     *      an error
     *
     * 1. Open file with O_RDONLY, O_RDWR
     * 2. flock(LOCK_EX) the file
     * 3. Verify cant lock 0-9 due to flock()
     * 4. Verify read/write
     * 5. With a child process, try opening the same file with
     *    O_RDONLY, O_RDWR
     * 5. If the child is allowed to open the file, then verify can not lock 5-9,
     *    and can not read/write
     * 6. Child closes its file
     * 7. After all the child open attempts finish, parent verifies read/write
     *    again, then removes the flock
     * 8. With another child process, try opening the same file with
     *    O_RDONLY, O_RDWR
     * 9. If the child is allowed to open the file, then verify can lock 5-9,
     *    unlock 5-9, and can read/write
     * 10. Child closes its file
     * 11. After all the child open attempts finish again, the Parent closes
     *     its file, then open/write/close to reset back to initial data.
     *
     * Total variants tested: 4
     */
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test for correct flock() behavior with different users. We do not test with O_EXLOCK/O_SHLOCK because flock() does make sense.",
                               "open,close,read,write,flock",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just two mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testMultiUserFlock1");
    do_create_mount_path(mp2, sizeof(mp2), "testMultiUserFlock2");

    error = mount_two_sessions(mp1, mp2, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Set up file paths on both mounts and create test file */
    error = setup_file_paths(mp1, mp2, default_test_filename,
                             file_path, sizeof(file_path),
                             file_path2, sizeof(file_path2));
    if (error) {
        XCTFail("setup_file_paths failed %d \n", error);
        goto done;
    }
    
    /*
     * Write the initial data
     */
    printf("Parent setting file to initial data \n");
    fd1 = open(file_path, O_NONBLOCK | O_RDWR);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
               errno, strerror(errno));
        //error = errno;
        goto done;
    }

    /* Write out and verify initial data */
    printf("Writing initial data \n");
    error = write_and_verify(fd1, data1, sizeof(data1), 0);
    if (error) {
        XCTFail("initial write_and_verify failed %d \n", error);
        close(fd1);
        goto done;
    }

    /* Close file */
    printf("Closing created file \n");
    error = close(fd1);
    if (error) {
        XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        //error = errno;
        goto done;
    }
    else {
        fd1 = -1;
    }
    
    for (i1 = 0; i1 < 2; i1++) {
        printf("----------\n");
        switch(i1) {
            case 0:
                printf("Parent opening file with O_RDONLY \n");
                oflag1 = O_NONBLOCK | O_RDONLY;
                expect_read1 = 1;
                expect_write1 = 0;
                break;
            case 1:
                printf("Parent opening file with O_RDWR \n");
                oflag1 = O_NONBLOCK | O_RDWR;
                expect_read1 = 1;
                expect_write1 = 1;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        printf("Parent flock on file \n");
        error = flock(fd1, LOCK_EX);
        if (error) {
            XCTFail("flock on <%s> failed %d:%s \n", file_path,
                    error, strerror(error));
            goto done;
        }

        printf("Parent verifying fail to lock 0-9 \n");
        error = byte_range_lock(file_path, fd1, 0, 10, kBRL_Lock, kBRL_FromStart);
        if (error != EINVAL) {
            XCTFail("lock of 0-9 on <%s> did not fail as expected %d:%s \n", file_path,
                    error, strerror(error));
            if (error == 0) {
                //error = EBADF; /* set to some error */
            }
            goto done;
        }

        if (expect_write1) {
            printf("Parent verifying write access \n");
            error = write_and_verify(fd1, data1, sizeof(data1),
                                     expect_read1 == 1 ? 0 : 1);
            if (error) {
                XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        if (expect_read1) {
            printf("Parent verifying read access \n");
            error = read_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        /* Kick off the child process now */
        child_pid = fork();
        if (child_pid != 0) {
            /*
             * Parent Testing code continues here
             */
            child_error = waitForChild(child_pid);
            if (child_error != 0) {
                XCTFail("Parent - Child test failed %d:%s \n",
                        child_error, strerror(child_error));
                goto done;
            }
            else {
                printf("Child test passed \n");
            }
            
            if (expect_read1) {
                printf("Parent verifying read access \n");
                error = read_and_verify(fd1, data1, sizeof(data1), 0);
                if (error) {
                    XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }

            if (expect_write1) {
                printf("Parent verifying write access \n");
                error = write_and_verify(fd1, data1, sizeof(data1),
                                         expect_read1 == 1 ? 0 : 1);
                if (error) {
                    XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }

            printf("Parent removing flock \n");
            error = flock(fd1, LOCK_UN);
            if (error) {
                XCTFail("flock on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
            
            /* Parent leaves the file open for next part of test */
        }
        else {
            /*
             * Child Testing code starts here
             * Do not call XCTFail, just printf
             */
            for (i2 = 0; i2 < 2; i2++) {
                /*
                 * With the child process, try various operations
                 */
                printf("     ----------\n");
                switch(i2) {
                    case 0:
                        printf("     Child opening file with O_RDONLY \n");
                        oflag2 = O_NONBLOCK | O_RDONLY;
                        expect_read2 = 1;
                        expect_write2 = 0;
                        expect_fail = 0;
                        break;
                    case 1:
                        printf("     Child opening file with O_RDWR \n");
                        oflag2 = O_NONBLOCK | O_RDWR;
                        expect_read2 = 1;
                        expect_write2 = 1;
                        if (oflag1 & O_SHLOCK) {
                            /* First open has denyWrite access so read/write will fail */
                            expect_fail = 1;
                        }
                        else {
                            expect_fail = 0;
                        }
                        break;
                    default:
                        printf("     Unknown selector for i2 %d \n", i2);
                        _Exit(EINVAL);
                }
                
                /*
                 * Open the testfile in child process
                 */
                fd2 = open(file_path2, oflag2);
                if (fd2 == -1) {
                    if (expect_fail) {
                        /* Expected Failure */
                    }
                    else {
                        /* Unexpected failure */
                        printf("     Unexpected failure - open on <%s> failed %d:%s \n", file_path2,
                                errno, strerror(errno));
                        _Exit(errno);
                    }
                }
                else {
                    if (expect_fail) {
                        /* Unexpected success */
                        printf("     Unexpected success - open on <%s> worked \n", file_path2);
                        _Exit(EINVAL);
                    }
                    else {
                        /* Expected success */
                    }
                }

                if (expect_fail == 0) {
                    printf("     Child verifying fail to lock 5-9 \n");
                    error = byte_range_lock(file_path2, fd2, 5, 5, kBRL_Lock, kBRL_FromStart);
                    if (error != EACCES) {
                        printf("     lock of 5-9 on <%s> did not fail as expected %d:%s \n", file_path2,
                                error, strerror(error));
                        if (error == 0) {
                            error = EINVAL; /* set to some error */
                        }
                        _Exit(error);
                    }

                   if (expect_read2) {
                        printf("     Child verifying fail to read bytes 0-5 \n");
                        error = read_offset(fd2, 0, 5);
                        if (error != EIO) {
                            printf("     read_offset on <%s> did not fail as expected %d:%s \n", file_path2,
                                   error, strerror(error));
                            if (error == 0) {
                                error = EINVAL; /* set to some error */
                            }
                            _Exit(error);
                        }
                    }
                    
                    if (expect_write2) {
                        printf("     Child verifying fail to write 0-5 \n");
                        error = write_offset(fd2, data1, 0, 5);
                        if (error != EIO) {
                            printf("     write_offset on <%s> did not fail as expected %d:%s \n", file_path2,
                                   error, strerror(error));
                            if (error == 0) {
                                error = EINVAL; /* set to some error */
                            }
                            _Exit(error);
                        }
                    }
                }
                
                /* Close file if needed */
                if (fd2 != -1) {
                    printf("     Child closing file \n");
                    error = close(fd2);
                    if (error) {
                        printf("     close on fd2 failed %d:%s \n", errno, strerror(errno));
                        _Exit(errno);
                    }
                    else {
                        //fd2 = -1;
                    }
                }
            } /* i2 loop */
        
            /*
             * Parents always have to clean up after this children,
             * so child just exits and leaves cleanup to parent process.
             */
            _Exit(0);
        } /* Child test code */

        /*
         * Resuming parent code here
         */
        
        /*
         * Parent has now removed the flock()
         * child process verifies it can open/read/write the file now
         *
         * Kick off the child process again now
         */
        //expect_fail = 0;

        /* Kick off the child process now for second attempt at open */
        child_pid = fork();
        if (child_pid != 0) {
            /*
             * Parent Testing code continues here
             */
            child_error = waitForChild(child_pid);
            if (child_error != 0) {
                XCTFail("Parent - Child test 2 failed %d:%s \n",
                        child_error, strerror(child_error));
                goto done;
            }
            else {
                printf("Child test 2 passed \n");
            }
            
            /* Close file */
            printf("Parent closing file \n");
            if (fd1 != -1) {
                error = close(fd1);
                if (error) {
                    XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
                    //error = errno;
                    goto done;
                }
                else {
                    //fd1 = -1;
                }
            }
            
            /*
             * Since the child changed the data, need to reset it
             */
            printf("Parent resetting file to initial data \n");
            fd1 = open(file_path, O_NONBLOCK | O_RDWR);
            if (fd1 == -1) {
                XCTFail("open on <%s> failed %d:%s \n", file_path,
                       errno, strerror(errno));
                //error = errno;
                goto done;
            }

            /* Write out and verify initial data */
            printf("Writing initial data again \n");
            error = write_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("initial write_and_verify failed %d \n", error);
                close(fd1);
                goto done;
            }

            /* Close file */
            printf("Closing created file again \n");
            error = close(fd1);
            if (error) {
                XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
                //error = errno;
                goto done;
            }
            else {
                fd1 = -1;
            }
        }
        else {
            /*
             * Child Testing code starts here
             * Do not call XCTFail, just printf
             */
            for (i2 = 0; i2 < 2; i2++) {
                /*
                 * With the child process, try various operations
                 */
                printf("     ----------\n");
                switch(i2) {
                    case 0:
                        printf("     Child opening file with O_RDONLY \n");
                        oflag2 = O_NONBLOCK | O_RDONLY;
                        expect_read2 = 1;
                        expect_write2 = 0;
                        expect_fail = 0;
                        break;
                    case 1:
                        printf("     Child opening file with O_RDWR \n");
                        oflag2 = O_NONBLOCK | O_RDWR;
                        expect_read2 = 1;
                        expect_write2 = 1;
                        if (oflag1 & O_SHLOCK) {
                            /* First open has denyWrite access so read/write will fail */
                            expect_fail = 1;
                        }
                        else {
                            expect_fail = 0;
                        }
                        break;
                    default:
                        printf("     Unknown selector for i2 %d \n", i2);
                        _Exit(EINVAL);
                }
                
                /*
                 * Open the testfile in child process
                 */
                fd2 = open(file_path2, oflag2);
                if (fd2 == -1) {
                    if (expect_fail) {
                        /* Expected Failure */
                    }
                    else {
                        /* Unexpected failure */
                        printf("     Unexpected failure - open on <%s> failed %d:%s \n", file_path2,
                                errno, strerror(errno));
                        _Exit(errno);
                    }
                }
                else {
                    if (expect_fail) {
                        /* Unexpected success */
                        printf("     Unexpected success - open on <%s> worked \n", file_path2);
                        _Exit(EINVAL);
                    }
                    else {
                        /* Expected success */
                    }
                }

                if (expect_fail == 0) {
                    printf("     Child verifying lock 0-9 \n");
                    error = byte_range_lock(file_path2, fd2, 0, 10, kBRL_Lock, kBRL_FromStart);
                    if (error) {
                        printf("     lock of 0-9 on <%s> failed %d:%s \n", file_path2,
                                error, strerror(error));
                        _Exit(error);
                    }

                    printf("     Child verifying unlock 0-9 \n");
                    error = byte_range_lock(file_path2, fd2, 0, 10, kBRL_UnLock, kBRL_FromStart);
                    if (error) {
                        printf("     unlock of 0-9 on <%s> failed %d:%s \n", file_path2,
                                error, strerror(error));
                        _Exit(error);
                    }

                    if (expect_read2) {
                        printf("     Child verifying read bytes 0-5 \n");
                        error = read_offset(fd2, 0, 5);
                        if (error) {
                            printf("     read_offset on <%s> failed %d:%s \n", file_path2,
                                   error, strerror(error));
                            _Exit(error);
                        }
                    }
                    
                    if (expect_write2) {
                        printf("     Child verifying write 0-5 \n");
                        error = write_offset(fd2, data1, 0, 5);
                        if (error) {
                            printf("     write_offset on <%s> failed %d:%s \n", file_path2,
                                   error, strerror(error));
                            _Exit(error);
                        }
                    }
                }
                
                /* Close file if needed */
                if (fd2 != -1) {
                    printf("     Child closing file \n");
                    error = close(fd2);
                    if (error) {
                        printf("     close on fd2 failed %d:%s \n", errno, strerror(errno));
                        _Exit(errno);
                    }
                    else {
                        //fd2 = -1;
                    }
                }
            } /* i2 loop */
        
            /*
             * Parents always have to clean up after this children,
             * so child just exits and leaves cleanup to parent process.
             */
            _Exit(0);
        } /* Child test code */
    } /* i1 loop */
        
    
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    if (unmount(mp2, MNT_FORCE) == -1) {
        XCTFail("unmount failed for second url %d\n", errno);
    }

    rmdir(mp1);
    rmdir(mp2);
}

-(void)testMultiProcessFlockClose
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int oflag2 = 0;
    int fd1 = -1;
    int fd2 = -1;
    int i1 = 0, i2 = 0;
    int expect_fail = 0;
    int expect_read1 = 0, expect_read2 = 0;
    int expect_write1 = 0, expect_write2 = 0;
    pid_t child_pid = 0;
    int child_error = 0;
    
    /*
     * Test that a flock() is freed by a close when multiple processes are used.
     * We do not test with O_EXLOCK because flock() does not make sense.
     * We do not test with O_SHLOCK because
     *   1. If file is already open with O_SHLOCK, then flock() is a nop and
     *      returns no error
     *   2. If file is open and flock'd, then an open with O_SHLOCK will get
     *      an error
     *
     * 1. Open file with O_RDONLY, O_RDWR
     * 2. Turn off UBC
     * 3. Verify read/write
     * 4. With a child process, try opening the same file with
     *    O_RDONLY, O_RDWR.
     * 5. If the child is allowed to open the file, then turn off UBC,
     *    do the flock(), verify read/write, then close the file.
     *    The flock() is left for the close to free it.
     * 6. Child closes its file
     * 7. After all the child open attempts finish, verifies read/write again
     *    to verify the flock() is gone, then closes its file
     *
     * Total variants tested: 4
     */
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that a flock() is freed by a close when multiple processes are used. We do not test with O_EXLOCK/O_SHLOCK, because flock() is not allowed for those open modes",
                               "open,close,read,write,flock",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testMultiProcessFlockClose");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    for (i1 = 1; i1 < 2; i1++) {
        printf("----------\n");
        switch(i1) {
            case 0:
                printf("Parent opening file with O_RDONLY \n");
                oflag1 = O_NONBLOCK | O_RDONLY;
                expect_read1 = 1;
                expect_write1 = 0;
                break;
            case 1:
                printf("Parent opening file with O_RDWR \n");
                oflag1 = O_NONBLOCK | O_RDWR;
                expect_read1 = 1;
                expect_write1 = 1;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        /* turn data caching off */
        printf("Parent disabling UBC caching \n");
        if (fcntl(fd1, F_NOCACHE, 1) != 0) {
            XCTFail( "F_NOCACHE failed %d:%s \n",
                    errno, strerror(errno));
            goto done;
        }

        if (expect_write1) {
            printf("Parent verifying write access \n");
            error = write_and_verify(fd1, data1, sizeof(data1),
                                     expect_read1 == 1 ? 0 : 1);
            if (error) {
                XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        if (expect_read1) {
            printf("Parent verifying read access \n");
            error = read_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        /* Kick off the child process now */
        child_pid = fork();
        if (child_pid != 0) {
            /*
             * Parent Testing code continues here
             */
            child_error = waitForChild(child_pid);
            if (child_error != 0) {
                XCTFail("Parent - Child test failed %d:%s \n",
                        child_error, strerror(child_error));
                goto done;
            }
            else {
                printf("Child test passed \n");
            }
            
            /*
             * Verify read/write to check that flock got released when child
             * closed their file
             */
            if (expect_read1) {
                printf("Parent verifying read access \n");
                error = read_and_verify(fd1, data1, sizeof(data1), 1);
                if (error) {
                    XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }

            if (expect_write1) {
                printf("Parent verifying write access \n");
                error = write_and_verify(fd1, data1, sizeof(data1),
                                         expect_read1 == 1 ? 0 : 1);
                if (error) {
                    XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }

            /* Close file */
            printf("Parent closing file \n");
            if (fd1 != -1) {
                error = close(fd1);
                if (error) {
                    XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
                    //error = errno;
                    goto done;
                }
                else {
                    fd1 = -1;
                }
            }
        }
        else {
            /*
             * Child Testing code starts here
             * Do not call XCTFail, just printf
             */
            for (i2 = 0; i2 < 2; i2++) {
                /*
                 * With the child process, try various operations
                 */
                printf("     ----------\n");
                switch(i2) {
                    case 0:
                        printf("     Child opening file with O_RDONLY \n");
                        oflag2 = O_NONBLOCK | O_RDONLY;
                        expect_read2 = 1;
                        expect_write2 = 0;
                        expect_fail = 0;
                        break;
                    case 1:
                        printf("     Child opening file with O_RDWR \n");
                        oflag2 = O_NONBLOCK | O_RDWR;
                        expect_read2 = 1;
                        expect_write2 = 1;
                        if (oflag1 & O_SHLOCK) {
                            /* First open has denyWrite access so read/write will fail */
                            expect_fail = 1;
                        }
                        else {
                            expect_fail = 0;
                        }
                        break;
                    default:
                        printf("     Unknown selector for i2 %d \n", i2);
                        _Exit(EINVAL);
                }
                
                /*
                 * Open the testfile in child process
                 */
                fd2 = open(file_path, oflag2);
                if (fd2 == -1) {
                    if (expect_fail) {
                        /* Expected Failure */
                    }
                    else {
                        /* Unexpected failure */
                        printf("     Unexpected failure - open on <%s> failed %d:%s \n", file_path,
                                errno, strerror(errno));
                        _Exit(errno);
                    }
                }
                else {
                    if (expect_fail) {
                        /* Unexpected success */
                        printf("     Unexpected success - open on <%s> worked \n", file_path);
                        _Exit(EINVAL);
                    }
                    else {
                        /* Expected success */
                    }
                }

                if (expect_fail == 0) {
                    /* turn data caching off */
                    printf("     Child disabling UBC caching \n");
                    if (fcntl(fd2, F_NOCACHE, 1) != 0) {
                        printf( "     Child F_NOCACHE failed %d:%s \n",
                               errno, strerror(errno));
                        _Exit(errno);
                    }

                    printf("     Child flock on file \n");
                    error = flock(fd2, LOCK_EX);
                    if (error) {
                        printf("flock on <%s> failed %d:%s \n", file_path,
                                error, strerror(error));
                        _Exit(error);
                    }

                   if (expect_read2) {
                       printf("     Child verifying read access \n");
                       error = read_and_verify(fd2, data1, sizeof(data1), 0);
                       if (error) {
                           printf("     read_and_verify on <%s> failed %d:%s \n", file_path,
                                  error, strerror(error));
                           _Exit(error);
                       }
                    }
                    
                    if (expect_write2) {
                        printf("     Child verifying write access \n");
                        error = write_and_verify(fd2, data1, sizeof(data1),
                                                 expect_read1 == 1 ? 0 : 1);
                        if (error) {
                            printf("     write_and_verify on <%s> failed %d:%s \n", file_path,
                                   error, strerror(error));
                            _Exit(error);
                        }
                    }

                    /*
                     * Leave the flock() in place to test that the close frees
                     * the lock
                     */
                }
                
                /* Close file if needed */
                if (fd2 != -1) {
                    printf("     Child closing file \n");
                    error = close(fd2);
                    if (error) {
                        printf("     close on fd2 failed %d:%s \n", errno, strerror(errno));
                        _Exit(errno);
                    }
                    else {
                        //fd2 = -1;
                    }
                }
            } /* i2 loop */
        
            /*
             * Parents always have to clean up after this children,
             * so child just exits and leaves cleanup to parent process.
             */
            _Exit(0);
        } /* Child test code */
    } /* i1 loop */
        
    
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testMultiUserFlockClose
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char file_path2[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    char mp2[PATH_MAX] = {0};
    int oflag1 = 0;
    int oflag2 = 0;
    int fd1 = -1;
    int fd2 = -1;
    int i1 = 0, i2 = 0;
    int expect_fail = 0;
    int expect_read1 = 0, expect_read2 = 0;
    int expect_write1 = 0, expect_write2 = 0;
    pid_t child_pid = 0;
    int child_error = 0;
    
    /*
     * Test that a flock() is freed by a close with different users.
     * We do not test with O_EXLOCK because flock() does not make sense.
     * We do not test with O_SHLOCK because
     *   1. If file is already open with O_SHLOCK, then flock() is a nop and
     *      returns no error
     *   2. If file is open and flock'd, then an open with O_SHLOCK will get
     *      an error
     *
     * 1. Open file with O_RDONLY, O_RDWR
     * 2. Turn off UBC
     * 3. Verify read/write
     * 4. With a child process, try opening the same file with
     *    O_RDONLY, O_RDWR.
     * 5. If the child is allowed to open the file, then turn off UBC,
     *    do the flock(), verify read/write, then close the file.
     *    The flock() is left for the close to free it.
     * 6. Child closes its file
     * 7. After all the child open attempts finish, verifies read/write again
     *    to verify the flock() is gone, then closes its file
     *
     * Total variants tested: 4
     */
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that a flock() is freed by a close with different usrs. We do not test with O_EXLOCK/O_SHLOCK, because flock() is not allowed for those open modes",
                               "open,close,read,write,O_SHLOCK,flock",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just two mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testMultiUserFlockClose1");
    do_create_mount_path(mp2, sizeof(mp2), "testMultiUserFlockClose2");

    error = mount_two_sessions(mp1, mp2, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Set up file paths on both mounts and create test file */
    error = setup_file_paths(mp1, mp2, default_test_filename,
                             file_path, sizeof(file_path),
                             file_path2, sizeof(file_path2));
    if (error) {
        XCTFail("setup_file_paths failed %d \n", error);
        goto done;
    }
    
    /*
     * Write the initial data
     */
    printf("Parent setting file to initial data \n");
    fd1 = open(file_path, O_NONBLOCK | O_RDWR);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
               errno, strerror(errno));
        //error = errno;
        goto done;
    }

    /* Write out and verify initial data */
    printf("Writing initial data \n");
    error = write_and_verify(fd1, data1, sizeof(data1), 0);
    if (error) {
        XCTFail("initial write_and_verify failed %d \n", error);
        close(fd1);
        goto done;
    }

    /* Close file */
    printf("Closing created file \n");
    error = close(fd1);
    if (error) {
        XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        //error = errno;
        goto done;
    }
    else {
        fd1 = -1;
    }

    for (i1 = 1; i1 < 2; i1++) {
        printf("----------\n");
        switch(i1) {
            case 0:
                printf("Parent opening file with O_RDONLY \n");
                oflag1 = O_NONBLOCK | O_RDONLY;
                expect_read1 = 1;
                expect_write1 = 0;
                break;
            case 1:
                printf("Parent opening file with O_RDWR \n");
                oflag1 = O_NONBLOCK | O_RDWR;
                expect_read1 = 1;
                expect_write1 = 1;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        /* turn data caching off */
        printf("Parent disabling UBC caching \n");
        if (fcntl(fd1, F_NOCACHE, 1) != 0) {
            XCTFail( "F_NOCACHE failed %d:%s \n",
                    errno, strerror(errno));
            goto done;
        }

        if (expect_write1) {
            printf("Parent verifying write access \n");
            error = write_and_verify(fd1, data1, sizeof(data1),
                                     expect_read1 == 1 ? 0 : 1);
            if (error) {
                XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        if (expect_read1) {
            printf("Parent verifying read access \n");
            error = read_and_verify(fd1, data1, sizeof(data1), 0);
            if (error) {
                XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }
        }

        /* Kick off the child process now */
        child_pid = fork();
        if (child_pid != 0) {
            /*
             * Parent Testing code continues here
             */
            child_error = waitForChild(child_pid);
            if (child_error != 0) {
                XCTFail("Parent - Child test failed %d:%s \n",
                        child_error, strerror(child_error));
                goto done;
            }
            else {
                printf("Child test passed \n");
            }
            
            /*
             * Verify read/write to check that flock got released when child
             * closed their file
             */
            if (expect_read1) {
                printf("Parent verifying read access \n");
                error = read_and_verify(fd1, data1, sizeof(data1), 1);
                if (error) {
                    XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }

            if (expect_write1) {
                printf("Parent verifying write access \n");
                error = write_and_verify(fd1, data1, sizeof(data1),
                                         expect_read1 == 1 ? 0 : 1);
                if (error) {
                    XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path,
                            error, strerror(error));
                    goto done;
                }
            }

            /* Close file */
            printf("Parent closing file \n");
            if (fd1 != -1) {
                error = close(fd1);
                if (error) {
                    XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
                    //error = errno;
                    goto done;
                }
                else {
                    fd1 = -1;
                }
            }
        }
        else {
            /*
             * Child Testing code starts here
             * Do not call XCTFail, just printf
             */
            for (i2 = 0; i2 < 2; i2++) {
                /*
                 * With the child process, try various operations
                 */
                printf("     ----------\n");
                switch(i2) {
                    case 0:
                        printf("     Child opening file with O_RDONLY \n");
                        oflag2 = O_NONBLOCK | O_RDONLY;
                        expect_read2 = 1;
                        expect_write2 = 0;
                        expect_fail = 0;
                        break;
                    case 1:
                        printf("     Child opening file with O_RDWR \n");
                        oflag2 = O_NONBLOCK | O_RDWR;
                        expect_read2 = 1;
                        expect_write2 = 1;
                        if (oflag1 & O_SHLOCK) {
                            /* First open has denyWrite access so read/write will fail */
                            expect_fail = 1;
                        }
                        else {
                            expect_fail = 0;
                        }
                        break;
                    default:
                        printf("     Unknown selector for i2 %d \n", i2);
                        _Exit(EINVAL);
                }
                
                /*
                 * Open the testfile in child process
                 */
                fd2 = open(file_path2, oflag2);
                if (fd2 == -1) {
                    if (expect_fail) {
                        /* Expected Failure */
                    }
                    else {
                        /* Unexpected failure */
                        printf("     Unexpected failure - open on <%s> failed %d:%s \n", file_path2,
                                errno, strerror(errno));
                        _Exit(errno);
                    }
                }
                else {
                    if (expect_fail) {
                        /* Unexpected success */
                        printf("     Unexpected success - open on <%s> worked \n", file_path2);
                        _Exit(EINVAL);
                    }
                    else {
                        /* Expected success */
                    }
                }

                if (expect_fail == 0) {
                    /* turn data caching off */
                    printf("     Child disabling UBC caching \n");
                    if (fcntl(fd2, F_NOCACHE, 1) != 0) {
                        printf( "     Child F_NOCACHE failed %d:%s \n",
                               errno, strerror(errno));
                        _Exit(errno);
                    }

                    printf("     Child flock on file \n");
                    error = flock(fd2, LOCK_EX);
                    if (error) {
                        printf("flock on <%s> failed %d:%s \n", file_path2,
                                error, strerror(error));
                        _Exit(error);
                    }

                   if (expect_read2) {
                       printf("     Child verifying read access \n");
                       error = read_and_verify(fd2, data1, sizeof(data1), 0);
                       if (error) {
                           printf("     read_and_verify on <%s> failed %d:%s \n", file_path2,
                                  error, strerror(error));
                           _Exit(error);
                       }
                    }
                    
                    if (expect_write2) {
                        printf("     Child verifying write access \n");
                        error = write_and_verify(fd2, data1, sizeof(data1),
                                                 expect_read1 == 1 ? 0 : 1);
                        if (error) {
                            printf("     write_and_verify on <%s> failed %d:%s \n", file_path2,
                                   error, strerror(error));
                            _Exit(error);
                        }
                    }

                    /*
                     * Leave the flock() in place to test that the close frees
                     * the lock
                     */
                }
                
                /* Close file if needed */
                if (fd2 != -1) {
                    printf("     Child closing file \n");
                    error = close(fd2);
                    if (error) {
                        printf("     close on fd2 failed %d:%s \n", errno, strerror(errno));
                        _Exit(errno);
                    }
                    else {
                        //fd2 = -1;
                    }
                }
            } /* i2 loop */
        
            /*
             * Parents always have to clean up after this children,
             * so child just exits and leaves cleanup to parent process.
             */
            _Exit(0);
        } /* Child test code */
    } /* i1 loop */
        
    
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    if (unmount(mp2, MNT_FORCE) == -1) {
        XCTFail("unmount failed for second url %d\n", errno);
    }

    rmdir(mp1);
    rmdir(mp2);
}


-(void)testMmap
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int fd1 = -1;
    int i1 = 0;
    //int expect_read1 = 0;
    //int expect_write1 = 0;
    void *ret_address = NULL;
    struct smbStatPB pb = {0};
    int protection = 0, flags = MAP_FILE | MAP_SHARED;
    //int first_len = 4096;
    int second_len = 8192;
    
    /*
     * Test for correct mmap/mnomap behavior.
     *
     * 1. Open file with O_RDONLY, O_RDWR,
     *    O_SHLOCK | O_RDONLY, O_SHLOCK | O_RDWR,
     *    O_EXLOCK | O_RDONLY, O_EXLOCK | O_RDWR
     *    O_WRONLY is not allowed
     *
     * Note: verifying the ref count is disabled since this is all caching
     * and its difficult to know exactly when certain mmap/mnomap operations
     * actually take place.
     *
     * 2. Call mmap twice to verify calling mmap multiple times is fine
     * 3. Verify refcnt is 2 and file is mmapped
     * 4. Call msync to verify mmap is still valid
     * 5. Call munmap once
     * 6. Verify refcnt is 1 and file is not mmapped
     * 7. Call mmap to map the file again and verify refcnt is back to 2
     *    and file is mmapped
     * 8. Close the file and the mmap should hold the file open still
     * 9. Verify refcnt is 1 due to the existing mmap
     * 10. Call msync to verify mmap is still valid
     * 11. Call munmap and verify refcnt goes to 0 and file is not mmapped
     *
     * Total variants tested: 5
     */
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test for correct mmap/mnomap behavior",
                               "open,close,mmap,mumap",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testMmap");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    for (i1 = 0; i1 < 6; i1++) {
        printf("----------\n");
        switch(i1) {
            case 0:
                printf("Parent opening file with O_RDONLY \n");
                oflag1 = O_NONBLOCK | O_RDONLY;
                //expect_read1 = 1;
                //expect_write1 = 0;
                protection = PROT_READ;
                break;
            case 1:
                printf("Parent opening file with O_RDWR \n");
                oflag1 = O_NONBLOCK | O_RDWR;
                //expect_read1 = 1;
                //expect_write1 = 1;
                protection = PROT_READ | PROT_WRITE;
               break;
            case 2:
                printf("Parent opening file with O_SHLOCK | O_RDONLY \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDONLY;
                //expect_read1 = 1;
                //expect_write1 = 0;
                protection = PROT_READ;
                break;
            case 3:
                printf("Parent opening file with O_SHLOCK | O_RDWR \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDWR;
                //expect_read1 = 1;
                //expect_write1 = 1;
                protection = PROT_READ | PROT_WRITE;
                break;
            case 4:
                printf("Parent opening file with O_EXLOCK | O_RDONLY \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_RDONLY;
                //expect_read1 = 1;
                //expect_write1 = 0;
                protection = PROT_READ;
                break;
            case 5:
                printf("Parent opening file with O_EXLOCK | O_RDWR \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_RDWR;
                //expect_read1 = 1;
                //expect_write1 = 1;
                protection = PROT_READ | PROT_WRITE;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

#if 0
        printf("MMapping first %d bytes \n", first_len);
        ret_address = mmap(NULL, first_len, protection, flags, fd1, 0);
        if (ret_address == MAP_FAILED) {
            error = errno;
            XCTFail("mmap on <%s> failed %d:%s \n", file_path,
                    error, strerror(error));
            goto done;
        }
#endif
        
        printf("MMapping second %d bytes \n", second_len);
        ret_address = mmap(NULL, second_len, protection, flags, fd1, 0);
        if (ret_address == MAP_FAILED) {
            error = errno;
            XCTFail("mmap on <%s> failed %d:%s \n", file_path,
                    error, strerror(error));
            goto done;
        }

        printf("Verify ref count is now 2 \n");
        bzero(&pb, sizeof(pb));
        error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
        if (error != 0) {
            XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
            goto done;
        }

        if (oflag1 & (O_SHLOCK | O_EXLOCK)) {
            /* Verify lockFID refcnt */
            if (pb.file.lockFID_refcnt != 2) {
                printf("Failed to verify lockFID refcnt 2 != <%d> \n",
                       pb.file.lockFID_refcnt);
                //goto done;
            }
        }
        else {
            /* Verify sharedFID refcnt */
            if (pb.file.sharedFID_refcnt != 2) {
                printf("Failed to verify sharedFID refcnt 2 != <%d> \n",
                       pb.file.sharedFID_refcnt);
                //goto done;
            }
        }
        
        printf("Verify file is mmapped \n");
        if (!(pb.file.flags & SMB_FILE_MMAPPED)) {
            XCTFail("Failed to verify file is mmapped, flags <0x%llx> \n",
                    pb.file.flags);
            goto done;
        }

        printf("Calling msync to invalidate %d bytes \n", second_len);
        error = msync(ret_address, second_len, MS_INVALIDATE);
        if (error != 0) {
            XCTFail("msync failed %d (%s)\n\n", error, strerror (error));
            goto done;
        }

        printf("Calling munmap for %d bytes \n", second_len);
        error = munmap(ret_address, second_len);
        if (error != 0) {
            XCTFail("munmap failed %d (%s)\n\n", error, strerror (error));
            goto done;
        }

        /* munmap does not result in ref count dropping right away */

        printf("Verify file is not mmapped \n");
        bzero(&pb, sizeof(pb));
        error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
        if (error != 0) {
            XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
            goto done;
        }

        if (pb.file.flags & SMB_FILE_MMAPPED) {
            XCTFail("Failed to verify file is not mmapped, flags <0x%llx> \n",
                    pb.file.flags);
            goto done;
        }

        printf("MMapping %d bytes again \n", second_len);
        ret_address = mmap(NULL, second_len, protection, flags, fd1, 0);
        if (ret_address == MAP_FAILED) {
            error = errno;
            XCTFail("mmap on <%s> failed %d:%s \n", file_path,
                    error, strerror(error));
            goto done;
        }

        printf("Verify ref count is now 2 again \n");
        bzero(&pb, sizeof(pb));
        error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
        if (error != 0) {
            XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
            goto done;
        }

        if (oflag1 & (O_SHLOCK | O_EXLOCK)) {
            /* Verify lockFID refcnt */
            if (pb.file.lockFID_refcnt != 2) {
                printf("Failed to verify lockFID refcnt 2 != <%d> \n",
                        pb.file.lockFID_refcnt);
                //goto done;
            }
        }
        else {
            /* Verify sharedFID refcnt */
            if (pb.file.sharedFID_refcnt != 2) {
                printf("Failed to verify sharedFID refcnt 2 != <%d> \n",
                        pb.file.sharedFID_refcnt);
                //goto done;
            }
        }

        printf("Verify file is mmapped \n");
        if (!(pb.file.flags & SMB_FILE_MMAPPED)) {
            XCTFail("Failed to verify file is mmapped, flags <0x%llx> \n",
                    pb.file.flags);
            goto done;
        }

        /* Close file */
        printf("Closing file \n");
        if (fd1 != -1) {
            error = close(fd1);
            if (error) {
                XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
                //error = errno;
                goto done;
            }
            else {
                fd1 = -1;
            }
        }

        printf("Verify ref count is now 1 \n");
        bzero(&pb, sizeof(pb));
        error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
        if (error != 0) {
            XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
            goto done;
        }

        if (oflag1 & (O_SHLOCK | O_EXLOCK)) {
            /* Verify lockFID refcnt */
            if (pb.file.lockFID_refcnt != 1) {
                printf("Failed to verify lockFID refcnt 2 != <%d> \n",
                        pb.file.lockFID_refcnt);
                //goto done;
            }
        }
        else {
            /* Verify sharedFID refcnt */
            if (pb.file.sharedFID_refcnt != 1) {
                printf("Failed to verify sharedFID refcnt 2 != <%d> \n",
                        pb.file.sharedFID_refcnt);
                //goto done;
            }
        }

        printf("Verify file is still mmapped \n");
        if (!(pb.file.flags & SMB_FILE_MMAPPED)) {
            XCTFail("Failed to verify file is mmapped, flags <0x%llx> \n",
                    pb.file.flags);
            goto done;
        }

        printf("Calling msync to invalidate %d bytes \n", second_len);
        error = msync(ret_address, second_len, MS_INVALIDATE);
        if (error != 0) {
            XCTFail("msync failed %d (%s)\n\n", error, strerror (error));
            goto done;
        }

        printf("Calling munmap for %d bytes \n", second_len);
        error = munmap(ret_address, second_len);
        if (error != 0) {
            XCTFail("munmap failed %d (%s)\n\n", error, strerror (error));
            goto done;
        }

        /* munmap does not result in ref count dropping right away */

        printf("Verify file is not mmapped \n");
        bzero(&pb, sizeof(pb));
        error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
        if (error != 0) {
            XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
            goto done;
        }

        if (pb.file.flags & SMB_FILE_MMAPPED) {
            XCTFail("Failed to verify file is not mmapped, flags <0x%llx> \n",
                    pb.file.flags);
            goto done;
        }

    } /* i1 loop */
        
    
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testFilesLeftOpenUnmount
{
    int error = 0;
    char file_path[PATH_MAX];
    int fd1 = -1, fd2 = -1;
    char mp1[PATH_MAX];

    /*
     * Test for internal code flow, so have to either have debug printing
     * added or dtrace running.
     *
     * When unmount(MNT_FORCE) is called and there are still open files, then
     * vnop_close() gets called only once (even if file is opened multiple
     * times), but the open mode is just IO_NDELAY (ie not copied from
     * vnop_open() modes), we are expected to close everything for that file.
     * Can only tell if we handle this correctly by watching internal code
     * flow.
     *
     * Total variants tested: 1
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test for internal code flow, so have to either have debug printing added or dtrace running",
                               "open,unmount",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testFilesLeftOpenUnmount");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    /*
     * Open the file on mp1
     */
    fd1 = open(file_path, O_RDONLY);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }

    fd2 = open(file_path, O_RDWR);
    if (fd2 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }
    
done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testOpenFileQueryInfo
{
    int error = 0;
    char file_path1[PATH_MAX] = {0};
    int fd1 = -1;
    char mp1[PATH_MAX];
    struct stat stat_buffer = {0};

    /*
     * testOpenFileQueryInfo - Verify that a QueryInfo was done instead of a
     *      Create/QueryInfo/Close was done on an open file.
     *
     * 1.   Create and open testfile on mount 1
     * 2.   Wait 10 seconds for meta data cache to expire
     * 3.   Do a fstat to hopefully trigger a QueryInfo
     * 4.   Have to check packet trace to verify this worked or not
     */
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Verify that a QueryInfo was done instead of a Create/QueryInfo/Close was done on an open file.",
                               "open,close,query_info",
                               "2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testOpenFileQueryInfo");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path1, sizeof(file_path1));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    /*
     * Open the file for the writer on mp1
     */
    printf("Open testfile and leave it open \n");
    fd1 = open(file_path1, O_RDWR);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path1,
                errno, strerror(errno));
        goto done;
    }

    /* Wait for meta data cache to expire */
    printf("Wait for meta data cache to expire \n");
    sleep(10);
    
    /*
     * This should trigger a QueryInfo on the wire.
     * Have to check packet trace to see if Compound Create/QueryInfo/Close (incorrect)
     * or just QueryInfo (correct) was done.
     */
    printf("Trigger a QueryInfo, hopefully \n");
    error = fstat(fd1, &stat_buffer);
    if (error) {
        XCTFail("fstat on <%s> failed %d:%s \n", file_path1,
                errno, strerror(errno));
        goto done;
    }

    /* Close file on mp1 */
    printf("Close testfile \n");
    error = close(fd1);
    if (error) {
        XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    else {
        fd1 = -1;
    }

    /* Do the Delete on test file */
    error = remove(file_path1);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path1, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (fd1 != -1) {
        /* Close file on mp1 */
        error = close(fd1);
        if (error) {
            XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testCreateResourceFork
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char rsrc_file_path[PATH_MAX] = {0};
    int fd = -1;
    int oflag = O_EXCL | O_CREAT | O_SHLOCK | O_NONBLOCK | O_RDWR;
    int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH; /* -rw-rw-rw- */
    int i;
    char mp1[PATH_MAX] = {0};

    /*
     * Note: fails against smbx, radar 90000390.  Also fails against Samba.
     */
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test creating resource fork on a file.  Needs manual check of packet traces to verify got leases.",
                               "open,close,read,write,O_EXLOCK,O_SHLOCK,resource_fork",
                               "1,2,3",
                               "90000390",
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testResourceFork");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }
    
    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    /* Create the resource fork path */
    strlcpy(rsrc_file_path, file_path, sizeof(rsrc_file_path));
    strlcat(rsrc_file_path, _PATH_RSRCFORKSPEC, sizeof(rsrc_file_path));

    for (i = 0; i < 3; i++) {
        switch(i) {
            case 0:
                printf("Test with plain open \n");
                oflag = O_EXCL | O_CREAT | O_NONBLOCK | O_RDWR;
                break;
            case 1:
                printf("Test with O_SHLOCK \n");
                oflag = O_EXCL | O_CREAT | O_NONBLOCK | O_RDWR | O_SHLOCK;
                break;
            case 2:
                printf("Test with O_EXLOCK \n");
                oflag = O_EXCL | O_CREAT | O_NONBLOCK | O_RDWR | O_EXLOCK;
                break;
        }

        /*
         * Open the resource fork file on mp1
         */
        fd = open(rsrc_file_path, oflag, mode);
        if (fd == -1) {
            XCTFail("open on <%s> failed %d:%s \n", rsrc_file_path,
                    errno, strerror(errno));
            goto done;
        }

        printf("Verifying write access \n");
        error = write_and_verify(fd, data1, sizeof(data1), 0);
        if (error) {
            XCTFail("write_and_verify on <%s> failed %d:%s \n", rsrc_file_path,
                    error, strerror(error));
            goto done;
        }

        printf("Verifying read access \n");
        error = read_and_verify(fd, data1, sizeof(data1), 0);
        if (error) {
            XCTFail("read_and_verify on <%s> failed %d:%s \n", rsrc_file_path,
                    error, strerror(error));
            goto done;
        }

        /* Close resource fork test file */
        error = close(fd);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            fd = -1;
        }

        /* Do the Delete on resource fork test file */
        error = remove(rsrc_file_path);
        if (error) {
            fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                    rsrc_file_path, strerror(errno), errno);
            goto done;
        }
    }

    /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (fd != -1) {
        /* Close file on mp1 */
        error = close(fd);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testReopenResourceFork
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char rsrc_file_path[PATH_MAX] = {0};
    int fd = -1;
    int oflag = O_EXCL | O_CREAT | O_SHLOCK | O_NONBLOCK | O_RDWR;
    int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH; /* -rw-rw-rw- */
    int i;
    char mp1[PATH_MAX] = {0};

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test opening an existing resource fork on a file.  Needs manual check of packet traces to verify got leases.",
                               "open,close,read,write,O_EXLOCK,O_SHLOCK,resource_fork",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testResourceFork");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }
    
    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    /* Create the resource fork path */
    strlcpy(rsrc_file_path, file_path, sizeof(rsrc_file_path));
    strlcat(rsrc_file_path, _PATH_RSRCFORKSPEC, sizeof(rsrc_file_path));

    /* Create the resource fork */
    printf("Creating resource fork \n");
    fd = open(rsrc_file_path, O_EXCL | O_CREAT | O_NONBLOCK | O_RDWR, mode);
    if (fd == -1) {
        XCTFail("open on <%s> failed %d:%s \n", rsrc_file_path,
                errno, strerror(errno));
        goto done;
    }

    printf("Write initial data \n");
    error = write_and_verify(fd, data1, sizeof(data1), 1);
    if (error) {
        XCTFail("write_and_verify on <%s> failed %d:%s \n", rsrc_file_path,
                error, strerror(error));
        goto done;
    }

    /* Close the resource fork */
    error = close(fd);
    if (error) {
        XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    else {
        fd = -1;
    }
    
    for (i = 0; i < 3; i++) {
        switch(i) {
            case 0:
                printf("Test with plain open \n");
                oflag = O_EXCL | O_NONBLOCK | O_RDWR;
                break;
            case 1:
                printf("Test with O_SHLOCK \n");
                oflag = O_EXCL | O_NONBLOCK | O_RDWR | O_SHLOCK;
                break;
            case 2:
                printf("Test with O_EXLOCK \n");
                oflag = O_EXCL | O_NONBLOCK | O_RDWR | O_EXLOCK;
                break;
        }

        /*
         * Open the resource fork file on mp1
         */
        fd = open(rsrc_file_path, oflag, mode);
        if (fd == -1) {
            XCTFail("open on <%s> failed %d:%s \n", rsrc_file_path,
                    errno, strerror(errno));
            goto done;
        }

        printf("Verifying write access \n");
        error = write_and_verify(fd, data1, sizeof(data1), 0);
        if (error) {
            XCTFail("write_and_verify on <%s> failed %d:%s \n", rsrc_file_path,
                    error, strerror(error));
            goto done;
        }

        printf("Verifying read access \n");
        error = read_and_verify(fd, data1, sizeof(data1), 0);
        if (error) {
            XCTFail("read_and_verify on <%s> failed %d:%s \n", rsrc_file_path,
                    error, strerror(error));
            goto done;
        }

        /* Close resource fork test file */
        error = close(fd);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            fd = -1;
        }
    }

    /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (fd != -1) {
        /* Close file on mp1 */
        error = close(fd);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

- (void)testNoDefCloseResourceFork
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char rsrc_file_path[PATH_MAX] = {0};
    int fd1 = -1;
    int i1 = 0, j = 0;
    int oflag1 = 0;
    char mp1[PATH_MAX] = {0};
    int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH; /* -rw-rw-rw- */

    /*
     * Test that a deferred close are NOT done for a resource fork
     *
     * 1. Open resource fork with all 9 variants of none/O_SHLOCK/O_EXLOCK and
     *    O_RDONLY, O_WRONLY, O_RDWR
     * 2. Open/Close the resource fork 3 times.
     * 3. fsctl() does not work with a named stream path so have to verify
     *    manually with a packet trace. Should see 3 open/closes on wire.
     *    Need to verify that a lease was granted on each open.
     *
     * Total variants tested: 9
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that resource forks never get a deferred close. Needs manual check of packet traces to verify deferred close did not occur (IE 3 open/close requests/replies happened for each type of open.",
                               "open,close,read,write,O_EXLOCK,O_SHLOCK,deferred_close,resource_fork",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testNoDefCloseResourceFork");
    
    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    /* Create the resource fork path */
    strlcpy(rsrc_file_path, file_path, sizeof(rsrc_file_path));
    strlcat(rsrc_file_path, _PATH_RSRCFORKSPEC, sizeof(rsrc_file_path));

    /* Create the resource fork */
    printf("Creating resource fork \n");
    fd1 = open(rsrc_file_path, O_EXCL | O_CREAT | O_NONBLOCK | O_RDWR, mode);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", rsrc_file_path,
                errno, strerror(errno));
        goto done;
    }

    printf("Write initial data \n");
    error = write_and_verify(fd1, data1, sizeof(data1), 1);
    if (error) {
        XCTFail("write_and_verify on <%s> failed %d:%s \n", rsrc_file_path,
                error, strerror(error));
        goto done;
    }

    /* Close the resource fork */
    error = close(fd1);
    if (error) {
        XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    else {
        fd1 = -1;
    }

    for (i1 = 0; i1 < 9; i1++) {
        switch(i1) {
            case 0:
                printf("Parent opening file with O_RDONLY \n");
                oflag1 = O_NONBLOCK | O_RDONLY;
                break;
            case 1:
                printf("Parent opening file with O_WRONLY \n");
                oflag1 = O_NONBLOCK | O_WRONLY;
                break;
            case 2:
                printf("Parent opening file with O_RDWR \n");
                oflag1 = O_NONBLOCK | O_RDWR;
                break;
            case 3:
                printf("Parent opening file with O_SHLOCK | O_RDONLY \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDONLY;
                break;
            case 4:
                printf("Parent opening file with O_SHLOCK | O_WRONLY \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_WRONLY;
                break;
            case 5:
                printf("Parent opening file with O_SHLOCK | O_RDWR \n");
                oflag1 = O_SHLOCK | O_NONBLOCK | O_RDWR;
                break;
            case 6:
                printf("Parent opening file with O_EXLOCK | O_RDONLY \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_RDONLY;
                break;
            case 7:
                printf("Parent opening file with O_EXLOCK | O_WRONLY \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_WRONLY;
                break;
            case 8:
                printf("Parent opening file with O_EXLOCK | O_RDWR \n");
                oflag1 = O_EXLOCK | O_NONBLOCK | O_RDWR;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * fsctl() does not work for named streams, so will need to inspect
         * a packet trace to verify open got a lease and that three open/close
         * happened.
         */
        for (j = 0; j < 3; j++) {
            /* Open/Close three times */
            printf("Opening resource fork with oflag <0x%x>. Iteration %d \n",
                   oflag1, j);
            fd1 = open(rsrc_file_path, oflag1);
            if (fd1 == -1) {
                XCTFail("open on <%s> failed %d:%s \n", rsrc_file_path,
                        errno, strerror(errno));
                goto done;
            }

            /* Close resource fork on file on mp1 */
            printf("Closing resource fork with oflag <0x%x>. Iteration %d \n",
                   oflag1, j);
            error = close(fd1);
            if (error) {
                XCTFail("close on fd1 failed %d:%s \n",
                        error, strerror(error));
                goto done;
            }
            else {
                fd1 = -1;
            }
        }
    } /* i1 loop */

    /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (fd1 != -1) {
        /* Close file on mp1 */
        error = close(fd1);
        if (error) {
            XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

- (void)testUpdateLeaseResourceFork
{
    int error = 0;
    char file_path1[PATH_MAX];
    char rsrc_file_path1[PATH_MAX] = {0};
    char file_path2[PATH_MAX];
    char rsrc_file_path2[PATH_MAX] = {0};
    int fd1 = -1, fd2 = -1;
    int mode1 = O_RDWR | O_CREAT;
    int mode2 = O_RDWR;
    char mp1[PATH_MAX];
    char mp2[PATH_MAX];
    //struct smbStatPB pb = {0};
    //struct smb_update_lease pb2 = {0};

    /*
     * Test lease behavior on resource fork including updating a lease
     *
     * NOTE: fsctl() is not allowed on the resource fork, so this test has to
     * be slightly different to work around that limitation
     *
     * 1. Open file resource fork with O_RDWR on mp1
     * 2. Verify got RWH lease on mp1
     * 3. Open the file resource fork on mp2 which will break the W lease for mp1
     * 4. Write/read data on mp2 which will break the RH lease for mp1
     * 5. Verify lease is completely broken on mp1
     * 6. Close file on mp2 which closes completely since its a resource fork
     * 7. Do fsctl(smbfsUpdateLeaseFSCTL) to force lease update attempt on mp1
     * 9. Verify have RWH lease again on mp1
     *
     * Total variants tested: 1
     *
     * Note: fails against smbx which does not give dur handle or lease to user2, radar 84680950
     * Note: fails against smbx which does not grant upgraded lease, radar 84681694
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test lease behavior on resource fork including updating a lease. Needs manual check of packet traces for expected lease behavior.",
                               "open,read,write,close,deferred_close,lease,lease_break,resource_fork",
                               "1,2,3",
                               "84680950,84681694",
                               NULL);
        return;
    }

    /*
     * We will need two mounts to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testUpdateLeaseResourceFork1");
    do_create_mount_path(mp2, sizeof(mp2), "testUpdateLeaseResourceFork2");
    
    error = mount_two_sessions(mp1, mp2, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Set up file paths on both mounts and create test file */
    error = setup_file_paths(mp1, mp2, default_test_filename,
                             file_path1, sizeof(file_path1),
                             file_path2, sizeof(file_path2));
    if (error) {
        XCTFail("setup_file_paths failed %d \n", error);
        goto done;
    }

    /* Create the resource fork path */
    strlcpy(rsrc_file_path1, file_path1, sizeof(rsrc_file_path1));
    strlcat(rsrc_file_path1, _PATH_RSRCFORKSPEC, sizeof(rsrc_file_path1));

    strlcpy(rsrc_file_path2, file_path2, sizeof(rsrc_file_path2));
    strlcat(rsrc_file_path2, _PATH_RSRCFORKSPEC, sizeof(rsrc_file_path2));

    /*
     * Open the file on mp1
     */
    printf("Opening file resource fork on mp1 with O_RDWR \n");
    fd1 = open(rsrc_file_path1, mode1, S_IRWXU | S_IRWXG | S_IRWXO);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", rsrc_file_path1,
                errno, strerror(errno));
        goto done;
    }

    printf("Write initial data on mp1 \n");
    error = write_and_verify(fd1, data1, sizeof(data1), 1);
    if (error) {
        XCTFail("write_and_verify on <%s> failed %d:%s \n", rsrc_file_path1,
                error, strerror(error));
        goto done;
    }

    /*
     * Call fsync on mp1 which should write the data out to the server
     * Need to do this since Samba servers will report ENOENT on a resource
     * fork that is 0 in size.
     */
    printf("Flush initial data on mp1 \n");
    error = fsync(fd1);
    if (error) {
        XCTFail("fsync on fd1 failed %d:%s \n", error, strerror(error));
        goto done;
    }

#if 0
    printf("Verify got RWH lease on mp1 \n");
    bzero(&pb, sizeof(pb));
    error = fsctl(rsrc_file_path1, smbfsStatFSCTL, &pb, 0);
    if (error != 0) {
        XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
        goto done;
    }

    if (!(pb.lease_flags & SMB2_LEASE_GRANTED)) {
        XCTFail("Failed to get lease. lease_flags 0x%llx \n",
                pb.lease_flags);
        goto done;
    }

    /*
     * Servers that support NFS ace will have a Create/SetInfo/Close done
     * to set the unix mode bits, but that will cause the lease to break down
     * to 0x03.
     */
    if ((pb.lease_curr_state != 0x7) && (pb.lease_curr_state != 0x3)) {
        XCTFail("Failed to get lease state. lease_curr_state 0x%x != 0x07 or 0x03 \n",
                pb.lease_curr_state);
        goto done;
    }
#endif
    
    /*
     * Switch to second connection, mp2 and open the file which should cause
     * a lease break to mp1 to downgrade its current lease
     */
    printf("Opening file resource fork on mp2 with O_RDWR\n");
    fd2 = open(rsrc_file_path2, mode2);
    if (fd2 == -1) {
        XCTFail("open on <%s> failed %d:%s \n",
                rsrc_file_path2, errno, strerror(errno));
        goto done;
    }

#if 0
    printf("Verify got RWH lease on mp2 \n");
    bzero(&pb, sizeof(pb));
    error = fsctl(rsrc_file_path2, smbfsStatFSCTL, &pb, 0);
    if (error != 0) {
        XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
        goto done;
    }

    /*
     * smbx has odd bug of 84680950 where the second user does not get granted
     * a durable handle or lease. Work around it for now.
     */
    if (!(pb.lease_flags & SMB2_LEASE_GRANTED)) {
        printf("Failed to get lease. lease_flags 0x%llx. Assume this is a non fixed smbx server? \n",
                pb.lease_flags);
        
        /* turn data caching off */
        printf("Disabling UBC caching to work around smbx bug \n");
        if (fcntl(fd2, F_NOCACHE, 1) != 0) {
            XCTFail( "F_NOCACHE failed %d:%s \n",
                    errno, strerror(errno));
            goto done;
        }
    }
#endif
    
    /* Write/Read data on mp2 to completely break the lease on mp1 */
    printf("Write data on mp2 \n");
    error = write_and_verify(fd2, data1, sizeof(data1), 0);
    if (error) {
        XCTFail("write_and_verify on <%s> failed %d:%s \n", file_path2,
                error, strerror(error));
        goto done;
    }
    
#if 0
    /* Verify file on mp1 go its lease broken */
    printf("Verify lease is broken on mp1 \n");
    bzero(&pb, sizeof(pb));
    error = fsctl(rsrc_file_path1, smbfsStatFSCTL, &pb, 0);
    if (error != 0) {
        XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
        goto done;
    }

    if (pb.lease_flags & SMB2_LEASE_GRANTED) {
        XCTFail("Failed to lose lease. lease_flags 0x%llx \n",
                pb.lease_flags);
        goto done;
    }

    if (pb.lease_curr_state != 0x00) {
        XCTFail("Failed to get lease state. lease_curr_state 0x%x != 0x00 \n",
                pb.lease_curr_state);
        goto done;
    }
#endif

    /* Close file on mp2 */
    printf("Closing file on mp2 \n");
    error = close(fd2);
    if (error) {
        XCTFail("close on fd2 failed %d:%s \n",
                error, strerror(error));
        goto done;
    }
    else {
        fd2 = -1;
    }
    
#if 0
    /* Force file on mp1 to try to update its lease now */
    printf("Force lease update on mp1 \n");
    bzero(&pb2, sizeof(pb2));
    error = fsctl(rsrc_file_path1, smbfsUpdateLeaseFSCTL, &pb2, 0);
    if (error != 0) {
        XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
        goto done;
    }

    /* Verify we got full lease back on mp1 */
    printf("Verify got RWH lease again on mp1 \n");
    bzero(&pb, sizeof(pb));
    error = fsctl(rsrc_file_path1, smbfsStatFSCTL, &pb, 0);
    if (error != 0) {
        XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
        goto done;
    }

    if (!(pb.lease_flags & SMB2_LEASE_GRANTED)) {
        XCTFail("Failed to get lease. lease_flags 0x%llx \n",
                pb.lease_flags);
        goto done;
    }

    if (pb.lease_curr_state != 0x7) {
        XCTFail("Failed to get lease state. lease_curr_state 0x%x != 0x07 \n",
                pb.lease_curr_state);
        goto done;
    }
#else
    /*
     * Have to wait because cant force lease upgrade because fcntl() is not
     * allowed on resource forks
     */
    
    printf("Sleeping for 60 seconds in hopes of a lease update occurring every 30 seconds \n");
    sleep(60);
#endif
    
    /* Close file on mp1 */
    printf("Closing file on mp1 \n");
    error = close(fd1);
    if (error) {
        XCTFail("close on fd1 failed %d:%s \n",
                error, strerror(error));
        goto done;
    }
    else {
        fd1 = -1;
    }

    /* Do the Delete on test file */
    error = remove(rsrc_file_path1);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path1, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (fd1 != -1) {
        /* Close file on mp1 */
        error = close(fd1);
        if (error) {
            XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (fd2 != -1) {
        /* Close file on mp2 */
        error = close(fd2);
        if (error) {
            XCTFail("close on fd2 failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    if (unmount(mp2, MNT_FORCE) == -1) {
        XCTFail("unmount failed for second url %d\n", errno);
    }

    rmdir(mp1);
    rmdir(mp2);
}

int check_all_dst_lengths(char *test_name,
                          int fd,
                          struct smbioc_path_convert *path_convert,
                          size_t dest_alloc_size,
                          size_t expected_buff_len) {
    int error = 0;
    path_convert->ioc_dest_len = dest_alloc_size;

    /* Send good data to convert */
    memset((void *)path_convert->ioc_dest, 'W', dest_alloc_size);
    error = ioctl(fd, SMBIOC_CONVERT_PATH, path_convert);
    if (error) {
        printf("failed to send ioctl (%s) error: %d errno: %d\n", test_name, error, errno);
      goto done;
    }
    printf("Conversion (%s) requires %llu bytes\n", test_name, path_convert->ioc_dest_len);

    if (path_convert->ioc_dest_len != expected_buff_len) {
        printf("failed convert (%s) in %lu bytes (actual %llu)\n", test_name, expected_buff_len, path_convert->ioc_dest_len);
        error = -1;
        goto done;
    }

    /* Send bad data to convert (dst buffer too small) */
    for (size_t dst_buff_size = 0; dst_buff_size<expected_buff_len; dst_buff_size++) {
        path_convert->ioc_dest_len = dst_buff_size;
        memset((void *)path_convert->ioc_dest, 'W', dest_alloc_size);
        error = ioctl(fd, SMBIOC_CONVERT_PATH, path_convert);
        if (!error) {
            printf("failed. Conversion (%s) succeeded although dst_buf is too small: %d errno: %d, buf_size %lu\n", test_name, error, errno, dst_buff_size);
            error = -1;
            goto done;
        }
    }

    /* Send good data to convert (dst buffer large enough) */
    for (size_t dst_buff_size = expected_buff_len; dst_buff_size <= dest_alloc_size; dst_buff_size++) {
        path_convert->ioc_dest_len = dst_buff_size;
        memset((void *)path_convert->ioc_dest, 'W', dest_alloc_size);
        error = ioctl(fd, SMBIOC_CONVERT_PATH, path_convert);
        if (error) {
            printf("failed. Conversion succeeded although dst_buf is large enough. error: %d errno: %d, buf_size %lu\n", error, errno, dst_buff_size);
            goto done;
        }
    }
done:
    printf("test (%s) returned error %d\n", test_name, error);
    return error;
}

- (void)testPathConvOverflow
{
    int error = 0;
    int fd    = 0;
    char *corrupted_path = NULL;
    char mp[PATH_MAX];

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test SMBIOC_CONVERT_PATH with various inputs",
                               "security,ioctl,SMBIOC_CONVERT_PATH",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    do_create_mount_path(mp, sizeof(mp), "testSecPathConvOverflow");

    struct smb_server_handle *mpConnection = NULL;
    SMBHANDLE shareConnection = NULL;
    uint32_t status = 0;

    /* First mount a volume */
    if ((mkdir(mp, S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST)) {
        error = errno;
        XCTFail("mkdir failed %d for <%s>\n",
                error, g_test_url1);
        goto done;
    }

    status = SMBOpenServerEx(g_test_url1, &mpConnection,
                             kSMBOptionNoPrompt | kSMBOptionSessionOnly);
    if (!NT_SUCCESS(status)) {
        XCTFail("SMBOpenServerEx failed 0x%x for <%s>\n",
                status, g_test_url1);
        goto done;
    }

    status = SMBMountShareEx(mpConnection, NULL, mp, 0, 0, 0, 0, NULL, NULL);
    if (!NT_SUCCESS(status)) {
        XCTFail("SMBMountShareEx failed 0x%x for <%s>\n",
                status, g_test_url1);
        goto done;
    }

    /* Now that we have a mounted volume we run the real test */
    status = SMBOpenServerWithMountPoint(mp, "IPC$",
                                         &shareConnection, 0);
    if (!NT_SUCCESS(status)) {
        XCTFail("SMBMountShareEx failed 0x%x for <%s>\n",
                status, g_test_url1);
        goto done;
    }

    struct smb_server_handle_local { // must match struct smb_server_handle
        volatile int32_t refcount;
        struct smb_ctx * context;
    } *pServerHandle;

    pServerHandle = (struct smb_server_handle_local*)mpConnection;
    fd = pServerHandle->context->ct_fd;

    /* Prepare ictl data */
    uint32_t target_size = 32;
    uint32_t dest_alloc_size  = target_size*4;

    corrupted_path = (char *)malloc(target_size);
    if (!corrupted_path) {
        XCTFail("malloc error");
        goto done;
    }
    memset(corrupted_path, 'A', target_size); // purposely no '\0'
    corrupted_path[target_size / 2] = '/';
    corrupted_path[target_size - 1] = '\0';
    size_t expected_buff_len = (target_size-1)*2;

    struct smbioc_path_convert path_convert;
    bzero((void *)&path_convert, sizeof(path_convert));
    path_convert.ioc_version = 170;
    path_convert.ioc_direction = 2;
    path_convert.ioc_src_len = target_size;
    path_convert.ioc_src = corrupted_path;
    path_convert.ioc_dest_len = dest_alloc_size;
    path_convert.ioc_dest = (char *)malloc(dest_alloc_size);
    if (!path_convert.ioc_dest) {
        XCTFail("malloc error");
        goto done;
    }

    /* Send good data to convert */
    char *test_name = "null terminated";
    error = check_all_dst_lengths(test_name,
                                  fd,
                                  &path_convert,
                                  dest_alloc_size,
                                  expected_buff_len);
    if (error) {
        XCTFail("failed to run test (%s) with error: %d errno: %d\n", test_name, error, errno);
        goto done;
    }

    // Not null terminated - should always fail
    corrupted_path[target_size - 1] = 'A';
    expected_buff_len = target_size*2;

    test_name = "not null terminated";
    error = check_all_dst_lengths(test_name,
                                  fd,
                                  &path_convert,
                                  dest_alloc_size,
                                  expected_buff_len);
    if (error) {
        XCTFail("failed to run test (%s) with error: %d errno: %d\n", test_name, error, errno);
        goto done;
    }

    /* Send standard path to convert */
    char *std_path = "//192.168.0.51/smb_share_path/xxx";
    size_t std_path_length = strlen(std_path);
    expected_buff_len = std_path_length*2;
    path_convert.ioc_src_len = (uint32_t)std_path_length;
    path_convert.ioc_src = std_path;

    test_name = "standard path";
    error = check_all_dst_lengths(test_name,
                                  fd,
                                  &path_convert,
                                  dest_alloc_size,
                                  expected_buff_len);
    if (error) {
        XCTFail("failed to run test (%s) with error: %d errno: %d\n", test_name, error, errno);
        goto done;
    }

done:
    /* Now cleanup everything */
    if (shareConnection) {
        SMBReleaseServer(shareConnection);
    }

    if (mpConnection) {
        SMBReleaseServer(mpConnection);
    }
    
    if (corrupted_path) {
        free(corrupted_path);
    }

    if (unmount(mp, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }
    rmdir(mp);
}
size_t parse_bulk_results(int num_of_results, char *result_buf, size_t result_buf_size) {

    if ((!result_buf) || (!result_buf_size))
        return EINVAL;

    printf(" -> entries returned");
    size_t total_length = 0;
    char *entry_start = result_buf;
    for (unsigned index = 0; index < num_of_results; index++) {
        struct val_attrs {
            uint32_t          length;
            attribute_set_t   returned;
            uint32_t          error;
            attrreference_t   name_info;
            char              *name;
            fsobj_type_t      obj_type;
        } attrs = {0};

        printf("\n Entry %4d", index);
        printf("  --  ");
        char *field = entry_start;
        attrs.length = *(uint32_t *)field;
        printf(" Length %4d ", attrs.length);
        total_length += attrs.length;
        field += sizeof(uint32_t);

        /* set starting point for next entry */
        entry_start += attrs.length;

        attrs.returned = *(attribute_set_t *)field;
        field += sizeof(attribute_set_t);

        if (attrs.returned.commonattr & ATTR_CMN_ERROR) {
            attrs.error = *(uint32_t *)field;
            field += sizeof(uint32_t);
        }

        if (attrs.returned.commonattr & ATTR_CMN_NAME) {
            attrs.name =  field;
            attrs.name_info = *(attrreference_t *)field;
            field += sizeof(attrreference_t);
            printf("  %-20s ", (attrs.name + attrs.name_info.attr_dataoffset));
        }

        /* Check for error for this entry */
        if (attrs.error) {
            /*
            * Print error and move on to next
            * entry
            */
            printf("Error in reading attributes for directory entry %d", attrs.error);
            continue;
        }

        printf("  --  ");
        if (attrs.returned.commonattr & ATTR_CMN_OBJTYPE) {
            attrs.obj_type = *(fsobj_type_t *)field;
            //field += sizeof(fsobj_type_t);

            switch (attrs.obj_type) {
                case VREG:
                    printf("file  ");
                    break;
                case VDIR:
                    printf("directory    ");
                    break;
                default:
                    printf("obj_type = %-2d  ", attrs.obj_type);
                    break;
            }
        }
        printf("  --  ");
    }
    printf("\n");
    return total_length;
}

int rmfile_cb(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    printf("Removing file %s...", path);
    int rv = remove(path);

    if (rv) {
        printf("error %d\n", errno);
        perror(path);
    } else {
        printf("success!\n");
    }
    return rv;
}


- (void)testCookieAllocAndFree
{
    int error = 0;
    char mp1[PATH_MAX];
    char mp2[PATH_MAX];

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that cookies are malloced and freed using GetAttrBulk",
                               "enumeration",
                               "1,2,3",
                               "87967770",
                               NULL);
        return;
    }

    /*
     * Test Procedure:
     * - Mount and Create a test-dir.
     * - Mount with a 2nd user - this will be used for clean up afterwards
     * - Create lots of files in the test-dir
     * - getattrlistbulk with a large buffer.
     * - getattrlistbulk with a small buffer, loop until all test-content is received (validate all cookies are freed).
     * - getattrlistbulk with a small buffer, get only a small portion of the resluts. Unmount (validate all cookies are freed).
     */

    /* Mount */
    do_create_mount_path(mp1, sizeof(mp1), "testCookieAllocAndFree1");
    do_create_mount_path(mp2, sizeof(mp2), "testCookieAllocAndFree2");

    error = mount_two_sessions(mp1, mp2, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    error = do_create_test_dirs(mp1);
    if (error) {
        XCTFail("do_create_test_dirs on <%s> failed %d:%s \n", mp1,
               error, strerror(error));
        goto done;
    }

    /* Fill up test-dir with test-files */
    char test_filename[PATH_MAX];
    uint32_t num_of_files = 100;
    for(unsigned u=0; u<num_of_files; u++) {
        sprintf(test_filename, "%s/%s/test_file_%u.txt", mp1, cur_test_dir, u);
        error = do_create_test_file(mp1, test_filename);
        if (error) {
            XCTFail("can't create file %s. Error %d \n", test_filename, error);
            goto done;
        }
    }

    /* enumerate the dir using getattrlistbulk, one large buffer */
    char test_dir_path[PATH_MAX];
    sprintf(test_dir_path, "%s/%s", mp1, cur_test_dir);
    int dirfd = open(test_dir_path, O_RDONLY, 0);
    if (dirfd == -1) {
        XCTFail("can't open test-dir %s. Error %d \n", test_filename, error);
        goto done;
    }

    char read_buf[64*1024] = {0};
    unsigned read_buf_size = sizeof(read_buf);
    struct attrlist attrlist = {0};
    attrlist.commonattr = ATTR_CMN_RETURNED_ATTRS |
                          ATTR_CMN_NAME |
                          ATTR_CMN_ERROR |
                          ATTR_CMN_OBJTYPE;
    attrlist.bitmapcount = ATTR_BIT_MAP_COUNT;
    unsigned int options = 0;

    int num_of_results = getattrlistbulk(dirfd, &attrlist, read_buf, read_buf_size, options);
    printf("result_of_results %d\n", num_of_results);
    if ((!num_of_results) || (num_of_results == -1)) {
        XCTFail("number_of_results is %d errno %d\n", num_of_results, errno);
        goto done;
    }

    size_t total_len = parse_bulk_results(num_of_results, read_buf, read_buf_size);
    if (!total_len) {
        XCTFail("error parsing bulk results. total_len is zero.\n");
        goto done;
    }
    printf("total result length %lu.\n", total_len);
    close(dirfd);

    /* get attributes with a smaller buffer */
    dirfd = open(test_dir_path, O_RDONLY, 0);
    if (dirfd == -1) {
        XCTFail("can't open test-dir %s. Error %d \n", test_filename, error);
        goto done;
    }
    size_t limited_buf_size = total_len/2;
    size_t total_num_of_results = 0;
    while(1) {
        bzero(read_buf, read_buf_size);
        num_of_results = getattrlistbulk(dirfd, &attrlist, read_buf, limited_buf_size, options);
        printf("result_of_results %d\n", num_of_results);
        if (!num_of_results)
            break;
        if (num_of_results == -1) {
            XCTFail("number_of_results is %d errno %d\n", num_of_results, errno);
            goto done;
        }
        total_num_of_results += num_of_results;

        total_len = parse_bulk_results(num_of_results, read_buf, read_buf_size);
        if (!total_len) {
            XCTFail("error parsing bulk results. total_len is zero.\n");
            goto done;
        }
        printf("total result length %lu.\n", total_len);
    }

    if (total_num_of_results != num_of_files) {
        XCTFail("error we should have had %u results. We have only %zu results.\n", num_of_files, total_num_of_results);
        goto done;
    }
    close(dirfd);

    /* get attributes with a smaller. unmount before completion */
    dirfd = open(test_dir_path, O_RDONLY, 0);
    if (dirfd == -1) {
        XCTFail("can't open test-dir %s. Error %d \n", test_filename, error);
        goto done;
    }
    bzero(read_buf, read_buf_size);
    num_of_results = getattrlistbulk(dirfd, &attrlist, read_buf, limited_buf_size, options);
    printf("result_of_results %d\n", num_of_results);
    if ((!num_of_results) || (num_of_results == -1)) {
        XCTFail("number_of_results is %d errno %d\n", num_of_results, errno);
        goto done;
    }
    total_len = parse_bulk_results(num_of_results, read_buf, read_buf_size);
    if (!total_len) {
        XCTFail("error parsing bulk results. total_len is zero.\n");
        goto done;
    }
    printf("total result length %lu.\n", total_len);

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

done:
    sprintf(test_dir_path, "%s/%s", mp2, cur_test_dir);
    nftw(test_dir_path, rmfile_cb, 64, FTW_DEPTH | FTW_PHYS);
    sprintf(test_dir_path, "%s/%s", mp2, root_test_dir);
    rmdir(test_dir_path);

    if (unmount(mp2, MNT_FORCE) == -1) {
        XCTFail("unmount failed for second url %d\n", errno);
    }

    rmdir(mp1);
    rmdir(mp2);
}

- (void)testNotifierIoctl
{
    int error = 0;
    int fd    = 0;
    char mp[PATH_MAX];

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test SMBIOC_UPDATE_CLIENT_INTERFACES with various inputs",
                               "security,ioctl,SMBIOC_UPDATE_CLIENT_INTERFACES",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    do_create_mount_path(mp, sizeof(mp), "testNotifierIoctl");

    struct smb_server_handle *mpConnection = NULL;
    SMBHANDLE shareConnection = NULL;
    uint32_t status = 0;

    /* First mount a volume */
    if ((mkdir(mp, S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST)) {
        error = errno;
        XCTFail("mkdir failed %d for <%s>\n",
                error, g_test_url1);
        goto done;
    }

    status = SMBOpenServerEx(g_test_url1, &mpConnection,
                             kSMBOptionNoPrompt | kSMBOptionSessionOnly);
    if (!NT_SUCCESS(status)) {
        XCTFail("SMBOpenServerEx failed 0x%x for <%s>\n",
                status, g_test_url1);
        goto done;
    }

    status = SMBMountShareEx(mpConnection, NULL, mp, 0, 0, 0, 0, NULL, NULL);
    if (!NT_SUCCESS(status)) {
        XCTFail("SMBMountShareEx failed 0x%x for <%s>\n",
                status, g_test_url1);
        goto done;
    }

    /* Now that we have a mounted volume we run the real test */
    status = SMBOpenServerWithMountPoint(mp, "IPC$",
                                         &shareConnection, 0);
    if (!NT_SUCCESS(status)) {
        XCTFail("SMBMountShareEx failed 0x%x for <%s>\n",
                status, g_test_url1);
        goto done;
    }
    struct smb_server_handle_local { // must match struct smb_server_handle
        volatile int32_t refcount;
        struct smb_ctx * context;
    } *pServerHandle;
    pServerHandle = (struct smb_server_handle_local*)mpConnection;
    fd = pServerHandle->context->ct_fd;

    // SMBIOC_UPDATE_CLIENT_INTERFACES
    uint32_t uNumOfNics = 10;
    struct smbioc_client_interface sClientInfo = {0};
    sClientInfo.interface_instance_count = uNumOfNics;
    sClientInfo.total_buffer_size = uNumOfNics * sizeof(struct network_nic_info);
    struct network_nic_info *psNicInfoArry = malloc(sClientInfo.total_buffer_size);
    sClientInfo.ioc_info_array = psNicInfoArry;
    for(uint32_t u=0; u<uNumOfNics; u++) {
        struct network_nic_info *psNicInfo = &psNicInfoArry[u];
        psNicInfo->nic_index = u;
        psNicInfo->next_offset = sizeof(struct network_nic_info);
        psNicInfo->addr.sa_len = 4;
        psNicInfo->addr.sa_data[0] = u;
        psNicInfo->addr.sa_data[1] = u+1;
        psNicInfo->addr.sa_data[2] = u+2;
        psNicInfo->addr.sa_data[3] = u+3;
    }

    // successful SClientInfo
    error = ioctl(fd, SMBIOC_UPDATE_CLIENT_INTERFACES, &sClientInfo);
    if (error) {
        XCTFail("failed to send a clean ioctl error: %d errno: %d\n", error, errno);
    }

    // test sClientInfo with buffer_size too small.
    sClientInfo.total_buffer_size--;
    error = ioctl(fd, SMBIOC_UPDATE_CLIENT_INTERFACES, &sClientInfo);
    if (!error) {
        XCTFail("failed: no error when total_buffer_size is too small. error: %d errno: %d\n", error, errno);
    } else {
        os_log_debug(OS_LOG_DEFAULT, "ioctl failed as expected (total_buffer_size is too small).\n");
    }
    sClientInfo.total_buffer_size++;

    // test psNicInfo->next_offset is outside buffer boundary
    psNicInfoArry[0].next_offset = (uint32_t)(-sizeof(struct network_nic_info));
    error = ioctl(fd, SMBIOC_UPDATE_CLIENT_INTERFACES, &sClientInfo);
    if (!error) {
        XCTFail("failed: no error when next_offset is negative. error: %d errno: %d\n", error, errno);
    } else {
        os_log_debug(OS_LOG_DEFAULT, "ioctl failed as expected (next_offset is negative).\n");
    }
    psNicInfoArry[0].next_offset = sizeof(struct network_nic_info);

    // test psNicInfo->next_offset is smaller than sizeof(network_nic_info)
    psNicInfoArry[3].next_offset = sizeof(struct network_nic_info) / 2;
    error = ioctl(fd, SMBIOC_UPDATE_CLIENT_INTERFACES, &sClientInfo);
    if (!error) {
        XCTFail("failed: no error when next_offset is smaller than sizeof(network_nic_info). error: %d errno: %d\n", error, errno);
    } else {
        os_log_debug(OS_LOG_DEFAULT, "ioctl failed as expected (next_offset is smaller than sizeof(network_nic_info)).\n");
    }
    psNicInfoArry[3].next_offset = sizeof(struct network_nic_info);

    // sClientInto that contains sNicInfo with IP addr len out of boundary
    psNicInfoArry[uNumOfNics-1].addr.sa_len = 255;
    error = ioctl(fd, SMBIOC_UPDATE_CLIENT_INTERFACES, &sClientInfo);
    if (!error) {
        XCTFail("failed: no error when sa_len leads outside the buffer. error: %d errno: %d\n", error, errno);
    } else {
        os_log_debug(OS_LOG_DEFAULT, "ioctl failed as expected (sa_len outside the buffer).\n");
    }
    psNicInfoArry[uNumOfNics-1].addr.sa_len = 4;

done:
    /* Now cleanup everything */
    if (shareConnection) {
        SMBReleaseServer(shareConnection);
    }

    if (mpConnection) {
        SMBReleaseServer(mpConnection);
    }

    if (unmount(mp, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }
    rmdir(mp);
}

- (void)testNicDoubleFree
{
    int error = 0;
    int fd    = 0;
    char mp[PATH_MAX];

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test SMBIOC_UPDATE_CLIENT_INTERFACES does not double free",
                               "security,ioctl,SMBIOC_UPDATE_CLIENT_INTERFACES",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    // Step 1: Connect to server
    do_create_mount_path(mp, sizeof(mp), "testNicDoubleFree");

    struct smb_server_handle *mpConnection = NULL;
    SMBHANDLE shareConnection = NULL;
    uint32_t status = 0;

    /* First mount a volume */
    if ((mkdir(mp, S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST)) {
        error = errno;
        XCTFail("mkdir failed %d for <%s>\n",
                error, g_test_url1);
        goto done;
    }

    status = SMBOpenServerEx(g_test_url1, &mpConnection,
                             kSMBOptionNoPrompt | kSMBOptionSessionOnly);
    if (!NT_SUCCESS(status)) {
        XCTFail("SMBOpenServerEx failed 0x%x for <%s>\n",
                status, g_test_url1);
        goto done;
    }

    status = SMBMountShareEx(mpConnection, NULL, mp, 0, 0, 0, 0, NULL, NULL);
    if (!NT_SUCCESS(status)) {
        XCTFail("SMBMountShareEx failed 0x%x for <%s>\n",
                status, g_test_url1);
        goto done;
    }

    status = SMBOpenServerWithMountPoint(mp, "IPC$",
                                         &shareConnection, 0);
    if (!NT_SUCCESS(status)) {
        XCTFail("SMBMountShareEx failed 0x%x for <%s>\n",
                status, g_test_url1);
        goto done;
    }

    struct smb_server_handle_local { // must match struct smb_server_handle
        volatile int32_t refcount;
        struct smb_ctx * context;
    } *pServerHandle;
    pServerHandle = (struct smb_server_handle_local*)mpConnection;
    fd = pServerHandle->context->ct_fd;

    // Step 2: Add a NIC to `sessionp->session_table.client_nic_info_list`
    struct network_nic_info nic_info;
    bzero(&nic_info, sizeof(nic_info));
    nic_info.nic_index = 1;
    nic_info.addr_4.sin_len = sizeof(struct sockaddr_in);
    nic_info.addr_4.sin_family = AF_INET;
    nic_info.addr_4.sin_port = htons(1234);
    nic_info.next_offset = sizeof(struct network_nic_info);
    inet_aton("127.0.0.1", &nic_info.addr_4.sin_addr);

    struct smbioc_client_interface update_req;
    bzero(&update_req,  sizeof(update_req));
    update_req.ioc_info_array = &nic_info;
    // Number of NICs in `ioc_info_array`
    update_req.interface_instance_count = 1;
    // Total size of NIC array in `ioc_info_array`
    update_req.total_buffer_size = sizeof(nic_info);

    if (ioctl(fd, SMBIOC_UPDATE_CLIENT_INTERFACES, &update_req)) {
        XCTFail("SMBIOC_UPDATE_CLIENT_INTERFACES ioctl failed, err: %s\n", strerror(errno));
        goto done;
    }

    if (update_req.ioc_errno) {
        XCTFail("SMBIOC_UPDATE_CLIENT_INTERFACES ioctl returned non zero ioc_errno: %d\n", update_req.ioc_errno);
        goto done;
    }
    // Step 3: Try to associate a socket address with length zero to NIC created in step 2.
    //       This will trigger the double free vulnerability
    struct network_nic_info bad_nic_info;
    bzero(&bad_nic_info, sizeof(bad_nic_info));
    // To trigger double free,  error must occur on an existing nic
    bad_nic_info.nic_index = nic_info.nic_index;
    // This will cause SMB_MALLOC in `smb2_mc_update_info_with_ip` to return NULL
    // `smb2_mc_update_info_with_ip` method will return ENOMEM
    bad_nic_info.addr_4.sin_len = 0;
    bad_nic_info.addr_4.sin_family = AF_INET;
    bad_nic_info.addr_4.sin_port = htons(1234);
    bad_nic_info.next_offset = sizeof(struct network_nic_info);
    // Use a different IP address so that `smb2_mc_does_ip_belong_to_interface`
    // will return FALSE
    inet_aton("127.0.0.2", &bad_nic_info.addr_4.sin_addr);

    bzero(&update_req,  sizeof(update_req));
    update_req.interface_instance_count = 1;
    update_req.ioc_info_array = &bad_nic_info;
    update_req.total_buffer_size = sizeof(bad_nic_info);

    ioctl(fd, SMBIOC_UPDATE_CLIENT_INTERFACES, &update_req);
    //if we don't panic, the test passed
done:
    /* Now cleanup everything */
    if (shareConnection) {
        SMBReleaseServer(shareConnection);
    }

    if (mpConnection) {
        SMBReleaseServer(mpConnection);
    }

    if (unmount(mp, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }
    rmdir(mp);
}

-(void)testDefCloseDeleteO_SHLOCK
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int fd1 = -1;
    int i1 = 0;
    struct smbStatPB pb = {0};

    /*
     * Test deleting a file that has a pending deferred close with O_SHLOCK
     *
     * 1. Open file with O_SHLOCK and all 3 variants of O_RDONLY/O_WRONLY/O_RDWR
     * 2. Verify got durable handle and lease
     * 3. Close the file
     * 4. Verify got deferred close
     * 5. Verify can delete the file
     *
     * Total variants tested: 3
     *
     * Note that open(O_WRONLY) will end up with Read access added because the
     * SMBClient always tries to add Read so that UBC can be used. This will
     * result in open(O_RDWR) to end up with a defer use count of 3 instead
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test deleting a file that has a pending deferred close with O_SHLOCK",
                               "open,close,delete,O_SHLOCK,deferred_close",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testDefCloseDeleteO_SHLOCK");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    for (i1 = 0; i1 < 3; i1++) {
        switch(i1) {
            case 0:
                printf("Parent opening file with O_SHLOCK | O_RDONLY \n");
                oflag1 = O_CREAT | O_SHLOCK | O_NONBLOCK | O_RDONLY;
                break;
            case 1:
                printf("Parent opening file with O_SHLOCK | O_WRONLY \n");
                oflag1 = O_CREAT | O_SHLOCK | O_NONBLOCK | O_WRONLY;
                break;
            case 2:
                printf("Parent opening file with O_SHLOCK | O_RDWR \n");
                oflag1 = O_CREAT | O_SHLOCK | O_NONBLOCK | O_RDWR;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile
         */
        fd1 = open(file_path, oflag1, S_IRWXU);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        printf("Verify got lease \n");
        error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
        if (error != 0) {
            XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
            goto done;
        }

        if (!(pb.lease_flags & SMB2_LEASE_GRANTED)) {
            XCTFail("Failed to get lease. lease_flags 0x%llx \n",
                    pb.lease_flags);
            goto done;
        }

        /* Close file */
        printf("Closing file \n");
        if (fd1 != -1) {
            error = close(fd1);
            if (error) {
                printf("close on fd1 failed %d:%s \n", errno, strerror(errno));
                //error = errno;
                goto done;
            }
            else {
                fd1 = -1;
            }
        }

        printf("Verify got deferred close \n");
        bzero(&pb, sizeof(pb));
        error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
        if (error != 0) {
            XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
            goto done;
        }

        if (!(pb.lease_flags & SMB2_DEFERRED_CLOSE)) {
            XCTFail("Failed to get deferred close. lease_flags 0x%llx \n",
                    pb.lease_flags);
            goto done;
        }

        /* Do the Delete on test file */
        printf("Deleting file \n");
        error = remove(file_path);
        if (error) {
            fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                    file_path, strerror(errno), errno);
            goto done;
        }
    } /* i1 loop */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testFlockO_SHLOCKFail
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int fd1 = -1;
    int fd2 = -1;

    /*
     * Test that SMB fd can be flock()'d but a subsequest open(O_SHLOCK)
     * will fail
     *
     * 1. Open the file with O_RDONLY
     * 2. Call flock(LOCK_EX) on the open file
     * 3. Try to open the file with (O_RDONLY | O_SHLOCK) and should get
     *    an error
     * 4. Verify read/write
     *
     * Total variants tested: 1
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that SMB fd can be flock()'d but a subsequest open(O_SHLOCK) will fail.",
                               "open,close,flock,O_SHLOCK",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testFlockO_SHLOCKFail");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    printf("Opening file with O_RDONLY \n");
    fd1 = open(file_path, O_NONBLOCK | O_RDONLY);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }

    printf("flock on file \n");
    error = flock(fd1, LOCK_EX);
    if (error) {
        XCTFail("flock on <%s> failed %d:%s \n", file_path,
                error, strerror(error));
        goto done;
    }

    /* This open attempt should fail with EBUSY */
    printf("Opening file with O_RDONLY | O_SHLOCK \n");
    fd2 = open(file_path, O_NONBLOCK | O_RDONLY | O_SHLOCK);
    if ((fd2 != -1) || (errno != EAGAIN)) {
        XCTFail("open on <%s> unexpectedly worked %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }
 
    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testFlockWithMultipleFD
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int fd1 = -1;
    int fd2 = -1;

    /*
     * Test that SMB fd1 can be flock()'d, then opened again with fd2 and
     * both fd1 and fd2 can read the file.
     *
     * This is an odd edge case. UBC has be turned off else the reads for both
     * fd's will come from UBC. Technically since fd1 has the file exclusively
     * byte range locked, any reads from fd2 should be blocked. Because flock()
     * is tracked by fd and not by pid, the read from fd2 will succeed.
     *
     * 1. Open the file with O_RDONLY with fd1
     * 2. Call flock(LOCK_EX) on the open file and disable UBC on fd1
     * 3. Open the file with O_RDONLY again with fd2
     * 4. Disable UBC on fd2
     * 5. Verify read works on both fd1 and fd2
     *
     * Total variants tested: 1
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that SMB fd1 can be flock()'d, then opened again with fd2 and both fd1 and fd2 can read the file.",
                               "open,close,read,flock",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testFlockWithMultipleFD");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    printf("Opening file with O_RDONLY on fd1 \n");
    fd1 = open(file_path, O_NONBLOCK | O_RDONLY);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }

    /* turn data caching off */
    printf("Disabling UBC caching on fd1 \n");
    if (fcntl(fd1, F_NOCACHE, 1) != 0) {
        XCTFail( "F_NOCACHE failed %d:%s \n",
                errno, strerror(errno));
        goto done;
    }

    printf("flock on file on fd1 \n");
    error = flock(fd1, LOCK_EX);
    if (error) {
        XCTFail("flock on <%s> failed %d:%s \n", file_path,
                error, strerror(error));
        goto done;
    }

    printf("Opening file with O_RDONLY again on fd2 \n");
    fd2 = open(file_path, O_NONBLOCK | O_RDONLY);
    if (fd2 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }

    /* turn data caching off */
    printf("Disabling UBC caching on fd2 \n");
    if (fcntl(fd2, F_NOCACHE, 1) != 0) {
        XCTFail( "F_NOCACHE failed %d:%s \n",
                errno, strerror(errno));
        goto done;
    }

    printf("Verifying read access on fd1 \n");
    error = read_and_verify(fd1, data1, sizeof(data1), 0);
    if (error) {
        XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                error, strerror(error));
        goto done;
    }

    printf("Verifying read access on fd2 \n");
    error = read_and_verify(fd2, data1, sizeof(data1), 0);
    if (error) {
        XCTFail("read_and_verify on <%s> failed %d:%s \n", file_path,
                error, strerror(error));
        goto done;
    }

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

    /* Close file if needed */
    if (fd2 != -1) {
        error = close(fd2);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd2 = -1;
        }
    }

    /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testMultipleFlock
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int fd1 = -1;
    int fd2 = -1;
    int i1 = 0;
    pid_t child_pid = 0;
    int child_error = 0;

    /*
     * Test that SMB fd1 can be flock()'d, but another process can open the
     * same file but not flock() the file.
     *
     * 1. Open the file with all 3 variants of O_RDONLY, O_RDWR
     * 2. Call flock(LOCK_EX) on the open file
     * 3. With a child process, open the file with O_RDONLY
     * 4. With a child process, try to flock() which should fail
     *
     * Total variants tested: 2
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that SMB fd1 can be flock()'d, but another process can open the same file but not flock() the file.",
                               "open,close,flock",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testMultipleFlock");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    for (i1 = 0; i1 < 2; i1++) {
        switch(i1) {
            case 0:
                printf("Parent opening file with O_RDONLY \n");
                oflag1 = O_NONBLOCK | O_RDONLY;
                break;
            case 1:
                printf("Parent opening file with O_RDWR \n");
                oflag1 = O_NONBLOCK | O_RDWR;
                break;
            default:
                XCTFail("Unknown selector for i1 %d \n", i1);
                goto done;
        }
        
        /*
         * Open the testfile in parent process
         */
        fd1 = open(file_path, oflag1);
        if (fd1 == -1) {
            XCTFail("open on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
            goto done;
        }

        printf("Parent flock on file \n");
        error = flock(fd1, LOCK_EX);
        if (error) {
            XCTFail("flock on <%s> failed %d:%s \n", file_path,
                    error, strerror(error));
            goto done;
        }

        /* Kick off the child process now */
        child_pid = fork();
        if (child_pid != 0) {
            /*
             * Parent Testing code continues here
             */
            child_error = waitForChild(child_pid);
            if (child_error != 0) {
                XCTFail("Parent - Child test failed %d:%s \n",
                        child_error, strerror(child_error));
                goto done;
            }
            else {
                printf("Child test passed \n");
            }
            
            printf("Parent removing flock \n");
            error = flock(fd1, LOCK_UN);
            if (error) {
                XCTFail("flock on <%s> failed %d:%s \n", file_path,
                        error, strerror(error));
                goto done;
            }

            /* Close file */
            printf("Parent closing file \n");
            if (fd1 != -1) {
                error = close(fd1);
                if (error) {
                    printf("close on fd2 failed %d:%s \n", errno, strerror(errno));
                    //error = errno;
                    goto done;
                }
                else {
                    fd1 = -1;
                }
            }
        }
        else {
            /*
             * Child Testing code starts here
             * Do not call XCTFail, just printf
             */
        
            /*
             * Test that another process can read/write on fd1 that came
             * from parent process
             */
            printf("     Child opening file with O_RDONLY access \n");
            fd2 = open(file_path, O_RDONLY);
            if (fd2 == -1) {
                printf("     open on <%s> failed %d:%s \n", file_path,
                        errno, strerror(errno));
                exit(error);
            }

            printf("     Child flock on file which should fail \n");
            error = flock(fd2, LOCK_EX);
            if ((error != -1) || (errno != EWOULDBLOCK)) {
                printf("     flock on <%s> unexpectedly worked %d:%s \n", file_path,
                       error, strerror(error));
                close(fd2);
                exit(error);
            }

            error = close(fd2);
            if (error) {
                printf("close on fd2 failed %d:%s \n", errno, strerror(errno));
                //error = errno;
            }

            /*
             * Parents always have to clean up after this children,
             * so child just exits and leaves cleanup to parent process.
             */
            exit(0);
        } /* Child test code */
    } /* i1 loop */
        
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

-(void)testMultipleO_SHLOCK
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int fd1 = -1;
    int fd2 = -1;

    /*
     * Test that a file can be opened multiple times with O_SHLOCK with
     * no problems.
     *
     * This is a unit test for <93013096>
     *
     * 1. Open the file with O_RDONLY | O_SHLOCK for fd1
     * 2. Open the file with O_RDONLY | O_SHLOCK for fd2
     * 3. Close fd2
     * 4. Close fd1
     * 5. Open the file with O_RDONLY | O_SHLOCK for fd1
     * 6. Close fd1
     *
     * Total variants tested: 1
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that a file can be opened multiple times with O_SHLOCK with no problems.",
                               "open,close,O_SHLOCK",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testMultipleO_SHLOCK");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    printf("Opening file with O_RDONLY | O_SHLOCK on fd1 \n");
    fd1 = open(file_path, O_NONBLOCK | O_RDONLY | O_SHLOCK);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }

    printf("Opening file with O_RDONLY | O_SHLOCK on fd2 \n");
    fd2 = open(file_path, O_NONBLOCK | O_RDONLY | O_SHLOCK);
    if (fd2 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }

    /* Close fd2 */
    printf("Closing fd2 \n");
    error = close(fd2);
    if (error) {
        XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }

    /* Close fd1 */
    printf("Closing fd1 \n");
    error = close(fd1);
    if (error) {
        XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }

    printf("Reopening file with O_RDONLY | O_SHLOCK on fd1 \n");
    fd1 = open(file_path, O_NONBLOCK | O_RDONLY | O_SHLOCK);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }

    /* Close fd1 */
    printf("Closing fd1 \n");
    error = close(fd1);
    if (error) {
        XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    
     /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

- (void)testNegotiateIoctlSaLen
{
    int error = 0;
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that a security fix for negotiate works",
                               "negotiate",
                               NULL,
                               "91452444",
                               "no_server_required");
        return;
    }
    
    error = smb_load_library();
    if (error != 0) {
        XCTFail("smb_load_library failed %d\n",error);
        goto done;
    }

    struct smb_ctx ctx;
    ctx.ct_fd = -1;
    error = smb_ctx_gethandle(&ctx);
    if (error != 0 || ctx.ct_fd < 0) {
        XCTFail("fd (%d) < 0\n",ctx.ct_fd);
        goto done;
    }

    struct sockaddr_in smb_addr;
    bzero(&smb_addr, sizeof(smb_addr));
    smb_addr.sin_family = AF_INET;
    smb_addr.sin_len = sizeof(smb_addr);
    smb_addr.sin_port = htons(445);
    inet_aton("127.0.0.1", &smb_addr.sin_addr);

    const int src_zone_size = 32;
    /// Amount of OOB data to be read from successor element in src_zone
    const int oob_read_size = 32;

    struct sockaddr_nb saddr;
    bzero(&saddr, sizeof(saddr));
    /// We cannot use AF_INET or AF_INET6 type socket addresses because their
    /// `sa_len` must be equal to sizeof(struct sockaddr_in) and sizeof(struct sockaddr_in6)
    /// respectively or the socket connect will fail
    saddr.snb_family = AF_NETBIOS;
    saddr.snb_len = src_zone_size + oob_read_size;
    saddr.snb_addrin = smb_addr;
    /// Any netbios name with length greater than zero will work
    saddr.snb_name[0] = 1;

    struct sockaddr_nb laddr;
    bzero(&laddr, sizeof(laddr));
    laddr.snb_family = AF_NETBIOS;
    laddr.snb_len = sizeof(laddr);
    /// Any netbios name with length greater than zero will work
    laddr.snb_name[0] = 1;

    struct smbioc_negotiate negotiate_req;
    bzero(&negotiate_req, sizeof(negotiate_req));
    negotiate_req.ioc_version = SMB_IOC_STRUCT_VERSION;
    negotiate_req.ioc_saddr = (struct sockaddr*)&saddr;
    negotiate_req.ioc_saddr_len = src_zone_size;
    negotiate_req.ioc_laddr = (struct sockaddr*)&laddr;
    negotiate_req.ioc_laddr_len = sizeof(struct sockaddr_nb);
    negotiate_req.ioc_extra_flags |= SMB_FORCE_NEW_SESSION;
    negotiate_req.ioc_ssn.ioc_owner = getuid();
    negotiate_req.ioc_extra_flags |= SMB_SMB1_ENABLED;

    error = ioctl(ctx.ct_fd, SMBIOC_NEGOTIATE, &negotiate_req);
    if (error != -1 || errno != EINVAL) {
        XCTFail("failed: should return EINVAL when snb_len is not equal to ioc_saddr_len size, returned %d\n",
                negotiate_req.ioc_errno);
        goto done;
    }

    negotiate_req.ioc_saddr->sa_len = sizeof(saddr);
    negotiate_req.ioc_saddr_len = sizeof(saddr);
    errno = 0;

    error = ioctl(ctx.ct_fd, SMBIOC_NEGOTIATE, &negotiate_req);
    if (error != 0 || errno == EINVAL) {
        XCTFail("failed: should not return EINVAL when snb_len is equal to ioc_saddr_len\n");
        goto done;
    }
done:
    /* Now cleanup everything */
    if (ctx.ct_fd >= 0) {
        close(ctx.ct_fd);
    }
}

- (void)testLeaseWithOpenFileRename
{
    int error = 0;
    char file_path1[PATH_MAX];
    char file_path2[PATH_MAX];
    int fd1 = -1;;
    char mp1[PATH_MAX];
    struct smbStatPB pb = {0};
    uint32_t saved_lease_state = 0;

    /*
     * Test that no lease break occurs during a rename while the file is
     * still open.
     *
     * 1. mount mp1
     * 2. Open the test file with O_RDWR
     * 3. Verify file has RWH lease
     * 4. Rename file
     * 5. Verify file's lease state has not changed
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that no lease break occurs during a rename of an open file",
                               "open,close,rename,lease",
                               "2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testLeaseWithOpenFileRename");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path1, sizeof(file_path1));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    printf("Opening file with O_RDWR \n");
    fd1 = open(file_path1, O_NONBLOCK | O_RDWR);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path1,
                errno, strerror(errno));
        goto done;
    }

    printf("Verify got RWH lease on mp1 \n");
    bzero(&pb, sizeof(pb));
    error = fsctl(file_path1, smbfsStatFSCTL, &pb, 0);
    if (error != 0) {
        XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
        goto done;
    }

    if (!(pb.lease_flags & SMB2_LEASE_GRANTED)) {
        XCTFail("Failed to get lease. lease_flags 0x%llx \n",
                pb.lease_flags);
        goto done;
    }

    /*
     * Servers that support NFS ace will have a Create/SetInfo/Close done
     * to set the unix mode bits, but that will cause the lease to break down
     * to 0x03.
     */
    if ((pb.lease_curr_state != 0x7) && (pb.lease_curr_state != 0x3)) {
        XCTFail("Failed to get lease state. lease_curr_state 0x%x != 0x07 or 0x03 \n",
                pb.lease_curr_state);
        goto done;
    }

    saved_lease_state = pb.lease_curr_state;
    
    /* Rename file */
    printf("Rename file while it is still open\n");
    strlcpy(file_path2, file_path1, sizeof(file_path2));
    strlcat(file_path2, "-2", sizeof(file_path2));
    error = rename(file_path1, file_path2);
    if (error != 0) {
        XCTFail("rename from %s to %s failed %d (%s)\n\n",
                file_path1, file_path2, errno, strerror (errno));
        goto done;
    }

    printf("Verify lease remains unchanged \n");
    bzero(&pb, sizeof(pb));
    error = fsctl(file_path2, smbfsStatFSCTL, &pb, 0);
    if (error != 0) {
        XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
        goto done;
    }

    if (!(pb.lease_flags & SMB2_LEASE_GRANTED)) {
        XCTFail("Failed to get lease. lease_flags 0x%llx \n",
                pb.lease_flags);
        goto done;
    }

    /*
     * Servers that support NFS ace will have a Create/SetInfo/Close done
     * to set the unix mode bits, but that will cause the lease to break down
     * to 0x03.
     */
    if (pb.lease_curr_state != saved_lease_state) {
        XCTFail("Failed to match previous lease state. lease_curr_state 0x%x != 0x%x (previous state) \n",
                pb.lease_curr_state, saved_lease_state);
        goto done;
    }

    /* Close fd1 */
    printf("Closing fd1 \n");
    error = close(fd1);
    if (error) {
        XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    fd1 = -1;

    /* Do the Delete on test file */
    error = remove(file_path2);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path2, strerror(errno), errno);
        goto done;
    }

   /*
    * If no errors, attempt to delete test dirs. This could fail if a
    * previous test failed and thats fine.
    */
   do_delete_test_dirs(mp1);

done:
    if (fd1 != -1) {
        /* Close file on mp1 */
        error = close(fd1);
        if (error) {
            XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        }
    }
    
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

- (void)testDefCloseRenamePar
{
    int error = 0;
    char file_path[PATH_MAX];
    char dir_path1[PATH_MAX];
    char dir_path2[PATH_MAX];
    int fd1 = -1;;
    char mp1[PATH_MAX];
    struct smbStatPB pb = {0};
    uint32_t saved_lease_state = 0;

    /*
     * Test that SMB Client closes any children files that have a pending
     * deferred close before attempting to rename the parent.
     *
     * 1. mount mp1
     * 2. Create a parent test dir
     * 3. Create a test file inside the parent dir
     * 4. Open the test file with O_RDWR
     * 5. Verify file has RWH lease
     * 6. Close the file to get a pending deferred close
     * 7. Rename parent dir
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that SMB Client closes any children files that have a pending deferred close before attempting to rename the parent.",
                               "open,close,rename,lease,deferred_close",
                               "2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testDefCloseRenamePar");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create the test dirs */
    error = do_create_test_dirs(mp1);
    if (error) {
        XCTFail("do_create_test_dirs on <%s> failed %d:%s \n", mp1,
                error, strerror(error));
        goto done;
    }

    /* Set up parent dir path */
    strlcpy(dir_path1, mp1, sizeof(dir_path1));
    strlcat(dir_path1, "/", sizeof(dir_path1));
    strlcat(dir_path1, cur_test_dir, sizeof(dir_path1));
    strlcat(dir_path1, "/", sizeof(dir_path1));
    strlcat(dir_path1, "parent_dir", sizeof(dir_path1));

    /* Set up rename dir path */
    strlcpy(dir_path2, dir_path1, sizeof(dir_path2));
    strlcat(dir_path2, "-2", sizeof(dir_path2));

    /* Set up filename path */
    strlcpy(file_path, dir_path1, sizeof(file_path));
    strlcat(file_path, "/", sizeof(file_path));
    strlcat(file_path, default_test_filename, sizeof(file_path));

    /* Create the parent directory */
    printf("Creating parent dir \n");
    error = mkdir(dir_path1, S_IRWXU);
    if (error) {
        XCTFail("mkdir on <%s> failed %d:%s \n",
                dir_path1, errno, strerror(errno));
        goto done;
    }

    /* Make sure the dir is read/write */
    error = chmod(dir_path1, S_IRWXU | S_IRWXG | S_IRWXO);
    if (error) {
        XCTFail("chmod on <%s> failed %d:%s \n",
                dir_path1, errno, strerror(errno));
        goto done;
    }

    printf("Opening file with O_RDWR \n");
    fd1 = open(file_path, O_NONBLOCK | O_RDWR | O_CREAT);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }

    printf("Verify got RWH lease on mp1 \n");
    bzero(&pb, sizeof(pb));
    error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
    if (error != 0) {
        XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
        goto done;
    }

    if (!(pb.lease_flags & SMB2_LEASE_GRANTED)) {
        XCTFail("Failed to get lease. lease_flags 0x%llx \n",
                pb.lease_flags);
        goto done;
    }

    /*
     * Servers that support NFS ace will have a Create/SetInfo/Close done
     * to set the unix mode bits, but that will cause the lease to break down
     * to 0x03.
     */
    if ((pb.lease_curr_state != 0x7) && (pb.lease_curr_state != 0x3)) {
        XCTFail("Failed to get lease state. lease_curr_state 0x%x != 0x07 or 0x03 \n",
                pb.lease_curr_state);
        goto done;
    }

    saved_lease_state = pb.lease_curr_state;
    
    /* Close fd1 */
    printf("Closing fd1 to get a pending deferred close\n");
    error = close(fd1);
    if (error) {
        XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    fd1 = -1;

    /*
     * Rename parent dir which should cause a lease break to close the
     * testfile so the rename can work.
     */
    printf("Rename parent dir \n");
    error = rename(dir_path1, dir_path2);
    if (error != 0) {
        XCTFail("rename parent from %s to %s failed %d (%s)\n\n",
                dir_path1, dir_path2, errno, strerror (errno));
        goto done;
    }

    /* Set up new filename path */
    strlcpy(file_path, dir_path2, sizeof(file_path));
    strlcat(file_path, "/", sizeof(file_path));
    strlcat(file_path, default_test_filename, sizeof(file_path));

    /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /* Do the Delete on parent dir */
    error = remove(dir_path2);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
    * If no errors, attempt to delete test dirs. This could fail if a
    * previous test failed and thats fine.
    */
   do_delete_test_dirs(mp1);

done:
    if (fd1 != -1) {
        /* Close file on mp1 */
        error = close(fd1);
        if (error) {
            XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        }
    }
    
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

- (void)testMultiUserDefCloseRenamePar
{
    int error = 0;
    char file_path[PATH_MAX];
    char dir_path1[PATH_MAX];
    char dir_path2[PATH_MAX];
    char dir_path3[PATH_MAX];
    int fd1 = -1;;
    char mp1[PATH_MAX];
    char mp2[PATH_MAX];
    struct smbStatPB pb = {0};
    uint32_t saved_lease_state = 0;

    /*
     * Test that a rename of a parent dir will cause a lease break that closes
     * a pending deferred close by another client.
     *
     * 1. mount mp1
     * 2. Create a parent test dir on mp1
     * 3. Create a test file inside the parent dir on mp1
     * 4. Open the test file with O_RDWR on mp1
     * 5. Verify file has RWH lease on mp1
     * 6. Close the file to get a pending deferred close on mp1
     * 7. Rename parent dir on mp2
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that a rename of a parent dir will cause a lease break that closes a pending deferred close by another client.",
                               "open,close,rename,lease,lease_break,deferred_close",
                               "2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need two mounts to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testMultiUserDefCloseRenamePar1");
    do_create_mount_path(mp2, sizeof(mp2), "testMultiUserDefCloseRenamePar2");

    error = mount_two_sessions(mp1, mp2, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create the test dirs */
    error = do_create_test_dirs(mp1);
    if (error) {
        XCTFail("do_create_test_dirs on <%s> failed %d:%s \n", mp1,
                error, strerror(error));
        goto done;
    }

    /* Set up parent dir path */
    strlcpy(dir_path1, mp1, sizeof(dir_path1));
    strlcat(dir_path1, "/", sizeof(dir_path1));
    strlcat(dir_path1, cur_test_dir, sizeof(dir_path1));
    strlcat(dir_path1, "/", sizeof(dir_path1));
    strlcat(dir_path1, "parent_dir", sizeof(dir_path1));

    /* Set up rename dir paths on mp2 */
    strlcpy(dir_path2, mp2, sizeof(dir_path2));
    strlcat(dir_path2, "/", sizeof(dir_path2));
    strlcat(dir_path2, cur_test_dir, sizeof(dir_path2));
    strlcat(dir_path2, "/", sizeof(dir_path2));
    strlcat(dir_path2, "parent_dir", sizeof(dir_path2));

    strlcpy(dir_path3, dir_path2, sizeof(dir_path3));
    strlcat(dir_path3, "-2", sizeof(dir_path3));

    /* Set up filename path */
    strlcpy(file_path, dir_path1, sizeof(file_path));
    strlcat(file_path, "/", sizeof(file_path));
    strlcat(file_path, default_test_filename, sizeof(file_path));

    /* Create the parent directory */
    printf("Creating parent dir \n");
    error = mkdir(dir_path1, S_IRWXU);
    if (error) {
        XCTFail("mkdir on <%s> failed %d:%s \n",
                dir_path1, errno, strerror(errno));
        goto done;
    }

    /* Make sure the dir is read/write */
    error = chmod(dir_path1, S_IRWXU | S_IRWXG | S_IRWXO);
    if (error) {
        XCTFail("chmod on <%s> failed %d:%s \n",
                dir_path1, errno, strerror(errno));
        goto done;
    }

    printf("Opening file with O_RDWR \n");
    fd1 = open(file_path, O_NONBLOCK | O_RDWR | O_CREAT);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }

    printf("Verify got RWH lease on mp1 \n");
    bzero(&pb, sizeof(pb));
    error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
    if (error != 0) {
        XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
        goto done;
    }

    if (!(pb.lease_flags & SMB2_LEASE_GRANTED)) {
        XCTFail("Failed to get lease. lease_flags 0x%llx \n",
                pb.lease_flags);
        goto done;
    }

    /*
     * Servers that support NFS ace will have a Create/SetInfo/Close done
     * to set the unix mode bits, but that will cause the lease to break down
     * to 0x03.
     */
    if ((pb.lease_curr_state != 0x7) && (pb.lease_curr_state != 0x3)) {
        XCTFail("Failed to get lease state. lease_curr_state 0x%x != 0x07 or 0x03 \n",
                pb.lease_curr_state);
        goto done;
    }

    saved_lease_state = pb.lease_curr_state;
    
    /* Close fd1 */
    printf("Closing fd1 to get a pending deferred close\n");
    error = close(fd1);
    if (error) {
        XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    fd1 = -1;

    /*
     * Rename parent dir which should cause a lease break to close the
     * testfile so the rename can work.
     */
    printf("Rename parent dir on mp2\n");
    error = rename(dir_path2, dir_path3);
    if (error != 0) {
        XCTFail("rename parent from %s to %s failed %d (%s)\n\n",
                dir_path1, dir_path2, errno, strerror (errno));
        goto done;
    }

    /* Set up new filename path */
    strlcpy(file_path, dir_path2, sizeof(file_path));
    strlcat(file_path, "/", sizeof(file_path));
    strlcat(file_path, default_test_filename, sizeof(file_path));

    /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /* Do the Delete on parent dir */
    error = remove(dir_path2);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
    * If no errors, attempt to delete test dirs. This could fail if a
    * previous test failed and thats fine.
    */
   do_delete_test_dirs(mp1);

done:
    if (fd1 != -1) {
        /* Close file on mp1 */
        error = close(fd1);
        if (error) {
            XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        }
    }
    
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    if (unmount(mp2, MNT_FORCE) == -1) {
        XCTFail("unmount failed for second url %d\n", errno);
    }

    rmdir(mp1);
    rmdir(mp2);
}

- (void)testDefCloseTimeout
{
    int error = 0;
    char file_path[PATH_MAX];
    int fd1 = -1;;
    char mp1[PATH_MAX];
    struct smbStatPB pb = {0};
    uint32_t saved_lease_state = 0;
    int max_wait = 65, i;

    /*
     * Test that a pending deferred close times out after 30 seconds and gets
     * closed as part of the vop_sync()
     *
     * 1. mount mp1
     * 2. Open the test file with O_RDWR
     * 3. Verify file has RWH lease
     * 4. Close the file to get a pending deferred close
     * 5. Wait until vop_sync has a change to run and close the def close
     * 6. Verify that file no longer has a pending deferred close
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that a pending deferred close times out after 30 seconds and gets closed as part of the vop_sync()",
                               "open,close,lease,deferred_close",
                               "2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testDefCloseTimeout");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }

    printf("Opening file with O_RDWR \n");
    fd1 = open(file_path, O_NONBLOCK | O_RDWR);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }

    printf("Verify got RWH lease on mp1 \n");
    bzero(&pb, sizeof(pb));
    error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
    if (error != 0) {
        XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
        goto done;
    }

    if (!(pb.lease_flags & SMB2_LEASE_GRANTED)) {
        XCTFail("Failed to get lease. lease_flags 0x%llx \n",
                pb.lease_flags);
        goto done;
    }

    /*
     * Servers that support NFS ace will have a Create/SetInfo/Close done
     * to set the unix mode bits, but that will cause the lease to break down
     * to 0x03.
     */
    if ((pb.lease_curr_state != 0x7) && (pb.lease_curr_state != 0x3)) {
        XCTFail("Failed to get lease state. lease_curr_state 0x%x != 0x07 or 0x03 \n",
                pb.lease_curr_state);
        goto done;
    }

    saved_lease_state = pb.lease_curr_state;
    
    /* Close fd1 */
    printf("Closing fd1 to get a pending deferred close\n");
    error = close(fd1);
    if (error) {
        XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    fd1 = -1;

    printf("Verify got deferred close on mp1 \n");
    bzero(&pb, sizeof(pb));
    error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
    if (error != 0) {
        XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
        goto done;
    }

    if (!(pb.lease_flags & SMB2_DEFERRED_CLOSE)) {
        XCTFail("Failed to get deferred close. lease_flags 0x%llx \n",
                pb.lease_flags);
        goto done;
    }

    /* A pending deferred close should get closed after 30 secs by vop_sync */
    printf("Waiting for deferred close timeout \n");

    for (i = 0; i < max_wait; i++) {
        bzero(&pb, sizeof(pb));
        error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
        if (error) {
            XCTFail("Waiting fsctl failed %d (%s)\n\n", errno, strerror (errno));
            goto done;
        }

        if (pb.lease_flags != 0) {
            /* Still pending, so keep waiting */
            sleep(1);
            printf("Waiting %d secs \n", i);
        }
        else {
            printf("Done waiting after %d secs \n", i);
            break;
        }
    }

    printf("Verify no pending deferred close on mp1 \n");
    bzero(&pb, sizeof(pb));
    error = fsctl(file_path, smbfsStatFSCTL, &pb, 0);
    if (error != 0) {
        XCTFail("fsctl failed %d (%s)\n\n", errno, strerror (errno));
        goto done;
    }

    if (pb.lease_flags != 0) {
        XCTFail("Failed to verify no pending deferred close. lease_flags 0x%llx \n",
                pb.lease_flags);
        goto done;
    }

    /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
    * If no errors, attempt to delete test dirs. This could fail if a
    * previous test failed and thats fine.
    */
   do_delete_test_dirs(mp1);

done:
    if (fd1 != -1) {
        /* Close file on mp1 */
        error = close(fd1);
        if (error) {
            XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        }
    }
    
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

/*
 * This test is only valid for servers that support AAPL create context and
 * support the NFS ACE for setting/getting Posix mode bits.
 */
extern ssize_t __getdirentries64(int, void *, size_t, off_t *);

- (void)test_o_search
{
    /*
     * Copied from xnu/tests/vfs/o_search.c
     */
#ifndef O_EXEC
#define O_EXEC 0x40000000
#define O_SEARCH (O_EXEC | O_DIRECTORY)
#endif

#define NUMDIRS 5

    int error = 0;
    char file_path[PATH_MAX];
    char dir_path[PATH_MAX];
    int dir_fd = -1;;
    char mp1[PATH_MAX];
    void *mapped = MAP_FAILED;
    off_t dirbyte = 0;
    int fd = -1;
    char namebuf[(sizeof(struct dirent) * (NUMDIRS + 2))];
    char attrbuf[256] = {0};

    /*
     * Various tests with the O_EXEC (file only) and O_SEARCH (dir only) flags
     *
     * 1. mount mp1
     * 2. Create a parent test dir
     * 3. Create a test file inside the parent dir with 644 permissions
     * 4. Verify that open(O_EXEC) on dir fails
     * 5. Open dir and do getdirentries and then close it
     * 6. Open dir with O_SEARCH, verify getdirentries fails
     * 7. Verify openat(dir, O_EXEC) fails because of 644 permissions
     * 8. chmod the testfile to 744
     * 9. Verify openat(dir, O_SEARCH) on testfile fails
     * 10. Verify openat(dir, O_EXEC) now works on testfile
     * 11. Verify read/write/mmap all fail on testfile due to the O_EXEC flag
     */

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Various tests with O_EXEC and O_SEARCH open flags",
                               "open,close,nfs_ace",
                               "2,3",
                               NULL,
                               "apple");
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "test_o_search");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create the test dirs */
    error = do_create_test_dirs(mp1);
    if (error) {
        XCTFail("do_create_test_dirs on <%s> failed %d:%s \n", mp1,
                error, strerror(error));
        goto done;
    }

    /* Set up file path */
    strlcpy(file_path, mp1, sizeof(file_path));
    strlcat(file_path, "/", sizeof(file_path));
    strlcat(file_path, cur_test_dir, sizeof(file_path));
    strlcat(file_path, "/", sizeof(file_path));
    strlcat(file_path, default_test_filename, sizeof(file_path));

    printf("Creating testfile \n");
    fd = open(file_path, O_CREAT | O_RDWR, 0644);
    if (fd == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }

    printf("Writing initial data \n");
    error = write_and_verify(fd, file_path, sizeof(file_path), 0);
    if (error) {
        XCTFail("initial write_and_verify failed %d \n", error);
        close(fd);
        goto done;
    }

    /* Close file */
    printf("Closing created file \n");
    error = close(fd);
    if (error) {
        XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    else {
        fd = -1;
    }

    /* Set up parent dir path */
    strlcpy(dir_path, mp1, sizeof(dir_path));
    strlcat(dir_path, "/", sizeof(dir_path));
    strlcat(dir_path, cur_test_dir, sizeof(dir_path));
    printf("Creating dir_path \n");

    printf("Opening dir with O_EXEC which should fail with EISDIR \n");
    dir_fd = open(dir_path, O_EXEC);
    if ((dir_fd != -1) || (errno != EISDIR)) {
        XCTFail("open on <%s> should have failed with EISDIR but got %d:%s \n", dir_path,
                errno, strerror(errno));
        goto done;
    }
    
    printf("Opening dir with O_RDONLY \n");
    dir_fd = open(dir_path, O_RDONLY);
    if (dir_fd == -1) {
        XCTFail("open on <%s> failed %d:%s \n", dir_path,
                errno, strerror(errno));
        goto done;
    }

    printf("Doing getdirentries64 \n");
    error = (int)__getdirentries64(dir_fd, namebuf, sizeof(namebuf), &dirbyte);
    if (error == -1) {
        XCTFail("getdirentries64 on dir_fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }

    printf("Closing dir \n");
    error = close(dir_fd);
    if (error) {
        XCTFail("close on dir_fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    dir_fd = -1;

    printf("Opening dir with O_SEARCH \n");
    dir_fd = open(dir_path, O_SEARCH);
    if (dir_fd == -1) {
        XCTFail("open on <%s> failed %d:%s \n", dir_path,
                errno, strerror(errno));
        goto done;
    }
    
    printf("Doing getdirentries64 which should fail with EBADF \n");
    error = (int)__getdirentries64(dir_fd, namebuf, sizeof(namebuf), &dirbyte);
    if ((error != -1) || (errno != EBADF)) {
        XCTFail("getdirentries64 should have failed with EBADF instead of %d:%s \n",
                errno, strerror(errno));
        goto done;
    }

    printf("Doing openat(dir_fd) with O_EXEC on file that only has 644 which should fail \n");
    fd = openat(dir_fd, default_test_filename, O_EXEC);
    if (fd != -1) {
        //if ((fd != -1) || (errno != EACCES)) {
        XCTFail("openat on <%s> should have failed with EACCES but got %d:%s \n",
                default_test_filename, errno, strerror(errno));
        goto done;
    }

    printf("chmod test file to 744 \n");
    error = fchmodat(dir_fd, default_test_filename, 0744, 0);
    if (error) {
        XCTFail("fchmodat failed %d:%s \n", errno, strerror(errno));
        goto done;
    }

    printf("Doing openat(dir_fd) with O_SEARCH on file which should fail \n");
    fd = openat(dir_fd, default_test_filename, O_SEARCH);
    if ((fd != -1) || (errno != ENOTDIR)) {
        XCTFail("openat on <%s> should have failed with ENOTDIR but got %d:%s \n",
                default_test_filename, errno, strerror(errno));
        goto done;
    }

    printf("Doing openat(dir_fd) with O_EXEC on file \n");
    fd = openat(dir_fd, default_test_filename, O_EXEC);
    if (fd == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }
    
    printf("Doing read on file opened with O_EXEC which should fail \n");
    error = (int)read(fd, &attrbuf, 2);
    if ((error != -1) || (errno != EBADF)) {
        XCTFail("read should have failed with EBADF instead of %d:%s \n",
                errno, strerror(errno));
        goto done;
    }

    printf("Doing write on file opened with O_EXEC which should fail \n");
    error = (int)write(fd, &attrbuf, 2);
    if ((error != -1) || (errno != EBADF)) {
        XCTFail("write should have failed with EBADF instead of %d:%s \n",
                errno, strerror(errno));
        goto done;
    }

    printf("Doing mmap(PROT_READ) on file opened with O_EXEC which should fail \n");
    mapped = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if ((mapped != MAP_FAILED) || (errno != EACCES)) {
        XCTFail("mmap should have failed with EACCES instead of %d:%s \n",
                errno, strerror(errno));
        goto done;
    }
    
    printf("Doing mmap(PROT_WRITE) on file opened with O_EXEC which should fail \n");
    mapped = mmap(NULL, PAGE_SIZE, PROT_WRITE, MAP_SHARED, fd, 0);
    if ((mapped != MAP_FAILED) || (errno != EACCES)) {
        XCTFail("mmap should have failed with EACCES instead of %d:%s \n",
                errno, strerror(errno));
        goto done;
    }

    printf("Closing created file \n");
    error = close(fd);
    if (error) {
        XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    fd = -1;

    printf("Closing dir \n");
    error = close(dir_fd);
    if (error) {
        XCTFail("close on dir_fd failed %d:%s \n", errno, strerror(errno));
        goto done;
    }
    dir_fd = -1;


    /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)>",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
    * If no errors, attempt to delete test dirs. This could fail if a
    * previous test failed and thats fine.
    */
   do_delete_test_dirs(mp1);

done:
    if (fd != -1) {
        /* Close file on mp1 */
        error = close(fd);
        if (error) {
            XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        }
    }
    
    if (dir_fd != -1) {
        /* Close dir on mp1 */
        error = close(dir_fd);
        if (error) {
            XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
        }
    }

    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

- (void)testTimeResolution
{
    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int need_to_remove = false;
    int is_mounted = false;
    char command[PATH_MAX] = "/usr/bin/touch -d 1976-04-13T10:02:34.123456789 ";
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("verify the time resolution of file modification time",
                               "open,close,stat",
                               "1,2,3",
                               "76242052",
                               "apple,windows");
        return;
    }

    // create a local mount point
    do_create_mount_path(mp1, sizeof(mp1), "testTimeResolution");

    // mount the share
    error = mount_two_sessions(mp1, NULL, 0);
    
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }
    is_mounted = true;

    // prepare the file path
    strcpy(file_path, mp1);
    strcat(file_path, "/tmp");
    
    // prepare the touch command
    strcat(command , file_path);
    
    // create a file with a specific date to check later
    error = system(command);
    if (error) {
        XCTFail("failed to run the touch command");
        goto done;
    }

    need_to_remove = true;
    struct stat st;
    // get the file's attributes
    stat(file_path, &st);
    
    // convert the file's modification time to a calendar date and time
    struct tm *time = localtime(&st.st_mtimespec.tv_sec);
    
    // tm_mon range is 0-11
    if ((time->tm_year + 1900 != 1976) || (time->tm_mon + 1 != 4) ||
        (time->tm_mday != 13) || time->tm_hour != 10 || (time->tm_min != 2) ||
        (time->tm_sec != 34)) {
        XCTFail("wrong date, expected:1976-04-13T10:02:34, actual: %d-%d-%dT%d:%d:%d",
                time->tm_year + 1900, time->tm_mon + 1, time->tm_mday, time->tm_hour,
                time->tm_min, time->tm_sec);
    }
    
    // smb supports time accuary in the 100 nanoseconds intervals
    if (st.st_mtimespec.tv_nsec != 123456700) {
        XCTFail("incorrect nanoseconds, expected: 123456700, actual: %lu",
                st.st_mtimespec.tv_nsec);
    }
     
done:
    // remove the test file
    if (need_to_remove) {
        remove(file_path);
    }
    if (is_mounted) {
        if (unmount(mp1, MNT_FORCE) == -1) {
            XCTFail("unmount failed for first url %d\n", errno);
        }
    }
    rmdir(mp1);
}

-(void)testOpenUnlink
{
    int error = 0;
    char dir_path[PATH_MAX] = {0};
    char file_path[PATH_MAX] = {0};
    char file_name[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int oflag1 = 0;
    int fd1 = -1, fd2 = -1;
    off_t offset = 0;
    char *last_slash = NULL;
    struct dirent *dp = NULL;
    DIR * dirp = NULL;

    /*
     * Test for correct unix open/unlink behavior.
     *
     * 1. Open file with O_RDWR
     * 2. Disable data caching
     * 3. Verify can write and read data on file
     * 4. Call remove (aka unlink) on the open file
     * 5. Verify lseek, write and read data still work on file
     * 6. Verify readdir can not find the file
     * 7. Close the file and it should truly get deleted on server at this time
     * 8. Verify remove gets ENOENT error since the file should be gone
     *
     * Total variants tested: 1
     */
    
    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test for correct open/unlink behavior",
                               "open,close,unlink",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testOpenUnlink");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create test dir and setup test file */
    error = initialFileSetup(mp1, file_path, sizeof(file_path));
    if (error) {
        XCTFail("initialFileSetup failed %d:%s \n", error, strerror(error));
        goto done;
    }
    
    strlcpy(dir_path, file_path, sizeof(dir_path));
    last_slash = strrchr(dir_path, '/');
    if (last_slash != NULL) {
        /* Found the last slash, so lop it off */
        strlcpy(file_name, last_slash+1, sizeof(file_name));

        *last_slash = '\0';
    }

    /*
     * Open the testfile in parent process
     */
    printf("Opening file with O_RDWR \n");
    oflag1 = O_NONBLOCK | O_RDWR;

    fd1 = open(file_path, oflag1);
    if (fd1 == -1) {
        XCTFail("open on <%s> failed %d:%s \n", file_path,
                errno, strerror(errno));
        goto done;
    }

    /* Turn off data caching. All IO should go out immediately */
    printf("Disabling caching \n");
    error = fcntl(fd1, F_NOCACHE, 1);
    if (error) {
        XCTFail("fcntl on fd1 failed %d:%s \n", error, strerror(error));
        goto done;
    }

    /* Write out and verify initial data on mp1 */
    printf("Writing/reading some data \n");
    error = write_and_verify(fd1, data1, sizeof(data1), 0);
    if (error) {
        XCTFail("initial write_and_verify failed %d \n", error);
        goto done;
    }

    /* Delete the open file */
    printf("Deleting the open file \n");
    error = unlink(file_path);
    if (error) {
        XCTFail("unlink on fd1 failed %d:%s \n",
                error, strerror(error));
        goto done;
    }

    fprintf (stderr, "Verifying second open fails with ENOENT \n");
    fd2 = open (file_path, oflag1);
    if ((fd2 != -1) && (errno != ENOENT)) {
        XCTFail("Second open failed (expected ENOENT) %d:%s\n",
                errno, strerror(errno));
        goto done;
    }

    /* Verify lseek still works */
    printf("Verifying lseek \n");
    offset = lseek(fd1, 0, 0);
    if (offset == -1) {
        XCTFail("lseek on fd1 failed %d:%s \n",
                errno, strerror(errno));
        goto done;
    }

    /* Write out and verify initial data on mp1 */
    printf("Verifying writing/reading some data \n");
    error = write_and_verify(fd1, data1, sizeof(data1), 0);
    if (error) {
        XCTFail("initial write_and_verify failed %d \n", error);
        goto done;
    }

    /*
     * Verify dir enumeration can not find the file
     */
    
    printf("Verifying enumeration does not find the file \n");

    /* Open the dir to be enumerated*/
    dirp = opendir(dir_path);
    if (dirp == NULL) {
        XCTFail("opendir on <%s> failed %d:%s \n",
                dir_path, errno, strerror(errno));
        goto done;
    }

    while ((dp = readdir(dirp)) != NULL) {
        if ((strcmp(dp->d_name, ".") == 0) || (strcmp(dp->d_name, "..") == 0)) {
            /* Skip . and .. */
            continue;
        }

        printf("  Checking <%s> \n", dp->d_name);
        if (strcmp(file_name, dp->d_name) == 0) {
            XCTFail("readdir found the deleted file of <%s> \n", dp->d_name);
            (void)closedir(dirp);
            error = EEXIST;
            goto done;
        }
    }
    
    (void)closedir(dirp);
    
    /* Close file */
    printf("Closing file which should do the actual delete \n");
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd1 failed %d:%s \n", errno, strerror(errno));
            //error = errno;
            goto done;
        }
        else {
            fd1 = -1;
        }
    }
    
    /*
     * Cleanup code only expected to be run by parent process
     */

    /* Close file if needed */
    if (fd1 != -1) {
        error = close(fd1);
        if (error) {
            XCTFail("close on fd failed %d:%s \n", errno, strerror(errno));
            goto done;
        }
        else {
            //fd1 = -1;
        }
    }

     /*
      * Do the Delete on test file which should fail as the file should
      * have gotten deleted on the close above.
      */
    printf("Verifying that file got deleted on last close \n");
    error = remove(file_path);
    if ((error != -1) && (errno != ENOENT)) {
        XCTFail("do_delete on <%s> did not fail as expected <%s (%d)> \n",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}


char* dir25kName = "static-25k_dirs";
/*
 * Note: this test takes a long time
 * this can be reduced by lowering entry_cnt
 */
-(void)testreaddir
{
    int error = 0;
    char mp[PATH_MAX] = {0};
    uint32_t entry_idx = 0;
    struct dirent *dp = NULL;
    DIR * dirp = NULL;
    char dir_path[PATH_MAX];
    uint32_t entry_cnt = 25000;
    uint8_t entries_arr[entry_cnt + 1];
    char *idx_ptr = NULL;
    memset(entries_arr, 0, sizeof(entries_arr));
    int dot = 0, dotdot = 0;
    int i = 0;

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test for correct readdir behavior",
                               "opendir,closedir,readdir",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    do_create_mount_path(mp, sizeof(mp), "testreaddir");

    error = mount_two_sessions(mp, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto error;
    }
    
    strlcpy(dir_path, mp, sizeof(dir_path));
    strlcat(dir_path, "/", sizeof(dir_path));
    strlcat(dir_path, dir25kName, sizeof(dir_path));
    

    for (i = 0; i < 2; i++) {
        dirp = opendir(dir_path);
        if (dirp == NULL) {
            XCTFail("opendir on <%s> failed %d:%s \n",
                    dir_path, errno, strerror(errno));
            goto error;
        }

        dot = dotdot = 0;
        memset(entries_arr, 0, sizeof(entries_arr));

        while ((dp = readdir(dirp)) != NULL) {
            if (strcmp(dp->d_name, ".") == 0) {
                dot++;
                continue;
            } else if (strcmp(dp->d_name, "..") == 0) {
                dotdot++;
                continue;
            }
            if (strncmp(dp->d_name, "dir.", 4) != 0) {
                XCTFail("entry <%s> not in correct format dir.<index> \n", dp->d_name);
                continue;
            }

            idx_ptr = strchr(dp->d_name, '.') + 1;
            entry_idx = atoi(idx_ptr);
            if (entry_idx < 1 || entry_idx > entry_cnt) {
                XCTFail("entry <%s> has unexpected index \n", dp->d_name);
                continue;
            }
            if (entries_arr[entry_idx] != 0) {
                XCTFail("directory <%s> seen %d times already \n", dp->d_name, entries_arr[entry_idx]);
            }
            entries_arr[entry_idx]++;
        }
        if (dot != 1) {
            XCTFail("dot was seen %d times", dot);
        }
        if (dotdot != 1) {
            XCTFail("dotdot was seen %d times", dot);
        }
        for (entry_idx = 1; entry_idx <= entry_cnt; entry_idx++){
            if (entries_arr[entry_idx] != 1) {
                XCTFail("directory dir.%05d was seen %d times \n", entry_idx, entries_arr[entry_idx]);
            }
        }
        closedir(dirp);
        dirp = NULL;
    }

error:
    if (dirp != NULL) {
        closedir(dirp);
        dirp = NULL;
    }
    if (unmount(mp, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp);
}

/*
 * Note: this test takes a long time
 * this can be reduced by lowering entry_cnt
 */
-(void)testgetattrlistbulk
{
    int error = 0;
    char mp[PATH_MAX] = {0};
    DIR * dirp = NULL;
    char dir_path[PATH_MAX];
    uint32_t entry_cnt = 25000;
    uint8_t entries_arr[entry_cnt];
    memset(entries_arr, 0, sizeof(entries_arr));
    int numReturnedBulk = 0;
    int total_returned = 0;
    int dirBulkReplySize = 0;
    char *dirBulkReplyPtr = NULL;
    attribute_set_t req_attrs = {0};
    time_t current_time = 0;
    char *time_str;
    int i = 0;

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test for correct getAttrListBulk behavior",
                               "opendir,closedir,getAttrListBulk",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp, sizeof(mp), "testGetattrlistbulk");

    error = mount_two_sessions(mp, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto error;
    }

    strlcpy(dir_path, mp, sizeof(dir_path));
    strlcat(dir_path, "/", sizeof(dir_path));
    strlcat(dir_path, dir25kName, sizeof(dir_path));
    
    for (i = 0; i < 2; i++) {
        dirp = opendir(dir_path);
        if (dirp == NULL) {
            XCTFail("opendir on <%s> failed %d:%s \n",
                    dir_path, errno, strerror(errno));
            goto error;
        }

        current_time = time(NULL);
        time_str = ctime(&current_time);
        time_str[strlen(time_str)-1] = '\0';
        if (gVerbose) {
            printf("Starting enumeration at: %s\n", time_str);
        }

        dirBulkReplySize = kEntriesPerCall * (sizeof(struct replyData)) + 1024;
        dirBulkReplyPtr = malloc(dirBulkReplySize);
        total_returned = 0;
        while ((numReturnedBulk = getAttrListBulk(dirp->__dd_fd, dirBulkReplySize,
                                                  dirBulkReplyPtr, &req_attrs)) > 0) {
            total_returned += numReturnedBulk;
            if (gVerbose) {
                current_time = time(NULL);
                char * time_str = ctime(&current_time);
                time_str[strlen(time_str)-1] = '\0';
                printf("Time: %s Got %d entries. Total %d\n",
                       time_str, numReturnedBulk, total_returned);
            }
        }
        current_time = time(NULL);
        time_str = ctime(&current_time);
        time_str[strlen(time_str)-1] = '\0';
        if (gVerbose) {
            printf("Finished enumeration at: %s\n", time_str);
        }
        if (numReturnedBulk < 0) {
            XCTFail("getAttrListBulk returned %d \n", numReturnedBulk);
        }
        if (entry_cnt != total_returned) {
            XCTFail("expected %d entries, but getAttrListBulk returned %d \n", entry_cnt, total_returned);
        }
        closedir(dirp);
        dirp = NULL;
    }

error:
    if (dirBulkReplyPtr) {
        free(dirBulkReplyPtr);
        dirBulkReplyPtr = NULL;
    }

    if (dirp != NULL) {
        closedir(dirp);
    }

    if (unmount(mp, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp);
}

-(void)testSymlinkXattr
{
    int error = 0;
    char xattr_value[] = "test_value";
    char file_path[PATH_MAX];
    char mp1[PATH_MAX];
    char buffer[PATH_MAX];
    ssize_t ret = 0;

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test that xattr operations work on a symlink",
                               "open,close,symlink,reparse_point,xattr",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }

    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testSymlinkXattr");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    /* Create the test dirs */
    error = do_create_test_dirs(mp1);
    if (error) {
        XCTFail("do_create_test_dirs on <%s> failed %d:%s \n", mp1,
                error, strerror(error));
        goto done;
    }

    /* Set up file path */
    strlcpy(file_path, mp1, sizeof(file_path));
    strlcat(file_path, "/", sizeof(file_path));
    strlcat(file_path, cur_test_dir, sizeof(file_path));
    strlcat(file_path, "/", sizeof(file_path));
    strlcat(file_path, default_test_filename, sizeof(file_path));

    /* Create the symlink with junk path */
    printf("Create symlink \n");
    error = symlink("foo/bar", file_path);
    if (error) {
        XCTFail("symlink on <%s> failed %d:%s \n", file_path,
                error, strerror(error));
        goto done;
    }

    /* Set xattr on the symlink */
    printf("Set xattr on symlink \n");
    error = setxattr(file_path, "test_xattr", xattr_value, sizeof(xattr_value), 0, XATTR_NOFOLLOW);
    if (error) {
        XCTFail("setxattr on <%s> failed %d:%s \n", file_path,
                error, strerror(error));
        goto done;
    }

    /* List xattr on the symlink */
    printf("List xattrs on symlink \n");
    ret = listxattr(file_path, buffer, sizeof(buffer), XATTR_NOFOLLOW);
    if ((ret == -1) || (ret == 0)) {
        if (ret == 0) {
            XCTFail("listxattr on <%s> returned %zd meaning no xattrs found \n", file_path,
                    ret);
        }
        else {
            XCTFail("listxattr on <%s> failed %d:%s \n", file_path,
                    errno, strerror(errno));
        }
        goto done;
    }

    /* Remove xattr on the symlink */
    printf("Delete xattr on symlink \n");
    error = removexattr(file_path, "test_xattr", XATTR_NOFOLLOW);
    if (error) {
        XCTFail("removexattr on <%s> failed %d:%s \n", file_path,
                error, strerror(error));
        goto done;
    }

    /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)> \n",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }

    rmdir(mp1);
}

#define LARGE_XATTR_SIZE 1024*1024*8
-(void)testLargeXattr
{
    int error = 0, i = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    ssize_t ret = 0;
    uint8_t* xattr_value = NULL;
    uint8_t* returned_xattr_value = NULL;

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test reading and writing large xattr",
                               "open,close,xattr",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }
    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testLargeXattr");

    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }

    error = setup_file_paths(mp1, NULL, default_test_filename, file_path, PATH_MAX, NULL, 0);
    if (error) {
        goto done;
    }

    xattr_value = malloc(LARGE_XATTR_SIZE);
    returned_xattr_value = malloc(LARGE_XATTR_SIZE);
    if (xattr_value == NULL || returned_xattr_value == NULL) {
        XCTFail("failed to allocate xattr_value:<%p> returned_xattr_value:<%p> \n",
                xattr_value, returned_xattr_value);
        goto cleanup;
    }

    for (i = 0; i < LARGE_XATTR_SIZE; i++) {
        xattr_value[i] = i % sizeof(xattr_value[i]);
    }

    error = setxattr(file_path, "test_xattr", xattr_value, LARGE_XATTR_SIZE, 0, 0);
    if (error) {
        XCTFail("setxattr failed errno:%d\n", errno);
        goto cleanup;
    }

    ret = getxattr(file_path, "test_xattr", returned_xattr_value, LARGE_XATTR_SIZE, 0, 0);
    if (ret != LARGE_XATTR_SIZE) {
        XCTFail("getxattr returned %zd, expected %d\n", ret, LARGE_XATTR_SIZE);
        goto cleanup;
    }

    for (i = 0; i < LARGE_XATTR_SIZE; i++) {
        if (xattr_value[i] != returned_xattr_value[i]) {
            XCTFail("byte#%d is %u, expected is %u\n", i, returned_xattr_value[i], xattr_value[i]);
            break;
        }
    }
cleanup:
    /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)> \n",
                file_path, strerror(errno), errno);
        goto done;
    }

    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);

done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d\n", errno);
    }
    if (xattr_value) {
        free(xattr_value);
    }
    if (returned_xattr_value) {
        free(returned_xattr_value);
    }

    rmdir(mp1);
}

-(void)testXattr
{
    /* Make sure BUFSZ is a multiple of 10 please */
    #define RSRC_XATTR_NAME_LENGTH 23
    #define BUF_SIZE (100 + RSRC_XATTR_NAME_LENGTH)
    #define SMALL_BUF_SIZE (BUF_SIZE / 2)
    #define LARGE_BUF_SIZE (BUF_SIZE * 2)
    #define NAME_SIZE 10
    #define KEY "sizecheck"
    #define VALUE "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
    #define VALUE_SIZE sizeof(VALUE)//-1

    int error = 0;
    char file_path[PATH_MAX] = {0};
    char mp1[PATH_MAX] = {0};
    int options = 0;
    char name_buf[NAME_SIZE] = {0};
    char recvbuf[BUF_SIZE] = {0};
    char small_recvbuf[SMALL_BUF_SIZE] = {0};
    char large_recvbuf[LARGE_BUF_SIZE] = {0};
    int i = 0;
    ssize_t xa_size = 0;

    if (list_tests_with_mdata == 1) {
        do_list_test_meta_data("Test various xattr operations",
                               "open,close,xattr,listxattr,getxattr,setxattr,removexattr",
                               "1,2,3",
                               NULL,
                               NULL);
        return;
    }
    /*
     * We will need just one mount to start with
     */
    do_create_mount_path(mp1, sizeof(mp1), "testXattr");
    
    error = mount_two_sessions(mp1, NULL, 0);
    if (error) {
        XCTFail("mount_two_sessions failed %d \n", error);
        goto done;
    }
    
    error = setup_file_paths(mp1, NULL, default_test_filename, file_path, PATH_MAX, NULL, 0);
    if (error) {
        XCTFail("setup_file_paths failed %d \n", error);
        goto done;
    }
    
    /* Test that listxattr returns correctly on file with no xattrs */
    xa_size = listxattr(file_path, recvbuf, BUF_SIZE, options);
    if (xa_size != 0) {
        XCTFail("listxattr() with no xattrs failed %zd, errno %d \n",
            xa_size, errno);
        goto done;
    }
    fprintf(stdout, "listxattr() on file with no xattrs worked \n");

    
    /* Test that getxattr returns correctly on file with no xattrs */
    xa_size = getxattr(file_path, "KeyName", recvbuf, BUF_SIZE, 0, options);
    if (xa_size == -1) {
        if ((errno != ENOATTR)) {
            XCTFail("getxattr() with no xattrs failed %zd, but has wrong errno %d \n",
                    xa_size, errno);
            goto done;
        }
    }
    else {
        XCTFail("getxattr() with no xattrs failed %zd, errno %d \n",
                xa_size, errno);
        goto done;
    }
    fprintf(stdout, "getxattr() on file with no xattrs worked \n");

    
    /*
     * Add xattr names (10 byte long names) enough to equal BUFSZ
     * Dont forget there is one byte for null terminator
     */
    bzero(name_buf, sizeof(name_buf));
    for (i = 0; i < ((BUF_SIZE - RSRC_XATTR_NAME_LENGTH) / NAME_SIZE); i++) {
        sprintf(name_buf, "12345%04d", i);
        
        if ((xa_size = setxattr(file_path, name_buf, VALUE, VALUE_SIZE, 0, options)) < 0) {
            XCTFail("setxattr() failed %d", errno);
            goto done;
        }
    }
    fprintf(stdout, "Added <%d> test xattrs with values \n", i);

    
    /* Add Resource Fork xattr */
    if ((xa_size = setxattr(file_path, XATTR_RESOURCEFORK_NAME, VALUE, VALUE_SIZE, 0, options)) < 0) {
        XCTFail("setxattr() failed %d for resource fork \n", errno);
        goto done;
    }
    fprintf(stdout, "Added resource fork with set value \n");

    
    /* Test correct size is returned for listxattr with NULL buffer */
    xa_size = listxattr(file_path, NULL, 0, options);
    if (xa_size != BUF_SIZE) {
        XCTFail("listxattr() to get buffer size failed %zd, errno %d \n",
                xa_size, errno);
        goto done;
    }
    fprintf(stdout, "listxattr() returned correct size of xattrs with NULL buffer \n");

    
    /* Test correct size is returned for getxattr with NULL buffer */
    xa_size = getxattr(file_path, name_buf, NULL, 0, 0, options);
    if (xa_size != VALUE_SIZE) {
        XCTFail("getxattr() to get buffer size failed %zd, errno %d \n",
                xa_size, errno);
        goto done;
    }
    fprintf(stdout, "getxattr() returned correct size for one ext attr with NULL buffer \n");


    /* Test correct size is returned for getxattr for resource fork with NULL buffer */
    xa_size = getxattr(file_path, XATTR_RESOURCEFORK_NAME, NULL, 0, 0, options);
    if (xa_size != VALUE_SIZE) {
        XCTFail("getxattr() to get buffer size failed %zd for resource fork, errno %d \n",
                xa_size, errno);
        goto done;
    }
    fprintf(stdout, "getxattr() returned correct size for resource fork with NULL buffer \n");

    
    /* Test for ERANGE with a too small buffer with listxattr */
    xa_size = listxattr(file_path, small_recvbuf, SMALL_BUF_SIZE, options);
    if (xa_size == -1) {
        if ((errno != ERANGE)) {
            XCTFail("listxattr() with too small buffer failed %zd, but has wrong errno %d \n",
                    xa_size, errno);
            goto done;
        }
    }
    else {
        XCTFail("listxattr() with too small buffer failed %zd, errno %d \n",
                xa_size, errno);
        goto done;
    }
    fprintf(stdout, "listxattr() returned correct ERANGE when given too small of a buffer \n");
    

    /* Test for ERANGE with a too small buffer with getxattr */
    xa_size = getxattr(file_path, name_buf, small_recvbuf, (VALUE_SIZE) / 2, 0, options);
    if (xa_size == -1) {
        if ((errno != ERANGE)) {
            XCTFail("getxattr() with too small buffer failed %zd, but has wrong errno %d \n",
                    xa_size, errno);
            goto done;
        }
    }
    else {
        XCTFail("getxattr() with too small buffer failed %zd, errno %d \n",
                xa_size, errno);
        goto done;
    }
    fprintf(stdout, "getxattr() returned correct ERANGE when given too small of a buffer \n");

    
    /* Test for ERANGE with a too small buffer with getxattr on resource fork */
    xa_size = getxattr(file_path, XATTR_RESOURCEFORK_NAME, small_recvbuf, (VALUE_SIZE) / 2, 0, options);
    if (xa_size == -1) {
        if ((errno != ERANGE)) {
            XCTFail("getxattr() with too small buffer failed %zd for resource fork, but has wrong errno %d \n",
                    xa_size, errno);
            goto done;
        }
    }
    else {
        /* Hmm, resource forks dont get ERANGE errors??? */
        if (xa_size == (VALUE_SIZE) / 2) {
            fprintf(stdout, "getxattr() did not get ERANGE error for resource fork, but got (VALUE_SIZE) / 2 returned instead \n");
        }
        else {
            XCTFail("getxattr() with too small buffer failed %zd for resource fork, errno %d \n",
                    xa_size, errno);
            goto done;
        }
    }
    fprintf(stdout, "getxattr() returned correct ERANGE when given too small of a buffer for resource fork \n");

    
    /* Test with just right sized buffer with listxattr */
    xa_size = listxattr(file_path, recvbuf, BUF_SIZE, options);
    if (xa_size != BUF_SIZE) {
        XCTFail("listxattr() with just right buffer failed %zd, errno %d \n",
                xa_size, errno);
        goto done;
    }
    fprintf(stdout, "listxattr() worked with just right buffer size \n");

    
    /* Test with just right sized buffer with getxattr */
    xa_size = getxattr(file_path, name_buf, recvbuf, VALUE_SIZE, 0, options);
    if (xa_size != VALUE_SIZE) {
        XCTFail("getxattr() with just right buffer failed %zd, errno %d \n",
                xa_size, errno);
        goto done;
    }
    fprintf(stdout, "getxattr() worked with just right buffer size \n");

    
    /* Test with just right sized buffer with getxattr for resource fork */
    xa_size = getxattr(file_path, XATTR_RESOURCEFORK_NAME, recvbuf, VALUE_SIZE, 0, options);
    if (xa_size != VALUE_SIZE) {
        XCTFail("getxattr() with just right buffer failed %zd for resource fork, errno %d \n",
                xa_size, errno);
        goto done;
    }
    fprintf(stdout, "getxattr() worked with just right buffer size for resource fork \n");
    
    
    /* Test with just larger than needed sized buffer */
    xa_size = listxattr(file_path, large_recvbuf, LARGE_BUF_SIZE, options);
    if (xa_size != BUF_SIZE) {
        XCTFail("listxattr() with extra large buffer failed %zd, errno %d \n",
                xa_size, errno);
        goto done;
    }
    fprintf(stdout, "listxattr() worked with too big of a buffer size \n");

    
    /* Test with just larger than needed sized buffer with getxattr */
    xa_size = getxattr(file_path, name_buf, recvbuf, VALUE_SIZE * 2, 0, options);
    if (xa_size != VALUE_SIZE) {
        XCTFail("getxattr() with extra large buffer failed %zd, errno %d \n",
                xa_size, errno);
        goto done;
    }
    fprintf(stdout, "getxattr() worked with too big of a buffer size \n");

        
    /* Test with just larger than needed sized buffer with getxattr */
    xa_size = getxattr(file_path, XATTR_RESOURCEFORK_NAME, recvbuf, VALUE_SIZE * 2, 0, options);
    if (xa_size != VALUE_SIZE) {
        XCTFail("getxattr() with extra large buffer failed %zd for resource fork, errno %d \n",
                xa_size, errno);
        goto done;
    }
    fprintf(stdout, "getxattr() worked with too big of a buffer size for resource fork \n");

    
    /* Test trying to delete non existent xattr */
    error = removexattr(file_path, "KeyName", options);
    if (error == -1) {
        if ((errno != ENOATTR)) {
            XCTFail("removexattr() with non existent xattr failed %d, but has wrong errno %d \n",
                    error, errno);
            goto done;
        }
    }
    else {
        XCTFail("removexattr() with non existent xattr failed %d, errno %d \n",
                error, errno);
        goto done;
    }
    fprintf(stdout, "removexattr() worked with non existent xattr \n");

    
    /* Test trying to delete xattr */
    error = removexattr(file_path, name_buf, options);
    if (error == -1) {
        XCTFail("removexattr() failed %d for xattr \n", errno);
        goto done;
    }
    fprintf(stdout, "removexattr() worked with xattr \n");

    
    /* Test trying to delete resource fork xattr */
    error = removexattr(file_path, XATTR_RESOURCEFORK_NAME, options);
    if (error == -1) {
        XCTFail("removexattr() failed %d for resource fork \n", errno);
        goto done;
    }
    fprintf(stdout, "removexattr() worked with resource fork \n");

     
    /* Do the Delete on test file */
    error = remove(file_path);
    if (error) {
        fprintf(stderr, "do_delete on <%s> failed <%s (%d)> \n",
                file_path, strerror(errno), errno);
        goto done;
    }
    
    /*
     * If no errors, attempt to delete test dirs. This could fail if a
     * previous test failed and thats fine.
     */
    do_delete_test_dirs(mp1);
    
done:
    if (unmount(mp1, MNT_FORCE) == -1) {
        XCTFail("unmount failed for first url %d \n", errno);
    }
    
    rmdir(mp1);
}


@end

