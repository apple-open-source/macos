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
 * FILE: watchvol.c
 * AUTH: Soren Spies (sspies)
 * DATE: 5 March 2006
 * DESC: watch volumes as they come, go, or are changed, fire rebuilds
 *       NOTE: everything in this file should happen on the main thread
 *
 * Here's roughly how it all works.
 * 1. sign up for diskarb notifications
 * 2. generate a data structure for each incoming comprehensible OS volume
 * 2a. set up notifications for all relevant paths on said volume
 *     [notifications <-> structures]
 * (2) uses bootcaches.plist to describe what caches a system needs.
 *     All top-level keys are assumed required (which means the mkext could
 *     get fancier in the future if an old-fashioned mkext was still okay).
 *     If keys exist that can't be understood or don't parse correctly, 
 *     we bail on watching that volume.
 *
 * 3. intelligently respond to notifications
 * 3a. set up a timer to fire so the system has time to settle
 * 3b. upon lazy firing, rebuild caches OR copy files to Apple_Boot
 * 3c. if someone tries to unmount a volume, cancel any timer and check
 * 3d. if a locker unlocks happily, cancel any timer and check  (TODO)
 * 3e. if a locker unlocks unhappily, need to force a check of non-caches? (???)
 * 3f. we don't care if the volume is locked; additional kextcaches wait
 * (3d) has the effect that the first kextcache effectively triggers the
 *      second one which copies caches down.  It also allows us ... to be
 *      smart about things like forcing reboots if we booted from staleness.
 *
 * 4. arbitrate kextcache locks
 * 4a. keep a Mach send right to a receive right in the locker
 * 4b. detect crashes via CFMachPortInvalidaton callback
 * 4c. take success information on unlock
 * 4d. if a lock was lost, force a rebuild (XX)?
 * TODO (still as part of 4252674):
 * (4) means that kextcache rebuilds the mkext (-> scheduling a timer)
 * (4c) means that we can schedule the Apple_Boot check on success (unschedules)
 *
 * 5. keep structures up to date
 * 5a. clean up when a volume goes away
 * 5b. disappear/appear whenever there's a change
 *
 * 6. reboot stuff: take a big lock; free it only if locker dies
 *
 * given that we read bootcaches.plist, we don't trust anything in it
 * ... but we push the checking off to kextcache, which ensures
 * (via dev_t/safecalls) that it is only operating on a single volume and 
 * not being redirected to other volumes, etc.  We have had Security review.
 *
 * XX we need to figure out what to do about ignored owners and UID 99.
 * We don't want to go writing an mkext with uid 99 ... but we do want
 * this scheme to work ... could we temporarily enable owners??
 * The current plan (4554031) is to notice when VQ_UPDATE's occur ...
 * enabling owners ourselves could lead to some very mysterious behavior.
 *  
 * $removing checkin comments expander in the header$
 */

// system includes
#include <notify.h> // yay notify_monitor_file (well, maybe someday ;)
#include <string.h> // strerror()
#include <sysexits.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>  // waitpid() (fork/daemon/waitpid)
#include <sys/wait.h>   // " "
#include <stdlib.h> // daemon(3)
#include <signal.h>
#include <unistd.h> // e.g. execv

#include <bless.h>
#include <CoreFoundation/CoreFoundation.h>
#include <TargetConditionals.h>
#if !TARGET_OS_EMBEDDED
#include <DiskArbitration/DiskArbitration.h>
#include <DiskArbitration/DiskArbitrationPrivate.h>
#endif
#include <IOKit/kext/kextmanager_types.h>

// notifyd SPI
extern uint32_t notify_monitor_file(int token, const char *name, int flags);
extern uint32_t notify_get_event(int token, int *ev, char *buf, int *len);

// project includes
#include "watchvol.h"           // kextd_watch_volumes
#include "bootcaches.h"         // struct bootCaches
#include "globals.h"            // gClientUID, kextd_raise_notification
#include "kextmanager_async.h"  // lock_*_reply()
#include "logging.h"            // kextd_ logging
#include "utility.h"            // fork_program()


// constants
#define kWatchKeyBase       "com.apple.system.kextd.fswatch"
#define kWatchSettleTime    5
#define kFailureLimit       5   // consecutive failures


// the type: struct watchedVol's (struct bootCaches in bootroot.h)
// created/destroyed with volumes coming/going; stored in sFsysWatchDict
// use notify_set_state on our notifications to point to these objects
struct watchedVol {
    // CFUUIDRef volUUID;       // DA id (is the key in sFsysWatchDict)
    CFRunLoopTimerRef delayer;  // non-NULL if something is scheduled
    CFMachPortRef lock;         // send right to locker's port
    CFMutableArrayRef waiters;  // reply ports awaiting this volume
    int errcount;               // did most recent locker report an error? (???)
    Boolean disableOwners;      // did we enable owners on lock?
    Boolean isBootRoot;         // should we try to update helpers?

    CFMutableArrayRef tokens;   // notify(3) tokens
    struct bootCaches *caches;  // parsed version of bootcaches.plist

    // XX we could track the current PID of any launched kextcache
};

// module-wide data
static DASessionRef sDASession = NULL;                  // learn about volumes
#if !TARGET_OS_EMBEDDED
static DAApprovalSessionRef sDAApproval = NULL;         // retain volumes
static CFMachPortRef sFsysChangedPort = NULL;           // let us know
static CFRunLoopSourceRef sFsysChangedSource = NULL;    // on the runloop
#endif
static CFMutableDictionaryRef sFsysWatchDict = NULL;    // disk ids -> wstruct*s
static CFMutableDictionaryRef sReplyPorts = NULL;       // cfports -> replyPorts
static CFMachPortRef sRebootLock = NULL;                // if locked for reboot
static CFMachPortRef sRebootWaiter = NULL;              // only need one


// function declarations (kextd_watch_volumes, _stop in watchvol.h)

// ctor/dtors
#if !TARGET_OS_EMBEDDED
static struct watchedVol* create_watchedVol(DADiskRef disk);
static void destroy_watchedVol(struct watchedVol *watched);
#endif
static CFMachPortRef createWatchedPort(mach_port_t mport, void *ctx);

// volume state
#if !TARGET_OS_EMBEDDED
static void vol_appeared(DADiskRef disk, void *ctx);
static void vol_changed(DADiskRef, CFArrayRef keys, void* ctx);
static void vol_disappeared(DADiskRef, void* ctx);
static DADissenterRef is_dadisk_busy(DADiskRef, void *ctx);
#endif
static Boolean check_vol_busy(struct watchedVol *watched);

// notification processing delay scheme
#if !TARGET_OS_EMBEDDED
static void fsys_changed(CFMachPortRef p, void *msg, CFIndex size, void *info);
static void check_now(CFRunLoopTimerRef timer, void *ctx);    // notify timer cb
#endif

// check and act
static Boolean check_rebuild(struct watchedVol*);   // true if launched

// CFMachPort invalidation callback
static void port_died(CFMachPortRef p, void *info);

// helpers for volume and reboot locking routines
static Boolean reconsiderVolumes(mountpoint_t busyVol);
static Boolean checkAllWatched(mountpoint_t busyVol);   // true => work to do
static void toggleOwners(mountpoint_t mount, Boolean enableOwners);

// additional "local" helpers are declared/defined just before use


// utility macros
#define CFRELEASE(x) if(x) { CFRelease(x); x = NULL; }


/******************************************************************************
 * kextd_watch_volumes sets everything up (on the current runloop)
 *****************************************************************************/
