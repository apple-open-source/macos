/*
 * Copyright (c) 1998-2009 Apple Inc. All Rights Reserved.
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

#include <sys/types.h>
#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitrationPrivate.h>
#include <IOKit/IOKitLib.h>

#include <fcntl.h>
#include <dirent.h>
#include <fsproperties.h>
#include <paths.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/loadable_fs.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ttycom.h>
#include <sys/wait.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOMedia.h>

#define dwarning(a) { if (g.debug) { printf a; fflush(stdout); } }
#define pwarning(a) { printf a; fflush(stdout); }

mach_port_t ioMasterPort;

CFMutableDictionaryRef plistDict = nil;
CFMutableArrayRef matchingArray = nil;

struct Disk {
        struct Disk *		next;
        char *			ioBSDName;
        unsigned		flags;
        io_object_t		service;
        UInt64			ioSize;
};

typedef struct Disk Disk;
typedef struct Disk * DiskPtr;

typedef struct {
    boolean_t	verbose;
    boolean_t	debug;
    DiskPtr	Disks;
    unsigned	NumDisks;
} GlobalStruct;

GlobalStruct g;

struct DiskVolume
{
    char *		fs_type;
    char *		disk_dev_name;
    char *		disk_name;
    boolean_t		removable;
    boolean_t		writable;
    boolean_t		internal;
    boolean_t		dirty;
    boolean_t		mounted;
    UInt64		size;
};

typedef struct DiskVolume DiskVolume, *DiskVolumePtr;

struct DiskVolumes
{
    CFMutableArrayRef list;
};
typedef struct DiskVolumes DiskVolumes, *DiskVolumesPtr;


char           *
daCreateCStringFromCFString(CFStringRef string)
{
	/*
         * result of daCreateCStringFromCFString should be released with free()
         */

	CFStringEncoding encoding = kCFStringEncodingMacRoman;
	CFIndex         bufferLength = CFStringGetLength(string) + 1;
	char           *buffer = malloc(bufferLength);

	if (buffer) {
		if (CFStringGetCString(string, buffer, bufferLength, encoding) == FALSE) {
			free(buffer);
			buffer = malloc(1);
			//See Radar 2457357.
				buffer[0] = '\0';
			//See Radar 2457357.
		}
	}
	return buffer;
}				/* daCreateCStringFromCFString */

char           *
fsDirForFS(char *fsname)
{
	char           *fsDir = malloc(MAXPATHLEN);
	sprintf(fsDir, "%s/%s%s", FS_DIR_LOCATION, fsname, FS_DIR_SUFFIX);
	return fsDir;

}

int
suffixfs(struct dirent * dp)
{
	char           *s;
	if ((s = strstr(&dp->d_name[0], FS_DIR_SUFFIX)))
		if (strlen(s) == strlen(FS_DIR_SUFFIX))
			return (1);
	return (0);
}

void 
cacheFileSystemDictionaries()
{

	if (!plistDict) {

		struct dirent **fsdirs = NULL;
		int             nfs = 0;	/* # filesystems defined in
						 * /usr/filesystems */
		int             n;

		plistDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

		/* discover known filesystem types */
		nfs = scandir(FS_DIR_LOCATION, &fsdirs, suffixfs, NULL);

		dwarning(("%d filesystems known:\n", nfs));
		for (n = 0; n < nfs; n++) {
			char            buf[MAXPATHLEN];
			CFDictionaryRef fsdict;
			CFStringRef     str;
			CFURLRef        bar;
			CFStringRef     zaz;

			dwarning(("%s\n", &fsdirs[n]->d_name[0]));
			sprintf(buf, "%s/%s", FS_DIR_LOCATION, &fsdirs[n]->d_name[0]);
			//get their dictionaries, test that they are okay and add them into the plistDict

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
		if (fsdirs) {
			for (n = 0; n < nfs; n++) {
				free((void *) fsdirs[n]);
			}
			free((void *) fsdirs);
		}
	}
}

CFComparisonResult 
compareDicts(const void *val1, const void *val2, void *context)
{
	int             val1ProbeOrder;
	int             val2ProbeOrder;

	CFNumberRef     val1Number = CFDictionaryGetValue(val1, CFSTR(kFSProbeOrderKey));
	CFNumberRef     val2Number = CFDictionaryGetValue(val2, CFSTR(kFSProbeOrderKey));

	CFNumberGetValue(val1Number, kCFNumberIntType, &val1ProbeOrder);
	CFNumberGetValue(val2Number, kCFNumberIntType, &val2ProbeOrder);

	//printf("%d, %d\n", val1ProbeOrder, val2ProbeOrder);

	if (val1ProbeOrder > val2ProbeOrder) {
		return kCFCompareGreaterThan;
	} else if (val1ProbeOrder < val2ProbeOrder) {
		return kCFCompareLessThan;
	}
	return kCFCompareEqualTo;
}

void 
cacheFileSystemMatchingArray()
{
	if (!matchingArray) {

		struct dirent **fsdirs = NULL;
		int             nfs = 0;	/* # filesystems defined in
						 * /usr/filesystems */
		int             n;
		int             i = 0;

		matchingArray = CFArrayCreateMutable(NULL, 0, NULL);

		/* discover known filesystem types */
		nfs = scandir(FS_DIR_LOCATION, &fsdirs, suffixfs, NULL);

		for (n = 0; n < nfs; n++) {
			char            buf[MAXPATHLEN];
			CFDictionaryRef fsdict;
			CFDictionaryRef mediaTypeDict;
			CFStringRef     str;
			CFURLRef        bar;

			sprintf(buf, "%s/%s", FS_DIR_LOCATION, &fsdirs[n]->d_name[0]);
			//get their dictionaries, test that they are okay and add them into the plistDict

				str = CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);
			bar = CFURLCreateWithFileSystemPath(NULL, str, kCFURLPOSIXPathStyle, 1);

			fsdict = CFBundleCopyInfoDictionaryInDirectory(bar);

			mediaTypeDict = CFDictionaryGetValue(fsdict, CFSTR(kFSMediaTypesKey));


			if (mediaTypeDict != NULL) {
				int             j = CFDictionaryGetCount(mediaTypeDict);
				CFDictionaryRef *dicts = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef) * j);
				CFDictionaryGetKeysAndValues(mediaTypeDict, NULL, (const void **) dicts);

				for (i = 0; i < j; i++) {
					CFStringRef     zaz;

					CFMutableDictionaryRef newDict = CFDictionaryCreateMutableCopy(NULL, 0, dicts[i]);
					zaz = CFStringCreateWithCString(NULL, &fsdirs[n]->d_name[0], kCFStringEncodingUTF8);
					CFDictionaryAddValue(newDict, CFSTR("FSName"), zaz);
					CFArrayAppendValue(matchingArray, newDict);
					CFRelease(zaz);
				}
				free(dicts);

			}
			CFRelease(fsdict);
			CFRelease(str);
			CFRelease(bar);

		}
		if (fsdirs) {
			for (n = 0; n < nfs; n++) {
				free((void *) fsdirs[n]);
			}
			free((void *) fsdirs);
		}
		CFArraySortValues(matchingArray, CFRangeMake(0, CFArrayGetCount(matchingArray)), compareDicts, NULL);
	}
}

