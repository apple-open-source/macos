#include <CoreFoundation/CoreFoundation.h>
#include <Security/Authorization.h>
#include <libc.h>


#include <IOKit/kext/KXKextManager.h>
#include <IOKit/kext/KextManagerPriv.h>

#define ALLOW_PATCH_OUTPUT  0
#define ALLOW_NO_START      0

/*******************************************************************************
* Global variables.
*******************************************************************************/

static char * progname = "(unknown)";

extern char ** environ;
#define KEXTD_LAUNCH         "KEXTD_LAUNCH_USERID="
#define KEXTD_LAUNCH_FORMAT  "KEXTD_LAUNCH_USERID=%d"
#define KEXTD_AUTHORIZATION         "KEXTD_AUTHORIZATION="
#define KEXTD_AUTHORIZATION_FORMAT  "KEXTD_AUTHORIZATION=%d"

/*******************************************************************************
* Utility and callback functions.
*******************************************************************************/

static Boolean check_file(const char * filename);
static Boolean check_dir(const char * dirname, int writeable);
static void verbose_log(const char * format, ...);
static void error_log(const char * format, ...);
static int user_approve(int default_answer, const char * format, ...);
static const char * user_input(const char * format, ...);
static Boolean addKextsToManager(
    KXKextManagerRef aManager,
    CFArrayRef kextNames,
    CFMutableArrayRef kextArray,
    Boolean do_tests);
static void usage(int level);

static CFDataRef createXMLDataForNonsecureKextload(
    int argc,
    const char ** argv,
    const char * kext_path);

KXKextManagerError _KXKextRaiseSecurityAlert(KXKextRef aKext, uid_t euid, AuthorizationRef auth_ref);
KXKextManagerError _KXKextMakeSecure(KXKextRef aKext);
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
    Boolean do_load,
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
    int verbose_level = 0;                   // -v
    Boolean safe_boot_mode = false;          // -x
    Boolean pretend_authentic = false;       // -z
    Boolean skip_dependencies = false;       // -Z (and with -t only!)
    Boolean check_loaded_for_dependencies = true; // -D to turn off

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

    char ** envp = NULL;
    Boolean kextd_launch = false;
    Boolean kextd_launch_with_ref = false;
    uid_t kextd_launch_userid = -1;
    AuthorizationRef kextd_authref = NULL;
    int kextd_auth_mbox = -1;
    AuthorizationExternalForm auth_ext_form;

    for (envp = environ; *envp; envp++) {
        char * env_string = *envp;
        if (!strncmp(env_string, KEXTD_LAUNCH, strlen(KEXTD_LAUNCH))) {
            kextd_launch = true;
            if (sscanf(env_string, KEXTD_LAUNCH_FORMAT, &kextd_launch_userid) != 1) {
                kextd_launch_userid = -1;
            }
        }
        if (!strncmp(env_string, KEXTD_AUTHORIZATION, strlen(KEXTD_AUTHORIZATION)) 
            && (sscanf(env_string, KEXTD_AUTHORIZATION_FORMAT, &kextd_auth_mbox) == 1)
            && !lseek(kextd_auth_mbox, 0, SEEK_SET)
            && (read(kextd_auth_mbox, &auth_ext_form, sizeof(auth_ext_form)) == 
                sizeof(auth_ext_form))
            && (errAuthorizationSuccess == AuthorizationCreateFromExternalForm(&auth_ext_form, &kextd_authref))) {
            kextd_launch_with_ref = true;
        }
    }
    

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
        fprintf(stderr, "memory allocation failure\n");
        goto finish;
    }
    bzero(addresses, addresses_cap * sizeof(char *));

    personalityNames = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!personalityNames) {
        exit_code = 1;
        fprintf(stderr, "memory allocation failure\n");
        goto finish;
    }

    dependencyNames = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!dependencyNames) {
        exit_code = 1;
        fprintf(stderr, "memory allocation failure\n");
        goto finish;
    }

    repositoryDirectories = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!repositoryDirectories) {
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

    kextIDs = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!kextIDs) {
        exit_code = 1;
        fprintf(stderr, "memory allocation failure\n");
        goto finish;
    }

    /*****
    * Process command line arguments. If running in kextload-compatibiliy
    * mode, accept its old set of options and none other. If running in
    * the new mode, process the new, larger set of options.
    */
