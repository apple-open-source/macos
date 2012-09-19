/*
 * Copyright (c) 2006-2011 Apple Inc. All rights reserved.
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
 * FILE: update_boot.c
 * AUTH: Soren Spies (sspies)
 * DATE: 8 June 2006
 * DESC: implement 'kextcache -u' (copying to Apple_Boot partitions)
 *
 */

#include <bless.h>
#include <miscfs/devfs/devfs.h>     // UID_ROOT, GID_WHEEL
#include <fcntl.h>
#include <libgen.h>
#include <mach/mach_error.h>
#include <mach/mach_port.h>     // mach_port_allocate()
#include <servers/bootstrap.h>
#include <sysexits.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/xattr.h>
#include <unistd.h>

#include <IOKit/kext/kextmanager_types.h>
#include <IOKit/kext/OSKextPrivate.h>
#include <IOKit/kext/kextmanager_types.h>
#include <IOKit/kext/kextmanager_mig.h>
#include <bootfiles.h>
#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <DiskArbitration/DiskArbitrationPrivate.h>

#include "bootcaches.h"
#include "bootroot_internal.h"
#include "fork_program.h"
#include "safecalls.h"
#include "kext_tools_util.h"


/******************************************************************************
* File-Globals
******************************************************************************/
static mach_port_t sBRUptLock = MACH_PORT_NULL;
static uuid_t      s_vol_uuid;
static mach_port_t sKextdPort = MACH_PORT_NULL;


/******************************************************************************
* Types
******************************************************************************/
enum bootReversions {
    nothingSerious = 0,
    noLabel,                // 1
    copyingOFBooter,        // 2
    copyingEFIBooter,       // 3
    copiedBooters,          // 4
    activatingOFBooter,     // 5
    activatingEFIBooter,    // 6
    activatedBooters,       // 7
};

enum blessIndices {
    kSystemFolderIdx = 0,
    kEFIBooterIdx = 1
    // Apple_Boot doesn't use 2-7
};

const char * bootReversionsStrings[] = {
    NULL,           // unused
    "Label deleted",
    "Unlinking and copying BootX booter",
    "Unlinking and copying EFI booter",
    "Booters copied",
    "Activating BootX",
    "Activating EFI booter",
    "Booters activated"
};


// for Apple_Boot update
struct updatingVol {
    struct bootCaches *caches;          // parsed bootcaches.plist data
    struct bootCaches *hostCaches;      // if there a host volume has caches
    char srcRoot[PATH_MAX];             // src for boot caches as char[]
    char altERpropcache[PATH_MAX];      // override location for CSFDE plist
    char hostroot[PATH_MAX];            // if initialRoot != srcRoot
    int hostfd;                         // scoping for scopyitem()
    CFDictionaryRef bootPrefOverrides;  // client props for c.a.Boot.plist
    uuid_string_t root_uuid;            // Root UUID for plist

    Boolean expectUpToDate;             // expecting things to be right (-U)
    Boolean earlyBoot;                  // detect early boot check
    Boolean doRPS, doMisc, doBooters;   // what needs updating
    Boolean doSanitize;                 // whether to cleanse each helper
    enum bootReversions changestate;    // track changes for rolling back
    CFArrayRef boots;                   // BSD Names of Apple_Boot partitions
    DASessionRef dasession;             // handle to diskarb
    OSKextLogSpec warnLogSpec;          // flags for file access warnings
    OSKextLogSpec errLogSpec;           // flags for file access errors

    // updated as each Apple_Boot is updated
    int bootIdx;                        // which helper are we updating
    char bsdname[DEVMAXPATHSIZE];       // bsdname of Apple_Boot
    DADiskRef curBoot;                  // and matching diskarb ref
    char curMount[MNAMELEN];            // path to current boot mountpt
    int curbootfd;                      // Sec: handle to curMount
    char curRPS[PATH_MAX];              // RPS dir inside
    char efidst[PATH_MAX], ofdst[PATH_MAX];
    Boolean onAPM;
};


/******************************************************************************
* Definitions
******************************************************************************/
#define COMPILE_TIME_ASSERT(pred) switch(0){case 0:case pred:;}

// for non-RPS content, including booters
#define OLDEXT ".old"
#define NEWEXT ".new"
#define SCALE_2xEXT "_2x"
#define CONTENTEXT ".contentDetails"

// NOTE: These strings must be the same length, or code in ucopyRPS will break!
// There is a compile time assert in the function to this effect.
#define BOOTPLIST_NAME "com.apple.Boot.plist"
#define BOOTPLIST_APM_NAME "com.apple.boot.plist"


/******************************************************************************
* Helpers
******************************************************************************/

// diskarb
static int mountBoot(struct updatingVol *up);
static void unmountBoot(struct updatingVol *up);

// ucopy = unlink & copy
// no race for RPS, so install it first
static int ucopyRPS(struct updatingVol *s);  // nuke/copy to inactive
// the label files (for example) have no fallback, .new is harmless
// XX ucopy"Preboot/Firmware"
static int ucopyMisc(struct updatingVol *s);        // use/overwrite .new names
// booters have fallback paths, but originals might be broken
static int ucopyBooters(struct updatingVol *s);     // nuke/copy booters (inact)
// no label -> hint of indeterminate state (label key in plist?)
static int moveLabels(struct updatingVol *s);       // move aside
static int nukeBRLabels(struct updatingVol *s);     // byebye (all?)
// booters have worst critical:fragile ratio (point of departure)
static int activateBooters(struct updatingVol *s);  // bless new names
// and the RPS data needed for booting
static int activateRPS(struct updatingVol *s);      // leap-frog w/rename()
// finally, the label (indicating a working system via this helper partition)
// XX activate"FirmwarePaths/postboot"
static int activateMisc(struct updatingVol *s);     // rename .new / label
// and now that we're safe
static int nukeFallbacks(struct updatingVol *s);
static int eraseRPS(struct updatingVol *up, char *toErase);

// cleanup routines (RPS is the last step; activateMisc handles label)
static int revertState(struct updatingVol *up);

/* Chain of Trust
 * Our goal is to do anything the bootcaches.plist says, but only to that vol.
 * #1 we only pay attention to root-owned bootcaches.plist files
 * #2 we get an fd to the bootcaches.plist              [trust is here]
// * #3 we validate the bc.plist fd after getting an fd to the volume's root
 * #4 we use stored bsdname for libbless
 * #5 we validate cachefd after the call to bless       [trust -> bsdname]
 * #6 we get curbootfd after each apple_boot mount
 * #7 we validate cachefd after the call                [trust -> curfd]
 * #8 operations take an fd limiting their scope to the mount
 */

// Q: do these *need* 'do { } while()' wrappers?
// XX should probably rename to all-caps
// seed errno since strlXXX routines do not set it. This will make
// downstream error messages more meaningful (since we're often logging the
// errno value and message)
#define pathcpy(dst, src) do { \
            Boolean useErrno = (errno == 0); \
            if (useErrno)       errno = ENAMETOOLONG; \
            if (strlcpy(dst, src, PATH_MAX) >= PATH_MAX)  goto finish; \
            if (useErrno)       errno = 0; \
    } while(0)
#define pathcat(dst, src) do { \
            Boolean useErrno = (errno == 0); \
            if (useErrno)       errno = ENAMETOOLONG; \
            if (strlcat(dst, src, PATH_MAX) >= PATH_MAX)  goto finish; \
            if (useErrno)       errno = 0; \
    } while(0)
// could have made this macro sooner
#define makebootpath(path, rpath) do { \
                                    pathcpy(path, up->curMount); \
                                    pathcat(path, rpath); \
                                } while(0)

// XX there is overlap between errno values and sysexits
static int getExitValueFor(errval)
{
    int rval;

    switch (errval) {
        case ELAST + 1:
            rval = EX_SOFTWARE;
            break;
        case EPERM:
            rval = EX_NOPERM;
            break;
        case EAGAIN:
        case ENOLCK:
            rval = EX_OSERR;
            break;
        case -1:
            switch (errno) {
                case EIO:
                    rval = EX_IOERR;
                    break;
                default:
                    rval = EX_OSERR;
                    break;
            }
            break;
        default:
            rval = errval;
    }

    return rval;
}

// TM should no longer add to Apple_Boot partitions (8992773)
#define MOBILEBACKUPS_DIR "/.MobileBackups"
#define MDS_BULWARK "/.metadata_never_index"
#define MDS_DIR "/.Spotlight-V100"
#define FSEVENTS_BULWARK "/.fseventsd/no_log"
#define FSEVENTS_DIR "/.fseventsd"
static int
sanitizeBoot(struct updatingVol *up)
{
    int rval = 0;
    int fd = -1;
    char bloatp[PATH_MAX], blockp[PATH_MAX];
    Boolean blockMissing = true;
    struct stat sb;
    
    OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
              "Removing unnecessary bloat.");

    // make sure root is owned by root!
    if ((fstat(up->curbootfd, &sb) == 0) &&
            (sb.st_uid != UID_ROOT || sb.st_gid != GID_WHEEL))
        rval |= fchown(up->curbootfd, UID_ROOT, GID_WHEEL);

    // Time Machine
    makebootpath(bloatp, MOBILEBACKUPS_DIR);
    if (0 == (stat(bloatp, &sb))) {
        fd = sdeepunlink(up->curbootfd, bloatp);
        if (fd == -1) {
            rval |= errno;
        } else {
            close(fd);
        }
    } 

    // Spotlight
    makebootpath(blockp, MDS_BULWARK);
    if (-1 == stat(blockp, &sb) && errno == ENOENT) {
        fd = sopen(up->curbootfd, blockp, O_CREAT, kCacheFileMode);
        if (fd == -1) {
            rval |= errno;
        } else {
            close(fd);
        }
    }
    makebootpath(bloatp, MDS_DIR);
    if (0 == (stat(bloatp, &sb))) {
        rval |= sdeepunlink(up->curbootfd, bloatp);
    } 

    // FSEvents has its antithesis inside its directory :P
    // we'll assume if no_log is present, that there's no cruft
    makebootpath(bloatp, FSEVENTS_DIR);
    makebootpath(blockp, FSEVENTS_BULWARK);
    if (0 == (stat(bloatp, &sb))) {
        if (-1 == stat(blockp, &sb) && errno == ENOENT) {
            // no bulwark, so nuke the whole thing
            rval |= sdeepunlink(up->curbootfd, bloatp);
        } else {
            blockMissing = false;
        }
    } 

    if (blockMissing) {
        // then recreate the directory and the "stay away" file
        rval |= sdeepmkdir(up->curbootfd, bloatp, kCacheDirMode);
        fd = sopen(up->curbootfd, blockp, O_CREAT, kCacheFileMode);
        if (fd == -1) {
            rval |= errno;
        } else {
            close(fd);
        }
    }

    // no accumulated errors -> success

finish:
    if (rval)
        OSKextLog(NULL,up->warnLogSpec,"Warning: trouble cleaning Apple_Boot");

    return rval;
}