char           *
resourcePathForFSName(char *fs)
{
	char            bundlePath[MAXPATHLEN];
	CFBundleRef     bundle;
	CFURLRef        bundleUrl;
	CFURLRef        resourceUrl;
	CFStringRef     resourceString;
	char           *path;
	char           *resourcePath = malloc(MAXPATHLEN);
	CFStringRef     str;

	sprintf(bundlePath, "%s/%s", FS_DIR_LOCATION, fs);

	str = CFStringCreateWithCString(NULL, bundlePath, kCFStringEncodingMacRoman);

	bundleUrl = CFURLCreateWithFileSystemPath(NULL, str, kCFURLPOSIXPathStyle, 1);
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

char           *
repairPathForFileSystem(char *fsname)
{
	CFDictionaryRef fsDict;
	CFDictionaryRef personalities;
	CFDictionaryRef personality;
	CFStringRef     fsckPath1;
	char            fs[128];
	char           *fsckPath;
	char           *finalPath = malloc(MAXPATHLEN);
	CFStringRef     str;

	if (strlen(fsname) == 0) {
		return finalPath;
	}
	sprintf(fs, "%s%s", fsname, FS_DIR_SUFFIX);
	str = CFStringCreateWithCString(NULL, fs, kCFStringEncodingMacRoman);
	fsDict = (CFDictionaryRef) CFDictionaryGetValue(plistDict, str);
	CFRelease(str);

	if (!fsDict) {
		return finalPath;
	}
	personalities = (CFDictionaryRef) CFDictionaryGetValue(fsDict, CFSTR(kFSPersonalitiesKey));

	{
		int persCount = CFDictionaryGetCount(personalities);
		CFDictionaryRef *dicts = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef) * persCount);
		CFDictionaryGetKeysAndValues(personalities, NULL, (const void **) dicts);
		personality = (CFDictionaryRef) dicts[0];
		free(dicts);
		//(CFDictionaryRef) CFArrayGetValueAtIndex(personalities, 0);

	}

	fsckPath1 = (CFStringRef) CFDictionaryGetValue(personality, CFSTR(kFSRepairExecutableKey));

	if (fsckPath1) {
		char           *resourcePath = resourcePathForFSName(fs);
		fsckPath = daCreateCStringFromCFString(fsckPath1);

		sprintf(finalPath, "%s%s", resourcePath, fsckPath);

		free(resourcePath);
		free(fsckPath);
	}
	return finalPath;

}

char           *
repairArgsForFileSystem(char *fsname)
{
	CFDictionaryRef fsDict;
	CFDictionaryRef personalities;
	CFDictionaryRef personality;
	CFStringRef     repairArgs1;
	char            fs[128];
	char           *repairArgs;
	CFStringRef     str;

	if (strlen(fsname) == 0) {
		repairArgs = malloc(MAXPATHLEN);
		return repairArgs;
	}
	sprintf(fs, "%s%s", fsname, FS_DIR_SUFFIX);
	str = CFStringCreateWithCString(NULL, fs, kCFStringEncodingMacRoman);
	fsDict = (CFDictionaryRef) CFDictionaryGetValue(plistDict, str);
	CFRelease(str);

	if (!fsDict) {
		repairArgs = malloc(MAXPATHLEN);
		return repairArgs;
	}
	personalities = (CFDictionaryRef) CFDictionaryGetValue(fsDict, CFSTR(kFSPersonalitiesKey));

	{
		int persCount = CFDictionaryGetCount(personalities);
		CFDictionaryRef *dicts = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef) * persCount);
		CFDictionaryGetKeysAndValues(personalities, NULL, (const void **) dicts);
		personality = (CFDictionaryRef) dicts[0];
		free(dicts);
		//(CFDictionaryRef) CFArrayGetValueAtIndex(personalities, 0);

	}

	repairArgs1 = (CFStringRef) CFDictionaryGetValue(personality, CFSTR(kFSRepairArgumentsKey));

	if (repairArgs1) {
		repairArgs = daCreateCStringFromCFString(repairArgs1);
	} else {
		repairArgs = malloc(MAXPATHLEN);
	}


	return repairArgs;

}