#if 0
// for testing
#define twrite(msg) write(STDERR_FILENO, msg, sizeof(msg))
static void debug_chld(int signum) __attribute__((unused))
{
    int olderrno = errno;
    int status;
    pid_t childpid;

    if (signum != SIGCHLD)
        twrite("debug_chld not registered for signal\n");
    else
    if ((childpid = waitpid(-1, &status, WNOHANG)) == -1)
        twrite("DEBUG: SIGCHLD received, but no children available?\n");
    else 
    if (!WIFEXITED(status))
        twrite("DEBUG: child quit on signal?\n");
    else
    if (WEXITSTATUS(status))
        twrite("DEBUG: child exited with unhappy status\n");
    else
        twrite("DEBUG: child exited with happy status\n");

    errno = olderrno;
}
#endif

int kextd_watch_volumes(int sourcePriority)
{
    int rval = ELAST + 1;
#if !TARGET_OS_EMBEDDED
    char *errmsg;
    CFRunLoopRef rl;

    errmsg = "kextd_watch_volumes() already initialized";
    if (sFsysWatchDict)     goto finish;

    // the callbacks will want to go digging in here, so set it up first
    errmsg = "couldn't create data structures";
    // sFsysWatchDict keeps track of watched volumes with UUIDs as keys
    sFsysWatchDict = CFDictionaryCreateMutable(nil, 0,
            &kCFTypeDictionaryKeyCallBacks, NULL);  // storing watchedVol*'s
    if (!sFsysWatchDict)    goto finish;
    // We keep two ports for a client; one to for death tracking and one  to
    // reply when the time comes.  sReplyPorts maps between the CF wrapper
    // for the death-tracking port and the mach_port_t replyPort.
    sReplyPorts = CFDictionaryCreateMutable(nil, 0,
            &kCFTypeDictionaryKeyCallBacks, NULL);  // storing mach_port_t's
    if (!sReplyPorts)       goto finish;

    errmsg = "error setting up ports and sources";
    rl = CFRunLoopGetCurrent();
    if (!rl)    goto finish;

    // change notifications will eventually come in through this port/source
    sFsysChangedPort = CFMachPortCreate(nil, fsys_changed, NULL, NULL);
    // we have to keep these objects so we can unschedule them later?
    if (!sFsysChangedPort)      goto finish;
    sFsysChangedSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault,
    sFsysChangedPort, sourcePriority);
    if (!sFsysChangedSource)    goto finish;
    CFRunLoopAddSource(rl, sFsysChangedSource, kCFRunLoopDefaultMode);

    // in general, being on the runloop means we could be called ...
    // and we are thus careful about our ordering.  In practice, however,
    // we're adding to the current runloop, which means nothing can happen
    // until this routine exits (we're on the one and only thread).

    /*
     * XX need to set up a better match dictionary
     * kDADiskDescriptionMediaWritableKey = true
     * kDADiskDescriptionVolumeNetworkKey != true
     */

    // make sure we have a chance to block unmounts
    errmsg = "couldn't set up diskarb sessions";
    sDAApproval = DAApprovalSessionCreate(nil);
    if (!sDAApproval)  goto finish;
    DARegisterDiskUnmountApprovalCallback(sDAApproval,
    kDADiskDescriptionMatchVolumeMountable, is_dadisk_busy, NULL);
    DAApprovalSessionScheduleWithRunLoop(sDAApproval, rl,kCFRunLoopDefaultMode);

    // set up the regular session
    sDASession = DASessionCreate(nil);
    if (!sDASession)  goto finish;
    DARegisterDiskAppearedCallback(sDASession,
    kDADiskDescriptionMatchVolumeMountable, vol_appeared, NULL);
    DARegisterDiskDescriptionChangedCallback(sDASession,
    kDADiskDescriptionMatchVolumeMountable,
    kDADiskDescriptionWatchVolumePath, vol_changed, NULL);
    DARegisterDiskDisappearedCallback(sDASession,
    kDADiskDescriptionMatchVolumeMountable, vol_disappeared, NULL);

    // okay, we're ready to rumble!
    DASessionScheduleWithRunLoop(sDASession, rl, kCFRunLoopDefaultMode);

    // if (signal(SIGCHLD, SIG_IGN) == SIG_ERR)  goto finish;
    // errmsg = "couldn't set debug signal handler";
    // if (signal(SIGCHLD, debug_chld) == SIG_ERR)  goto finish;

    errmsg = NULL;
    rval = 0;

    // volume notifications should start coming in shortly

finish:
    if (rval) {
        kextd_error_log("kextd_watch_volumes: %s", errmsg);
        kextd_stop_volwatch();
    }

#endif
    return rval;
}
/******************************************************************************
 * kextd_giveup_volwatch (4692369) initializes sFsysWatchDict to empty
 * so that locking can occur.
 *****************************************************************************/
int kextd_giveup_volwatch()
{
    int rval = ENOMEM;
    sFsysWatchDict = CFDictionaryCreateMutable(nil, 0,
            &kCFTypeDictionaryKeyCallBacks, NULL);  // storing watchedVol*'s

    if (sFsysWatchDict) {
        rval = 0;
    } else {
        kextd_error_log("giveup_volwatch(): allocation failure");
    }

    return rval;
}


/******************************************************************************
 * kextd_stop_volwatch unregisters from everything and cleans up
 * - called from watch_volumes to handle partial cleanup
 *****************************************************************************/
// to help clear out sFsysWatch
#if !TARGET_OS_EMBEDDED
static void free_dict_item(const void* key, const void *val, void *c)
{
    destroy_watchedVol((struct watchedVol*)val);
}
#endif

// public entry point to this module
void kextd_stop_volwatch()
{
#if !TARGET_OS_EMBEDDED
    CFRunLoopRef rl;

    // runloop cleanup
    rl = CFRunLoopGetCurrent();
    if (rl && sDASession)   DASessionUnscheduleFromRunLoop(sDASession, rl,
            kCFRunLoopDefaultMode);
    if (rl && sDAApproval)  DAApprovalSessionUnscheduleFromRunLoop(sDAApproval,
            rl, kCFRunLoopDefaultMode);

    // use CFRELEASE to nullify cfrefs in case watch_volumes called again
    if (sDASession) {
        DAUnregisterCallback(sDASession, vol_disappeared, NULL);
        DAUnregisterCallback(sDASession, vol_changed, NULL);
        DAUnregisterCallback(sDASession, vol_appeared, NULL);
        CFRELEASE(sDASession);
    }

    if (sDAApproval) {
        DAUnregisterApprovalCallback(sDAApproval, is_dadisk_busy, NULL);
        CFRELEASE(sDAApproval);
    }

    if (rl && sFsysChangedSource)  CFRunLoopRemoveSource(rl, sFsysChangedSource,
            kCFRunLoopDefaultMode);
    CFRELEASE(sFsysChangedSource);
    CFRELEASE(sFsysChangedPort);

    if (sFsysWatchDict) {
        CFDictionaryApplyFunction(sFsysWatchDict, free_dict_item, NULL);
        CFRELEASE(sFsysWatchDict);
    }
#endif
}

#if !TARGET_OS_EMBEDDED
/******************************************************************************
* destroy_watchedVol unregisters any notification tokens and frees
* pieces created in create_watchedVol
******************************************************************************/
static void destroy_watchedVol(struct watchedVol *watched)
{
    CFIndex ntokens;
    int token;      
    int errnum;

    // assert that ->delayer, and ->lock have already been cleaned up
    if (watched->tokens) {
        ntokens = CFArrayGetCount(watched->tokens);
        while(ntokens--) { 
            token = (int)CFArrayGetValueAtIndex(watched->tokens,ntokens);
            // XX should take (hacky) steps to insure token is never zero?
            if (/* !token || */ (errnum = notify_cancel(token)))
                kextd_error_log("destroy_watchedVol: "
                        "error %d canceling notification", errnum);
        }
        CFRelease(watched->tokens);
    }    
    if (watched->caches)    destroyCaches(watched->caches);
    free(watched);
}
#endif

