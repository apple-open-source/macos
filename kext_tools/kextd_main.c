/*
 * Copyright (c) 2000-2008 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPriv.h>  // for _CFRunLoopSetCurrent()
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <IOKit/IOCFURLAccess.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/storage/RAID/AppleRAIDUserLib.h>
#include <IOKit/kext/OSKext.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_error.h>
#include <mach-o/arch.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <libc.h>
#include <servers/bootstrap.h>
#include <signal.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <unistd.h>
#include <paths.h>

#include <IOKit/kext/OSKext.h>
#include <IOKit/kext/OSKextPrivate.h>
#include <IOKit/kext/kextmanager_types.h>
#include <bootfiles.h>

#include "kextd_main.h"

#include "kext_tools_util.h"
#include "kextd_globals.h"
#include "kextd_personalities.h"
#include "kextd_mig_server.h"
#include "kextd_request.h"
#include "kextd_usernotification.h"
#include "kextd_watchvol.h"

#include "bootcaches.h"


/*******************************************************************************
* Globals set from invocation arguments (xxx - could use fewer globals).
*******************************************************************************/
const char * progname = "(unknown)";  // don't free

/*******************************************************************************
*******************************************************************************/
struct option sOptInfo[] = {
    { kOptNameHelp,             no_argument,       NULL, kOptHelp },
    { kOptNameNoCaches,         no_argument,       NULL, kOptNoCaches },
    { kOptNameDebug,            no_argument,       NULL, kOptDebug },
    { kOptNameRepository,       no_argument,       NULL, kOptRepository },

    { kOptNameQuiet,            required_argument, NULL, kOptQuiet },
    { kOptNameVerbose,          optional_argument, NULL, kOptVerbose },

    { NULL, 0, NULL, 0 }  // sentinel to terminate list
};

/*******************************************************************************
* Globals created at run time.
*******************************************************************************/
KextdArgs                 sToolArgs;
static CFArrayRef         sAllKexts                         = NULL;

Boolean                   gKernelRequestsPending = false;

// all the following are released in setUpServer()
static CFRunLoopTimerRef  sReleaseKextsTimer                = NULL;
static CFRunLoopSourceRef sClientRequestRunLoopSource       = NULL;
static CFMachPortRef      sKextdSignalMachPort              = NULL;
static mach_port_t        sKextSignalMachPortMachPort       = MACH_PORT_NULL;
static CFRunLoopSourceRef sSignalRunLoopSource              = NULL;

const NXArchInfo        * gKernelArchInfo                   = NULL;  // do not free

ExitStatus                sKextdExitStatus                  = kKextdExitOK;

/*******************************************************************************
*******************************************************************************/

int main(int argc, char * const * argv)
{
    char       logSpecBuffer[16];  // enough for a 64-bit hex value

   /*****
    * Find out what my name is.
    */
    progname = rindex(argv[0], '/');
    if (progname) {
        progname++;   // go past the '/'
    } else {
        progname = (char *)argv[0];
    }

    OSKextSetLogOutputFunction(&tool_log);

   /* Read command line and set a few other args that don't involve
    * logging. (Syslog is opened up just below; we have to do that after
    * parsing args, unfortunately.)
    */
    sKextdExitStatus = readArgs(argc, argv, &sToolArgs);
    if (sKextdExitStatus != EX_OK) {
        goto finish;
    }

   /*****
    * If not running in debug mode, then hook up to syslog.
    */
    if (!sToolArgs.debugMode) {
        tool_openlog("com.apple.kextd");
    }
    
   /*****
    * Set an environment variable that children (such as kextcache)
    * can use to alter behavior (logging to syslog etc.).
    */
    setenv("KEXTD_SPAWNED", "", /* overwrite */ 1);
    snprintf(logSpecBuffer, sizeof(logSpecBuffer), "0x%x",
        OSKextGetLogFilter(/* kernel? */ true));
    setenv("KEXT_LOG_FILTER_KERNEL", logSpecBuffer, /* overwrite */ 1);
    snprintf(logSpecBuffer, sizeof(logSpecBuffer), "0x%x",
        OSKextGetLogFilter(/* kernel? */ false));
    setenv("KEXT_LOG_FILTER_USER", logSpecBuffer, /* overwrite */ 1);

   /* Get the running kernel arch for OSKext ops & cache creation.
    */
    if (!gKernelArchInfo) {
        gKernelArchInfo = OSKextGetRunningKernelArchitecture();
        if (!gKernelArchInfo) {
            goto finish;
        }
    }

   /* Check kernel first for safe boot, -x/-safe-boot is only for debugging.
    */
    if (OSKextGetActualSafeBoot()) {
        sToolArgs.safeBootMode = true;
    } else if (sToolArgs.safeBootMode) {
        OSKextSetSimulatedSafeBoot(true);
    }

    if (sToolArgs.safeBootMode) {
        sToolArgs.useRepositoryCaches = false;  // never use caches in safe boot
    }

    OSKextSetUsesCaches(sToolArgs.useRepositoryCaches);

   /* safeBootMode may have been set from cmd line, check apart from the OSKext
    * call.  If safeboot, force a rebuild (if not in near future, at next reboot)
    */
    if (sToolArgs.safeBootMode) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogGeneralFlag | kOSKextLogLoadFlag,
            "Safe boot mode detected; invalidating system extensions caches.");
        utimes("/System/Library/Extensions", NULL);
    }

   /*****
    * Check Extensions.mkext needs/ed a rebuild (BootRoot depending)
    */
    checkStartupMkext(&sToolArgs);    

