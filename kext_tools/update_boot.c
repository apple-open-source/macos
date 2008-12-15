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
 * FILE: update_boot.c
 * AUTH: Soren Spies (sspies)
 * DATE: 8 June 2006
 * DESC: implement 'kextcache -u' (copying to Apple_Boot partitions)
 *
 */

#include <bless.h>
#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <sysexits.h>
#include <sys/mount.h>
#include <sys/xattr.h>
#include <unistd.h>
#include <IOKit/kext/kextmanager_types.h>

#include <bootfiles.h>
#include <CoreFoundation/CoreFoundation.h>
#include <TargetConditionals.h>
#if TARGET_OS_EMBEDDED
#define kDAReturnBusy 2
#define err_local_diskarbitration err_sub( 0x368 )
#else
#include <DiskArbitration/DiskArbitration.h>
#include <DiskArbitration/DiskArbitrationPrivate.h>
#endif

#include "bootcaches.h"
#include "logging.h"
#include "safecalls.h"
#include "update_boot.h"
#include "utility.h"        // g_verbose_level

enum bootReversions {
    nothingSerious = 0,
    noLabels,               // 1
    copyingOFBooter,        // 2
    copyingEFIBooter,       // 3
    copiedBooters,          // 4
    activatingOFBooter,     // 5
    activatingEFIBooter,    // 6
    activatedBooters        // 7
};

const char * bootReversionsStrings[] = {
    NULL,           // unused
    "Labels deleted",
    "Unlinking and copying BootX booter",
    "Unlinking and copying EFI booter",
    "Booters copied",
    "Activating BootX",
    "Activating EFI booter",
    "Booters activated"
};

#define COMPILE_TIME_ASSERT(pred) switch(0){case 0:case pred:;}

// for non-RPS content, including booters
#define OLDEXT ".old"
#define NEWEXT ".new"
#define CONTENTEXT ".contentDetails"

// NOTE: These strings must be the same length, or code in ucopyRPS will break!
// There is a compile time assert in the function to this effect.
#define BOOTPLIST_NAME "com.apple.Boot.plist"
#define BOOTPLIST_APM_NAME "com.apple.boot.plist"

// for Apple_Boot update
struct updatingVol {
    struct bootCaches *caches;          // parsed bootcaches.plist data
    Boolean doRPS, doMisc, doBooters;   // what needs updating
    Boolean expectUpToDate;             // expecting things to be right (-U)
    enum bootReversions changestate;    // track changes for rolling back
    CFArrayRef boots;                   // BSD Names of Apple_Boot partitions
    DASessionRef dasession;             // handle to diskarb

    // changed for each Apple_Boot
    int bootIdx;                        // which helper are we updating
    char bsdname[DEVMAXPATHSIZE];       // bsdname of Apple_Boot
    DADiskRef curBoot;                  // and matching diskarb ref
    char curMount[MNAMELEN];            // path to current boot mountpt
    int curbootfd;                      // Sec: handle to curMount
    char curRPS[PATH_MAX];              // RPS dir inside
    char efidst[PATH_MAX], ofdst[PATH_MAX];
    Boolean isGPT;
};

// diskarb
static int mountBoot(struct updatingVol *up);
static int unmountBoot(struct updatingVol *up);

// ucopy = unlink & copy
// no race for RPS, so install it first
static int ucopyRPS(struct updatingVol *s);  // nuke/copy to inactive
// labels (e.g.) have no fallback, .new is harmless
// XX ucopy"Preboot/Firmware"
static int ucopyMisc(struct updatingVol *s);	    // use/overwrite .new names
// booters have fallback paths, but originals might be broken
static int ucopyBooters(struct updatingVol *s);     // nuke/copy booters (inact)
// no label -> hint of indeterminate state (label key in plist?)
static int moveLabels(struct updatingVol *s);	    // move aside
static int nukeLabels(struct updatingVol *s);	    // byebye (all?)
// booters have worst critical:fragile ratio (point of departure)
static int activateBooters(struct updatingVol *s);  // bless new names
// and the RPS data needed for booting
static int activateRPS(struct updatingVol *s);	    // leap-frog w/rename()
// finally, the labels (indicating a working system)
// XX activate"FirmwarePaths/postboot"
static int activateMisc(struct updatingVol *s);     // rename .new / label
// and now that we're safe
static int nukeFallbacks(struct updatingVol *s);

// cleanup routines (RPS is the last step; activateMisc handles label)
static int revertState(struct updatingVol *up);

/* Chain of Trust
 * Our goal is to do anything the bootcaches.plist says, but only to that vol.
 * #1 we only pay attention to root-owned bootcaches.plist files
 * #2 we get an fd to the bootcaches.plist		[trust is here]
// * #3 we validate the bc.plist fd after getting an fd to the volume's root
 * #4 we use stored bsdname for libbless
 * #5 we validate cachefd after the call to bless	[trust -> bsdname]
 * #6 we get curbootfd after each apple_boot mount
 * #7 we validate cachefd after the call		[trust -> curfd]
 * #8 operations take an fd limiting their scope to the mount
 */

// ? do these *need* do { } while() wrappers?
// XX should probably rename to all-caps
#define pathcpy(dst, src) do { \
	if (strlcpy(dst, src, PATH_MAX) >= PATH_MAX)  goto finish; \
    } while(0)
#define pathcat(dst, src) do { \
	if (strlcat(dst, src, PATH_MAX) >= PATH_MAX)  goto finish; \
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

