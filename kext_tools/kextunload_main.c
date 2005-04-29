#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <libc.h>

#include <IOKit/kext/KXKextManager.h>

static char * progname = "(unknown)";
static int verbose_level = kKXKextManagerLogLevelDefault; // -v, -q set this
static mach_port_t gMasterPort;

static char calc_buffer[2] = "";
static int kern_result_buffer_length = 80;
static char * kern_result_buffer;     // for formatting status messages

/*******************************************************************************
*******************************************************************************/

// FIXME: Several of these belong in a separate module
int format_kern_result(kern_return_t result);
static void verbose_log(const char * format, ...);
static void error_log(const char * format, ...);
static void qerror(const char * format, ...);
int addKextsToManager(
    KXKextManagerRef aManager,
    CFArrayRef kextNames,
    CFMutableArrayRef kextArray);
static void usage(int level);

/*******************************************************************************
*******************************************************************************/


int main(int argc, const char *argv[]) {
    int exit_code = 0;
    int optchar;
    KXKextManagerRef myKextManager = NULL;   // must release
    KXKextManagerError result;
    kern_return_t kern_result;

    CFIndex i, count;

   /*****
    * Set by command line option flags.
    */
    Boolean unload_personalities = true;      // -P

    CFMutableArrayRef kextNames = NULL;       // args; must release
    CFMutableArrayRef kextClassNames = NULL;  // -c; must release
    CFMutableArrayRef kextBundleIDs = NULL;   // -b/-m; must release
    CFMutableArrayRef kexts = NULL;           // must release
    KXKextRef theKext = NULL;                 // don't release


   /*****
    * Find out what my name is.
    */
    progname = rindex(argv[0], '/');
    if (progname) {
        progname++;   // go past the '/'
    } else {
        progname = (char *)argv[0];
    }

    kern_result = IOMasterPort(NULL, &gMasterPort);
    if (kern_result != KERN_SUCCESS) {
        qerror("can't get I/O Kit master port\n");
        exit_code = 1;
        goto finish;
    }

   /*****
    * Allocate CF collection objects.
    */
    kextNames = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!kextNames) {
        exit_code = 1;
        qerror("can't allocate memory\n");
        goto finish;
    }

    kextClassNames = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        NULL /* C pointers */);
    if (!kextClassNames) {
        exit_code = 1;
        qerror("can't allocate memory\n");
        goto finish;
    }

    kextBundleIDs = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        NULL /* C pointers */);
    if (!kextBundleIDs) {
        exit_code = 1;
        qerror("can't allocate memory\n");
        goto finish;
    }

    kexts = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!kexts) {
        exit_code = 1;
        qerror("can't allocate memory\n");
        goto finish;
    }


    /*****
    * Process command line arguments.
    */
    while ((optchar = getopt(argc, (char * const *)argv, "b:c:hm:pqv")) != -1) {

        switch (optchar) {
          case 'b':
          case 'm':
            CFArrayAppendValue(kextBundleIDs, optarg);
            break;
          case 'c':
            CFArrayAppendValue(kextClassNames, optarg);
            break;
          case 'h':
            usage(2);
            goto finish;
            break;
          case 'p':
            unload_personalities = false;
            break;
          case 'q':
            if (verbose_level != kKXKextManagerLogLevelDefault) {
                qerror("duplicate use of -v and/or -q option\n\n");
                usage(0);
                exit_code = 1;
                goto finish;
            }
            verbose_level = kKXKextManagerLogLevelSilent;
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
        }
    }

   /* Update argc, argv for building dependency lists.
    */
    argc -= optind;
    argv += optind;

   /*****
    * Record the kext names from the command line.
    */
    for (i = 0; i < argc; i++) {
        CFStringRef kextName = CFStringCreateWithCString(kCFAllocatorDefault,
              argv[i], kCFStringEncodingMacRoman);
        if (!kextName) {
            qerror("Can't create kext name string for \"%s\"; "
                "no memory?\n", argv[i]);
            exit_code = 1;
            goto finish;
        }
        CFArrayAppendValue(kextNames, kextName);
        CFRelease(kextName);
    }


    if (CFArrayGetCount(kextNames) == 0 &&
        CFArrayGetCount(kextBundleIDs) == 0 &&

        CFArrayGetCount(kextClassNames) == 0) {
        qerror("no kernel extension specified\n\n");
        usage(1);
        exit_code = 1;
        goto finish;
    }


   /*****
    * Set up the kext manager.
    */
    myKextManager = KXKextManagerCreate(kCFAllocatorDefault);
    if (!myKextManager) {
        qerror("can't allocate kext manager\n");
        exit_code = 1;
        goto finish;
    }

    result = KXKextManagerInit(myKextManager, true, false);
    if (result != kKXKextManagerErrorNone) {
        qerror("can't initialize manager (%s)\n",
            KXKextManagerErrorStaticCStringForError(result));
        exit_code = 1;
        goto finish;
    }

    KXKextManagerSetLogLevel(myKextManager, verbose_level);
    KXKextManagerSetLogFunction(myKextManager, &verbose_log);
    KXKextManagerSetErrorLogFunction(myKextManager, &error_log);

   /*****
    * Disable clearing of relationships until we're done putting everything
    * together.
    */
    KXKextManagerDisableClearRelationships(myKextManager);

    // FIXME: put the code between the disable/enable in a function

   /*****
    * Add each kext named on the command line to the manager.
    */
    if (!addKextsToManager(myKextManager, kextNames, kexts)) {
        exit_code = 1;
        goto finish;
    }

    KXKextManagerCalculateVersionRelationships(myKextManager);

    kern_result_buffer = (char *)malloc(sizeof(char) * kern_result_buffer_length);
    if (!kern_result_buffer) {
        exit_code = 1;
        goto finish;
    }
    kern_result_buffer[0] = '\0';

   /*****
    * Get busy unloading kexts. First do the class names, then do the
    * bundle identifiers.
    */
    count = CFArrayGetCount(kextClassNames);
    for (i = 0; i < count; i++) {
        char * kext_class_name = NULL;  // don't free

        kext_class_name = (char *)CFArrayGetValueAtIndex(kextClassNames, i);
        kern_result = IOCatalogueTerminate(gMasterPort,
            kIOCatalogServiceTerminate,
            kext_class_name);
        if (kern_result == kIOReturnNotPrivileged) {
             qerror("permission denied; you must be root to unload kexts\n");
             exit_code = 1;
             goto finish;
        }
        if (kern_result != KERN_SUCCESS) {
             if (!format_kern_result(kern_result)) {
                 exit_code = 1;
                 goto finish;
             }
             exit_code = 1;
        }
        verbose_log("terminate instances for class %s %s%s",
            kext_class_name,
            kern_result == KERN_SUCCESS ? "succeeded" : "failed",
            kern_result != KERN_SUCCESS ? kern_result_buffer : "");
        kern_result_buffer[0] = '\0';
    }

    count = CFArrayGetCount(kextBundleIDs);
    for (i = 0; i < count; i++) {
        char * kext_id = NULL;  // don't free

        kext_id = (char *)CFArrayGetValueAtIndex(kextBundleIDs, i);
        kern_result = IOCatalogueTerminate(gMasterPort,
            unload_personalities ? kIOCatalogModuleUnload :
                kIOCatalogModuleTerminate,
            kext_id);
        if (kern_result == kIOReturnNotPrivileged) {
             qerror("permission denied; you must be root to unload kexts\n");
             exit_code = 1;
             goto finish;
        }
        if (kern_result != KERN_SUCCESS) {
             if (!format_kern_result(kern_result)) {
                 exit_code = 1;
                 goto finish;
             }
             exit_code = 1;
        }
        verbose_log("unload id %s %s%s",
            kext_id,
            kern_result == KERN_SUCCESS ? "succeeded" : "failed",
            kern_result != KERN_SUCCESS ? kern_result_buffer :
                (unload_personalities ? " (any personalities also unloaded)" :
                " (any personalities not unloaded)"));
        kern_result_buffer[0] = '\0';
    }

    count = CFArrayGetCount(kexts);
    for (i = 0; i < count; i++) {
        CFStringRef kextName = NULL; // don't release
        CFStringRef kextID = NULL;   // don't release
        char kext_name[MAXPATHLEN];
        char kext_id[255];

        theKext = (KXKextRef)CFArrayGetValueAtIndex(kexts, i);
        kextName = (CFStringRef)CFArrayGetValueAtIndex(kextNames, i);
        kextID = KXKextGetBundleIdentifier(theKext);
        if (!CFStringGetCString(kextName,
            kext_name, sizeof(kext_name) - 1, kCFStringEncodingMacRoman)) {

            exit_code = 1;
            continue;
        }

        if (!CFStringGetCString(kextID,
            kext_id, sizeof(kext_id) - 1, kCFStringEncodingMacRoman)) {

            exit_code = 1;
            continue;
        }

        kern_result = IOCatalogueTerminate(gMasterPort,
            unload_personalities ? kIOCatalogModuleUnload :
                kIOCatalogModuleTerminate,
            kext_id);
        if (kern_result == kIOReturnNotPrivileged) {
             qerror("permission denied; you must be root to unload kexts\n");
             exit_code = 1;
             goto finish;
        }
/* PR-3424027: we are now returning an error for unspecified kernel failures. -DH */
         if (kern_result != KERN_SUCCESS) {
             if (!format_kern_result(kern_result)) {
                 exit_code = 1;
                 goto finish;
             }
             exit_code = 1;
        }
        verbose_log("unload kext %s %s",
            kext_name,
            kern_result == KERN_SUCCESS ? "succeeded" : "failed",
            kern_result != KERN_SUCCESS ? kern_result_buffer :
                (unload_personalities ? " (any personalities also unloaded)" :
                " (any personalities not unloaded)"));
    }