#if 0
   /*****
    * If we are booting with stale data, don't bother reading any kexts. 
    * XXX revisit since kmod_control loop needs manager, plus we don't
    * want to skip updating the mkext in the next clause.
    * Or maybe we shouldn't call kmod_control at all when stale?
    * XXX bootroot system hangs at boot w/stale even without this goto.  :P
    * (launchctl -> kextcache -U should be fixing and rebooting in that case)
    *
    * OSKext update: We no longer have a kmod_control loop, we get pings
    * from the kernel and handle requests as they come. Talk to S.
    */
    if (sToolArgs.staleStartupMkext) {
        goto server_start;
    }
#endif /* 0 - NOT USED */
    
    OSKextSetRecordsDiagnostics(kOSKextDiagnosticsFlagNone);
    readExtensions();

// server_start:    // unneeded?

    sKextdExitStatus = setUpServer(&sToolArgs);
    if (sKextdExitStatus != EX_OK) {
        goto finish;
    }

   /* Tell the IOCatalogue that we are ready to service load requests.
    */
    sendActiveToKernel();

   /*****
    * Send the kext personalities to the kernel to trigger matching.
    * Now that we have UUID dependency checks, the kernel shouldn't
    * be able to hurt itself if kexts are out of date somewhere.
    */
    if (kOSReturnSuccess != sendPersonalitiesToKernel()) {
        sKextdExitStatus = EX_OSERR;
        goto finish;
    }

   /* Let IOCatalogue drop the artificial busy count on the registry
    * now that it has personalities (which are sure to have naturally
    * bumped the busy count).
    */
    sendFinishedToKernel();

    // Start run loop
    CFRunLoopRun();

    // Runloop is done - for restart performance exit asap
    _exit(sKextdExitStatus);

finish:
#ifndef NO_CFUserNotification
    stopMonitoringConsoleUser();
#endif /* ifndef NO_CFUserNotification */

    kextd_stop_volwatch();    // no-op if watch_volumes not called
    
    if (sKextdExitStatus == kKextdExitHelp) {
        sKextdExitStatus = kKextdExitOK;
    }

    exit(sKextdExitStatus);

    SAFE_RELEASE(sToolArgs.repositoryURLs);

    return sKextdExitStatus;
}