#if !TARGET_OS_EMBEDDED
/******************************************************************************
* create_watchedVol calls readCaches and creates watch-specific necessities
******************************************************************************/
static struct watchedVol* create_watchedVol(DADiskRef disk)
{
    struct watchedVol *watched, *rval = NULL;
    char *errmsg;
    Boolean isGPT;

    errmsg = "allocation error";
    watched = calloc(1, sizeof(*watched));
    if (!watched)   goto finish;

    errmsg = NULL;  // no bootcaches.plist, no problem

    watched->caches = readCaches(disk);     // readCaches logs errors
    if (!watched->caches)   goto finish;

    // 4616366: only watch BootRoot volumes (until 4243070)
    if (hasBoots(watched->caches->bsdname, NULL, &isGPT)) {
        Boolean APMBootRoot;    // Inca only supported GPT BootRoot

        // 5158091: don't update boots for Inca systems on APM
        APMBootRoot = watched->caches->ofbooter.rpath[0];   // look for BootX
        watched->isBootRoot = isGPT || APMBootRoot;
    }

    // There will be RPS paths, booters, "misc" paths, and the exts folder.
    // For now, we'll just set the array size to 0 and let it grow.
    errmsg = "allocation error";
    watched->tokens = CFArrayCreateMutable(nil, 0, NULL);
    if (!watched->tokens)   goto finish;

    errmsg = NULL;
    rval = watched;     // success!

finish:
    if (errmsg) {
        if (watched && watched->caches && watched->caches->root[0]) {
            kextd_error_log("%s: %s", watched->caches->root, errmsg);
        } else {
            kextd_error_log("create_watchedVol(): %s", errmsg);
        }
    }
    if (!rval && watched) {
        destroy_watchedVol(watched);
    }
    
    return rval;
}
#endif

// helper: caller must remove port from other structures (e.g. waiters queue)
static int cleanupPort(CFMachPortRef *port)
{
    mach_port_t lport;

    if (sReplyPorts)
        CFDictionaryRemoveValue(sReplyPorts, *port); // stop tracking replyPort
    CFMachPortSetInvalidationCallBack(*port, NULL);  // else port_died called
    lport = CFMachPortGetPort(*port);
    CFRelease(*port);
    *port = NULL;

    return mach_port_deallocate(mach_task_self(), lport);
}

// caller responsibile for setting up the lock and cleaning up the waiter
static int signalWaiter(CFMachPortRef waiter, int status)
{
    int rval = KERN_FAILURE;
    mach_port_t replyPort;

    // extract this client's reply port and reply
    replyPort = (mach_port_t)CFDictionaryGetValue(sReplyPorts, waiter);
    CFDictionaryRemoveValue(sReplyPorts, waiter);

    if (replyPort != MACH_PORT_NULL)
        if (waiter == sRebootWaiter) {
            mountpoint_t empty;
            rval = lock_reboot_reply(replyPort, KERN_SUCCESS, empty, status);
        } else {
            rval = lock_volume_reply(replyPort, KERN_SUCCESS, status);
        }

    if (rval)
        kextd_error_log("signalWaiter failed: %s", mach_error_string(rval));
    return rval;
} 

// sRebootWaiter must be set; sRebootLock is released if held
static void handleRebootHandoff()
{
    mountpoint_t busyVol;

    // make sure we're allowed to give out the reboot lock
    if (checkAllWatched(busyVol) == EBUSY) {
        kextd_log("%s is still busy, delaying reboot", busyVol);
        goto finish;
    }

    // signal the waiter 
    if (signalWaiter(sRebootWaiter, KERN_SUCCESS) == 0) {
        // on success, make the waiter the locker
        sRebootLock = sRebootWaiter;
    }
    sRebootWaiter = NULL;

finish:
    return;
}

// cleans up watched->lock, checks for waiters, assigns the lock, and signals
static void handleWatchedHandoff(struct watchedVol *watched)
{
    CFMachPortRef waiter = NULL;

    // release existing lock
    if (watched->lock)
        cleanupPort(&watched->lock);
 
    // see if we have any waiters
    if (watched->waiters && CFArrayGetCount(watched->waiters)) {
        waiter = (CFMachPortRef)CFArrayGetValueAtIndex(watched->waiters, 0);

        // move waiter into the pole position and remove from the array
        watched->lock = waiter;     // context already set to 'watched'
        CFArrayRemoveValueAtIndex(watched->waiters, 0);

        // signal the waiter, cleaning up on failure
        if (signalWaiter(waiter, KERN_SUCCESS)) {
            cleanupPort(&watched->lock);    // deallocates former waiter
        }
    }
}

/******************************************************************************
 * vol_appeared checks whether a volume is interesting
 * (note: the first time we see a volume, it's probably not mounted yet)
 * (we rely on vol_changed to call us when the mountpoint actually appears)
 * - signs up for notifications -> creates new entries in our structures
 * - initiates an initial volume check
 *****************************************************************************/
// set up notifications for a single path
#if !TARGET_OS_EMBEDDED
static int watch_path(char *path, mach_port_t port, struct watchedVol* watched)
{
    int rval = ELAST + 1;   // cheesy
    char key[PATH_MAX];
    int token = 0;
    int errnum;
    uint64_t state;

    // generate key, register for token, monitor, record pointer in token
    if (strlcpy(key, kWatchKeyBase, PATH_MAX) >= PATH_MAX)  goto finish;
    if (strlcat(key, path, PATH_MAX) >= PATH_MAX)  goto finish;
    if (notify_register_mach_port(key, &port, NOTIFY_REUSE, &token))
        goto finish;
    state = (intptr_t)watched;
    if (notify_set_state(token, state))  goto finish;
    if (notify_monitor_file(token, path, 1)) goto finish;

    CFArrayAppendValue(watched->tokens, (void*)token);

    rval = 0;

finish:
    if (rval && token != -1 && (errnum = notify_cancel(token)))
    kextd_error_log("watch_path: error %d canceling token", errnum);

    return rval;
}
#endif

#define makerootpath(caches, dst, path) do { \
        if (strlcpy(dst, caches->root, PATH_MAX) >= PATH_MAX)   goto finish; \
        if (strlcat(dst, path, PATH_MAX) >= PATH_MAX)           goto finish; \
    } while(0)
