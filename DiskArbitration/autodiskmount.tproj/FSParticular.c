/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include "FSParticular.h"
#include "DiskVolume.h"
#include "DiskArbitrationServerMain.h"


#include <libc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <bsd/dev/disk.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <grp.h>
#include <ctype.h>
#include <dirent.h>
#include <mach/boolean.h>
#include <sys/loadable_fs.h>
#include <ufs/ufs/ufsmount.h>
#include <string.h>

#include <bsd/fsproperties.h>

#define MAXNAMELEN	256

CFMutableDictionaryRef plistDict = nil;
CFMutableArrayRef matchingArray = nil;

char * daCreateCStringFromCFString(CFStringRef string)
{
    /*
     * result of daCreateCStringFromCFString should be released with free()
     */

    CFStringEncoding encoding     = kCFStringEncodingMacRoman;
    CFIndex          bufferLength = CFStringGetLength(string) + 1;
    char *           buffer       = malloc(bufferLength);

    if (buffer)
    {
        if (CFStringGetCString(string, buffer, bufferLength, encoding) == FALSE)
        {
            free(buffer);
            buffer = malloc(1); // See Radar 2457357.
                        buffer[ 0 ] = '\0'; // See Radar 2457357.
        }
    }

    return buffer;
} /* daCreateCStringFromCFString */

/*
 * This function implements ordering for hybrids.  It makes 9660/UDF
 * (DVD-Video bridge format) automount as UDF.  Likewise 9660/HFS
 * will automount as HFS.
 *
 * XXX CSM - externalize the dependencies! (ala kext .xml files).
 */

int
sortfs(const void *v1, const void *v2)
{
    char *s1 = &(*(struct dirent **)v1)->d_name[0];
    char *s2 = &(*(struct dirent **)v2)->d_name[0];
    char udf[16] = FS_TYPE_UDF;
    char iso[16] = FS_TYPE_CD9660;

    strcat(udf, FS_DIR_SUFFIX);
    strcat(iso, FS_DIR_SUFFIX);

    /* force udf to be first and 9660 to be last */
    if (!strcmp(s1, udf) || !strcmp(s2, iso))
        return (-1);
    if (!strcmp(s2, udf) || !strcmp(s1, iso))
        return (1);
    return (0);
}


boolean_t DiskVolume_mount_ufs(DiskVolumePtr diskVolume)
{
    struct ufs_args 	args;
    int 		mntflags = 0;
    char 		specname[MAXNAMELEN];

    sprintf(specname, "/dev/%s", diskVolume->disk_dev_name);
    args.fspec = specname;		/* The name of the device file. */

    if (diskVolume->writable == FALSE)
    {
                mntflags |= MNT_RDONLY;
    }

    if (diskVolume->removable)
    {
                mntflags |= MNT_NOSUID | MNT_NODEV;
    }
#define DEFAULT_ROOTUID	-2
    args.export.ex_root = DEFAULT_ROOTUID;

    if (mntflags & MNT_RDONLY)
    {
                args.export.ex_flags = MNT_EXRDONLY;
    }
    else
    {
                args.export.ex_flags = 0;
    }

    if (mount(FS_TYPE_UFS, diskVolume->mount_point, mntflags, &args) < 0)
    {
                pwarning(("mount %s on %s failed: %s\n", specname, diskVolume->mount_point, strerror(errno)));
                return (FALSE);
    }

    pwarning(("Mounted ufs %s on %s\n", specname, diskVolume->mount_point));

    DiskVolume_setMounted(diskVolume,TRUE);

    return (TRUE);
}

char *fsDirForFS(char *fsname) {
    char *           fsDir       = malloc(MAXPATHLEN);
    sprintf(fsDir, "%s/%s%s", FS_DIR_LOCATION, fsname, FS_DIR_SUFFIX);
    return fsDir;

}

