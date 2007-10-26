/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPriv.h>  // for _CFRunLoopSetCurrent()
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <IOKit/IOCFURLAccess.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/storage/RAID/AppleRAIDUserLib.h>
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

#include <IOKit/kext/KXKextManager.h>
#include <IOKit/kext/kextmanager_types.h>
#include <IOKit/kext/kernel_check.h>
#include <bootfiles.h>
#include "globals.h"
#include "logging.h"
#include "queue.h"
#include "request.h"
#include "watchvol.h"
#include "PTLock.h"
#include "bootcaches.h"
#include "utility.h"

/*******************************************************************************
* Globals set from invocation arguments (XX could use fewer globals :?).
*******************************************************************************/
#define kAppleSetupDonePath     "/var/db/.AppleSetupDone"
#define kKXROMExtensionsFolder  "/System/Library/Caches/com.apple.romextensions/"

const char * progname = "(unknown)";  // don't free
Boolean use_repository_caches = true;
Boolean debug = false;
Boolean load_in_task = false;
Boolean jettison_kernel_linker = true;
int g_verbose_level = kKXKextManagerLogLevelDefault;  // nonzero for -v option
Boolean g_first_boot = false;
Boolean g_safe_boot_mode = false;

static Boolean gStaleMkext = false;
Boolean gStaleBootNotificationNeeded = false;


// options for these are not yet implemented
char * g_kernel_file = NULL;  // don't free
char * g_patch_dir = NULL;    // don't free
char * g_symbol_dir = NULL;   // don't free
Boolean gOverwrite_symbols = true;

/*******************************************************************************
* Globals created at run time.  (XX organize by which threads access them?)
*******************************************************************************/

KXKextManagerRef gKextManager = NULL;                  // released in main
CFRunLoopRef gMainRunLoop = NULL;                      // released in main

// all the following are released in kextd_set_up_server()
CFRunLoopTimerRef gStaleMkextMessageRunLoopTimer = NULL;
CFRunLoopSourceRef gKernelRequestRunLoopSource = NULL; 
CFRunLoopSourceRef gClientRequestRunLoopSource = NULL;
static CFMachPortRef gKextdSignalMachPort = NULL;
static CFRunLoopSourceRef gSignalRunLoopSource = NULL;

#ifndef NO_CFUserNotification
CFRunLoopSourceRef gNotificationQueueRunLoopSource = NULL;     // must release
#endif /* NO_CFUserNotification */

const char * default_kernel_file = NULL;

queue_head_t g_request_queue;
PTLockRef gKernelRequestQueueLock = NULL;
PTLockRef gKernSymfileDataLock = NULL;

static CFRunLoopTimerRef sDiskArbWaiter = NULL;     // delay diskarb/watchvol

/*******************************************************************************
* Function prototypes.
*******************************************************************************/

static Boolean is_safe_boot(void);
static Boolean kextd_prepare_link_kernel();
static void kext_send_finished(void);
static Boolean is_netboot(void);
static Boolean kextd_set_up_server(void);
static void try_diskarb(CFRunLoopTimerRef timer, void *ctx);
void kextd_register_signals(void);

static Boolean kextd_find_rom_mkexts(void);
static void check_extensions_mkext(void);
static Boolean kextd_download_personalities(void);

static void usage(int level);

void kextd_handle_signal(int signum);
void kextd_runloop_signal(
    CFMachPortRef port,
        void * msg,
        CFIndex size,
        void * info);
void kextd_rescan(void);

void kextd_log_stale_mkext(CFRunLoopTimerRef timer, void *info);

/*******************************************************************************
*******************************************************************************/

int main (int argc, const char * argv[])
{
    int exit_status = 0;
    KXKextManagerError result = kKXKextManagerErrorNone;
    int optchar;
    CFIndex count, i, rom_repository_idx = -1;
    Boolean have_rom_mkexts = false;
    struct stat stat_buf;

    CFMutableArrayRef repositoryDirectories = NULL;  // -f; must free

//sleep(10);    // enable to debug under launchd until 4438161 is fixed

   /*****
    * Find out what my name is.
    */
    progname = rindex(argv[0], '/');
    if (progname) {
        progname++;   // go past the '/'
    } else {
        progname = (char *)argv[0];
    }

   /*****
    * Allocate CF collection objects.
    */
    repositoryDirectories = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!repositoryDirectories) {
        fprintf(stderr, "%s: memory allocation failure\n", progname);
        exit_status = 1;
        goto finish;
    }

    gKextloadedKextPaths = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!gKextloadedKextPaths) {
        fprintf(stderr, "%s: memory allocation failure\n", progname);
        exit_status = 1;
        goto finish;
    }

#ifndef NO_CFUserNotification

    gPendedNonsecureKextPaths = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!gPendedNonsecureKextPaths) {
        fprintf(stderr, "%s: memory allocation failure\n", progname);
        exit_status = 1;
        goto finish;
    }

    gNotifiedNonsecureKextPaths = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!gNotifiedNonsecureKextPaths) {
        fprintf(stderr, "%s: memory allocation failure\n", progname);
        exit_status = 1;
        goto finish;
    }

