/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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
 *  firmwaresyncd.c
 *  bless
 *
 *  Created by Shantonu Sen on 10/13/07.
 *  Copyright 2007 Apple Inc. All rights reserved.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <err.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <syslog.h>
#include <sysexits.h>
#include <copyfile.h>
#include <sys/sysctl.h>

#include <DiskArbitration/DiskArbitration.h>
#include <DiskArbitration/DiskArbitrationPrivate.h>

#include <IOKit/kext/kextmanager_types.h>
#include <IOKit/kext/kextmanager_mig.h>

#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <sys/resource.h>

#include "bless.h"
#include "bless_private.h"

#define kFirmwareFileOSPath "/usr/standalone/i386/Firmware.scap"
#define kFirmwareFileEFIDir "/EFI/APPLE/EXTENSIONS"
#define kFirmwareFileEFIPath "/EFI/APPLE/EXTENSIONS/Firmware.scap"
#define kTimeDelay (4*60)
#define kTSCacheDir         "/System/Library/Caches/com.apple.bootstamps"

extern char **environ;

void usage(void);
void catch_sigterm(int sig);

static bool gSIGTERM = false;

/* Should we even run? If not, exit out. If so, use the volume UUID for / */
bool should_run(void);
bool get_uuid(CFUUIDRef *uuid);
bool allocate_mach_ports(mach_port_t *kextdport, mach_port_t *vollock);
bool deallocate_mach_ports(mach_port_t kextdport, mach_port_t vollock);
bool lock_volume(CFUUIDRef uuid, mach_port_t kextdport, mach_port_t vollock);
bool unlock_volume(CFUUIDRef uuid, mach_port_t kextdport, mach_port_t vollock);
bool generate_timestamp_path(CFUUIDRef uuid, char *path);
bool check_if_uptodate(CFUUIDRef uuid);
bool update_esp(void);
bool update_timestamp(CFUUIDRef uuid);
bool run_tool(char *argv[], CFDataRef *output);

int main(int argc, char *argv[]) {

    int ch;
    int opt_d = 0;
    CFUUIDRef uuid = NULL;
    mach_port_t vollock = MACH_PORT_NULL, kextdport = MACH_PORT_NULL;
    bool needunlock = false, immediately = false;
    unsigned int sleepleft;
    
    signal(SIGTERM, catch_sigterm);
	setpriority(PRIO_PROCESS, 0, PRIO_DARWIN_BG);
    
    while ((ch = getopt(argc, argv, "di")) != -1) {
        switch (ch) {
            case 'd':
                opt_d = 1;
                break;
            case 'i':
                immediately = true;
                break;
            case '?':
            default:
                usage();
                break;
        }
    }
    
    argc -= optind;
    argv += optind;
    
    openlog(getprogname(), LOG_PID | (opt_d ? LOG_PERROR : 0), LOG_DAEMON);
    setlogmask(opt_d ? LOG_UPTO(LOG_DEBUG) : LOG_UPTO(LOG_ERR));
    
//    syslog(LOG_INFO, "This is informational");
//    syslog(LOG_DEBUG, "This is debuggingational");

    // In general we try exit on failures without complaining
    if (!should_run()) {
        goto done;
    }
    
    syslog(LOG_DEBUG, "Preflight passed");
    
    if (!get_uuid(&uuid)) {
        goto done;
    }
    
    if (!check_if_uptodate(uuid)) {
        goto done;
    }
    
    sleepleft = (immediately ? 0 : kTimeDelay);
    syslog(LOG_DEBUG, "Sleeping for %u seconds", sleepleft);
    do {
        if (gSIGTERM) {
            syslog(LOG_DEBUG, "Caught SIGTERM and exiting");
            goto done;
        }
        sleepleft = sleep(sleepleft);
    } while (sleepleft > 0);
    syslog(LOG_DEBUG, "Done sleeping");

    if (!allocate_mach_ports(&kextdport, &vollock)) {
        goto done;
    }
        
    // relock, in case the state changed while we were asleep
    if (!lock_volume(uuid, kextdport, vollock)) {
        goto done;
    }
    needunlock = true;
    
    if (!check_if_uptodate(uuid)) {
        goto done;
    }
    
    if (!update_esp()) {
        goto done;
    }
    
    if (!update_timestamp(uuid)) {
        goto done;
    }

    if (!unlock_volume(uuid, kextdport, vollock)) {
        goto done;
    }
    needunlock = false;

    
done:
    if (needunlock) {
        unlock_volume(uuid, kextdport, vollock);
    }
    if (kextdport != MACH_PORT_NULL || vollock != MACH_PORT_NULL) {
        deallocate_mach_ports(kextdport, vollock);
    }
    if (uuid) {
        CFRelease(uuid);
    }
    closelog();
    return 0;
}