#if !TARGET_OS_EMBEDDED
static void vol_appeared(DADiskRef disk, void *launchCtx)
{
    int result = 0; // for now, ignore inability to get basic data (4528851)
    mach_port_t fsPort;
    CFDictionaryRef ddesc = NULL;
    CFBooleanRef traitVal;
    CFUUIDRef volUUID;
    struct watchedVol *watched = NULL;
    Boolean launched = false;

    struct bootCaches *caches;
    int i;
    char path[PATH_MAX];

    // get description so we can see if the disk is writable, etc
    ddesc = DADiskCopyDescription(disk);
    if (!ddesc)     goto finish;

    // volUUID is the key in the dictionary (might we already be watching?)
    volUUID = CFDictionaryGetValue(ddesc, kDADiskDescriptionVolumeUUIDKey);
    if (!volUUID || CFGetTypeID(volUUID) != CFUUIDGetTypeID())      goto finish;
    if ((watched = (void *)CFDictionaryGetValue(sFsysWatchDict, volUUID))) {
        kextd_log("WARNING: removing pre-existing '%s' from watch table",
                  watched->caches->root);
        // here's where we'd put required check-in from kextcache -U
        // (a stub entry impliying lock interest?)
        // (waiters removed from watched before vol_disappeared; re-added below)
        vol_disappeared(disk, NULL);
    }

    // check traits (need custom dict)
    if (!CFDictionaryGetValue(ddesc, kDADiskDescriptionVolumePathKey))
        goto finish;    // ignore unmounted volumes

    traitVal = CFDictionaryGetValue(ddesc, kDADiskDescriptionMediaWritableKey);
    if (!traitVal || CFGetTypeID(traitVal) != CFBooleanGetTypeID()) goto finish;
    if (CFEqual(traitVal, kCFBooleanFalse))     goto finish;

    traitVal = CFDictionaryGetValue(ddesc, kDADiskDescriptionVolumeNetworkKey);
    if (!traitVal || CFGetTypeID(traitVal) != CFBooleanGetTypeID()) goto finish;
    if (CFEqual(traitVal, kCFBooleanTrue))      goto finish;


    // does it have a usable bootcaches.plist? (if not, just ignored)
    if (!(watched = create_watchedVol(disk)))   goto finish;

    result = -1;    // anything after this is an error
    caches = watched->caches;
    // set up notifications on the change port
    fsPort = CFMachPortGetPort(sFsysChangedPort);
    if (fsPort == MACH_PORT_NULL)               goto finish;

    // for path in { exts, rpspaths[], booters, miscpaths[] }
    // rpspaths contains mkext, bootconfig; miscpaths the label file
    // cache paths are relative; need to make absolute
    makerootpath(caches, path, caches->exts);
    if (watch_path(path, fsPort, watched))      goto finish;
    for (i = 0; i < caches->nrps; i++) {
        makerootpath(caches, path, caches->rpspaths[i].rpath);
        if (watch_path(path, fsPort, watched))      goto finish;
    }
    if (caches->efibooter.rpath[0]) {
        makerootpath(caches, path, caches->efibooter.rpath);
        if (watch_path(path, fsPort, watched))      goto finish;
    }
    if (caches->ofbooter.rpath[0]) {
        makerootpath(caches, path, caches->ofbooter.rpath);
        if (watch_path(path, fsPort, watched))      goto finish;
    }
    for (i = 0; i < caches->nmisc; i++) {
        makerootpath(caches, path, caches->miscpaths[i].rpath);
        if (watch_path(path, fsPort, watched))      goto finish;
    }

    // we handled any pre-existing entry for volUUID above
    CFDictionarySetValue(sFsysWatchDict, volUUID, watched);

    // make sure the new volume gets updated if needed
    launched = check_rebuild(watched);

    // reconsiderVolume() gets return value through launchCtx
    if (launchCtx) {
        Boolean *didLaunch = launchCtx;
        *didLaunch = launched;
    }

    result = 0;           // we made it

finish:
    if (ddesc)   CFRelease(ddesc);

    if (result) {
        if (watched) {
            kextd_error_log("error setting up notifications on '%s'",
                            watched->caches->root);
            destroy_watchedVol(watched);
        }
    }
}
#endif

/******************************************************************************
 * vol_changed updates our structures if the mountpoint changed
 * - includes the initial mount after a device appears 
 * - thus we only call appeared and disappeared as appropriate
 *   _appeared and _disappeared are smart enough, but debugging is a pain
 *   when vol_disappeared gets called on a volume mount!
 *****************************************************************************/
#if !TARGET_OS_EMBEDDED
static void vol_changed(DADiskRef disk, CFArrayRef keys, void* ctx)
{
    CFIndex i = CFArrayGetCount(keys);
    CFTypeRef key;
    CFDictionaryRef ddesc = DADiskCopyDescription(disk);
    CFUUIDRef volUUID;

    if (!ddesc)  goto finish;   // can't do much otherwise

    volUUID = CFDictionaryGetValue(ddesc, kDADiskDescriptionVolumeUUIDKey);
    if (!volUUID)  goto finish;

    while (i--)
    if ((key = CFArrayGetValueAtIndex(keys, i)) &&
        CFEqual(key, kDADiskDescriptionVolumePathKey)) {

        // XX need to use a custom match dictionary
        // diskarb sends lots of notifications about random stuff
        // thus: only need to call _disappeared if we're watching it
        if (CFDictionaryGetValue(sFsysWatchDict, volUUID))
            vol_disappeared(disk, ctx);
        // and: only need to call _appeared if there's a mountpoint
        if (CFDictionaryGetValue(ddesc, key))
            vol_appeared(disk, ctx);
    } else {
        kextd_log("vol_changed: ignoring update: no mountpoint change");
    }

finish:
    if (ddesc)  CFRelease(ddesc);
}
#endif

/******************************************************************************
 * vol_disappeared removes entries from the relevant structures
 * - handles forced removal by invalidating the lock
 *****************************************************************************/
#if !TARGET_OS_EMBEDDED
static void vol_disappeared(DADiskRef disk, void* ctx)
{
    // we used to report errors, but we got weird requests (4528851)
    CFDictionaryRef ddesc = NULL;
    CFUUIDRef volUUID;
    struct watchedVol *watched;

    ddesc = DADiskCopyDescription(disk);
    if (!ddesc)     goto finish;
    volUUID = CFDictionaryGetValue(ddesc, kDADiskDescriptionVolumeUUIDKey);
    if (!volUUID)   goto finish;

    watched = (void*)CFDictionaryGetValue(sFsysWatchDict, volUUID);
    if (!watched)   goto finish;

    // take it off the watch list
    CFDictionaryRemoveValue(sFsysWatchDict, volUUID);

    // and in case some action was in progress
    if (watched->delayer) {
        CFRunLoopTimerInvalidate(watched->delayer); // refcount->0
        watched->delayer = NULL;
    }
    // see if any lockers are waiting 
    // (off the list of watched vols so no new requests can come in)
    if (watched->waiters) {
        CFIndex i = CFArrayGetCount(watched->waiters);
        CFMachPortRef waiter;

        while(i--) {
            waiter = (CFMachPortRef)CFArrayGetValueAtIndex(watched->waiters,i);
            signalWaiter(waiter, ENOENT);
            cleanupPort(&waiter);
        }
        CFRelease(watched->waiters);    // should remove all elements
    }

    // no need to toggle owners since the volume is gone

    destroy_watchedVol(watched);    // cancels notifications

finish:
    if (ddesc)  CFRelease(ddesc);
}
#endif

/******************************************************************************
 * is_dadisk_busy lets diskarb know if we'd rather nothing changed
 * note: dissenter callback is called when root initiates an unmount,
 * but the result is ignored.
 *****************************************************************************/
#if !TARGET_OS_EMBEDDED
static DADissenterRef is_dadisk_busy(DADiskRef disk, void *ctx)
{
    int result = 0;     // ignore weird requests for now (4528851)
    DADissenterRef rval = NULL;
    CFDictionaryRef ddesc = NULL;
    CFUUIDRef volUUID;
    struct watchedVol *watched;

    ddesc = DADiskCopyDescription(disk);
    if (!ddesc)     goto finish;
    volUUID = CFDictionaryGetValue(ddesc, kDADiskDescriptionVolumeUUIDKey);
    if (!volUUID)   goto finish;

    result = -1;
    watched = (void*)CFDictionaryGetValue(sFsysWatchDict, volUUID);
    if (!watched) {
        // it might have become worth watching while we weren't :?
        vol_appeared(disk, NULL);
        watched = (void*)CFDictionaryGetValue(sFsysWatchDict, volUUID);
    }
    if (watched && check_vol_busy(watched)) {
        rval = DADissenterCreate(nil, kDAReturnBusy, CFSTR("kextmanager busy"));
        if (!rval)      goto finish;
    }
    
    result = 0;

finish:
    if (result) kextd_error_log("is_dadisk_busy had trouble answering diskarb");
    // else kextd_log("returning dissenter %p", rval);
    if (ddesc)  CFRelease(ddesc);

    return rval;    // caller releases dissenter if non-null
}
#endif