#endif /* NO_CFUserNotification */

    /*****
    * Process command line arguments.
    */
    while ((optchar = getopt(argc, (char * const *)argv, "bcdfhjf:r:vx")) !=
        -1) {

        CFStringRef optArg = NULL;    // must release

        switch (optchar) {
          case 'b':
            fprintf(stderr, "%s: -b is unused; ignoring", progname);
            break;
          case 'c':
            use_repository_caches = false;
            break;
          case 'd':
            debug = true;
            break;
          case 'f':
            load_in_task = true;
            break;
          case 'h':
            usage(1);
            exit_status = 1;
            goto finish;
            break;
          case 'j':
            jettison_kernel_linker = false;
            break;
          case 'r':
            if (!optarg) {
                kextd_error_log("%s: no argument for -f", progname);
                usage(0);
                exit_status = 1;
                goto finish;
            }
            optArg = CFStringCreateWithCString(kCFAllocatorDefault,
               optarg, kCFStringEncodingUTF8);
            if (!optArg) {
                fprintf(stderr, "%s: memory allocation failure\n", progname);
                exit_status = 1;
                goto finish;
            }
            CFArrayAppendValue(repositoryDirectories, optArg);
            CFRelease(optArg);
            optArg = NULL;
            break;
          case 'v':
            {
                const char * next;

                if (optind >= argc) {
                    g_verbose_level = kKXKextManagerLogLevelBasic;
                } else {
                    next = argv[optind];
                    if ((next[0] == '1' || next[0] == '2' || next[0] == '3' ||
                         next[0] == '4' || next[0] == '5' || next[0] == '6') &&
                         next[1] == '\0') {

                        g_verbose_level = atoi(next);
                        optind++;
                    } else if (next[0] == '-') {
                        g_verbose_level = kKXKextManagerLogLevelBasic;
                    } else if (optind < (argc - 1)) {
                        fprintf(stderr,"%s: invalid argument to -v option",
                            progname);
                        usage(0);
                        exit_status = 1;
                        goto finish;
                    } else {
                        g_verbose_level = kKXKextManagerLogLevelBasic;
                    }
                }
            }
            break;
          case 'x':
            g_safe_boot_mode = true;
            use_repository_caches = false;  // -x implies -c
            break;
          default:
            usage(0);
            exit_status = 1;
            goto finish;
        }
    }

   /* Update argc, argv for building dependency lists.
    */
    argc -= optind;
    argv += optind;

    if (argc != 0) {
        usage(0);
        exit_status = 1;
        goto finish;
    }

   /*****
    * If not running in debug mode, then hook up to syslog.
    */
    if (!debug) {
        kextd_openlog("kextd");  // should that arg be progname?
    }

   /*****
    * Do some checks (XX roll into check_extensions_mkext()?):
    * - note whether in-memory mkext differs from the one on the root
    * - if safeboot, force a rebuild (if not in near future, at next reboot)
    * 
    * launchd / kextcache -U have already done any obvious updates
    *
    * We can't just disable loading outside of boot!=root scenarios.  Not
    * only might kernel developers boot from custom mkexts, but to netboot
    * an image of a DVD, the TFTP's mkext will need network drivers that
    * aren't going to be in the mkext on the DVD.
    *
    * For now, gStaleMkext only left active for boot!=root.
    * is_netboot() check should stay even if we change boot!=root policy.
    */
    if (is_bootroot_active() && !is_netboot()) {
        gStaleMkext  = bootedFromDifferentMkext();
        if (gStaleMkext)
            kextd_log("WARNING: mkext on root filesystem does not match "
                "the one used for startup");
    }

    // check kernel for safe boot, -x only for debugging now
    if (is_safe_boot()) {
        kextd_log("safe boot detected");
        g_safe_boot_mode = true;
        use_repository_caches = false;  // never use caches in safe boot
    }

    if (g_safe_boot_mode) {
        kextd_log("safe boot; invalidating extensions caches");
        utimes("/System/Library/Extensions", NULL);
    }

    // g_first_boot initialized to false
    if (stat(kAppleSetupDonePath, &stat_buf) == -1 && errno == ENOENT) {
        g_first_boot = true;
    } 

   /*****
    * Check Extensions.mkext needs/ed a rebuild (BootRoot depending)
    */
    check_extensions_mkext();    

    // XX Extensions vs. mkext times being off should also cancel kext loading.
    // launchd shouldn't have let it come to that.  check_extensions_mkext()
    // sets gStaleMkext if we're booted via bootroot and the mkext is stale
    if (gStaleMkext) {
        // kextd_log_stale_mkext(NULL, NULL);   // timer fires right away
        gStaleBootNotificationNeeded = true;
    }

   /*****
    * Jettison the kernel linker if required, and if the startup mkext &
    * kernel aren't stale (system may end up in a weird state).
    */
    // FIXME: Need a way to make this synchronous!
    if (jettison_kernel_linker && !gStaleMkext) {
        kern_return_t kern_result;
        kern_result = IOCatalogueSendData(kIOMasterPortDefault,
            kIOCatalogRemoveKernelLinker, 0, 0);
        if (kern_result != KERN_SUCCESS) {
            kextd_error_log(
                "couldn't remove linker from kernel; error %d "
                "(may have been removed already)", kern_result);
            // this is only serious the first time kextd launches....
            // FIXME: how exactly should we handle this? Create a separate
            // FIXME: ... program to trigger KLD unload?
            // FIXME: should kextd exit in this case?
        }

        have_rom_mkexts = kextd_find_rom_mkexts();
    }

   /*****
    * Make sure we scan the standard Extensions folder.
    */
    CFArrayInsertValueAtIndex(repositoryDirectories, 0,
        kKXSystemExtensionsFolder);

   /*****
    * Make sure we scan the ROM Extensions folder.
    */
    if (have_rom_mkexts) {
    rom_repository_idx = 1;
    CFArrayInsertValueAtIndex(repositoryDirectories, rom_repository_idx,
        CFSTR(kKXROMExtensionsFolder));
    }

   /*****
    * If we're not replacing the in-kernel linker, we're done.
    */
    if (!jettison_kernel_linker) {
        kext_send_finished();
        goto finish;
    }

   /*****
    * If we are booting with stale data, don't bother setting up the
    * KXKextManager. 
    * XXX revisit since kmod_control loop needs manager plus we don't
    * want to skip updating the mkext in the next clause.
    * Or maybe we shouldn't call kmod_control at all when stale?
    * XXX bootroot system hangs at boot w/stale even without this goto.  :P
    * (launchctl -> kextcache -U should be fixing and rebooting in that case)
    if (gStaleMkext) {
        goto server_start;
    }
    */

   /*****
    * Set up the kext manager.
    */
    gKextManager = KXKextManagerCreate(kCFAllocatorDefault);
    if (!gKextManager) {
        kextd_error_log("can't allocate kext manager");
        exit_status = 1;
        goto finish;
    }

    result = KXKextManagerInit(gKextManager,
        false, // don't load in task; fork and wait
        g_safe_boot_mode);
    if (result != kKXKextManagerErrorNone) {
        kextd_error_log("can't initialize manager (%s)",
            KXKextManagerErrorStaticCStringForError(result));
        exit_status = 1;
        goto finish;
    }

    KXKextManagerSetPerformLoadsInTask(gKextManager, load_in_task);
    KXKextManagerSetPerformsStrictAuthentication(gKextManager, true);
    KXKextManagerSetPerformsFullTests(gKextManager, false);
    KXKextManagerSetLogLevel(gKextManager, g_verbose_level);
    KXKextManagerSetLogFunction(gKextManager, kextd_log);
    KXKextManagerSetErrorLogFunction(gKextManager, kextd_error_log);
    KXKextManagerSetWillUpdateCatalog(gKextManager, true);

   /*****
    * Disable clearing of relationships until we're done putting everything
    * together.
    */
    KXKextManagerDisableClearRelationships(gKextManager);

    // FIXME: put the code between the disable/enable into a function

   /*****
    * Add the extensions folders specified with -f to the manager.
    */
    count = CFArrayGetCount(repositoryDirectories);
    for (i = 0; i < count; i++) {
        CFStringRef directory = (CFStringRef)CFArrayGetValueAtIndex(
            repositoryDirectories, i);
        CFURLRef directoryURL =
            CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                directory, kCFURLPOSIXPathStyle, true);
        if (!directoryURL) {
            kextd_error_log("memory allocation failure");
            exit_status = 1;

            goto finish;
        }

        result = KXKextManagerAddRepositoryDirectory(gKextManager,
            directoryURL, true /* scanForAdditions */,
            (use_repository_caches && (i != rom_repository_idx)),
        NULL);
        if (result != kKXKextManagerErrorNone) {
            kextd_error_log("can't add repository (%s).",
                KXKextManagerErrorStaticCStringForError(result));
        }
        CFRelease(directoryURL);
        directoryURL = NULL;
    }

    CFRelease(repositoryDirectories);
    repositoryDirectories = NULL;

    KXKextManagerEnableClearRelationships(gKextManager);