#define PIPEFULL	(4 * 1024)
static char *
read_output(int fd)
{
	char *		buf = NULL;
	ssize_t 	count;
	ssize_t		where = 0;

	buf = malloc(PIPEFULL);
	if (buf == NULL) {
		return (NULL);
	}

	/* this handles up to PIPEFULL - 1 bytes */
	while (where < (PIPEFULL - 1)
	       && (count = read(fd, buf + where, PIPEFULL - 1 - where))) {
	    if (count == -1) {
		free(buf);
		return (NULL);
	    }
	    where += count;
	}
	buf[where] = '\0';
	return (buf);
}

void 
cleanUpAfterFork(int fdp[])
{
    int fd, maxfd = getdtablesize();

        /* Close all inherited file descriptors */

    for (fd = 0; fd < maxfd; fd++)
    {
	    if (fdp == NULL || (fdp[0] != fd && fdp[1] != fd)) {
		    close(fd);
	    }
    }

        /* Disassociate ourselves from any controlling tty */

    if ((fd = open("/dev/tty", O_NDELAY)) >= 0)
    {
                ioctl(fd, TIOCNOTTY, 0);
                close(fd);
    }

    /* Reset the user and group id's to their real values */

    setgid(getgid());
    setuid(getuid());

    (void)setsid();
    
    (void)chdir("/");

    if ((fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
	    /* point stdin -> /dev/null */
	    (void)dup2(fd, STDIN_FILENO);
	    if (fdp != NULL) {
		    /* point stdout to one end of pipe fdp[1] */
		    (void)dup2(fdp[1], STDOUT_FILENO);

		    /* close the other end */
		    close(fdp[0]);
	    }
	    else {
		    (void)dup2(fd, STDOUT_FILENO);
	    }
	    (void)dup2(fd, STDERR_FILENO);
	    if (fd > 2)
		    (void)close (fd);
    }
    return;
}

boolean_t
do_exec(const char * dir, const char * argv[], int * result,
	char * * output)
{
	int 		fdp[2];
	boolean_t	got_result = FALSE;
	int 		pid;

	if (g.debug) {
		const char * * scan;
		printf("do_exec(");
		for (scan = argv; *scan; scan++) {
			printf("%s%s", (scan != argv) ? " " : "", *scan);
		}
		printf(")\n");
	}
	if (output != NULL) {
		*output = NULL;
		if (pipe(fdp) == -1) {
			pwarning(("do_exec(): pipe() failed, %s", 
					strerror(errno)));
			return (FALSE);
		}
	}
	if (access(argv[0], F_OK) == 0) {
		pid = fork();
		if (pid == 0) {
			/* CHILD PROCESS */
			if (output == NULL)
				cleanUpAfterFork(NULL);
			else
				cleanUpAfterFork(fdp);

			if (dir) {
				chdir(dir);
			}
			execve(argv[0], (char * const *)argv, 0);
			exit(-127);
		}
		else if (pid > 0) {  /* PARENT PROCESS */
			int statusp;
			int waitResult;
			
			if (output != NULL) {
				close(fdp[1]);
				*output = read_output(fdp[0]);
                                close(fdp[0]);
			}
			dwarning(("wait4(pid=%d,&statusp,0,NULL)...\n", pid));
			waitResult = wait4(pid,&statusp,0,NULL);
			dwarning(("wait4(pid=%d,&statusp,0,NULL) => %d\n", 
				  pid, waitResult));
			if (waitResult > 0
			    && WIFEXITED(statusp)) {
				got_result = TRUE;
				*result = (int)(char)(WEXITSTATUS(statusp));
			}
		}
		else {
			pwarning(("do_exec: fork() failed, %s",
					strerror(errno)));
		}
	}
	return (got_result);
}

DiskPtr NewDisk(	char * ioBSDName,
                                        io_object_t	service,
					unsigned flags,
					UInt64 ioSize)
{
	DiskPtr result;
	
	dwarning(("%s(ioBSDName = '%s', flags = $%08x)\n",
				__FUNCTION__,
				ioBSDName,
           flags ));

	/* Allocate memory */

	result = (DiskPtr) malloc( sizeof( * result ) );
	if ( result == NULL )
	{
		dwarning(("%s(...): malloc failed!\n", __FUNCTION__));
		/* result = NULL; */
		goto Return;
	}

	bzero( result, sizeof( * result ) );

	/* Link it onto the front of the list */

	result->next = g.Disks;
	g.Disks = result;

	/* Fill in the fields */

	result->ioBSDName = strdup( ioBSDName ? ioBSDName : "" );
        result->service = service;
	   result->ioSize = ioSize;

	result->flags = flags;

	/* Increment count */

	g.NumDisks ++ ;

	/* Retain service */

	if ( service )
	{
		IOObjectRetain( service );
	}

Return:
	return result;

} // NewDisk

static struct statfs *
get_fsstat_list(int * number)
{
    int n;
    struct statfs * stat_p;

    n = getfsstat(NULL, 0, MNT_NOWAIT);
    if (n <= 0)
    {
		return (NULL);
    }

    stat_p = (struct statfs *)malloc(n * sizeof(*stat_p));
    if (stat_p == NULL)
    {
		return (NULL);
    }

    if (getfsstat(stat_p, n * sizeof(*stat_p), MNT_NOWAIT) <= 0)
    {
		free(stat_p);
		return (NULL);
    }
    *number = n;

    return (stat_p);
}

static struct statfs *
fsstat_lookup_spec(struct statfs * list_p, int n, dev_t dev, char * fstype)
{
    int 				i;
    struct statfs * 	scan;

    for (i = 0, scan = list_p; i < n; i++, scan++)
    {
		if (strcmp(scan->f_fstypename, fstype) == 0
		    && scan->f_fsid.val[0] == dev) {
		    return (scan);
		}
    }
    return (NULL);
}

boolean_t
fsck_needed(char * devname, char * fstype)
{
    const char * argv[] = {
	NULL,
	"-q",
	NULL,
	NULL,
    };
    char 	devpath[64];
    int result;
    char *fsckCmd 	= repairPathForFileSystem(fstype);

    snprintf(devpath, sizeof(devpath), "/dev/r%s", devname);
    argv[0] = fsckCmd;
    argv[2]= devpath;
    if (do_exec(NULL, argv, &result, NULL) == FALSE) {
	result = -1;
    }
    dwarning(("%s('%s'): '%s' => %d\n", __FUNCTION__, devname, fsckCmd,
	      result));
    free(fsckCmd);

    if (result <= 0) {
	return (FALSE);
    }
    return (TRUE);
}

/* foreignProbe: run the -p(robe) option of the given <fsName>.util program in a child process */
/* returns the volume name in volname_p */

static int
foreignProbe(const char *fsName, const char *execPath, const char *probeCmd, const char *devName, int removable, int writable, char * * volname_p)
{
    int result;
    const char *childArgv[] = {	execPath,
                                probeCmd,
                                devName,
                                removable ? DEVICE_REMOVABLE : DEVICE_FIXED,
                                writable? DEVICE_WRITABLE : DEVICE_READONLY,
                                0 };
    char *fsDir = fsDirForFS((char *)fsName);

    dwarning(("%s('%s', '%s', removable=%d, writable=%d):\n'%s %s %s %s %s'\n",
		__FUNCTION__, fsName, devName, removable, writable, execPath, childArgv[1], childArgv[2], childArgv[3], childArgv[4]));


    if (do_exec(fsDir, childArgv, &result, volname_p) == FALSE) {
        result = FSUR_IO_FAIL;
    }
    dwarning(("%s(...) => %d\n", __FUNCTION__, result));
    free(fsDir);
    return result;
}

void setVar(char **var,char *val)
{
    if (*var)
    {
		free(*var);
    }
    if (val == NULL)
    {
		*var = NULL;
    }
    else 
    {
		*var = strdup(val);
    }

}
void DiskVolume_setFSType(DiskVolumePtr diskVolume,char *t)
{
    setVar(&(diskVolume->fs_type),t);
}
void DiskVolume_setDiskName(DiskVolumePtr diskVolume,char *t)
{
    setVar(&(diskVolume->disk_name),t);
}
void DiskVolume_setDiskDevName(DiskVolumePtr diskVolume,char *t)
{
    setVar(&(diskVolume->disk_dev_name),t);
}
void DiskVolume_setMounted(DiskVolumePtr diskVolume,boolean_t val)
{
        diskVolume->mounted = val;
}

void DiskVolume_setWritable(DiskVolumePtr diskVolume,boolean_t val)
{
    diskVolume->writable = val;
}
void DiskVolume_setRemovable(DiskVolumePtr diskVolume,boolean_t val)
{
    diskVolume->removable = val;
}
void DiskVolume_setInternal(DiskVolumePtr diskVolume,boolean_t val)
{
    diskVolume->internal = val;
}
void DiskVolume_setDirtyFS(DiskVolumePtr diskVolume,boolean_t val)
{
    diskVolume->dirty = val;
}
void DiskVolume_new(DiskVolumePtr *diskVolume)
{
    *diskVolume = malloc(sizeof(DiskVolume));
    (*diskVolume)->fs_type = nil;
    (*diskVolume)->disk_dev_name = nil;
    (*diskVolume)->disk_name = nil;
}
void DiskVolume_delete(DiskVolumePtr diskVolume)
{
    int                 i;
    char * *    l[] = { &(diskVolume->fs_type),
                        &(diskVolume->disk_dev_name),
                        &(diskVolume->disk_name),
                        NULL };


    if(!diskVolume)
        return;
        
    for (i = 0; l[i] != NULL; i++)
    {
                if (*(l[i]))
                {
                    free(*(l[i]));
                }
                *(l[i]) = NULL;
    }

    free(diskVolume);
}

static dev_t
dev_from_spec(const char * specName)
{
    struct stat sb;

    if (stat(specName, &sb)) {
	return (-1);
    }
    if (S_ISCHR(sb.st_mode) || S_ISBLK(sb.st_mode)) {
	return (sb.st_rdev);
    }
    return (-1);
}

#define MAXNAMELEN	256
DiskVolumePtr 
DiskVolumes_newVolume(DiskVolumesPtr diskList, DiskPtr media, boolean_t isRemovable,
		      boolean_t isWritable, boolean_t isInternal,
		      struct statfs * stat_p, int stat_number, UInt64 ioSize)
{
    char *			devname = media->ioBSDName;
    struct statfs *		fs_p;
    dev_t			fs_dev;
    char * 			fsname = NULL;
    int 			ret;
    char			specName[MAXNAMELEN];
    DiskVolumePtr 		volume = 0x0;
    int 			matchingPointer = 0;
    int 			count = CFArrayGetCount(matchingArray);

        for (matchingPointer = 0;matchingPointer < count;matchingPointer++) {

                // see if the diskPtr->service matches any of the filesystem types
                // if it does test that first
                // otherwise, start at the top of the list and test them alls
                int matches;
                CFDictionaryRef dictPointer = CFArrayGetValueAtIndex(matchingArray, matchingPointer);
                CFDictionaryRef mediaProps = CFDictionaryGetValue(dictPointer, CFSTR(kFSMediaPropertiesKey));
                kern_return_t error;

                error = IOServiceMatchPropertyTable(media->service, mediaProps, &matches);

                if (error) {
                    dwarning(("some kind of error while matching service to array... %d\n", error));
                }

                if (matches) {
                    CFStringRef utilArgsFromDict;
                    CFStringRef fsNameFromDict;
                    CFArrayRef fsNameArray;
                    CFStringRef utilPathFromDict;

                    char *utilPathFromDict2;
                    char *utilArgsFromDict2;
                    char *fsNameFromDict2;
                    char *fstype;
                    char *resourcePath;

                    char utilPath[MAXPATHLEN];

                    dwarning(("********We have a match for devname = %s!!!**********\n", devname));

                    utilArgsFromDict = CFDictionaryGetValue(dictPointer, CFSTR(kFSProbeArgumentsKey));
                    fsNameFromDict = CFDictionaryGetValue(dictPointer, CFSTR("FSName"));
                    fsNameArray = CFStringCreateArrayBySeparatingStrings(NULL, fsNameFromDict, CFSTR("."));
                    utilPathFromDict = CFDictionaryGetValue(dictPointer, CFSTR(kFSProbeExecutableKey));

                    utilPathFromDict2 = daCreateCStringFromCFString(utilPathFromDict);
                    utilArgsFromDict2 = daCreateCStringFromCFString(utilArgsFromDict);
                    fsNameFromDict2 = daCreateCStringFromCFString(fsNameFromDict);
                    fstype = daCreateCStringFromCFString(CFArrayGetValueAtIndex(fsNameArray, 0));
                    resourcePath = resourcePathForFSName(fsNameFromDict2);

                    sprintf(utilPath, "%s%s", resourcePath, utilPathFromDict2);

                    // clean up
                    CFRelease(fsNameArray);
                    free(utilPathFromDict2);
                    free(fsNameFromDict2);
                    free(resourcePath);

                    ret = foreignProbe(fstype, utilPath, utilArgsFromDict2, devname, isRemovable, isWritable, &fsname);

                    free(utilArgsFromDict2);

                    if (ret == FSUR_RECOGNIZED || ret == -9)
                    {
                        if (fsname == NULL) {
                            fsname = strdup(fstype);
                        }

                        DiskVolume_new(&volume);
                        DiskVolume_setDiskDevName(volume,devname);
                        DiskVolume_setFSType(volume,fstype);
                        DiskVolume_setDiskName(volume,fsname);
                        DiskVolume_setWritable(volume,isWritable);
                        DiskVolume_setRemovable(volume,isRemovable);
                        DiskVolume_setInternal(volume,isInternal);
                        DiskVolume_setMounted(volume,FALSE);
                        DiskVolume_setDirtyFS(volume,FALSE);
                        volume->size = ioSize;

			sprintf(specName,"/dev/%s",devname);
			fs_dev = dev_from_spec(specName);

                        fs_p = fsstat_lookup_spec(stat_p, stat_number, fs_dev, fstype);
                        if (fs_p)
                        {
                                /* already mounted */
                                DiskVolume_setMounted(volume,TRUE);
                        }
                        else if (isWritable)
                        {
                                DiskVolume_setDirtyFS(volume,fsck_needed(devname,fstype));
                        }
                        free(fstype);
                        if (fsname) 
                            free(fsname);
                        fsname = NULL;
                        break;
                    } else {
                        free(fstype);
                        if (fsname) 
                            free(fsname);
                        fsname = NULL;
                        dwarning(("Volume is bad\n"));
                        volume = 0x0;
                    }

                }
        }

    return volume;
}
void DiskVolumes_new(DiskVolumesPtr *diskList)
{
    *diskList = malloc(sizeof(DiskVolumes));
    (*diskList)->list = CFArrayCreateMutable(NULL,0,NULL);
}
void DiskVolumes_delete(DiskVolumesPtr diskList)
{
    int i;
    int count = CFArrayGetCount(diskList->list);
    if(!diskList)
        return;
    
    for (i = 0; i < count; i++)
    {
            DiskVolume_delete((DiskVolumePtr)CFArrayGetValueAtIndex(diskList->list,i));
    }
    
    CFArrayRemoveAllValues(diskList->list);

    CFRelease(diskList->list);

    free(diskList);
}

DiskVolumesPtr DiskVolumes_do_volumes(DiskVolumesPtr diskList)
{
	DiskPtr				diskPtr;
	boolean_t			success = FALSE;
	struct statfs *		stat_p;
	int					stat_number;
	int	nfs = 0; /* # filesystems defined in /usr/filesystems */
	struct dirent **fsdirs = NULL;
	int	n; /* iterator for nfs/fsdirs */
    
	stat_p = get_fsstat_list(&stat_number);
	if (stat_p == NULL || stat_number == 0)
	{
		goto Return;
	}

	/* discover known filesystem types */
	nfs = scandir(FS_DIR_LOCATION, &fsdirs, suffixfs, NULL);
	/*
	 * suffixfs ensured we have only names ending in ".fs"
	 * now we convert the periods to nulls to give us
	 * filesystem type strings.
	 */
	for (n = 0; n < nfs; n++)
	{
		*strrchr(&fsdirs[n]->d_name[0], '.') = '\0';
	}
	if ( g.debug ) {
		dwarning(("%d filesystems known:\n", nfs));
		for (n=0; n<nfs; n++)
		{
			dwarning(("%s\n", &fsdirs[n]->d_name[0]));
		}
	}

	for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next )
	{
		int isWritable, isRemovable, isInternal;
		DiskVolumePtr volume = 0x0;

		/* Initialize some convenient flags */
		
		isWritable = ( diskPtr->flags & kDiskArbDiskAppearedLockedMask ) == 0;
		isRemovable = ( diskPtr->flags & kDiskArbDiskAppearedEjectableMask ) != 0;
		isInternal = ( diskPtr->flags & kDiskArbDiskAppearedInternal ) != 0;

                if ((diskPtr->flags & kDiskArbDiskAppearedNoSizeMask) != 0) {
                    continue;  // if it's zero length, skip it
                };

                volume = DiskVolumes_newVolume(diskList,
					       diskPtr,
					       isRemovable,
					       isWritable,
					       isInternal,
					       stat_p,
					       stat_number,
					       diskPtr->ioSize);
		
		if (volume != nil) {
                    CFArrayAppendValue(diskList->list,volume);
                }

	} /* for */

    success = TRUE;

Return:
	if (fsdirs) {
		for (n = 0; n < nfs; n++) {
			free((void *)fsdirs[n]);
		}
		free((void *)fsdirs);
	}
	if (stat_p)
	{
		free(stat_p);
	}

	if (success)
	{
		return diskList;
	}

        DiskVolumes_delete(diskList);
	return nil;

}