finish:

   /*****
    * Clean everything up.
    */
    if (kextNames)             CFRelease(kextNames);
    if (kextClassNames)        CFRelease(kextClassNames);
    if (kextBundleIDs)         CFRelease(kextBundleIDs);
    if (kexts)                 CFRelease(kexts);

    if (myKextManager)         CFRelease(myKextManager);
        
//    while (1) ;  // for memory leak testing

    exit(exit_code);
    return exit_code;
}

int format_kern_result(kern_return_t kern_result)
{
    int check_length = snprintf(calc_buffer, 1,
        " (result code 0x%x)", kern_result);

    if (check_length + 1 > kern_result_buffer_length) {
        kern_result_buffer = (char *)realloc(kern_result_buffer,
            sizeof(char) * (check_length + 1));
        if (!kern_result_buffer) {
            return 0;
        }
    }
    sprintf(kern_result_buffer, " (result code 0x%x)", kern_result);
    return 1;
}

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
        qerror("malloc failure\n");
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


static void error_log(const char * format, ...)
{
    va_list ap;
    char fake_buffer[2];
    int output_length;
    char * output_string;

    if (verbose_level < kKXKextManagerLogLevelErrorsOnly) return;

    va_start(ap, format);
    output_length = vsnprintf(fake_buffer, 1, format, ap);
    va_end(ap);

    output_string = (char *)malloc(output_length + 1);
    if (!output_string) {
        qerror("malloc failure\n");
        return;
    }

    va_start(ap, format);
    vsprintf(output_string, format, ap);
    va_end(ap);

    va_start(ap, format);
    qerror("%s: %s\n", progname, output_string);
    va_end(ap);

    free(output_string);

    return;
}