char *utilPathForFS(char *fsname) {

    char fsDir[MAXPATHLEN];
    char *execPath 		= malloc(MAXPATHLEN);
    sprintf(fsDir, "%s/%s%s", FS_DIR_LOCATION, fsname, FS_DIR_SUFFIX);
    sprintf(execPath, "%s/%s%s", fsDir, fsname, FS_UTIL_SUFFIX);
    return execPath;
}

/* renameUFSDevice */

int renameUFSDevice(const char *devName, const char *mountPoint)
{
    int result;
    char *cmd = "-n";
    char *fsName = "ufs";
    char *execPath = utilPathForFS((char *)fsName);
    const char *childArgv[] = {	execPath,
            cmd,
                                devName,
                                mountPoint, 0};
#warning .util code here - needs to change for pluggable
    char *fsDir = fsDirForFS((char *)fsName);
    int pid;

    dwarning(("%s('%s', '%s')\n",
                        __FUNCTION__, devName, mountPoint));

    if ((pid = fork()) == 0)
    {
                /* CHILD PROCESS */

                cleanUpAfterFork();
                chdir(fsDir);
                execve(execPath, childArgv, 0);
                exit(-127);
    }
    else if (pid > 0)
    {
                int statusp;
                int waitResult;

                /* PARENT PROCESS */

                dwarning(("wait4(pid=%d,&statusp,0,NULL)...\n", pid));
                waitResult = wait4(pid,&statusp,0,NULL);
                dwarning(("wait4(pid=%d,&statusp,0,NULL) => %d\n", pid, waitResult));
                if (waitResult > 0)
                {
                        if (WIFEXITED(statusp))
                        {
                                result = (int)(char)(WEXITSTATUS(statusp));
                                goto Return;
                        }
                }
    }


    result = 0;

Return:
    free(execPath);
    free(fsDir);
    dwarning(("%s(...) => %d\n", __FUNCTION__, result));
    return result;
} /* renameUFSDevice */

char *fsNameForFSWithMediaName(char *fsname, char *mediaName)
{
    char *name 		= malloc(MAXPATHLEN);
    if (strcmp(fsname, FS_TYPE_UFS) == 0)
    {
        sprintf(name, "%s", mediaName); /* UFS has old-style label */
    } else {
        sprintf(name, "%s", fsname);
    }
return name;
   
}

int
suffixfs(struct dirent *dp)
{
    char *s;
    if (s = strstr(&dp->d_name[0], FS_DIR_SUFFIX))
        if (strlen(s) == strlen(FS_DIR_SUFFIX))
            return (1);
    return (0);
}

void cacheFileSystemDictionaries()
{

    if (!plistDict) {

        struct dirent **fsdirs = NULL;
        int	nfs = 0; /* # filesystems defined in /usr/filesystems */
        int n;

       plistDict =  CFDictionaryCreateMutable(NULL,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);

        /* discover known filesystem types */
        nfs = scandir(FS_DIR_LOCATION, &fsdirs, suffixfs, sortfs);

        if ( gDebug ) {
                dwarning(("%d filesystems known:\n", nfs));
        }
        for (n=0; n<nfs; n++)
        {
            char buf[MAXPATHLEN];
            CFDictionaryRef fsdict;
            CFStringRef str;
            CFURLRef bar;
            CFStringRef zaz;
            
            if ( gDebug ) {
                dwarning(("%s\n", &fsdirs[n]->d_name[0]));
            }

            sprintf(buf, "%s/%s", FS_DIR_LOCATION ,&fsdirs[n]->d_name[0]);
            // get their dictionaries, test that they are okay and add them into the plistDict

            str = CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);
            bar = CFURLCreateWithFileSystemPath(NULL, str, kCFURLPOSIXPathStyle, 1);

            fsdict = CFBundleCopyInfoDictionaryInDirectory(bar);

            zaz = CFStringCreateWithCString(NULL, &fsdirs[n]->d_name[0], kCFStringEncodingUTF8);

            CFDictionaryAddValue(plistDict, zaz, fsdict);

            CFRelease(zaz);
            CFRelease(bar);
            CFRelease(str);
            CFRelease(fsdict);
            
        }
    }
}

