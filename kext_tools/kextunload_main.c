#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <libc.h>

#include <IOKit/kext/KXKextManager.h>

static char * progname = "(unknown)";
static int verbose_level = 0;           // nonzero for -v option
static mach_port_t gMasterPort;


/*******************************************************************************
*******************************************************************************/

// FIXME: Several of these belong in a separate module
static void verbose_log(const char * format, ...);
static void error_log(const char * format, ...);
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
        fprintf(stderr, "can't get I/O Kit master port\n");
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
        fprintf(stderr, "can't allocate memory\n");
        goto finish;
    }

    kextClassNames = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        NULL /* C pointers */);
    if (!kextClassNames) {
        exit_code = 1;
        fprintf(stderr, "can't allocate memory\n");
        goto finish;
    }

    kextBundleIDs = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        NULL /* C pointers */);
    if (!kextBundleIDs) {
        exit_code = 1;
        fprintf(stderr, "can't allocate memory\n");
        goto finish;
    }

    kexts = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!kexts) {
        exit_code = 1;
        fprintf(stderr, "can't allocate memory\n");
        goto finish;
    }


    /*****
    * Process command line arguments.
    */
    while ((optchar = getopt(argc, (char * const *)argv, "b:c:hm:pv")) != -1) {

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
          case 'v':
            {
                const char * next;

                if (optind >= argc) {
                    verbose_level = 1;
                } else {
                    next = argv[optind];
                    if ((next[0] == '1' || next[0] == '2' || next[0] == '3' ||
                         next[0] == '4' || next[0] == '5' || next[0] == '6') &&
                         next[1] == '\0') {

                        verbose_level = atoi(next);
                        optind++;
                    } else {
                        verbose_level = 1;
                    }
                }
            }
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
            fprintf(stderr, "Can't create kext name string for \"%s\"; "
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
        fprintf(stderr, "no kernel extension specified\n\n");
        usage(1);
        exit_code = 1;
        goto finish;
    }


   /*****
    * Set up the kext manager.
    */
    myKextManager = KXKextManagerCreate(kCFAllocatorDefault);
    if (!myKextManager) {
        fprintf(stderr, "can't allocate kext manager\n");
        exit_code = 1;
        goto finish;
    }

    result = KXKextManagerInit(myKextManager, true, false);
    if (result != kKXKextManagerErrorNone) {
        fprintf(stderr, "can't initialize manager (%s)\n",
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
             printf("permission denied; you must be root to unload kexts\n");
             exit_code = 1;
             goto finish;
        }
        printf("terminate instances for class %s %s",
            kext_class_name, kern_result == KERN_SUCCESS ?
            "succeeded" : "failed");
        if (kern_result != KERN_SUCCESS) {
             printf(" (result code 0x%x)\n", kern_result);
        }
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
             printf("permission denied; you must be root to unload kexts\n");
             exit_code = 1;
             goto finish;
        }
        printf("unload id %s %s",
            kext_id, kern_result == KERN_SUCCESS ? "succeeded" : "failed");
        if (kern_result != KERN_SUCCESS) {
             printf(" (result code 0x%x)\n", kern_result);
        } else {
            printf(" (any personalities%s unloaded)\n",
                unload_personalities ? " also" : " not");
        }
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
             printf("permission denied; you must be root to unload kexts\n");
             exit_code = 1;
             goto finish;
        }
        printf("unload kext %s %s",
            kext_name, kern_result == KERN_SUCCESS ? "succeeded" : "failed");
        if (kern_result != KERN_SUCCESS) {
             printf(" (result code 0x%x)\n", kern_result);
        } else {
            printf(" (any personalities%s unloaded)\n",
                unload_personalities ? " also" : " not");
        }
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
        fprintf(stderr, "malloc failure\n");
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

    va_start(ap, format);
    output_length = vsnprintf(fake_buffer, 1, format, ap);
    va_end(ap);

    output_string = (char *)malloc(output_length + 1);
    if (!output_string) {
        fprintf(stderr, "malloc failure\n");
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
            fprintf(stderr, "can't create kext URL; no memory?\n");
            result = 0;
            goto finish;
        }

        kxresult = KXKextManagerAddKextWithURL(aManager, kextURL, true, &theKext);
        if (kxresult != kKXKextManagerErrorNone) {
            // FIXME: Improve this error message.
            fprintf(stderr, "can't add kext (%s).\n",
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
    fprintf(stderr,
        "usage: %s [-b bundle_id] ... [-c class_name] ... [-h]\n"
        "    [-p] [-v [1-6]] [kext] ...\n\n",
        progname);
    if (level > 1) {
        fprintf(stderr, "  -b bundle_id: unload the kernel extension whose\n");
        fprintf(stderr, "      CFBundleIdentifier is bundle_id\n");
        fprintf(stderr, "  -c class_name: terminate instances of class_name but\n");
        fprintf(stderr, "      do not unload code or personalities\n");
        fprintf(stderr, "  -h: help; print this message\n");
        fprintf(stderr, "  -p: don't remove personalities when unloading\n");
        fprintf(stderr, "      (unnecessary with -c)\n");
        fprintf(stderr, "  -v: verbose mode; print info about activity\n"
            "     (more info printed as optional value increases "
            " from 1 to 6)\n");
        fprintf(stderr, "  kext: unload the named kernel extension bundle(s)\n");
    }
    return;
}
