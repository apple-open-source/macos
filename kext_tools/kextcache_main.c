#include <CoreFoundation/CoreFoundation.h>
#include <libc.h>
#include <Kernel/libsa/mkext.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <IOKit/kext/KXKextManager.h>

/*******************************************************************************
* Global variables.
*******************************************************************************/

char * progname = "(unknown)";

/*******************************************************************************
* Extern functions.
*******************************************************************************/

// In compression.c
__private_extern__ u_int8_t *
encodeLZSS(u_int8_t *dstP, long dstLen, u_int8_t *srcP, long srcLen);
__private_extern__ void
checkLZSS(u_int8_t *codeP, u_int8_t *srcEnd, u_int8_t *textP, u_int32_t tLen);

// In arch.c
__private_extern__ void
find_arch(u_int8_t **dataP, off_t *sizeP, cpu_type_t in_cpu,
    cpu_subtype_t in_cpu_subtype, u_int8_t *data_ptr, off_t filesize);
__private_extern__ int
get_arch_from_flag(char *name, cpu_type_t *cpuP, cpu_subtype_t *subcpuP);

// in mkext_file.c
Boolean createMkextArchive(CFDictionaryRef kextDict,
    const char * mkextFilename,
    const char * archName,
    cpu_type_t archCPU,
    cpu_subtype_t archSubtype,
    int verbose_level);

/*******************************************************************************
* Utility and callback functions.
*******************************************************************************/
char * CFURLCopyCString(CFURLRef anURL);
static void addKextForMkextCache(
    KXKextRef theKext,
    CFMutableDictionaryRef checkDictionary,
    cpu_type_t archCPU,
    cpu_subtype_t archSubtype,
    Boolean local,
    Boolean network,
    int verbose_level,
    Boolean do_tests);
static void collectKextsForMkextCache(
    CFArrayRef kexts,
    CFMutableDictionaryRef checkDictionary,
    cpu_type_t archCPU,
    cpu_subtype_t archSubtype,
    Boolean local,
    Boolean network,
    int verbose_level,
    Boolean do_tests);

static void verbose_log(const char * format, ...);
static void error_log(const char * format, ...);
static int user_approve(int default_answer, const char * format, ...);
static const char * user_input(const char * format, ...);
static Boolean addKextsToManager(
    KXKextManagerRef aManager,
    CFArrayRef kextNames,
    CFMutableArrayRef kextNamesToUse,
    Boolean do_tests);
static void usage(int level);

/*******************************************************************************
*******************************************************************************/