CFComparisonResult compareDicts(const void *val1, const void *val2, void *context)
{
    int val1ProbeOrder;
    int val2ProbeOrder;
    
    CFNumberRef val1Number = CFDictionaryGetValue(val1, CFSTR(kFSProbeOrderKey));
    CFNumberRef val2Number = CFDictionaryGetValue(val2, CFSTR(kFSProbeOrderKey));

    CFNumberGetValue(val1Number, kCFNumberIntType, &val1ProbeOrder);
    CFNumberGetValue(val2Number, kCFNumberIntType, &val2ProbeOrder);

    // printf("%d, %d\n", val1ProbeOrder, val2ProbeOrder);

    if (val1ProbeOrder > val2ProbeOrder) {
        return kCFCompareGreaterThan;
    } else if (val1ProbeOrder < val2ProbeOrder) {
        return kCFCompareLessThan;
    }
    
    return kCFCompareEqualTo;
}

void cacheFileSystemMatchingArray()
{
    if (!matchingArray) {

        struct dirent **fsdirs = NULL;
        int	nfs = 0; /* # filesystems defined in /usr/filesystems */
        int n;
        int i = 0;

        matchingArray = CFArrayCreateMutable(NULL, 0, NULL);

        /* discover known filesystem types */
        nfs = scandir(FS_DIR_LOCATION, &fsdirs, suffixfs, sortfs);

        for (n=0; n<nfs; n++)
        {
            char buf[MAXPATHLEN];
            CFDictionaryRef fsdict;
            CFDictionaryRef mediaTypeDict;
            CFStringRef str;
            CFURLRef bar;

            sprintf(buf, "%s/%s", FS_DIR_LOCATION ,&fsdirs[n]->d_name[0]);
            // get their dictionaries, test that they are okay and add them into the plistDict

            str = CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);
            bar = CFURLCreateWithFileSystemPath(NULL, str, kCFURLPOSIXPathStyle, 1);

            fsdict = CFBundleCopyInfoDictionaryInDirectory(bar);

            mediaTypeDict = CFDictionaryGetValue(fsdict, CFSTR(kFSMediaTypesKey));


            {
                int j = CFDictionaryGetCount(mediaTypeDict);
                CFDictionaryRef		dicts[j];
                CFStringRef		keys[j];
                CFDictionaryGetKeysAndValues(mediaTypeDict,(void**)keys,(void**)dicts);

                for (i=0;i<j;i++) {
                    CFStringRef zaz;
                    
                    CFMutableDictionaryRef newDict = CFDictionaryCreateMutableCopy(NULL,0,dicts[i]);
                    zaz = CFStringCreateWithCString(NULL, &fsdirs[n]->d_name[0], kCFStringEncodingUTF8);
                    CFDictionaryAddValue(newDict, CFSTR("FSName"), zaz);
                    CFArrayAppendValue(matchingArray, newDict);
                    CFRelease(zaz);
                }
                
            }
            CFRelease(fsdict);
            CFRelease(str);
            CFRelease(bar);

        }
        CFArraySortValues(matchingArray, CFRangeMake(0, CFArrayGetCount(matchingArray)), compareDicts, NULL);
    }
}

char *resourcePathForFSName(char *fs)
{
    char bundlePath[MAXPATHLEN];
    CFBundleRef bundle;
    CFURLRef bundleUrl;
    CFURLRef resourceUrl;
    CFStringRef resourceString;
    char *path;
    char *resourcePath = malloc(MAXPATHLEN);
    CFStringRef str;

    sprintf(bundlePath, "%s/%s", FS_DIR_LOCATION, fs);

    str = CFStringCreateWithCString(NULL, bundlePath, kCFStringEncodingMacRoman);

    bundleUrl = CFURLCreateWithFileSystemPath(NULL, str,kCFURLPOSIXPathStyle ,1);
    CFRelease(str);
    bundle = CFBundleCreate(NULL, bundleUrl);
    resourceUrl = CFBundleCopyResourcesDirectoryURL(bundle);
    resourceString = CFURLCopyPath(resourceUrl);

    path = daCreateCStringFromCFString(resourceString);

    sprintf(resourcePath, "%s/%s", bundlePath, path);

    CFRelease(bundleUrl);
    CFRelease(bundle);
    CFRelease(resourceUrl);
    CFRelease(resourceString);
    free(path);

    return resourcePath;
}

