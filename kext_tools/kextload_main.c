#include <CoreFoundation/CoreFoundation.h>
#include <libc.h>
#include <IOKit/IOTypes.h>

#include <IOKit/kext/KXKextManager.h>
#include <IOKit/kext/KextManagerPriv.h>

#define ALLOW_PATCH_OUTPUT  0
#define ALLOW_NO_START      0

/*******************************************************************************
* Global variables.
*******************************************************************************/

static char * progname = "(unknown)";
static int verbose_level = kKXKextManagerLogLevelDefault;  // -v, -q options set this


/*******************************************************************************
* Utility and callback functions.
*******************************************************************************/

static Boolean check_file(const char * filename);
static Boolean check_dir(const char * dirname, int writeable);
static void qerror(const char * format, ...);
static void verbose_log(const char * format, ...);
static void error_log(const char * format, ...);
static int user_approve(int default_answer, const char * format, ...);
static const char * user_input(const char * format, ...);
static int addKextsToManager(
    KXKextManagerRef aManager,
    CFArrayRef kextNames,
    CFMutableArrayRef kextArray,
    Boolean do_tests);
static void usage(int level);

extern KXKextManagerError _KXKextManagerPrepareKextForLoading(
    KXKextManagerRef aKextManager,
    KXKextRef aKext,
    const char * kext_name,
    Boolean check_loaded_for_dependencies,
    Boolean do_load,
    CFMutableArrayRef inauthenticKexts);
extern KXKextManagerError _KXKextManagerLoadKextUsingOptions(
    KXKextManagerRef aKextManager,
    KXKextRef aKext,
    const char * kext_name,
    const char * kernel_file,
    const char * patch_dir,
    const char * symbol_dir,
    IOOptionBits load_options,
    Boolean do_start_kext,
    int     interactive_level,
    Boolean ask_overwrite_symbols,
    Boolean overwrite_symbols,
    Boolean get_addrs_from_kernel,
    unsigned int num_addresses,
    char ** addresses);

extern const char * _KXKextCopyCanonicalPathnameAsCString(KXKextRef aKext);

/*******************************************************************************
*******************************************************************************/

int main(int argc, const char *argv[]) {
    int exit_code = 0;
    int failure_code = 0;  // if any kext load fails during the loop
    int add_kexts_result = 1;  // assume success here
    int argc_mod = argc;
    int argc_opt_count = 0;
    const char ** argv_mod = argv;
    int optchar;
    KXKextManagerRef theKextManager = NULL;  // must release
    CFURLRef         kextURL = NULL;         // must release
    KXKextManagerError result;

    CFIndex i, count;

    int approve;                             // for interactive mode queries

   /*****
    * Set by command line option flags.
    */
    unsigned short int flag_n = 0;  // used to sanity-check -n, -l, -m
    unsigned short int flag_l = 0;  // before setting behavior-changing
    unsigned short int flag_m = 0;  // variables do_load & do_start_matching

    Boolean get_addrs_from_kernel = false;   // -A
    Boolean use_repository_caches = true;    // -c to turn off
    Boolean skip_extensions_folder = false;  // -e
    Boolean overwrite_symbols = true;        // -i turns off
    int interactive_level = 0;               // -i/-I turns on

    Boolean do_load = true;                  // -l is load but no matching
    Boolean do_start_kmod = true;            // -j is load, don't start,
                                             //    no matching
    Boolean do_start_matching = true;        // -m is don't load but start
                                             //    matching
                                             // -n is don't do either

    Boolean do_tests = false;                // -t
    Boolean strict_auth = true;
    Boolean safe_boot_mode = false;          // -x
    Boolean pretend_authentic = false;       // -z
    Boolean skip_dependencies = false;       // -Z (and with -t only!)
    Boolean check_loaded_for_dependencies = false; // -D to turn off (obsolete)

    unsigned int addresses_cap = 10;
    unsigned int num_addresses = 0;
    char ** addresses = NULL;                // -a; must free

    CFMutableArrayRef kextIDs = NULL;                // -b; must release
    CFMutableArrayRef personalityNames = NULL;       // -p; must release
    CFMutableArrayRef dependencyNames = NULL;        // -d; must release
    CFMutableArrayRef repositoryDirectories = NULL;  // -r; must release
    CFMutableArrayRef kextNames = NULL;              // args; must release
    CFMutableArrayRef kextNamesToUse = NULL;         // must release
    CFMutableArrayRef inauthenticKexts = NULL;       // must release
    KXKextRef theKext = NULL;                        // don't release

    const char * default_kernel_file = "/mach";
    const char * kernel_file = NULL;  // overriden by -k option
    const char * symbol_dir = NULL;   // set by -s option;
                                      // for writing debug files for kmods
    const char * patch_dir = NULL;    // set by -P option;
                                      // for writing patchup files for kmods

    CFIndex inauthentic_kext_count = 0;
    CFIndex k = 0;

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
    addresses = (char **)malloc(addresses_cap * sizeof(char *));
    if (!addresses) {
        exit_code = 1;
        qerror("memory allocation failure\n");
        goto finish;
    }
    bzero(addresses, addresses_cap * sizeof(char *));

    personalityNames = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!personalityNames) {
        exit_code = 1;
        qerror("memory allocation failure\n");
        goto finish;
    }

    dependencyNames = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!dependencyNames) {
        exit_code = 1;
        qerror("memory allocation failure\n");
        goto finish;
    }

    repositoryDirectories = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!repositoryDirectories) {
        exit_code = 1;
        qerror("memory allocation failure\n");
        goto finish;
    }

    kextNames = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!kextNames) {
        exit_code = 1;
        qerror("memory allocation failure\n");
        goto finish;
    }

    kextNamesToUse = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!kextNamesToUse) {
        exit_code = 1;
        qerror("memory allocation failure\n");
        goto finish;
    }

    kextIDs = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!kextIDs) {
        exit_code = 1;
        qerror("memory allocation failure\n");
        goto finish;
    }

    /*****
    * Process command line arguments. If running in kextload-compatibiliy
    * mode, accept its old set of options and none other. If running in
    * the new mode, process the new, larger set of options.
    */
