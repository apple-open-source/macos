#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPriv.h>  // for _CFRunLoopSetCurrent()
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <IOKit/IOCFURLAccess.h>
#include <IOKit/IOCFUnserialize.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_error.h>
#include <mach-o/arch.h>
#include <libc.h>
#include <servers/bootstrap.h>
#include <signal.h>
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

static const char * KEXTD_SERVER_NAME = "com.apple.KernelExtensionServer";

#define kKXROMExtensionsFolder        "/System/Library/Caches/com.apple.romextensions/"
#define kKXCSystemExtensionsFolder    "/System/Library/Extensions"

char * progname = "(unknown)";  // don't free
Boolean use_repository_caches = true;
Boolean debug = false;
Boolean load_in_task = false;
Boolean jettison_kernel_linker = true;
int g_verbose_level = 0;        // nonzero for -v option
Boolean safe_boot_mode = false;

static Boolean parent_received_sigterm = false;
static Boolean parent_received_sigchld = false;
static Boolean parent_received_sigalrm = false;

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
static CFMachPortRef gKextdSignalMachPort = NULL;      // must release
static CFRunLoopSourceRef gSignalRunLoopSource = NULL; // must release

#ifndef NO_CFUserNotification
CFRunLoopSourceRef gNotificationQueueRunLoopSource = NULL;     // must release
#endif /* NO_CFUserNotification */

const char * default_kernel_file = "/mach";

queue_head_t g_request_queue;
PTLockRef gKernelRequestQueueLock = NULL;
PTLockRef gRunLoopSourceLock = NULL;

static mach_port_t gBootstrap_port = 0;

/*******************************************************************************
* Function prototypes.
*******************************************************************************/

static Boolean kextd_is_running(mach_port_t * bootstrap_port_ref);
static int kextd_get_mach_ports(void);
static int kextd_fork(void);
static Boolean kextd_set_up_server(void);
static void kextd_release_parent_task(void);

void kextd_register_signals(void);
void kextd_parent_handle_signal(int signum);
void kextd_handle_sighup(int signum);
void kextd_handle_rescan(void * info);

static Boolean kextd_find_rom_mkexts(void);
static void check_extensions_mkext(void);
static Boolean kextd_download_personalities(void);

static void usage(int level);

char * CFURLCopyCString(CFURLRef anURL);

void kextd_runloop_handle_sighup(
    CFMachPortRef port,
    void *msg,
    CFIndex size,
    void *info);

/*******************************************************************************
*******************************************************************************/

int main (int argc, const char * argv[]) {
    int exit_status = 0;
    KXKextManagerError result = kKXKextManagerErrorNone;
    int optchar;
    CFIndex count, i, rom_repository_idx = -1;
    Boolean have_rom_mkexts = FALSE;

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
    if (have_rom_mkexts)
    {
	rom_repository_idx = 1;
	CFArrayInsertValueAtIndex(repositoryDirectories, rom_repository_idx,
	    CFSTR(kKXROMExtensionsFolder));
    }

   /*****
    * If we're not replacing the in-kernel linker, we're done.
    */
    if (!jettison_kernel_linker) {
        goto finish;
    }

   /*****
    * Check Extensions.mkext needs a rebuild
    */
    check_extensions_mkext();

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
    if (gPendedNonsecureKextPaths)     CFRelease(gPendedNonsecureKextPaths);
    if (gNotifiedNonsecureKextPaths)   CFRelease(gNotifiedNonsecureKextPaths);
#endif /* NO_CFUserNotification */

    exit(exit_status);
    return exit_status;
}

/*******************************************************************************
*
*******************************************************************************/

#define TEMP_FILE		"/tmp/com.apple.iokit.kextd.XX"
#define MKEXTUNPACK_COMMAND	"/usr/sbin/mkextunpack "	\
				"-d "kKXROMExtensionsFolder" "

