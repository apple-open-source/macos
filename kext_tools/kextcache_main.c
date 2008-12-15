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
#include <CoreFoundation/CFBundlePriv.h>
#include <err.h>    	// warn[x]
#include <errno.h>
#include <libc.h>
#include <libgen.h>	// dirname()
#include <Kernel/libsa/mkext.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fts.h>
#include <paths.h>
#include <mach-o/arch.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_port.h>	// mach_port_allocate()
#include <mach/mach_types.h>
#include <mach/machine/vm_param.h>
#include <mach/kmod.h>
#include <servers/bootstrap.h>	// bootstrap mach ports
#include <unistd.h>		// sleep(3)
#include <sys/types.h>
#include <sys/stat.h>

#include <TargetConditionals.h>
#if !TARGET_OS_EMBEDDED
#include <DiskArbitration/DiskArbitrationPrivate.h>
#endif
#include <IOKit/IOTypes.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/IOCFSerialize.h>
#include <libkern/OSByteOrder.h>

#include <IOKit/kext/KXKextManager.h>
#include <IOKit/kext/kextmanager_types.h>
#include <IOKit/kext/kextmanager_mig.h>
#include <IOKit/kext/kernel_check.h>
#include <bootfiles.h>

#include "bootcaches.h"
#include "update_boot.h"
#include "utility.h"
#include "logging.h"

// constants
#define SA_HAS_RUN_PATH		"/var/db/.AppleSetupDone"
#define kKXROMExtensionsFolder  "/System/Library/Caches/com.apple.romextensions/"
#define MKEXT_PERMS             (0644)


/*******************************************************************************
* Program Globals
*******************************************************************************/
const char * progname = "(unknown)";

/*******************************************************************************
* File-Globals
*******************************************************************************/
static mach_port_t sLockPort = MACH_PORT_NULL;
static uuid_t s_vol_uuid;
static mach_port_t sKextdPort = MACH_PORT_NULL;

/*******************************************************************************
* Extern functions.
*******************************************************************************/

// In compression.c
__private_extern__ u_int32_t
local_adler32(u_int8_t *buffer, int32_t length);

// In arch.c
__private_extern__ void
find_arch(u_int8_t **dataP, off_t *sizeP, cpu_type_t in_cpu,
    cpu_subtype_t in_cpu_subtype, u_int8_t *data_ptr, off_t filesize);
__private_extern__ int
get_arch_from_flag(char *name, cpu_type_t *cpuP, cpu_subtype_t *subcpuP);

// in mkext_file.c
ssize_t createMkextArchive(
    int fd,
    CFDictionaryRef kextDict,
    const char * mkextFilename,
    const char * archName,
    cpu_type_t archCPU,
    cpu_subtype_t archSubtype,
    int verbose_level);
Boolean mkextArchiveSizeOverLimit(ssize_t size, cpu_type_t cpuType);

// in prelink.c
KXKextManagerError
PreLink(KXKextManagerRef theKextManager, CFDictionaryRef kextDict,
        const char * kernelCacheFilename,
	const struct timeval *cacheFileTimes,
	const char * kernel_file_name, 
	const char * platform_name,
	const char * root_path,
	CFSetRef kernel_requests,
	Boolean all_plists,
	const NXArchInfo * arch,
	int verbose_level, Boolean debug_mode);

/*******************************************************************************
* Utility and callback functions.
*******************************************************************************/
static void addKextForMkextCache(
    KXKextRef theKext,
    CFMutableDictionaryRef checkDictionary,
    cpu_type_t archCPU,
    cpu_subtype_t archSubtype,
    Boolean local,
    Boolean network,
    Boolean safeboot,
    CFSetRef kernel_requests,
    int verbose_level,
    Boolean do_tests);
static void collectKextsForMkextCache(
    CFArrayRef kexts,
    CFMutableDictionaryRef checkDictionary,
    cpu_type_t archCPU,
    cpu_subtype_t archSubtype,
    Boolean local,
    Boolean network,
    Boolean safeboot,
    CFSetRef kernel_requests,
    int verbose_level,
    Boolean do_tests);

// put/take helpers
static int takeVolumeForPath(const char *path);

static void get_catalog_demand_lists(CFMutableSetRef * kernel_requests,
				      CFSetRef * kernel_cache_misses);
static void usage(int level);

#define kMaxArchs 64
#define kRootPathLen 256

/*******************************************************************************
*******************************************************************************/

// so utility.c can get at it
int g_verbose_level = kKXKextManagerLogLevelDefault;  // -v