#pragma mark Major Subroutines
/*******************************************************************************
* Major Subroutines
*******************************************************************************/
ExitStatus
readArgs(
    int            argc,
    char * const * argv,
    KextdArgs    * toolArgs)
{
    ExitStatus   result = EX_USAGE;
    CFArrayRef   SystemExtensionsFolderURLs = NULL;  // do not release
    struct stat  stat_buf;
    int          optchar;
    int          longindex;
    CFStringRef  scratchString   = NULL;  // must release
    CFNumberRef  scratchNumber   = NULL;  // must release
    CFURLRef     scratchURL      = NULL;  // must release

   /* Kextd is not interested in log messages from the kernel, since
    * kextd itself prints to the system log and this would duplicate
    * things coming from the kernel. If you want more kernel kext
    * logging, use the kextlog boot arg.
    */
    OSKextSetLogFilter(kDefaultServiceLogFilter, /* kernel? */ false);
    OSKextSetLogFilter(kOSKextLogSilentFilter, /* kernel? */ true);

    bzero(toolArgs, sizeof(*toolArgs));
    toolArgs->debugMode           = false;
    toolArgs->useRepositoryCaches = true;

    if (stat(kAppleSetupDonePath, &stat_buf) == -1 && errno == ENOENT) {
        toolArgs->firstBoot = true;
    } else {
        toolArgs->firstBoot = false;
    }

 
   /*****
    * Allocate collection objects.
    * xxx - we are logging before a -q might be processed...hm.
    */
    if (!createCFMutableArray(&toolArgs->repositoryURLs,
        &kCFTypeArrayCallBacks)) {

        result = EX_OSERR;
        OSKextLogMemError();
        exit(result);
    }

   /* Put the system extensions folders at the beginning of the list
    * of repositoryURLs (to which the -r flag might add).
    *
    * We might want to allow for an alternate set of folders someday,
    * but I've never seen any need for it. Maybe when we can retarget
    * boot to a whole System folder....
    */
    SystemExtensionsFolderURLs = OSKextGetSystemExtensionsFolderURLs();
    if (!SystemExtensionsFolderURLs) {

        result = EX_OSERR;
        OSKextLogMemError();
        result = EX_OSERR;
        goto finish;
    }
    CFArrayAppendArray(toolArgs->repositoryURLs, SystemExtensionsFolderURLs,
        RANGE_ALL(SystemExtensionsFolderURLs));

    while ((optchar = getopt_long_only(argc, (char * const *)argv,
        kOptChars, sOptInfo, &longindex)) != -1) {

        SAFE_RELEASE_NULL(scratchString);
        SAFE_RELEASE_NULL(scratchNumber);
        SAFE_RELEASE_NULL(scratchURL);

        switch (optchar) {
            case kOptHelp:
                usage(kUsageLevelFull);
                result = kKextdExitHelp;
                goto finish;
                break;
                
            case kOptRepository:
                scratchURL = CFURLCreateFromFileSystemRepresentation(
                    kCFAllocatorDefault,
                    (const UInt8 *)optarg, strlen(optarg), true);
                if (!scratchURL) {
                    OSKextLogStringError(/* kext */ NULL);
                    result = EX_OSERR;
                    goto finish;
                }
                CFArrayAppendValue(toolArgs->repositoryURLs, scratchURL);
                break;
                
            case kOptNoCaches:
                toolArgs->useRepositoryCaches = false;
                break;
                
            case kOptDebug:
                toolArgs->debugMode = true;
                break;
                
            case kOptQuiet:
                beQuiet();
                break;

            case kOptVerbose:
               /* Set the log flags by the command line, but then turn off
                * the kernel bridge again.
                */
                result = setLogFilterForOpt(argc, argv, /* forceOnFlags */ 0);
                OSKextSetLogFilter(kOSKextLogSilentFilter, /* kernel? */ true);
                break;

            case kOptSafeBoot:
                toolArgs->safeBootMode = true;
                toolArgs->useRepositoryCaches = false;  // -x implies -c
                break;

            default:
               /* getopt_long_only() prints an error message for us. */
                goto finish;
                break;

        } /* switch (optchar) */
    } /* while (optchar = getopt_long_only(...) */

    argc -= optind;
    argv += optind;

    if (argc) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Extra input on command line; %s....", argv[0]);
        goto finish;
    }
    
    result = EX_OK;

finish:
    SAFE_RELEASE(scratchString);
    SAFE_RELEASE(scratchNumber);
    SAFE_RELEASE(scratchURL);

    if (result == EX_USAGE) {
        usage(kUsageLevelBrief);
    }
    return result;
}