/******************************************************************************
 * checkForMissingFiles
 * Look for missing files in the relevant Apple_boot (helper) partition.  If
 * any of the files we care about are missing then we force an update of those 
 * files.
******************************************************************************/
static void
checkForMissingFiles(struct updatingVol *up)
{
    unsigned i;
    char srcpath[PATH_MAX], dstpath[PATH_MAX];
    struct stat sb;

    OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
              "Looking for missing files.");
   
    // forced updates not allowed with -U (early boot)
    if (up->expectUpToDate)  return;
    
    /* looking for missing .VolumeIcon.icns, SystemVersion.plist, 
     * PlatformSupport.plist, .disk_label, etc
     */
    if (!up->doMisc) {
        for (i = 0; i < up->caches->nmisc; i++) {
            pathcpy(srcpath, up->caches->root);
            pathcat(srcpath, up->caches->miscpaths[i].rpath);
            pathcpy(dstpath, up->curMount);
            pathcat(dstpath, up->caches->miscpaths[i].rpath);
        
            /* look to see if our source file exists (some may not) and if it does
             * and it's clone is NOT in Apple_Boot then force an update
             */
            if (stat(srcpath, &sb) == 0) { 
                // source file exists, now check on Apple_Boot
                if (stat(dstpath, &sb) != 0 && errno == ENOENT) {
                    // missing file, force an update
                    up->doMisc = true;
                    OSKextLog(nil,kOSKextLogFileAccessFlag|kOSKextLogBasicLevel,
                        "Helper partition missing misc files, forcing update");
                    break;
                }
            }         
        }
    }
    
    // now look for boot.efi
    if (!up->doBooters) {
        if (up->caches->efibooter.rpath[0]) {
            pathcpy(dstpath, up->curMount);
            pathcat(dstpath, up->caches->efibooter.rpath);
            if (stat(dstpath, &sb) != 0 && errno == ENOENT) {
                // missing file, force an update
                up->doBooters = true;
                OSKextLog(NULL, kOSKextLogFileAccessFlag|kOSKextLogBasicLevel,
                         "Helper partition missing EFI booter, forcing update");
                goto finish;
            }
        }
        // OF booter deserves love too :)
        if (up->caches->ofbooter.rpath[0]) {
            pathcpy(dstpath, up->curMount);
            pathcat(dstpath, up->caches->ofbooter.rpath);
            if (stat(dstpath, &sb) != 0 && errno == ENOENT) {
                // missing file, force an update
                up->doBooters = true;
                OSKextLog(NULL, kOSKextLogFileAccessFlag|kOSKextLogBasicLevel,
                         "Helper partition missing OF booter, forcing update");
                goto finish;
            }
        }
    }
    
finish:
    return;
}

/*******************************************************************************
* updateBootHelpers() updates per the passed-in struct updatingVol.
* Sec: must ensure each target is one of the source's Apple_Boot partitions
* Logically, callers provide up->boots,caches but initContext() also
* fills in up->dasession.  Callers must also cleanUpContext() afterwards.
*
* updateBootHelpers() neither updates timestamps nor substitutes EX_OSFILE
* on success w/expectUpToDate == true.
******************************************************************************/
static int
updateBootHelpers(struct updatingVol *up, Boolean expectUpToDate)
{
    int result = 0;
    up->curbootfd = -1;
    struct stat sb;
    CFIndex bootcount, bootupdates = 0;
 
    // if the plist has gone stale, punt
    if ((result = fstat(up->caches->cachefd, &sb))) {
        OSKextLog(NULL, up->errLogSpec, "fstat(cachefd): %s", strerror(errno));
        goto finish;
    }

    bootcount = CFArrayGetCount(up->boots);
    for (up->bootIdx = 0; up->bootIdx < bootcount; up->bootIdx++) {
        up->changestate = nothingSerious;                // init state
        if ((result = mountBoot(up))) {
            goto bootfail;  // sets curMount
        }

        // nuke anything that doesn't belong (discovered helpers only)
        if (up->doSanitize) {
            (void)sanitizeBoot(up);
        }

        // check to see if files of interest are missing from Apple_Boot
        checkForMissingFiles(up);

        if (up->doRPS && (result = ucopyRPS(up))) {
            goto bootfail;  // -> inactive
        }
        if (up->doMisc) {
            (void) ucopyMisc(up);  // -> .new files
        }
        
        // get the label out of the way (should be optional?)
        // expectUpToDate => early boot -> harder to generate label?
        if (expectUpToDate) {
            if ((result = moveLabels(up))) {
                goto bootfail;
            }
        } else {
            if ((result = nukeBRLabels(up))) {
                goto bootfail;
            }
        }
        
        if (up->doBooters && (result = ucopyBooters(up))) {                
            goto bootfail;      // .old still active
        }
        // If Recovery OS was available, we could swap these two and leave
        // the Recovery OS blessed until RPS and new booters were activated.
        if (up->doBooters && (result = activateBooters(up))) { // committed
            goto bootfail;
        }
        // 10.x.n+1 booters remain compatible 10.x.n kernels?? (power outage!)
        if (up->doRPS && (result = activateRPS(up))) {         // complete
            goto bootfail;
        }
        if ((result = activateMisc(up))) {
            goto bootfail;      // reverts label
        }

        up->changestate = nothingSerious;
        bootupdates++;      // loop success
        // -U -> updates are a warning
        OSKextLog(NULL, kOSKextLogFileAccessFlag |
                  (expectUpToDate?kOSKextLogWarningLevel:kOSKextLogBasicLevel),
                  "Successfully updated helper partition %s.", up->bsdname);
        
bootfail:
        // clean up this helper only, no hard failures in the loop
        if (up->changestate != nothingSerious) {
            OSKextLog(NULL, up->errLogSpec,
                      "Error updating helper partition %s, state %d: %s.",
                      up->bsdname, up->changestate,
                      bootReversionsStrings[up->changestate]);
        }
        // unroll any changes we may have made
        (void)revertState(up);     // smart enough to do nothing
        
        // always unmount
        if (nukeFallbacks(up))
            OSKextLog(NULL, up->errLogSpec,
                      "Helper partition %s may be untidy.", up->bsdname);
        if (up->curBoot)
            unmountBoot(up);       // best effort
    }

    if (bootupdates != bootcount) {
        OSKextLog(NULL, up->errLogSpec, "failed to update helper partition%s",
                  bootcount - bootupdates == 1 ? "" : "s");
        // bullet-proofing: make sure there is a generic error
        if (result == 0) {
            // should always be a non-zero result at this point
            result = ELAST + 1;
        }
        goto finish;
    }

finish:
    return result;
}

/******************************************************************************
* entry points to update caches and copy files to helper partitions
* these culminate in BRUpdateBootFiles() and BRCopyBootFiles().
******************************************************************************/
// XX move to bootcaches.[ch]?
// sBRUptLock is accessible here and could be used to conditionalize
// the setting of _skiplocks.  But having kextd use this would be worth
// the move.
int
checkRebuildAllCaches(struct bootCaches *caches, int oodLogSpec)
{
    int result;         // all paths set result
    struct stat sb;
 
    // if the caches data is no longer valid, abort immediately
    if ((result = fstat(caches->cachefd, &sb))) { 
        goto finish;
    }

    OSKextLog(NULL, kOSKextLogProgressLevel | kOSKextLogArchiveFlag,
              "Ensuring %s's caches are up to date.", caches->root);

    /* XX Sec (re-review?): can't let an external volume insert a cache
     * - mktmp/mkstmp used to create temp file at destination
     * - final rename must be on whatever volume provided the kexts
     * - if volume is /, then kexts owned by root can be trusted (4623559 fstat)
     * - otherwise, rename from wrong volume will fail
     */
     
    // We have to rely on the system's kextcache + IOKit.framework to
    // rebuild these caches.  If called on an older system via
    // libBootRoot against newer cache files, the launched kextcache
    // processes are unlikely to know how to update the caches.  Errors
    // should be returned.

    // avoid deadlock with the kextcache processes which might launch below
    // XX callers of this function should have taken the lock via initContext()
    setenv("_com_apple_kextd_skiplocks", "1", 1);


    // update the various mach kernel caches
    if (check_kext_boot_cache_file(caches,
                  caches->kext_boot_cache_file->rpath, caches->kernel)) {

        // rebuild the mkext under our lock / lack thereof
        // (-v forwarded via environment variable by kextcache & kextd)
        OSKextLog(nil, oodLogSpec, "rebuilding %s",
                caches->kext_boot_cache_file->rpath);
        if ((result = rebuild_kext_boot_cache_file(caches, true /*wait*/,
                caches->kext_boot_cache_file->rpath, caches->kernel))) {
            OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
                      "Error %d rebuilding %s.", result,
                      caches->kext_boot_cache_file->rpath);
                goto finish;
        }
    } else {
        OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogArchiveFlag,
                  "Primary kext cache does not need update.");
    }

    // Check/rebuild the CSFDE property cache which goes into the Apple_Boot.
    // A stale copy can boot, but we want only the latest secrets to work.
    if (check_csfde(caches)) {
        OSKextLog(NULL,oodLogSpec,"rebuilding %s",caches->erpropcache->rpath);
        if ((result = rebuild_csfde_cache(caches))) {
            OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
                      "Error %d rebuilding %s.", result,
                      caches->erpropcache->rpath);
            goto finish;
        }
    } else {
        OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogArchiveFlag,
                  "CSFDE property cache does not need update.");
    }

    // check on the (optional) localized resources used by EFI Login
    if (check_loccache(caches)) {
        OSKextLog(NULL,oodLogSpec,"rebuilding %s",caches->efiloccache->rpath);
        if ((result = rebuild_loccache(caches))) {
            OSKextLog(NULL, kOSKextLogWarningLevel | kOSKextLogArchiveFlag,
                      "Warning: Error %d rebuilding %s",
                      result, caches->efiloccache->rpath);
        }
        // efiloccache is not required when copying rpspaths
        // so we can ignore failures to rebuild the cache.
    }

    // success!
    result = 0;

finish:
    return result;
}


/*****************************************************************************
* initContext() sets up a struct updatingVol for use by other functions
* - srcVol must have a supported bootcaches.plist
* - srcVol is locked with kextd
* - diskarb is configured in up->dasession
* - helperBSDName -> up->boots = [ helperBSDName ]
* cleanUpContext() should be called after the work is done.
*****************************************************************************/
#define BOOTCOUNT 1
static int
initContext(struct updatingVol *up, CFURLRef srcVol, CFStringRef helperBSDName,
            int expectUpToDate)
{
    int result;
    const void         *values[BOOTCOUNT] = { helperBSDName };

    // start fresh (caller's job to have called cleanUpContext())
    bzero(up, sizeof(struct updatingVol));

    up->warnLogSpec = kOSKextLogArchiveFlag | kOSKextLogWarningLevel;
    up->errLogSpec = kOSKextLogArchiveFlag | kOSKextLogErrorLevel;

    up->expectUpToDate = expectUpToDate;

    // takeVolumeForPath() wants a char* ... comes before up->caches = ...
    if (!CFURLGetFileSystemRepresentation(srcVol, /* resolveToBase */ true,
                             (UInt8 *)up->srcRoot,sizeof(up->srcRoot))){
        OSKextLogStringError(NULL);
        result = EX_OSERR;
        goto finish;
    }

    // Technically, we don't need to lock to read bootcaches.plist,
    // but if there are multiple kextcache -u processes, it leaves
    // a longer window during which files can be updated.  Also, 
    // being locked means owners are enabled so it's okay for
    // readCaches to make [yes, now SIC] the bootstamps directory.

    // For now, kextcache -U in early boot doesn't lock.
    // (part of why kextd delays auto-rebuild for 5 minutes)
    if (expectUpToDate && getppid() == 2 /* launchctl */) {
        up->earlyBoot = true;
    } else {
        if ((result = takeVolumeForPath(up->srcRoot))) { // lock (logs)
            goto finish;
        }
    }

    // initializing the context fails if there's no bootcaches.plist
    if (!(up->caches = readBootCachesForVolURL(srcVol))) {
        if (errno == ENOENT) {
            char    cfURLBuf[PATH_MAX];
            if (false == CFURLGetFileSystemRepresentation ( srcVol, true, (UInt8 *) cfURLBuf, PATH_MAX )) {
                strlcpy(cfURLBuf, "(CFURL string unavailable)", PATH_MAX);
            }
            // warnLogSpec b/c some code paths don't mind
            OSKextLog(NULL, up->warnLogSpec,
                "%s: no " kBootCachesPath "; nothing to do", cfURLBuf);
        }
        if (errno) {
            result = errno;
        } else {
            result = EINVAL;
        }
        goto finish;
    }

    // configure a disk arb session
    if (!(up->dasession = DASessionCreate(nil))) {
        result = ENOMEM;
        goto finish;
    }
    // mountBoot and unmountBoot will spin the runloop for this DA session
    DASessionScheduleWithRunLoop(up->dasession, CFRunLoopGetCurrent(),
            kCFRunLoopDefaultMode);


    // if specified, this partition is the one to update
    if (helperBSDName) {
        up->boots = CFArrayCreate(nil,values,BOOTCOUNT,&kCFTypeArrayCallBacks);
    }

    result = 0;

finish:
    return result;
}