char *verifyPathForFileSystem(char *fsname)
{
    CFDictionaryRef fsDict;
    CFDictionaryRef personalities;
    CFDictionaryRef personality;
    CFStringRef fsckPath1;
    char fs[128];
    char *fsckPath;
    char *finalPath = malloc(MAXPATHLEN);
    CFStringRef str;
    
    if (strlen(fsname) == 0) {
        return finalPath;
    }

    sprintf(fs, "%s%s", fsname, FS_DIR_SUFFIX);
    str = CFStringCreateWithCString(NULL, fs, kCFStringEncodingMacRoman);
    fsDict = (CFDictionaryRef)CFDictionaryGetValue(plistDict, str);
    CFRelease(str);

    if (!fsDict) {
        return finalPath;
    }
    
    personalities = (CFDictionaryRef)CFDictionaryGetValue(fsDict, CFSTR(kFSPersonalitiesKey));

    {
        CFDictionaryRef		dicts[CFDictionaryGetCount(personalities)];
        CFStringRef		keys[CFDictionaryGetCount(personalities)];
        CFDictionaryGetKeysAndValues(personalities,(void**)keys,(void**)dicts);
        personality = (CFDictionaryRef)dicts[0]; //(CFDictionaryRef)CFArrayGetValueAtIndex(personalities, 0);

    }

    fsckPath1 = (CFStringRef)CFDictionaryGetValue(personality, CFSTR(kFSVerificationExecutableKey));

    if (fsckPath1) {

        char *resourcePath = resourcePathForFSName(fs);
        fsckPath = daCreateCStringFromCFString(fsckPath1);

        sprintf(finalPath, "%s%s", resourcePath, fsckPath);

        free(resourcePath);
        free(fsckPath);
    }
    return finalPath;
}

char *repairPathForFileSystem(char *fsname)
{
    CFDictionaryRef fsDict;
    CFDictionaryRef personalities;
    CFDictionaryRef personality;
    CFStringRef fsckPath1;
    char fs[128];
    char *fsckPath;
    char *finalPath = malloc(MAXPATHLEN);
    CFStringRef str;

    if (strlen(fsname) == 0) {
        return finalPath;
    }

    sprintf(fs, "%s%s", fsname, FS_DIR_SUFFIX);
    str = CFStringCreateWithCString(NULL, fs, kCFStringEncodingMacRoman);
    fsDict = (CFDictionaryRef)CFDictionaryGetValue(plistDict, str);
    CFRelease(str);

    if (!fsDict) {
        return finalPath;
    }

    personalities = (CFDictionaryRef)CFDictionaryGetValue(fsDict, CFSTR(kFSPersonalitiesKey));

    {
        CFDictionaryRef		dicts[CFDictionaryGetCount(personalities)];
        CFStringRef		keys[CFDictionaryGetCount(personalities)];
        CFDictionaryGetKeysAndValues(personalities,(void**)keys,(void**)dicts);
        personality = (CFDictionaryRef)dicts[0]; //(CFDictionaryRef)CFArrayGetValueAtIndex(personalities, 0);

    }

    fsckPath1 = (CFStringRef)CFDictionaryGetValue(personality, CFSTR(kFSRepairExecutableKey));

    if (fsckPath1) {
        char *resourcePath = resourcePathForFSName(fs);
        fsckPath = daCreateCStringFromCFString(fsckPath1);

        sprintf(finalPath, "%s%s", resourcePath, fsckPath);

        free(resourcePath);
        free(fsckPath);
    }
    return finalPath;

}