/******************************************************************************
 * check_vol_busy
 * - busy if locked
 * - check_rebuild to check once more (return code indicates if it did anything)
 *****************************************************************************/
static Boolean check_vol_busy(struct watchedVol *watched)
{
    Boolean rval = (watched->lock != NULL);

    if (!rval && watched->errcount < kFailureLimit) {
        rval = check_rebuild(watched);
    }

    return rval;
}


/******************************************************************************
 * fsys_changed gets the mach messages from notifyd
 * - schedule a timer (urgency detected elsewhere calls direct, canceling timer)
 *****************************************************************************/
#if !TARGET_OS_EMBEDDED
static void fsys_changed(CFMachPortRef p, void *m, CFIndex size, void *info)
{
    int result = -1;
    uint64_t nstate;
    struct watchedVol *watched;
    int token;
    mach_msg_empty_rcv_t *msg = (mach_msg_empty_rcv_t*)m;

    // msg_id==token -> notify_get_state() -> watchedVol*
    // XX if (token == 0, perhaps a force-rebuild message?)
    token = msg->header.msgh_id;
    if (notify_get_state(token, &nstate))   goto finish;
    // XX should call notify_get_event() here to consume events?
    // how to know when this notification's events have been consumed?
    watched = (struct watchedVol*)(intptr_t)nstate;
    if (!watched)   goto finish;

    // is the volume valid? (notification should have been canceled)
    if (CFDictionaryGetCountOfValue(sFsysWatchDict, watched)) {
        CFRunLoopTimerContext tc = { 0, watched, NULL, NULL, NULL };
        CFAbsoluteTime firetime = CFAbsoluteTimeGetCurrent() + kWatchSettleTime;

        // cancel any existing timer (evidently updates are in progress)
        if (watched->delayer)
            CFRunLoopTimerInvalidate(watched->delayer);

        // schedule a timer to call check_now after a delay
        watched->delayer=CFRunLoopTimerCreate(nil,firetime,0,0,0,check_now,&tc);
        if (!watched->delayer)      goto finish;

        CFRunLoopAddTimer(CFRunLoopGetCurrent(), watched->delayer,
            kCFRunLoopDefaultMode);
        CFRelease(watched->delayer);  // so later invalidation will free
    } else {
        kextd_error_log("invalid token/volume: %d, %p", token, watched);
    }

    result = 0;

finish:
    if (result)
        kextd_error_log("couldn't respond to filesystem change notification!");

    return;
}
#endif

#if !TARGET_OS_EMBEDDED
/******************************************************************************
 * check_now, called after the timer expires, calls check_rebuild() 
 * It does not look at errcount because if something changed, we're willing
 * to look at it again.
 *****************************************************************************/
void check_now(CFRunLoopTimerRef timer, void *info)
{
    struct watchedVol *watched = (struct watchedVol*)info;
    // kextd_log("DEBUG: check_now(%p): entry", info);

    // is the volume still being watched?
    if (watched && CFDictionaryGetCountOfValue(sFsysWatchDict, watched)) {
        watched->delayer = NULL;        // timer is no longer pending
        (void)check_rebuild(watched);   // don't care what it did
    } else {
        kextd_log("%p's timer fired when it should have been invalid", watched);
    }
}
#endif

/******************************************************************************
 * returns the result of fork/exec (negative on error; pid on success)
 * a helper returning an error doesn't count (?)
 * - Boolean 'force' passes -f so that bootstamps are ignored
 *****************************************************************************/
// kextcache -u helper sets up argv
static pid_t rebuild_all(struct bootCaches *caches, Boolean force)
{
    pid_t rval = -1;
    int argc, argi = 0; 
    char **kcargs = NULL;

    //  argv[0] '-F'  '-u'  root          -f ?       NULL
    argc =  1  +  1  +  1  +  1  + (force == true) +  1;
    kcargs = malloc(argc * sizeof(char*));
    if (!kcargs)    goto finish;

    kcargs[argi++] = "/usr/sbin/kextcache";
    kcargs[argi++] = "-F";      // lower priority within kextcache
    if (force)
        kcargs[argi++] = "-f";
    kcargs[argi++] = "-u";
    kcargs[argi++] = caches->root;
    // kextcache reads bc.plist so nothing more needed

    kcargs[argi] = NULL;    // terminate the list

   /* wait:false means the return value is <0 for fork/exec failures and
    * the pid of the forked process if >0.
    */
    rval = fork_program(kcargs[0], kcargs, 0 /* delay */, false /* wait */);

finish:
    if (kcargs)     free(kcargs);

    if (rval < 0)
        kextd_error_log("error launching kextcache -u");

    return rval;
}

/******************************************************************************
 * check_rebuild uses needUpdates() to stat everything and rebuilds as necessary
 * - kextcache -u used to do all the work (rebuild mkext, boot's etc)
 *
 * XX if kextcache is broken (e.g. a copy of 'false'), errcount will never
 * be incremented -> infiniste reboot stall.  Enforcing a kWatchSettleTime
 * between launches (even for reboot requests) would mean we could 
 * reasonably give up after launching five kextcaches and not hearing back.
 *****************************************************************************/
static Boolean check_rebuild(struct watchedVol *watched)
{
    Boolean launched = false;
    Boolean rebuild;

    // if we came in some other way and there's a timer pending, cancel it
    if (watched->delayer) {  
        CFRunLoopTimerInvalidate(watched->delayer);  // runloop holds last ref
        watched->delayer = NULL;
    }

    // stat stuff to see if a rebuild is needed
    // errcount updated to cap handleRebootHandoff()/checkAllWatched() retries.
    rebuild = check_mkext(watched->caches);
    if (!rebuild && watched->isBootRoot) {
        if (needUpdates(watched->caches, &rebuild, NULL, NULL, NULL)) {
            watched->errcount++;
            rebuild = true;
        }
    }

    if (rebuild) {
        if (rebuild_all(watched->caches, false) > 0) {
            launched = true;
        } else {
            watched->errcount++;
            kextd_error_log("error launching kextcache -u");
        }
    }

    if (0 == strcmp(watched->caches->root, "/") &&
        check_plist_cache(watched->caches)) {
        kextd_handle_signal(SIGHUP);
        // X someday SIGHUP may call back to rebuild_caches() to force update
    }

    return launched;
}


// ---- locking services (prototyped via MiG and kextmanager[_mig].defs) ----

/******************************************************************************
 * kextmanager_lock_reboot ensures "all clean" (used by shutdown(8), reboot(8))
 *****************************************************************************/
// iterator helper locking for locked or should-be-locked volumes
static void check_locked(const void *key, const void *val, void *ctx)
{
    struct watchedVol *watched = (struct watchedVol*)val;
    // pointer to the mountpoint_t at the other end
    char *busyVol = ctx;

    // report this one if:
    // it's already locked or if it needs a rebuild
    // check_vol_busy() ensures checks for excessive errors
    if (check_vol_busy(watched)) {
        strlcpy(busyVol, watched->caches->root, MNAMELEN);
    }
}