/*******************************************************************************
*******************************************************************************/
void checkStartupMkext(KextdArgs * toolArgs)
{
    struct stat extensions_stat_buf;
    struct stat mkext_stat_buf;
    Boolean outOfDate;

   /*****
    * Do some checks for the Boot!=Root case:
    * - note whether in-memory mkext differs from the one on the root
    * - note whether the mkext vs. Extensions folder timestamps are correct
    * 
    * launchd / kextcache -U *should* have done obvious updates by now
    * isNetboot() check should stay even if we change boot!=root policy.
    */

    // in-memory vs. root filesystem mkext CRC check
    if (isBootRootActive() && !isNetboot()) {
        toolArgs->staleBootNotificationNeeded = bootedFromDifferentMkext();
        if (toolArgs->staleBootNotificationNeeded) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                "Warning: mkext on root filesystem does not match "
                "the one used for startup");
        }
    }

    // root filesystem timestamp check
    outOfDate = (0 != stat(_kOSKextStartupMkextPath, &mkext_stat_buf));
    if (!outOfDate && (0 == stat(kSystemExtensionsDir, &extensions_stat_buf))) {
        outOfDate = (mkext_stat_buf.st_mtime !=
            (extensions_stat_buf.st_mtime + 1));
    }

    if (outOfDate) {
        do {
           /* 4618030: allow mkext rebuilds on startup if not BootRoot
            * had to fix kextcache to take the lock *after* forking
            * (-F was giving up the lock before the work was done!)
            */
            if (isBootRootActive()) {
                OSKextLog(/* kext */ NULL,
                    kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                    "Warning: mkext unexpectedly out of date relative "
                    "to Extensions folder.");
                toolArgs->staleStartupMkext = true;

                break;  // skip since BootRoot logic in watchvol.c will handle
            }
        } while (false);
    }
    return;
}

/*******************************************************************************
*******************************************************************************/
Boolean isNetboot(void)
{
    Boolean result = false;
    int     netboot_mib_name[] = { CTL_KERN, KERN_NETBOOT };
    int     netboot = 0;
    size_t  netboot_len = sizeof(netboot);

   /* Get the size of the buffer we need to allocate.
    */
   /* Now actually get the kernel version.
    */
    if (sysctl(netboot_mib_name, sizeof(netboot_mib_name) / sizeof(int),
        &netboot, &netboot_len, NULL, 0) != 0) {

        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Failed to detect netboot - %s.", strerror(errno));
        goto finish;
    }

    result = netboot ? true : false;

finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
void sendActiveToKernel(void)
{
    kern_return_t kernelResult;
    kernelResult = IOCatalogueSendData(kIOMasterPortDefault,
        kIOCatalogKextdActive, 0, 0);
    if (kernelResult != KERN_SUCCESS) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Failed to notify kernel that kextd is active - %s.",
            safe_mach_error_string(kernelResult));
    }

    return;
}

/*******************************************************************************
* sendFinishedToKernel()
*
* Tells the IOCatalogue that kextd is ready to service kernel requests.
* The IOCatalogue then drops its artificial busy count on the registry.
*******************************************************************************/
void sendFinishedToKernel(void)
{
    kern_return_t kernelResult;
    kernelResult = IOCatalogueSendData(kIOMasterPortDefault,
        kIOCatalogKextdFinishedLaunching, 0, 0);
    if (kernelResult != KERN_SUCCESS) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Failed to notify kernel that kextd is finished launching - %s.",
            safe_mach_error_string(kernelResult));
    }

    return;
}