// server_start:    // unneeded?

    // Create CFRunLoop & sources
    if (!kextd_set_up_server()) {
        // kextd_set_up_server() logged an error message
        exit_status = 1;
        goto finish;
    }

    // Register signal handlers
    // PR-5466716: The signal handler use a mach port that is initialized in
    //             kextd_set_up_server(), so register signals after that call
    kextd_register_signals();

    // Spawn kernel monitor thread
    if (!kextd_launch_kernel_request_thread()) {
        // kextd_launch_kernel_request_thread() logged an error message
        exit_status = 1;
        goto finish;
    }

   /*****
    * If our startup mkext and kernel are ok, then send the kext personalities
    * down to the kernel to trigger matching.
    */
    if (!gStaleMkext) {
        if (!kextd_download_personalities()) {
            // kextd_download_personalities() logged an error message
            exit_status = 1;
            goto finish;
        }
    }

    if (!kextd_prepare_link_kernel()) {
        goto finish;
    }

    kext_send_finished();

    // Start run loop
    CFRunLoopRun();

finish:
    kextd_stop_volwatch();    // no-op if watch_volumes not called
    if (gKextManager)                  CFRelease(gKextManager);
    if (gMainRunLoop)                  CFRelease(gMainRunLoop);

    if (gKextloadedKextPaths)          CFRelease(gKextloadedKextPaths);
#ifndef NO_CFUserNotification
    if (gPendedNonsecureKextPaths)     CFRelease(gPendedNonsecureKextPaths);
    if (gNotifiedNonsecureKextPaths)   CFRelease(gNotifiedNonsecureKextPaths);
#endif /* NO_CFUserNotification */

    exit(exit_status);
    return exit_status;
}

/*******************************************************************************
* is_safe_boot()
*
* Tell us whether the kernel says we are safe booted or not.
*******************************************************************************/
#define MIB_LENGTH   (2)

Boolean is_safe_boot(void)
{
    Boolean result = false;
    int     kern_safe_boot;
    size_t  length;
    int     mib_name[MIB_LENGTH] = { CTL_KERN, KERN_SAFEBOOT };

    /* First check the kernel sysctl. */
    length = sizeof(kern_safe_boot);
    if (sysctl(mib_name, MIB_LENGTH, &kern_safe_boot, &length, NULL, 0) == 0) {

        result = kern_safe_boot ? true : false;

    } else {
        kextd_error_log("sysctl failed for KERN_SAFEBOOT");
    }

    return result;
}