boolean_t
DiskVolumes_findDisk(DiskVolumesPtr diskList, boolean_t all, 
		     const char * volume_name)
{
	DiskVolumePtr	best = NULL;
	boolean_t	found = FALSE;
	boolean_t	best_is_internal = FALSE;
	int 		i;
	int 		count = CFArrayGetCount(diskList->list);
	
	for (i = 0; i < count; i++) {
		DiskVolumePtr	vol;
		
		vol = (DiskVolumePtr)CFArrayGetValueAtIndex(diskList->list,i);
		if (vol->removable
		    || vol->writable == FALSE
		    || vol->mounted == TRUE
		    || vol->dirty == TRUE
		    || vol->fs_type == NULL
		    || !(strcmp(vol->fs_type, "hfs") == 0
			 || strcmp(vol->fs_type, "ufs") == 0)) {
			continue;
		}
		if (volume_name != NULL
		    && strcmp(volume_name, vol->disk_name)) {
			continue;
		}
		found = TRUE;
		if (all == TRUE) {
			printf("%s %s\n", vol->disk_dev_name, vol->fs_type);
		}
		else if (best_is_internal && vol->internal == FALSE) {
			continue;
		}
		else {
			if (best == NULL || vol->size > best->size) {
				best_is_internal = vol->internal;
				best = vol;
			}
		}
	}
	if (best) {
		printf("%s %s\n", best->disk_dev_name, best->fs_type);
	}
	return (found);
}