char *verifyArgsForFileSystem(char *fsname)
{
    CFDictionaryRef fsDict;
    CFDictionaryRef personalities;
    CFDictionaryRef personality;
    CFStringRef repairArgs1;
    char fs[128];
    char *repairArgs;
    CFStringRef str;

    if (strlen(fsname) == 0) {
        repairArgs = malloc(MAXPATHLEN);
        return repairArgs;
    }

    sprintf(fs, "%s%s", fsname, FS_DIR_SUFFIX);
    str = CFStringCreateWithCString(NULL, fs, kCFStringEncodingMacRoman);
    fsDict = (CFDictionaryRef)CFDictionaryGetValue(plistDict, str);
    CFRelease(str);

    if (!fsDict) {
        repairArgs = malloc(MAXPATHLEN);
        return repairArgs;
    }

    personalities = (CFDictionaryRef)CFDictionaryGetValue(fsDict, CFSTR(kFSPersonalitiesKey));

    {
        CFDictionaryRef		dicts[CFDictionaryGetCount(personalities)];
        CFStringRef		keys[CFDictionaryGetCount(personalities)];
        CFDictionaryGetKeysAndValues(personalities,(void**)keys,(void**)dicts);
        personality = (CFDictionaryRef)dicts[0]; //(CFDictionaryRef)CFArrayGetValueAtIndex(personalities, 0);

    }

    repairArgs1 = (CFStringRef)CFDictionaryGetValue(personality, CFSTR(kFSVerificationArgumentsKey));

    if (repairArgs1) {
        repairArgs = daCreateCStringFromCFString(repairArgs1);
    } else {
        repairArgs = malloc(MAXPATHLEN);
    }


    return repairArgs;

}

char *repairArgsForFileSystem(char *fsname)
{
    CFDictionaryRef fsDict;
    CFDictionaryRef personalities;
    CFDictionaryRef personality;
    CFStringRef repairArgs1;
    char fs[128];
    char *repairArgs;
    CFStringRef str;

    if (strlen(fsname) == 0) {
        repairArgs = malloc(MAXPATHLEN);
        return repairArgs;
    }

    sprintf(fs, "%s%s", fsname, FS_DIR_SUFFIX);
    str = CFStringCreateWithCString(NULL, fs, kCFStringEncodingMacRoman);
    fsDict = (CFDictionaryRef)CFDictionaryGetValue(plistDict, str);
    CFRelease(str);

    if (!fsDict) {
        repairArgs = malloc(MAXPATHLEN);
        return repairArgs;
    }

    personalities = (CFDictionaryRef)CFDictionaryGetValue(fsDict, CFSTR(kFSPersonalitiesKey));

    {
        CFDictionaryRef		dicts[CFDictionaryGetCount(personalities)];
        CFStringRef		keys[CFDictionaryGetCount(personalities)];
        CFDictionaryGetKeysAndValues(personalities,(void**)keys,(void**)dicts);
        personality = (CFDictionaryRef)dicts[0]; //(CFDictionaryRef)CFArrayGetValueAtIndex(personalities, 0);

    }

    repairArgs1 = (CFStringRef)CFDictionaryGetValue(personality, CFSTR(kFSRepairArgumentsKey));

    if (repairArgs1) {
        repairArgs = daCreateCStringFromCFString(repairArgs1);
    } else {
        repairArgs = malloc(MAXPATHLEN);
    }


   return repairArgs;

}

void printArgsForFsname(char *fsname)
{
    printf("arguments for %s\n\tVerify = %s %s\n\tRepair = %s %s\n", fsname, verifyPathForFileSystem(fsname), verifyArgsForFileSystem(fsname), repairPathForFileSystem(fsname), repairArgsForFileSystem(fsname));
    
}