/*******************************************************************************
* kextd_write_link_kernel_async()
*******************************************************************************/
void * kextd_write_link_kernel_async(void * arg)
{
    mach_timespec_t  waitTime = { 40, 0 };
    kern_return_t kern_result;
    char * symfile_path = kKernelSymfile;
    int symfile_fd = -1;
    Boolean unlink_symfile = false;
    Boolean free_kernel_linkedit = false;

   /* Run at a low I/O priority.
    */
    setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_THREAD, IOPOL_THROTTLE);

    kern_result = IOKitWaitQuiet(kIOMasterPortDefault, &waitTime);
    if (kern_result == kIOReturnTimeout) {
        kextd_error_log("IOKitWaitQuiet() timed out waiting to write kernel symbols");
    } else if (kern_result != kIOReturnSuccess) {
        kextd_error_log("IOKitWaitQuiet() failed, waiting to write kernel symbols, "
            "with result code %x", kern_result);
    }

   /* Now sleep for 10 seconds to give the BootCache time to finish reading
    * its playlist (? relying on background I/O for the rest ?)
    */
    sleep(10);
    
   /* Write the symbol file if possible.
    */
    symfile_fd = open(symfile_path, O_CREAT|O_EXCL|O_WRONLY, 0644);
    if (symfile_fd == -1) {
        kextd_error_log("failed to open kernel link data file (%s) - %s",
            symfile_path, strerror(errno));
    } else {
        ssize_t total_written = 0;
        ssize_t written = 0;

        kextd_log("writing kernel link data to %s", symfile_path);

        while (total_written < _kload_optimized_kern_sym_length) {
            written = write(symfile_fd, _kload_optimized_kern_sym_data + total_written,
                _kload_optimized_kern_sym_length - total_written);
            if (written == -1) {
                break;
            }
            total_written += written;
        }
        if (total_written == _kload_optimized_kern_sym_length) {
            free_kernel_linkedit = true;
        } else {
            if (g_verbose_level >= kKXKextManagerLogLevelBasic) {
                kextd_log("failed to write link data to %s",
                    symfile_path);
            }
            unlink_symfile = true;
        }
    }

   /* If we couldn't write the whole file, unlink it.
    */
    if (unlink_symfile) {
        unlink(symfile_path);
    }

   /* Whether we wrote the symfile or not, ditch it from kextd so that we fall
    * back to the standard mechanisms for both kextload and kextd.
    */
    PTLockTakeLock(gKernSymfileDataLock);
    vm_deallocate(mach_task_self(),
        (vm_address_t)_kload_optimized_kern_sym_data, _kload_optimized_kern_sym_length);
    _kload_optimized_kern_sym_data = NULL;
    _kload_optimized_kern_sym_length = 0;
    PTLockUnlock(gKernSymfileDataLock);

   /* If we wrote usable symbols, tell the kernel to free its
    * linkedit segment. We may have already done this in
    * kextd_prepare_link_kernel(), but we may not have!
    */
    if (free_kernel_linkedit) {
        mach_port_t host_port = MACH_PORT_NULL;
        void * data = 0;
        mach_msg_type_number_t data_size = 0;

        if (!unlink_symfile && g_verbose_level >= kKXKextManagerLogLevelBasic) {
            kextd_log("using %s for kernel link symbols", symfile_path);
        }

        host_port = mach_host_self(); /* must be privileged to work */
        if (!MACH_PORT_VALID(host_port)) {
            kextd_error_log("can't get host port to communicate with kernel");
            goto finish;
        }

       /* We don't care about the return value here.
        */
        kmod_control(host_port, 0, KMOD_CNTL_FREE_LINKEDIT_DATA, &data,
            &data_size);

        mach_port_deallocate(mach_task_self(), host_port);
    }

finish:
    if (symfile_fd != -1) {
        close(symfile_fd);
    }

    pthread_exit(NULL);
    return NULL;
}

/*******************************************************************************
* kextd_write_link_kernel_async()
*
* Returns false if a nonrecoverable error has occurred, true otherwise. We
* may be unable to link kexts if we couldn't locate kernel link symbols, but
* kextd will be running to service other requests.
*******************************************************************************/
static Boolean kextd_prepare_link_kernel()
{
    Boolean result = false;
    mach_port_t host_port = MACH_PORT_NULL;
    char * which_kernel = NULL;
    struct statfs root_statfs_buf;
    kern_return_t kern_result;
    int compareResult;
    Boolean free_kernel_linkedit = false;

    host_port = mach_host_self(); /* must be privileged to work */
    if (!MACH_PORT_VALID(host_port)) {
        kextd_error_log("can't get host port to communicate with kernel");
        goto finish;
    }

    gKernSymfileDataLock = PTLockCreate();
    if (!gKernSymfileDataLock) {
        kextd_error_log("failed to create kernel symbol data lock");
        goto finish;
    }

   /*****
    * First check if the root filesystem is read-only. If it is, we can't
    * generate symbols and will end up falling back to /mach_kernel or the
    * running kernel.
    */
    if (statfs("/", &root_statfs_buf) != 0) {
        kextd_error_log("can't stat root filesystem");
        // not a hard failure
    } else if (root_statfs_buf.f_flags & MNT_RDONLY) {
        kextd_error_log("root filesystem is read-only; "
            "skipping kernel link data generation");
    } else {

       /* Direct from the kernel if available, and write to disk.
        */
        if (g_verbose_level >= kKXKextManagerLogLevelBasic) {
            kextd_log("requesting link data from running kernel");
        }
        PTLockTakeLock(gKernSymfileDataLock);
        kern_result = kmod_control(host_port, 0, KMOD_CNTL_GET_KERNEL_SYMBOLS,
            (kmod_args_t *)&_kload_optimized_kern_sym_data, &_kload_optimized_kern_sym_length);
        PTLockUnlock(gKernSymfileDataLock);
        if (kern_result == KERN_SUCCESS && _kload_optimized_kern_sym_length > 0) {
            pthread_attr_t thread_attr;
            pthread_t      thread;

            pthread_attr_init(&thread_attr);
            pthread_create(&thread,
                &thread_attr,
                kextd_write_link_kernel_async, NULL);
            pthread_detach(thread);

            which_kernel = "(running kernel)";

        } else if (kern_result == KERN_MEMORY_FAILURE) {
           /* KERN_MEMORY_FAILURE means the kernel has explicitly
            * dropped its link data.
            */
            kextd_error_log("kernel freed its link data; "
                "trying on-disk kernel file (%s)", kDefaultKernel);
        } else {
            kextd_error_log("can't get link data from running kernel");
        }
    }

   /* If we couldn't write a symbol file, check /mach_kernel to see if we
    * can use that.
    */
    if (g_verbose_level >= kKXKextManagerLogLevelBasic) {
        kextd_log("checking %s for UUID match and kernel link symbols",
            kDefaultKernel);
    }

    compareResult = machoUUIDsMatch(host_port, NULL /* running kernel */,
        kDefaultKernel);

    if (1 == compareResult) {
        if (!which_kernel) {
            which_kernel = kDefaultKernel;
        }
        free_kernel_linkedit = true;
    } else if (0 == compareResult) {
        kextd_error_log("notice - UUID of on-disk kernel (%s) "
            "does not match running kernel", kDefaultKernel);
    } else {
        kextd_error_log("error - can't compare running/on-disk kernel UUIDs");
    }

   /* If we found usable symbols on disk, tell the kernel to free its
    * linkedit segment.
    */
    if (free_kernel_linkedit) {
        void * data = 0;
        mach_msg_type_number_t data_size = 0;

       /* We don't care about the return value here.
        */
        kmod_control(host_port, 0, KMOD_CNTL_FREE_LINKEDIT_DATA, &data,
            &data_size);
    }
    
    result = true;
finish:

   /* Dispose of the host port to prevent security breaches and port
    * leaks. We don't care about the kern_return_t value of this
    * call for now as there's nothing we can do if it fails.
    */
    if (MACH_PORT_NULL != host_port) {
        mach_port_deallocate(mach_task_self(), host_port);
        host_port = MACH_PORT_NULL;
    }

    if (which_kernel && g_verbose_level >= kKXKextManagerLogLevelBasic) {
        kextd_log("using %s for kernel link symbols", which_kernel);
    }

    return result;
}