void usage(void)
{
    fprintf(stderr, "Usage: %s [-d]\n", getprogname());
    exit(EX_USAGE);
}

void catch_sigterm(int sig)
{
    gSIGTERM = true;
}

bool should_run(void)
{
    bool result = false;
    BLPreBootEnvType preBootType;
    struct stat sb;
    uint32_t safeboot = 0;
    size_t safebootsize = sizeof(safeboot);
    int ret;
    
    ret = sysctlbyname("kern.safeboot", &safeboot, &safebootsize, NULL, 0);
    if (ret) {
        syslog(LOG_DEBUG, "Could not determine safeboot status: %s", strerror(errno));
        return result;
    }
    syslog(LOG_DEBUG, "Safeboot status: %u", safeboot);
    if (safeboot) {
        return result;
    }
    
    if (0 != BLGetPreBootEnvironmentType(NULL, &preBootType)) {
        syslog(LOG_DEBUG, "Could not determine preboot environment type");
        return result;
    }
    if (preBootType != kBLPreBootEnvType_EFI) {
        syslog(LOG_DEBUG, "Preboot environment type is not EFI");
        return result;
    }
    
    if (0 != lstat(kFirmwareFileOSPath, &sb) || !S_ISREG(sb.st_mode)) {
        syslog(LOG_DEBUG, "Font file %s is not accessible or not a regular file", kFirmwareFileOSPath);
        return result;
    }
    
    result = true;
    
    return result;
}


static void _DADiskAppearedCallback( DADiskRef disk, void * context );

static void _DADiskAppearedCallback( DADiskRef disk, void * context )
{
    CFUUIDRef *uuid = (CFUUIDRef *)context;
    CFUUIDRef dauuid = NULL;
    CFDictionaryRef dadescription;

    dadescription = DADiskCopyDescription(disk);
    if (dadescription) {
        dauuid = CFDictionaryGetValue(dadescription, kDADiskDescriptionVolumeUUIDKey);
        if (dauuid) {
            if (*uuid) {
                CFRelease(*uuid);
            }
            *uuid = CFRetain(dauuid);
        }
        CFRelease(dadescription);
    }
    
}

bool get_uuid(CFUUIDRef *uuid)
{
    DASessionRef dasession;
    CFURLRef rootpath;
    CFMutableDictionaryRef matchdict;
    bool result = false;
    
    *uuid = NULL;
    
    rootpath = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (UInt8 *)"/", 1, true);
    if (rootpath) {
        dasession = DASessionCreate(kCFAllocatorDefault);
        if (dasession) {
            DASessionScheduleWithRunLoop(dasession, CFRunLoopGetCurrent(), CFSTR("FIRMWARESYNCD"));
            
            matchdict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CFDictionaryAddValue(matchdict, kDADiskDescriptionVolumePathKey, rootpath);
            DARegisterDiskAppearedCallback(dasession, matchdict, _DADiskAppearedCallback, uuid);
            CFRelease(matchdict);

            while (!gSIGTERM && !*uuid) {
                CFRunLoopRunInMode(CFSTR("FIRMWARESYNCD"), 1.0, true);
            }
            DASessionUnscheduleFromRunLoop(dasession, CFRunLoopGetCurrent(), CFSTR("FIRMWARESYNCD"));
            CFRelease(dasession);
        }
        CFRelease(rootpath);
    }
    if (*uuid) {
        result = true;
        syslog(LOG_DEBUG, "Determined volume UUID for /");
    } else {
        syslog(LOG_DEBUG, "Could not determine volume UUID for /");
    }
    
    return result;
}

bool lock_volume(CFUUIDRef uuid, mach_port_t kextdport, mach_port_t vollock)
{
    bool result = false;
    kern_return_t kret;
    
    uuid_t s_vol_uuid;
    CFUUIDBytes uuidbytes;
    int lckres = 0;
    
    uuidbytes = CFUUIDGetUUIDBytes(uuid);
    memcpy(&s_vol_uuid, &uuidbytes, sizeof(uuidbytes));

    syslog(LOG_DEBUG, "Locking volume");

    kret = kextmanager_lock_volume(kextdport, vollock, s_vol_uuid,
                                      1 /* block */, &lckres);
	if (kret || lckres) {
        syslog(LOG_DEBUG, "Failed to obtain lock: %s/%s", mach_error_string(kret), strerror(lckres));
        goto done;
    }
    
    result = true;
    
done:
    return result;
}