int 	DiskVolumes_count(DiskVolumesPtr diskList)
{
    return CFArrayGetCount(diskList->list);
}
DiskVolumePtr 	DiskVolumes_objectAtIndex(DiskVolumesPtr diskList,int index)
{
    return (DiskVolumePtr)CFArrayGetValueAtIndex(diskList->list,index);
}

int diskIsInternal(io_registry_entry_t media)
{
    io_registry_entry_t parent = 0;
    //(needs release
       io_registry_entry_t parentsParent = 0;
    //(needs release)
        io_registry_entry_t service = media;
    //mandatory initialization
        kern_return_t kr;

        int             isInternal = 0;
    //by default inited

    kr = IORegistryEntryGetParentEntry(service, kIOServicePlane, &parent);
    if (kr != KERN_SUCCESS)
        return 1;

    while (parent) {

        kr = IORegistryEntryGetParentEntry(parent, kIOServicePlane, &parentsParent);
        if (kr != KERN_SUCCESS)
            break;

        if (IOObjectConformsTo(parent, "IOBlockStorageDevice"))
        {
            CFDictionaryRef characteristics = IORegistryEntryCreateCFProperty(parent, CFSTR("Protocol Characteristics"), kCFAllocatorDefault, kNilOptions);

            if (characteristics) {
                CFStringRef connection;
                // CFShow(characteristics);
                connection = (CFStringRef) CFDictionaryGetValue(characteristics, CFSTR("Physical Interconnect Location"));
                if (connection) {
                    CFComparisonResult result;
                    assert(CFGetTypeID(connection) == CFStringGetTypeID());

                    result = CFStringCompare(connection, CFSTR("Internal"), 0);
                    if (result == kCFCompareEqualTo) {
                        isInternal = 1;
                    }
                }

                CFRelease(characteristics);
            }
            break;
        }
        if (parent)
            IOObjectRelease(parent);
        parent = parentsParent;
        parentsParent = 0;

    }

    if (parent)
        IOObjectRelease(parent);
    if (parentsParent)
        IOObjectRelease(parentsParent);

    return isInternal;
}