/*******************************************************************************
*
*******************************************************************************/

#define TEMP_FILE        "/tmp/com.apple.iokit.kextd.XXXXXX"
#define MKEXTUNPACK_COMMAND    "/usr/sbin/mkextunpack "    \
                "-d "kKXROMExtensionsFolder" "
#define TEMP_FILE_PERMS         (0644)

static kern_return_t process_mkext(const UInt8 * bytes, CFIndex length)
{
    kern_return_t    err;
    char        temp_file[1 + strlen(TEMP_FILE)];
    char        mkextunpack_cmd[1 + strlen(TEMP_FILE) + strlen(MKEXTUNPACK_COMMAND)];
    const char *    rom_ext_dir = kKXROMExtensionsFolder;
    int         outfd = -1;
    struct stat         stat_buf;
    mode_t              real_umask;
    
    strcpy(temp_file, TEMP_FILE);

    outfd = mkstemp(temp_file);
    if (-1 == outfd) {
        kextd_error_log("can't create %s - %s", temp_file,
            strerror(errno));
        err = kKXKextManagerErrorFileAccess;
        goto finish;
    }

   /* Set the umask to get it, then set it back to iself. Wish there were a
    * better way to query it.
    */
    real_umask = umask(0);
    umask(real_umask);

    if (-1 == fchmod(outfd, TEMP_FILE_PERMS & ~real_umask)) {
        kextd_error_log("unable to chmod temp file %s: %s", temp_file,
            strerror(errno));
    }

    if (length != write(outfd, bytes, length))
        err = kKXKextManagerErrorDiskFull;
    else
        err = kKXKextManagerErrorNone;

    if (kKXKextManagerErrorNone != err) {
        kextd_error_log("couldn't write output");
        goto finish;
    }

    close(outfd);
    outfd = -1;

    if (-1 == stat(rom_ext_dir, &stat_buf))
    {
        if (0 != mkdir(rom_ext_dir, 0755)) {
            kextd_error_log("mkdir(%s) failed: %s", rom_ext_dir, strerror(errno));
            err = kKXKextManagerErrorFileAccess;
            goto finish;
        }
    }

    strcpy(mkextunpack_cmd, MKEXTUNPACK_COMMAND);
    strcat(mkextunpack_cmd, temp_file);

    // kextd_error_log(mkextunpack_cmd);

    if (0 != system(mkextunpack_cmd)) {
        kextd_error_log(mkextunpack_cmd);
        kextd_error_log("failed");
        err = kKXKextManagerErrorChildTask;
        goto finish;
    }

finish:
    if (-1 != outfd) {
        close(outfd);
    }
    unlink(temp_file);

    return err;
}

/*******************************************************************************
*
*******************************************************************************/

static Boolean kextd_find_rom_mkexts(void)
{
    kern_return_t  kr;
    CFSetRef       set = NULL;
    CFDataRef *    mkexts = NULL;
    CFIndex        count, idx;
    char *         propertiesBuffer;
    uint32_t       loaded_bytecount;
    enum         { _kIOCatalogGetROMMkextList = 4  };

    kr = IOCatalogueGetData(MACH_PORT_NULL, _kIOCatalogGetROMMkextList,
                &propertiesBuffer, &loaded_bytecount);
    if (kIOReturnSuccess == kr)
    { 
    set = (CFSetRef)
        IOCFUnserialize(propertiesBuffer, kCFAllocatorDefault, 0, 0);
    vm_deallocate(mach_task_self(), (vm_address_t) propertiesBuffer, loaded_bytecount);
    }
    if (!set)
    return false;

    count  = CFSetGetCount(set);
    if (count)
    {
    mkexts = (CFDataRef *) calloc(count, sizeof(CFDataRef));
    CFSetGetValues(set, (const void **) mkexts);
    for (idx = 0; idx < count; idx++)
    {
        process_mkext(CFDataGetBytePtr(mkexts[idx]), CFDataGetLength(mkexts[idx]));
    }
    free(mkexts);
    }
    CFRelease(set);

    return (count > 0);
}