static void
cleanUpContext(struct updatingVol *up, int status)
{
    // unmountBoot should have taken care of this
    if (up->curbootfd != -1)
        close(up->curbootfd);

    if (up->boots)      CFRelease(up->boots);

    if (up->dasession) {
        DASessionUnscheduleFromRunLoop(up->dasession, CFRunLoopGetCurrent(),
                kCFRunLoopDefaultMode);
        CFRelease(up->dasession);
        up->dasession = NULL;
    }

    if (up->hostCaches)  destroyCaches(up->hostCaches);
    if (up->caches)      destroyCaches(up->caches);

    putVolumeForPath(up->srcRoot, status);
}


/******************************************************************************
* checkUpdateCachesAndBoots() returns
* - success (EX_OK / 0) if nothing needs updating
* - success if updates were successfully made (and expectUTD = false)
* - EX_OSFILE if updates were unexpectedly needed and successfully made
******************************************************************************/
#define BRDBG_OOD_HANG_BOOT_F "/var/db/.BRHangBootOnOODCaches"
#define BRDBG_HANG_MSG PRODUCT_NAME ": " BRDBG_OOD_HANG_BOOT_F \
                    "-> hanging on out of date caches"
#define BRDBG_CONS_MSG "[via /dev/console] " BRDBG_HANG_MSG "\n"
int
checkUpdateCachesAndBoots(
    CFURLRef volumeURL,
    Boolean  force,
    Boolean  expectUpToDate,
    Boolean  cachesOnly)
{
    int result = 0;     // should be initialized on all error paths
    OSKextLogSpec oodLogSpec = kOSKextLogGeneralFlag | kOSKextLogBasicLevel;
    struct updatingVol up = { /*NULL...*/ };
    up.curbootfd = -1;
    struct statfs sfs;
    Boolean   doAny;

    // Check for bootcaches.plist, else politely bail (initC() logs if missing)
    if ((result = initContext(&up, volumeURL, NULL, expectUpToDate))) {
        // B!=R update ignores unintelligele bootcaches.plist data
        if (result == ENOENT || result == EINVAL) {
            result = 0;
        }
        goto finish;
    }
    // -U logs what is out of date at a a more urgent level than -u
    if (expectUpToDate) {
        oodLogSpec = up.errLogSpec;
    }

    // do some real work updating caches *in* the source volume
    // checkRebuildAllCaches() logs its own errors
    if ((result = checkRebuildAllCaches(up.caches, oodLogSpec))) {
        goto finish;
    }
    
    // If installer called us then we only update the caches - 9455881
    if (cachesOnly) {
        goto skipHelperUpdates;
    }

    // determine the helper partitions, if any
    // hasBoots gets helpers from bless, GPT info from the registry
    if (!hasBootRootBoots(up.caches, &up.boots, NULL, &up.onAPM)) {
/*
        // USB TESTING require '/' to have Apple_Boots for TESTING ONLY
        // see BLESS_BUG_RESOLVED in bootcaches.c
        if (0 == strncmp(up.srcRoot, "/", 2)) {
            OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                  "%s: no supported helper partitions to update.", up.srcRoot);
            result = EDEVERR;
        } else
*/ 
        {
            result = 0;     // no boots -> nothing more to do
            OSKextLog(NULL, kOSKextLogBasicLevel | kOSKextLogFileAccessFlag,
                  "%s: no supported helper partitions to update.", up.srcRoot);
        }
        goto finish;
    }

    /* --- updating a Boot!=Root volume --- */

    // these are dedicated helper partitions & should be clean
    up.doSanitize = true;

    // figure out what needs updating
    // needUpdates() also populates the timestamp values used by updateStamps()
    doAny = needUpdates(up.caches, &up.doRPS, &up.doBooters, &up.doMisc,
                        oodLogSpec);
    if (up.earlyBoot && doAny) {
        struct stat sb;
        int consfd = open(_PATH_CONSOLE, O_WRONLY|O_APPEND);
        while (stat(BRDBG_OOD_HANG_BOOT_F, &sb) == 0) {
            OSKextLog(NULL, up.errLogSpec, BRDBG_HANG_MSG);
            if (consfd > -1)
                write(consfd, BRDBG_CONS_MSG, sizeof(BRDBG_CONS_MSG)-1);
            sleep(30);
        }
    }

    if (force) {
        up.doRPS = up.doBooters = up.doMisc = true;
    } else if (!doAny) {
        result = 0;
        // LogLevelBasic is only emitted with -v and above
        OSKextLog(NULL, kOSKextLogBasicLevel | kOSKextLogFileAccessFlag,
                  "%s: helper partitions appear up to date.", up.srcRoot);
        goto finish;
    }

    // fill in root_uuid since updateBootHelpers no longer gets from up.caches
    if (strlcpy(up.root_uuid, up.caches->fsys_uuid, NCHARSUUID) > NCHARSUUID) {
        result = EOVERFLOW;
        OSKextLog(NULL, up.errLogSpec, "error copying root_uuid");
        goto finish;
    }

    // request actual updates (logs its own errors)
    if ((result = updateBootHelpers(&up, expectUpToDate)))
        goto finish;

    // don't try to apply bootstamps to a read-only volume
    if (statfs(up.srcRoot, &sfs) == 0) {
        if ((sfs.f_flags & MNT_RDONLY) == 0) {
            if ((result = updateStamps(up.caches, kBCStampsApplyTimes))) {
                goto finish;
            }
        } else {
            OSKextLog(NULL, up.warnLogSpec,
                      "Warning: %s is read-only: skipping bootstamp updates",
                      up.caches->root);
        }
    } else {
        OSKextLog(NULL, up.errLogSpec, "statfs(%s): %s",
              up.caches->root, strerror(errno));
        goto finish;
    }

    /* We're here if we successfully updated the helpers or skipped doing so.  
     * If we are not expecting to make updates, return EX_OSFILE, otherwise 
     * return success.  
     */
skipHelperUpdates:
    if (expectUpToDate) {
        result = EX_OSFILE;
    } else {
        result = EX_OK;
    }

finish:
    // expectUpToDate means someone (usu. launchd during early boot) is
    // asking us whether we had to update anything ... and it's going to
    // use the answer to decide whether or not to reboot the system (off
    // freshened Apple_Boot bits).  EX_OSFILE is the exit status indicating
    // "success with changes."  Otherwise, we'll be proceeding with
    // boot (result != EX_OSFILE) which means we should (if we're root)
    // notify kextd that we're done by sending it a "fake" lock request.
    if (up.earlyBoot) {
        if (result != EX_OSFILE) {
            // kextd likes knowing kextcache -U ran.  A lock + unlock (below)
            // could allow it to start automatic rebuilds earlier.
            if (takeVolumeForPath(up.srcRoot)) {
                OSKextLog(NULL, up.errLogSpec, "kextd lock/signal failed");
            }
        }
    }

    // since updateBoots() -> exit(), convert common errors to sysexits(3)
    if (result && result != EX_OSFILE) {
        result = getExitValueFor(result);
    }

    // handles unlock / reporting to kextd
    cleanUpContext(&up, result);

    // all error paths should log if the functions they call don't

    return result;
}

#define kBRCheckLogSpec (kOSKextLogArchiveFlag | kOSKextLogProgressLevel)
OSStatus
BRUpdateBootFiles(CFURLRef volURL, Boolean force)
{
    if (!volURL)
        return EINVAL;

    return checkUpdateCachesAndBoots(volURL, force, 
                                     false /*ood okay*/, 
                                     false /*update helper parts okay*/);
}


static OSStatus
mergeHostVolInfo(struct updatingVol *up, CFURLRef hostVol)
{
    OSStatus result;
 
    up->hostCaches = readBootCachesForVolURL(hostVol);
    if (up->hostCaches) {
        // main item: capture filesystem properties
        if (strlcpy(up->root_uuid,up->hostCaches->fsys_uuid,NCHARSUUID)>NCHARSUUID){
            result = EOVERFLOW;
            goto finish;
        }
        pathcpy(up->hostroot, up->hostCaches->root);

        if (up->hostCaches->erpropcache) {
            pathcpy(up->altERpropcache, up->hostCaches->erpropcache->rpath);
            // currently only need up->hostroot if up->erpropcache is valid
            up->hostfd = open(up->hostroot, O_RDONLY);
            if (up->hostfd == -1)       goto finish;
        } 

    } else {
        // XX for now, the host volume must have an OS with EfiLoginUI ...
        // (we can make that optional later for volumes that don't need it)
        uuid_t vol_uuid;
        if (!CFURLGetFileSystemRepresentation(hostVol, true /*resolve base*/,
                (UInt8*)up->hostroot, PATH_MAX)) {
            result = EOVERFLOW;
            goto finish;
        }

        // get UUIDs
        if ((result = copyVolumeUUIDs(up->hostroot, vol_uuid, NULL)))
            goto finish;

        // expand filesystem UUID into place
        uuid_unparse_upper(vol_uuid, up->root_uuid);

        // XX need to use a temporary EncryptedRootPlist.plist.wipekey
        // could insert via up->hostroot?
    }

    result = 0;
    
finish:    
    return result;
}


/******************************************************************************
* copy boot files from source volume to destination partition
* - makes sure caches are up to date
* - ignore up to date bootstamps
* - *do* update bootstamps if srcVol == hostVol
* see bootroot.h for more details
******************************************************************************/
OSStatus
BRCopyBootFiles(CFURLRef srcVol,
                CFURLRef hostVol,
                CFStringRef helperBSDName,
                CFDictionaryRef bootPrefOverrides)
{
    OSStatus            result;     // libBR funcs should have good errors
    struct updatingVol  up = { /* NULL, ... */ };

    // defend libBootRoot entry point
    if (!srcVol || !hostVol || !helperBSDName) {
        result = EINVAL;
        goto finish;
    }

    // configure a single-helper context
    result = initContext(&up, srcVol, helperBSDName, false /*expectUTD*/);
    if (result)     goto finish;

    // Make sure all caches are up to date on the source
    // (undefined if OOD & system's kext management can't rebuild)
    result = checkRebuildAllCaches(up.caches, kBRCheckLogSpec);
    if (result)     goto finish;

    // gather timestamp data; ignore results
    (void)needUpdates(up.caches, NULL, NULL, NULL,
                      kOSKextLogGeneralFlag | kOSKextLogProgressLevel);

    // extract various overrides from the host volume
    // (may populate up.hostCaches)
    result = mergeHostVolInfo(&up, hostVol);
    if (result)     goto finish;

    // BRCopyBootFiles() always copies everything
    up.doRPS = up.doBooters = up.doMisc = true;

    // set up overrides dict
    up.bootPrefOverrides = bootPrefOverrides;

    // XX should ensure system Boot!=Root disabled, nuke stamps, etc
    
    // get it updated!
    result = updateBootHelpers(&up, false /*ood fine*/);

    // if this was a preparatory build, go ahead and update bootstamps [XX ?]
    if (result == 0 && CFEqual(srcVol, hostVol)) {
        result = updateStamps(up.caches, kBCStampsApplyTimes);
    }

finish:
    cleanUpContext(&up, result);

    return result;
}