// -j option currently masked out
    while ((optchar = getopt(argc, (char * const *)argv,
               "a:Ab:cd:DehiIk:lLmnp:P:r:s:tvxzZ")) != -1) {

        char * address_string = NULL;  // don't free
        unsigned int address;
        CFStringRef optArg = NULL;    // must release

        switch (optchar) {
          case 'a':
            flag_n = 1;  // -a implies -n

            if (!optarg) {
                fprintf(stderr, "no argument for -a\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            address_string = index(optarg, '@');
            if (!address_string) {
                fprintf(stderr, "invalid use of -a option\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            address_string++;
            address = strtoul(address_string, NULL, 16);
            if (!address) {
                fprintf(stderr,
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
                    fprintf(stderr,
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
                fprintf(stderr, "no argument for -b\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            optArg = CFStringCreateWithCString(kCFAllocatorDefault,
                optarg, kCFStringEncodingMacRoman);
            if (!optArg) {
                fprintf(stderr, "memory allocation failure\n");
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
                fprintf(stderr, "no argument for -d\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            optArg = CFStringCreateWithCString(kCFAllocatorDefault,
                optarg, kCFStringEncodingMacRoman);
            if (!optArg) {
                fprintf(stderr, "memory allocation failure\n");
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
                fprintf(stderr, "use only one of -i or -I\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            overwrite_symbols = false;
            interactive_level = 1;
            break;

          case 'I':
            if (interactive_level) {
                fprintf(stderr, "use only one of -i or -I\n\n");
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
                fprintf(stderr, "duplicate use of -k option\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            if (!optarg) {
                fprintf(stderr, "no argument for -k\n");
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
                fprintf(stderr, "no argument for -p\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            optArg = CFStringCreateWithCString(kCFAllocatorDefault,
                optarg, kCFStringEncodingMacRoman);
            if (!optArg) {
                fprintf(stderr, "memory allocation failure\n");
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
                fprintf(stderr, "duplicate use of -P option\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            if (!optarg) {
                fprintf(stderr, "no argument for -P\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            patch_dir = optarg;
            break;
#endif

          case 'r':
            /* fall through */
          case 'L':
            if (!optarg) {
                fprintf(stderr, "no argument for -%c\n", optchar);
                usage(0);
                exit_code = 1;
                goto finish;
            }
            optArg = CFStringCreateWithCString(kCFAllocatorDefault,
               optarg, kCFStringEncodingMacRoman);
            if (!optArg) {
                fprintf(stderr, "memory allocation failure\n");
                exit_code = 1;
                goto finish;
            }
            CFArrayAppendValue(repositoryDirectories, optArg);
            CFRelease(optArg);
            optArg = NULL;
            break;

          case 's':
            if (symbol_dir) {
                fprintf(stderr, "duplicate use of -s option\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            if (!optarg) {
                fprintf(stderr, "no argument for -s\n");
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

                if (verbose_level > 0) {
                    fprintf(stderr, "duplicate use of -v option\n\n");
                    usage(0);
                    exit_code = 1;
                    goto finish;
                }
                if (optind >= argc) {
                    verbose_level = 1;
                } else {
                    next = argv[optind];
                    if ((next[0] == '1' || next[0] == '2' ||
                         next[0] == '3' || next[0] == '4' ||
                         next[0] == '5' || next[0] == '6') &&
                         next[1] == '\0') {

                        verbose_level = atoi(next);
                        optind++;
                    } else {
                        verbose_level = 1;
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
            fprintf(stderr, "unknown option -%c\n", optchar);
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
        fprintf(stderr, "only one of -l/-m/-n is allowed"
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
        fprintf(stderr, "you must be running as root "
            "to load modules into the kernel\n");
        exit_code = 1;
        goto finish;
    }

    if (num_addresses > 0 && get_addrs_from_kernel) {
        fprintf(stderr, "don't use -a with -A\n");
        usage(0);
        exit_code = 1;
        goto finish;
    }

    if (num_addresses > 0 && (do_load || do_start_matching)) {
        fprintf(stderr, "don't use -a with -l or -m\n");
        usage(0);
        exit_code = 1;
        goto finish;
    }

    if (get_addrs_from_kernel && (do_load || do_start_matching)) {
        fprintf(stderr, "don't use -A with -l or -m\n");
        usage(0);
        exit_code = 1;
        goto finish;
    }

    // FIXME: Is this a valid restriction? The default kernel_file
    // FIXME: ...is /mach, which is written out by the kernel during
    // FIXME: ...boot, and always represents the currently running kernel.
    //
    if (kernel_file && (do_load || do_start_matching)) {
        fprintf(stderr, "use -k only with -n\n");
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
        fprintf(stderr, "-z is only allowed when not loading\n");
        usage(0);
        exit_code = 1;
        goto finish;
    }

   /****
    * If we're getting addresses from the kernel we have to call
    * down there, so we might as well check what's loaded before
    * resolving dependencies too (-A overrides -D). If we're not
    * performing a load, then don't check (-n/-m implies -D).
    */
    if (get_addrs_from_kernel) {
        check_loaded_for_dependencies = true;
    } else if (!do_load) {
        check_loaded_for_dependencies = false;
    }

   /*****
    * If we're not loading and have no request to emit a symbol
    * or patch file, there's nothing to do!
    */
    if (!do_tests && !do_load && !do_start_matching &&
        !symbol_dir && !patch_dir) {

        fprintf(stderr, "no work to do; check your options\n\n");
        usage(0);
        exit_code = 1;
        goto finish;
    }

   /*****
    * -Z is only allowed if you don't need to generate dependencies.
    */
    if (skip_dependencies && (!do_tests || do_load || symbol_dir || patch_dir)) {
#if ALLOW_PATCH_OUTPUT
        fprintf(stderr, "use -Z only with -nt and not with -s or -P\n");
#else
        fprintf(stderr, "use -Z only with -nt and not with -s\n");
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
            fprintf(stderr, "memory allocation failure\n");
            exit_code = 1;
            goto finish;
        }
        CFArrayAppendValue(kextNames, kextName);
        CFRelease(kextName);
    }

    if (CFArrayGetCount(kextNames) == 0 && CFArrayGetCount(kextIDs) == 0) {
        fprintf(stderr, "no kernel extension specified\n\n");
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
        if (!do_load && verbose_level >= 1) {
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
        fprintf(stderr, "can't allocate kernel extension manager\n");
        exit_code = 1;
        goto finish;
    }

    result = KXKextManagerInit(theKextManager, true /* load_in_task */,
        safe_boot_mode);
    if (result != kKXKextManagerErrorNone) {
        fprintf(stderr, "can't initialize kernel extension manager (%s)\n",
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
    */
    if (!addKextsToManager(theKextManager, dependencyNames, NULL, do_tests)) {
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
            fprintf(stderr, "memory allocation failure\n");
            exit_code = 1;

            goto finish;
        }

        result = KXKextManagerAddRepositoryDirectory(theKextManager,
            directoryURL, true /* scanForKexts */,
            use_repository_caches, NULL);
        if (result != kKXKextManagerErrorNone) {
            fprintf(stderr, "can't add repository (%s).\n",
                KXKextManagerErrorStaticCStringForError(result));
            exit_code = 1;
            goto finish;
        }
        CFRelease(directoryURL);
        directoryURL = NULL;
    }

   /*****
    * Add each kext named on the command line to the manager.
    */
    if (!addKextsToManager(theKextManager, kextNames, kextNamesToUse, do_tests)) {
        exit_code = 1;
        goto finish;
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

            fprintf(stderr, "internal error; no memory?\n");
            exit_code = 1;
            goto finish;
        }
        if (verbose_level >= 1) {
            verbose_log("looking up extension with identifier %s\n",
                name_buffer);
        }

        if (!theKext) {
            if (!CFStringGetCString(thisKextID,
                name_buffer, sizeof(name_buffer) - 1,
                kCFStringEncodingMacRoman)) {

                fprintf(stderr, "internal error; no memory?\n");
                exit_code = 1;
                goto finish;
            }
            fprintf(stderr, "can't find extension with identifier %s\n",
                name_buffer);
            exit_code = 1;
            goto finish;
        }
        kextName = KXKextCopyAbsolutePath(theKext);
        if (!kextName) {
            fprintf(stderr, "memory allocation failure\n");
            exit_code = 1;
            goto finish;
        }
        if (!CFStringGetCString(kextName,
            name_buffer, sizeof(name_buffer) - 1, kCFStringEncodingMacRoman)) {

            fprintf(stderr, "internal error; no memory?\n");
            exit_code = 1;
            goto finish;
        }
        if (verbose_level >= 1) {
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
            fprintf(stderr, "memory allocation failure\n");
            goto finish;
        }

        if (kextURL) {
            CFRelease(kextURL);
            kextURL = NULL;
        }

        kextName = CFArrayGetValueAtIndex(kextNamesToUse, i);
        if (!CFStringGetCString(kextName,
            kext_name, sizeof(kext_name) - 1, kCFStringEncodingMacRoman)) {

            fprintf(stderr, "memory allocation or string conversion failure\n");
            exit_code = 1;
            goto finish;
        }

        kextURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
            kextName, kCFURLPOSIXPathStyle, true);
        if (!kextURL) {
            fprintf(stderr, "memory allocation failure\n");
            exit_code = 1;
            goto finish;
        }

        theKext = KXKextManagerGetKextWithURL(theKextManager, kextURL);

        if (!theKext) {
            fprintf(stderr, "can't find extension %s in database\n",
                kext_name);
            failure_code = 1;
            continue;
        }

       /*****
        * Confirm kext is loadable.
        */
        if (safe_boot_mode && !KXKextIsEligibleDuringSafeBoot(theKext)) {
            fprintf(stderr, "extension %s is not eligible for safe boot "
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

            fprintf(stderr, "kernel extension %s has problems", kext_name);
            if (do_tests) {
                fprintf(stderr, ":\n");
                KXKextPrintDiagnostics(theKext, stderr);
            } else {
                fprintf(stderr, " (run %s with -t for diagnostic output)\n",
                    progname);
            }
            failure_code = 1;
        }

        if (failure_code) {
            continue;
        }

        if (do_tests || verbose_level > 0) {
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
                fprintf(stderr, "error reading response\n");
                failure_code = 1;
                goto finish;
            } else if (approve == 0) {
                fprintf(stderr, "not loading extension %s\n", kext_name);
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
                uid_t login_euid = -1;
                KXKextRef thisKext =
                    (KXKextRef)CFArrayGetValueAtIndex(inauthenticKexts, k);

                if (kextd_launch) {
                    login_euid = kextd_launch_userid;
                } else {
                    login_euid = _KextManagerGetLoggedInUserid();
                }

                if (login_euid == -1) {
                    if (!kextd_launch) {
                        const char * kext_path = NULL; // must free
                        CFDataRef xmlData = NULL; // must release
                        const char * rpc_data = NULL; // don't free
                        size_t data_length = 0;

                        kext_path = _KXKextCopyCanonicalPathnameAsCString(thisKext);
                        if (!kext_path) {
                            fprintf(stderr, "memory allocation failure\n");
                            exit_code = 1;
                            goto finish;
                        }
                        xmlData = createXMLDataForNonsecureKextload(
                            argc_opt_count, argv,
                            kext_path);
                        if (!xmlData) {
                            fprintf(stderr, "memory allocation failure\n");
                            exit_code = 1;
                            goto finish;
                        }
                        rpc_data = CFDataGetBytePtr(xmlData);
                        data_length = CFDataGetLength(xmlData);
                        fprintf(stderr,
                            "no user logged in, can't offer to fix kext %s; "
                            "informing kextd\n",
                            kext_name);
                        _KextManagerRecordNonsecureKextload(rpc_data, data_length);
                        if (kext_path) free((char *)kext_path);
                        if (xmlData) CFRelease(xmlData);
                        failure_code = 1;
                        break;
                    }
                } else {
                    const char * kext_path = NULL; // must free
                    CFDataRef xmlData = NULL; // must release
                    const char * rpc_data = NULL; // don't free
                    size_t data_length = 0;

                    kext_path = _KXKextCopyCanonicalPathnameAsCString(thisKext);
                    if (!kext_path) {
                        fprintf(stderr, "memory allocation failure\n");
                        exit_code = 1;
                        goto finish;
                    }

                   /* Raise the security alert. If this fails on an IPC error,
                    * tell kextd using _KextManagerRecordNonsecureKext(),
                    * just in case.
                    */
                    
                    if (!kextd_launch_with_ref)
                        AuthorizationCreate(NULL, NULL, 0, &kextd_authref);
                    
                    switch ( _KXKextRaiseSecurityAlert(thisKext, login_euid, kextd_authref)) {
                      case kKXKextManagerErrorNone:
                        break;
                      case kKXKextManagerErrorIPC:
                        if (!kextd_launch) {
                            xmlData = createXMLDataForNonsecureKextload(
                                argc_opt_count, argv, kext_path);
                            if (!xmlData) {
                                fprintf(stderr, "memory allocation failure\n");
                                exit_code = 1;
                                goto finish;
                            }
                            rpc_data = CFDataGetBytePtr(xmlData);
                            data_length = CFDataGetLength(xmlData);
                            _KextManagerRecordNonsecureKextload(rpc_data,
                                data_length);
                            if (xmlData) CFRelease(xmlData);
                        }
                        break;
                      case kKXKextManagerErrorUserAbort:
                        fprintf(stderr, "load aborted for extension %s\n",
                            kext_name);
                        /* fall through */
                      default:
                        failure_code = 1;
                        break;
                    }

                    if (kext_path) free((char *)kext_path);

                    if (failure_code) {
                        break;  /* for loop */
                    }
                }
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
                    fprintf(stderr, "load aborted for extension %s\n",
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
                        fprintf(stderr, "error reading response\n");
                        failure_code = 1;
                        goto finish;
                    } else if (approve == 0) {
                        fprintf(stderr, "skipping extension %s\n", kext_name);
                        continue;
                    }
                } else {
                    fprintf(stderr, "rescanning all kexts due to cache inconsistency\n");
                }
                KXKextManagerResetAllRepositories(theKextManager);
                cache_retry_count--;
                goto retry;
            } else if (result != kKXKextManagerErrorNone) {
                if (do_load) {
                    fprintf(stderr, "load failed for extension %s\n",
                        kext_name);
                }
                if (do_tests && !KXKextIsLoadable(theKext, safe_boot_mode)) {
                    fprintf(stderr, "kernel extension problems:\n");
                    KXKextPrintDiagnostics(theKext, stderr);
                } else {
                    fprintf(stderr, " (run %s with -t for diagnostic output)\n",
                        progname);
                }
                failure_code = 1;
                continue;
            }

            if (do_load) {
                verbose_log("%s loaded successfully", kext_name);
            }
        } else if (do_load && verbose_level >= 1) {
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
                (interactive_level || verbose_level >= 1) ) {
                verbose_log("matching started for %s", kext_name);
            } else if (result != kKXKextManagerErrorNone) {
                failure_code = 1;
                if (interactive_level || verbose_level >= 1) {
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
    AuthorizationFree(kextd_authref, 0);
    if (kextd_auth_mbox >= 0) close (kextd_auth_mbox);
    
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
        fprintf(stderr, "%s is not a regular file\n", filename);
        result = false;
        goto finish;
    }

    if (access(filename, R_OK) != 0) {
        fprintf(stderr, "%s is not readable\n", filename);
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
        fprintf(stderr, "%s is not a directory\n", dirname);
        result = false;
        goto finish;
    }

    if (writeable) {
        if (access(dirname, W_OK) != 0) {
            fprintf(stderr, "%s is not writeable\n", dirname);
            result = false;
            goto finish;
        }
    }
finish:
    return result;
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

    va_start(ap, format);
    output_length = vsnprintf(fake_buffer, 1, format, ap);
    va_end(ap);

    output_string = (char *)malloc(output_length + 1);
    if (!output_string) {
        fprintf(stderr, "memory allocation failure\n");
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

    va_start(ap, format);
    output_length = vsnprintf(fake_buffer, 1, format, ap);
    va_end(ap);

    output_string = (char *)malloc(output_length + 1);
    if (!output_string) {
        fprintf(stderr, "memory allocation failure\n");
        return;
    }

    va_start(ap, format);
    vsprintf(output_string, format, ap);
    va_end(ap);

    va_start(ap, format);
    fprintf(stderr, "%s: %s\n", progname, output_string);
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
        fprintf(stderr, "memory allocation failure\n");
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
        fprintf(stderr, "memory allocation failure\n");
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
*******************************************************************************/
static Boolean addKextsToManager(
    KXKextManagerRef aManager,
    CFArrayRef kextNames,
    CFMutableArrayRef kextNamesToUse,
    Boolean do_tests)
{
    Boolean result = true;     // assume success
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
            fprintf(stderr, "memory allocation failure\n");
            result = false;
            goto finish;
        }

        if (!CFStringGetCString(kextName,
            name_buffer, sizeof(name_buffer) - 1, kCFStringEncodingMacRoman)) {

            fprintf(stderr, "memory allocation or string conversion error\n");
            result = false;
            goto finish;
        }

        kxresult = KXKextManagerAddKextWithURL(aManager, kextURL, true, &theKext);
        if (kxresult != kKXKextManagerErrorNone) {
            fprintf(stderr, "can't add kernel extension %s (%s)",
                name_buffer, KXKextManagerErrorStaticCStringForError(kxresult));
            fprintf(stderr, " (run %s on this kext with -t for diagnostic output)\n",
                progname);
#if 0
            if (do_tests && theKext) {
                fprintf(stderr, "kernel extension problems:\n");
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
    fprintf(stderr,
      "usage: %s [-h] [-v [1-6]] [-t [-Z]] [-i | -I] [-x] [-z] [-e] [-c] [-D]\n"
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
        fprintf(stderr, "use %s -h for an explanation of each option\n",
            progname);
        return;
    }

    fprintf(stderr, "  extension: the kext bundle to load\n");
    fprintf(stderr,
        "  -a kext_id@address: kext_id is loaded at address\n");
    fprintf(stderr,
        "  -A: get load addresses for all kexts from those in the kernel\n");
    fprintf(stderr,
        "  -b bundle_id: load/use the kext whose CFBundleIdentifier is "
        "bundle_id\n");
    fprintf(stderr,
        "  -c: don't use repository caches; scan repository folders\n");
    fprintf(stderr,
        "  -d extension: consider extension as a candidate dependency\n");
    fprintf(stderr,
        "  -D: don't check for loaded kexts when resolving "
        "dependencies\n");
    fprintf(stderr, "  -e: don't examine /System/Library/Extensions\n");
    fprintf(stderr, "  -h: print this message\n");
    fprintf(stderr,
        "  -i: interactive mode\n");
    fprintf(stderr,
        "  -I: interactive mode for extension and all its dependencies\n");
#if ALLOW_NO_START
    fprintf(stderr,
        "  -j: just load; don't even start the extension running\n");
#endif
    fprintf(stderr,
        "  -k kernel_file: link against kernel_file (default is /mach)\n");
    fprintf(stderr,
        "  -l: load & start only; don't start matching\n");
    fprintf(stderr,
        "  -L: same as -r (remains for backward compatibility)\n");
    fprintf(stderr,
        "  -m: don't load but do start matching\n");
    fprintf(stderr,
        "  -n: neither load nor start matching\n");
    fprintf(stderr,
        "  -p personality: send the named personality to the catalog\n");
#if ALLOW_PATCH_OUTPUT
    fprintf(stderr,
        "  -P directory: write the patched binary file into directory\n");
#endif ALLOW_PATCH_OUTPUT
    fprintf(stderr,
        "  -r directory: use directory as a repository of dependency kexts\n");
    fprintf(stderr,
        "  -s directory: write symbol files for all kexts into directory\n");
    fprintf(stderr, "  -t: perform all possible tests and print a report on "
        "the extension\n");
    fprintf(stderr, "  -v: verbose mode; print info about load process\n");
    fprintf(stderr,
        "  -x: run in safe boot mode (only qualified kexts will load)\n");
    fprintf(stderr,
        "  -z: don't authenticate kexts (for use during development)\n");
    fprintf(stderr,
        "  -Z: don't check dependencies when diagnosing with -nt\n");
    fprintf(stderr,
        "  --: end of options\n");

    return;
}

/*******************************************************************************
* createXMLDataForNonsecureKextload()
*******************************************************************************/
static CFDataRef createXMLDataForNonsecureKextload(
    int argc,
    const char ** argv,
    const char * kext_path)
{
    char * working_dir = NULL;  // must free
    CFDataRef xmlData = NULL;    // returned, caller must release
    CFMutableDictionaryRef dataDictionary = NULL;  // must release
    CFDataRef dataValue = NULL;  // scratch value, must release
    CFStringRef dataKey = NULL;  // scratch key, must release
    CFMutableArrayRef argvArray = NULL;  // must release
    int i;

    dataDictionary = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!dataDictionary) {
        goto finish;
    }

    argvArray = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!argvArray) {
        goto finish;
    }

   /* Add the working directory for kextload to the dictionary.
    */
    working_dir = getcwd(NULL, 0);
    if (!working_dir) {
        goto finish;
    }

    dataValue = CFDataCreate(kCFAllocatorDefault, working_dir,
        1 + strlen(working_dir));
    if (!dataValue) {
        goto finish;
    }

    CFDictionarySetValue(dataDictionary, CFSTR("workingDir"),
        dataValue);
    CFRelease(dataValue);
    dataValue = NULL;

   /* Add the path to kextload as argv[0].
    */
    dataValue = CFDataCreate(kCFAllocatorDefault, "/sbin/kextload",
        1 + strlen("/sbin/kextload"));
    if (!dataValue) {
        goto finish;
    }
    CFArrayAppendValue(argvArray, dataValue);
    CFRelease(dataValue);
    dataValue = NULL;

   /* Add the command line options used for this kextload invocation.
    */
    for (i = 1 /* skip slot 0! */; i < argc; i++) {
        if (!strcmp(argv[i], "-b")) {
            i++;  // skip -b and its arg
            continue;
        }
        dataValue = CFDataCreate(kCFAllocatorDefault, argv[i],
            1 + strlen(argv[i]));
        if (!dataValue) {
            goto finish;
        }
        CFArrayAppendValue(argvArray, dataValue);
        CFRelease(dataValue);
        dataValue = NULL;
    }

   /* Add the kext to load.
    */
    dataValue = CFDataCreate(kCFAllocatorDefault, kext_path,
        1 + strlen(kext_path));
    if (!dataValue) {
        goto finish;
    }
    CFArrayAppendValue(argvArray, dataValue);
    CFRelease(dataValue);
    dataValue = NULL;

    CFDictionarySetValue(dataDictionary, CFSTR("argv"),
        argvArray);
    CFRelease(argvArray);
    argvArray = NULL;

   /* Roll up the XML data.
    */
    xmlData = CFPropertyListCreateXMLData(kCFAllocatorDefault,
         dataDictionary);
    if (!xmlData) {
        goto finish;
    }

finish:

     if (working_dir)     free(working_dir);
     if (dataDictionary)  CFRelease(dataDictionary);
     if (dataValue)       CFRelease(dataValue);
     if (dataKey)         CFRelease(dataKey);
     if (argvArray)       CFRelease(argvArray);

    return xmlData;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError _KXKextRaiseSecurityAlert(KXKextRef aKext, uid_t euid, AuthorizationRef auth_ref)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    CFMutableDictionaryRef alertDict = NULL;  // must release (reused)
    CFMutableArrayRef alertMessageArray = NULL; // must release (reused)
    CFURLRef iokitFrameworkBundleURL = NULL;  // must release
    CFUserNotificationRef securityNotification = NULL; // must release
    CFUserNotificationRef failureNotification = NULL; // must release
    SInt32 userNotificationError = 0;
    CFOptionFlags response = 0;
    int userNotificationResult = 0;
    uid_t real_euid = geteuid();

#ifdef NO_CFUserNotification

    result = kKXKextManagerErrorUnspecified;
    goto finish;

#else

    alertDict = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!alertDict) {
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

    iokitFrameworkBundleURL = CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault,
        CFSTR("/System/Library/Frameworks/IOKit.framework"),
        kCFURLPOSIXPathStyle, true);
    if (!iokitFrameworkBundleURL) {
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

    alertMessageArray = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!alertMessageArray) {
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

   /* This is the localized format string for the alert message.
    */
    CFArrayAppendValue(alertMessageArray,
        CFSTR("The file \""));
    CFArrayAppendValue(alertMessageArray, KXKextGetBundleDirectoryName(aKext));
    CFArrayAppendValue(alertMessageArray,
        CFSTR("\" has problems that may reduce the security of "
            "your computer. You should contact the manufacturer of the "
            "product you are using for a new version. If you are sure the "
            "file is OK, you can allow the application to use it, or fix "
            "it and then use it. If you click Don't Use, any other files "
            "that depend on this file will not be used."));

    CFDictionarySetValue(alertDict, kCFUserNotificationLocalizationURLKey,
        iokitFrameworkBundleURL);
    CFDictionarySetValue(alertDict, kCFUserNotificationAlertHeaderKey,
        CFSTR("The program you are using needs to use a system file that may "
              "reduce the security of your computer."));
    CFDictionarySetValue(alertDict, kCFUserNotificationDefaultButtonTitleKey,
        CFSTR("Don't Use"));
    CFDictionarySetValue(alertDict, kCFUserNotificationAlternateButtonTitleKey,
        CFSTR("Fix and Use"));
    CFDictionarySetValue(alertDict, kCFUserNotificationOtherButtonTitleKey,
        CFSTR("Use"));
    CFDictionarySetValue(alertDict, kCFUserNotificationAlertMessageKey,
        alertMessageArray);
    CFRelease(alertMessageArray);
    alertMessageArray = NULL;

    securityNotification = CFUserNotificationCreate(kCFAllocatorDefault,
        0 /* time interval */, kCFUserNotificationCautionAlertLevel,
        &userNotificationError, alertDict);
    if (!securityNotification) {
        fprintf(stderr, 
            "error creating user notification (%d)\n", userNotificationError);
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }
    userNotificationResult = CFUserNotificationReceiveResponse(
        securityNotification, 0, &response);

    if (userNotificationResult != 0) {
        fprintf(stderr, 
            "can't ask user to allow load of nonsecure kext\n");
        result = kKXKextManagerErrorIPC;
        goto finish;
    }

    if (response == kCFUserNotificationDefaultResponse) {
        // The default response is to not load the kext.
        result = kKXKextManagerErrorUserAbort;
        goto finish;
    } else if (response == kCFUserNotificationAlternateResponse ||
               response == kCFUserNotificationOtherResponse) {

        AuthorizationItem fixkextright = { "system.kext.make_secure", 0,
            NULL, 0 };
        const AuthorizationItemSet fixkextrightset = { 1, &fixkextright };
        int flags = kAuthorizationFlagExtendRights |
            kAuthorizationFlagInteractionAllowed;
        OSStatus auth_result = errAuthorizationSuccess;

        if (seteuid(euid) != 0) {
            fprintf(stderr, "call to seteuid() failed\n");
            result = kKXKextManagerErrorUnspecified;
            goto finish;
        }

        auth_result = AuthorizationCopyRights(auth_ref, &fixkextrightset, NULL, flags, NULL);
        
        seteuid(real_euid);

        if (auth_result != errAuthorizationSuccess) {
            result = kKXKextManagerErrorUserAbort;
            goto finish;
        }

        KXKextManagerRequalifyKext(KXKextGetManager(aKext), aKext);

        if (response == kCFUserNotificationAlternateResponse) {
            result = _KXKextMakeSecure(aKext);

           /* If the kext couldn't be made secure (presumably because it's on
            * a read-only or closed network filesystem) then inform the user
            * and mark it authentic so it will be loaded anyway.
            */
            if (result == kKXKextManagerErrorFileAccess) {
                result = kKXKextManagerErrorNone;
                KXKextMarkAuthentic(aKext);

                alertDict = CFDictionaryCreateMutable(
                    kCFAllocatorDefault, 0,
                    &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);
                if (!alertDict) {
                    result = kKXKextManagerErrorUnspecified;
                    goto finish;
                }

                alertMessageArray = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                    &kCFTypeArrayCallBacks);
                if (!alertMessageArray) {
                    result = kKXKextManagerErrorUnspecified;
                    goto finish;
                }

               /* This is the localized format string for the alert message.
                */
                CFArrayAppendValue(alertMessageArray,
                    CFSTR("The file \""));
                CFArrayAppendValue(alertMessageArray,
                    KXKextGetBundleDirectoryName(aKext));
                CFArrayAppendValue(alertMessageArray,
                    CFSTR("\" could not be fixed, but it will be used anyway."));
                CFDictionarySetValue(alertDict, kCFUserNotificationLocalizationURLKey,
                    iokitFrameworkBundleURL);
                CFDictionarySetValue(alertDict, kCFUserNotificationAlertHeaderKey,
                    CFSTR("File Access Error"));
                CFDictionarySetValue(alertDict, kCFUserNotificationDefaultButtonTitleKey,
                    CFSTR("OK"));
                CFDictionarySetValue(alertDict, kCFUserNotificationAlertMessageKey,
                    alertMessageArray);
                CFRelease(alertMessageArray);
                alertMessageArray = NULL;

                failureNotification = CFUserNotificationCreate(kCFAllocatorDefault,
                    0 /* time interval */, kCFUserNotificationCautionAlertLevel,
                    &userNotificationError, alertDict);

                if (!failureNotification) {
                    fprintf(stderr, 
                        "error creating user notification (%d)\n", userNotificationError);
                    result = kKXKextManagerErrorUnspecified;
                    goto finish;
                }
                userNotificationResult = CFUserNotificationReceiveResponse(
                    failureNotification, 0, &response);

                if (userNotificationResult != 0) {
                    fprintf(stderr,
                        "couldn't get response to failure notification\n");
                    result = kKXKextManagerErrorIPC;
                    goto finish;
                }
            }
            goto finish;
        } else if (response == kCFUserNotificationOtherResponse) {
            KXKextMarkAuthentic(aKext);
            goto finish;
        }
    }

#endif /* NO_CFUserNotification */

finish:
    if (alertDict)               CFRelease(alertDict);
    if (alertMessageArray)       CFRelease(alertMessageArray);
    if (securityNotification)    CFRelease(securityNotification);
    if (failureNotification)     CFRelease(failureNotification);
    if (iokitFrameworkBundleURL) CFRelease(iokitFrameworkBundleURL);

    return result;
}