/*******************************************************************************
* setUpServer()
*******************************************************************************/
ExitStatus setUpServer(KextdArgs * toolArgs)
{
    ExitStatus             result         = EX_OSERR;
    kern_return_t          kernelResult   = KERN_SUCCESS;
    unsigned int           sourcePriority = 1;
    CFMachPortRef          kextdMachPort  = NULL;  // must release
    mach_port_limits_t     limits;  // queue limit for signal-handler port
    mach_port_t            servicePort;

   /* Check in with the bootstrap services.
    */
    kernelResult = bootstrap_check_in(bootstrap_port,
            KEXTD_SERVER_NAME, &servicePort);

    if (kernelResult != BOOTSTRAP_SUCCESS) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag | kOSKextLogIPCFlag,
            "Failed server check-in - %s", bootstrap_strerror(kernelResult));
        exit(EX_OSERR);
    }

   /* Initialize the run loop so we can start adding sources.
    */
    if (!CFRunLoopGetCurrent()) {
       OSKextLog(/* kext */ NULL,
           kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
           "Failed to create run loop.");
        goto finish;
    }

   /*****
    * Add the runloop sources in decreasing priority (increasing "order").
    * Signals are handled first, followed by kernel requests, and then by
    * client requests. It's important that each source have a distinct
    * priority; sharing them causes unpredictable behavior with the runloop.
    * Note: CFRunLoop.h, however, says 'order' should generally be 0 for all.
    */

    kextdMachPort = CFMachPortCreateWithPort(kCFAllocatorDefault,
        servicePort, kextd_mach_port_callback, /* CFMachPortContext */ NULL,
        /* shouldFreeInfo? */ NULL);
    sClientRequestRunLoopSource = CFMachPortCreateRunLoopSource(
        kCFAllocatorDefault, kextdMachPort, sourcePriority++);
    if (!sClientRequestRunLoopSource) {
       OSKextLog(/* kext */ NULL,
           kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
           "Failed to create client request run loop source.");
        goto finish;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), sClientRequestRunLoopSource,
        kCFRunLoopDefaultMode);

    // 5519500: kextd_watch_volumes now holds off on updates on its own
    if (kextd_watch_volumes(sourcePriority++)) {
        goto finish;
    }

    sKextdSignalMachPort = CFMachPortCreate(kCFAllocatorDefault,
        handleSignalInRunloop, NULL, NULL);
    if (!sKextdSignalMachPort) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Failed to create signal-handling Mach port.");
        goto finish;
    }
    sKextSignalMachPortMachPort = CFMachPortGetPort(sKextdSignalMachPort);
    limits.mpl_qlimit = 1;
    kernelResult = mach_port_set_attributes(mach_task_self(),
        sKextSignalMachPortMachPort,
        MACH_PORT_LIMITS_INFO,
        (mach_port_info_t)&limits,
        MACH_PORT_LIMITS_INFO_COUNT);
    if (kernelResult != KERN_SUCCESS) {
        OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Failed to set signal-handling port limits.");
    }
    sSignalRunLoopSource = CFMachPortCreateRunLoopSource(
        kCFAllocatorDefault, sKextdSignalMachPort, sourcePriority++);
    if (!sSignalRunLoopSource) {
        OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Failed to create signal-handling run loop source.");
        goto finish;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), sSignalRunLoopSource,
        kCFRunLoopDefaultMode);

   /* Watch for RAID changes so we can forcibly update their boot partitions.
    */
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(),
        NULL, // const void *observer
        updateRAIDSet,
        CFSTR(kAppleRAIDNotificationSetChanged),
        NULL, // const void *object
        CFNotificationSuspensionBehaviorHold);
    kernelResult = AppleRAIDEnableNotifications();
    if (kernelResult != KERN_SUCCESS) {
        OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Failed to register for RAID notifications.");
    }

#ifndef NO_CFUserNotification
    result = startMonitoringConsoleUser(toolArgs, &sourcePriority);
    if (result != EX_OK) {
        goto finish;
    }
#endif /* ifndef NO_CFUserNotification */

    signal(SIGHUP,  handleSignal);
    signal(SIGTERM, handleSignal);
    signal(SIGCHLD, handleSignal);

    result = EX_OK;

finish:
    SAFE_RELEASE(sClientRequestRunLoopSource);
    SAFE_RELEASE(sKextdSignalMachPort);
    SAFE_RELEASE(sSignalRunLoopSource);
    SAFE_RELEASE(kextdMachPort);

    return result;
}

/******************************************************************************
* isBootRootActive() checks for the booter hint
******************************************************************************/
bool isBootRootActive(void)
{
    int          result       = false;
    io_service_t chosen       = 0;     // must IOObjectRelease()
    CFTypeRef    bootrootProp = 0;  // must CFRelease()

    chosen = IORegistryEntryFromPath(kIOMasterPortDefault,
           "IODeviceTree:/chosen");
    if (!chosen) {
        goto finish;
    }
    
    bootrootProp = IORegistryEntryCreateCFProperty(
        chosen, CFSTR(kBootRootActiveKey), kCFAllocatorDefault,
        0 /* options */);
        
   /* Mere presence of the property indicates that we are
    * boot!=root, type and value are irrelevant.
    */
    if (bootrootProp) {
        result = true;
    }
    
finish:
    if (chosen)       IOObjectRelease(chosen);
    SAFE_RELEASE(bootrootProp);
    return result;
}