/******************************************************************************
* FindRPSDir plays rock, paper scissors to identify the location of
* the latest complete copy of the files the booter needs.
******************************************************************************/
static int
FindRPSDir(struct updatingVol *up, char prev[PATH_MAX], char current[PATH_MAX],
            char next[PATH_MAX])
{
     char rpath[PATH_MAX], ppath[PATH_MAX], spath[PATH_MAX];   
/*
 * FindRPSDir looks for a "rock," "paper," or "scissors" directory
 * - handle all permutations: 3 dirs, any 2 dirs, any 1 dir
 */
// static EFI_STATUS
// FindRPSDir(EFI_FILE_HANDLE BootDir, EFI_FILE_HANDLE *newBoot)
// 
    int rval = ELAST + 1, status;
    struct stat r, p, s;
    Boolean haveR = false, haveP = false, haveS = false;
    char *prevp = NULL, *curp = NULL, *nextp = NULL;

    // set up full paths with intervening slash
    pathcpy(rpath, up->curMount);
    pathcat(rpath, "/");
    pathcpy(ppath, rpath);
    pathcpy(spath, rpath);

    pathcat(rpath, kBootDirR);
    pathcat(ppath, kBootDirP);
    pathcat(spath, kBootDirS);

    status = stat(rpath, &r);   // easier to let this fail
    haveR = (status == 0);
    status = stat(ppath, &p);
    haveP = (status == 0);
    status = stat(spath, &s);
    haveS = (status == 0);

    if (haveR && haveP && haveS) {    // NComb(3,3) = 1
        OSKextLog(NULL, up->warnLogSpec,
                  "Warning: all of R,P,S exist: picking 'R'; destroying 'P'.");
        curp = rpath;   nextp = ppath;  prevp = spath;
        if ((rval = eraseRPS(up, nextp)))
            goto finish;
    }   else if (haveR && haveP) {          // NComb(3,2) = 3
        // p wins
        curp = ppath;   nextp = spath;  prevp = rpath;
    } else if (haveR && haveS) {
        // r wins
        curp = rpath;   nextp = ppath;  prevp = spath;
    } else if (haveP && haveS) {
        // s wins
        curp = spath;   nextp = rpath;  prevp = ppath;
    } else if (haveR) {                     // NComb(3,1) = 3
        // r wins by default
        curp = rpath;   nextp = ppath;  prevp = spath;
    } else if (haveP) {
        // p wins by default
        curp = ppath;   nextp = spath;  prevp = rpath;
    } else if (haveS) {
        // s wins by default
        curp = spath;   nextp = rpath;  prevp = ppath;
    } else {                                          // NComb(3,0) = 0
        // we'll start with rock
        curp = rpath;   nextp = ppath;  prevp = spath;
    }

    if (strlcpy(prev, prevp, PATH_MAX) >= PATH_MAX)     goto finish;
    if (strlcpy(current, curp, PATH_MAX) >= PATH_MAX)   goto finish;
    if (strlcpy(next, nextp, PATH_MAX) >= PATH_MAX)     goto finish;

    rval = 0;

finish:
    if (rval) {
        /* can't use errno here since strlcpy and strlcat don't set it */
        OSKextLog(NULL, up->errLogSpec,
                  "%s - strlcpy or cat failed - >= PATH_MAX", __FUNCTION__);
    }

    return rval;
}

/******************************************************************************
* BREraseBootFiles() un-does BRCopyBootFiles()
******************************************************************************/
// helper does wraps BLSetFinderVolumeInfo with schdir()
static int
sBLSetBootFinderInfo(struct updatingVol *up, uint32_t newvinfo[8])
{
    int result, fd = -1;
    uint32_t    vinfo[8];

    if (schdir(up->curbootfd, up->curMount, &fd))           goto finish;
    result = BLGetVolumeFinderInfo(NULL, ".", vinfo);
    if (result)         goto finish;
    vinfo[kSystemFolderIdx] = newvinfo[kSystemFolderIdx];
    vinfo[kEFIBooterIdx] = newvinfo[kEFIBooterIdx];
    result = BLSetVolumeFinderInfo(NULL, ".", vinfo);

finish:
    if (fd != -1)  
        (void)restoredir(fd);
    return result;
}

// helper attempts to bless the Recovery OS if present
static int
blessRecovery(struct updatingVol *up)
{
    int result;
    char path[PATH_MAX];
    struct stat sb;
    uint32_t vinfo[8] = { 0, };

    // look up pathnames & file IDs
    result = ENAMETOOLONG;

    makebootpath(path, "/" kRecoveryBootDir);
    if (stat(path, &sb) == -1) {
        result = errno;
        goto finish;
    }
    vinfo[kSystemFolderIdx] = (uint32_t)sb.st_ino;

    // append boot.efi
    pathcat(path, "/");
    pathcat(path, basename(up->caches->efibooter.rpath));
    if (stat(path, &sb) == -1) {
        result = errno;
        goto finish;
    }
    vinfo[kEFIBooterIdx] = (uint32_t)sb.st_ino;

    if ((result = sBLSetBootFinderInfo(up, vinfo))) {
        OSKextLog(NULL, up->warnLogSpec,
                 "Warning: found recovery booter but couldn't bless it.");
    }

finish:
    return result;
}

#define RECERR(up, opres, warnmsg) do { \
            if (opres == -1 && errno == ENOENT) { \
                opres = 0; \
            } \
            if (opres) { \
                if (warnmsg) { \
                    OSKextLog(NULL, up->warnLogSpec, warnmsg); \
                } \
                if (firstErr == 0) { \
                    OSKextLog(NULL, up->warnLogSpec, "capturing err %d / %d", \
                              opres, errno); \
                    firstErr = opres; \
                    if (firstErr == -1)     firstErrno = errno; \
                } \
            } \
        } while(0)
OSStatus
BREraseBootFiles(CFURLRef srcVolRoot, CFStringRef helperBSDName)
{
    OSStatus result;     // error or non-error paths explicit
    int opres, firstErrno, firstErr = 0; 
    struct updatingVol  up = { /* NULL, ... */ }, *upp = &up;
    char path[PATH_MAX], prevRPS[PATH_MAX], nextRPS[PATH_MAX];
    uint32_t zerowords[8] = { 0, };
    unsigned i;
    
    // defend libBootRoot entry point
    if (!srcVolRoot || !helperBSDName) {
        result = EINVAL;
        goto finish;
    }

    result = initContext(&up, srcVolRoot, helperBSDName, false /*expUTD*/);
    if (result)     goto finish;

    if ((result = mountBoot(&up))) {
        goto finish;  // sets curMount
    }

    // generally best effort

    // bless recovery booter if present; else unbless volume
    if ((opres = blessRecovery(&up))) {
        if ((opres = sBLSetBootFinderInfo(&up, zerowords))) {
            firstErr = opres;
            OSKextLog(NULL, up.warnLogSpec,
                      "Warning: couldn't unbless %s", up.curMount);
        }
    }

    // kill label
    opres = nukeBRLabels(&up);
    RECERR(upp, opres,"Warning: trouble nuking (inactive?) Boot!=Root label.");

    // unlink booters
    if (up.caches->ofbooter.rpath[0]) {
        pathcpy(path, up.curMount);
        pathcat(path, up.caches->ofbooter.rpath);
        opres = sunlink(up.curbootfd, path);
        RECERR(upp, opres, "couldn't unlink OF booter" /* remove w/9217695 */);
    }
    if (up.caches->efibooter.rpath[0]) {
        pathcpy(path, up.curMount);
        pathcat(path, up.caches->efibooter.rpath);
        opres = sunlink(up.curbootfd, path);
        RECERR(upp, opres, "couldn't unlink EFI booter" /* NULL w/9217695 */);
    }

    // find & nuke all RPS directories
    opres = FindRPSDir(&up, prevRPS, up.curRPS, nextRPS);
    if (opres == 0) {
        opres = eraseRPS(&up, prevRPS);
        RECERR(upp, opres, "Warning: trouble erasing R.");
        opres = eraseRPS(&up, up.curRPS);
        RECERR(upp, opres, "Warning: trouble erasing P.");
        opres = eraseRPS(&up, nextRPS);
        RECERR(upp, opres, "Warning: trouble erasing S.");
    } else {
        RECERR(upp, opres, "Warning: couldn't find RPS directions.");
    }
        
    for (i=0; i < up.caches->nmisc; i++) {
        char *rpath = up.caches->miscpaths[i].rpath;

        if (strlcpy(path, up.curMount, PATH_MAX) > PATH_MAX)   continue;
        if (strlcat(path, rpath, PATH_MAX) > PATH_MAX)         continue;
        opres = sdeepunlink(up.curbootfd, path);
        RECERR(upp, opres, "error unlinking miscpath" /* NULL w/9217695 */);
    }
    
    unmountBoot(&up);

    // no errors -> firstErr = 0
    if (firstErr == -1)     errno = firstErrno;
    result = firstErr;

finish:
    return result;
}


/******************************************************************************
* revertState() rolls back incomplete changes
******************************************************************************/
static int revertState(struct updatingVol *up)
{
    int rval = 0;       // optimism to accumulate errors with |=
    char path[PATH_MAX], oldpath[PATH_MAX];
    struct bootCaches *caches = up->caches;
    Boolean doMisc;
    struct stat sb;

    OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
              "Rolling back any incomplete updates.");
        
    switch (up->changestate) {
        // inactive booters are still good
        case activatedBooters:
            // we've blessed the new booters; so let's bless the old ones
            pathcat(up->ofdst, OLDEXT);
            pathcat(up->efidst, OLDEXT);
            // reactivates the old *if* present
            rval |= activateBooters(up);
        case activatingEFIBooter:
    case activatingOFBooter:        // unneeded since 'bless' is one op
        case copiedBooters:
    case copyingEFIBooter:
        if (caches->efibooter.rpath[0]) {
            makebootpath(path, caches->efibooter.rpath);
            pathcpy(oldpath, path);         // old ones are blessed; rename
            pathcat(oldpath, OLDEXT);
            // only unlink current booter if old one present
            if (stat(oldpath, &sb) == 0) {
                (void)sunlink(up->curbootfd, path);
                rval |= srename(up->curbootfd, oldpath, path);
            }
        }

    case copyingOFBooter:
        if (caches->ofbooter.rpath[0]) {
            makebootpath(path, caches->ofbooter.rpath);
            pathcpy(oldpath, path);
            pathcat(oldpath, OLDEXT);
            // only unlink current booter if old one present
            if (stat(oldpath, &sb) == 0) {
                (void)sunlink(up->curbootfd, path);
                rval |= srename(up->curbootfd, oldpath, path);
            }
        }

    // XX
    // case copyingMisc:
    // would clean up the .new turds

        case noLabel:
            // XX hacky (c.f. nukeFallbacks which nukes .disabled label)
            doMisc = up->doMisc;
            up->doMisc = false;
            rval |= activateMisc(up);  // writes new label if !doMisc
            up->doMisc = doMisc;

        case nothingSerious:
            // everything is good
            break;
    }

finish:
    if (rval) {
        OSKextLog(NULL, kOSKextLogErrorLevel,
                  "error rolling back incomplete updates.");
    }

    return rval;
};