void 
GetDisksFromRegistry(io_iterator_t iter, int initialRun, int mountExisting)
{
	kern_return_t   kr;
	io_registry_entry_t entry;

	io_name_t       ioMediaName;
	UInt64          ioSize;
	int             ioWritable, ioEjectable;
	unsigned        flags;
	mach_port_t     masterPort;
	mach_timespec_t timeSpec;


	timeSpec.tv_sec = (initialRun ? 10 : 1);
	timeSpec.tv_nsec = 0;

	IOMasterPort(bootstrap_port, &masterPort);

	//sleep(1);
	IOKitWaitQuiet(masterPort, &timeSpec);

	while ((entry = IOIteratorNext(iter))) {
		char           *ioBSDName = NULL;
		//(needs release)
			CFBooleanRef    boolean = 0;
		//(don 't release)
		   CFNumberRef number = 0;
		//(don 't release)
		   CFDictionaryRef properties = 0;
		//(needs release)
			CFStringRef     string = 0;
		//(don 't release)

		//MediaName

			kr = IORegistryEntryGetName(entry, ioMediaName);
		if (KERN_SUCCESS != kr) {
			dwarning(("can't obtain name for media object\n"));
			goto Next;
		}
		//Get Properties

        kr = IORegistryEntryCreateCFProperties(entry, (CFMutableDictionaryRef *)&properties, kCFAllocatorDefault, kNilOptions);
		if (KERN_SUCCESS != kr) {
			dwarning(("can't obtain properties for '%s'\n", ioMediaName));
			goto Next;
		}
		assert(CFGetTypeID(properties) == CFDictionaryGetTypeID());

		//BSDName

			string = (CFStringRef) CFDictionaryGetValue(properties, CFSTR(kIOBSDNameKey));
		if (!string) {
			/* We're only interested in disks accessible via BSD */
			dwarning(("kIOBSDNameKey property missing for '%s'\n", ioMediaName));
			goto Next;
		}
		assert(CFGetTypeID(string) == CFStringGetTypeID());

		ioBSDName = daCreateCStringFromCFString(string);
		assert(ioBSDName);

		dwarning(("ioBSDName = '%s'\t", ioBSDName));

		//Content

			string = (CFStringRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaContentKey));
		if (!string) {
			dwarning(("\nkIOMediaContentKey property missing for '%s'\n", ioBSDName));
			goto Next;
		}
		assert(CFGetTypeID(string) == CFStringGetTypeID());

		//Writable

			boolean = (CFBooleanRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaWritableKey));
		if (!boolean) {
			dwarning(("\nkIOMediaWritableKey property missing for '%s'\n", ioBSDName));
			goto Next;
		}
		assert(CFGetTypeID(boolean) == CFBooleanGetTypeID());

		ioWritable = (kCFBooleanTrue == boolean);

		dwarning(("ioWritable = %d\t", ioWritable));

		//Ejectable

			boolean = (CFBooleanRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaEjectableKey));
		if (!boolean) {
			dwarning(("\nkIOMediaEjectableKey property missing for '%s'\n", ioBSDName));
			goto Next;
		}
		assert(CFGetTypeID(boolean) == CFBooleanGetTypeID());

		ioEjectable = (kCFBooleanTrue == boolean);

		dwarning(("ioEjectable = %d\t", ioEjectable));

		//ioSize

			number = (CFNumberRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaSizeKey));
		if (!number) {
			dwarning(("\nkIOMediaSizeKey property missing for '%s'\n", ioBSDName));
		}
		assert(CFGetTypeID(number) == CFNumberGetTypeID());

		if (!CFNumberGetValue(number, kCFNumberLongLongType, &ioSize)) {
			goto Next;
		}
		dwarning(("ioSize = %ld\t", (long int) ioSize));

		//Construct the < flags > word

			flags = 0;

		if (!ioWritable)
			flags |= kDiskArbDiskAppearedLockedMask;

		if (ioEjectable)
			flags |= kDiskArbDiskAppearedEjectableMask;

		if (!ioSize)
			flags |= kDiskArbDiskAppearedNoSizeMask;
		//blank media

        if (diskIsInternal(entry)) {
            dwarning(("\nInternal disk appeared ...\n"));
            flags |= kDiskArbDiskAppearedInternal;
        }

		//Create a disk record

        {
				/*
				 * Create a new disk
				 */
				DiskPtr         disk = NewDisk(ioBSDName,
							       entry,
							       flags,
							       ioSize);
				if (!disk) {
					pwarning(("%s: NewDisk() failed!\n", __FUNCTION__));
				}
		}