bool unlock_volume(CFUUIDRef uuid, mach_port_t kextdport, mach_port_t vollock)
{
    bool result = false;
    kern_return_t kret;
    
    uuid_t s_vol_uuid;
    CFUUIDBytes uuidbytes;
    
    uuidbytes = CFUUIDGetUUIDBytes(uuid);
    memcpy(&s_vol_uuid, &uuidbytes, sizeof(uuidbytes));
    
    syslog(LOG_DEBUG, "Unlocking volume");
    
    kret = kextmanager_unlock_volume(kextdport, vollock, s_vol_uuid,
                                   0);
	if (kret) {
        syslog(LOG_DEBUG, "Failed to unlock: %s", mach_error_string(kret));
        goto done;
    }
    
    result = true;
    
done:
    return result;
}

bool allocate_mach_ports(mach_port_t *kextdport, mach_port_t *vollock)
{
    bool result = false;
    kern_return_t kret;
    
    mach_port_t sLockPort = MACH_PORT_NULL;
    mach_port_t sKextdPort = MACH_PORT_NULL;

    syslog(LOG_DEBUG, "Obtaining mach ports");

    if (sKextdPort == MACH_PORT_NULL) {
        kret = bootstrap_look_up(bootstrap_port, KEXTD_SERVER_NAME, &sKextdPort);
        if (kret) {
            syslog(LOG_DEBUG, "Failed to look up port for %s: %s", KEXTD_SERVER_NAME, mach_error_string(kret));
            goto done;
        }
    }
    
    if (sLockPort == MACH_PORT_NULL) {
        kret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &sLockPort);
        if (kret) {
            syslog(LOG_DEBUG, "Failed to look up port for %s: %s", KEXTD_SERVER_NAME, mach_error_string(kret));
            goto done;
        }
    }
    
    
    result = true;
    *kextdport = sKextdPort;
    *vollock = sLockPort;
    
done:
    return result;
}

bool deallocate_mach_ports(mach_port_t kextdport, mach_port_t vollock)
{
    bool result = false;
    kern_return_t kret;
    
    syslog(LOG_DEBUG, "Deallocating mach ports");

    if (kextdport != MACH_PORT_NULL) {
        kret = mach_port_mod_refs(mach_task_self(), kextdport, MACH_PORT_RIGHT_SEND, -1);
        if (kret) {
            syslog(LOG_DEBUG, "Failed to drop reference for %s: %s", KEXTD_SERVER_NAME, mach_error_string(kret));
            goto done;
        }
    }
    
    if (vollock != MACH_PORT_NULL) {
        kret = mach_port_mod_refs(mach_task_self(), vollock, MACH_PORT_RIGHT_RECEIVE, -1);
        if (kret) {
            syslog(LOG_DEBUG, "Failed to drop reference for lock %s", mach_error_string(kret));
            goto done;
        }
    }
    
    result = true;
    
done:
    return result;
}

bool generate_timestamp_path(CFUUIDRef uuid, char *path)
{
    char timepath[MAXPATHLEN];
    char colonpath[MAXPATHLEN], *cptr;
    char uuidcomponent[NAME_MAX];
    CFStringRef uuidstr;
    bool result = false;
    
    uuidstr = CFUUIDCreateString(kCFAllocatorDefault, uuid);
    if (!CFStringGetCString(uuidstr, uuidcomponent, sizeof(uuidcomponent), kCFStringEncodingUTF8)) {
        CFRelease(uuidstr);
        goto done;
    }
    CFRelease(uuidstr);
    
    strlcpy(colonpath, kFirmwareFileOSPath, sizeof(colonpath));
    cptr = strchr(colonpath, '/');
    while (cptr) {
        *cptr++ = ':';
        cptr = strchr(cptr, '/');
    }
    
    // we check for overflow on the last operation, since it should be cumulative
    strlcpy(timepath, kTSCacheDir, sizeof(timepath));
    strlcat(timepath, "/", sizeof(timepath));
    strlcat(timepath, uuidcomponent, sizeof(timepath));
    strlcat(timepath, "/", sizeof(timepath));
    if (strlcat(timepath, colonpath, sizeof(timepath)) >= sizeof(timepath)) {
        goto done;
    }

    strlcpy(path, timepath, MAXPATHLEN);
    result = true;
    
done:
    return result;
}