/*******************************************************************************
* On receiving a SIGHUP or SIGTERM, the daemon sends a Mach message to the
* signal port, causing the run loop handler function rescanExtensions() to be
* called on the main thread.
*
* IMPORTANT: This is a UNIX signal handler, so no allocating or any other unsafe
* calls. Sending a hand-rolled Mach message off the stack is okay.
*******************************************************************************/
void handleSignal(int signum)
{
    kextd_mach_msg_signal_t msg;
    kern_return_t           kernelResult;

    msg.signum                  = signum;

    msg.header.msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    msg.header.msgh_size        = sizeof(msg.header);
    msg.header.msgh_remote_port = sKextSignalMachPortMachPort;
    msg.header.msgh_local_port  = MACH_PORT_NULL;
    msg.header.msgh_id          = 0;

    kernelResult = mach_msg(
        &msg.header,                          /* msg */
        MACH_SEND_MSG | MACH_SEND_TIMEOUT,    /* options */
        sizeof(msg),                          /* send_size */
        0,                          /* rcv_size */
        MACH_PORT_NULL,             /* rcv_name */
        0,                          /* timeout */
        MACH_PORT_NULL);            /* notify */

    return;
}

/*******************************************************************************
*******************************************************************************/
void handleSignalInRunloop(
    CFMachPortRef   port,
    void          * msg,
    CFIndex         size,
    void          * info)
{
    kextd_mach_msg_signal_t * signal_msg = (kextd_mach_msg_signal_t *)msg;
    int                       signum = signal_msg->signum;
    
    if (signum == SIGHUP) {
        rescanExtensions();
    } else if (signum == SIGTERM) {
        CFRunLoopStop(CFRunLoopGetCurrent());
        sKextdExitStatus = kKextdExitSigterm;
    } else if (signum == SIGCHLD) {
        pid_t child_pid    = -1;
        int   child_status = 0;
        
       /* Reap all spawned child processes that have exited.
        */
        do {
            child_pid = waitpid(-1 /* any child */, &child_status, WNOHANG);
            if (child_pid == -1) {
                if (errno != ECHILD) {
                    OSKextLog(/* kext */ NULL,
                        kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                        "Error %s waiting on child processes.",
                        strerror(errno));
                }
            } else if (child_pid > 0) {
                OSKextLogSpec logSpec;

                // EX_SOFTWARE generally means kextcache called
                // _kextmanager_unlock_volume which logs the error
                if (WEXITSTATUS(child_status)==0 || child_status==EX_SOFTWARE) {
                    logSpec = kOSKextLogDetailLevel;
                } else {
                    logSpec = kOSKextLogErrorLevel;
                }
                OSKextLog(/* kext */ NULL, logSpec,
                    "async child pid %d exited with status %d",
                    child_pid, WEXITSTATUS(child_status));
            }
        } while (child_pid > 0);
        
    }

    return;
}

/*******************************************************************************
* This function reads the extensions if necessary; that is, if the extensions
* folder looks like it's changed, or if we don' thave the extensions read in
* memory already.
*
* This function does not update personalities in the kernel or update any
* caches other than those the OSKext library updates in the course of reading
* the extensions. We have a notify thread on the extensions folder that does
* that anyhow, with a slight delay.
*******************************************************************************/
void readExtensions(void)
{
    static Boolean         haveStattedExtensions = false;
    static struct timespec lastTimespec;
    struct stat            statBuf;
    Boolean                needReread            = false;

   /* If we fail to stat the extensions folder, note need to reread.
    * If it succeeds, and we have an old timespec and the new one differs,
    * note need to reread.
    */
    if (0 != stat(kSystemExtensionsDir, &statBuf)) {
        needReread = true;
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Failed to stat extensions folder (%s); rereading.",
            strerror(errno));
    } else {
        if (!haveStattedExtensions) {
            haveStattedExtensions = true;
        } else if ((lastTimespec.tv_sec != statBuf.st_mtimespec.tv_sec) ||
            (lastTimespec.tv_nsec != statBuf.st_mtimespec.tv_nsec)) {

            needReread = true;
            OSKextLog(/* kext */ NULL,
                kOSKextLogProgressLevel | kOSKextLogGeneralFlag,
                "Extensions folder mod time has changed; rereading.");
        }
        lastTimespec = statBuf.st_mtimespec;
    }
    
    if (needReread) {
        releaseExtensions(/* timer */ NULL, /* context */ NULL);
    }

    if (!sAllKexts && sToolArgs.repositoryURLs) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogProgressLevel | kOSKextLogGeneralFlag,
            "Reading extensions.");
        sAllKexts = OSKextCreateKextsFromURLs(kCFAllocatorDefault,
            sToolArgs.repositoryURLs);
    }
    scheduleReleaseExtensions();
    return;
}