/******************************************************************************
* mountBoot digs in for the root, and mounts up the Apple_Boots
* mountpoints are stored in up->bootparts
******************************************************************************/
static int mountBoot(struct updatingVol *up)
{
    int rval = ELAST + 1;
    CFStringRef mountargs[] = { CFSTR("perm"), CFSTR("nobrowse"), NULL };
    CFStringRef str;
    DADissenterRef dis = (void*)kCFNull;
    CFDictionaryRef ddesc = NULL;
    CFURLRef volURL;
    struct statfs bsfs;
    uint32_t mntgoal;
    struct stat secsb;

    OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
              "Mounting helper partition...");

    // request the Apple_Boot mount
    str = (CFStringRef)CFArrayGetValueAtIndex(up->boots, up->bootIdx);
    if (!str) {
        goto finish;
    }
    if (!CFStringGetFileSystemRepresentation(str,up->bsdname,DEVMAXPATHSIZE)){
        goto finish;
    }
    if (!(up->curBoot=DADiskCreateFromBSDName(nil,up->dasession,up->bsdname))){
        goto finish;
    }
    
    OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
              "Mounted %s.", up->bsdname);

    // DADiskMountWithArgument might call _daDone before it returns (e.g. if it
    // knows your request is impossible ...)
    // _daDone updates our 'dis[senter]'
    DADiskMountWithArguments(up->curBoot, NULL/*mnt*/,kDADiskMountOptionDefault,
                             _daDone, &dis, mountargs);

    // ... so we use kCFNull and check the value before CFRunLoopRun()
    if (dis == (void*)kCFNull) {
        CFRunLoopRun();         // stopped by _daDone (which updates 'dis')
    }
    if (dis) {
        rval = DADissenterGetStatus(dis);
        // only an error if it's not already mounted
        if (rval != kDAReturnBusy) {
            goto finish;
        }
    }

    // get and stash the mountpoint of the boot partition
    if (!(ddesc = DADiskCopyDescription(up->curBoot)))  goto finish;
    volURL = CFDictionaryGetValue(ddesc, kDADiskDescriptionVolumePathKey);
    if (!volURL || CFGetTypeID(volURL) != CFURLGetTypeID())  goto finish;
    if (!CFURLGetFileSystemRepresentation(volURL, true /*resolve base*/,
            (UInt8*)up->curMount, PATH_MAX))        goto finish;

    // Sec: get a non-spoofable handle to the current boot (trust moves)
    if (-1 == (up->curbootfd = open(up->curMount, O_RDONLY, 0)))   goto finish;
    if (fstat(up->caches->cachefd, &secsb))  goto finish;    // rootvol extant?

    // Make sure the mount is read/write and has owners enabled.
    // Because helper partitions should always have owners enabled
    // and because we soft-unmount afterwards, we don't attempt to
    // restore this state.
    if (fstatfs(up->curbootfd, &bsfs))  goto finish;
    mntgoal = bsfs.f_flags;
    mntgoal &= ~(MNT_RDONLY|MNT_IGNORE_OWNERSHIP);
    if ((bsfs.f_flags != mntgoal) && updateMount(up->curMount, mntgoal)) {
        OSKextLog(NULL, up->warnLogSpec,
                  "Warning: couldn't update mount to read/write + owners");
    }

    // we only support 128 MB Apple_Boot partitions
    if (bsfs.f_blocks * bsfs.f_bsize < (128 * 1<<20)) {
        OSKextLog(NULL, up->errLogSpec, "skipping Apple_Boot helper < 128 MB.");
        goto finish;
    }

    rval = 0;

finish:
    if (ddesc)      CFRelease(ddesc);
    if (dis && dis != (void*)kCFNull) { // for spurious CFRunLoopRun() return
        CFRelease(dis);
    }

    if (rval != 0 && up->curBoot) {
        unmountBoot(up);        // unmount anything we managed to mount
    }
    if (rval) {
        if (rval != ELAST + 1) {
            OSKextLog(NULL, up->errLogSpec,
                "Failed to mount helper partition: error %#X (DA err# %#.2x).",
                rval, rval & ~(err_local|err_local_diskarbitration));
        } else {
            OSKextLog(NULL, up->errLogSpec,"Failed to mount helper partition.");
        }
    }

    if (rval)
        OSKextLog(NULL,kOSKextLogErrorLevel,"error mounting helper partition.");

    return rval;
}

/******************************************************************************
* unmountBoot 
* attempt to unmount; no worries on failure
******************************************************************************/
static void unmountBoot(struct updatingVol *up)
{
    DADissenterRef dis = (void*)kCFNull;
    
    OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
              "Unmounting helper partition %s.", up->bsdname);

    // bail if nothing to actually unmount (still free up curBoot below)
    if (!up->curBoot)           goto finish;
    if (!up->curMount[0])       goto finish;

    // clean up curbootfd
    if (up->curbootfd != -1) {
        close(up->curbootfd);
        up->curbootfd = -1;
    }

    // _daDone populates 'dis'[senter]
    DADiskUnmount(up->curBoot, kDADiskMountOptionDefault, _daDone, &dis);
    if (dis == (void*)kCFNull) {            // in case _daDone already called
        CFRunLoopRun();
    }

    // if that didn't work, just log
    if (dis) {
        OSKextLog(NULL, up->warnLogSpec, "%s didn't unmount, leaving mounted",
                  up->bsdname);
    }

finish:
    up->curMount[0] = '\0';     // keep tidy
    if (up->curBoot) {
        CFRelease(up->curBoot);
        up->curBoot = NULL;
    }
    if (dis && dis != (void*)kCFNull) {
        CFRelease(dis);
    }
}


/******************************************************************************
* ucopyRPS unlinks old/copies new RPS content w/o activating
* RPS files are considered important -- non-zero file sizes only!
* XX could validate the kernel with Mach-o header
* several intervening helpers including eraseRPS()
******************************************************************************/
static void
addDictValues(const void *key, const void *value, void *ctx)
{
    CFMutableDictionaryRef tgtDict = (CFMutableDictionaryRef)ctx;

    // AddValue is "add if absent" so here were implement override
    if (CFDictionaryContainsKey(tgtDict, key))
        CFDictionaryRemoveValue(tgtDict, key);

    CFDictionaryAddValue(tgtDict, key, value);
}

// UUID helper for ucopyRPS
static int
insertUUID(struct updatingVol *up, char *srcpath, char *dstpath)
{
    int rval = ELAST + 1;
    int fd = -1;
    struct stat sb;
    sb.st_mode = kCacheFileMode;    // default mode for new plist
    void *buf = NULL;
    CFDataRef data = NULL;
    CFMutableDictionaryRef pldict = NULL;
    CFStringRef str = NULL;

    mode_t dirmode;
    char dstparent[PATH_MAX];
    CFIndex len;

    OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
              "Inserting filesystem UUID (%s) into Boot.plist.",
              up->root_uuid);

    // suck in any existing plist
    do {
        if (-1 == (fd=sopen(up->caches->cachefd,srcpath,O_RDONLY,0)))
            break;
        if (fstat(fd, &sb))                                 break;
        if (sb.st_size > UINT_MAX || sb.st_size > LONG_MAX) break;
        if (!(buf = malloc((size_t)sb.st_size)))            break;
        if (read(fd, buf, (size_t)sb.st_size) != sb.st_size)break;
        if (!(data = CFDataCreate(nil, buf, (long)sb.st_size)))
            break;

        // make mutable dictionary from file data
        pldict =(CFMutableDictionaryRef)CFPropertyListCreateFromXMLData(nil,
                    data, kCFPropertyListMutableContainers, NULL/*errstr*/);
    } while(0);

    // if we didn't get a dictionary from the file, create one
    if (!pldict || CFGetTypeID(pldict)!=CFDictionaryGetTypeID()) {
        SAFE_RELEASE_NULL(pldict);
        pldict = CFDictionaryCreateMutable(nil, 1,
                                        &kCFTypeDictionaryKeyCallBacks,
                                        &kCFTypeDictionaryValueCallBacks);
        if (!pldict)    goto finish;
    }

    // make a CFStr out of the UUID and insert
    str = CFStringCreateWithCString(nil, up->root_uuid, kCFStringEncodingASCII);
    if (!str)   goto finish;
    CFDictionarySetValue(pldict, CFSTR(kRootUUIDKey), str);

    // add any override values
    if (up->bootPrefOverrides) {
        CFDictionaryApplyFunction(up->bootPrefOverrides,addDictValues,pldict);
    }


    // and write dictionary back

    // figure out directory mode
    dirmode = ((sb.st_mode & ~S_IFMT) | S_IWUSR | S_IXUSR /* u+wx */);
    if (dirmode & S_IRGRP)      dirmode |= S_IXGRP;     // add conditional o+x
    if (dirmode & S_IROTH)      dirmode |= S_IXOTH;

    // and recursively create the parent directory       
    if (strlcpy(dstparent, dirname(dstpath), PATH_MAX) >= PATH_MAX) goto finish;
    if ((sdeepmkdir(up->curbootfd, dstparent, dirmode)))            goto finish;

    // sopen adds O_EXCL to O_CREAT
    (void)sunlink(up->curbootfd, dstpath);
    close(fd);      // before using fd again :P
    if (-1 == (fd=sopen(up->curbootfd, dstpath, O_WRONLY|O_CREAT,sb.st_mode))){
        goto finish;
    }
    if (data)       CFRelease(data);
    if (!(data = CFPropertyListCreateXMLData(nil, pldict)))
        goto finish;
    len = CFDataGetLength(data);
    if (write(fd, CFDataGetBytePtr(data), len) != len)      goto finish;

    rval = 0;

finish:
    if (rval) {
        OSKextLog(NULL, up->errLogSpec,
                  "%s - Error inserting UUID: %d %s.", 
                  __FUNCTION__, errno, strerror(errno));
    }
    
    if (str)        CFRelease(str);
    if (pldict)     CFRelease(pldict);
    if (data)       CFRelease(data);
    if (buf)        free(buf);
    if (fd != -1)   close(fd);


    return rval;
}

// correctly erase (hopefully old :) items in the Apple_Boot
static int
eraseRPS(struct updatingVol *up, char *toErase)
{
    int rval = ELAST+1;
    char *erpath = NULL;
    char path[PATH_MAX];
    struct stat sb;

    // if nothing to erase, return cleanly
    if (stat(toErase, &sb) == -1 && errno == ENOENT) {
        rval = 0;
        goto finish;
    }

    // erpropcache must be zeroed as well as unlinked
    // XX should test one FDE-aware OS with a truly different location
    if (up->altERpropcache[0]) {
        erpath = up->altERpropcache;
    } else if (up->caches->erpropcache) {
        erpath = up->caches->erpropcache->rpath;
    }

    if (erpath) {
        // pathc*() seed errno
        pathcpy(path, toErase);
        pathcat(path, erpath);
        // szerofile() won't complain if it is missing
        if (szerofile(up->curbootfd, path))
            goto finish;
    }

    rval = sdeepunlink(up->curbootfd, toErase);

finish:
    if (rval) {
        OSKextLog(NULL, up->errLogSpec | kOSKextLogFileAccessFlag,
                  "%s - %s. errno %d %s", 
                  __FUNCTION__, up->curRPS, errno, strerror(errno));
    }

    return rval;
}