/* Check in /S/L/C if we have a cookie file newer than the font file */
bool check_if_uptodate(CFUUIDRef uuid)
{
    bool result = false, needupdate = false;
    struct stat sb, sb2;
    int ret;
    char timepath[MAXPATHLEN];
    
    // ...
    if (0 != lstat(kFirmwareFileOSPath, &sb)) {
        syslog(LOG_DEBUG, "Could not access %s: %s", kFirmwareFileOSPath, strerror(errno));
        goto done;
    }
    
    if (!S_ISREG(sb.st_mode)) {
        syslog(LOG_DEBUG, "%s is not a regular file", kFirmwareFileOSPath);
        goto done;        
    }
    
    if (!generate_timestamp_path(uuid, timepath)) {
        syslog(LOG_DEBUG, "Could not generate path (%s)", timepath);
        goto done;
    }
    
    syslog(LOG_DEBUG, "Timestamp file is %s", timepath);
    ret = lstat(timepath, &sb2);
    if (ret) {
        if (errno == ENOENT) {
            syslog(LOG_DEBUG, "Timestamp does not exist");
            needupdate = true;
        } else {
            ret = unlink(timepath);
            if (ret) {
                syslog(LOG_DEBUG, "Could not remove %s", timepath);
                goto done;
            }
            needupdate = true;
        }
    } else {
        struct timeval filetime, stamptime;
        
        if (!S_ISREG(sb2.st_mode)) {
            syslog(LOG_DEBUG, "%s is not a regular file", timepath);
            goto done;        
        }
        
        TIMESPEC_TO_TIMEVAL(&filetime, &sb.st_mtimespec);
        TIMESPEC_TO_TIMEVAL(&stamptime, &sb2.st_mtimespec);
        
        if (timercmp(&filetime, &stamptime, >)) {
            syslog(LOG_DEBUG, "File newer than timestamp");
            needupdate = true;
        } else {
            syslog(LOG_DEBUG, "File matches timestamp"); 
            needupdate = false;
        }
    }

    if (needupdate) {
        result = true;
    }
    
done:
    return result;
}