static kern_return_t process_mkext(const UInt8 * bytes, CFIndex length)
{
    kern_return_t	err;
    char		temp_file[1 + strlen(TEMP_FILE)];
    char		mkextunpack_cmd[1 + strlen(TEMP_FILE) + strlen(MKEXTUNPACK_COMMAND)];
    const char *	rom_ext_dir = kKXROMExtensionsFolder;
    int 		outfd = -1;
    struct stat		stat_buf;
    
    strcpy(temp_file, TEMP_FILE);
    mktemp(temp_file);
    outfd = open(temp_file, O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (-1 == outfd) {
	kextd_error_log("can't create %s - %s\n", temp_file,
	    strerror(errno));
	err = kKXKextManagerErrorFileAccess;
	goto finish;
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
	if (0 != mkdir(rom_ext_dir, 0755))
	{
	    kextd_error_log("mkdir(%s) failed: %s\n", rom_ext_dir, strerror(errno));
	    err = kKXKextManagerErrorFileAccess;
	    goto finish;
	}
    }

    strcpy(mkextunpack_cmd, MKEXTUNPACK_COMMAND);
    strcat(mkextunpack_cmd, temp_file);

    // kextd_error_log(mkextunpack_cmd);

    if (0 != system(mkextunpack_cmd))
    {
	kextd_error_log(mkextunpack_cmd);
	kextd_error_log("failed");
	err = kKXKextManagerErrorChildTask;
        goto finish;
    }

finish:
    if (-1 != outfd)
	close(outfd);
    unlink(temp_file);

    return err;
}

/*******************************************************************************
*
*******************************************************************************/

static Boolean kextd_find_rom_mkexts(void)
{
    kern_return_t	kr;
    CFSetRef		set = NULL;
    CFDataRef *		mkexts = NULL;
    CFIndex		count, idx;
    char *		propertiesBuffer;
    int			loaded_bytecount;
    enum {		_kIOCatalogGetROMMkextList = 4  };

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

#define REBUILDMKEXT_COMMAND	"/usr/sbin/kextcache -elF -a "

static void check_extensions_mkext(void)
{
    struct stat extensions_stat_buf;
    struct stat mkext_stat_buf;
    Boolean rebuild;
    char rebuildmkext_cmd[1 + strlen(REBUILDMKEXT_COMMAND) + 64];

    rebuild = (0 != stat(kKXCSystemExtensionsFolder ".mkext", &mkext_stat_buf));
    if (!rebuild && (0 == stat(kKXCSystemExtensionsFolder, &extensions_stat_buf)))
	rebuild = (mkext_stat_buf.st_mtime != (extensions_stat_buf.st_mtime + 1));
    
    if (rebuild) do
    {
	const NXArchInfo *
	arch = NXGetLocalArchInfo();
	if (arch)
	    arch = NXGetArchInfoFromCpuType(arch->cputype, CPU_SUBTYPE_MULTIPLE);
	if (!arch)
	{
	    kextd_error_log("unknown architecture");
	    break;
	}

	strcpy(rebuildmkext_cmd, REBUILDMKEXT_COMMAND);
	strcat(rebuildmkext_cmd, arch->name);

	// kextd_error_log(rebuildmkext_cmd);

	if (0 != system(rebuildmkext_cmd))
	{
	    kextd_error_log(rebuildmkext_cmd);
	    kextd_error_log("failed");
	    break;
	}
    }
    while (false);
}

/*******************************************************************************
*
*******************************************************************************/

static Boolean kextd_is_running(mach_port_t * bootstrap_port_ref)
{
    boolean_t active = FALSE;
    Boolean result = false;
    kern_return_t kern_result = KERN_SUCCESS;
    mach_port_t   the_bootstrap_port;

    if (bootstrap_port_ref && (*bootstrap_port_ref != PORT_NULL)) {
        the_bootstrap_port = *bootstrap_port_ref;
    } else {
        /* Get the bootstrap server port */
        kern_result = task_get_bootstrap_port(mach_task_self(),
            &the_bootstrap_port);
        if (kern_result != KERN_SUCCESS) {
            kextd_error_log("task_get_bootstrap_port(): %s\n",
                mach_error_string(kern_result));
            exit (EX_UNAVAILABLE);
        }
        if (bootstrap_port_ref) {
            *bootstrap_port_ref = the_bootstrap_port;
        }
    }

    /* Check "kextd" server status */
    kern_result = bootstrap_status(the_bootstrap_port,
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
    sigset_t signal_set;
    sigset_t old_signal_set;
    uid_t pid;

    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGTERM);
    sigaddset(&signal_set, SIGCHLD);
    sigaddset(&signal_set, SIGALRM);

    if (sigprocmask(SIG_BLOCK, &signal_set, &old_signal_set)) {
         kextd_error_log("sigprocmask() failed");
         return 0;
    }

    // prep parent to receive signals from/about child
    signal(SIGTERM, kextd_parent_handle_signal);
    signal(SIGCHLD, kextd_parent_handle_signal);
    signal(SIGALRM, kextd_parent_handle_signal);

    pid = fork();
    switch (pid) {

      case -1:
        return 0;  // error
        break;

      case 0:   // child task

        // child doesn't process sigterm or sigalrm
        signal(SIGTERM, SIG_DFL);
        signal(SIGALRM, SIG_DFL);

        if (sigprocmask(SIG_UNBLOCK, &old_signal_set, NULL)) {
             kextd_error_log("sigprocmask() failed");
             return 0;
        }

        // Reregister/get Mach ports for the child
        if (!kextd_get_mach_ports()) {
            // kextd_get_mach_ports() logged an error message
            exit(1);
        }

        // FIXME: old kextd did this; is it needed?
        _CFRunLoopSetCurrent(NULL);
        break;

      default:  // parent task
	    kextd_openlog("kextd-parent");  // should that arg be progname?

		struct itimerval timer_buf;
		timer_buf.it_interval.tv_sec = 0;
		timer_buf.it_interval.tv_usec = 0;
		timer_buf.it_value.tv_sec = 40;
		timer_buf.it_value.tv_usec = 0;
		setitimer(ITIMER_REAL, &timer_buf, NULL);

		while (1) {
			sigsuspend(&old_signal_set);
			if (parent_received_sigterm) {
				kern_return_t    kern_result;
				mach_timespec_t  waitTime = { 40, 0 };

				kern_result = IOKitWaitQuiet(g_io_master_port, &waitTime);
				if (kern_result == kIOReturnTimeout) {
					kextd_error_log("IOKitWaitQuiet() timed out",
					kern_result);
				} else if (kern_result != kIOReturnSuccess) {
					kextd_error_log("IOKitWaitQuiet() failed with result code %lx",
					kern_result);
					exit(1);
				}
				exit(0);
			} else if (parent_received_sigchld) {
				kextd_error_log("kextd forked child task exited abnormally");
				exit(1);
			} else if (parent_received_sigalrm) {
				kextd_error_log("kextd parent task timed out waiting for signal from child");
				exit(1);
			}
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
// in mig_server.c
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

    if (kextd_is_running(&gBootstrap_port)) {
        result = false;
        goto finish;
    }

    gRunLoopSourceLock = PTLockCreate();
    if (!gRunLoopSourceLock) {
        kextd_error_log(
            "failed to create kernel request run loop source lock");
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
    sourceContext.perform = kextd_handle_rescan;
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

    gKextdSignalMachPort = CFMachPortCreate(kCFAllocatorDefault,
        kextd_runloop_handle_sighup, NULL, NULL);
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
    if (gNotificationQueueRunLoopSource) CFRelease(gNotificationQueueRunLoopSource);
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
void kextd_parent_handle_signal(int signum)
{
    switch (signum) {
      case SIGTERM:
        parent_received_sigterm = true;
        break;
      case SIGCHLD:
        parent_received_sigchld = true;
        break;
      case SIGALRM:
        parent_received_sigalrm = true;
        break;
    }
    return;
}

/*******************************************************************************
* On receiving a SIGHUP, the daemon sends a Mach message to the signal port,
* causing the run loop handler function kextd_runloop_handle_sighup() to be called.
*******************************************************************************/
void kextd_handle_sighup(int signum)
{
    mach_msg_empty_send_t msg;
    mach_msg_option_t options;
    kern_return_t kern_result;

    if (signum != SIGHUP) {
        return;
    }

   /*
    * send message to indicate that a request has been made
    * for the daemon to be shutdown.
    */
    msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    msg.header.msgh_size = sizeof(msg);
    msg.header.msgh_remote_port = CFMachPortGetPort(gKextdSignalMachPort);
    msg.header.msgh_local_port = MACH_PORT_NULL;
    msg.header.msgh_id = 0;
    options = MACH_SEND_TIMEOUT;
    kern_result = mach_msg(&msg.header,  /* msg */
        MACH_SEND_MSG|options,      /* options */
        msg.header.msgh_size,       /* send_size */
        0,                          /* rcv_size */
        MACH_PORT_NULL,             /* rcv_name */
        0,                          /* timeout */
        MACH_PORT_NULL);            /* notify */
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void kextd_runloop_handle_sighup(
    CFMachPortRef port,
    void *msg,
    CFIndex size,
    void *info)
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
#endif /* NO_CFUserNotification */


/*******************************************************************************
*
*******************************************************************************/
void kextd_handle_rescan(void * info)
{
#ifndef NO_CFUserNotification
    kextd_clear_all_notifications();
#endif /* NO_CFUserNotification */

    KXKextManagerResetAllRepositories(gKextManager);

    // FIXME: Should we exit if this fails?
    kextd_download_personalities();

    return;
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