// -j option currently masked out
    while ((optchar = getopt(argc, (char * const *)argv,
               "a:Ab:cd:DehiIk:lLmnp:P:qr:s:tvxzZ")) != -1) {

        char * address_string = NULL;  // don't free
        unsigned int address;
        CFStringRef optArg = NULL;    // must release

        switch (optchar) {
          case 'a':
            flag_n = 1;  // -a implies -n

            if (!optarg) {
                qerror("no argument for -a\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            address_string = index(optarg, '@');
            if (!address_string) {
                qerror("invalid use of -a option\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            address_string++;
            address = strtoul(address_string, NULL, 16);
            if (!address) {
                qerror(
                    "address must be specified and non-zero\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }

            if (num_addresses >= addresses_cap) {
                addresses_cap *= 2;
                addresses = (char **)realloc(addresses,
                    (addresses_cap * sizeof(char *)));
                if (!addresses) {
                    qerror(
                        "memory allocation failure\n");
                    exit_code = 1;
                    goto finish;
                }
                bzero(addresses + num_addresses,
                    (addresses_cap-num_addresses) * sizeof(char *));
            }
            addresses[num_addresses++] = optarg;
            break;

          case 'A':
            flag_n = 1;   // -A implies -n
            get_addrs_from_kernel = true;
            break;

          case 'b':
            if (!optarg) {
                qerror("no argument for -b\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            optArg = CFStringCreateWithCString(kCFAllocatorDefault,
                optarg, kCFStringEncodingMacRoman);
            if (!optArg) {
                qerror("memory allocation failure\n");
                exit_code = 1;
                goto finish;
           }
            CFArrayAppendValue(kextIDs, optArg);
            CFRelease(optArg);
            optArg = NULL;
            break;

          case 'c':
            use_repository_caches = false;
            break;

          case 'd':
            if (!optarg) {
                qerror("no argument for -d\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            optArg = CFStringCreateWithCString(kCFAllocatorDefault,
                optarg, kCFStringEncodingMacRoman);
            if (!optArg) {
                qerror("memory allocation failure\n");
                exit_code = 1;
                goto finish;
            }
            CFArrayAppendValue(dependencyNames, optArg);
            CFRelease(optArg);
            optArg = NULL;
            break;

          case 'D':
            check_loaded_for_dependencies = false;
            break;

          case 'e':
            skip_extensions_folder = true;
            break;

          case 'h':
            usage(2);
            goto finish;
            break;

          case 'i':
            if (interactive_level) {
                qerror("use only one of -i or -I\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            overwrite_symbols = false;
            interactive_level = 1;
            break;

          case 'I':
            if (interactive_level) {
                qerror("use only one of -i or -I\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            overwrite_symbols = false;
            interactive_level = 2;
            break;

#if ALLOW_NO_START
          case 'j':
            do_load = true;
            do_start_kmod = false;
            do_start_matching = false;
            break;
#endif ALLOW_NO_START

          case 'k':
            if (kernel_file) {
                qerror("duplicate use of -k option\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            if (!optarg) {
                qerror("no argument for -k\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            kernel_file = optarg;
            break;

          case 'l':
            flag_l = 1;
            break;

          // see case 'r' for case 'L'

          case 'm':
            flag_m = 1;
            break;

          case 'n':
            flag_n = 1;
            break;

          case 'p':
            if (!optarg) {
                qerror("no argument for -p\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            optArg = CFStringCreateWithCString(kCFAllocatorDefault,
                optarg, kCFStringEncodingMacRoman);
            if (!optArg) {
                qerror("memory allocation failure\n");
                exit_code = 1;
                goto finish;
            }
            CFArrayAppendValue(personalityNames, optArg);
            CFRelease(optArg);
            optArg = NULL;
            break;
#if ALLOW_PATCH_OUTPUT
          case 'P':
            if (patch_dir) {
                qerror("duplicate use of -P option\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            if (!optarg) {
                qerror("no argument for -P\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            patch_dir = optarg;
            break;
#endif

          case 'q':
            if (verbose_level != kKXKextManagerLogLevelDefault) {
                qerror("duplicate use of -v and/or -q option\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            verbose_level = kKXKextManagerLogLevelSilent;
            break;

          case 'r':
            /* fall through */
          case 'L':
            if (!optarg) {
                qerror("no argument for -%c\n", optchar);
                usage(0);
                exit_code = 1;
                goto finish;
            }
            optArg = CFStringCreateWithCString(kCFAllocatorDefault,
               optarg, kCFStringEncodingMacRoman);
            if (!optArg) {
                qerror("memory allocation failure\n");
                exit_code = 1;
                goto finish;
            }
            CFArrayAppendValue(repositoryDirectories, optArg);
            CFRelease(optArg);
            optArg = NULL;
            break;

          case 's':
            if (symbol_dir) {
                qerror("duplicate use of -s option\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            if (!optarg) {
                qerror("no argument for -s\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            symbol_dir = optarg;
            break;

          case 't':
            do_tests = true;
            break;

          case 'v':
            {
                const char * next;

                if (verbose_level != kKXKextManagerLogLevelDefault) {
                    qerror("duplicate use of -v and/or -q option\n\n");
                    usage(0);
                    exit_code = 1;
                    goto finish;
                }
                if (optind >= argc) {
                    verbose_level = kKXKextManagerLogLevelBasic;
                } else {
                    next = argv[optind];
                    if ((next[0] == '0' || next[0] == '1' ||
                         next[0] == '2' || next[0] == '3' || 
                         next[0] == '4' || next[0] == '5' ||
                         next[0] == '6') && next[1] == '\0') {

                        if (next[0] == '0') {
                            verbose_level = kKXKextManagerLogLevelErrorsOnly;
                        } else {
                            verbose_level = atoi(next);
                        }
                        optind++;
                    } else {
                        verbose_level = kKXKextManagerLogLevelBasic;
                    }
                }
            }
            break;

          case 'x':
            safe_boot_mode = true;
            use_repository_caches = false;  // -x implies -c
            break;

          case 'z':
            pretend_authentic = true;
            break;

          case 'Z':
            skip_dependencies = true;
            break;

        default:
            qerror("unknown option -%c\n", optchar);
            usage(0);
            exit_code = 1;
            goto finish;
        }
    }

   /* Update argc, argv based on option processing.
    */
    argc_mod = argc - optind;
    argc_opt_count = optind;
    argv_mod = argv + optind;

   /*****
    * Check for bad combinations of options.
    */
    if (flag_l + flag_m + flag_n > 1) {
        qerror("only one of -l/-m/-n is allowed"
            " (-a and -A imply -n)\n\n");
        usage(0);
        exit_code = 1;
        goto finish;
    } else if (flag_l) {
        do_load = true;
        do_start_matching = false;
    } else if (flag_m) {
        do_load = false;
        do_start_matching = true;
    } else if (flag_n) {
        do_load = false;
        do_start_matching = false;
    }

    if (do_load && geteuid() != 0) {
        qerror("you must be running as root "
            "to load modules into the kernel\n");
        exit_code = 1;
        goto finish;
    }

    if (num_addresses > 0 && get_addrs_from_kernel) {
        qerror("don't use -a with -A\n");
        usage(0);
        exit_code = 1;
        goto finish;
    }

    if (num_addresses > 0 && (do_load || do_start_matching)) {
        qerror("don't use -a with -l or -m\n");
        usage(0);
        exit_code = 1;
        goto finish;
    }

    if (get_addrs_from_kernel && (do_load || do_start_matching)) {
        qerror("don't use -A with -l or -m\n");
        usage(0);
        exit_code = 1;
        goto finish;
    }

    // FIXME: Is this a valid restriction? The default kernel_file
    // FIXME: ...is /mach, which is written out by the kernel during
    // FIXME: ...boot, and always represents the currently running kernel.
    //
    if (kernel_file && (do_load || do_start_matching)) {
        qerror("use -k only with -n\n");
        usage(0);
        exit_code = 1;
        goto finish;
    }

   /*****
    * Make sure if we're just doing debug symbols that we don't
    * load or start matching (-a/-A imply -n).
    */
    if (num_addresses > 0 || get_addrs_from_kernel) {
        do_load = 0;
        do_start_matching = 0;
    }

    if (pretend_authentic && do_load) {
        qerror("-z is only allowed when not loading\n");
        usage(0);
        exit_code = 1;
        goto finish;
    }

   /* If running in quiet mode and the invocation might require
    * interaction, just exit with an error.
    */
    if (verbose_level == kKXKextManagerLogLevelSilent) {
        if (interactive_level > 0 ||
            (flag_n && num_addresses == 0 && get_addrs_from_kernel == 0)) {

            exit_code = 1;
            goto finish;
        }
    }

   /****
    * If we're getting addresses from the kernel we have to call
    * down there, so we might as well check what's loaded before
    * resolving dependencies too (-A overrides -D). If we're not
    * performing a load, then don't check (-n/-m implies -D).
    */
    if (get_addrs_from_kernel) {
        check_loaded_for_dependencies = true;
    }
#if 0
// Default behavior now is to not check loaded for dependencies and
// do pure version-based dependency resolution. We might introduce
// a new flag to turn this on for general load cases....
else if (!do_load) {
        check_loaded_for_dependencies = false;
    }
#endif /* 0 */

   /*****
    * If we're not loading and have no request to emit a symbol
    * or patch file, there's nothing to do!
    */
    if (!do_tests && !do_load && !do_start_matching &&
        !symbol_dir && !patch_dir) {

        qerror("no work to do; check your options\n\n");
        usage(0);
        exit_code = 1;
        goto finish;
    }

   /*****
    * -Z is only allowed if you don't need to generate dependencies.
    */
    if (skip_dependencies && (!do_tests || do_load || symbol_dir || patch_dir)) {
#if ALLOW_PATCH_OUTPUT
        qerror("use -Z only with -nt and not with -s or -P\n");
#else
        qerror("use -Z only with -nt and not with -s\n");
#endif ALLOW_PATCH_OUTPUT
        usage(0);
        exit_code = 1;
        goto finish;
    }

   /*****
    * Record the kext names from the command line.
    */
    for (i = 0; i < argc_mod; i++) {
        CFStringRef kextName = CFStringCreateWithCString(kCFAllocatorDefault,
              argv_mod[i], kCFStringEncodingMacRoman);
        if (!kextName) {
            qerror("memory allocation failure\n");
            exit_code = 1;
            goto finish;
        }
        CFArrayAppendValue(kextNames, kextName);
        CFRelease(kextName);
    }

    if (CFArrayGetCount(kextNames) == 0 && CFArrayGetCount(kextIDs) == 0) {
        qerror("no kernel extension specified\n\n");
        usage(0);
        exit_code = 1;
        goto finish;
    }

   /*****
    * Make sure we scan the standard Extensions folder unless told explicitly
    * not to.
    */
    if (!skip_extensions_folder) {
        CFArrayInsertValueAtIndex(repositoryDirectories, 0,
            kKXSystemExtensionsFolder);
    }

   /*****
    * Make sure the kernel file, symbol directory, and patch directory
    * exist and are readable/writable.
    */
    if (!kernel_file) {
        kernel_file = default_kernel_file;
        if (!do_load && verbose_level >= kKXKextManagerLogLevelBasic) {
            verbose_log("no kernel file specified; using %s", kernel_file);
        }
    }

    if (!check_file(kernel_file)) {
        // check_file() prints an error message
        exit_code = 1;
        goto finish;
    }

    if (symbol_dir) {
        if (!check_dir(symbol_dir, 1)) {
            // check_dir() prints an error message
            exit_code = 1;
            goto finish;
        }
    }

    if (patch_dir) {
        if (!check_dir(patch_dir, 1)) {
            // check_dir() prints an error message
            exit_code = 1;
            goto finish;
        }
    }

   /*****
    * Set up the kext manager.
    */
    theKextManager = KXKextManagerCreate(kCFAllocatorDefault);
    if (!theKextManager) {
        qerror("can't allocate kernel extension manager\n");
        exit_code = 1;
        goto finish;
    }

    result = KXKextManagerInit(theKextManager, true /* load_in_task */,
        safe_boot_mode);
    if (result != kKXKextManagerErrorNone) {
        qerror("can't initialize kernel extension manager (%s)\n",
            KXKextManagerErrorStaticCStringForError(result));
        exit_code = 1;
        goto finish;
    }

    KXKextManagerSetPerformsFullTests(theKextManager, do_tests);
    KXKextManagerSetPerformsStrictAuthentication(theKextManager, strict_auth);
    KXKextManagerSetLogLevel(theKextManager, verbose_level);
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
    * Add the dependency kexts to the manager. Do this *before* adding
    * whole repositories; the kext manager searches for duplicate versions
    * in order of addition, so we want explicitly named dependencies to
    * be found first. (This won't help a bit if a later version is in a
    * repository, of course; the user has to know how to use the program,
    * after all.)
    *
    * This invocation is a failure only if a fatal error occurs; if some
    * of the kexts couldn't be added we will keep going and deal with a
    * missing dependency later.
    */
    if (addKextsToManager(theKextManager, dependencyNames, NULL, do_tests) == -1) {
        exit_code = 1;
        goto finish;
    }

   /*****
    * Add the extensions folders specified with -r to the manager.
    */
    count = CFArrayGetCount(repositoryDirectories);
    for (i = 0; i < count; i++) {
        CFStringRef directory = (CFStringRef)CFArrayGetValueAtIndex(
            repositoryDirectories, i);
        CFURLRef directoryURL =
            CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                directory, kCFURLPOSIXPathStyle, true);
        if (!directoryURL) {
            qerror("memory allocation failure\n");
            exit_code = 1;

            goto finish;
        }

        result = KXKextManagerAddRepositoryDirectory(theKextManager,
            directoryURL, true /* scanForKexts */,
            use_repository_caches, NULL);
        if (result != kKXKextManagerErrorNone) {
            qerror("can't add repository (%s).\n",
                KXKextManagerErrorStaticCStringForError(result));
            exit_code = 1;
            goto finish;
        }
        CFRelease(directoryURL);
        directoryURL = NULL;
    }

   /*****
    * Add each kext named on the command line to the manager. If any can't
    * be added, addKextsToManager() returns 0, so set the exit_code to 1, but
    * go on to process the kexts that could be added.
    * If a fatal error occurs, addKextsToManager() returns -1, so go right
    * to the finish;
    */
    add_kexts_result = addKextsToManager(theKextManager, kextNames,
       kextNamesToUse, do_tests);
    if (add_kexts_result < 1) {
        exit_code = 1;
        if (add_kexts_result < 0) {
            goto finish;
        }
    }

   /*****
    * Either fake authentication, or if doing all tests then authenticate
    * everything. (It may not be necessary to do this for
    * *everything*; might be able to just do it on the named kexts.)
    */
    if (pretend_authentic) {
        // Yes, do this even if do_tests is true; -tz means fake authentication.
        KXKextManagerMarkKextsAuthentic(theKextManager);
    } else if (do_tests) {
        KXKextManagerAuthenticateKexts(theKextManager);
    }

    KXKextManagerEnableClearRelationships(theKextManager);

   /*****
    * If doing full tests, assemble the whole database of version and
    * dependency relationships. (It may not be necessary to do this for
    * *everything*; might be able to just do it on the named kexts.)
    */
    if (do_tests) {
        KXKextManagerCalculateVersionRelationships(theKextManager);
        if (!skip_dependencies) {
            KXKextManagerResolveAllKextDependencies(theKextManager);
        }
    }

   /*****
    * If we got CFBundleIdentifiers, then look them up. We just add to the
    * kextNamesToUse array here.
    */
    count = CFArrayGetCount(kextIDs);
    for (i = 0; i < count; i++) {
        CFStringRef thisKextID = (CFStringRef)
            CFArrayGetValueAtIndex(kextIDs, i);
        KXKextRef theKext = KXKextManagerGetKextWithIdentifier(
            theKextManager, thisKextID);
        CFStringRef kextName = NULL;  // must release
        char name_buffer[255];

        if (!CFStringGetCString(thisKextID,
            name_buffer, sizeof(name_buffer) - 1, kCFStringEncodingMacRoman)) {

            qerror("internal error; no memory?\n");
            exit_code = 1;
            goto finish;
        }
        if (verbose_level >= kKXKextManagerLogLevelBasic) {
            verbose_log("looking up extension with identifier %s",
                name_buffer);
        }

        if (!theKext) {
            if (!CFStringGetCString(thisKextID,
                name_buffer, sizeof(name_buffer) - 1,
                kCFStringEncodingMacRoman)) {

                qerror("internal error; no memory?\n");
                exit_code = 1;
                goto finish;
            }
            qerror("can't find extension with identifier %s\n",
                name_buffer);
            exit_code = 1;
            continue;  // not fatal, keep trying the others
        }
        kextName = KXKextCopyAbsolutePath(theKext);
        if (!kextName) {
            qerror("memory allocation failure\n");
            exit_code = 1;
            goto finish;
        }
        if (!CFStringGetCString(kextName,
            name_buffer, sizeof(name_buffer) - 1, kCFStringEncodingMacRoman)) {

            qerror("internal error; no memory?\n");
            exit_code = 1;
            goto finish;
        }
        if (verbose_level >= kKXKextManagerLogLevelBasic) {
            verbose_log("found extension bundle %s", name_buffer);
        }
        CFArrayAppendValue(kextNamesToUse, kextName);
        CFRelease(kextName);
    }

   /*****
    * Get busy loading kexts.
    */
    count = CFArrayGetCount(kextNamesToUse);
    for (i = 0; i < count; i++) {
        CFStringRef kextName = NULL; // don't release
        char kext_name[MAXPATHLEN];
        unsigned short cache_retry_count = 1;

       /*****
        * If the last iteration flipped failure_code, make the exit_code
        * nonzero and reset failure_code for this iteration.
        */
        if (failure_code) {
            exit_code = 1;
            failure_code = 0;
        }

retry:

        if (inauthenticKexts) {
            CFRelease(inauthenticKexts);
            inauthenticKexts = NULL;
        }

        inauthenticKexts = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
        if (!inauthenticKexts) {
            exit_code = 1;
            qerror("memory allocation failure\n");
            goto finish;
        }

        if (kextURL) {
            CFRelease(kextURL);
            kextURL = NULL;
        }

        kextName = CFArrayGetValueAtIndex(kextNamesToUse, i);
        if (!CFStringGetCString(kextName,
            kext_name, sizeof(kext_name) - 1, kCFStringEncodingMacRoman)) {

            qerror("memory allocation or string conversion failure\n");
            exit_code = 1;
            goto finish;
        }

        kextURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
            kextName, kCFURLPOSIXPathStyle, true);
        if (!kextURL) {
            qerror("memory allocation failure\n");
            exit_code = 1;
            goto finish;
        }

        theKext = KXKextManagerGetKextWithURL(theKextManager, kextURL);

        if (!theKext) {
            qerror("can't find extension %s in database\n",
                kext_name);
            failure_code = 1;
            continue;
        }

       /*****
        * Confirm kext is loadable.
        */
        if (safe_boot_mode && !KXKextIsEligibleDuringSafeBoot(theKext)) {
            qerror("extension %s is not eligible for safe boot "
                "(has no OSBundleRequired setting, "
                "or all personalities have nonzero IOKitDebug setting)\n",
                kext_name);
            failure_code = 1;
            // keep going here to print diagnostics and hit the "continue" below
        }

        // don't offer to fix or load a kext if run in diagnostic mode. If a
        // dependency is nonsecure, then checking here would really involve too
        // much work.

        if (!KXKextIsValid(theKext) ||
            (do_tests && (
               (!pretend_authentic && !KXKextIsAuthentic(theKext)) ||
               (!skip_dependencies && !KXKextGetHasAllDependencies(theKext))
           ))) {

            qerror("kernel extension %s has problems", kext_name);
            if (do_tests && verbose_level >= kKXKextManagerLogLevelErrorsOnly) {
                qerror(":\n");
                KXKextPrintDiagnostics(theKext, stderr);
            } else {
                qerror(" (run %s with -t for diagnostic output)\n",
                    progname);
            }
            failure_code = 1;
        }

        if (failure_code) {
            continue;
        }

        if (do_tests || verbose_level > kKXKextManagerLogLevelDefault) {
            verbose_log("extension %s appears to be valid", kext_name);
        }

        if (KXKextHasDebugProperties(theKext)) {
            verbose_log("notice: extension %s has debug properties set",
                kext_name);
        }

       /* If there's nothing else to do for this kext, then move on
        * to the next one.
        */
        if (!do_load && !do_start_matching && !patch_dir && !symbol_dir) {
            continue;
        }

       /*****
        * If requested, get user approval to continue.
        */
        if (interactive_level > 0 && do_load) {
            approve = user_approve(1, "Load extension %s and its dependencies",
                kext_name);
            if (approve < 0) {
                qerror("error reading response\n");
                failure_code = 1;
                goto finish;
            } else if (approve == 0) {
                qerror("not loading extension %s\n", kext_name);
                continue;
            }
        }

        if (failure_code) {
            continue;
        }

        result = _KXKextManagerPrepareKextForLoading(
            theKextManager, theKext, kext_name,
            check_loaded_for_dependencies, do_load,
            inauthenticKexts);

        if (result == kKXKextManagerErrorAlreadyLoaded ||
            result == kKXKextManagerErrorLoadedVersionDiffers) {

            // this is not considered a failure
            verbose_log("extension %s is already loaded", kext_name);
            continue;
        } else if (result != kKXKextManagerErrorNone &&
            result != kKXKextManagerErrorAuthentication) {

            failure_code = 1;
            exit_code = 1;
            continue;
        } 

        inauthentic_kext_count = CFArrayGetCount(inauthenticKexts);
        if (inauthentic_kext_count) {
            for (k = 0; k < inauthentic_kext_count; k++) {
                KXKextRef thisKext =
                    (KXKextRef)CFArrayGetValueAtIndex(inauthenticKexts, k);

                const char * kext_path = NULL; // must free
                CFDataRef xmlData = NULL; // must release
                const char * rpc_data = NULL; // don't free (ref to kext_path)
                size_t data_length = 0;

                kext_path = _KXKextCopyCanonicalPathnameAsCString(thisKext);
                if (!kext_path) {
                    fprintf(stderr, "memory allocation failure\n");
                    exit_code = 1;
                    goto finish;
                }
                error_log("extension %s is not authentic (check ownership and permissions)", kext_path);
                rpc_data = kext_path;
                data_length = 1 + strlen(kext_path);
                _KextManagerRecordNonsecureKextload(rpc_data, data_length);
                if (kext_path) free((char *)kext_path);
                if (xmlData) CFRelease(xmlData);
                failure_code = 1;
                exit_code = 1;
            }
        }

        if (failure_code) {
            continue;
        }

       /*****
        * We can't load a kext that has no code (or can we?).
        */
        if ( (do_load || patch_dir || symbol_dir) &&
             KXKextGetDeclaresExecutable(theKext)) {

            result = _KXKextManagerLoadKextUsingOptions(
                theKextManager, theKext, kext_name, kernel_file,
                patch_dir, symbol_dir,
                do_load, do_start_kmod,
                interactive_level,
                (interactive_level > 0) /* ask overwrite symbols */,
                overwrite_symbols,
                get_addrs_from_kernel, num_addresses, addresses);

            if (result == kKXKextManagerErrorInvalidArgument) {
                failure_code = 1;
                continue;
            } else if (result == kKXKextManagerErrorAlreadyLoaded ||
                result == kKXKextManagerErrorLoadedVersionDiffers) {

                // this is not considered a failure
                verbose_log("extension %s is already loaded", kext_name);
                continue;
            } else if (result == kKXKextManagerErrorUserAbort) {
                if (do_load) {
                    qerror("load aborted for extension %s\n",
                        kext_name);
                }
                // user abort is not a failure
                continue;
            } else if (result == kKXKextManagerErrorCache) {
                if (cache_retry_count == 0) {
                    continue;
                }
                if (interactive_level > 0 ) {
                    approve = user_approve(1,
                        "Cache inconsistency for %s; rescan and try again",
                        kext_name);
                    if (approve < 0) {
                        qerror("error reading response\n");
                        failure_code = 1;
                        goto finish;
                    } else if (approve == 0) {
                        qerror("skipping extension %s\n", kext_name);
                        continue;
                    }
                } else {
                    qerror("rescanning all kexts due to cache inconsistency\n");
                }
                KXKextManagerResetAllRepositories(theKextManager);
                cache_retry_count--;
                goto retry;
            } else if (result != kKXKextManagerErrorNone) {
                if (do_load) {
                    qerror("load failed for extension %s\n",
                        kext_name);
                }
                if (do_tests && !KXKextIsLoadable(theKext, safe_boot_mode) &&
                    verbose_level >= kKXKextManagerLogLevelErrorsOnly) {

                    qerror("kernel extension problems:\n");
                    KXKextPrintDiagnostics(theKext, stderr);
                } else {
                    qerror(" (run %s with -t for diagnostic output)\n",
                        progname);
                }
                failure_code = 1;
                continue;
            }

            if (do_load) {
                verbose_log("%s loaded successfully", kext_name);
            }
        } else if (do_load && verbose_level >= kKXKextManagerLogLevelBasic) {
            verbose_log("extension %s has no executable", kext_name);
            // FIXME: Is this an error?
        }

        if (failure_code) {
            continue;
        }

       /*****
        * Send the personalities down to the kernel.
        */
        if (do_start_matching && KXKextHasPersonalities(theKext)) {
            result = KXKextManagerSendKextPersonalitiesToCatalog(
                theKextManager, theKext, personalityNames,
                (interactive_level > 0), safe_boot_mode);

            if (result == kKXKextManagerErrorNone &&
                (interactive_level || verbose_level >= kKXKextManagerLogLevelBasic) ) {
                verbose_log("matching started for %s", kext_name);
            } else if (result != kKXKextManagerErrorNone) {
                failure_code = 1;
                if (interactive_level || verbose_level >= kKXKextManagerLogLevelBasic) {
                    verbose_log("start matching failed for %s", kext_name);
                }
            }
        }

        if (failure_code) {
            continue;
        }
    }

    if (failure_code) {
        exit_code = 1;
    }

finish:

   /*****
    * Clean everything up.
    */
    if (addresses)             free(addresses);
    if (repositoryDirectories) CFRelease(repositoryDirectories);
    if (dependencyNames)       CFRelease(dependencyNames);
    if (personalityNames)      CFRelease(personalityNames);
    if (kextNames)             CFRelease(kextNames);
    if (kextNamesToUse)        CFRelease(kextNamesToUse);
    if (kextIDs)               CFRelease(kextIDs);
    if (inauthenticKexts)      CFRelease(inauthenticKexts);

    if (theKextManager)        CFRelease(theKextManager);

    exit(exit_code);
    return exit_code;
}

/*******************************************************************************
* check_file()
*
* This function makes sure that a given file exists, is a regular file, and
* is readable.
*******************************************************************************/
static Boolean check_file(const char * filename)
{
    Boolean result = true;  // assume success
    struct stat stat_buf;

    if (stat(filename, &stat_buf) != 0) {
        perror(filename);
        result = false;
        goto finish;
    }

    if ( !(stat_buf.st_mode & S_IFREG) ) {
        qerror("%s is not a regular file\n", filename);
        result = false;
        goto finish;
    }

    if (access(filename, R_OK) != 0) {
        qerror("%s is not readable\n", filename);
        result = false;
        goto finish;
    }

finish:
    return result;
}

/*******************************************************************************
* check_dir()
*
* This function makes sure that a given directory exists, and is writeable.
*******************************************************************************/
static Boolean check_dir(const char * dirname, int writeable)
{
    int result = true;  // assume success
    struct stat stat_buf;

    if (stat(dirname, &stat_buf) != 0) {
        perror(dirname);
        result = false;
        goto finish;
    }

    if ( !(stat_buf.st_mode & S_IFDIR) ) {
        qerror("%s is not a directory\n", dirname);
        result = false;
        goto finish;
    }

    if (writeable) {
        if (access(dirname, W_OK) != 0) {
            qerror("%s is not writeable\n", dirname);
            result = false;
            goto finish;
        }
    }
finish:
    return result;
}

/*******************************************************************************
* qerror()
*
* Quick wrapper over printing that checks verbose level.
*******************************************************************************/
static void qerror(const char * format, ...)
{
    va_list ap;

    if (verbose_level <= kKXKextManagerLogLevelSilent) return;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    return;
}

/*******************************************************************************
* verbose_log()
*
* Print a log message prefixed with the name of the program.
*******************************************************************************/
static void verbose_log(const char * format, ...)
{
    va_list ap;
    char fake_buffer[2];
    int output_length;
    char * output_string;

    if (verbose_level < kKXKextManagerLogLevelDefault) return;

    va_start(ap, format);
    output_length = vsnprintf(fake_buffer, 1, format, ap);
    va_end(ap);

    output_string = (char *)malloc(output_length + 1);
    if (!output_string) {
        qerror("memory allocation failure\n");
        return;
    }

    va_start(ap, format);
    vsprintf(output_string, format, ap);
    va_end(ap);

    va_start(ap, format);
    fprintf(stdout, "%s: %s\n", progname, output_string);
    va_end(ap);

    fflush(stdout);

    free(output_string);

    return;
}

/*******************************************************************************
* error_log()
*
* Print an error message prefixed with the name of the program.
*******************************************************************************/
static void error_log(const char * format, ...)
{
    va_list ap;
    char fake_buffer[2];
    int output_length;
    char * output_string;

    if (verbose_level <= kKXKextManagerLogLevelSilent) return;

    va_start(ap, format);
    output_length = vsnprintf(fake_buffer, 1, format, ap);
    va_end(ap);

    output_string = (char *)malloc(output_length + 1);
    if (!output_string) {
        qerror("memory allocation failure\n");
        return;
    }

    va_start(ap, format);
    vsprintf(output_string, format, ap);
    va_end(ap);

    va_start(ap, format);
    qerror("%s: %s\n", progname, output_string);
    va_end(ap);

    fflush(stderr);

    free(output_string);

    return;
}

/*******************************************************************************
* user_approve()
*
* Ask the user a question and wait for a yes/no answer.
*******************************************************************************/
static int user_approve(int default_answer, const char * format, ...)
{
    int result = 1;
    va_list ap;
    char fake_buffer[2];
    int output_length;
    char * output_string;
    char * prompt_string = NULL;
    int c, x;

    va_start(ap, format);
    output_length = vsnprintf(fake_buffer, 1, format, ap);
    va_end(ap);

    output_string = (char *)malloc(output_length + 1);
    if (!output_string) {
        qerror("memory allocation failure\n");
        result = -1;
        goto finish;
    }

    va_start(ap, format);
    vsprintf(output_string, format, ap);
    va_end(ap);

    prompt_string = default_answer ? " [Y/n]" : " [y/N]";
    
    while ( 1 ) {
        fprintf(stdout, "%s%s%s", output_string, prompt_string, "? ");
        fflush(stdout);

        c = fgetc(stdin);

        if (c == EOF) {
            result = -1;
            goto finish;
        }

       /* Make sure we get a newline.
        */
        if ( c != '\n' ) {
            do {
                x = fgetc(stdin);
            } while (x != '\n' && x != EOF);

            if (x == EOF) {
                result = -1;
                goto finish;
            }
        }

        if (c == '\n') {
            result = default_answer ? 1 : 0;
            goto finish;
        } else if (tolower(c) == 'y') {
            result = 1;
            goto finish;
        } else if (tolower(c) == 'n') {
            result = 0;
            goto finish;
        }
    }

finish:
    if (output_string) free(output_string);

    return result;
}

/*******************************************************************************
* user_input()
*
* Ask the user for input.
*******************************************************************************/
static const char * user_input(const char * format, ...)
{
    char * result = NULL;  // return value
    va_list ap;
    char fake_buffer[2];
    int output_length;
    char * output_string = NULL;
    unsigned index;
    size_t size = 80;
    int c;

    result = (char *)malloc(size);
    if (!result) {
        goto finish;
    }
    index = 0;

    va_start(ap, format);
    output_length = vsnprintf(fake_buffer, 1, format, ap);
    va_end(ap);

    output_string = (char *)malloc(output_length + 1);
    if (!output_string) {
        qerror("memory allocation failure\n");
        result = NULL;
        goto finish;
    }

    va_start(ap, format);
    vsprintf(output_string, format, ap);
    va_end(ap);

    fprintf(stdout, "%s ", output_string);
    fflush(stdout);

    c = fgetc(stdin);
    while (c != '\n' && c != EOF) {
        if (index >= size) {
            size += 80;
            result = realloc(result, size);
            if (!result) {
                goto finish;
            }
        }
        result[index++] = (char)c;
        c = fgetc(stdin);
    }
    if (c == EOF) {
        if (result) free(result);
        result = NULL;
        goto finish;
    }

finish:
    if (output_string) free(output_string);

    return result;
}

/*******************************************************************************
* addKextsToManager()
*
* Add the kexts named in the kextNames array to the given kext manager, and
* put their names into the kextNamesToUse.
*
* Return values:
*   1: all kexts added successfully
*   0: one or more could not be added
*  -1: program-fatal error; exit as soon as possible
*******************************************************************************/
static int addKextsToManager(
    KXKextManagerRef aManager,
    CFArrayRef kextNames,
    CFMutableArrayRef kextNamesToUse,
    Boolean do_tests)
{
    int result = 1;     // assume success
    KXKextManagerError kxresult = kKXKextManagerErrorNone;
    CFIndex i, count;
    KXKextRef theKext = NULL;  // don't release
    CFURLRef kextURL = NULL;   // must release

   /*****
    * Add each kext named to the manager.
    */
    count = CFArrayGetCount(kextNames);
    for (i = 0; i < count; i++) {
        char name_buffer[MAXPATHLEN];

        CFStringRef kextName = (CFStringRef)CFArrayGetValueAtIndex(
            kextNames, i);

        if (kextURL) {
            CFRelease(kextURL);
            kextURL = NULL;
        }

        kextURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
            kextName, kCFURLPOSIXPathStyle, true);
        if (!kextURL) {
            qerror("memory allocation failure\n");
            result = -1;
            goto finish;
        }

        if (!CFStringGetCString(kextName,
            name_buffer, sizeof(name_buffer) - 1, kCFStringEncodingMacRoman)) {

            qerror("memory allocation or string conversion error\n");
            result = -1;
            goto finish;
        }

        kxresult = KXKextManagerAddKextWithURL(aManager, kextURL, true, &theKext);
        if (kxresult != kKXKextManagerErrorNone) {
            result = 0;
            qerror("can't add kernel extension %s (%s)",
                name_buffer, KXKextManagerErrorStaticCStringForError(kxresult));
            qerror(" (run %s on this kext with -t for diagnostic output)\n",
                progname);
#if 0
            if (do_tests && theKext && verbose_level >= kKXKextManagerLogLevelErrorsOnly) {
                qerror("kernel extension problems:\n");
                KXKextPrintDiagnostics(theKext, stderr);
            }
            continue;
#endif 0
        }
        if (kextNamesToUse && theKext &&
            (kxresult == kKXKextManagerErrorNone || do_tests)) {

            CFArrayAppendValue(kextNamesToUse, kextName);
        }
    }

finish:
    if (kextURL) CFRelease(kextURL);
    return result;
}

/*******************************************************************************
* usage()
*******************************************************************************/
static void usage(int level)
{
    qerror(
      "usage: %s [-h] [-v [0-6]] [-q] [-t [-Z]] [-i | -I] [-x] [-z] [-e] [-c] [-D]\n"
      "    [-k kernel_file] [-d extension] ... [-r directory] ...\n"
// -j currently masked out of code
      "    [-l | -m | -n | -A | -a kext_id@address ...] [-s directory]\n"
      "    [-p personality] ... [-b bundle_id] ... [--] [extension] ...\n"
      "\n",
      progname);

    if (level < 1) {
        return;
    }

    if (level == 1) {
        qerror("use %s -h for an explanation of each option\n",
            progname);
        return;
    }

    qerror("  extension: the kext bundle to load\n");
    qerror(
        "  -a kext_id@address: kext_id is loaded at address\n");
    qerror(
        "  -A: get load addresses for all kexts from those in the kernel\n");
    qerror(
        "  -b bundle_id: load/use the kext whose CFBundleIdentifier is "
        "bundle_id\n");
    qerror(
        "  -c: don't use repository caches; scan repository folders\n");
    qerror(
        "  -d extension: consider extension as a candidate dependency\n");
    qerror(
        "  -D: don't check for loaded kexts when resolving "
        "dependencies (obsolete; setting is now on by default)\n");
    qerror("  -e: don't examine /System/Library/Extensions\n");
    qerror("  -h: print this message\n");
    qerror(
        "  -i: interactive mode\n");
    qerror(
        "  -I: interactive mode for extension and all its dependencies\n");
#if ALLOW_NO_START
    qerror(
        "  -j: just load; don't even start the extension running\n");
#endif
    qerror(
        "  -k kernel_file: link against kernel_file (default is /mach)\n");
    qerror(
        "  -l: load & start only; don't start matching\n");
    qerror(
        "  -L: same as -r (remains for backward compatibility)\n");
    qerror(
        "  -m: don't load but do start matching\n");
    qerror(
        "  -n: neither load nor start matching\n");
    qerror(
        "  -p personality: send the named personality to the catalog\n");
#if ALLOW_PATCH_OUTPUT
    qerror(
        "  -P directory: write the patched binary file into directory\n");
#endif ALLOW_PATCH_OUTPUT
    qerror(
        "  -q: quiet mode: print no informational or error messages\n");
    qerror(
        "  -r directory: use directory as a repository of dependency kexts\n");
    qerror(
        "  -s directory: write symbol files for all kexts into directory\n");
    qerror("  -t: perform all possible tests and print a report on "
        "the extension\n");
    qerror("  -v: verbose mode; print info about load process\n");
    qerror(
        "  -x: run in safe boot mode (only qualified kexts will load)\n");
    qerror(
        "  -z: don't authenticate kexts (for use during development)\n");
    qerror(
        "  -Z: don't check dependencies when diagnosing with -nt\n");
    qerror(
        "  --: end of options\n");

    return;
}