/*******************************************************************************
* updateBoots will lock the volume and update the booter partitions 
* Sec: must ensure each target is one of the source's Apple_Boot partitions
*******************************************************************************/
int updateBoots(char *volRoot, int filec, const char *files[],
		       Boolean force, Boolean expectUpToDate)
{
    int rval = ELAST + 1;
    char *errmsg;
    struct updatingVol up = { NULL, };
    up.curbootfd = -1;
    struct stat sb;

    CFURLRef volURL = NULL;
    DADiskRef dadisk = NULL;
    CFIndex bootcount, bootupdates = 0;
    Boolean doAny;

    errmsg = "error getting description from Disk Arbitration";
#if TARGET_OS_EMBEDDED
    goto finish;
#else
    up.dasession = DASessionCreate(nil);
    if (!up.dasession)	    goto finish;
#endif
    volURL = CFURLCreateFromFileSystemRepresentation(nil, (UInt8*)volRoot,
		strlen(volRoot) + 1, true /*isDir*/);
    if (!volURL)    	    goto finish;
#if !TARGET_OS_EMBEDDED
    dadisk = DADiskCreateFromVolumePath(nil, up.dasession, volURL);
#endif
    if (!dadisk)    	    goto finish;

    // first check for usable bootcaches.plist, else politely bail
    errmsg = NULL;  // locking, readCaches log their own errors
    if (!(up.caches = readCaches(dadisk))) {
	if (g_verbose_level)
	    kextd_log("%s: no "kBootCachesPath"; nothing to do", volRoot);
	rval = 0;
	goto finish;
    }

    // don't need lock/owners enabled in Leopard to *read* bootcaches.plist
    // but we do want it before checking the mkext (which may be being rebuilt)!
    // kextcache -U doesn't lock and kextd holds off on rebuilds for 60 seconds
    if (!expectUpToDate) {
	if (takeVolumeForPaths(volRoot, filec, files))  // lock (logs)
	    goto finish;  
    } else if (getppid() != 2 /* launchctl */) { 
	kextd_log("WARNING: no locks preventing kextd-triggered updates");
    }

    /* XX Sec reviewed: how we secure against replacing /'s mkext from external:
     * - mktmp/mkstmp used to create temp file at destination
     * - final rename must be on whatever volume provided the kexts
     * - if volume is /, then kexts owned by root can be trusted (4623559 fstat)
     * - otherwise, rename from wrong volume will fail
     */
    if (check_mkext(up.caches)) {
        // rebuild the mkext under our lock / lack thereof (-v not passed)
        if (g_verbose_level > 2)
	    kextd_log("Rebuilding out-of-date mkext");
	setenv("_com_apple_kextd_skiplocks", "1", 1);	    // no locks, child!
        if (rebuild_mkext(up.caches, true /*wait*/) != 0)
	    goto finish;
    }

    // hasBoots gets helpers from bless, GPT info from the registry
    errmsg = "couldn't get Apple_Boot information";
    if (!hasBoots(up.caches->bsdname, &up.boots, &up.isGPT)) {
        rval = 0;	// no boots -> nothing to do; byebye
        if (g_verbose_level > 0)
	    kextd_log("%s: no helper partitions to update", volRoot);
        goto finish;
    }

    if (fstat(up.caches->cachefd, &sb))     	// validate plist -> helpers
	goto finish;

    // 5158091: ignore Inca systems on non-GPT
    if (!up.isGPT && up.caches->ofbooter.rpath[0] == '\0') {
	rval = 0;
	if (g_verbose_level > 0)
	    kextd_log("%s only supports GPT-based helper partitions", volRoot);
	goto finish;
    }

    // Have boot partitions and want to update them
    errmsg = "trouble analyzing what needs updating";
    // needUpdates populates our timestamp values for applyStamps
    if (needUpdates(up.caches, &doAny, &up.doRPS, &up.doBooters, &up.doMisc))
	goto finish;
    if (!doAny && !force) {
        rval = 0;
        if (g_verbose_level > 0)
	    kextd_log("%s: helper partitions appear up to date", volRoot);
        goto finish;
    }
    if (force)	up.doRPS = up.doBooters = up.doMisc = true;
    up.expectUpToDate = expectUpToDate;

    // Begin work on actual update	    [updateBoots vs. checkUpdateBoots?]
    errmsg = "trouble updating one or more helper partitions";

#if !TARGET_OS_EMBEDDED
    // mountBoot and unmountBoot will spin the runloop for this DA session
    DASessionScheduleWithRunLoop(up.dasession, CFRunLoopGetCurrent(),
	    kCFRunLoopDefaultMode);
#endif
    bootcount = CFArrayGetCount(up.boots);
    for (up.bootIdx = 0; up.bootIdx < bootcount; up.bootIdx++) {
        up.changestate = nothingSerious;		// init state
        if ((mountBoot(&up)))           goto bootfail;  // sets curMount
        if (up.doRPS && ucopyRPS(&up))  goto bootfail;  // -> inactive
        if (up.doMisc)          (void) ucopyMisc(&up);  // -> .new files
        
        // get the labels out of the way (should be optional?)
        if (expectUpToDate) {
            if (moveLabels(&up))        goto bootfail;  
        } else {
            if (nukeLabels(&up))        goto bootfail;
        }
        
        if (up.doBooters && ucopyBooters(&up))	        // .old still active
            goto bootfail;
        if (up.doBooters && activateBooters(&up))	// oh boy
            goto bootfail;
        // new booters remain mostly compatible with old kernels (power outage!)
        if (up.doRPS && activateRPS(&up))	  	// mv to safety
            goto bootfail;
        if (activateMisc(&up))
	    goto bootfail;	// reverts label
        
        up.changestate = nothingSerious;
        bootupdates++;	    // loop success
        if (g_verbose_level > 0) {
            kextd_log("Successfully updated helper partition %s", up.bsdname);
        }
        
bootfail:
        if (g_verbose_level > 0 && up.changestate != nothingSerious) {
            kextd_error_log("error updating helper partition %s, state %d: %s", up.bsdname,
            up.changestate, bootReversionsStrings[up.changestate]);
        }
        // unroll any changes we may have made
        (void)revertState(&up);	    // smart enough to do nothing
        
        // always unmount
        if (nukeFallbacks(&up))
            kextd_error_log("helper partition %s may be untidy", up.bsdname);
        if (up.curBoot && unmountBoot(&up))
            kextd_error_log("couldn't unmount helper partition %s", up.bsdname);
    }
    if (bootupdates != bootcount)  goto finish;

    errmsg = "trouble updating bootstamps";
    if (applyStamps(up.caches))	    goto finish;

   /* We're here if we successfully updated the helpers.  If we were expecting
    * no updates to be needed, return EX_OSFILE, otherwise return ok.  */
    if (expectUpToDate) {
        rval = EX_OSFILE;
    } else {
        rval = EX_OK;
    }

finish:
    // since updateBoots() -> exit(), convert common to sysexits(3) values
    if (rval && rval != EX_OSFILE)
	rval = getExitValueFor(rval);
    putVolumeForPath(volRoot, rval);		// handles not locked (& logs)

    if (volURL)		    CFRelease(volURL);
    if (dadisk)		    CFRelease(dadisk);
    if (up.boots)	    CFRelease(up.boots);
    if (up.curbootfd != -1) close(up.curbootfd);
    if (up.dasession) {
#if !TARGET_OS_EMBEDDED
        DASessionUnscheduleFromRunLoop(up.dasession, CFRunLoopGetCurrent(),
		kCFRunLoopDefaultMode);
#endif
        CFRelease(up.dasession);
    }
    if (up.caches)	    destroyCaches(up.caches);

    if (rval && rval != EX_OSFILE && errmsg) {
	warnx("%s: %s", volRoot, errmsg);
    }

    return rval;
}