// create a CFMachPort with invalidation -> port_died
static CFMachPortRef
createWatchedPort(mach_port_t mport, void *ctx)
{
    CFMachPortRef rval = NULL;
    int result = ELAST + 1;
    CFRunLoopSourceRef invalidator;
    CFMachPortContext mp_ctx = { 0, ctx, 0, };
    CFRunLoopRef rl = CFRunLoopGetCurrent();

    if(!(rval = CFMachPortCreateWithPort(nil, mport, NULL, &mp_ctx, false)))
        goto finish;
    invalidator = CFMachPortCreateRunLoopSource(nil, rval, 0);
    if (!invalidator)       goto finish;
    CFMachPortSetInvalidationCallBack(rval, port_died);
    CFRunLoopAddSource(rl, invalidator, kCFRunLoopDefaultMode);
    CFRelease(invalidator); // owned by the runloop now

    result = 0;

finish:
    if (result && rval) {
        CFRelease(rval);
        rval = NULL;
    }

    return rval;
}

Boolean checkAllWatched(mountpoint_t busyVol)
{
    int result;

    // if we've contacted diskarb, scan the dictionary for locked items
    busyVol[0] = '\0';
    if (sFsysWatchDict) {
        CFDictionaryApplyFunction(sFsysWatchDict, check_locked, busyVol);
    }
    if (busyVol[0] == '\0') {
        result = 0;     // you got it!
    } else {
        // busyVol (at least) was locked, try again later
        result = EBUSY;
    }

    return result;
}

kern_return_t _kextmanager_lock_reboot(mach_port_t p, mach_port_t replyPort,
    mach_port_t client, int waitForLock, mountpoint_t busyVol, int *busyStatus)
{
    kern_return_t rval = KERN_FAILURE;
    int result = ELAST + 1;
    // kextd_log("DEBUG: _lock_reboot(..%d/%d..)...", client, replyPort);

    if (!busyStatus) {
        result = EINVAL;
        rval = KERN_SUCCESS;    // for MiG
        goto finish;
    }
    
    if (gClientUID != 0) {
        kextd_error_log("non-root doesn't need to lock for reboot");
        result = EPERM;
        rval = KERN_SUCCESS;    // for MiG
        goto finish;
    }
    
    // shutdown/reboot proceed on result == EALREADY
    if (sRebootLock) {
        result = EALREADY;
        rval = KERN_SUCCESS;    // for MiG
        kextd_log("WARNING: reboot lock request while reboot in progress");
        goto finish;
    }

    // check all the volumes we are watching and 
    // if any new volumes have become eligible
    if (checkAllWatched(busyVol) || reconsiderVolumes(busyVol)) {
        result = EBUSY;
        rval = KERN_SUCCESS;    // for MiG
    } else {
        // great, this guy gets to take the uber reboot lock
        if (!(sRebootLock = createWatchedPort(client, &sRebootLock)))
            goto finish;
        result = 0;             // success
        rval = KERN_SUCCESS;    // for MiG
    }

    // should we reply now or later?
    if (waitForLock && result == EBUSY && sRebootWaiter == NULL) {
        // client will block until we reply with lock or failure
        // [&sRebootLock is context for all interested in the reboot lock]
        if (!(sRebootWaiter = createWatchedPort(client, &sRebootLock)))
            goto finish;

        // stash reply port so we can get it later (X someday dual-use port?)
        CFDictionarySetValue(sReplyPorts, sRebootWaiter, (void*)replyPort);
        rval = MIG_NO_REPLY;    // for MiG; no result
    } else {
        // we reply to the client if: a. failure other than EBUSY,
        // b. the client won't wait, or c. we've no way to track him.
        rval = KERN_SUCCESS;        // MiG will return to the caller
    }

finish:
    if (rval == KERN_SUCCESS) {
        *busyStatus = result;
    } else if (rval != MIG_NO_REPLY) {
        kextd_error_log("error %d locking for reboot", rval);
    }

    // pop up a dialog if reboot is going to stall
    if (result == EBUSY && waitForLock) {
        CFMutableArrayRef msg;
        kextd_log("'%s' updating, delaying reboot", busyVol);

        if((msg = CFArrayCreateMutable(nil, 1, &kCFTypeArrayCallBacks))) {
            CFArrayAppendValue(msg, CFSTR(
                "Caches necessary for booting are being updated.  "
                "System shutdown will proceed once the caches are up to date."
                ));
            kextd_raise_notification(CFSTR("Updating Boot Caches"), msg);
            CFRelease(msg);
        }
    }

    return rval;
}

/******************************************************************************
 * _kextmanager_lock_volume locks volumes for kextcache
 * - vol_uuid is in CFUUIDBytes
 *****************************************************************************/
kern_return_t _kextmanager_lock_volume(mach_port_t p, mach_port_t replyPort,
    mach_port_t client, uuid_t vol_uuid, int waitForLock, int *lockStatus)
{
    kern_return_t rval = KERN_FAILURE;
    int result;
    CFUUIDBytes uuidBytes;
    CFUUIDRef volUUID = NULL;
    struct watchedVol *watched = NULL;
    struct statfs sfs;
    // kextd_log("DEBUG: _lock_volume(..%d..)...", client);

    if (!lockStatus) {
        kextd_error_log("kextmanager_lock_volume requires lockStatus != NULL");
        result = EINVAL;
        rval = KERN_SUCCESS;    // for MiG
    }

    if (gClientUID != 0 /*watched->fsinfo->f_owner ?*/) {
        kextd_error_log("non-root doesn't need to lock or unlock volumes");
        result = EPERM;
        rval = KERN_SUCCESS;    // for MiG
        goto finish;
    }

    // still initializing; sorry (XX someday could allow you to wait)
    if (!sFsysWatchDict) {
        result = EAGAIN;
        rval = KERN_SUCCESS;    // for MiG
        goto finish;
    }

    // still initializing / rebooting -> client not getting the lock
    if (sRebootLock) {
        result = ENOLCK;
        rval = KERN_SUCCESS;    // for MiG
        goto finish;
    }

    result = ENOMEM;
    memcpy(&uuidBytes.byte0, vol_uuid, sizeof(uuid_t));
    volUUID = CFUUIDCreateFromUUIDBytes(nil, uuidBytes);
    if (!volUUID)   goto finish;
    watched = (void*)CFDictionaryGetValue(sFsysWatchDict, volUUID);
    if (!watched) {
        result = ENOENT;
        rval = KERN_SUCCESS;    // for MiG
        goto finish;
    }

    // if not locked, grant the lock
    if (watched->lock == NULL) {
        // take lock
        if (!(watched->lock = createWatchedPort(client, watched))) {
            goto finish;
        }
        // try to enable owners if not currently honored (XX ignore failure?)
        if (statfs(watched->caches->root, &sfs) == 0 &&
                (sfs.f_flags & MNT_IGNORE_OWNERSHIP)) {
            toggleOwners(watched->caches->root, true);      // logs errors
            watched->disableOwners = true;
        }
        result = 0;             // success; lock granted
        rval = KERN_SUCCESS;    // for MiG
    } else {
        // lock can't be granted; let the client wait if willing
        if (waitForLock) {
            rval = MIG_NO_REPLY;    // for MiG; no result
        } else {
            result = EBUSY;         // for client
            rval = KERN_SUCCESS;    // for MiG
        }
    }

    // if we're not replying yet, so add client to the wait queue
    if (rval == MIG_NO_REPLY) {
        CFMachPortRef waiter;

        // create waiter array (of CFMachPortRefs) if needed
        if (!watched->waiters) {
            watched->waiters=CFArrayCreateMutable(0,1,&kCFTypeArrayCallBacks);
            if (!watched->waiters) {
                rval = KERN_FAILURE;
                goto finish;
            }
        }

        // create waiter and insert into array
        if (!(waiter = createWatchedPort(client, watched))) {
            rval = KERN_FAILURE;
            goto finish;
        }

        // store waiter, replyPort; cleanupPort releases waiter create above
        CFArrayAppendValue(watched->waiters, waiter);
        CFDictionarySetValue(sReplyPorts, waiter, (void*)replyPort);

        // success: rval remains MIG_NO_REPLY
    }


finish:
    if (volUUID)  CFRelease(volUUID);

    if (rval == KERN_SUCCESS) {
        *lockStatus = result;
    } else if (rval != MIG_NO_REPLY && result != EPERM) {
        kextd_error_log("error %d locking '%s'", rval, watched->caches->root);
        cleanupPort(&watched->lock);
    } 

    return rval;
}