bool update_esp(void)
{
    bool result = false, needunmount = false, needrmdir = false;
    struct statfs sb;
    int ret;
    CFDictionaryRef dict = NULL;
    CFArrayRef array = NULL;
    CFStringRef esp = NULL;
    char *newargv[10];
    int newargc;
    char *slash;
    char espname[MAXPATHLEN], espdev[MAXPATHLEN], mntpath[MAXPATHLEN], espfontpath[MAXPATHLEN];
    CFDataRef output = NULL;
    
    ret = statfs("/", &sb);
    if (ret) {
        syslog(LOG_DEBUG, "Failed to statfs /: %s", strerror(errno));
        goto done;
    }
    
    if (0 != strncmp(sb.f_mntfromname, "/dev/", 5)) {
        goto done;
    }
    
    ret = BLCreateBooterInformationDictionary(NULL, sb.f_mntfromname + 5, &dict);
    if (ret) {
        syslog(LOG_DEBUG, "Failed to obtain EFI System Partition information: %d", ret);
        goto done;
    }
    
    array = CFDictionaryGetValue(dict, kBLSystemPartitionsKey);
    if(array) {
        if(CFArrayGetCount(array) > 0) {
            esp = CFArrayGetValueAtIndex(array, 0);
            
            if (!BLIsEFIRecoveryAccessibleDevice(NULL, esp)) {
                syslog(LOG_DEBUG, "ESP %s is not accessible as a recovery device\n" , BLGetCStringDescription(esp));
                goto done;
            }
        }
    }        
    
    if(!esp) {
        // needed ESP, but could not find it
        syslog(LOG_DEBUG, "No appropriate ESP for %s\n" , "/");
        goto done;
    }
    
    if (!CFStringGetCString(esp, espname, sizeof(espname), kCFStringEncodingUTF8)) {
        goto done;
    }
    
    CFRelease(dict);
    dict = NULL;
    esp = NULL;
    
    strlcpy(espdev, "/dev/", sizeof(espdev));
    strlcat(espdev, espname, sizeof(espdev));
    syslog(LOG_DEBUG, "ESP partition is %s", espdev);
    
    
    syslog(LOG_DEBUG, "Verifying %s", espdev);
    
    newargc = 0;
    newargv[newargc++] = "/sbin/fsck_msdos";
    newargv[newargc++] = "-fn";
    newargv[newargc++] = espdev;
    newargv[newargc++] = NULL;
    
    if (!run_tool(newargv, &output)) {
        if (output) {
            syslog(LOG_ERR, "Command %s output: %.*s", newargv[0], (int)CFDataGetLength(output), CFDataGetBytePtr(output));
            CFRelease(output);
        }
        goto done;
    }
    if (output) {
        CFRelease(output);
    }
    
    strlcpy(mntpath, "/Volumes/firmwaresyncd.XXXXXX", sizeof(mntpath));
    if(!mkdtemp(mntpath)) {
        syslog(LOG_DEBUG, "Could not make temporary directory %s", mntpath);
        goto done;
    }
    needrmdir = true;
    syslog(LOG_DEBUG, "Temporary mount point is %s", mntpath);
    
    newargc = 0;
    newargv[newargc++] = "/sbin/mount";
    newargv[newargc++] = "-t";
    newargv[newargc++] = "msdos";
    newargv[newargc++] = "-o";
    newargv[newargc++] = "perm";
    newargv[newargc++] = "-o";
    newargv[newargc++] = "nobrowse";
    newargv[newargc++] = espdev;
    newargv[newargc++] = mntpath;
    newargv[newargc++] = NULL;

    if (!run_tool(newargv, &output)) {
        if (output) {
            syslog(LOG_ERR, "Command %s output: %.*s", newargv[0], (int)CFDataGetLength(output), CFDataGetBytePtr(output));
            CFRelease(output);
        }
        goto done;
    }
    if (output) {
        CFRelease(output);
    }
            
    needunmount = true;
    
    strlcpy(espfontpath, mntpath, sizeof(espfontpath));
    strlcat(espfontpath, kFirmwareFileEFIDir, sizeof(espfontpath));
    
    // make parent directories if they are not present
    slash = espfontpath + strlen(mntpath);
    while (strsep(&slash, "/")) {
        syslog(LOG_DEBUG, "Checking %s", espfontpath);
        ret = mkdir(espfontpath, S_IRWXU);
        if (ret && errno != EEXIST) {
            syslog(LOG_DEBUG, "Failed to create %s: %s", espfontpath, strerror(errno));
            break;
        }
        if (slash == NULL) {
            break;
        }
        *(slash - 1) = '/';
    }
    
    strlcpy(espfontpath, mntpath, sizeof(espfontpath));
    strlcat(espfontpath, kFirmwareFileEFIPath, sizeof(espfontpath));

    ret = copyfile(kFirmwareFileOSPath, espfontpath, NULL, COPYFILE_DATA);
    if (ret) {
        syslog(LOG_DEBUG, "Could not copy %s to %s: %d", kFirmwareFileOSPath, espfontpath, ret);
        goto done;
    }
    syslog(LOG_DEBUG, "Copied %s to %s", kFirmwareFileOSPath, espfontpath);
        
    result = true;
    
done:
    if (needunmount) {
        newargc = 0;
        newargv[newargc++] = "/sbin/umount";
        newargv[newargc++] = mntpath;
        newargv[newargc++] = NULL;
        
        if (!run_tool(newargv, &output)) {
            if (output) {
                syslog(LOG_ERR, "Command %s output: %.*s", newargv[0], (int)CFDataGetLength(output), CFDataGetBytePtr(output));
                CFRelease(output);
            }
            goto done2;
        }
        if (output) {
            CFRelease(output);
        }
        
    }      
    
done2:
    if (needrmdir) {
        syslog(LOG_DEBUG, "Removing %s\n", mntpath);

        ret = rmdir(mntpath);
    }
    
    if (dict) {
        CFRelease(dict);
    }
    return result;
}