// ucopyBooters and activateBooters, backwards
static int revertState(struct updatingVol *up)
{
    int rval = 0;	// optimism to accumulate errors with |=
    char path[PATH_MAX], oldpath[PATH_MAX];
    struct bootCaches *caches = up->caches;
    Boolean doMisc;

    if (g_verbose_level > 2) kextd_log("Rolling back any incomplete updates");
	
    switch (up->changestate) {
	// inactive booters are still good
	case activatedBooters:
	    // we've blessed the new booters; so let's bless the old ones
	    pathcat(up->ofdst, OLDEXT);
	    pathcat(up->efidst, OLDEXT);
	    rval |= activateBooters(up);    // XX this should reactivate the old
	case activatingEFIBooter:
    	case activatingOFBooter:	    // unneeded since 'bless' is one op
	case copiedBooters:
    	case copyingEFIBooter:
	if (caches->efibooter.rpath[0]) {
	    makebootpath(path, caches->efibooter.rpath);
	    pathcpy(oldpath, path);	    // old ones are blessed; rename
	    pathcat(oldpath, OLDEXT);
	    (void)sunlink(up->curbootfd, path);
	    rval |= srename(up->curbootfd, oldpath, path);
	}

    	case copyingOFBooter:
	if (caches->ofbooter.rpath[0]) {
	    makebootpath(path, caches->ofbooter.rpath);
	    pathcpy(oldpath, path);
	    pathcat(oldpath, OLDEXT);
	    (void)sunlink(up->curbootfd, path);
	    rval |= srename(up->curbootfd, oldpath, path);
	}

	// XX
	// case copyingMisc:
	// would clean up the .new turds

	case noLabels:
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
    return rval;
};

/*******************************************************************************
* mountBoot digs in for the root, and mounts up the Apple_Boots
* mountpoints are stored in up->bootparts
*******************************************************************************/
static int mountBoot(struct updatingVol *up)
{
    int rval = ELAST + 1;
#if !TARGET_OS_EMBEDDED
    CFStringRef mountargs[] = { CFSTR("perm"), CFSTR("nobrowse"), NULL };
    CFStringRef str;
    DADissenterRef dis = (void*)kCFNull;
    CFDictionaryRef ddesc = NULL;
    CFURLRef volURL;
    struct statfs bsfs;
    struct stat secsb;

    if (g_verbose_level > 2) kextd_log("Mounting helper partition");

    // request the Apple_Boot mount
    str = (CFStringRef)CFArrayGetValueAtIndex(up->boots, up->bootIdx);
    if (!str)  goto finish;
    if (!CFStringGetFileSystemRepresentation(str, up->bsdname, DEVMAXPATHSIZE))
	goto finish;
    if (!(up->curBoot = DADiskCreateFromBSDName(nil,up->dasession,up->bsdname)))
	goto finish;
    
    if (g_verbose_level > 2) kextd_log("%s mounted", up->bsdname);

    // DADiskMountWithArgument might call _daDone before it returns (e.g. if it
    // knows your request is impossible ...)
    // _daDone updates our 'dis[senter]'
    DADiskMountWithArguments(up->curBoot, NULL/*mnt*/,kDADiskMountOptionDefault,
   			     _daDone, &dis, mountargs);

    // ... so we use kCFNull and check the value before CFRunLoopRun()
    if (dis == (void*)kCFNull)
	CFRunLoopRun();		// stopped by _daDone (which updates 'dis')
    if (dis) {
	rval = DADissenterGetStatus(dis);
	// if it's already mounted, try to unmount it? (XX skank DEBUG(?) hack)
	if (rval == kDAReturnBusy && up->curMount[0] != '\1') {
	    up->curMount[0] = '\1';
            if (0 == unmountBoot(up)) {
		// try again
                return mountBoot(up);
	    }
	}
	goto finish;
    }

    // get and stash the mountpoint of the boot partition
    if (!(ddesc = DADiskCopyDescription(up->curBoot)))  goto finish;
    volURL = CFDictionaryGetValue(ddesc, kDADiskDescriptionVolumePathKey);
    if (!volURL || CFGetTypeID(volURL) != CFURLGetTypeID())  goto finish;
    if (!CFURLGetFileSystemRepresentation(volURL, true /*resolve base*/,
	    (UInt8*)up->curMount, PATH_MAX))  	    goto finish;

    // Sec: get a non-spoofable handle to the current boot (trust moves)
    if (-1 == (up->curbootfd = open(up->curMount, O_RDONLY, 0)))   goto finish;
    if (fstat(up->caches->cachefd, &secsb))  goto finish;    // rootvol extant?

    // we only support 128 MB Apple_Boot partitions
    if (fstatfs(up->curbootfd, &bsfs))	goto finish;
    if (bsfs.f_blocks * bsfs.f_bsize < (128 * 1<<20)) {
	kextd_error_log("Apple_Boot helper < 128 MB; skipping");
	goto finish;
    }

    rval = 0;

finish:
    if (ddesc)	    CFRelease(ddesc);
    if (dis && dis != (void*)kCFNull) // for spurious CFRunLoopRun() return
	CFRelease(dis);

    if (rval != 0 && up->curBoot) {
	unmountBoot(up);	// unmount anything we managed to mount
    }
    if (rval) {
	if (rval != ELAST + 1)
	    kextd_error_log("couldn't mount helper: error %#X (DA err# %#.2x)",
			    rval,rval & ~(err_local|err_local_diskarbitration));
	else
	    kextd_error_log("couldn't mount helper partition");
    }

#endif
    return rval;
}

/*******************************************************************************
* unmountBoot 
* works like mountBoot, but for unmount
*******************************************************************************/
static int unmountBoot(struct updatingVol *up)
{
    int rval = 0;
#if !TARGET_OS_EMBEDDED
    DADissenterRef dis = (void*)kCFNull;
    
    if (g_verbose_level > 2) kextd_log("Unmounting helper partition %s", up->bsdname);

    // bail if nothing to actually unmount (still free up curBoot below)
    if (!up->curBoot)  	    	goto finish;
    if (!up->curMount[0])   	goto finish;

    if (up->curbootfd != -1)	close(up->curbootfd);

    // _daDone populates 'dis'[senter]
    rval = ELAST + 1;
    DADiskUnmount(up->curBoot, kDADiskMountOptionDefault, _daDone, &dis);
    if (dis == (void*)kCFNull)	    // in case _daDone already called
	CFRunLoopRun();

    // if that didn't work, try harder
    if (dis) {
	CFRelease(dis);
	dis = (void*)kCFNull;
	kextd_log("trouble unmounting boot partition; forcing...");
	DADiskUnmount(up->curBoot, kDADiskUnmountOptionForce, _daDone, &dis);
	if (dis == (void*)kCFNull)
	    CFRunLoopRun();
	if (dis)  goto finish;
    }

    rval = 0;

finish:
    up->curMount[0] = '\0';	// to keep tidy
    if (up->curBoot) {
	CFRelease(up->curBoot);
	up->curBoot = NULL;
    }
    if (dis && dis != (void*)kCFNull)
	CFRelease(dis);

#endif
    return rval;
}

/*******************************************************************************
* ucopyRPS unlinks old/copies new RPS content w/o activating
* RPS files are considered important -- non-zero file sizes only!
* XX could validate the kernel with Mach-o header
*******************************************************************************/
// if we were good, I'd be able to share "statRPS" with the efiboot sources
typedef int EFI_STATUS;
typedef struct stat EFI_FILE_HANDLE;
typedef char UINT16;
typedef Boolean BOOLEAN;
// typedef ...
/* 
:'a,'bs/EFI_ERROR//
:'a,'bs/L"/"/
:'a,'bs/%a/%s/
#define printf kextd_error_log
#define SPrint snprintf
#define EFI_NOT_FOUND ENOENT
#define BOOT_STRING_LEN PATH_MAX
*/
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
    Boolean haveR, haveP, haveS;
    char *prevp, *curp, *nextp;

    haveR = haveP = haveS = false;
    prevp = curp = nextp = NULL;

    // set up full paths with intervening slash
    pathcpy(rpath, up->curMount);
    pathcat(rpath, "/");
    pathcpy(ppath, rpath);
    pathcpy(spath, rpath);

    pathcat(rpath, kBootDirR);
    pathcat(ppath, kBootDirP);
    pathcat(spath, kBootDirS);

    status = stat(rpath, &r);	// easier to let this fail
    haveR = (status == 0);
    status = stat(ppath, &p);
    haveP = (status == 0);
    status = stat(spath, &s);
    haveS = (status == 0);

    if (haveR && haveP && haveS) {    // NComb(3,3) = 1
        kextd_log("WARNING: all of R,P,S exist: picking 'R'\n");
	curp = rpath;	nextp = ppath;	prevp = spath;
    }   else if (haveR && haveP) {          // NComb(3,2) = 3
        // p wins
	curp = ppath;	nextp = spath;	prevp = rpath;
    } else if (haveR && haveS) {
        // r wins
	curp = rpath;	nextp = ppath;	prevp = spath;
    } else if (haveP && haveS) {
        // s wins
	curp = spath; 	nextp = rpath;	prevp = ppath;
    } else if (haveR) {                     // NComb(3,1) = 3
        // r wins by default
	curp = rpath;	nextp = ppath;	prevp = spath;
    } else if (haveP) {
        // p wins by default
	curp = ppath;	nextp = spath;	prevp = rpath;
    } else if (haveS) {
        // s wins by default
	curp = spath; 	nextp = rpath;	prevp = ppath;
    } else {                                          // NComb(3,0) = 0
	// we'll start with rock
	curp = rpath;	nextp = ppath;	prevp = spath;
    }

    if (strlcpy(prev, prevp, PATH_MAX) >= PATH_MAX)	goto finish;
    if (strlcpy(current, curp, PATH_MAX) >= PATH_MAX)	goto finish;
    if (strlcpy(next, nextp, PATH_MAX) >= PATH_MAX)	goto finish;

    rval = 0;

finish:
    if (rval)
	kextd_error_log("FindRPSDir(): %s", strerror(errno));
    return rval;
}