// we can bail on any error because only a whole RPS dir makes sense
static int
ucopyRPS(struct updatingVol *up)
{
    int bsderr, rval = ELAST+1;
    char discard[PATH_MAX];
    unsigned i;
    char srcpath[PATH_MAX], dstpath[PATH_MAX];
    char * plistNamePtr;
    COMPILE_TIME_ASSERT(sizeof(BOOTPLIST_NAME)==sizeof(BOOTPLIST_APM_NAME));
    
    OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
              "Copying files used by the booter.");

    // we're going to copy into the currently-inactive directory
    if (FindRPSDir(up, up->curRPS, discard, discard))  goto finish;

    // we expect to have removed it and eraseRPS() doesn't mind it missing
    if (eraseRPS(up, up->curRPS))
        goto finish;

    // create the directory
    if (smkdir(up->curbootfd, up->curRPS, kRPSDirMask)) {
        OSKextLog(NULL, up->errLogSpec, "%s - mkdir failed for %s", 
                  __FUNCTION__, up->curRPS);
        goto finish;
    }

    // and loop
    for (i = 0; i < up->caches->nrps; i++) {
        cachedPath *curItem = &up->caches->rpspaths[i];

        pathcpy(srcpath, up->caches->root);
        pathcat(srcpath, curItem->rpath);
        pathcpy(dstpath, up->curRPS);
        pathcat(dstpath, curItem->rpath);

        // check for special files; first Boot.plist
        if (curItem == up->caches->bootconfig) {
            // PR-5115900 - call it com.apple.boot.plist on APM since Tiger
            // (since Tiger bless scribbles on com.apple.Boot.plist)
            if (up->onAPM) {
                // see assert above
                plistNamePtr = strstr(dstpath, BOOTPLIST_NAME);
                if (plistNamePtr) {
                    strncpy(plistNamePtr, BOOTPLIST_APM_NAME, strlen(BOOTPLIST_NAME));
                }
            }

            // insert the root volume's UUID
            if (insertUUID(up, srcpath, dstpath)) {
                goto finish;
            }
        } else if (up->altERpropcache[0] && curItem==up->caches->erpropcache) {
            // skip up->caches->erpropcache if there's an override
            continue;
        } else {
            // could deny zero-size cookies, busted Mach-O, etc here
            // scopyitem creates any intermediate directories
            bsderr=scopyitem(up->caches->cachefd,srcpath,up->curbootfd,dstpath);
            if (bsderr) {
                // erpropcache, efiloccache are optional
                if ((curItem == up->caches->erpropcache ||
                            curItem == up->caches->efiloccache)
                        && bsderr == -1 && errno == ENOENT) {
                    continue;
                } else {
                    OSKextLog(NULL, up->errLogSpec,
                              "Error copying %s to %s",
                              srcpath, dstpath);
                    goto finish;
                }
            }
        }
    }

    // copy any override erpropcache (normal one skipped above)
    if (up->altERpropcache[0]) {
        pathcpy(srcpath, up->hostroot);
        pathcat(srcpath, up->altERpropcache);
        pathcpy(dstpath, up->curRPS);
        pathcat(dstpath, up->altERpropcache);
        if ((bsderr = scopyitem(up->hostfd, srcpath, up->curbootfd, dstpath))
                && errno != ENOENT)
            goto finish;
    }

    rval = 0;

finish:
    if (rval) {
        OSKextLog(NULL, up->errLogSpec,
                  "%s - Error copying files used by the booter. errno %d %s", 
                  __FUNCTION__, errno, strerror(errno));
    }

    return rval;
}

/******************************************************************************
* ucopyMisc writes misc files to .new (inactive) name
#ifndef OPENSOURCE
* [the label file is re-written during activation for non-open-source builds]
#endif
******************************************************************************/
static int ucopyMisc(struct updatingVol *up)
{
    int bsderr, rval = -1;
    unsigned i, nprocessed = 0;
    char srcpath[PATH_MAX], dstpath[PATH_MAX];
    struct stat sb;

    OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
              "Copying files read before the booter runs.");
   
    for (i = 0; i < up->caches->nmisc; i++) {
        pathcpy(srcpath, up->caches->root);
        pathcat(srcpath, up->caches->miscpaths[i].rpath);
        pathcpy(dstpath, up->curMount);
        pathcat(dstpath, up->caches->miscpaths[i].rpath);
        pathcat(dstpath, ".new");

        if (stat(srcpath, &sb) == 0) { 
            // file exists and is accessible
            if ((bsderr = scopyitem(up->caches->cachefd, srcpath,
                                    up->curbootfd, dstpath))) {
                OSKextLog(NULL, up->errLogSpec, 
                          "Error copying %s to %s",
                          srcpath, dstpath);
                continue;
            }
        } else if (errno != ENOENT) {
            continue;
        }

        nprocessed++;
    }

    rval = (nprocessed != i);

finish:
    if (rval) {
        OSKextLog(NULL, up->errLogSpec,
                  "%s - Error copying files read before the booter runs. errno %d %s", 
                  __FUNCTION__, errno, strerror(errno));
    }

    return rval;
}

/******************************************************************************
* moveLabels moves the label aside in case they're needed again
* activateMisc() will move these back if needed
* no label -> hint of indeterminate state (label key in plist/other file??)
* XX put/switch in some sort of "(updating!)" label (see BL[ess] routines)
******************************************************************************/
static int moveLabels(struct updatingVol *up)
{
    int rval = -1;
    char path[PATH_MAX];
    struct stat sb;
    int fd = -1;
    
    OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
              "Moving aside old label.");

    // pathc*() seed errno
    pathcpy(path, up->curMount);
    pathcat(path, up->caches->label->rpath);
    if (0 == (stat(path, &sb))) {
        char newpath[PATH_MAX];
        unsigned char nulltype[32] = {'\0', };

        // rename
        pathcpy(newpath, path);
        pathcat(newpath, NEWEXT);
        rval = srename(up->curbootfd, path, newpath);
        if (rval)       goto finish;

        // remove magic type/creator
        if (-1 == (fd=sopen(up->curbootfd, newpath, O_RDWR, 0)))  goto finish;
        if(fsetxattr(fd,XATTR_FINDERINFO_NAME,&nulltype,sizeof(nulltype),0,0)) {
            goto finish;
        }
    } 

    up->changestate = noLabel;
    rval = 0;

finish:
    if (fd != -1)   close(fd);

    if (rval) {
        OSKextLog(NULL, up->errLogSpec,
                  "%s - Error moving aside old label. errno %d %s.", 
                  __FUNCTION__, errno, strerror(errno));
   }

    return rval;
}

/******************************************************************************
* nukeBRLabels gets rid of the label and .contentDetails files
* no label -> hint of indeterminate state (label key in plist/other file?)
* Leopard: put/switch in some sort of "(updating!)" label (see BL[ess] routines)
******************************************************************************/
static int
nukeBRLabels(struct updatingVol *up)
{
    int rval;       // assigned to firstErr below
    int opres, firstErrno, firstErr = 0;
    char labelp[PATH_MAX];
    struct stat sb;
    
   OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
              "Removing current disk label.");

    // .disk_label
    pathcpy(labelp, up->curMount);
    pathcat(labelp, up->caches->label->rpath);
    if (0 == (stat(labelp, &sb))) {
        opres = sunlink(up->curbootfd, labelp);
        RECERR(up, opres, "error removing label" /*NULL w/9217695*/);
    } else {
        errno = 0;
    }

    // .disk_label_2x
    pathcpy(labelp, up->curMount);
    pathcat(labelp, up->caches->label->rpath);
    pathcat(labelp, SCALE_2xEXT);       // append extension
    if (0 == (stat(labelp, &sb))) {
        opres = sunlink(up->curbootfd, labelp);
        RECERR(up, opres, "error removing .contentDetails" /*NULL w/9217695*/);
    } else {
        errno = 0;
    }

    // .disk_label.contentsDetail
    pathcpy(labelp, up->curMount);
    pathcat(labelp, up->caches->label->rpath);
    pathcat(labelp, CONTENTEXT);        // append extension
    if (0 == (stat(labelp, &sb))) {
        opres = sunlink(up->curbootfd, labelp);
        RECERR(up, opres, "error removing .contentDetails" /*NULL w/9217695*/);
    } else {
        errno = 0;
    }

    up->changestate = noLabel;

    if (firstErr == -1)     errno = firstErrno;
    rval = firstErr;

finish:
    if (rval)
        OSKextLog(NULL, kOSKextLogErrorLevel, "Error removing disk label.");

    return rval;
}

/******************************************************************************
* ucopyBooters unlink/copies down booters but doesn't bless them
******************************************************************************/
static int ucopyBooters(struct updatingVol *up)
{
    int rval = ELAST + 1;
    int bsderr;
    char srcpath[PATH_MAX], oldpath[PATH_MAX];
    int nbooters = 0;

    if (up->caches->ofbooter.rpath[0])      nbooters++;
    if (up->caches->efibooter.rpath[0])     nbooters++;
    OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
              "Copying new booter%s.", nbooters == 1 ? "" : "s");
    
    // copy BootX, boot.efi
    up->changestate = copyingOFBooter;
    if (up->caches->ofbooter.rpath[0]) {
        // pathc*() seed errno
        pathcpy(srcpath, up->caches->root);
        pathcat(srcpath, up->caches->ofbooter.rpath);   // <root>/S/L/CS/BootX
        pathcpy(up->ofdst, up->curMount);
        pathcat(up->ofdst, up->caches->ofbooter.rpath); // <boot>/S/L/CS/BootX
        pathcpy(oldpath, up->ofdst);
        pathcat(oldpath, OLDEXT);                  // <boot>/S/L/CS/BootX.old

        (void)sunlink(up->curbootfd, oldpath);
        bsderr = srename(up->curbootfd, up->ofdst, oldpath);
        if (bsderr && errno !=ENOENT) {
            OSKextLog(NULL, up->errLogSpec, 
                      "%s - Error rename old %s new %s",
                      __FUNCTION__, up->ofdst, oldpath);
            goto finish;
        }
        if ((bsderr = scopyitem(up->caches->cachefd, srcpath,
                                up->curbootfd, up->ofdst))) {
            OSKextLog(NULL, up->errLogSpec, "%s - Error copying %s to %s",
                      __FUNCTION__, srcpath, up->ofdst);
            goto finish;
        }
    }

    up->changestate = copyingEFIBooter;
    if (up->caches->efibooter.rpath[0]) {
        // pathc*() seed errno
        pathcpy(srcpath, up->caches->root);
        pathcat(srcpath, up->caches->efibooter.rpath);   // ... boot.efi
        pathcpy(up->efidst, up->curMount);
        pathcat(up->efidst, up->caches->efibooter.rpath);
        pathcpy(oldpath, up->efidst);
        pathcat(oldpath, OLDEXT);

        (void)sunlink(up->curbootfd, oldpath);
        bsderr = srename(up->curbootfd, up->efidst, oldpath);
        if (bsderr && errno != ENOENT) {
            OSKextLog(NULL, up->errLogSpec, 
                      "%s - Error rename old %s new %s",
                      __FUNCTION__, up->efidst, oldpath);
            goto finish;
        }
        if ((bsderr = scopyitem(up->caches->cachefd, srcpath,
                                up->curbootfd, up->efidst))) {
            OSKextLog(NULL, up->errLogSpec, "%s - Error copying %s to %s",
                      __FUNCTION__, srcpath, up->efidst);
            goto finish;
        }
    }

    up->changestate = copiedBooters;
    rval = 0;

finish:
    if (rval)
        OSKextLog(NULL, up->errLogSpec, 
                  "%s - Error copying booter.  errno %d %s",
                  __FUNCTION__, errno, strerror(errno));

    return rval;
}


// booters have worst critical:fragile ratio (basically point of no return)
/******************************************************************************
* bless recently-copied booters
* operatens entirely on up->??dst which allows revertState to use it ..?
******************************************************************************/
#define CLOSE(fd) do { (void)close(fd); fd = -1; } while(0)
static int activateBooters(struct updatingVol *up)
{
    int rval = ELAST + 1;
    int fd = -1;
    uint32_t vinfo[8] = { 0, };
    struct stat sb;
    char parent[PATH_MAX];
    int nbooters = 0;

    if (up->caches->ofbooter.rpath[0])      nbooters++;   
    if (up->caches->efibooter.rpath[0])     nbooters++;    

    OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
              "Activating new booter%s.", nbooters == 1 ? "" : "s");
    
    // activate BootX, boot.efi
    up->changestate = activatingOFBooter;
    if (up->caches->ofbooter.rpath[0]) {
        unsigned char tbxichrp[32] = {'t','b','x','i','c','h','r','p','\0',};

        // flush booter bytes to disk (really)
        if (-1 == (fd=sopen(up->curbootfd, up->ofdst, O_RDWR, 0)))  goto finish;
        if (fcntl(fd, F_FULLFSYNC))                                 goto finish;

        // apply type/creator (assuming same folder as previous, now active)
        if(fsetxattr(fd,XATTR_FINDERINFO_NAME,&tbxichrp,sizeof(tbxichrp),0,0)) {
            goto finish;
        }
        CLOSE(fd);

        // get fileID of booter's enclosing folder 
        pathcpy(parent, dirname(up->ofdst));
        if (-1 == (fd=sopen(up->curbootfd, parent, O_RDONLY, 0)))  goto finish;
        if (fstat(fd, &sb))                                 goto finish;
        CLOSE(fd);
        if (sb.st_ino < (__darwin_ino64_t)2<<31) {
            vinfo[kSystemFolderIdx] = (uint32_t)sb.st_ino;
        } else {
            rval = EOVERFLOW;
            goto finish;
        }
    }

    up->changestate = activatingEFIBooter;
    if (up->caches->efibooter.rpath[0]) {
        // sync to disk
        if (-1==(fd=sopen(up->curbootfd, up->efidst, O_RDONLY, 0))) goto finish;
        if (fcntl(fd, F_FULLFSYNC))                                 goto finish;

        // get file ID
        if (fstat(fd, &sb))     goto finish;
        CLOSE(fd);
        if (sb.st_ino < (__darwin_ino64_t)2<<31) {
            vinfo[kEFIBooterIdx] = (uint32_t)sb.st_ino;
        } else {
            rval = EOVERFLOW;
            goto finish;
        }

        // since Inca has only one booter, but we want a blessed folder
        if (!vinfo[0]) {
            // get fileID of booter's enclosing folder 
            pathcpy(parent, dirname(up->efidst));
            if (-1 == (fd=sopen(up->curbootfd, parent, O_RDONLY, 0))) {
                goto finish;
            }
            if (fstat(fd, &sb))                             goto finish;
            CLOSE(fd);
            if (sb.st_ino < (__darwin_ino64_t)2<<31) {
                vinfo[kSystemFolderIdx] = (uint32_t)sb.st_ino;
            } else {
                rval = EOVERFLOW;
                goto finish;
            }
        }
    }

    // blessing efiboot/sysfolder happens by updating the root of the volume
    if ((rval = sBLSetBootFinderInfo(up, vinfo)))        goto finish;    
    
    up->changestate = activatedBooters;

