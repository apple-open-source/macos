#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPriv.h>  // for _CFRunLoopSetCurrent()
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <IOKit/IOCFURLAccess.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_error.h>
#include <libc.h>
#include <servers/bootstrap.h>
#include <sysexits.h>

#include <IOKit/kext/KXKextManager.h>
#include "globals.h"
#include "request.h"
#include "logging.h"
#include "queue.h"
#include "PTLock.h"
#include "paths.h"

/*******************************************************************************
* Globals set from invocation arguments.
*******************************************************************************/

static const char * KEXTD_SERVER_NAME = "Kernel Extension Server";

char * progname = "(unknown)";  // don't free
Boolean use_repository_caches = true;
Boolean debug = false;
Boolean load_in_task = false;
Boolean jettison_kernel_linker = true;
int g_verbose_level = 0;        // nonzero for -v option
Boolean safe_boot_mode = false;

// options for these are not yet implemented
char * g_kernel_file = NULL;  // don't free
char * g_patch_dir = NULL;    // don't free
char * g_symbol_dir = NULL;   // don't free
Boolean gOverwrite_symbols = true;

/*******************************************************************************
* Globals created at run time.
*******************************************************************************/

mach_port_t g_io_master_port;

KXKextManagerRef gKextManager = NULL;                  // must release
CFRunLoopRef gMainRunLoop = NULL;                      // must release
CFRunLoopSourceRef gRescanRunLoopSource = NULL;        // must release
CFRunLoopSourceRef gKernelRequestRunLoopSource = NULL; // must release
CFRunLoopSourceRef gClientRequestRunLoopSource = NULL; // must release
#ifndef NO_CFUserNotification
CFRunLoopSourceRef gNonsecureKextRunLoopSource = NULL;     // must release
#endif /* NO_CFUserNotification */

const char * default_kernel_file = "/mach";

queue_head_t g_request_queue;
PTLockRef gKernelRequestQueueLock = NULL;
PTLockRef gRunLoopSourceLock = NULL;

/*******************************************************************************
* Function prototypes.
*******************************************************************************/

static Boolean kextd_is_running(mach_port_t * bootstrap_port_ref);
static int kextd_get_mach_ports(void);
static int kextd_fork(void);
static Boolean kextd_set_up_server(void);
static void kextd_release_parent_task(void);

void kextd_register_signals(void);
void kextd_handle_sigterm(int signum);
void kextd_handle_sighup(int signum);
void kextd_handle_sighup_in_runloop(void * info);

static Boolean kextd_download_personalities(void);

static Boolean kextd_process_ndrvs(CFArrayRef repositoryDirectories);

static void usage(int level);

char * CFURLCopyCString(CFURLRef anURL);

__private_extern__ 
void IOLoadPEFsFromURL( CFURLRef url, io_service_t service );

/*******************************************************************************
*******************************************************************************/