/*******************************************************************************
*
*******************************************************************************/

static void check_extensions_mkext(void)
{
    struct stat extensions_stat_buf;
    struct stat mkext_stat_buf;
    Boolean outOfDate;

/* BootRoot takes care of this now (4243070)
    const char * kextcache_cmd = "/usr/sbin/kextcache";
    char * const kextcache_argv[] = {
        "kextcache",
        "-elF",
        "-a", "ppc",
        "-a", "i386",
        NULL,
    };
*/

    // should probably move all of this to 
    outOfDate = (0 != stat(kDefaultMkext, &mkext_stat_buf));
    if (!outOfDate && (0 == stat(kSystemExtensionsDir, &extensions_stat_buf)))
    outOfDate = (mkext_stat_buf.st_mtime != (extensions_stat_buf.st_mtime + 1));
    
    if (outOfDate) {
        do {
            // XX arch is hardcoded above; what's this about?
            const NXArchInfo * arch = NXGetLocalArchInfo();
            // int fork_result;

            // 4618030: allow mkext rebuilds on startup if not BootRoot
            // had to fix kextcache to take the lock *after* forking
            // (-F was giving up the lock before the work was done!)
            if (is_bootroot_active()) {
                kextd_error_log("WARNING: mkext unexpectedly out of date w/rt Extensions folder");
                gStaleMkext = true;
                break;  // skip since BootRoot logic in watchvol.c will handle
            }

            if (arch) {
                arch = NXGetArchInfoFromCpuType(arch->cputype, CPU_SUBTYPE_MULTIPLE);
            }
            if (!arch) {
                kextd_error_log("unknown architecture");
                break;
            }

#ifdef NO_4243070
           /* wait:false means the return value is <0 for fork/exec failures and
            * the pid of the forked process if >0.
            */
            fork_result = fork_program(kextcache_cmd, kextcache_argv,
                g_first_boot ? KEXTCACHE_DELAY_FIRST_BOOT : KEXTCACHE_DELAY_STD /* delay */,
                false /* wait */);
            if (fork_result < 0) {
                kextd_error_log("couldn't fork/exec kextcache to update mkext");
            }
#endif  // NO_4243070

        } while (false);
    }
    return;
}

/*******************************************************************************
* is_neboot()
*******************************************************************************/
static Boolean is_netboot(void)
{
    Boolean result = false;
    int netboot_mib_name[] = { CTL_KERN, KERN_NETBOOT };
    int netboot = 0;
    size_t netboot_len = sizeof(netboot);

   /* Get the size of the buffer we need to allocate.
    */
   /* Now actually get the kernel version.
    */
    if (sysctl(netboot_mib_name, sizeof(netboot_mib_name) / sizeof(int),
        &netboot, &netboot_len, NULL, 0) != 0) {

        kextd_error_log("sysctl for netboot failed");
        goto finish;
    }

    result = netboot ? true : false;

finish:
    return result;
}

/*******************************************************************************
* kext_send_finished()
*******************************************************************************/

static void kext_send_finished(void)
{
    kern_return_t kern_result;
    kern_result = IOCatalogueSendData(kIOMasterPortDefault,
        kIOCatalogKextdFinishedLaunching, 0, 0);
    if (kern_result != KERN_SUCCESS) {
        kextd_error_log(
            "couldn't notify kernel of kextd launch; error %d", kern_result);
    }

    return;
}

static void try_diskarb(CFRunLoopTimerRef timer, void *spctx)
{
    static int retries = 0;
    CFIndex priority = (CFIndex)(intptr_t)spctx;
    int result = -1;

    result = kextd_watch_volumes(priority);

    if (result == 0 || ++retries >= kKXDiskArbMaxRetries) {
        CFRunLoopTimerInvalidate(sDiskArbWaiter);   // runloop held last retain
        sDiskArbWaiter = NULL;
    }

    if (result) {
        if (retries > 1) {
            if (retries < kKXDiskArbMaxRetries) {
                kextd_log("diskarb isn't ready yet; we'll try again soon");
            } else {
                kextd_error_log("giving up on diskarb; auto-rebuild disabled");
                (void)kextd_giveup_volwatch();        // logs own errors
            }
        }
    }

}


/*******************************************************************************
* kextd_set_up_server()
*******************************************************************************/
// in mig_server.c (XX whither mig_server.h?)
extern void kextd_mach_port_callback(
    CFMachPortRef port,
    void *msg,
    CFIndex size,
    void *info);

