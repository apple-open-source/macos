//
//  BLAPFSUtilities.c
//
//

#include <sys/param.h>
#include <sys/mount.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOMedia.h>
#include "bless.h"
#include "bless_private.h"


int BLAPFSCreatePhysicalStoreBSDsFromVolumeBSD(BLContextPtr context, const char *volBSD, CFArrayRef *physBSDs)
{
    io_service_t        volDev;
    kern_return_t       kret = KERN_SUCCESS;
    io_service_t        parent;
    io_service_t        p = IO_OBJECT_NULL;
    io_iterator_t       psIter;
    CFStringRef         bsd;
    CFMutableArrayRef   devs;
    
    
    volDev = IOServiceGetMatchingService(kIOMasterPortDefault, IOBSDNameMatching(kIOMasterPortDefault, 0, volBSD));
    if (volDev == IO_OBJECT_NULL) {
        contextprintf(context, kBLLogLevelError, "Could not get IOService for %s\n", volBSD);
        return 2;
    }
    if (!IOObjectConformsTo(volDev, "AppleAPFSVolume")) {
        contextprintf(context, kBLLogLevelError, "%s is not an APFS volume\n", volBSD);
        IOObjectRelease(volDev);
        return 2;
    }
    
    // Hierarchy is IOMedia (for physical store 1)                   IOMedia (for physical store 2)
    //                |                                                 |
    //                -> AppleAPFSContainerScheme <----------------------
    //                              |
    //                              -> IOMedia (for synthesized whole disk)
    //                                      |
    //                                      -> AppleAPFSContainer (subclass of IOPartitionScheme)
    //                                                  |
    //                                                  -> IOMedia (individual APFS volume)
    
    // We're at the bottom, trying to get to the top, so we have to go up 4 levels.
    kret = IORegistryEntryGetParentEntry(volDev, kIOServicePlane, &parent);
    if (kret) goto badHierarchy;
    p = parent;
    kret = IORegistryEntryGetParentEntry(p, kIOServicePlane, &parent);
    if (kret) goto badHierarchy;
    IOObjectRelease(p);
    p = parent;
    kret = IORegistryEntryGetParentEntry(p, kIOServicePlane, &parent);
    if (kret) goto badHierarchy;
    IOObjectRelease(p);
    p = parent;
    
    devs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    IORegistryEntryGetParentIterator(p, kIOServicePlane, &psIter);
    while ((parent = IOIteratorNext(psIter))) {
        if (!IOObjectConformsTo(parent, kIOMediaClass)) {
            contextprintf(context, kBLLogLevelError, "IORegistry hierarchy for %s has unexpected type\n", volBSD);
            IOObjectRelease(parent);
            IOObjectRelease(psIter);
            CFRelease(devs);
            return 2;
        }
        bsd = IORegistryEntryCreateCFProperty(parent, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
        CFArrayAppendValue(devs, bsd);
        CFRelease(bsd);
        IOObjectRelease(parent);
    }
    IOObjectRelease(psIter);
    IOObjectRelease(p);
    p = IO_OBJECT_NULL;
    *physBSDs = devs;
    
badHierarchy:
    if (kret) {
        contextprintf(context, kBLLogLevelError, "Couldn't get physical store IOMedia for %s\n", volBSD);
    }
    if (p != IO_OBJECT_NULL) IOObjectRelease(p);
    IOObjectRelease(volDev);
    return kret ? 3 : 0;
}



int BLMountContainerVolume(BLContextPtr context, const char *bsdName, char *mntPoint, int mntPtStrSize, bool readOnly)
{
    int		ret;
    char    vartmpLoc[MAXPATHLEN];
    char	fulldevpath[MNAMELEN];
    char	*newargv[13];
    char    *installEnv;
	int		i;
    
    installEnv = getenv("__OSINSTALL_ENVIRONMENT");
    if (installEnv && (atoi(installEnv) > 0 || strcasecmp(installEnv, "yes") == 0 || strcasecmp(installEnv, "true") == 0)) {
        strlcpy(vartmpLoc, "/var/tmp/RecoveryTemp", sizeof vartmpLoc);
    } else {
        if (!confstr(_CS_DARWIN_USER_TEMP_DIR, vartmpLoc, sizeof vartmpLoc)) {
            // We couldn't get our path in /var/folders, so just try /var/tmp.
            strlcpy(vartmpLoc, "/var/tmp/", sizeof vartmpLoc);
        }
    }
    if (access(vartmpLoc, W_OK) < 0) {
        contextprintf(context, kBLLogLevelError, "Temporary directory \"%s\" is not writable.\n", vartmpLoc);
    }
	realpath(vartmpLoc, fulldevpath);
    snprintf(mntPoint, mntPtStrSize, "%sbless.XXXX", fulldevpath);
    if (!mkdtemp(mntPoint)) {
        contextprintf(context, kBLLogLevelError,  "Can't create mountpoint %s\n", mntPoint);
    }
    
    contextprintf(context, kBLLogLevelVerbose, "Mounting at %s\n", mntPoint);
    
    snprintf(fulldevpath, sizeof(fulldevpath), "/dev/%s", bsdName);
    
    newargv[0] = "/sbin/mount";
    newargv[1] = "-t";
    newargv[2] = "apfs";
    newargv[3] = "-o";
    newargv[4] = "perm";
	newargv[5] = "-o";
	newargv[6] = "owners";
    newargv[7] = "-o";
    newargv[8] = "nobrowse";
	i = 9;
	if (readOnly) newargv[i++] = "-r";
    newargv[i++] = fulldevpath;
    newargv[i++] = mntPoint;
    newargv[i] = NULL;
    
    
    contextprintf(context, kBLLogLevelVerbose, "Executing \"%s\"\n", "/sbin/mount");
    
    pid_t p = fork();
    if (p == 0) {
        setuid(geteuid());
        ret = execv("/sbin/mount", newargv);
        if (ret == -1) {
            contextprintf(context, kBLLogLevelError,  "Could not exec %s\n", "/sbin/mount");
        }
        _exit(1);
    }
    
    do {
        p = wait(&ret);
    } while (p == -1 && errno == EINTR);
    
    contextprintf(context, kBLLogLevelVerbose, "Returned %d\n", ret);
    if (p == -1 || ret) {
        contextprintf(context, kBLLogLevelError,  "%s returned non-0 exit status\n", "/sbin/mount");
        rmdir(mntPoint);
        return 3;
    }
    
    return 0;
}




int BLUnmountContainerVolume(BLContextPtr context, char *mntPoint)
{
    int				err;
    char			*newargv[3];
	struct statfs	sfs;
	
	// We allow the passed-in path to be a general path, not just
	// a mount point, so we first need to resolve it to a mount point
	// or device.
	if (statfs(mntPoint, &sfs) < 0) {
		return errno;
	}
	
    newargv[0] = "/sbin/umount";
    newargv[1] = sfs.f_mntfromname;
    newargv[2] = NULL;
    
    contextprintf(context, kBLLogLevelVerbose, "Executing \"%s\"\n", "/sbin/umount");
    
    pid_t p = fork();
    if (p == 0) {
        setuid(geteuid());
        err = execv("/sbin/umount", newargv);
        if (err == -1) {
            contextprintf(context, kBLLogLevelError,  "Could not exec %s\n", "/sbin/umount");
        }
        _exit(1);
    }
    
    do {
        p = wait(&err);
    } while (p == -1 && errno == EINTR);
    
    contextprintf(context, kBLLogLevelVerbose, "Returned %d\n", err);
    if(p == -1 || err) {
        contextprintf(context, kBLLogLevelError,  "%s returned non-0 exit status\n", "/sbin/umount");
        return 3;
    }
    rmdir(sfs.f_mntonname);
    
    return 0;
}