static void qerror(const char * format, ...)
{
    va_list ap;

    if (verbose_level < kKXKextManagerLogLevelErrorsOnly) return;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    return;
}

int addKextsToManager(
    KXKextManagerRef aManager,
    CFArrayRef kextNames,
    CFMutableArrayRef kextArray)
{
    int result = 1;
    KXKextManagerError kxresult = kKXKextManagerErrorNone;
    CFIndex i, count;
    KXKextRef theKext = NULL;  // don't release
    CFURLRef kextURL = NULL;   // must release

   /*****
    * Add each kext named to the manager.
    */
    count = CFArrayGetCount(kextNames);
    for (i = 0; i < count; i++) {
        CFStringRef kextName = (CFStringRef)CFArrayGetValueAtIndex(
            kextNames, i);
        kextURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
            kextName, kCFURLPOSIXPathStyle, true);
        if (!kextURL) {
            qerror("can't create kext URL; no memory?\n");
            result = 0;
            goto finish;
        }

        kxresult = KXKextManagerAddKextWithURL(aManager, kextURL, true, &theKext);
        if (kxresult != kKXKextManagerErrorNone) {
            // FIXME: Improve this error message.
            qerror("can't add kext (%s).\n",
                KXKextManagerErrorStaticCStringForError(kxresult));
            result = 0;
            goto finish;
        }
        if (kextArray && theKext) {
            CFArrayAppendValue(kextArray, theKext);
        }
        CFRelease(kextURL);
        kextURL = NULL;
    }

finish:
    if (kextURL) CFRelease(kextURL);
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
static void usage(int level)
{
    qerror("usage: %s [-b bundle_id] ... [-c class_name] ... [-h]\n"
        "    [-p] [-v [0-6]] [kext] ...\n\n",
        progname);
    if (level > 1) {
        qerror("  -b bundle_id: unload the kernel extension whose\n");
        qerror("      CFBundleIdentifier is bundle_id\n");
        qerror("  -c class_name: terminate instances of class_name but\n");
        qerror("      do not unload code or personalities\n");
        qerror("  -h: help; print this message\n");
        qerror("  -p: don't remove personalities when unloading\n");
        qerror("      (unnecessary with -c)\n");
        qerror(
            "  -q: quiet mode: print no informational or error messages\n");
        qerror("  -v: verbose mode; print info about activity\n"
            "     (more info printed as optional value increases)\n");
        qerror("  kext: unload the named kernel extension bundle(s)\n");
    }
    return;
}