static Boolean kextd_set_up_server(void)
{
    Boolean result = true;
    kern_return_t kern_result = KERN_SUCCESS;
    CFRunLoopSourceContext sourceContext;
    unsigned int sourcePriority = 1;
    CFMachPortRef kextdMachPort = NULL;  // must release
    mach_port_limits_t limits;  // queue limit for signal-handler port
    mach_port_t servicePort;
    CFRunLoopTimerContext spctx = { 0, };   // to pass along source priority
    CFAbsoluteTime diskArbDelay;

    kern_result = bootstrap_check_in(bootstrap_port,
            KEXTD_SERVER_NAME, &servicePort);

    if (kern_result != BOOTSTRAP_SUCCESS) {
        kextd_error_log("bootstrap_check_in(): %s", bootstrap_strerror(kern_result));
        exit(EX_OSERR);
    }

    gMainRunLoop = CFRunLoopGetCurrent();
    if (!gMainRunLoop) {
       kextd_error_log("couldn't create run loop");
        result = false;
        goto finish;
    }

    bzero(&sourceContext, sizeof(CFRunLoopSourceContext));
    sourceContext.version = 0;

   /*****
    * Add the runloop sources in decreasing priority (increasing "order").
    * Signals are handled first, followed by kernel requests, and then by
    * client requests. It's important that each source have a distinct
    * priority; sharing them causes unpredictable behavior with the runloop.
    * Note: CFRunLoop.h says 'order' should generally be 0 for all.
    */

    sourceContext.perform = kextd_handle_kernel_request;
    gKernelRequestRunLoopSource = CFRunLoopSourceCreate(kCFAllocatorDefault,
        sourcePriority++, &sourceContext);
    if (!gKernelRequestRunLoopSource) {
       kextd_error_log("couldn't create kernel request run loop source");
        result = false;
        goto finish;
    }
    CFRunLoopAddSource(gMainRunLoop, gKernelRequestRunLoopSource,
        kCFRunLoopDefaultMode);

    kextdMachPort = CFMachPortCreateWithPort(kCFAllocatorDefault,
        servicePort, kextd_mach_port_callback, NULL, NULL);
    gClientRequestRunLoopSource = CFMachPortCreateRunLoopSource(
        kCFAllocatorDefault, kextdMachPort, sourcePriority++);
    if (!gClientRequestRunLoopSource) {
       kextd_error_log("couldn't create client request run loop source");
        result = false;
        goto finish;
    }
    CFRunLoopAddSource(gMainRunLoop, gClientRequestRunLoopSource,
        kCFRunLoopDefaultMode);

    // don't talk to diskarb until five minutes after first boot
    // XX should move this delay so it prevents auto-updates, not diskarb
    diskArbDelay =g_first_boot ? KEXTCACHE_DELAY_FIRST_BOOT:KEXTCACHE_DELAY_STD;
    spctx.info = (void*)(intptr_t)sourcePriority++;
    sDiskArbWaiter = CFRunLoopTimerCreate(nil, CFAbsoluteTimeGetCurrent() + 
        diskArbDelay,diskArbDelay,0,sourcePriority++,try_diskarb,&spctx);
    if (!sDiskArbWaiter) {
        result = false;
        goto finish;
    }
    CFRunLoopAddTimer(gMainRunLoop, sDiskArbWaiter, kCFRunLoopDefaultMode);
    CFRelease(sDiskArbWaiter);        // later invalidation will free

    gKextdSignalMachPort = CFMachPortCreate(kCFAllocatorDefault,
        kextd_runloop_signal, NULL, NULL);
    limits.mpl_qlimit = 1;
    kern_result = mach_port_set_attributes(mach_task_self(),
        CFMachPortGetPort(gKextdSignalMachPort),
        MACH_PORT_LIMITS_INFO,
        (mach_port_info_t)&limits,
        MACH_PORT_LIMITS_INFO_COUNT);
    if (kern_result != KERN_SUCCESS) {
        kextd_error_log("failed to set signal-handling port limits");
    }
    gSignalRunLoopSource = CFMachPortCreateRunLoopSource(
        kCFAllocatorDefault, gKextdSignalMachPort, sourcePriority++);
    if (!gSignalRunLoopSource) {
    kextd_error_log("couldn't create signal-handling run loop source");
        result = false;
        goto finish;
    }
    CFRunLoopAddSource(gMainRunLoop, gSignalRunLoopSource,
        kCFRunLoopDefaultMode);

#ifndef NO_CFUserNotification

    sourceContext.perform = kextd_check_notification_queue;
    gNotificationQueueRunLoopSource = CFRunLoopSourceCreate(kCFAllocatorDefault,
        sourcePriority++, &sourceContext);
    if (!gNotificationQueueRunLoopSource) {
       kextd_error_log("couldn't create alert run loop source");
        result = false;
        goto finish;
    }
    CFRunLoopAddSource(gMainRunLoop, gNotificationQueueRunLoopSource,
        kCFRunLoopDefaultMode);

#endif /* NO_CFUserNotification */

    if (gStaleMkext) {
        gStaleMkextMessageRunLoopTimer=CFRunLoopTimerCreate(kCFAllocatorDefault,
            0, 30.0 /* seconds */, 0, 0, &kextd_log_stale_mkext, NULL);
        if (!gStaleMkextMessageRunLoopTimer) {
           kextd_error_log("couldn't create kernel stale message run loop timer");
            result = false;
            goto finish;
        }
        CFRunLoopAddTimer(gMainRunLoop, gStaleMkextMessageRunLoopTimer,
            kCFRunLoopDefaultMode);
    }

   /* Watch for RAID changes so we can forcibly update their boot partitions.
    */
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(),
        NULL, // const void *observer
        updateRAIDSet,
        CFSTR(kAppleRAIDNotificationSetChanged),
        NULL, // const void *object
        CFNotificationSuspensionBehaviorHold);
    kern_result = AppleRAIDEnableNotifications();
    if (kern_result != KERN_SUCCESS) {
        kextd_error_log("couldn't register for RAID notifications");
    }

finish:
    if (gStaleMkextMessageRunLoopTimer)  CFRelease(gStaleMkextMessageRunLoopTimer);
    if (gKernelRequestRunLoopSource)  CFRelease(gKernelRequestRunLoopSource);
    if (gClientRequestRunLoopSource)  CFRelease(gClientRequestRunLoopSource);
    if (gKextdSignalMachPort)         CFRelease(gKextdSignalMachPort);
    if (gSignalRunLoopSource)         CFRelease(gSignalRunLoopSource);