Next:

		if (properties)
			CFRelease(properties);
		if (ioBSDName)
			free(ioBSDName);

		IOObjectRelease(entry);

	}			/* while */

}				/* GetDisksFromRegistry */

/*
 * Function: string_to_argv
 * Purpose:
 *   The given string "str" looks like a set of command-line arguments, space-separated e.g.
 *   "-v -d -s", or, "-y", or "".  Turn that into an array of string pointers using the given
 *   "argv" array to store up to "n" of them.
 *
 *   The given string is modified, as each space is replaced by a nul.
 */
int
string_to_argv(char * str, char * * argv, int n)
{
	int 	count;
	char *	scan;
    
	if (str == NULL)
		return (0);

	for (count = 0, scan = str; count < n; ) {
		char * 	space;

		argv[count++] = scan;
		space = strchr(scan, ' ');
		if (space == NULL)
			break;
		*space = '\0';
		scan = space + 1;
	}
	return (count);
}

/*
 * We only want to trigger the quotacheck command
 * on a volume when we fsck it and mark it clean.
 * The quotacheck must be done post mount.
 */
boolean_t
fsck_vols(DiskVolumesPtr vols)
{
	boolean_t       result = TRUE;	/* mandatory initialization */
	int             i;

	for (i = 0; i < DiskVolumes_count(vols); i++) {

		DiskVolumePtr   vol = (DiskVolumePtr) DiskVolumes_objectAtIndex(vols, i);
		if (!vol) {
			return FALSE;
		}

		if (vol->writable && vol->dirty) {
#define NUM_ARGV	6
			const char * 	argv[NUM_ARGV] = {
				NULL, /* fsck */
				NULL, /* -y */
				NULL, /* /dev/rdisk0s8 */
				NULL, /* termination */
				NULL, /* 2 extra args in case someone wants to pass */
				NULL  /* extra args beyond -y */
			};
			int 		argc;
			char           *fsckCmd = repairPathForFileSystem(vol->fs_type);
			char           *rprCmd = repairArgsForFileSystem(vol->fs_type);
			char 		devpath[64];
			int 		ret;

			snprintf(devpath, sizeof(devpath), "/dev/r%s", vol->disk_dev_name);
			argv[0] = fsckCmd;
			argc = string_to_argv(rprCmd, (char * *)argv + 1, NUM_ARGV - 3);
			argv[1 + argc] = devpath;

			if (do_exec(NULL, argv, &ret, NULL) == FALSE) {
				/* failed to get a result, assume the volume is clean */
				dwarning(("*** vol dirty? ***\n"));
				vol->dirty = FALSE;
				ret = 0;
			}
			else if (ret == 0) {
				/* Mark the volume as clean so that it will be mounted */
				vol->dirty = FALSE;
			}
			else {
				dwarning(("'%s' failed: %d\n", fsckCmd, ret));
			}

			/*
			 * Result will be TRUE iff each fsck command
			 * is successful
			 */
			dwarning(("*** result? ***\n"));
			result = result && (ret == 0);

			dwarning(("*** freeing? ***\n"));

			free(fsckCmd);
			free(rprCmd);

		} //if dirty
    } //for each
    return result;

}