/*******************************************************************************
*******************************************************************************/
void scheduleReleaseExtensions(void)
{
    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel | kOSKextLogGeneralFlag,
        "%scheduling release of all kexts.", sReleaseKextsTimer ? "Res" : "S");

    if (sReleaseKextsTimer) {
        CFRunLoopTimerInvalidate(sReleaseKextsTimer);
        SAFE_RELEASE_NULL(sReleaseKextsTimer);
    }
    sReleaseKextsTimer = CFRunLoopTimerCreate(kCFAllocatorDefault,
        CFAbsoluteTimeGetCurrent() + kReleaseKextsDelay, /* interval */ 0,
        /* flags */ 0, /* order */ 0, &releaseExtensions, /* context */ NULL);
    if (!sReleaseKextsTimer) {
        OSKextLogMemError();
        goto finish;
    }
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), sReleaseKextsTimer,
        kCFRunLoopDefaultMode);

finish:
    return;
}

/*******************************************************************************
*******************************************************************************/
void releaseExtensions(
    CFRunLoopTimerRef   timer,
    void              * context __unused)
{
    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel | kOSKextLogGeneralFlag,
        "Releasing all kexts.");

    if (timer == sReleaseKextsTimer) {
        SAFE_RELEASE_NULL(sReleaseKextsTimer);
    }
    SAFE_RELEASE_NULL(sAllKexts);
    return;
}

/*******************************************************************************
*******************************************************************************/
void rescanExtensions(void)
{
    CFIndex count, i;

    OSKextLog(/* kext */ NULL,
        kOSKextLogBasicLevel | kOSKextLogGeneralFlag,
        "Rescanning kernel extensions.");

#ifndef NO_CFUserNotification
    resetUserNotifications(/* dismissAlert */ false);
#endif /* ifndef NO_CFUserNotification */

    releaseExtensions(/* timer */ NULL, /* context */ NULL);
    readExtensions();
    
    // need to trigger check_rebuild (in watchvol.c) for mkext, etc
    // perhaps via mach message to the notification port
    // should we let it handle the ResetAllRepos?

    // xxx Should we exit if this fails?
    // xxx - Have IOCatalogue dump its personalities?
    sendPersonalitiesToKernel();

   /* Update the loginwindow prop/value cache(s) as necessary.
    * We just read 'em and don't use the values; this causes
    * the caches to be updated.
    */
    count = CFArrayGetCount(sToolArgs.repositoryURLs);
    for (i = 0; i < count; i++) {
        CFURLRef directoryURL = CFArrayGetValueAtIndex(
            sToolArgs.repositoryURLs, i);

        readKextPropertyValuesForDirectory(directoryURL,
            CFSTR(kOSBundleHelperKey), gKernelArchInfo,
            /* forceUpdate? */ true, /* values */ NULL);
    }
}

/*******************************************************************************
*******************************************************************************/
void usage(UsageLevel usageLevel)
{
    fprintf(stderr,
        "usage: %s [-c] [-d] [-f] [-h] [-j] [-r dir] ... [-v [1-6]] [-x]\n",
        progname);

    if (usageLevel == kUsageLevelBrief) {
        goto finish;
    }

    fprintf(stderr, "\n");

    fprintf(stderr, "Arguments and options\n");
    fprintf(stderr, "\n");

    fprintf(stderr, "-%s (-%c):\n"
        "        don't use repository caches; scan repository folders\n",
        kOptNameNoCaches, kOptNoCaches);
    fprintf(stderr, "-%s (-%c):\n"
        "        run in debug mode (log to stderr)\n",
        kOptNameDebug, kOptDebug);
    fprintf(stderr, "-%s <directory> (-%c):\n"
        "        look in <directory> for kexts in addition to system extensions folders\n",
        kOptNameRepository, kOptRepository);
    fprintf(stderr, "-%s (-%c):\n"
        "        run as if the system is in safe boot mode\n",
        kOptNameSafeBoot, kOptSafeBoot);
    fprintf(stderr, "\n");

    fprintf(stderr, "-%s (-%c):\n"
        "        quiet mode: log/print no informational or error messages\n",
        kOptNameQuiet, kOptQuiet);
    fprintf(stderr, "-%s [ 0-6 | 0x<flags> ] (-%c):\n"
        "        verbose mode; log/print info about analysis & loading\n",
        kOptNameVerbose, kOptVerbose);
    fprintf(stderr, "\n");

    fprintf(stderr, "-%s (-%c): print this message and exit\n",
        kOptNameHelp, kOptHelp);

finish:
    return;
}