bool update_timestamp(CFUUIDRef uuid)
{
    bool result = false, closefd = false;;
    char timepath[MAXPATHLEN];
    int ret, tfd;
    struct stat sb;
    struct timeval times[2];
    
    if (0 != lstat(kFirmwareFileOSPath, &sb)) {
        syslog(LOG_DEBUG, "Could not access %s: %s", kFirmwareFileOSPath, strerror(errno));
        goto done;
    }
    
    if (!S_ISREG(sb.st_mode)) {
        syslog(LOG_DEBUG, "%s is not a regular file", kFirmwareFileOSPath);
        goto done;        
    }
    
    if (!generate_timestamp_path(uuid, timepath)) {
        syslog(LOG_DEBUG, "Could not generate path (%s)", timepath);
        goto done;
    }
    
    syslog(LOG_DEBUG, "Updating timestamp file %s", timepath);
    ret = unlink(timepath);
    if (ret && errno != ENOENT) {
        syslog(LOG_DEBUG, "Could not remove %s: %s", timepath, strerror(errno));
        goto done;
    }
    
    tfd = open(timepath, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (tfd < 0) {
        syslog(LOG_DEBUG, "Could not create %s: %s", timepath, strerror(errno));
        goto done;
    }
    closefd = true;
    
    TIMESPEC_TO_TIMEVAL(&times[0], &sb.st_atimespec);
    TIMESPEC_TO_TIMEVAL(&times[1], &sb.st_mtimespec);

    ret = futimes(tfd, times);
    if (ret) {
        syslog(LOG_DEBUG, "Could not set time for %s: %s", timepath, strerror(errno));
        goto done;
    }    
    
    result = true;
    
done:
    if (closefd) {
        ret = close(tfd);
        if (ret) {
            syslog(LOG_DEBUG, "Could not close %s: %s", timepath, strerror(errno));
        }
    }    
    
    return result;

}

bool run_tool(char *argv[], CFDataRef *output)
{
    bool result = false, destroyFileActions = false, closeFDs = false;
    pid_t p, p2;
    int ret;
    CFMutableDataRef dataRef;
    int fds[2];
    posix_spawn_file_actions_t file_actions;
    char buffer[100];
    ssize_t readBytes;
    
    dataRef = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ret = pipe(fds);
    if (ret) {
        syslog(LOG_DEBUG, "Could not call posix_spawn: %d", errno);
        goto done;        
    }
    closeFDs = true;
    
    ret = posix_spawn_file_actions_init(&file_actions);
    if (ret < 0) {
        syslog(LOG_DEBUG, "Could not call posix_spawn_file_actions_init: %d", ret);
        goto done;        
    }
    destroyFileActions = true;
    
    posix_spawn_file_actions_addclose(&file_actions, fds[0]);
    if (fds[1] != STDOUT_FILENO) {
        (void)posix_spawn_file_actions_adddup2(&file_actions, fds[1], STDOUT_FILENO);
        if (fds[1] != STDERR_FILENO) {
            (void)posix_spawn_file_actions_adddup2(&file_actions, fds[1], STDERR_FILENO);
        }        
        (void)posix_spawn_file_actions_addclose(&file_actions, fds[1]);
    }
    
    syslog(LOG_DEBUG, "Calling %s\n", argv[0]);
    ret = posix_spawn(&p, argv[0], &file_actions, NULL, argv, environ);
    if (ret) {
        syslog(LOG_DEBUG, "Could not call posix_spawn: %d", ret);
        goto done;
    }
    
    // read 100 bytes at a time until we get EOF (child closed pipe);
    close(fds[1]);
    fds[1] = -1;
    do {
        readBytes = read(fds[0], buffer, sizeof(buffer));
        syslog(LOG_DEBUG, "Read %ld from pipe", readBytes);
        if (readBytes > 0) {
            CFDataAppendBytes(dataRef, (UInt8 *)buffer, readBytes);
        }
    } while (readBytes > 0);
    
    if (readBytes < 0) {
        syslog(LOG_DEBUG, "read returned error: %d\n", errno );
        goto done;        
    }
    
    do {
        p2 = waitpid(p, &ret, 0);
    } while (p2 == -1 && errno == EINTR);
    
    syslog(LOG_DEBUG, "Returned %d\n", ret);
    if(p2 == -1) {
        syslog(LOG_DEBUG, "%s failed to return: %d\n", argv[0], errno );
        goto done;
    }
    if(ret) {
        if (WIFEXITED(ret)) {
            syslog(LOG_ERR, "%s exited with %d\n", argv[0], WEXITSTATUS(ret) );
        } else {
            syslog(LOG_ERR, "%s signaled with %d\n", argv[0], WTERMSIG(ret) );
        }
        goto done;
    }
    
    result = true;
    
done:
    
    if (destroyFileActions) {
        posix_spawn_file_actions_destroy(&file_actions);
    }
    
    if (closeFDs) {
        close(fds[0]);
        close(fds[1]);
    }
    
    *output = dataRef;
    
    return result;
}