finish:
    if (fd != -1)   close(fd);

    if (rval)
        OSKextLog(NULL, kOSKextLogErrorLevel, "Error activating booter.");

    return rval;
}

/******************************************************************************
* leap-frog w/rename()
******************************************************************************/
static int activateRPS(struct updatingVol *up)
{
    int rval = ELAST + 1;
    char prevRPS[PATH_MAX], curRPS[PATH_MAX], nextRPS[PATH_MAX];
    
    OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
              "Activating files used by the booter.");

    if (FindRPSDir(up, prevRPS, curRPS, nextRPS))   goto finish;

    // if current != the one we just populated
    if (strncmp(curRPS, up->curRPS, PATH_MAX) != 0) {
        // rename prev -> next ... done!?
        if (srename(up->curbootfd, prevRPS, nextRPS))   goto finish;
    }

    // thwunk everything to disk (now that essential boot files are in place)
    if (fcntl(up->curbootfd, F_FULLFSYNC))              goto finish;

    rval = 0;

finish:
    if (rval) {
        OSKextLog(NULL, kOSKextLogErrorLevel,
              "Error activating files used by the booter.");
    }

    return rval;
}


/******************************************************************************
* activateMisc renames .new files to final names and relabels the volumes
* active label indicates an updated helper partition
* - construct new label with a trailing number as appropriate
* - use BLGenerateLabelData() and overwrite any copied-down label
* X need to be consistent throughout regarding missing misc files (esp. label?)
******************************************************************************/
#ifndef OPENSOURCE      // BLGenerateLabelData() uses CG?
static int writeLabels(struct updatingVol *up, char *labelpath)
{
    int temp_err, rval = ELAST + 1;
    CFDataRef lData = NULL;
    CFIndex len;
    int fd = -1;
    char bootname[NAME_MAX];
    char path[PATH_MAX];
    // if .disk_label stayed up to date, we wouldn't need to call this
    // with w/1 Apple_Boot.
    char *fmt = (CFArrayGetCount(up->boots) == 1) ? "%s" : "%s %d";
    
    OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
              "Writing new disk label.");

    // create name & parent directory for .disk_label* files
    if (NAME_MAX <= snprintf(bootname, NAME_MAX, fmt,
                            up->caches->volname, up->bootIdx + 1))
        goto finish;
    pathcpy(path, dirname(labelpath));
    if (-1 == sdeepmkdir(up->curbootfd, path, kCacheDirMode))
        goto finish;

    // generate and write .disk_label
    temp_err = BLGenerateLabelData(NULL, bootname, kBitmapScale_1x, &lData);
    if (temp_err) {
        // BLGenerateLabelData() will return a non-zero error code that
        // indicates at what point it failed within the function.
        OSKextLog(NULL, up->errLogSpec, "%s - BLGenerateLabelData(): %d", 
                  __FUNCTION__, temp_err);
        goto finish;
    }
    fd = sopen(up->curbootfd, labelpath, O_CREAT|O_WRONLY, kCacheFileMode);
    if (fd == -1)           goto finish;
    len = CFDataGetLength(lData);
    if (write(fd, CFDataGetBytePtr(lData), len) != len) goto finish;

    // generate and write .disk_label_2x
    CFRelease(lData);   lData = NULL;
    temp_err = BLGenerateLabelData(NULL, bootname, kBitmapScale_2x, &lData);
    if (temp_err) {
        // BLGenerateLabelData() will return a non-zero error code that
        // indicates at what point it failed within the function.
        OSKextLog(NULL, up->errLogSpec, "%s - BLGenerateLabelData(): %d", 
                  __FUNCTION__, temp_err);
        goto finish;
    }
    pathcpy(path, labelpath);
    pathcat(path, SCALE_2xEXT);
    fd = sopen(up->curbootfd, path, O_CREAT|O_WRONLY, kCacheFileMode);
    if (fd == -1)           goto finish;
    len = CFDataGetLength(lData);
    if (write(fd, CFDataGetBytePtr(lData), len) != len) goto finish;

    // and write the content detail
    pathcpy(path, labelpath);
    pathcat(path, CONTENTEXT);
    close(fd);
    fd = sopen(up->curbootfd, path, O_CREAT|O_WRONLY, kCacheFileMode);
    if (fd == -1)           goto finish;
    len = strlen(bootname);
    if (write(fd, bootname, len) != len)        goto finish;

    rval = 0;

finish:
    if (rval)
        OSKextLog(NULL, up->errLogSpec,
                  "%s - Warning: trouble writing disk label: %d %s.", 
                  __FUNCTION__, errno, strerror(errno));
    
    if (fd != -1)   close(fd);
    if (lData)      CFRelease(lData);

// XX fix & delete
#ifdef WHEN_9028349_FIXED
    return rval;
#else
    return 0;
#endif
}
#endif  // OPENSOURCE

static int activateMisc(struct updatingVol *up)     // rename the .new
{
    int rval = ELAST + 1;
    char path[PATH_MAX], opath[PATH_MAX];
    unsigned i = 0, nprocessed = 0;
    int fd = -1;
    struct stat sb;
    unsigned char tbxjchrp[32] = { 't','b','x','j','c','h','r','p','\0', };
   
    if (up->doMisc) {
        OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
                  "Activating files used before the booter runs.");
        
        // do them all
        for (i = 0; i < up->caches->nmisc; i++) {
            if (strlcpy(path, up->curMount, PATH_MAX) >= PATH_MAX)   continue;
            if (strlcat(path, up->caches->miscpaths[i].rpath, PATH_MAX) 
                        > PATH_MAX)   continue;
            if (strlcpy(opath, path, PATH_MAX) >= PATH_MAX)     continue;
            if (strlcat(opath, NEWEXT, PATH_MAX) >= PATH_MAX)   continue;

            if (stat(opath, &sb) == 0) {
                if (srename(up->curbootfd, opath, path))        continue;
            }

            nprocessed++;
        }

    }

    pathcpy(path, up->curMount);
    pathcat(path, up->caches->label->rpath);
#ifndef OPENSOURCE
    // expectUpToDate is trying to avoid having to render labels.
    // If there's only one Apple_Boot, we don't need a custom label.
    // Unfortunately, the .disk_label file isn't updated aggressively.
    if (up->expectUpToDate) {
                                // || CFArrayGetCount(up->boots) == 1
#endif
        // move label back
        char newpath[PATH_MAX];

        pathcpy(newpath, path);     // just rename
        pathcat(newpath, NEWEXT);
        (void)srename(up->curbootfd, newpath, path);
#ifndef OPENSOURCE
    } else {
        // write label
        (void)sunlink(up->curbootfd, path);
        if (writeLabels(up, path))          goto finish;
#endif
    }

    // assign type/creator to the label
    if (0 == (stat(path, &sb))) {
        if (-1 == (fd = sopen(up->curbootfd, path, O_RDWR, 0)))   goto finish;

        if (fsetxattr(fd,XATTR_FINDERINFO_NAME,&tbxjchrp,sizeof(tbxjchrp),0,0))
            goto finish;
        close(fd); fd = -1;
    }

    rval = (i != nprocessed);

finish:
    if (fd != -1)   close(fd);

    if (rval) {
        OSKextLog(NULL, kOSKextLogErrorLevel,
                  "Error activating files used before the booter runs.");
    }

    return rval;
}

/******************************************************************************
* get rid of everything "extra"
******************************************************************************/
static int nukeFallbacks(struct updatingVol *up)
{
    int rval = 0;               // OR-ative return value
    int bsderr;
    char delpath[PATH_MAX];
    struct bootCaches *caches = up->caches;

    OSKextLog(NULL, kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
              "Cleaning up fallbacks.");

    // using pathcpy b/c if that's failing, it's worth bailing
    // XX should probably only try to unlink if present

    // maybe mount failed (in which there aren't any fallbacks
    if (!up->curBoot)   goto finish;

    // if needed, unlink .old booters
    if (up->doBooters) {
        if (caches->ofbooter.rpath[0]) {
            makebootpath(delpath, caches->ofbooter.rpath);
            pathcat(delpath, OLDEXT);
            if ((bsderr = sunlink(up->curbootfd, delpath)) && errno != ENOENT) {
                rval |= bsderr;
            }
        }
        if (caches->efibooter.rpath[0]) {
            makebootpath(delpath, caches->efibooter.rpath);
            pathcat(delpath, OLDEXT);
            if ((bsderr = sunlink(up->curbootfd, delpath)) && errno != ENOENT) {
                rval |= bsderr;
            }
        }
    }

    // if needed, erase prevRPS
    // which, conveniently, will be right regardless of whether we succeeded
    if (up->doRPS) {
        char ignore[PATH_MAX];

        if (0 == FindRPSDir(up, delpath, ignore, ignore)) {
            // eraseRPS ignores if missing
            rval |= eraseRPS(up, delpath);
        }
    }

finish:
    if (rval)
        OSKextLog(NULL, kOSKextLogErrorLevel, "Error cleaning up fallbacks.");

    return rval;
}

/*********************************************************************
// XXX not yet used / tested
*********************************************************************/
static int kill_kextd(void)
{
    int           result         = -1;
    kern_return_t kern_result    = kOSReturnError;
    mach_port_t   bootstrap_port = MACH_PORT_NULL;
    mach_port_t   kextd_port     = MACH_PORT_NULL;
    int           kextd_pid      = -1;

    kern_result = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
    if (kern_result != kOSReturnSuccess) {
        goto finish;
    }

    kern_result = bootstrap_look_up(bootstrap_port,
        (char *)KEXTD_SERVER_NAME, &kextd_port);
    if (kern_result != kOSReturnSuccess) {
        goto finish;
    }

    kern_result = pid_for_task(kextd_port, &kextd_pid);
    if (kern_result != kOSReturnSuccess) {
        goto finish;
    }

    result = kill(kextd_pid, SIGKILL);
    if (-1 == result) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
             "kill kextd failed - %s.", strerror(errno));
    }

finish:
    if (kern_result != kOSReturnSuccess) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
             "kill kextd failed - %s.", safe_mach_error_string(kern_result));
    }
    return result;
}