#ifndef NO_CFUserNotification
    if (gNotificationQueueRunLoopSource) CFRelease(gNotificationQueueRunLoopSource);
#endif /* NO_CFUserNotification */
    if (kextdMachPort)                CFRelease(kextdMachPort);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
void kextd_register_signals(void)
{
    signal(SIGHUP, kextd_handle_signal);
    signal(SIGTERM, kextd_handle_signal);
    return;
}

/*******************************************************************************
* On receiving a SIGHUP, the daemon sends a Mach message to the signal port,
* causing the run loop handler function kextd_rescan() to be
* called on the main thread.
*******************************************************************************/
typedef struct {
    mach_msg_header_t header;
    int signum;
} kextd_mach_msg_signal_t;
    
void kextd_handle_signal(int signum)
{
    kextd_mach_msg_signal_t msg;
    mach_msg_option_t options;
    kern_return_t kern_result;

    if (signum != SIGHUP && signum != SIGTERM) {
        return;
    }

    msg.signum = signum;

    msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    msg.header.msgh_size = sizeof(msg.header);
    msg.header.msgh_remote_port = CFMachPortGetPort(gKextdSignalMachPort);
    msg.header.msgh_local_port = MACH_PORT_NULL;
    msg.header.msgh_id = 0;
    options = MACH_SEND_TIMEOUT;
    kern_result = mach_msg(&msg.header,  /* msg */
        MACH_SEND_MSG | options,    /* options */
        sizeof(msg),                /* send_size */
        0,                          /* rcv_size */
        MACH_PORT_NULL,             /* rcv_name */
        0,                          /* timeout */
        MACH_PORT_NULL);            /* notify */
    return;
}

void kextd_clear_all_notifications(void);
/*******************************************************************************
*******************************************************************************/
void kextd_rescan(void)
{
#ifndef NO_CFUserNotification
    kextd_clear_all_notifications();
#endif /* NO_CFUserNotification */

    KXKextManagerResetAllRepositories(gKextManager);

    // need to trigger check_rebuild (in watchvol.c) for mkext, etc
    // perhaps via mach message to the notification port
    // should we let it handle the ResetAllRepos?

    // FIXME: Should we exit if this fails?
    kextd_download_personalities();
}

/*******************************************************************************
*******************************************************************************/
void kextd_runloop_signal(CFMachPortRef port, void * msg, CFIndex size, void * info)
{
    kextd_mach_msg_signal_t * signal_msg = (kextd_mach_msg_signal_t *)msg;
    int signum = signal_msg->signum;
    
    if (signum == SIGHUP) {
        kextd_rescan();
    } else if (signum == SIGTERM) {
        CFRunLoopStop(CFRunLoopGetCurrent());
    }
    return;
}

#ifndef NO_CFUserNotification
/*******************************************************************************
*
*******************************************************************************/
void kextd_clear_all_notifications(void)
{
    CFArrayRemoveAllValues(gPendedNonsecureKextPaths);

   /* Release any reference to the current user notification.
    */
    if (gCurrentNotification) {
        CFUserNotificationCancel(gCurrentNotification);
        CFRelease(gCurrentNotification);
        gCurrentNotification = NULL;
    }

    if (gCurrentNotificationRunLoopSource) {
        CFRunLoopRemoveSource(gMainRunLoop, gCurrentNotificationRunLoopSource,
            kCFRunLoopDefaultMode);
        CFRelease(gCurrentNotificationRunLoopSource);
        gCurrentNotificationRunLoopSource = NULL;
    }

   /* Clear the record of which kexts the user has been told are insecure.
    * They'll get all the same warnings upon logging in again.
    */
    CFDictionaryRemoveAllValues(gNotifiedNonsecureKextPaths);

    return;
}
#else
#define kextd_clear_all_notifications() do { } while(0)
#endif /* NO_CFUserNotification */


/*******************************************************************************
*
*******************************************************************************/
void kextd_log_stale_mkext(
   CFRunLoopTimerRef timer, 
   void *info
)
{
    kextd_error_log("in-memory mkext doesn't match on-disk mkext; "
                    "non-boot kexts won't be loaded");
    return;
}


/******************************************************************************
* is_bootroot_active() checks for the booter hint
******************************************************************************/
bool is_bootroot_active(void)
{
    int result = false;
    io_service_t chosen = 0;     // must IOObjectRelease()
    CFTypeRef bootrootProp = 0;  // must CFRelease()

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
    if (bootrootProp) CFRelease(bootrootProp);
    return result;
}


/*******************************************************************************
*
*******************************************************************************/
static Boolean kextd_download_personalities(void)
{
    KXKextManagerError result;

    result = KXKextManagerSendAllKextPersonalitiesToCatalog(gKextManager);

    return (kKXKextManagerErrorNone == result) ? true : false;
}


/*******************************************************************************
*
*******************************************************************************/

static void usage(int level)
{
    fprintf(stderr,
        "usage: %s [-c] [-d] [-f] [-h] [-j] [-r dir] ... [-v [1-6]] [-x]\n",
        progname);
    if (level > 0) {
        fprintf(stderr, "    -c   don't use repository caches; scan repository folders\n");
        fprintf(stderr, "    -d   run in debug mode (don't fork daemon)\n");
        fprintf(stderr, "    -f   don't fork when loading (for debugging only)\n");
        fprintf(stderr, "    -h   help; print this list\n");
        fprintf(stderr, "    -j   don't jettison kernel linker; "
            "just load NDRVs and exit (for install CD)\n");
        fprintf(stderr, "    -r <dir>  use <dir> in addition to "
            "/System/Library/Extensions\n");
        fprintf(stderr, "    -v   verbose mode\n");
        fprintf(stderr, "    -x   run in safe boot mode.\n");
    }
    return;
}