// UUID helper for ucopyRPS
static int insertUUID(struct updatingVol *up, char *srcpath, char *dstpath)
{
    int rval = ELAST + 1;
    int fd = -1;
    struct stat sb;
    void *buf;
    CFDataRef data = NULL;
    CFMutableDictionaryRef pldict = NULL;
    CFStringRef str = NULL;

    mode_t dirmode;
    char dstparent[PATH_MAX];
    CFIndex len;

    // suck in plist
    if (-1 == (fd = sopen(up->caches->cachefd, srcpath, O_RDONLY, 0)))
	goto finish;
    if (fstat(fd, &sb))	    			    	goto finish;
    if (!(buf = malloc(sb.st_size)))		    	goto finish;
    if (read(fd, buf, sb.st_size) != sb.st_size)    	goto finish;
    if (!(data = CFDataCreate(nil, buf, sb.st_size))) 	goto finish;
    // make mutable dictionary
    pldict = (CFMutableDictionaryRef)CFPropertyListCreateFromXMLData(nil, data,
	    kCFPropertyListMutableContainers, NULL /* errstring */);
    if (!pldict || CFGetTypeID(pldict)!=CFDictionaryGetTypeID()) {
	// maybe the plist is empty
	pldict = CFDictionaryCreateMutable(nil, 0 /* could be 1 */, 
	    &kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
	if (!pldict)	goto finish;
    }

    // make a CFStr out of the UUID
    str = CFStringCreateWithFileSystemRepresentation(nil, up->caches->uuid_str);
    str = CFStringCreateWithCString(nil, up->caches->uuid_str, kCFStringEncodingASCII);
    if (!str)	goto finish;
    CFDictionarySetValue(pldict, CFSTR(kRootUUIDKey), str);


    // and write dictionary back

    (void)sunlink(up->curbootfd, dstpath);

    // figure out directory mode
    dirmode = ((sb.st_mode&~S_IFMT) | S_IWUSR | S_IXUSR /* u+wx */);
    if (dirmode & S_IRGRP)      dirmode |= S_IXGRP;     // add conditional o+x
    if (dirmode & S_IROTH)      dirmode |= S_IXOTH;

    // and recursively create the parent directory       
    if (strlcpy(dstparent, dirname(dstpath), PATH_MAX) >= PATH_MAX) goto finish;
    if ((sdeepmkdir(up->curbootfd, dstparent, dirmode)))            goto finish;

    close(fd);
    if (-1 == (fd=sopen(up->curbootfd, dstpath, O_WRONLY|O_CREAT, sb.st_mode)))
	goto finish;
    CFRelease(data);
    if (!(data = CFPropertyListCreateXMLData(nil, pldict)))	goto finish;
    len = CFDataGetLength(data);
    if (write(fd, CFDataGetBytePtr(data), len) != len)		goto finish;

    rval = 0;

finish:
    if (str)	    CFRelease(str);
    if (data)	    CFRelease(data);
    if (pldict)     CFRelease(pldict);
    if (fd != -1)   close(fd);

    return rval;
}

// we can bail on any error because only a whole RPS dir makes sense
static int ucopyRPS(struct updatingVol *up)
{
    int rval = ELAST+1;
    char discard[PATH_MAX];
    struct stat sb;
    int i;
    char srcpath[PATH_MAX], dstpath[PATH_MAX];
    char * plistNamePtr;
    
    if (g_verbose_level > 2) kextd_log("Beginning copy and atomic activation");

    // we're going to copy into the currently-inactive directory
    if (FindRPSDir(up, up->curRPS, discard, discard))  goto finish;

    // erase if present (we expect to have removed it)
    if (stat(up->curRPS, &sb) == 0) {
	if (sdeepunlink(up->curbootfd, up->curRPS)) {
	    kextd_error_log("%s: %s", up->curRPS, strerror(errno));
	    goto finish;
	}
    }

    // create the directory
    if (smkdir(up->curbootfd, up->curRPS, kRPSDirMask)) {
	kextd_error_log("%s: %s", up->curRPS, strerror(errno));
	goto finish;
    }

    // and loop
    for (i = 0; i < up->caches->nrps; i++) {
	pathcpy(srcpath, up->caches->root);
	pathcat(srcpath, up->caches->rpspaths[i].rpath);
	pathcpy(dstpath, up->curRPS);
	pathcat(dstpath, up->caches->rpspaths[i].rpath);

	// is it Boot.plist?
	if (&up->caches->rpspaths[i] == up->caches->bootconfig) {
	    
	    // PR-5115900 - name com.apple.boot.plist on APM since Tiger
	    //	bless scribbles on com.apple.Boot.plist

	    // This assert ensures BOOTPLIST_NAME and BOOTPLIST_APM_NAME are the same
	    // length (defined at top of file).  If they differ, the code will not compile.
	    COMPILE_TIME_ASSERT(sizeof(BOOTPLIST_NAME) == sizeof(BOOTPLIST_APM_NAME));

	    // If we are on an APM partition, substitute the new plist name
	    if (!up->isGPT) {
		plistNamePtr = strstr(dstpath, BOOTPLIST_NAME);
		if (plistNamePtr) {
		    strncpy(plistNamePtr, BOOTPLIST_APM_NAME, strlen(BOOTPLIST_NAME));
		}
	    }

	    if (insertUUID(up, srcpath, dstpath)) {
		kextd_error_log("error populating config file %s", dstpath);
		continue;
	    }
	} else {
	    /* we might want a zero-length (cookie) file in the Apple_Boot
	    // XX Leopard(?) other checks like is your Mach-O complete?
	    if (stat(srcpath, &sb) == 0 && sb.st_size == 0) {
		kextd_error_log("zero-size RPS file %s?", srcpath);
		goto finish;
	    } */
	    // scopyfile creates any intermediate directories
	    if (scopyfile(up->caches->cachefd,srcpath,up->curbootfd,dstpath)) {
		kextd_error_log("error copying %s", srcpath);
		goto finish;
	    }
	}
    }

    rval = 0;

finish:
    return rval;
}

/*******************************************************************************
* ucopyMisc writes misc files (customizing labels ;?) to .new (inactive) names
* [redundant label copy would be easy to avoid]
*******************************************************************************/
static int ucopyMisc(struct updatingVol *up)
{
    int rval = -1;
    int i, nprocessed = 0;
    char srcpath[PATH_MAX], dstpath[PATH_MAX];
    struct stat sb;

    if (g_verbose_level > 2) kextd_log("Copying new non-booter files");
    
    for (i = 0; i < up->caches->nmisc; i++) {
	pathcpy(srcpath, up->caches->root);
	pathcat(srcpath, up->caches->miscpaths[i].rpath);
	pathcpy(dstpath, up->curMount);
	pathcat(dstpath, up->caches->miscpaths[i].rpath);
	pathcat(dstpath, ".new");

	if (stat(srcpath, &sb) == 0) { 
	    if (scopyfile(up->caches->cachefd,srcpath,up->curbootfd,dstpath)) {
		kextd_error_log("error copying %s to %s", srcpath, dstpath);
	    }
	    continue;
	}

	nprocessed++;
    }

    rval = (nprocessed != i);

finish:
    return rval;
}

/*******************************************************************************
* moveLabels moves the labels aside in case they're needed again
* activateMisc will move these back
* no label -> hint of indeterminate state (label key in plist/other file?)
* Leopard: put/switch in some sort of "(updating!)" label (see BL[ess] routines)
*******************************************************************************/
static int moveLabels(struct updatingVol *up)
{
    int rval = 0;
    char path[PATH_MAX];
    struct stat sb;
    int fd = -1;
    
    if (g_verbose_level > 2) kextd_log("Moving aside old labels");

    pathcpy(path, up->curMount);
    pathcat(path, up->caches->label->rpath);
    if (0 == (stat(path, &sb))) {
	char newpath[PATH_MAX];
	unsigned char tbxichrp[32] = {'\0', };

	// rename
	pathcpy(newpath, path);	
	pathcat(newpath, NEWEXT);
	rval = srename(up->curbootfd, path, newpath);
	if (rval)	goto finish;

	// remove magic type/creator
	if (-1 == (fd=sopen(up->curbootfd, newpath, O_RDWR, 0)))  goto finish;
	if(fsetxattr(fd,XATTR_FINDERINFO_NAME,&tbxichrp,sizeof(tbxichrp),0,0))
	    goto finish;
    } 

    up->changestate = noLabels;

finish:
    if (fd != -1)   close(fd);

    return rval;
}

/*******************************************************************************
* nukeLabels gets rid of the label and .contentDetails files
* since activateMisc can create a new label, we just nuke
* no label -> hint of indeterminate state (label key in plist/other file?)
* Leopard: put/switch in some sort of "(updating!)" label (see BL[ess] routines)
*******************************************************************************/
static int nukeLabels(struct updatingVol *up)
{
    int rval = 0;
    char labelp[PATH_MAX];
    struct stat sb;
    
    if (g_verbose_level > 2) kextd_log("Destroying old labels");

    pathcpy(labelp, up->curMount);
    pathcat(labelp, up->caches->label->rpath);
    if (0 == (stat(labelp, &sb))) {
	rval |= sunlink(up->curbootfd, labelp);
    } 

    // now for the content details (if any)
    pathcat(labelp, CONTENTEXT);	// append extension

    if (0 == (stat(labelp, &sb))) {
	rval |= sunlink(up->curbootfd, labelp);
    }

    up->changestate = noLabels;

finish:
    return rval;
}

/*******************************************************************************
* ucopyBooters unlink/copies down booters but doesn't bless them
*******************************************************************************/
static int ucopyBooters(struct updatingVol *up)
{
    int rval = ELAST + 1;
    char srcpath[PATH_MAX], oldpath[PATH_MAX];

    if (g_verbose_level > 2) kextd_log("Copying new booters");
    
    // copy BootX, boot.efi
    up->changestate = copyingOFBooter;
    if (up->caches->ofbooter.rpath[0]) {
	pathcpy(srcpath, up->caches->root);
	pathcat(srcpath, up->caches->ofbooter.rpath);   // <root>/S/L/CS/BootX
	pathcpy(up->ofdst, up->curMount);
	pathcat(up->ofdst, up->caches->ofbooter.rpath); // <boot>/S/L/CS/BootX
	pathcpy(oldpath, up->ofdst);
	pathcat(oldpath, OLDEXT);	    	   // <boot>/S/L/CS/BootX.old

	(void)sunlink(up->curbootfd, oldpath);
	if (srename(up->curbootfd, up->ofdst, oldpath) && errno !=ENOENT)
	    goto finish;
	if (scopyfile(up->caches->cachefd, srcpath, up->curbootfd, up->ofdst)) {
	    kextd_error_log("%s: %s", srcpath, strerror(errno));
	    goto finish;
	}
    }

    up->changestate = copyingEFIBooter;
    if (up->caches->efibooter.rpath[0]) {
	pathcpy(srcpath, up->caches->root);
	pathcat(srcpath, up->caches->efibooter.rpath);   // ... boot.efi
	pathcpy(up->efidst, up->curMount);
	pathcat(up->efidst, up->caches->efibooter.rpath);
	pathcpy(oldpath, up->efidst);
	pathcat(oldpath, OLDEXT);

	(void)sunlink(up->curbootfd, oldpath);
	if (srename(up->curbootfd, up->efidst, oldpath) && errno != ENOENT)
	    goto finish;
	if (scopyfile(up->caches->cachefd, srcpath, up->curbootfd, up->efidst)){
	    kextd_error_log("failure copying booter %s", srcpath);
	    goto finish;
	}
    }

    up->changestate = copiedBooters;
    rval = 0;

finish:
    return rval;
}


// booters have worst critical:fragile ratio (basically point of no return)
/*******************************************************************************
* bless recently-copied booters
* operatens entirely on up->??dst which allows revertState to use it ..?
*******************************************************************************/
#define CLOSE(fd) do { (void)close(fd); fd = -1; } while(0)
enum blessIndices {
    kSystemFolderIdx = 0,
    kEFIBooterIdx = 1
    // Apple_Boot doesn't use 2-7
};
static int activateBooters(struct updatingVol *up)
{
    int rval = ELAST + 1;
    int fd = -1;
    uint32_t vinfo[8] = { 0, };
    struct stat sb;
    char parent[PATH_MAX];

    if (g_verbose_level > 2) kextd_log("Activating new booters");
    
    // activate BootX, boot.efi
    up->changestate = activatingOFBooter;
    if (up->caches->ofbooter.rpath[0]) {
	unsigned char tbxichrp[32] = {'t','b','x','i','c','h','r','p','\0',};

	// flush booter bytes to disk (really)
	if (-1 == (fd=sopen(up->curbootfd, up->ofdst, O_RDWR, 0)))  goto finish;
	if (fcntl(fd, F_FULLFSYNC))			    	    goto finish;

	// apply type/creator (assuming same folder as previous, now active)
	if(fsetxattr(fd,XATTR_FINDERINFO_NAME,&tbxichrp,sizeof(tbxichrp),0,0))
	    goto finish;
	CLOSE(fd);

	// get fileID of booter's enclosing folder 
	pathcpy(parent, dirname(up->ofdst));
	if (-1 == (fd=sopen(up->curbootfd, parent, O_RDONLY, 0)))  goto finish;
	if (fstat(fd, &sb))				    goto finish;
	CLOSE(fd);
	vinfo[kSystemFolderIdx] = sb.st_ino;
    }

    up->changestate = activatingEFIBooter;
    if (up->caches->efibooter.rpath[0]) {
	// sync to disk
	if (-1==(fd=sopen(up->curbootfd, up->efidst, O_RDONLY, 0))) goto finish;
	if (fcntl(fd, F_FULLFSYNC))				    goto finish;

	// get file ID
	if (fstat(fd, &sb))	goto finish;
	CLOSE(fd);
	vinfo[kEFIBooterIdx] = sb.st_ino;

	// since Inca has only one booter, but we want a blessed folder
	if (!vinfo[0]) {
	    // get fileID of booter's enclosing folder 
	    pathcpy(parent, dirname(up->efidst));
	    if (-1 == (fd=sopen(up->curbootfd, parent, O_RDONLY, 0)))
		goto finish;
	    if (fstat(fd, &sb))				    goto finish;
	    CLOSE(fd);
	    vinfo[kSystemFolderIdx] = sb.st_ino;
	}
    }

    // blessing efiboot/sysfolder happens by updating the root of the volume
    if (schdir(up->curbootfd, up->curMount, &fd))	    goto finish;
    if ((rval = BLSetVolumeFinderInfo(NULL, ".", vinfo)))   goto finish;
    (void)restoredir(fd);	    // tidy up (closes fd)
    fd = -1;

    up->changestate = activatedBooters;

finish:
    if (fd != -1)   close(fd);
    return rval;
}

/*******************************************************************************
* leap-frog w/rename()
*******************************************************************************/
static int activateRPS(struct updatingVol *up)
{
    int rval = ELAST + 1;
    char prevRPS[PATH_MAX], curRPS[PATH_MAX], nextRPS[PATH_MAX];
    
    if (g_verbose_level > 2) kextd_log("Completing copy and atomic activation");

    if (FindRPSDir(up, prevRPS, curRPS, nextRPS))   goto finish;

    // if current != the one we just populated
    if (strncmp(curRPS, up->curRPS, PATH_MAX) != 0) {
	// rename prev -> next ... done!?
	if (srename(up->curbootfd, prevRPS, nextRPS))   goto finish;
    }

    // thwunk everything to disk (now that essential boot files are in place)
    if (fcntl(up->curbootfd, F_FULLFSYNC))	    	goto finish;

    rval = 0;

finish:
    return rval;
}


/*******************************************************************************
* activateMisc renames .new files to final names and relabels the volumes
* active labels indicate an updated system
* - construct new labels with trailing numbers
* - use BLGenerateOFLabel() and overwrite any copied-down label
* X need to be consistent throughout regarding missing misc files (esp. label?)
*******************************************************************************/
#ifndef OPENSOURCE	// BLGenerateOFLabel uses CG
static int writeLabels(struct updatingVol *up, char *labelp)
{
    int rval = ELAST + 1;
    CFDataRef lData = NULL;
    CFIndex len;
    int fd = -1;
    char bootname[NAME_MAX];
    char contentPath[PATH_MAX];
    char *fmt = (CFArrayGetCount(up->boots) == 1) ? "%s" : "%s %d";
    
    if (g_verbose_level > 2) kextd_log("Writing new labels");

    if (NAME_MAX <= snprintf(bootname, NAME_MAX, fmt,
    			    up->caches->volname, up->bootIdx + 1))
	goto finish;
    if (BLGenerateOFLabel(NULL, bootname, &lData))	goto finish;

    // write the data
    if (-1 == (fd = sopen(up->curbootfd, labelp, O_CREAT|O_WRONLY, 0644)))
	goto finish;
    len = CFDataGetLength(lData);
    if (write(fd, CFDataGetBytePtr(lData), len) != len)	goto finish;

    // and write the content detail
    pathcpy(contentPath, labelp);
    pathcat(contentPath, CONTENTEXT);
    close(fd);
    if (-1 == (fd = sopen(up->curbootfd, contentPath, O_CREAT|O_WRONLY, 0644)))
	goto finish;
    len = strlen(bootname);
    if (write(fd, bootname, len) != len)	goto finish;

    rval = 0;

finish:
    if (fd != -1)   close(fd);
    if (lData)	    CFRelease(lData);

    return rval;
}
#endif	// OPENSOURCE

static int activateMisc(struct updatingVol *up)     // rename the .new
{
    int rval = ELAST + 1;
    char path[PATH_MAX], opath[PATH_MAX];
    int i = 0, nprocessed = 0;
    int fd = -1;
    struct stat sb;
    unsigned char tbxjchrp[32] = { 't','b','x','j','c','h','r','p','\0', };
    
    if (up->doMisc) {
        
        if (g_verbose_level > 2) kextd_log("Activating non-booter files");
        
	// do them all
	for (i = 0; i < up->caches->nmisc; i++) {
	    if (strlcpy(path, up->curMount, PATH_MAX) >= PATH_MAX)   continue;
	    if (strlcat(path, up->caches->miscpaths[i].rpath, PATH_MAX) 
			> PATH_MAX)   continue;
	    if (strlcpy(opath, path, PATH_MAX) >= PATH_MAX)	continue;
	    if (strlcat(opath, NEWEXT, PATH_MAX) >= PATH_MAX)	continue;

	    if (stat(opath, &sb) == 0) {
		if (srename(up->curbootfd, opath, path))	continue;
	    }

	    nprocessed++;
	}

    }

    pathcpy(path, up->curMount);
    pathcat(path, up->caches->label->rpath);
#ifndef OPENSOURCE
    if (up->expectUpToDate) {
#endif
        // move label back
        char newpath[PATH_MAX];

        pathcpy(newpath, path);     // just rename
        pathcat(newpath, NEWEXT);
        (void)srename(up->curbootfd, newpath, path);
#ifndef OPENSOURCE
    } else {
        // write labels
        (void)sunlink(up->curbootfd, path);
        if (writeLabels(up, path))          goto finish;
#endif
    }

    // assign type/creator to the label (non-OPENSOURCE might have copied)
    if (0 == (stat(path, &sb))) {
        if (-1 == (fd = sopen(up->curbootfd, path, O_RDWR, 0)))   goto finish;

        if (fsetxattr(fd,XATTR_FINDERINFO_NAME,&tbxjchrp,sizeof(tbxjchrp),0,0))
            goto finish;
        close(fd); fd = -1;
    }

    rval = (i != nprocessed);

finish:
    if (fd != -1)   close(fd);
    return rval;
}

/*******************************************************************************
* get rid of everything "extra"
*******************************************************************************/
static int nukeFallbacks(struct updatingVol *up)
{
    int rval = 0;		// OR-ative return value
    int bsderr;
    char delpath[PATH_MAX];
    struct bootCaches *caches = up->caches;

    // using pathcpy b/c if that's failing, it's worth bailing
    // XX should probably only try to unlink if present

    // maybe mount failed (in which there aren't any fallbacks
    if (!up->curBoot)	goto finish;

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

    // if needed, deepunlink prevRPS
    // which, conveniently, will be right regardless of whether we succeeded
    if (up->doRPS) {
	char toss[PATH_MAX];

	if (0 == FindRPSDir(up, delpath, toss, toss)) {
	    if ((bsderr=sdeepunlink(up->curbootfd,delpath)) && bsderr!=ENOENT) {
		rval |= bsderr;
	    }
	}
    }

finish:
    return rval;
}