/******************************************************************************
// XXX not yet used / tested  
******************************************************************************/
int renameBootcachesPlist(
    char * hostVolume,
    char * oldPlistPath,
    char * newPlistPath)
{
    int    result            = -1;
    int    bootcachesPlistFd = -1;
    char * errorMessage      = NULL;
    char * errorPath         = NULL;
    char   oldname[PATH_MAX];
    char   newname[PATH_MAX];
    char * kextcacheArgs[] = {
        "/usr/sbin/kextcache",
        "-f",
        "-u",
        NULL, // replace with hostVolume
        NULL };

    errorMessage = "path concatenation error";
    errorPath = hostVolume;

    if (strlcpy(oldname, hostVolume, PATH_MAX) >= PATH_MAX) {
        goto finish;
    }
    if (strlcpy(newname, hostVolume, PATH_MAX) >= PATH_MAX) {
        goto finish;
    }

    errorPath = oldPlistPath;
    if (strlcpy(oldname, oldPlistPath, PATH_MAX) >= PATH_MAX) {
        goto finish;
    }

    errorPath = newPlistPath;
    if (strlcpy(newname, newPlistPath, PATH_MAX) >= PATH_MAX) {
        goto finish;
    }

    errorPath = oldname;
    bootcachesPlistFd = open(oldname, O_RDONLY);
    if (-1 == bootcachesPlistFd) {
        errorMessage = strerror(errno);
        goto finish;
    }
    
    if (-1 == srename(bootcachesPlistFd, oldname, newname)) {
        errorMessage = "rename failed.";
        goto finish;
    }
    
    errorMessage = "couldn't kill kextd";
    if (-1 == kill_kextd()) {
        goto finish;
    }

   /* Do we want to check fork_program's return value?
    */
    kextcacheArgs[3] = hostVolume;
    result = fork_program(kextcacheArgs[0], kextcacheArgs, true /* wait */);

finish:
    if (errorMessage) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
             "%s - %s.", errorPath, errorMessage);
    }
    if (bootcachesPlistFd >= 0) {
        close(bootcachesPlistFd);
    }
    return result;
}

/******************************************************************************
// XXX remove prototype code once library is complete
******************************************************************************/
#if 0
int activateBootFiles(
    char              * hostVolume,
    char              * sourceVolume,
    CFPropertyListRef   efiBootPlist)
{
    int                 result              = -1;
    char              * errorMessage        = NULL;
    char              * errorPath           = NULL;
    struct updatingVol  up                  = { NULL, };

   /*****
    * Rename the host volume's bootcaches.plist file and kill kextd so that
    * things don't get updated.
    */
    if (renameBootcachesPlist(hostVolume, kBootCachesPath,
        kBootCachesDisabledPath)) {

        // rename func prints error
        goto finish;
    }

    if (-1 == kill_kextd()) {
        goto finish;
    }
    
   /*****
    * Now read the bootcaches.plist file from the source volume,
    * retarget it to the host volume, and copy files from it.
    */
    up.caches = readBootCachesForVolPath(sourceVolume);
    if (!up.caches) {
        // read should have logged error
        goto finish;
    }

    errorMessage = "path concatenation error";
    errorPath = hostVolume;
    if (strlcpy(up.caches->root, hostVolume, sizeof(up.caches->root)) > sizeof(up.caches->root)) {
        goto finish;
    }
    errorMessage = NULL;
    errorPath = NULL;

    up.efiBootPlist = efiBootPlist;

    if (!hasBootRootBoots(up.caches, &up.boots,
        /* dataPartitions */ NULL, /* isAPM */ &up.onAPM))
    {
        // log error?
        // special return value to let caller know they need to bless?
        goto finish;
    }

    errorMessage = "path concatenation error";
    errorPath = hostVolume;
    if (strlcpy(up.caches->root, sourceVolume, sizeof(up.caches->root)) >
        sizeof(up.caches->root))
    {
        goto finish;
    }
    errorMessage = NULL;
    errorPath = NULL;

    // Begin work on actual update          [updateBoots vs. checkUpdateBoots?]
    errorMessage = "trouble updating one or more helper partitions";
    errorPath = hostVolume;

    result = updateBootHelpers(&up, /* expectUpToDate */ FALSE);
    if (result != 0) {
        goto finish;
    }

    errorMessage = NULL;
    errorPath = NULL;

finish:
    if (errorMessage) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
             "%s - %s.", errorPath, errorMessage);
    }
    
    SAFE_RELEASE(up.boots);
    if (up.caches) destroyCaches(up.caches);
    return result;
}
#endif

/******************************************************************************
// XXX remove prototype code once library is complete
******************************************************************************/
int revertBootFiles(char * hostVolume)
{
    int    result       = -1;
    char * errorMessage = NULL;
    char * errorPath    = NULL;

    if (renameBootcachesPlist(hostVolume, kBootCachesDisabledPath,
        kBootCachesPath)) {

        // rename func prints error
        goto finish;
    }
    
    // // // kill kextd // // //

    // uses system-installed kextcache
    (void)launch_rebuild_all(hostVolume, /*force*/ true, /*wait*/ true);
    
    result = 0;

finish:
    if (errorMessage) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
             "%s - %s.", errorPath, errorMessage);
    }
    return result;
}


/******************************************************************************
* takeVolumeForPath turns the path into a volume UUID and locks with kextd
******************************************************************************/
// upstat() stat()s "up" the path if a file doesn't exist
static int upstat(const char *path, struct stat *sb, struct statfs *sfs)
{
    int rval = ELAST+1;
    char buf[PATH_MAX], *tpath = buf;
    struct stat defaultsb;

    if (strlcpy(buf, path, PATH_MAX) > PATH_MAX)        goto finish;

    if (!sb)    sb = &defaultsb;
    while ((rval = stat(tpath, sb)) == -1 && errno == ENOENT) {
        // "." and "/" should always exist, but you never know
        if (tpath[0] == '.' && tpath[1] == '\0')  goto finish;
        if (tpath[0] == '/' && tpath[1] == '\0')  goto finish;
        tpath = dirname(tpath);     // Tiger's dirname() took const char*
    }

    // call statfs if the caller needed it
    if (sfs)
        rval = statfs(tpath, sfs);

finish:
    if (rval) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogFileAccessFlag,
                "Couldn't find volume for %s.", path);
    }

    return rval;
}


/******************************************************************************
* theoretically, takeVolumeForPaths() ensured all paths are on the given
* volume, then locked
******************************************************************************/
// int takeVolumeForPaths(char *volPath)

/******************************************************************************
* takeVolumeForPath() is all we ended up needing ...
* can return success if a lock isn't needed
* can return failure if sBRUptLock is already in use
******************************************************************************/
#define WAITFORLOCK 1
int takeVolumeForPath(const char *path)
{
    int rval = ELAST + 1;
    kern_return_t macherr = KERN_SUCCESS;
    int lckres = 0;
    struct statfs sfs;
    const char *volPath = "<unknown>";  // llvm can't track lckres/macherr
    mach_port_t taskport = MACH_PORT_NULL;

    if (sBRUptLock) {
        return EALREADY;        // only support one lock at a time
    }

    if (geteuid() != 0) {
        // kextd shouldn't be watching anything you can touch
        // and ignores locking requests from non-root anyway
        OSKextLog(NULL, kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                 "Warning: non-root can't lock the volume for %s", path);
        rval = 0;
        goto finish;
    }

    // look up kextd port if not cached
    // XX if there's a way to know kextd isn't already running, we could skip
    // unnecessarily bringing it up in the boot-time case (see 5108882).
    if (!sKextdPort) {
        macherr=bootstrap_look_up(bootstrap_port,KEXTD_SERVER_NAME,&sKextdPort);
        if (macherr)  goto finish;
    }

    // get the volume's UUID
    if ((rval = upstat(path, NULL, &sfs)))      goto finish;
    volPath = sfs.f_mntonname;
    if ((rval = copyVolumeUUIDs(volPath, s_vol_uuid, NULL))) {
        goto finish;
    }
    
    // allocate a port to pass (in case we die -- kernel cleans up on exit())
    taskport = mach_task_self();
    if (taskport == MACH_PORT_NULL)  goto finish;
    macherr = mach_port_allocate(taskport,MACH_PORT_RIGHT_RECEIVE,&sBRUptLock);
    if (macherr)  goto finish;

    // try to take the lock; warn if it's busy and then wait for it
    // X kextcache -U, if it is going to lock at all, needs only WAITFORLOCK
    macherr = kextmanager_lock_volume(sKextdPort, sBRUptLock, s_vol_uuid,
                                      !WAITFORLOCK, &lckres);
    if (macherr)        goto finish;

    // 5519500: sleep until kextd is up and running (w/diskarb, etc)
    while (lckres == EAGAIN) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
            "kextd wasn't ready; waiting 10 seconds and trying again.");
        sleep(10);
        macherr = kextmanager_lock_volume(sKextdPort, sBRUptLock, s_vol_uuid,
                                          !WAITFORLOCK, &lckres);
        if (macherr)    goto finish;
    }

    // With kextd set up, we sleep until the volume is free.
    // WAITFORLOCK should cause the function not to return w/o the lock
    // but 8679674 suggested something was going awry so we'll retry.
    while (lckres == EBUSY) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
            "%s locked; waiting for lock.", volPath);
        macherr = kextmanager_lock_volume(sKextdPort, sBRUptLock, s_vol_uuid,
                                          WAITFORLOCK, &lckres);
        if (macherr)    goto finish;
        if (lckres == 0) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                "Lock acquired; proceeding.");
        }
    }

    
    // kextd might not be watching this volume (isn't currently competing)
    // so we set our success to the existance of the volume's root
    if (lckres == ENOENT) {
        struct stat sb;
        rval = stat(volPath, &sb);
        if (rval == 0) {
            OSKextLog(NULL, kOSKextLogProgressLevel | kOSKextLogGeneralFlag,
                "WARNING: kextd not watching %s; proceeding w/o lock", volPath);
        }
    } else {
        rval = lckres;
    }

finish: 
    if (sBRUptLock != MACH_PORT_NULL && (lckres != 0 || macherr)) {
        mach_port_mod_refs(taskport, sBRUptLock, MACH_PORT_RIGHT_RECEIVE, -1);
        sBRUptLock = MACH_PORT_NULL;
    }

    /* XX needs unraveling XX */
    // if kextd isn't competing with us, then we didn't need the lock
    if (macherr == BOOTSTRAP_UNKNOWN_SERVICE) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
            "WARNING: kextd unavailable; proceeding w/o lock for %s", volPath);
        rval = 0;
    } else if (macherr) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
            "Couldn't lock %s: %s (%d).", path,
            safe_mach_error_string(macherr), macherr);
        rval = macherr;
    } else {
        // dump rval
        if (rval == -1) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogWarningLevel | kOSKextLogFileAccessFlag,
                "Couldn't lock %s.", path);
            rval = errno;
        } else if (rval) {
            // lckres == EAGAIN should get here
            OSKextLog(/* kext */ NULL,
                kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                "Couldn't lock %s: %s", path, strerror(rval));
        }
    }

    return rval;
}

/******************************************************************************
* putVolumeForPath will unlock the relevant volume, passing 'status' to
* inform kextd whether we succeded, failed, or just need more time
******************************************************************************/
int putVolumeForPath(const char *path, int status)
{
    int rval = KERN_SUCCESS;

    // if not locked, don't sweat it
    if (sBRUptLock == MACH_PORT_NULL)
        goto finish;

    rval = kextmanager_unlock_volume(sKextdPort,sBRUptLock,s_vol_uuid,status);

    // tidy up; the server will clean up its stuff if we die prematurely
    mach_port_mod_refs(mach_task_self(),sBRUptLock,MACH_PORT_RIGHT_RECEIVE,-1);
    sBRUptLock = MACH_PORT_NULL;

finish:
    if (rval) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
            "Couldn't unlock volume for %s: %s (%d).",
            path, safe_mach_error_string(rval), rval);
    }

    return rval;
}