int main(int argc, const char * argv[])
{
    int exit_code = 0;
    int optchar;
    KXKextManagerRef theKextManager = NULL;  // must release
    KXKextManagerError result;
    mode_t             real_umask;

    CFIndex i, count;

   /*****
    * Set by command line option flags.
    * XX a struct would allow much simpler initialization
    * *and* allow argument processing elsewhere.
    */
    Boolean do_tests = false;                // -t
    Boolean forkExit = false;                // -F
    Boolean local_for_repositories = false;  // -l
    Boolean network_for_repositories = false;// -n
    Boolean safeboot_for_repositories = false; // -s
    Boolean local_for_all = false;           // -L
    Boolean network_for_all = false;         // -N
    Boolean safeboot_for_all = false;        // -S
    Boolean repositoryCaches = false;        // -k
    const char * mkextFilename = NULL;       // -m; don't release
    char kernelCacheBuffer[PATH_MAX];
    const char * kernelCacheFilename = NULL; // -c; don't release
    Boolean pretend_authentic = false;       // -z
    Boolean load_in_task = false;
    Boolean include_kernel_requests = false;
    Boolean cache_looks_uptodate = false;
    Boolean debug_mode = false;		     // -d
    Boolean forceUpdate = false;             // -f
    char *updateRoot = NULL;		     // -u/-U
    Boolean expectUpToDate = false;          // -U
    const char * default_kernel_file = kDefaultKernel;
    const char * kernel_file = NULL;  // overriden by -K option
    const char * source_extensions    = kSystemExtensionsDir;
    int fd = -1;
    const char * output_filename = NULL;
    char temp_file[MAXPATHLEN];

    struct stat kernel_stat_buf;
    struct stat extensions_stat_buf;
    struct stat rom_extensions_stat_buf;
    Boolean have_kernel_time, have_extensions_time;
    Boolean need_default_kernelcache_info = false;
    struct timeval _cacheFileTimes[2];
    struct timeval *cacheFileTimes = NULL;

    struct {
	char platform_name[64];
	char root_path[kRootPathLen];
    } platform_name_root_path;
    io_registry_entry_t entry;
    Boolean all_plists;

    // -a for these three
    Boolean explicit_arch = false;  // if any -a used
    NXArchInfo archAny = {
        .name = "any",
        .cputype = CPU_TYPE_ANY,
        .cpusubtype = CPU_SUBTYPE_MULTIPLE,
        .byteorder = NX_UnknownByteOrder,
        .description = "any"
    };

    CFMutableArrayRef repositoryDirectories = NULL;  // must release
    CFMutableArrayRef repositories = NULL;           // must release
    CFMutableArrayRef kextNames = NULL;              // args; must release
    CFMutableArrayRef kextNamesToUse = NULL;         // must release
    CFMutableArrayRef namedKexts = NULL;             // must release
    CFMutableArrayRef repositoryKexts = NULL;        // must release
    CFMutableDictionaryRef checkDictionary = NULL;   // must release
    CFMutableSetRef   kernel_requests = NULL;	     // must release
    CFSetRef	      kernel_cache_misses = NULL;    // must release
    const NXArchInfo *archs[kMaxArchs] = {NULL};
    int nArchs = 0;

   /*****
    * Find out what the program was invoked as.
    */
    progname = rindex(argv[0], '/');
    if (progname) {
        progname++;   // go past the '/'
    } else {
        progname = (char *)argv[0];
    }

   /*****
    * Allocate collection objects.
    */
    repositoryDirectories = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!repositoryDirectories) {
        exit_code = 1;
        fprintf(stderr, "memory allocation failure\n");
        goto finish;
    }

    repositories = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!repositories) {
        exit_code = 1;
        fprintf(stderr, "memory allocation failure\n");
        goto finish;
    }

    kextNames = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!kextNames) {
        exit_code = 1;
        fprintf(stderr, "memory allocation failure\n");
        goto finish;
    }

    kextNamesToUse = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!kextNamesToUse) {
        exit_code = 1;
        fprintf(stderr, "memory allocation failure\n");
        goto finish;
    }

    namedKexts = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!namedKexts) {
        exit_code = 1;
        fprintf(stderr, "memory allocation failure\n");
        goto finish;
    }

    repositoryKexts = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!repositoryKexts) {
        exit_code = 1;
        fprintf(stderr, "memory allocation failure\n");
        goto finish;
    }

    checkDictionary = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!checkDictionary) {
        exit_code = 1;
        fprintf(stderr, "memory allocation failure\n");
        goto finish;
    }

    bzero(&platform_name_root_path, sizeof(platform_name_root_path));
    entry = IORegistryEntryFromPath(kIOMasterPortDefault, kIOServicePlane ":/");
    if (entry)
    {
	if (KERN_SUCCESS != IORegistryEntryGetName(entry, platform_name_root_path.platform_name))
	    platform_name_root_path.platform_name[0] = 0;
	IOObjectRelease(entry);
    }

    entry = IORegistryEntryFromPath(kIOMasterPortDefault, kIODeviceTreePlane ":/chosen");
    if (entry)
    {
	CFTypeRef obj = 0;
	obj = IORegistryEntryCreateCFProperty(entry, CFSTR("rootpath"), kCFAllocatorDefault, kNilOptions);
	if (obj && (CFGetTypeID(obj) == CFDataGetTypeID()))
	{
	    CFIndex len = CFDataGetLength((CFDataRef) obj);
	    strncpy(platform_name_root_path.root_path, (char *)CFDataGetBytePtr((CFDataRef) obj), len);
	    platform_name_root_path.root_path[len] = 0;
	} else {
            const char *data;
            char *ptr = platform_name_root_path.root_path;
            CFIndex len;
            // Construct entry from UUID of boot volume and kernel name.
            obj = 0;
            do {
                obj = IORegistryEntryCreateCFProperty(entry, CFSTR("boot-device-path"), kCFAllocatorDefault, kNilOptions);
                if (!obj)
                    break;

                if (CFGetTypeID(obj) == CFDataGetTypeID()) {
                    data = (char *)CFDataGetBytePtr((CFDataRef) obj);
                    len = CFDataGetLength((CFDataRef) obj);
                } else if (CFGetTypeID(obj) == CFStringGetTypeID()) {
                    data = CFStringGetCStringPtr((CFStringRef) obj, kCFStringEncodingUTF8);
                    if (!data)
                        break;
                    len = strlen(data) + 1; // include trailing null
                } else {
                    break;
                }
                if (len > kRootPathLen)
                    len = kRootPathLen;
                memcpy(ptr, data, len);
                ptr += len;

                CFRelease(obj);

                obj = IORegistryEntryCreateCFProperty(entry, CFSTR("boot-file"), kCFAllocatorDefault, kNilOptions);
                if (!obj)
                    break;

                if (CFGetTypeID(obj) == CFDataGetTypeID()) {
                    data = (char *)CFDataGetBytePtr((CFDataRef) obj);
                    len = CFDataGetLength((CFDataRef) obj);
                } else if (CFGetTypeID(obj) == CFStringGetTypeID()) {
                    data = CFStringGetCStringPtr((CFStringRef) obj, kCFStringEncodingUTF8);
                    if (!data)
                        break;
                    len = strlen(data);
                } else {
                    break;
                }
                if ((ptr - platform_name_root_path.root_path + len) >= kRootPathLen)
                    len = kRootPathLen - (ptr - platform_name_root_path.root_path);
                memcpy(ptr, data, len);
            } while (0);
        }
        if (obj)
            CFRelease(obj);
	IOObjectRelease(entry);
    }
    if (!platform_name_root_path.platform_name[0] || !platform_name_root_path.root_path[0])
	platform_name_root_path.platform_name[0] = platform_name_root_path.root_path[0] = 0;

    /*****
    * Process command line arguments. If running in kextload-compatibiliy
    * mode, accept its old set of options and none other. If running in
    * the new mode, process the new, larger set of options.
    */
    while ((optchar = getopt(argc, (char * const *)argv,
               "a:cdefFhkK:lLm:nNrsStu:U:vz")) != -1) {

        switch (optchar) {

          case 'a':
            if (nArchs >= kMaxArchs) {
                  fprintf(stderr, "maximum of %d architectures supported\n", kMaxArchs);
                  exit_code = 1;
                  goto finish;
            }
            archs[nArchs] = NXGetArchInfoFromName(optarg);
            if (archs[nArchs] == NULL) {
                fprintf(stderr, "unknown CPU arch %s\n\n", optarg);
                usage(0);
                exit_code = 1;
                goto finish;
            }
            explicit_arch = true;
            ++nArchs;
            break;

          case 'c':
            if (kernelCacheFilename) {
                fprintf(stderr, "an output filename has already been set\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }

            if (optind >= argc) {
                need_default_kernelcache_info = true;
            } else {
                kernelCacheFilename = argv[optind++];
            }
            break;

          case 'd':
            debug_mode = true;
            break;

          case 'e':
            if (mkextFilename) {
                fprintf(stderr, "an output filename has already been set\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            mkextFilename = "/System/Library/Extensions.mkext";
            CFArrayAppendValue(repositoryDirectories, kKXSystemExtensionsFolder);
            break;

          case 'f':
            forceUpdate = true;
            break;

          case 'F':
            forkExit = true;
            break;

          case 'h':
            usage(2);
            exit_code = 0;
            goto finish;

          case 'K':
            kernel_file = optarg;
            break;

          case 'k':
            repositoryCaches = true;
            break;

          case 'l':
            local_for_repositories = true;
            break;

          case 'L':
            local_for_all = true;
            break;

          case 'm':
            if (mkextFilename) {
                fprintf(stderr, "an mkext filename has already been set\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            mkextFilename = optarg;
            break;

          case 'n':
            network_for_repositories = true;
            break;

          case 'N':
            network_for_all = true;
            break;

          case 'r':
	        include_kernel_requests = true;
	        break;

          case 's':
            safeboot_for_repositories = true;
            break;

          case 'S':
            safeboot_for_all = true;
            break;

          case 't':
            do_tests = true;
            break;

          case 'u':
          case 'U':
            if (updateRoot) {
                fprintf(stderr, "a volume to update has already been set\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            updateRoot = optarg;
            if (optchar == 'U') {
                expectUpToDate = true;
            }
            break;
	  
          case 'v':
            {
                const char * next;

                if (g_verbose_level > kKXKextManagerLogLevelDefault) {
                    fprintf(stderr, "duplicate use of -v option\n\n");
                    usage(0);
                    exit_code = 1;
                    goto finish;
                }
                if (optind >= argc) {
                    g_verbose_level = kKXKextManagerLogLevelBasic;
                } else {
                    next = argv[optind];
                    if ((next[0] == '1' || next[0] == '2' ||
                         next[0] == '3' || next[0] == '4' ||
                         next[0] == '5' || next[0] == '6') &&
                         next[1] == '\0') {

                        g_verbose_level = atoi(next);
                        optind++;
                    } else {
                        g_verbose_level = kKXKextManagerLogLevelBasic;
                    }
                }
            }
            break;

          case 'z':
            pretend_authentic = true;
            break;

          case '?':
            usage(0);
            exit_code = 1;	// should be EX_USAGE (sysexits.h)
            goto finish;

          default:
            fprintf(stderr, "unknown option -%c\n", optchar);
            usage(0);
            exit_code = 1;
            goto finish;
        }
    }

   /* If no kernel file was specified, use /mach_kernel. For prelinked kernel
    * generation, its UUID must match that of the running kernel. This is always
    * the case unless doing development and booting from a non-default kernel,
    * or after the kernel has been replaced (in which case a prelinked kernel
    * can still be generated by a developer if they specify the kernel file
    * explicitly).
    */
    if (!kernel_file) {
        kernel_file = default_kernel_file;
        if (need_default_kernelcache_info && include_kernel_requests) {
            mach_port_t host_port = MACH_PORT_NULL;
            char * running_uuid = NULL;  // must free
            unsigned int running_uuid_size;
            const char * reason = NULL;

           /* Clear these flags for now, they're set back to true if we can
            * verify the UUID.
            */
            need_default_kernelcache_info = false;
            include_kernel_requests = false;

            host_port = mach_host_self(); /* must be privileged to work */
            if (!MACH_PORT_VALID(host_port)) {
                reason = "can't get host port to retrieve kernel symbols\n";
            } else if (1 == copyKextUUID(host_port, NULL, NULL,
                &running_uuid, &running_uuid_size)) {
                if (1 == machoFileMatchesUUID(kernel_file,
                    running_uuid, running_uuid_size)) {

                    need_default_kernelcache_info = true;
                    include_kernel_requests = true;
                } else {
                    reason = "/mach_kernel's UUID doesn't match running kernel\n";
                }
            }
            if (!need_default_kernelcache_info) {
                fprintf(stderr, "skipping prelinked kernel generation; %s\n",
                    reason);
            }
            if (MACH_PORT_NULL != host_port) {
                mach_port_deallocate(mach_task_self(), host_port);
            }
            if (running_uuid) free(running_uuid);
        }
    }

   /* Get the default kernel timestamps before mucking with kernelcache info.
    */
    have_kernel_time     = (0 == stat(kernel_file,       &kernel_stat_buf));
    have_extensions_time = (0 == stat(source_extensions, &extensions_stat_buf));

    if (have_kernel_time || have_extensions_time) {
        cacheFileTimes = _cacheFileTimes;
        if (!have_kernel_time 
            || (have_extensions_time && (extensions_stat_buf.st_mtime > kernel_stat_buf.st_mtime))) {
            
            TIMESPEC_TO_TIMEVAL(&cacheFileTimes[0], &extensions_stat_buf.st_atimespec);
            TIMESPEC_TO_TIMEVAL(&cacheFileTimes[1], &extensions_stat_buf.st_mtimespec);
        } else {
            TIMESPEC_TO_TIMEVAL(&cacheFileTimes[0], &kernel_stat_buf.st_atimespec);
            TIMESPEC_TO_TIMEVAL(&cacheFileTimes[1], &kernel_stat_buf.st_mtimespec);
        }
        cacheFileTimes[1].tv_sec++;
    }

   /* If -c was given as the last argument, get the default kernelcache info
    * using information gleaned from the previous default kernel file stats.
    */
    if (need_default_kernelcache_info) {
        // default args for kextd's usage
        int stat_result;
        Boolean make_directory = false;

        kernelCacheFilename = kPrelinkedKernelDir "/" kPrelinkedKernelBase;
        struct stat cache_stat_buf;

        stat_result = stat(kPrelinkedKernelDir, &cache_stat_buf);
        if (0 == stat_result && !S_ISDIR(cache_stat_buf.st_mode)) {
            kextd_error_log("%s exists as plain file; removing", kPrelinkedKernelDir);
            if (-1 == unlink(kPrelinkedKernelDir)) {
                kextd_error_log("failed to remove %s: %s", kPrelinkedKernelDir, strerror(errno));
                exit_code = 1;
                goto finish;
            }
            make_directory = true;
        }
        if (-1 == stat_result) {
            if (ENOENT == errno) {
                make_directory = true;
            } else {
                kextd_error_log("error checking %s: %s",
                    kPrelinkedKernelDir, strerror(errno));
            }
        }

        if (make_directory) {
            if (-1 == mkdir(kPrelinkedKernelDir, 0755)) {
                kextd_error_log("error creating %s: %s",
                    kPrelinkedKernelDir, strerror(errno));
                exit_code = 1;
                goto finish;
            }
        }

        // Make sure we scan the standard Extensions folder.
        CFArrayAppendValue(repositoryDirectories,
            kKXSystemExtensionsFolder);
        // Make sure we scan the ROM Extensions folder.
        if (0 == stat(kKXROMExtensionsFolder, &rom_extensions_stat_buf))
            CFArrayAppendValue(repositoryDirectories, CFSTR(kKXROMExtensionsFolder));

        if (!platform_name_root_path.platform_name[0]) {
            // need more than the minimal set
            local_for_repositories = true;
        }

        if (!include_kernel_requests) {
            // this cache isn't tied to a config
            platform_name_root_path.root_path[0] = platform_name_root_path.platform_name[0] = 0;
        }

        if (platform_name_root_path.platform_name[0] || platform_name_root_path.root_path[0])
        {
            sprintf(kernelCacheBuffer, "%s.%08X", 
                kernelCacheFilename,
                NXSwapHostIntToBig(local_adler32((u_int8_t *) &platform_name_root_path, 
                                    sizeof(platform_name_root_path))));
            kernelCacheFilename = kernelCacheBuffer;
        }

        if ((0 == stat(kernelCacheFilename, &cache_stat_buf))
            && have_kernel_time && have_extensions_time)
        {
            if ((cache_stat_buf.st_mtime > kernel_stat_buf.st_mtime)
             &&  (cache_stat_buf.st_mtime > extensions_stat_buf.st_mtime)
             &&  (cache_stat_buf.st_mtime == cacheFileTimes[1].tv_sec))
                cache_looks_uptodate = true;
        }

        if (-1 == stat(SA_HAS_RUN_PATH, &cache_stat_buf)) {
            if (g_verbose_level >= 1) {
                verbose_log("SetupAssistant not yet run");
            }
            exit(0);
        }
    }
    
   /* Update argc, argv based on option processing.
    */
    argc -= optind;
    argv += optind;

   /*****
    * Check for bad combinations of options.
    */
    if (!mkextFilename && !kernelCacheFilename && !repositoryCaches
	    && !updateRoot) {
        fprintf(stderr, "no work to do; one of -m, -c, -k, or -u/-U must be specified\n");
        usage(1);
        exit_code = 1;
        goto finish;
    }
    
    if (forceUpdate && (!updateRoot || expectUpToDate)) {
        fprintf(stderr, "-f is only allowed with -u\n");
        usage(1);
        exit_code = 1;
        goto finish;
    }

   /*****
    * Record the kext/directory names from the command line.
    */
    for (i = 0; i < argc; i++) {
        CFStringRef argString = CFStringCreateWithCString(kCFAllocatorDefault,
              argv[i], kCFStringEncodingUTF8);
        if (!argString) {
            fprintf(stderr, "memory allocation failure\n");
            exit_code = 1;
            goto finish;
        }

        // FIXME: Use a more robust test here?
        if (CFStringHasSuffix(argString, CFSTR(".kext")) ||
            CFStringHasSuffix(argString, CFSTR(".kext/")) ) {
            CFArrayAppendValue(kextNames, argString);
        }
        else
        {
            if (!CFArrayGetCount(repositoryDirectories))
            {
                source_extensions = argv[i];
                have_extensions_time = (0 == stat(source_extensions, &extensions_stat_buf));
                if (have_kernel_time || have_extensions_time)
                {
                    cacheFileTimes = _cacheFileTimes;
                    if (!have_kernel_time 
                        || (have_extensions_time && (extensions_stat_buf.st_mtime > kernel_stat_buf.st_mtime)))
                    {
                        TIMESPEC_TO_TIMEVAL(&cacheFileTimes[0], &extensions_stat_buf.st_atimespec);
                        TIMESPEC_TO_TIMEVAL(&cacheFileTimes[1], &extensions_stat_buf.st_mtimespec);
                    }
                    else
                    {
                        TIMESPEC_TO_TIMEVAL(&cacheFileTimes[0], &kernel_stat_buf.st_atimespec);
                        TIMESPEC_TO_TIMEVAL(&cacheFileTimes[1], &kernel_stat_buf.st_mtimespec);
                    }
                    cacheFileTimes[1].tv_sec++;
                }
            }
            CFArrayAppendValue(repositoryDirectories, argString);
        }
        CFRelease(argString);
    }

    if (!updateRoot && CFArrayGetCount(kextNames) == 0 &&
        CFArrayGetCount(repositoryDirectories) == 0) {

        fprintf(stderr, "no kernel extensions or directories specified\n\n");
        usage(0);
        exit_code = 1;
        goto finish;
    }

    if (forkExit) {
        if (g_verbose_level >= kKXKextManagerLogLevelBasic) {
            verbose_log("running in low-priority background mode");
        }

        setpriority(PRIO_PROCESS, getpid(), 20); // run at really low priority
        if (include_kernel_requests) {
            kern_return_t    kern_result;
            mach_timespec_t  waitTime = { 40, 0 };
            
            kern_result = IOKitWaitQuiet(kIOMasterPortDefault, &waitTime);
            if (kern_result == kIOReturnTimeout) {
                fprintf(stderr, "IOKitWaitQuiet() timed out");
            } else if (kern_result != kIOReturnSuccess) {
                fprintf(stderr, "IOKitWaitQuiet() failed with result code %x",
                        kern_result);
            }
        }
    }

    if (updateRoot) {
        struct stat sb;
        
        if (mkextFilename || kernelCacheFilename || repositoryCaches) {
            fprintf(stderr, "-u/-U (auto-update) incompatible with other flags\n");
            usage(0);
            exit_code = 1;
            goto finish;
        }
        
        if (stat(updateRoot, &sb)) {
            warn("%s", updateRoot);
            exit_code = 1;
        } else {
            exit_code=updateBoots(updateRoot,argc,argv,forceUpdate,expectUpToDate);
        }
        
        goto finish;
    }
    
   /* Try a lock on the volume for the mkext being updated.
    */
    if (mkextFilename && !getenv("_com_apple_kextd_skiplocks")) {
	result = takeVolumeForPath(mkextFilename);
        if (result) {
            goto finish;
        }
    }

    if (include_kernel_requests)
	get_catalog_demand_lists(&kernel_requests, &kernel_cache_misses);

    if (cache_looks_uptodate)
    {
       /* Only log these particular bits if it's likely an interactive run. We don't want
        * this spew in the system logs.
        */
        if (!forkExit) {
            CFShow(kernel_cache_misses);
            CFShow(kernel_requests);
        }
	if ((!kernel_cache_misses || !kernel_requests)
	    || (!CFSetGetCount(kernel_cache_misses))
	    || (CFSetGetCount(kernel_cache_misses) == CFSetGetCount(kernel_requests)))
	{
	    if (g_verbose_level >= kKXKextManagerLogLevelBasic) {
		verbose_log("cache %s up to date", kernelCacheFilename);
	    }
	    exit(0);
	}
    }

   /*****
    * Set up the kext manager.
    */

    theKextManager = KXKextManagerCreate(kCFAllocatorDefault);
    if (!theKextManager) {
        fprintf(stderr, "can't allocate kernel extension manager\n");
        exit_code = 1;
        goto finish;
    }

    result = KXKextManagerInit(theKextManager, load_in_task,
        false /* safeBoot */);
    if (result != kKXKextManagerErrorNone) {
        fprintf(stderr, "can't initialize kernel extension manager (%s)\n",
            KXKextManagerErrorStaticCStringForError(result));
        exit_code = 1;
        goto finish;
    }

    KXKextManagerSetPerformsFullTests(theKextManager, do_tests);
    KXKextManagerSetPerformsStrictAuthentication(theKextManager, true);
    KXKextManagerSetLogLevel(theKextManager, g_verbose_level);
    KXKextManagerSetLogFunction(theKextManager, &verbose_log);
    KXKextManagerSetErrorLogFunction(theKextManager, &error_log);
    KXKextManagerSetUserVetoFunction(theKextManager, &user_approve);
    KXKextManagerSetUserApproveFunction(theKextManager, &user_approve);
    KXKextManagerSetUserInputFunction(theKextManager, &user_input);

   /*****
    * Disable clearing of relationships until we're done putting everything
    * together.
    */
    KXKextManagerDisableClearRelationships(theKextManager);

   /*****
    * Add the extensions folders specified with -r to the manager.
    * And collect all their kexts--good and bad--into the repositoryKexts
    * array.
    */
    count = CFArrayGetCount(repositoryDirectories);
    for (i = 0; i < count; i++) {
        CFURLRef directoryURL = NULL;          // must release
        KXKextRepositoryRef repository = NULL; // don't release
        CFArrayRef candidateKexts = NULL;      // must release
        CFArrayRef badKexts = NULL;            // must release

        CFStringRef directory = (CFStringRef)CFArrayGetValueAtIndex(
            repositoryDirectories, i);
        directoryURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                directory, kCFURLPOSIXPathStyle, true);
        if (!directoryURL) {
            char repositoryPath[MAXPATHLEN];
            if (!CFStringGetCString(directory, repositoryPath,
                sizeof(repositoryPath) / sizeof(char), kCFStringEncodingUTF8)) {

                fprintf(stderr, "string conversion error\n");

            } else {
                fprintf(stderr, "'%s': no such directory\n", repositoryPath);
            }
            exit_code = 1;

            goto finish;
        }

        result = KXKextManagerAddRepositoryDirectory(theKextManager,
            directoryURL, true /* scanForKexts */,
            false /* use_repository_caches */, &repository);
        if (result != kKXKextManagerErrorNone) {
            fprintf(stderr, "can't add repository (%s).\n",
                KXKextManagerErrorStaticCStringForError(result));
	    continue;
        }

        CFArrayAppendValue(repositories, repository);

        candidateKexts = KXKextRepositoryCopyCandidateKexts(repository);
        if (candidateKexts) {
            CFArrayAppendArray(repositoryKexts, candidateKexts,
                CFRangeMake(0, CFArrayGetCount(candidateKexts)));
            CFRelease(candidateKexts);
            candidateKexts = NULL;
        }

        badKexts = KXKextRepositoryCopyBadKexts(repository);
        if (badKexts) {
            CFArrayAppendArray(repositoryKexts, badKexts,
                CFRangeMake(0, CFArrayGetCount(badKexts)));
            CFRelease(badKexts);
            badKexts = NULL;
        }

        CFRelease(directoryURL);
        directoryURL = NULL;
    }

   /*****
    * Add each kext named on the command line to the manager and collect
    * their names in the kextNamesToUse array.
    */
    if (addKextsToManager(theKextManager,kextNames,kextNamesToUse,do_tests)<1){
        exit_code = 1;
        goto finish;
    }

    if (pretend_authentic) {
        // Yes, do this even if do_tests is true; -tz means fake authentication.
        KXKextManagerMarkKextsAuthentic(theKextManager);
    } else if (do_tests) {
        KXKextManagerAuthenticateKexts(theKextManager);
    }

    KXKextManagerEnableClearRelationships(theKextManager);

    KXKextManagerCalculateVersionRelationships(theKextManager);
    KXKextManagerResolveAllKextDependencies(theKextManager);

   /*****
    * Write kextcache files for repositories.
    */
    if (repositoryCaches) {
        count = CFArrayGetCount(repositories);
        for (i = 0; i < count; i++) {
            KXKextRepositoryRef repository = (KXKextRepositoryRef)
                CFArrayGetValueAtIndex(repositories, i);
            KXKextManagerError kmResult =  KXKextRepositoryWriteCache(
                repository, NULL);
            if (kmResult != kKXKextManagerErrorNone) {
                // write function prints error message
                exit_code = 1;
                goto finish;
            }
        }
    }

   /*****
    * Do the cache files.
    */
    if (!mkextFilename && !kernelCacheFilename) {
        goto finish;
    }

   /*****
    * Get KXKextRef objects for each of the kexts named on the command line.
    */
    count = CFArrayGetCount(kextNamesToUse);
    for (i = 0; i < count; i++) {
        CFStringRef kextName = CFArrayGetValueAtIndex(kextNamesToUse, i);
        CFURLRef kextURL = NULL;  // must release
        KXKextRef thisKext = NULL;

        kextURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
            kextName, kCFURLPOSIXPathStyle, true);
        if (!kextURL) {
            fprintf(stderr, "memory allocation failure\n");
            exit_code = 1;
            goto finish;
        }

        thisKext = KXKextManagerGetKextWithURL(theKextManager,
            kextURL);
        CFRelease(kextURL);
        if (!thisKext) {
            fprintf(stderr, "internal error; kext not found\n");
            exit_code = 1;
            goto finish;
        }

        CFArrayAppendValue(namedKexts, thisKext);
    }

    /* If no architectures are specified on the command line,
     * create one kext file that contains everything.
     * If one architecture is specified, create one mkext file
     * that is thinned to the specified architecture.
     * If multiple architectures are specified,
     * create a fat file containing multiple thin mkext files.
     */

    if (nArchs == 0) {
        archs[nArchs++] = &archAny;
    }

    if (kernelCacheFilename)
    {
	KXKextManagerError err;
	const NXArchInfo * hostArch;
	
	hostArch = NXGetLocalArchInfo();
	if (!hostArch) {
	    fprintf(stderr, "can't determine the host architecture\n");
	    exit_code = 1;
	    // still create mkext if requested
	    goto domkext;
	}

	if((nArchs > 1) ||
	  ((archs[0]->cputype != CPU_TYPE_ANY) && (archs[0]->cputype != hostArch->cputype))) {
	    fprintf(stderr, "can't create fat cache\n");
	    exit_code = 1;
	    // still create mkext if requested
	    goto domkext;
	}

        CFDictionaryRemoveAllValues(checkDictionary);
        collectKextsForMkextCache(namedKexts, checkDictionary,
                                  hostArch->cputype, hostArch->cpusubtype,
                                  local_for_all, network_for_all, safeboot_for_all,
				  kernel_requests,
                                  g_verbose_level, do_tests);
        collectKextsForMkextCache(repositoryKexts, checkDictionary,
                                  hostArch->cputype, hostArch->cpusubtype,
                                  local_for_repositories || local_for_all,
                                  network_for_repositories || network_for_all,
                                  safeboot_for_repositories || safeboot_for_all,
				  kernel_requests,
                                  g_verbose_level, do_tests);

	all_plists = false;
	if (!include_kernel_requests) {
	    // this cache isn't tied to a config
	    platform_name_root_path.root_path[0] = platform_name_root_path.platform_name[0] = 0;
	    all_plists = !(    local_for_repositories || local_for_all
			    || network_for_repositories || network_for_all
			    || safeboot_for_repositories || safeboot_for_all);
	}
	err = PreLink(theKextManager, checkDictionary, kernelCacheFilename, cacheFileTimes, kernel_file,
			platform_name_root_path.platform_name, platform_name_root_path.root_path,
			kernel_requests, all_plists,
			hostArch, // archs[0],
			g_verbose_level, debug_mode);

	if (kKXKextManagerErrorNone != err)
	{
	    exit_code = 1;
	    // still create mkext if requested
	    goto domkext;
	}
    }

domkext:
    if (!mkextFilename) {
        goto finish;
    }

    if (strlcpy(temp_file, mkextFilename, sizeof(temp_file)) >= sizeof(temp_file)) {
        fprintf(stderr, "cache file name too long: %s",
            mkextFilename);
        exit_code = 1;
        goto finish;
    }
    if (strlcat(temp_file, ".XXXX", sizeof(temp_file)) >= sizeof(temp_file)) {
        fprintf(stderr, "cache file name too long: %s",
            mkextFilename);
        exit_code = 1;
        goto finish;
    }

    fd = mkstemp(temp_file);
    if (-1 == fd) {
        fprintf(stderr, "can't create %s - %s\n", temp_file,
            strerror(errno));
        exit_code = 1;
        goto finish;  // FIXME: mkextcache used to exit with EX_CANTCREAT
    }

    output_filename = temp_file;

   /* Set the umask to get it, then set it back to iself. Wish there were a
    * better way to query it.
    */
    real_umask = umask(0);
    umask(real_umask);

    if (-1 == fchmod(fd, MKEXT_PERMS & ~real_umask)) {
        fprintf(stderr, "%s - %s\n",
            temp_file, strerror(errno));
    }

    struct fat_header fatHeader;
    struct fat_arch fatArchs[kMaxArchs];
    unsigned long fat_offset = 0;
    ssize_t bytes_written = 0;

    if (nArchs > 1 || explicit_arch) {
        fat_offset = sizeof(struct fat_header) + (sizeof(struct fat_arch) * nArchs);
        lseek(fd, fat_offset, SEEK_SET);
    }

    for (i=0; i<nArchs; i++) {
        fatArchs[i].cputype = NXSwapHostLongToBig(archs[i]->cputype);
        fatArchs[i].cpusubtype = NXSwapHostLongToBig(archs[i]->cpusubtype);
        fatArchs[i].offset = NXSwapHostLongToBig(fat_offset);

        if (g_verbose_level >= kKXKextManagerLogLevelBasic) {
            verbose_log("processing architecture %s", archs[i]->name);
        }
        CFDictionaryRemoveAllValues(checkDictionary);
        collectKextsForMkextCache(namedKexts, checkDictionary,
            archs[i]->cputype, archs[i]->cpusubtype,
            local_for_all, network_for_all, safeboot_for_all,
            NULL /*kernel_requests*/,
            g_verbose_level, do_tests);
        collectKextsForMkextCache(repositoryKexts, checkDictionary,
            archs[i]->cputype, archs[i]->cpusubtype,
            local_for_repositories || local_for_all,
            network_for_repositories || network_for_all,
            safeboot_for_repositories || safeboot_for_all,
            NULL /*kernel_requests*/,
            g_verbose_level, do_tests);

        bytes_written = createMkextArchive(fd, checkDictionary, output_filename,
            archs[i]->name, archs[i]->cputype,
            archs[i]->cpusubtype, g_verbose_level);

        if (bytes_written < 0) {
            exit_code = 1;
            goto finish;
        }
        fat_offset += bytes_written;

        if (mkextArchiveSizeOverLimit(bytes_written, archs[i]->cputype)) {
            fprintf(stderr, "archive would be too large; aborting\n");
            exit_code = 1;
            goto finish;
        }

        fatArchs[i].size = NXSwapHostLongToBig(bytes_written);
        fatArchs[i].align = NXSwapHostLongToBig(0);
    }

    if (nArchs > 1 || explicit_arch) {
        lseek(fd, 0, SEEK_SET);
        fatHeader.magic = NXSwapHostLongToBig(FAT_MAGIC);
        fatHeader.nfat_arch = NXSwapHostLongToBig(nArchs);
        bytes_written = write(fd, &fatHeader, sizeof(fatHeader));
        if (bytes_written != sizeof(fatHeader)) {
            perror("write");
            exit_code = 1;
            goto finish;
        }

        for (i=0; i<nArchs; i++) {
            bytes_written = write(fd, &fatArchs[i], sizeof(fatArchs[i]));
            if (bytes_written != sizeof(fatArchs[i])) {
                perror("write");
                exit_code = 1;
                goto finish;
            }
        }
    }

    close(fd);
    fd = -1;

    if (have_extensions_time) {
        struct timespec mod_time = extensions_stat_buf.st_mtimespec;
        if ((0 == stat(source_extensions, &extensions_stat_buf))
          && ((mod_time.tv_sec != extensions_stat_buf.st_mtimespec.tv_sec)
              || (mod_time.tv_nsec != extensions_stat_buf.st_mtimespec.tv_nsec)))
        {
            fprintf(stderr, "cache stale - not creating %s\n", mkextFilename);
            exit_code = 1;
            goto finish;
        }

        TIMESPEC_TO_TIMEVAL(&cacheFileTimes[0], &extensions_stat_buf.st_atimespec);
        TIMESPEC_TO_TIMEVAL(&cacheFileTimes[1], &extensions_stat_buf.st_mtimespec);
        cacheFileTimes[1].tv_sec++;
    }

    // move it to the final destination
    if (-1 == rename(output_filename, mkextFilename)) {
        fprintf(stderr, "%s - %s\n", mkextFilename, strerror(errno));
        exit_code = 1;
        goto finish;
    }
    output_filename = NULL;

    // give the cache file the mod time of the source files when kextcache started
    if (cacheFileTimes && (-1 == utimes(mkextFilename, cacheFileTimes))) {
        fprintf(stderr, "%s - %s\n", mkextFilename,
            strerror(errno));
        exit_code = 1;
        goto finish;
    }

finish:

   /*****
    * Clean everything up.
    */

    if (-1 != fd) close(fd);
    if (output_filename) {
        if (-1 == unlink(output_filename)) {
            fprintf(stderr, "%s - %s\n", output_filename,
                strerror(errno));
        }
    }
    if (repositoryDirectories) CFRelease(repositoryDirectories);
    if (kextNames)             CFRelease(kextNames);
    if (kextNamesToUse)        CFRelease(kextNamesToUse);
    if (namedKexts)            CFRelease(namedKexts);
    if (repositoryKexts)       CFRelease(repositoryKexts);
    if (checkDictionary)       CFRelease(checkDictionary);

    if (theKextManager)        CFRelease(theKextManager);
    putVolumeForPath(mkextFilename, exit_code);	    // handles not locked

    exit(exit_code);
    return exit_code;
}

/*******************************************************************************
* takeVolumeForPath turns the path into a volume UUID and locks with kextd
*******************************************************************************/
// upstat() stat()s "up" the path if a file doesn't exist
static int upstat(const char *path, struct stat *sb, struct statfs *sfs)
{
    int rval = ELAST+1;
    char buf[PATH_MAX], *tpath = buf;
    struct stat defaultsb;

    if (strlcpy(buf, path, PATH_MAX) > PATH_MAX)  	goto finish;

    if (!sb)	sb = &defaultsb;
    while ((rval = stat(tpath, sb)) == -1 && errno == ENOENT) {
	// "." and "/" should always exist, but you never know
	if (tpath[0] == '.' && tpath[1] == '\0')  goto finish;
	if (tpath[0] == '/' && tpath[1] == '\0')  goto finish;
	tpath = dirname(tpath);	    // Tiger's dirname() took const char*
    }

    // call statfs if the caller needed it
    if (sfs)
	rval = statfs(tpath, sfs);

finish:
    if (rval)
	warn("couldn't find volume for %s", path);

    return rval;
}

#define COMPILE_TIME_ASSERT(pred)   switch(0){case 0:case pred:;}
static int getVolumeUUID(const char *volPath, uuid_t vol_uuid)
{
    int rval = ELAST + 1;
    DADiskRef dadisk = NULL;
    CFDictionaryRef dadesc = NULL;
#if TARGET_OS_EMBEDDED
    goto finish;
#else
    CFUUIDRef volUUID;      // just a reference into the dict
    CFUUIDBytes uuidBytes;
COMPILE_TIME_ASSERT(sizeof(CFUUIDBytes) == sizeof(uuid_t));

    dadisk = createDiskForMount(NULL, volPath);
    if (!dadisk)        goto finish;
    dadesc = DADiskCopyDescription(dadisk);
    if (!dadesc)        goto finish;
    volUUID = CFDictionaryGetValue(dadesc, kDADiskDescriptionVolumeUUIDKey);
    if (!volUUID)       goto finish;
    uuidBytes = CFUUIDGetUUIDBytes(volUUID);
    memcpy(vol_uuid, &uuidBytes.byte0, sizeof(uuid_t));   // sizeof(vol_uuid)?

    rval = 0;
#endif
finish:
    if (dadesc)     CFRelease(dadesc);
    if (dadisk)     CFRelease(dadisk);

    if (rval)
	warnx("%s: couldn't get volume UUID");
    return rval;
}


// takeVolumeForPaths ensures all paths are on the given volume, then locks
int takeVolumeForPaths(char *volPath, int filec, const char *files[])
{
    int i, bsderr, rval = ELAST + 1;
    struct stat volsb;

    bsderr = stat(volPath, &volsb);
    if (bsderr)  goto finish;

    for (i = 0; i < filec; i++) {
	struct stat sb;

	if ((bsderr = upstat(files[i], &sb, NULL)))
	    goto finish;

	// better be on the same device as the volume
	if (sb.st_dev != volsb.st_dev) {
	    warnx("can't lock: %s, %s on different volumes", volPath, files[i]);
	    errno = EPERM;
	    bsderr = -1;
	    goto finish;
	}
    }

    rval = takeVolumeForPath(volPath);

finish:
    if (bsderr) {
	warn("couldn't lock paths on volume %s", volPath);
	rval = bsderr;
    } 

    return rval;
}

// can return success if a lock isn't needed
// can return failure if sLockPort is already in use
#define WAITFORLOCK 1
static int takeVolumeForPath(const char *path)
{
    int rval = ELAST + 1;
    kern_return_t macherr = KERN_SUCCESS;
    int lckres = 0;
    struct statfs sfs;
    const char *volPath;
    mach_port_t taskport = MACH_PORT_NULL;

    if (sLockPort) {
        return EALREADY;        // only support one lock at a time
    }

    if (getuid() != 0) {
	// kextd shouldn't be watching anything you can touch
	// and ignores locking requests from non-root anyway
	rval = 0;
	goto finish;
    }

    // look up kextd port if not cached
    // XX if there's a way to know kextd isn't running, we could skip
    // unnecessarily bringing it up in the boot-time case (5108882?).
    if (!sKextdPort) {
        macherr=bootstrap_look_up(bootstrap_port,KEXTD_SERVER_NAME,&sKextdPort);
        if (macherr)  goto finish;
    }

    if ((rval = upstat(path, NULL, &sfs)))	goto finish;
    volPath = sfs.f_mntonname;

    // get the volume's UUID
    if (getVolumeUUID(volPath, s_vol_uuid)) {
	goto finish;
    }
    
    // allocate a port to pass (in case we die -- kernel cleans up on exit())
    taskport = mach_task_self();
    if (taskport == MACH_PORT_NULL)  goto finish;
    macherr = mach_port_allocate(taskport, MACH_PORT_RIGHT_RECEIVE, &sLockPort);
    if (macherr)  goto finish;

    // try to take the lock; warn if it's busy and then wait for it
    // X kextcache -U, if it is going to lock at all, needs only WAITFORLOCK
    macherr = kextmanager_lock_volume(sKextdPort, sLockPort, s_vol_uuid,
	    			      !WAITFORLOCK, &lckres);
    if (macherr) 	goto finish;
    if (lckres == EBUSY) {
	warnx("%s locked; waiting for lock", volPath);
	macherr = kextmanager_lock_volume(sKextdPort, sLockPort, s_vol_uuid,
					  WAITFORLOCK, &lckres);
	if (macherr) 	goto finish;
	if (lckres == 0)
	    warnx("proceeding");
    }
    
    // kextd might not be watching this volume 
    // or might be telling us that it went away (not watching any more)
    // so we set our success to the existance of the volume's root
    if (lckres == ENOENT) {
	struct stat sb;
	rval = stat(volPath, &sb);
    } else {
	rval = lckres;
    }

finish:	
    if (sLockPort != MACH_PORT_NULL && (lckres != 0 || macherr)) {
	mach_port_mod_refs(taskport, sLockPort, MACH_PORT_RIGHT_RECEIVE, -1);
	sLockPort = MACH_PORT_NULL;
    }

    /* XX needs unraveling XX */
    // if kextd isn't competing with us, then we didn't need the lock
    if (macherr == BOOTSTRAP_UNKNOWN_SERVICE) {
	rval = 0;
    } else if (macherr) {
	warnx("couldn't lock %s: %s (%d)", path,
	    mach_error_string(macherr), macherr);
	rval = macherr;
    } else {
	// dump rval
	if (rval == -1) {
	    warn("couldn't lock %s", path);
	    rval = errno;
	} else if (rval) {
	    // lckres == EAGAIN should get here
	    warnx("couldn't lock %s: %s", path, strerror(rval));
	}
    }

    return rval;
}


/*******************************************************************************
* putVolumeForPath will unlock the relevant volume, passing 'status' to
* inform kextd whether we succeded, failed, or just need more time
*******************************************************************************/
int putVolumeForPath(const char *path, int status)
{
    int rval = KERN_SUCCESS;

    // if not locked, don't sweat it
    if (sLockPort == MACH_PORT_NULL)
	goto finish;

    rval = kextmanager_unlock_volume(sKextdPort, sLockPort, s_vol_uuid, status);

    // tidy up; the server will clean up its stuff if we die prematurely
    mach_port_mod_refs(mach_task_self(),sLockPort,MACH_PORT_RIGHT_RECEIVE,-1);
    sLockPort = MACH_PORT_NULL;

finish:
    if (rval) {
	warnx("couldn't unlock volume for %s: %s (%d)",
	    path, mach_error_string(rval), rval);
    }

    return rval;
}

/*******************************************************************************
*
*******************************************************************************/

static void get_catalog_demand_lists(CFMutableSetRef * kernel_requests,
				      CFSetRef * kernel_cache_misses)
{
    kern_return_t kr;
    char *   propertiesBuffer;
    uint32_t loaded_bytecount;

    kr = IOCatalogueGetData(MACH_PORT_NULL, kIOCatalogGetModuleDemandList,
			    &propertiesBuffer, &loaded_bytecount);
    if (kIOReturnSuccess == kr)
    { 
	CFSetRef set;
	set = (CFSetRef)
		IOCFUnserialize(propertiesBuffer, kCFAllocatorDefault, 0, 0);
	vm_deallocate(mach_task_self(), (vm_address_t) propertiesBuffer, loaded_bytecount);
	*kernel_requests = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, set);
	CFRelease(set);
    }

    kr = IOCatalogueGetData(MACH_PORT_NULL, kIOCatalogGetCacheMissList,
			    &propertiesBuffer, &loaded_bytecount);
    if (kIOReturnSuccess == kr)
    { 
	if (g_verbose_level > kKXKextManagerLogLevelDefault)
	    verbose_log("cache misses:\n%s", propertiesBuffer);
	*kernel_cache_misses = (CFSetRef)
		IOCFUnserialize(propertiesBuffer, kCFAllocatorDefault, 0, 0);
	vm_deallocate(mach_task_self(), (vm_address_t) propertiesBuffer, loaded_bytecount);
    }
}

/*******************************************************************************
*
*******************************************************************************/
// XX can stop passing verbose_level to these functions
static void addKextForMkextCache(
    KXKextRef theKext,
    CFMutableDictionaryRef checkDictionary,
    cpu_type_t archCPU,
    cpu_subtype_t archSubtype,
    Boolean local,
    Boolean network,
    Boolean safeboot,
    CFSetRef kernel_requests,
    int verbose_level,
    Boolean do_tests)
{
    Boolean include_it = true;  // assume success
    Boolean do_children = true;

    CFStringRef kextName = NULL; // must release
    char kext_name[MAXPATHLEN] = "";

    CFBundleRef kextBundle = NULL;        // don't release
    CFStringRef requiredString = NULL;    // don't release

    CFURLRef executableURL = NULL;        // must release
    char executable_path[PATH_MAX];

    int fd = -1;         // sentinel value for close() call at finish
    caddr_t machO = (caddr_t)-1; // sentinel value for munmap() call at finish
    off_t machOSize = 0;

    kextName = KXKextCopyAbsolutePath(theKext);
    if (!kextName) {
        fprintf(stderr, "memory allocation failure\n");
        exit(1);
    }

    if (!CFStringGetCString(kextName, kext_name,
        sizeof(kext_name) / sizeof(char), kCFStringEncodingUTF8)) {

        fprintf(stderr, "string conversion failure\n");
        include_it = false;
        do_children = false;
        goto finish;
    }

    if (CFDictionaryGetValue(checkDictionary, kextName)) {
        include_it = false;
        do_children = true;
        goto finish;
    }

    if (!KXKextIsValid(theKext)) {
        fprintf(stderr, "kernel extension %s is not valid; "
             "skipping it and any plugins\n", kext_name);
        if (do_tests) {
            fprintf(stderr, "kext diagnostics:\n");
            KXKextPrintDiagnostics(theKext, stderr);
        }
        include_it = false;
        do_children = false;
        goto finish;
    }

    kextBundle = KXKextGetBundle(theKext);

    include_it = false;
    do_children = true;

    if (kernel_requests) {
	CFStringRef bundleIdentifier = CFBundleGetIdentifier(kextBundle);
	if (!bundleIdentifier) {
	    fprintf(stderr, "can't get identifier for kext\n");
	    do_children = false;
	    goto finish;
	}
	include_it = CFSetContainsValue(kernel_requests, bundleIdentifier);
    } else if (!local && !network && !safeboot)
	include_it = true;

    if (!include_it && (local || network || safeboot)) {
        requiredString = CFBundleGetValueForInfoDictionaryKey(kextBundle,
            CFSTR("OSBundleRequired"));
    
        if (!requiredString) {

            if (verbose_level >= kKXKextManagerLogLevelDetails) {
                verbose_log("skipping bundle %s; no OSBundleRequired key "
                    "(still checking plugins)",
                    kext_name);
            }
            include_it = false;
            do_children = true;
            goto finish;

        } else if (CFStringCompare(requiredString,
                CFSTR("Root"), 0) == kCFCompareEqualTo ||
            CFStringCompare(requiredString,
                CFSTR("Console"), 0) == kCFCompareEqualTo) {

            include_it = true;
            do_children = true;

        } else if (local &&
            CFStringCompare(requiredString, CFSTR("Local-Root"), 0) ==
                kCFCompareEqualTo) {

            include_it = true;
            do_children = true;

        } else if (network &&
            CFStringCompare(requiredString, CFSTR("Network-Root"), 0) ==
                kCFCompareEqualTo) {

            include_it = true;
            do_children = true;

        } else if (safeboot &&
            CFStringCompare(requiredString, CFSTR("Safe Boot"), 0) ==
                kCFCompareEqualTo) {

            include_it = true;
            do_children = true;

        } else {
            char required_string[120];

           // skip for any other value

           if (!CFStringGetCString(requiredString, required_string,
               sizeof(required_string) / sizeof(char),
               kCFStringEncodingASCII)) {

               strcpy(required_string, "(unknown)");
            }

            if (verbose_level >= kKXKextManagerLogLevelDetails) {
                verbose_log(
                    "skipping bundle %s; OSBundleRequired key is \"%s\" "
                    "(still checking plugins)",
                    kext_name, required_string);
            }
            include_it = false;
            do_children = true;
            goto finish;
        }
    }

    if (!KXKextHasBeenAuthenticated(theKext)) {
        KXKextAuthenticate(theKext);
    }

    if (!KXKextIsAuthentic(theKext)) {
        fprintf(stderr, "kernel extension %s is not authentic (check ownership and permissions); "
             "skipping it and any plugins\n", kext_name);
        if (do_tests) {
            fprintf(stderr, "kext diagnostics:\n");
            KXKextPrintDiagnostics(theKext, stderr);
        }
        include_it = false;
        do_children = false;
        goto finish;
    }

    KXKextResolveDependencies(theKext);
    if (!KXKextGetHasAllDependencies(theKext)) {
        fprintf(stderr, "warning: kernel extension %s is missing dependencies "
            "(including in cache anyway; "
            "dependencies may be available from elsewhere)\n",
            kext_name);
        if (do_tests) {
            fprintf(stderr, "kext diagnostics:\n");
            KXKextPrintDiagnostics(theKext, stderr);
        }
        // include it anyway
	include_it = true;
	do_children = true;
    }

    if (KXKextGetDeclaresExecutable(theKext)) {
        executableURL = CFBundleCopyExecutableURL(kextBundle);
        if (!executableURL) {
            fprintf(stderr, "skipping bundle %s; "
                " declares an exectuable but has none "
                "(still checking plugins)\n",
                kext_name);
            include_it = false;
            do_children = true;
            goto finish;
        } else {

            if (!CFURLGetFileSystemRepresentation(executableURL,true/*resolve*/,
		    (UInt8*)executable_path, PATH_MAX)) {
                fprintf(stderr, "memory allocation failure\n");
                exit(1);
            } else {
                struct stat stat_buf;
                off_t archSize;
    
                fd = open(executable_path, O_RDONLY, 0);
                if (-1 == fd) { // Can't read executable
                    fprintf(stderr, "can't open file %s; "
                        "skipping the kext that contains it\n",
                        executable_path);
                    include_it = false;
                    do_children = true;
                    goto finish;
                }

                if (fstat(fd, &stat_buf) < 0) {
                    fprintf(stderr, "can't get size of file %s; "
                        "skipping the kext that contains it\n",
                        executable_path);
                    include_it = false;
                    do_children = true;
                    goto finish;
                }
                machOSize = stat_buf.st_size;

                machO = mmap(NULL, machOSize,
                    PROT_READ|PROT_WRITE, MAP_FILE|MAP_PRIVATE,
                    fd, 0 /* offset */);
                if (machO == (caddr_t)-1) {
                    fprintf(stderr, "can't map file %s; "
                        "skipping the kext that contains it\n",
                        executable_path);
                    include_it = false;
                    do_children = true;
                    goto finish;
                }
  
                find_arch(NULL, &archSize, archCPU, archSubtype,
                   (u_int8_t *)machO, machOSize);
                if (!archSize) { // Couldn't find an architecture
                    if (verbose_level >= kKXKextManagerLogLevelDetails) {
                        fprintf(stderr, "%s doesn't contain code for the "
                            "architecture specified; "
                            "skipping the kext that contains it\n",
                            executable_path);
                    }
                    include_it = false;
                    do_children = true;
                    goto finish;
                }
            }
        }
    }

finish:

    if (machO != (caddr_t)-1) {
        munmap(machO, machOSize);
    }

    if (fd != -1) {
        close(fd);
    }

    if (include_it && !CFDictionaryGetValue(checkDictionary, kextName)) {
        CFDictionarySetValue(checkDictionary, kextName, theKext);
    }

    if (do_children) {
        collectKextsForMkextCache(KXKextGetPlugins(theKext),
            checkDictionary, archCPU, archSubtype, local, network, safeboot, kernel_requests,
            verbose_level, do_tests);
    }

    if (kextName)        CFRelease(kextName);
    if (executableURL)   CFRelease(executableURL);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
static void collectKextsForMkextCache(
    CFArrayRef kexts,
    CFMutableDictionaryRef checkDictionary,
    cpu_type_t archCPU,
    cpu_subtype_t archSubtype,
    Boolean local,
    Boolean network,
    Boolean safeboot,
    CFSetRef kernel_requests,
    int verbose_level,
    Boolean do_tests)
{
    CFIndex count, i;
    KXKextRef theKext = NULL; // don't release
    if (!kexts) {
        goto finish;
    }

   /*****
    * Get busy processing kexts.
    */
    count = CFArrayGetCount(kexts);
    for (i = 0; i < count; i++) {
        theKext = (KXKextRef)CFArrayGetValueAtIndex(kexts, i);

        addKextForMkextCache(theKext, checkDictionary, archCPU, archSubtype,
            local, network, safeboot, kernel_requests, verbose_level, do_tests);
    }

finish:

    return;
}

/*******************************************************************************
* usage()
*******************************************************************************/
static void usage(int level)
{
    fprintf(stderr,
      "usage: %s [-a arch] [-c kernel_cache_filename] [-e] [-F] [-h] [-k]\n"
      "       [-K kernel_filename] [-l | -L] [-r] [-m mkext_filename] [-n | -N]"
      "\n"
      "       [-r] [-s | -S] [-t] [-v [1-6]] [-z] [kext_or_directory] ...\n"
      "\n",
      progname);
    fprintf(stderr, "       %s [-v [#]] [-f] -u volume\n", progname);
    fprintf(stderr, "       %s [-v [#]] -U volume\n\n", progname);

    if (level < 1) {
        return;
    }

    if (level == 1) {
        fprintf(stderr, "use %s -h for an explanation of each option\n",
            progname);
        return;
    }

    fprintf(stderr,
        "  kext_or_directory: Add the kext or all kexts in directory to cache\n"
        "    (required unless using -e)\n");
    fprintf(stderr,
        "  -a arch: Add only kexts that contain code for arch\n");
    fprintf(stderr,
        "  -c kernel_cache_filename: Create an kernel prelink cache\n");
    fprintf(stderr,
        "  -e: Create or update the mkext cache for /System/Library/Extensions\n");
    fprintf(stderr,
        "  -F: Fork and exit immediately (to work in background)\n");
    fprintf(stderr,
        "  -h: This usage statement\n");
    fprintf(stderr,
        "  -k: Create or update any repository .kextcache files\n");
    fprintf(stderr,
        "  -K kernel_filename: Name of kernel file for kernel cache\n");
    fprintf(stderr,
        "  -l: Add local-root kexts in directories to an mkext cache\n");
    fprintf(stderr,
        "  -L: Add local-root kexts for all args to an mkext cache\n");
    fprintf(stderr,
        "  -m mkext_filename: Create an mkext archive\n");
    fprintf(stderr,
        "  -n: Add network-root kexts in directories to an mkext cache\n");
    fprintf(stderr,
        "  -N: Add network-root kexts for all args to an mkext cache\n");
    fprintf(stderr,
        "  -r: Add kexts previously loaded by this machine\n");
    fprintf(stderr,
        "  -s: Add safe boot kexts in directories to an mkext cache\n");
    fprintf(stderr,
        "  -S: Add safe boot kexts for all args to an mkext cache\n");
    fprintf(stderr,
        "  -t: Perform diagnostic tests on kexts and print results\n");
    fprintf(stderr,
        "  -u/-U: update any caches needed for booting (-U errors if out of date)\n");
    fprintf(stderr,
        "  -v: verbose mode; print info about caching process\n");
    fprintf(stderr,
        "  -z: don't authenticate kexts (for use during development)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "At least one of -k, -c, -m, -u, or -U must be specified.\n");
    fprintf(stderr,
        "-l/-L and -n/-N may both be specified to make a cache of local-\n"
        "and network-root kexts\n");

    return;
}