int main(int argc, const char * argv[])
{
    int exit_code = 0;
    int optchar;
    KXKextManagerRef theKextManager = NULL;  // must release
    KXKextManagerError result;

    CFIndex i, count;

   /*****
    * Set by command line option flags.
    */
    Boolean do_tests = false;                // -t
    Boolean forkExit = false;                // -F
    Boolean local_for_repositories = false;  // -l
    Boolean network_for_repositories = false;// -n
    Boolean local_for_all = false;           // -L
    Boolean network_for_all = false;         // -N
#if 0 // unsupported
    Boolean patch_vtables = false;           // -p
#endif 0
    Boolean repositoryCaches = false;        // -k
    char * mkextFilename = NULL;             // -m; don't release
    Boolean pretend_authentic = false;       // -z

    // -a for these three
    Boolean archSet = false;
    const char * archName = "any";
    cpu_type_t archCPU = CPU_TYPE_ANY;
    cpu_subtype_t archSubtype = CPU_SUBTYPE_MULTIPLE;

    int verbose_level = 0;                   // -v

    CFMutableArrayRef repositoryDirectories = NULL;  // must release
    CFMutableArrayRef repositories = NULL;           // must release
    CFMutableArrayRef kextNames = NULL;              // args; must release
    CFMutableArrayRef kextNamesToUse = NULL;         // must release
    CFMutableArrayRef namedKexts = NULL;             // must release
    CFMutableArrayRef repositoryKexts = NULL;        // must release
    CFMutableDictionaryRef checkDictionary = NULL;   // must release

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


    /*****
    * Process command line arguments. If running in kextload-compatibiliy
    * mode, accept its old set of options and none other. If running in
    * the new mode, process the new, larger set of options.
    */
    while ((optchar = getopt(argc, (char * const *)argv,
               "a:eFklLm:nNptvz")) != -1) {

        switch (optchar) {

          case 'a':
            if (archSet) {
                fprintf(stderr, "duplicate use of -%c option\n\n", optchar);
                usage(0);
                exit_code = 1;
                goto finish;
            }
            if (!get_arch_from_flag(optarg, &archCPU, &archSubtype)) {
                fprintf(stderr, "unknown CPU arch %s\n\n", optarg);
                usage(0);
                exit_code = 1;
                goto finish;
            }
            archName = optarg;
            archSet = true;
            break;

          case 'e':
            if (mkextFilename) {
                fprintf(stderr, "an mkext filename has already been set\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            mkextFilename = "/System/Library/Extensions.mkext";
            CFArrayAppendValue(repositoryDirectories,
                CFSTR("/System/Library/Extensions"));
            break;

          case 'F':
            forkExit = true;
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

#if 0 //unsupported
          case 'p':
            patch_vtables = true;
            break;
#endif 0

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

          case 'z':
            pretend_authentic = true;
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
    argc -= optind;
    argv += optind;

   /*****
    * Check for bad combinations of options.
    */
    if (!mkextFilename && !repositoryCaches) {
        fprintf(stderr, "no work to do; one of -m or -k must be specified\n");
        usage(0);
        exit_code = 1;
        goto finish;
    }

   /*****
    * Record the kext/directory names from the command line.
    */
    for (i = 0; i < argc; i++) {
        CFStringRef argString = CFStringCreateWithCString(kCFAllocatorDefault,
              argv[i], kCFStringEncodingMacRoman);
        if (!argString) {
            fprintf(stderr, "memory allocation failure\n");
            exit_code = 1;
            goto finish;
        }

        // FIXME: Use a more robust test here?
        if (CFStringHasSuffix(argString, CFSTR(".kext")) ||
            CFStringHasSuffix(argString, CFSTR(".kext/")) ) {
            CFArrayAppendValue(kextNames, argString);
        } else {
            CFArrayAppendValue(repositoryDirectories, argString);
        }
        CFRelease(argString);
    }

    if (CFArrayGetCount(kextNames) == 0 &&
        CFArrayGetCount(repositoryDirectories) == 0) {

        fprintf(stderr, "no kernel extensions or directories specified\n\n");
        usage(0);
        exit_code = 1;
        goto finish;
    }

    if (forkExit) {
        if (verbose_level >= 1) {
            verbose_log("forking a child process to do the work\n");
        }

        switch (fork()) {
          case -1:
            fprintf(stderr, "cannot fork\n");
            exit_code = 1;
            goto finish;
            break;
          case 0:   // child task
            setpriority(PRIO_PROCESS, getpid(), 20); // run at really low priority
            break;
          default:  // parent task
            exit_code = 0;
            goto finish;
            break;
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

    result = KXKextManagerInit(theKextManager, false /* load_in_task */,
        false /* safeBoot */);
    if (result != kKXKextManagerErrorNone) {
        fprintf(stderr, "can't initialize kernel extension manager (%s)\n",
            KXKextManagerErrorStaticCStringForError(result));
        exit_code = 1;
        goto finish;
    }

    KXKextManagerSetPerformsFullTests(theKextManager, do_tests);
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
            fprintf(stderr, "memory allocation failure\n");
            exit_code = 1;

            goto finish;
        }

        result = KXKextManagerAddRepositoryDirectory(theKextManager,
            directoryURL, true /* scanForKexts */,
            false /* use_repository_caches */, &repository);
        if (result != kKXKextManagerErrorNone) {
            fprintf(stderr, "can't add repository (%s).\n",
                KXKextManagerErrorStaticCStringForError(result));
            exit_code = 1;
            goto finish;
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
    if (!addKextsToManager(theKextManager, kextNames, kextNamesToUse, do_tests)) {
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
            KXKextManagerError kmResult = KXKextRepositoryWriteCache(
                repository, NULL);
            // FIXME: Do anything with kmResult?
        }
    }

   /*****
    * Do the mkext cache file.
    */
    if (!mkextFilename) {
        goto finish;
    }

    // FIXME: Check that mkextFilename can be created & written to.

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

    collectKextsForMkextCache(namedKexts, checkDictionary,
        archCPU, archSubtype,
        local_for_all, network_for_all,  
        verbose_level, do_tests);
    collectKextsForMkextCache(repositoryKexts, checkDictionary,
        archCPU, archSubtype, local_for_repositories || local_for_all,
        network_for_repositories || network_for_all,
        verbose_level, do_tests);

    if (!createMkextArchive(checkDictionary, mkextFilename,
        archName, archCPU, archSubtype, verbose_level)) {
        exit_code = 1;
        goto finish;
    }

finish:

   /*****
    * Clean everything up.
    */
    if (repositoryDirectories) CFRelease(repositoryDirectories);
    if (kextNames)             CFRelease(kextNames);
    if (kextNamesToUse)        CFRelease(kextNamesToUse);
    if (namedKexts)            CFRelease(namedKexts);

    if (theKextManager)        CFRelease(theKextManager);

    exit(exit_code);
    return exit_code;
}

/*******************************************************************************
*
*******************************************************************************/
char * CFURLCopyCString(CFURLRef anURL)
{
    char * string = NULL; // returned
    CFIndex bufferLength;
    CFURLRef absURL = NULL;        // must release
    CFStringRef urlString = NULL;  // must release
    Boolean error = false;

    absURL = CFURLCopyAbsoluteURL(anURL);
    if (!absURL) {
        goto finish;
    }

    urlString = CFURLCopyFileSystemPath(absURL, kCFURLPOSIXPathStyle);
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
    if (absURL)    CFRelease(absURL);
    if (urlString) CFRelease(urlString);
    return string;
}


/*******************************************************************************
*
*******************************************************************************/
static void addKextForMkextCache(
    KXKextRef theKext,
    CFMutableDictionaryRef checkDictionary,
    cpu_type_t archCPU,
    cpu_subtype_t archSubtype,
    Boolean local,
    Boolean network,
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
    char * executable_path = NULL;  // must free

    int fd = -1;         // sentinel value for close() call at finish
    caddr_t machO = (caddr_t)-1; // sentinel value for munmap() call at finish
    off_t machOSize = 0;

    kextName = KXKextCopyAbsolutePath(theKext);
    if (!kextName) {
        fprintf(stderr, "memory allocation failure\n");
        exit(1);
    }

    if (!CFStringGetCString(kextName, kext_name,
        sizeof(kext_name) / sizeof(char), kCFStringEncodingMacRoman)) {

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

    if (local || network) {
        requiredString = CFBundleGetValueForInfoDictionaryKey(kextBundle,
            CFSTR("OSBundleRequired"));
    
        if (!requiredString) {

            if (verbose_level >= 3) {
                verbose_log("skipping bundle %s; no OSBundleRequired key "
                    "(still checking plugins)\n",
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

        } else {
            char required_string[120];

           // skip for any other value

           if (!CFStringGetCString(requiredString, required_string,
               sizeof(required_string) / sizeof(char),
               kCFStringEncodingASCII)) {

               strcpy(required_string, "(unknown)");
            }

            if (verbose_level >= 3) {
                verbose_log(
                    "skipping bundle %s; OSBundleRequired key is \"%s\" "
                    "(still checking plugins)\n",
                    kext_name, required_string);
            }
            include_it = false;
            do_children = true;
            goto finish;
        }
    }

    KXKextAuthenticate(theKext);

    if (!KXKextIsAuthentic(theKext)) {
        fprintf(stderr, "kernel extension %s is not authentic; "
             "skipping it and any plugins\n", kext_name);
        if (do_tests) {
            fprintf(stderr, "kext diagnostics:\n");
            KXKextPrintDiagnostics(theKext, stderr);
        }
        include_it = false;
        do_children = false;
        goto finish;
    }

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
            executable_path = CFURLCopyCString(executableURL);
            if (!executable_path) {
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
                   machO, machOSize);
                if (!archSize) { // Couldn't find an architecture
                    fprintf(stderr, "%s doesn't contain code for the "
                        "architecture specified; "
                        "skipping the kext that contains it\n",
                        executable_path);
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
            checkDictionary, archCPU, archSubtype, local, network,
            verbose_level, do_tests);
    }

    if (kextName)        CFRelease(kextName);
    if (executableURL)   CFRelease(executableURL);
    if (executable_path) free(executable_path);

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
            local, network, verbose_level, do_tests);
    }

finish:

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
      "usage: %s [-a arch] [-e] [-F] [-k] [-l | -L] [-m mkext_filename]\n"
      "    [-n | -N] [-t] [-v [1-6]] [-z] [kext_or_directory] ...\n\n",
      progname);

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
        "  -e: Create or update the mkext cache for /System/Library/Extensions\n");
    fprintf(stderr,
        "  -F: Fork and exit immediately (to work in background)\n");
    fprintf(stderr,
        "  -k: Create or update any repository .kextcache files\n");
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
        "  -t: Perform diagnostic tests on kexts and print results\n");
    fprintf(stderr,
        "  -v: verbose mode; print info about caching process\n");
    fprintf(stderr,
        "  -z: don't authenticate kexts (for use during development)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "At least one of -k or -m must be specified.\n");
    fprintf(stderr,
        "-l/-L and -n/-N may both be specified to make a cache of local-\n"
        "and network-root kexts\n");

    return;
}