/******************************************************************************
 * _kextmanager_unlock_volume unlocks for clients (i.e. kextcache)
 *****************************************************************************/
kern_return_t _kextmanager_unlock_volume(mach_port_t p, mach_port_t client,
    uuid_t vol_uuid, int exitstatus)
{
    kern_return_t rval = KERN_FAILURE;
    CFUUIDRef volUUID = NULL;
    struct watchedVol *watched = NULL;
    CFUUIDBytes uuidBytes;
    // kextd_log("DEBUG: _kextmanager_unlock_volume()...");

    // since we don't need the extra send right added by MiG (XX why?)
    if (mach_port_deallocate(mach_task_self(), client))  goto finish;

    if (gClientUID != 0 /*watched->fsinfo->f_owner ?*/) {
        kextd_error_log("non-root doesn't need to lock or unlock volumes");
        rval = KERN_SUCCESS;
        goto finish;
    }

    // make sure we're set up
    if (!sFsysWatchDict)    goto finish;

    memcpy(&uuidBytes.byte0, vol_uuid, sizeof(uuid_t));
    volUUID = CFUUIDCreateFromUUIDBytes(nil, uuidBytes);
    if (!volUUID)           goto finish;
    watched = (void*)CFDictionaryGetValue(sFsysWatchDict, volUUID);
    if (!watched)           goto finish;

    if (!watched->lock) {
        kextd_error_log("'%s' isn't locked", watched->caches->root);
        goto finish;
    }
    
    if (client != CFMachPortGetPort(watched->lock)) {
        kextd_error_log("%d didn't lock '%s'", client, watched->caches->root);
        goto finish;
    }

    // okay, recording any error and releasing the lock
    if (exitstatus) {
        watched->errcount++;
        kextd_log("kextcache error updating %s (error count: %d)",
            watched->caches->root, watched->errcount);
    } else {
        if (watched->errcount > 0) {
            // put reassuring message in the log
            kextd_log("kextcache succeeded with '%s' (previously failed)",
                watched->caches->root);
            watched->errcount = 0;
        }
    }
 
    // disable owners if we enabled them for the locker
    if (watched->disableOwners) {
        toggleOwners(watched->caches->root, false);     // logs errors
        watched->disableOwners = false;
    }

    // if kextcache failed, handleRebootHandoff() will fire off another
    // but only if errcount is less than the failure limit
    handleWatchedHandoff(watched);
    if (!sRebootLock && sRebootWaiter)
        handleRebootHandoff();

    // once upon a time, we thought we could save five seconds here
    // instead, we will just call kextcache -u and it will build the mkext

    rval = KERN_SUCCESS;

finish:
    if (volUUID)    CFRelease(volUUID);
    if (rval && watched) {
        kextd_error_log("couldn't unlock '%s'", watched->caches->root);
    }

    return rval;
}

/* {
mach_port_urefs_t refs = 0xff;
kextd_log("DEBUG: mach_port_get_refs(..%d, send..): %x -> %x ref(s)", client,
mach_port_get_refs(mach_task_self(), client, MACH_PORT_RIGHT_SEND, &refs),refs);
} */


/******************************************************************************
* port_died() tells us when a tracked send right goes away.
* We track send rights (on the client ports passed to us) as long as we
* have resources allocated to those clients.  If they die, we get notified
* that the send right went away and then we clean up the associated resource.
*
* This function should only be called when shutdown/reboot exits before kextd
* or when a kextcache process is terminated against its will.
*
* If the client explicitly deallocates its *receive* right / port while we are
* tracking the corresponding send right, port_died() is also called, though
* kextcache should unlock the volume before doing that.
*****************************************************************************/
static void port_died(CFMachPortRef cfport, void *info)
{
    mach_port_t mport = cfport ? CFMachPortGetPort(cfport) : MACH_PORT_NULL;
    struct watchedVol* watched;
    // kextd_log("DEBUG: port_died(%p/%d): entry", cfport, mport);

    // all watched-associated ports should have context
    if (!info || !cfport) {
        kextd_error_log("port_died() fatal error: invalid data");
        goto finish;
    }


    // only means of release for reboot lock
    if (info == &sRebootLock) {
        if (cfport == sRebootLock) {
            // reboot/shutdown happened to exit before kextd
            // XX start timer now or when reboot lock granted?
            cleanupPort(&sRebootLock);
        } else if (cfport == sRebootWaiter) {
            cleanupPort(&sRebootWaiter);        // gave up waiting
        } else {
            kextd_error_log("improperly tracked shutdown/reboot process died");
        }
        goto finish;
    }


    // else ... handle volume lockers and waiters

    watched = (struct watchedVol*)info;

    // vol_disappeared() removes the volume from sFsysWatchDict before
    // cleaning up all the waiters, so unless the runloop somehow jumped
    // over here from the midst of that callout, no ports affiliated
    // with missing watchVol*'s should be dying.  Even multiple
    // reboot waiters would all have the same context and the mach
    // port check above would catch them.
    if (CFDictionaryGetCountOfValue(sFsysWatchDict, watched) == 0) {
        kextd_error_log("WARNING: missing context for deallocated helper port");
        cleanupPort(&cfport);
        goto finish;
    }

    // watched points to valid data ... was it the locker?
    if (watched->lock && mport == CFMachPortGetPort(watched->lock)) {
        // try to disable owners if we enabled them for the locker
        if (watched->disableOwners) {
            toggleOwners(watched->caches->root, false);  // logs errors
            watched->disableOwners = false;
        }
        
        // if locked, is anyone waiting?
        if (watched->lock) {
            kextd_error_log("%p exited without unlocking '%s'",
                            watched->lock, watched->caches->root);
            handleWatchedHandoff(watched);      // cleans up watched->lock
            if (!sRebootLock && sRebootWaiter)
                handleRebootHandoff();
        }
        // if we were storing the worker pid, we'd clean it up here

    } else {    // it must have been a waiter
        CFIndex i;
        if (!watched->waiters) {
            kextd_log("WARNING: presumed waiter died, but no waiters");
            goto finish;
        }

        for (i = CFArrayGetCount(watched->waiters); i-- > 0;) {
            CFMachPortRef waiter; 

            waiter = (CFMachPortRef)CFArrayGetValueAtIndex(watched->waiters,i);
            if (mport == CFMachPortGetPort(waiter)) {
                cleanupPort(&waiter);       // --retainCount
                CFArrayRemoveValueAtIndex(watched->waiters, i); // release
                goto finish;      // success
            }
        }

        kextd_log("WARNING: %s: unknown helper exited", watched->caches->root);
    }

finish:
    return;
}

/******************************************************************************
 * reconsiderVolume() rechecks to see if a volume has become interesting.
 * Given that we watch owners-ignored volumes, reconsiderVolume() should
 * only be needed for OS copies.
 *****************************************************************************/