boolean_t
autodiskmount_findDisk(boolean_t all, const char * volume_name)
{
	boolean_t	   	found = TRUE;
	DiskVolumesPtr 	 	vols;
	
	DiskVolumes_new(&vols);
	DiskVolumes_do_volumes(vols);

	(void)fsck_vols(vols);
	found = DiskVolumes_findDisk(vols, all, volume_name);

	DiskVolumes_delete(vols);
	return (found);
}

int
findDiskInit()
{
	kern_return_t r;
        io_iterator_t ioIterator;  // first match

	r = IOMasterPort(bootstrap_port, &ioMasterPort);
	if (r != KERN_SUCCESS)
	{
		pwarning(("(%s:%d) IOMasterPort failed: {0x%x}\n", __FILE__, __LINE__, r));
		return -1;
	}

	r = IOServiceGetMatchingServices(ioMasterPort,
					 IOServiceMatching( "IOMedia" ),
					 &ioIterator);
        if (r != KERN_SUCCESS)
        {
                pwarning(("(%s:%d) IOServiceGetMatching Services: {0x%x}\n", __FILE__, __LINE__, r));
                return -1;
        }
        GetDisksFromRegistry(ioIterator, 1, 0);
        IOObjectRelease(ioIterator);
        cacheFileSystemDictionaries();
        cacheFileSystemMatchingArray();
	return (0);
}

int
main(int argc, char * argv[])
{
	const char * 	volume_name = NULL;
	char * progname;
	char ch;
	boolean_t	find = FALSE;
	boolean_t	all = FALSE;
	
	/* Initialize globals */
	
	g.Disks = NULL;
	g.NumDisks = 0;
	
	g.verbose = FALSE;
	g.debug = FALSE;
	
	/* Initialize <progname> */
	
	progname = argv[0];
	
	/* Must run as root */
	if (getuid() != 0) {
		pwarning(("%s: must be run as root\n", progname));
		exit(1);
	}
	
	/* Parse command-line arguments */
	while ((ch = getopt(argc, argv, "avdFV:")) != -1)	{
		switch (ch) {
		case 'a':
			all = TRUE;
			break;
		case 'v':
			g.verbose = TRUE;
			break;
		case 'd':
			g.debug = TRUE;
			break;
		case 'F':
			find = TRUE;
			break;
		case 'V':
			volume_name = optarg;
			break;
		}
	}
	
	if (find == TRUE) {
		extern int findDiskInit();
		
		if (findDiskInit() < 0) {
			exit(2);
		}
		if (autodiskmount_findDisk(all, volume_name) == FALSE) {
			exit(1);
		}
		exit(0);
    }

	exit(0);
}