int main (int argc, const char * argv[]) {
    int exit_status = 0;
    KXKextManagerError result = kKXKextManagerErrorNone;
    int optchar;
    CFIndex count, i;

    CFMutableArrayRef repositoryDirectories = NULL;  // -f; must free


   /*****
    * Find out what my name is.
    */
    progname = rindex(argv[0], '/');
    if (progname) {
        progname++;   // go past the '/'
    } else {
        progname = (char *)argv[0];
    }

    if (kextd_is_running(NULL)) {
        // kextd_is_running() printed an error message
        exit_status = EX_UNAVAILABLE;
        goto finish;
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

#ifndef NO_CFUserNotification

    gPendedNonsecureKexts = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!gPendedNonsecureKexts) {
        fprintf(stderr, "%s: memory allocation failure\n", progname);
        exit_status = 1;
        goto finish;
    }

    gPendedKextloadOperations = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!gPendedKextloadOperations) {
        fprintf(stderr, "%s: memory allocation failure\n", progname);
        exit_status = 1;
        goto finish;
    }

    gScheduledNonsecureKexts = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!gScheduledNonsecureKexts) {
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
               optarg, kCFStringEncodingMacRoman);
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
                    g_verbose_level = 1;
                } else {
                    next = argv[optind];
                    if ((next[0] == '1' || next[0] == '2' || next[0] == '3' ||
                         next[0] == '4' || next[0] == '5' || next[0] == '6') &&
                         next[1] == '\0') {

                        g_verbose_level = atoi(next);
                        optind++;
                    } else if (next[0] == '-') {
                        g_verbose_level = 1;
                    } else if (optind < (argc - 1)) {
                        fprintf(stderr,"%s: invalid argument to -v option",
                            progname);
                        usage(0);
                        exit_status = 1;
                        goto finish;
                    } else {
                        g_verbose_level = 1;
                    }
                }
            }
            break;
          case 'x':
            safe_boot_mode = true;
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

    // Register/get Mach ports for the parent process
    if (!kextd_get_mach_ports()) {
        // kextd_get_mach_ports() logged the error message
        exit_status = 1;
        goto finish;
    }

   /*****
    * If not running in debug mode, then fork and hook up to the syslog
    * facility. Note well: a fork, if done, must be done before setting
    * anything else up. Mach ports and other things do not transfer
    * to the child task.
    */
    if (!debug && jettison_kernel_linker) {
        // Fork daemon process
        if (!kextd_fork()) {
            // kextd_fork() logged the error message
            exit_status = 1;
            goto finish;
        }
        // Hook up to syslogd
        kextd_openlog("kextd");  // should that arg be progname?
    }

    // Register signal handlers
    kextd_register_signals();

    // Jettison kernel linker
    // FIXME: Need a way to make this synchronous!
    if (jettison_kernel_linker) {
        kern_return_t kern_result;
        kern_result = IOCatalogueSendData(g_io_master_port,
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
    } 

   /*****
    * Make sure we scan the standard Extensions folder.
    */
    CFArrayInsertValueAtIndex(repositoryDirectories, 0,
        kKXSystemExtensionsFolder);

   /*****
    * Send those NDRVs down to the kernel.
    */
    if (!kextd_process_ndrvs(repositoryDirectories)) {
        // kextd_process_ndrvs() logged an error message
        exit_status = 1;
        goto finish;
    }

   /*****
    * If we're not replacing the in-kernel linker, we're done.
    */
    if (!jettison_kernel_linker) {
        goto finish;
    }

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
        safe_boot_mode);
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
            use_repository_caches, NULL);
        if (result != kKXKextManagerErrorNone) {
            kextd_error_log("can't add repository (%s).",
                KXKextManagerErrorStaticCStringForError(result));
            exit_status = 1;
            goto finish;
        }
        CFRelease(directoryURL);
        directoryURL = NULL;
    }

    KXKextManagerEnableClearRelationships(gKextManager);

    // Create CFRunLoop & sources
    if (!kextd_set_up_server()) {
        // kextd_set_up_server() logged an error message
        exit_status = 1;
        goto finish;
    }

    // Spawn kernel monitor thread
    if (!kextd_launch_kernel_request_thread()) {
        // kextd_launch_kernel_request_thread() logged an error message
        exit_status = 1;
        goto finish;
    }

    if (!kextd_download_personalities()) {
        // kextd_download_personalities() logged an error message
        exit_status = 1;
        goto finish;
    }

    // Allow parent of forked daemon to exit
    if (!debug) {
        kextd_release_parent_task();
    }

    // Start run loop
    CFRunLoopRun();

finish:
    if (gKextManager)                 CFRelease(gKextManager);
    if (gMainRunLoop)                 CFRelease(gMainRunLoop);

#ifndef NO_CFUserNotification
    if (gPendedNonsecureKexts)         CFRelease(gPendedNonsecureKexts);
    if (gPendedKextloadOperations)     CFRelease(gPendedKextloadOperations);
    if (gScheduledNonsecureKexts)      CFRelease(gScheduledNonsecureKexts);
#endif /* NO_CFUserNotification */

    exit(exit_status);
    return exit_status;
}

/*******************************************************************************
*
*******************************************************************************/