static Boolean reconsiderVolume(mountpoint_t volToCheck)
{
    Boolean rval = false;
#if !TARGET_OS_EMBEDDED
    int result = 0;
    DADiskRef disk = NULL;
    CFDictionaryRef dadesc = NULL;
    CFUUIDRef volUUID;

    // if unknown to diskarb, we don't care
    disk = createDiskForMount(sDASession, volToCheck);
    if (!disk)      goto finish;

    result = -1;
    dadesc = DADiskCopyDescription(disk);
    if (!dadesc)    goto finish;
    volUUID = CFDictionaryGetValue(dadesc, kDADiskDescriptionVolumeUUIDKey);
    if (!volUUID)   goto finish;

    // if not watched, add to watch dict / start rebuild (rval = didLaunch)
    if (!CFDictionaryGetValue(sFsysWatchDict, volUUID)) {
        vol_appeared(disk, &rval);
    }

    result = 0;

finish:
    if (disk)       CFRelease(disk);
    if (dadesc)     CFRelease(dadesc);
    if (result) {
        kextd_error_log("error reconsidering volume %s", volToCheck);
    }

#endif
    return rval;
}

/******************************************************************************
 * reconsiderVolumes() iterates the mount list, reconsidering all local mounts.
 * reconsiderVolume() calls vol_appeared on any we aren't yet watching.
 * If any newly added one needed an update, busyVol is set to its mountpoint.
 *****************************************************************************/
static Boolean reconsiderVolumes(mountpoint_t busyVol)
{
    Boolean rval = false;
    char *errmsg = NULL;
    int nfsys, i;
    size_t bufsz;
    struct statfs *mounts = NULL;

    // if not set up ...
    if (!sDASession)        goto finish;

    errmsg = "error while getting mount list";
    if (-1 == (nfsys = getfsstat(NULL, 0, 0)))      goto finish;
    bufsz = nfsys * sizeof(struct statfs);
    if (!(mounts = malloc(bufsz)))                  goto finish;
    if (-1 == getfsstat(mounts, bufsz, MNT_NOWAIT)) goto finish;

    errmsg = NULL;  // let reconsiderVolume() take it from here
    for (i = 0; i < nfsys; i++) {
        struct statfs *sfs = &mounts[i];

        if (sfs->f_flags & MNT_LOCAL && strcmp(sfs->f_fstypename, "devfs")) {
            if (reconsiderVolume(sfs->f_mntonname) && !rval) {
                // only capture first volume, but check them all
                strlcpy(busyVol, sfs->f_mntonname, MNAMELEN);
                rval = true;
            }
        }
    }

    errmsg = NULL;

finish:
    if (errmsg)     kextd_error_log(errmsg);
    if (mounts)     free(mounts);

    return rval;
}

/******************************************************************************
 * toggleOwners() enables or disables owners as requested
 *****************************************************************************/
static void toggleOwners(mountpoint_t mount, Boolean enableOwners)
{
#if !TARGET_OS_EMBEDDED
    int result = ELAST + 1;
    DASessionRef session = NULL;
    CFStringRef toggleMode = CFSTR("toggleOwnersMode");
    CFURLRef volURL;
    DADiskRef disk = NULL;
    DADissenterRef dis = (void*)kCFNull;
    CFStringRef mountargs[] = { CFSTR("update"), NULL,  NULL };

    if (enableOwners) {
        mountargs[1] = CFSTR("owners");
    } else {
        mountargs[1] = CFSTR("noowners");
    }

    // same 'dis' logic as mountBoot in update_boot.c
    if (!(session = DASessionCreate(nil)))      goto finish;
    DASessionScheduleWithRunLoop(session, CFRunLoopGetCurrent(), toggleMode);
    volURL=CFURLCreateFromFileSystemRepresentation(nil,(void*)mount,MNAMELEN,1);
    if (!(volURL))      goto finish;
    if (!(disk = DADiskCreateFromVolumePath(nil, session, volURL))) goto finish;
    DADiskMountWithArguments(disk, NULL, kDADiskMountOptionDefault, _daDone,
                             &dis, mountargs);

    while (dis == (void*)kCFNull) {
        CFRunLoopRunInMode(toggleMode, 0, true);    // _daDone updates 'dis'
    }
    if (dis)    goto finish;

    result = 0;

finish:
    if (dis && dis != (void*)kCFNull)   CFRelease(dis);
    if (disk)                           CFRelease(disk);
    if (session)                        CFRelease(session);

    if (result) {
        kextd_log("WARNING: couldn't %s owners for %s", 
            enableOwners ? "enable":"disable", mount);
    }
#endif
}

/*******************************************************************************
* updateRAIDSet() -- Something on a RAID set has changed, so we may need to
* update its boot partition info.
*******************************************************************************/
#define RAID_MATCH_SIZE   (2)

void updateRAIDSet(
    CFNotificationCenterRef center,
    void * observer,
    CFStringRef name,
    const void * object,
    CFDictionaryRef userInfo)
{
    char * errorMessage = NULL;
    CFStringRef matchingKeys[RAID_MATCH_SIZE] = {
        CFSTR("RAID"),
        CFSTR("UUID") };
    CFTypeRef matchingValues[RAID_MATCH_SIZE] = {
        (CFTypeRef)kCFBooleanTrue,
        (CFTypeRef)object };
    CFDictionaryRef matchPropertyDict = NULL;
    CFMutableDictionaryRef matchingDict = NULL;
    io_service_t theRAIDSet = MACH_PORT_NULL;
    CFStringRef bsdName = NULL;
    struct watchedVol * watched = NULL;  // do not free

    // nothing to do if we're not watching yet
    if (!sFsysWatchDict)    goto finish;    

    errorMessage = "No RAID set named in RAID set changed notification.";
    if (!object) {
        goto finish;
    }

    errorMessage = "Unable to create matching dictionary for RAID set.";
    matchPropertyDict = CFDictionaryCreate(kCFAllocatorDefault,
        (const void **)&matchingKeys,
        (const void **)&matchingValues,
        RAID_MATCH_SIZE,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!matchPropertyDict) {
        goto finish;
    }

    matchingDict = CFDictionaryCreateMutable(kCFAllocatorDefault,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!matchingDict) {
        goto finish;
    }
    CFDictionarySetValue(matchingDict, CFSTR(kIOPropertyMatchKey), 
        matchPropertyDict);

    errorMessage = NULL;    // maybe the RAID just went away
    theRAIDSet  = IOServiceGetMatchingService(kIOMasterPortDefault,
        matchingDict);
    matchingDict = NULL;  // IOServiceGetMatchingService() consumes reference!
    if (!theRAIDSet) {
        goto finish;
    }

    errorMessage = "Missing BSD Name for updated RAID set.";
    bsdName = IORegistryEntryCreateCFProperty(theRAIDSet,
        CFSTR("BSD Name"),
        kCFAllocatorDefault,
        0);
    if (!bsdName) {
        goto finish;
    }

    watched = (void*)CFDictionaryGetValue(sFsysWatchDict, bsdName);
    if (watched) {
        (void)rebuild_all(watched->caches, true /* force rebuild */);
    }

    errorMessage = NULL;

finish:
    if (errorMessage) {
        kextd_error_log(errorMessage);
    }
    if (matchPropertyDict) CFRelease(matchPropertyDict);
    if (matchingDict)      CFRelease(matchingDict);
    if (theRAIDSet)        IOObjectRelease(theRAIDSet);
    if (bsdName)           CFRelease(bsdName);
    return;
}