static Boolean kextd_is_running(mach_port_t * bootstrap_port_ref)
{
    boolean_t active = FALSE;
    Boolean result = false;
    kern_return_t kern_result = KERN_SUCCESS;
    mach_port_t   bootstrap_port;

    if (bootstrap_port_ref && (*bootstrap_port_ref != PORT_NULL)) {
        bootstrap_port = *bootstrap_port_ref;
    } else {
        /* Get the bootstrap server port */
        kern_result = task_get_bootstrap_port(mach_task_self(),
            &bootstrap_port);
        if (kern_result != KERN_SUCCESS) {
            kextd_error_log("task_get_bootstrap_port(): %s\n",
                mach_error_string(kern_result));
            exit (EX_UNAVAILABLE);
        }
        if (bootstrap_port_ref) {
            *bootstrap_port_ref = bootstrap_port;
        }
    }

    /* Check "kextd" server status */
    kern_result = bootstrap_status(bootstrap_port,
        (char *)KEXTD_SERVER_NAME, &active);
    switch (kern_result) {
      case BOOTSTRAP_SUCCESS:
        if (active) {
            kextd_error_log("kextd: '%s' is already active\n",
                KEXTD_SERVER_NAME);
            result = true;
            goto finish;
        }
        break;

      case BOOTSTRAP_UNKNOWN_SERVICE:
        result = false;
        goto finish;
        break;

      default:
        kextd_error_log("bootstrap_status(): %s\n",
            mach_error_string(kern_result));
        exit(EX_UNAVAILABLE);
    }

finish:
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
static int kextd_get_mach_ports(void)
{
    kern_return_t kern_result;

    kern_result = IOMasterPort(NULL, &g_io_master_port);
    // FIXME: check specific kernel error result for permission or whatever
    if (kern_result != KERN_SUCCESS) {
       kextd_error_log("couldn't get catalog port");
       return 0;
    }
    return 1;
}

/*******************************************************************************
*
*******************************************************************************/
int kextd_fork(void)
{
    uid_t pid;

    // prep parent to receive sigterm from child
    signal(SIGTERM, kextd_handle_sigterm);

    pid = fork();
    switch (pid) {
      case -1:
        return 0;
        break;
      case 0:   // child task
        // Reregister/get Mach ports for the child
        if (!kextd_get_mach_ports()) {
            // kextd_get_mach_ports() logged an error message
            exit(1);
        }

        // child doesn't process sigterm
        signal(SIGTERM, SIG_DFL);
        // FIXME: old kextd did this; is it needed?
        _CFRunLoopSetCurrent(NULL);
        break;
      default:  // parent task
        {
            /* parent: wait for signal, then exit */
            int status;

            wait4(pid, (int *)&status, 0, 0);
            if (WIFEXITED(status)) {
                kextd_error_log(
                    "*** %s (daemon) failed to start, exit status=%d",
                    progname, WEXITSTATUS(status));
            } else {
                kextd_error_log(
                    "*** %s (daemon) failed to start, received signal=%d",
                    progname, WTERMSIG(status));
            }
            fflush (stderr);
            exit(1);
        }
        break;
    }

   /****
    * Set a new session for the kextd child process.
    */
    if (setsid() == -1) {
        return 0;
    }

   /****
    * Be sure to run relative to the root of the filesystem, just in case.
    */
    if (chdir("/") == -1) {
        return 0;
    }

    return 1;
}

/*******************************************************************************
* kextd_set_up_server()
*******************************************************************************/
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

    if (kextd_is_running(&bootstrap_port)) {
        result = false;
        goto finish;
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
    * Add the runloop sources in decreasing priority. Signals are handled
    * first, followed by kernel requests, and then by client requests.
    * It's important that each source have a distinct priority; sharing
    * them causes unpredictable behavior with the runloop.
    */
    sourceContext.perform = kextd_handle_sighup_in_runloop;
    gRescanRunLoopSource = CFRunLoopSourceCreate(kCFAllocatorDefault,
        sourcePriority++, &sourceContext);
    if (!gRescanRunLoopSource) {
       kextd_error_log("couldn't create signal-handling run loop source");
        result = false;
        goto finish;
    }
    CFRunLoopAddSource(gMainRunLoop, gRescanRunLoopSource,
        kCFRunLoopDefaultMode);

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

    kextdMachPort = CFMachPortCreate(kCFAllocatorDefault,
        kextd_mach_port_callback, NULL, NULL);
    gClientRequestRunLoopSource = CFMachPortCreateRunLoopSource(
        kCFAllocatorDefault, kextdMachPort, sourcePriority++);
    if (!gClientRequestRunLoopSource) {
       kextd_error_log("couldn't create client request run loop source");
        result = false;
        goto finish;
    }
    CFRunLoopAddSource(gMainRunLoop, gClientRequestRunLoopSource,
        kCFRunLoopDefaultMode);

#ifndef NO_CFUserNotification

    sourceContext.perform = kextd_handle_pended_kextload;
    gNonsecureKextRunLoopSource = CFRunLoopSourceCreate(kCFAllocatorDefault,
        sourcePriority++, &sourceContext);
    if (!gNonsecureKextRunLoopSource) {
       kextd_error_log("couldn't create pended kextload run loop source");
        result = false;
        goto finish;
    }
    CFRunLoopAddSource(gMainRunLoop, gNonsecureKextRunLoopSource,
        kCFRunLoopDefaultMode);

#endif /* NO_CFUserNotification */


    kextd_log("registering service \"%s\"", KEXTD_SERVER_NAME);
    kern_result = bootstrap_register(bootstrap_port,
        (char *)KEXTD_SERVER_NAME, CFMachPortGetPort(kextdMachPort));

    switch (kern_result) {
      case BOOTSTRAP_SUCCESS:
        /* service not currently registered, "a good thing" (tm) */
        break;

      case BOOTSTRAP_NOT_PRIVILEGED:
        kextd_error_log("bootstrap_register(): bootstrap not privileged");
        exit(EX_OSERR);

      case BOOTSTRAP_SERVICE_ACTIVE:
        kextd_error_log("bootstrap_register(): bootstrap service active");
        exit(EX_OSERR);

      default:
        kextd_error_log("bootstrap_register(): %s",
            mach_error_string(kern_result));
        exit(EX_OSERR);
    }

finish:
    if (gRescanRunLoopSource)         CFRelease(gRescanRunLoopSource);
    if (gKernelRequestRunLoopSource)  CFRelease(gKernelRequestRunLoopSource);
    if (gClientRequestRunLoopSource)  CFRelease(gClientRequestRunLoopSource);
#ifndef NO_CFUserNotification
    if (gNonsecureKextRunLoopSource)  CFRelease(gNonsecureKextRunLoopSource);
#endif /* NO_CFUserNotification */
    if (kextdMachPort)                CFRelease(kextdMachPort);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
void kextd_release_parent_task(void)
{
    // FIXME: Add error checking?
    kill(getppid(), SIGTERM);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void kextd_register_signals(void)
{
    signal(SIGHUP, kextd_handle_sighup);
    return;
}

/*******************************************************************************
* registered and used by parent of forked daemon to exit
* upon signal from forked daemon.
*******************************************************************************/
void kextd_handle_sigterm(int signum)
{
    kern_return_t    kern_result;
    mach_timespec_t  waitTime = { 40, 0 };

    kern_result = IOKitWaitQuiet(g_io_master_port, &waitTime);
    if (kern_result == kIOReturnTimeout) {
        kextd_error_log("IOKitWaitQuiet() timed out",
        kern_result);
    } else if (kern_result != kIOReturnSuccess) {
        kextd_error_log("IOKitWaitQuiet() failed with result code %lx",
        kern_result);
    }
    exit(0);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void kextd_handle_sighup(int signum)
{
    if (gRescanRunLoopSource) {
        PTLockTakeLock(gRunLoopSourceLock);
        kextd_log("received SIGHUP; rescanning all kexts and resetting catalogue");
        CFRunLoopSourceSignal(gRescanRunLoopSource);
        CFRunLoopWakeUp(gMainRunLoop);
        PTLockUnlock(gRunLoopSourceLock);
    } else {
        kextd_log("received SIGHUP before entering run loop; ignoring");
    }


    return;
}

/*******************************************************************************
*
*******************************************************************************/
void kextd_rescan(void)
{
#ifndef NO_CFUserNotification
   /* Dump all nonsecure kexts awaiting load by kextd. We're rescanning so
    * this will all get retriggered.
    */
    CFArrayRemoveAllValues(gPendedNonsecureKexts);
    CFArrayRemoveAllValues(gScheduledNonsecureKexts);

   /* Clear any security alert awaiting user input. Don't clear a kextload
    * operation awaiting user input, however, as that would likely never
    * get retriggered, and it doesn't access the kextmanager data set.
    */
    if (gSecurityNotification) {
        CFUserNotificationCancel(gSecurityNotification);
        CFRelease(gSecurityNotification);
        gSecurityNotification = NULL;
    }
    if (gFailureNotification) {
        CFUserNotificationCancel(gFailureNotification);
        CFRelease(gFailureNotification);
        gFailureNotification = NULL;
    }
    gSecurityAlertKext = NULL;
    gResendSecurityAlertKextPersonalities = false;

#endif /* NO_CFUserNotification */

    KXKextManagerResetAllRepositories(gKextManager);

    // FIXME: Should we exit if this fails?
    kextd_download_personalities();

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void kextd_handle_sighup_in_runloop(void * info)
{
    kextd_rescan();
    return;
}

/*******************************************************************************
*
*******************************************************************************/
static Boolean kextd_download_personalities(void)
{
    Boolean result = true;
    CFArrayRef allKextPersonalities = NULL;  // must release

   /*****
    * Empty the kernel's catalogue and send all candidate kext personalities
    * down.
    */
    IOCatalogueReset(g_io_master_port, kIOCatalogResetDefault);

    allKextPersonalities = KXKextManagerCopyAllKextPersonalities(gKextManager);
    if (!allKextPersonalities) {
        kextd_error_log("can't get kext personalities to send to kernel");
        result = false;
        goto finish;
    }

    if (KXKextManagerSendPersonalitiesToCatalog(gKextManager,
           allKextPersonalities) != kKXKextManagerErrorNone) {

        kextd_error_log("can't send kext personalities to kernel");
        result = false;
        goto finish;
    }

finish:

    if (allKextPersonalities) {
        CFRelease(allKextPersonalities);
    }

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
static Boolean kextd_process_ndrvs(CFArrayRef repositoryDirectories)
{
    Boolean     result = true;
    CFIndex     repositoryCount, r;
    CFStringRef thisPath = NULL;        // don't release
    CFURLRef    repositoryURL = NULL;   // must release
    CFURLRef    ndrvDirURL = NULL;      // must release

    repositoryCount = CFArrayGetCount(repositoryDirectories);
    for (r = 0; r < repositoryCount; r++) {

       /* Clean up at top of loop in case of a continue.
        */
        if (repositoryURL) {
            CFRelease(repositoryURL);
            repositoryURL = NULL;
        }
        if (ndrvDirURL) {
            CFRelease(ndrvDirURL);
            ndrvDirURL = NULL;
        }

        thisPath = (CFStringRef)CFArrayGetValueAtIndex(
            repositoryDirectories, r);
        repositoryURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
            thisPath, kCFURLPOSIXPathStyle, true);
        if (!repositoryURL) {
            kextd_error_log("memory allocation failure");
            result = 0;
            goto finish;
        }
        ndrvDirURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault,
            repositoryURL, CFSTR("AppleNDRV"), true);
        if (!ndrvDirURL) {
            kextd_error_log("memory allocation failure");
            result = 0;
            goto finish;
        }

	IOLoadPEFsFromURL( ndrvDirURL, MACH_PORT_NULL );
    }

finish:
    if (repositoryURL)   CFRelease(repositoryURL);
    if (ndrvDirURL)      CFRelease(ndrvDirURL);

    return result;
}


/*******************************************************************************
*
*******************************************************************************/
static void usage(int level)
{
    fprintf(stderr,
        "usage: %s [-c] [-d] [-f] [-h] [-j] [-r directory] ... [-v [1-6]] [-x]",
        progname);
    if (level > 1) {
        kextd_error_log("    -c   don't use repository caches; scan repository folders\n");
        kextd_error_log("    -d   run in debug mode (don't fork daemon)\n");
        kextd_error_log("    -f   don't fork when loading (for debugging only)\n");
        kextd_error_log("    -h   help; print this list\n");
        kextd_error_log("    -j   don't jettison kernel linker; "
            "just load NDRVs and exit (for startup from install CD)\n");
        kextd_error_log("    -r   start up with kexts in directory in addition to "
            "those in /System/Library/Extensions\n");
        kextd_error_log("    -v   verbose mode\n");
        kextd_error_log("    -x   run in safe boot mode.\n");
    }
    return;
}

/*******************************************************************************
*
*******************************************************************************/
char * CFURLCopyCString(CFURLRef anURL)
{
    char * string = NULL; // returned
    CFIndex bufferLength;
    CFStringRef urlString = NULL;  // must release
    Boolean error = false;

    urlString = CFURLCopyFileSystemPath(anURL, kCFURLPOSIXPathStyle);
    if (!urlString) {
        goto finish;
    }

    bufferLength = 1 + CFStringGetLength(urlString);

    string = (char *)malloc(bufferLength * sizeof(char));
    if (!string) {
        goto finish;
    }

    if (!CFStringGetCString(urlString, string, bufferLength,
           kCFStringEncodingMacRoman)) {

        error = true;
        goto finish;
    }

finish:
    if (error) {
        free(string);
        string = NULL;
    }
    if (urlString) CFRelease(urlString);
    return string;
}
