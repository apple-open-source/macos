
#include <stdio.h>
#include <unistd.h>
#include <sysexits.h>
#include <sys/wait.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <IOKit/IOCFSerialize.h>
#include <mach/mach_types.h>

#include <IOKit/kext/KEXTManager.h>

static Boolean sSafeBoot = false;
static Boolean sVerbose = false;
static Boolean sInteractive = false;
static Boolean sAuthenticateAll = false;

static KEXTManagerRef manager = NULL;
static const char * sCmdName;

#define kBundleIDKey        "CFBundleIdentifier"
#define kPersonalityNameKey "IOPersonalityName"

#if 0
// no longer (or never was!) used
#define kModuleKey         "Module"
#define kModulesKey        "Modules"
#define kModuleFileKey     "File"
#define kPersonalityKey	   "Personality"
#define kPersonalitiesKey  "Personalities"
#define kNameKey           "Name"
#define kVendorKey         "Vendor"
#define kVersionKey        "Version"
#define kRequiresKey       "Requires"
#define kModuleAliasesKey  "Aliases"
#endif 0

#define kDefaultSearchPath "/System/Library/Extensions"

// these don't seem to be used
#if 0
#define kInfoMacOS         "Info-macos"
#define kInfoMacOSType     "xml"
#endif 0

static void usage(Boolean help)
{
    fprintf(stderr, "Usage: %s [-eihvx] [[-L dir] ...] [-p personality] kextpath\n", sCmdName);
    if ( help ) {
        fprintf(stderr, "\t-e   Don't scan System extensions.\n");
        fprintf(stderr, "\t-h   Help (this menu).\n");
        fprintf(stderr, "\t-i   Interactive mode.\n");
        fprintf(stderr, "\t-L   Search Library dir.\n");
        fprintf(stderr, "\t-p   Personality to load.\n");
        fprintf(stderr, "\t-v   Verbose mode.\n");
        fprintf(stderr, "\t-x   Run in safe boot mode.\n");
    }
    exit(EX_USAGE);
}

static void printError(const char * string)
{
    fprintf(stderr, string);
    return;
}

static void printMessage(const char * string)
{
    if (sVerbose) {
        fprintf(stdout, string);
    }
    return;
}

static Boolean Prompt(CFStringRef message, Boolean defaultValue)
{
    Boolean ret;
    CFIndex len;
    char * buf;
    char * dp;
    char c;
    char dv;

    ret = false;
    len = CFStringGetLength(message) + 1;
    buf = (char *)malloc(sizeof(char) * len);
    if ( !CFStringGetCString(message, buf, len, kCFStringEncodingASCII) ) {
        free(buf);
        return false;
    }
    
    dv = defaultValue?'y':'n';
    dp = defaultValue?" [Y/n]":" [y/N]";
    
    while ( 1 ) {
        printf(buf);
        printf(dp);
        printf("? ");
        fflush(stdout);
        fscanf(stdin, "%c", &c);
        if ( c != 10 ) while ( fgetc(stdin) != 10 );
        if ( (c == 10) || (tolower(c) == dv) ) {
            ret = defaultValue;
            break;
        }
        else if ( tolower(c) == 'y' ) {
            ret = true;
            break;
        }
        else if ( tolower(c) == 'n' ) {
            ret = false;
            break;
        }
    }
    free(buf);

    return ret;
}

static CFURLRef
URLCreateAbsoluteWithPath(CFStringRef path)
{
    CFURLRef url, base;

    url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                        path, kCFURLPOSIXPathStyle, true);
    base = CFURLCopyAbsoluteURL(url); CFRelease(url);
    path = CFURLGetString(base);
    
    url = CFURLCreateWithString(kCFAllocatorDefault, path, NULL);
    CFRelease(base);

    return url;
}

// Override the default authentication scheme so we can load
// kext not owned by root into the kernel.
static KEXTReturn authenticate(CFURLRef url, void * context)
{
    if ( !sAuthenticateAll ) {
        KEXTReturn ret;
        
        ret = KEXTManagerAuthenticateURL(url);
        if ( ret != kKEXTReturnSuccess ) {
            if ( sVerbose ) {
                CFURLRef absUrl;
                CFStringRef message;
                CFStringRef path;

                absUrl = CFURLCopyAbsoluteURL(url);
                path = CFURLGetString(absUrl);

                message = CFStringCreateWithFormat(
                                            kCFAllocatorDefault,
                                            NULL,
                                            CFSTR("Error (%d) Authentication failed: %@"),
                                            ret,
                                            path);

                CFShow(message);
                CFRelease(message);
                CFRelease(absUrl);
            }
            return ret;
        }
    }

    return kKEXTReturnSuccess;
}

// Print out a message when a module is about to load.
static Boolean mWillLoad(KEXTManagerRef manager, KEXTModuleRef module, void * context)
{
    CFStringRef name;
    CFStringRef message;

    if ( !sVerbose ) {
        return true;
    }
    
    name = (CFStringRef)KEXTModuleGetProperty(module, CFSTR(kBundleIDKey));
    message = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Loading module: %@."), name);
    CFShow(message);
    CFRelease(message);
    
    return true;
}

// Print out a message when the module was successfully loaded.
static void mWasLoaded(KEXTManagerRef manager, KEXTModuleRef module, void * context)
{
    CFStringRef name;
    CFStringRef message;

    if ( !sVerbose ) {
        return;
    }

    name = (CFStringRef)KEXTModuleGetProperty(module, CFSTR(kBundleIDKey));
    message = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Loaded module: %@."), name);
    CFShow(message);
    CFRelease(message);
}

// Print an error message if there was an error loading the module.
static KEXTReturn mLoadError(KEXTManagerRef manager, KEXTModuleRef module, KEXTReturn error, void * context)
{
    CFStringRef name;
    CFStringRef message;

    name = (CFStringRef)KEXTModuleGetProperty(module, CFSTR(kBundleIDKey));
    switch ( error ) {
        case kKEXTReturnModuleAlreadyLoaded:

           /* If the module was already loaded, that isn't an error
            * as far as kextload is concerned.
            */
            error = kKEXTReturnSuccess;

            if ( sVerbose ) {
                    message = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                        CFSTR("Module '%@' is already loaded; continuing."), name);
                    CFShow(message);
                    CFRelease(message);
            }
            break;

        default:
            if ( sVerbose ) {
                message = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                    CFSTR("Error loading module '%@'"), name);
                KEXTError(error, message);
                CFRelease(message);
            }
            break;
    }

    return error;
}

// Initialize the KEXTManager.
static Boolean InitManager(CFArrayRef libList)
{
    CFStringRef scanpath;
    int i, count;
    CFURLRef url;
    KEXTReturn error;
    KEXTManagerBundleLoadingCallbacks bCallback = {
        0, authenticate, NULL, NULL, NULL, NULL, NULL,
    };
    KEXTManagerModuleLoadingCallbacks mCallbacks = {
        0, mWillLoad, mWasLoaded, mLoadError, NULL, NULL,
    };

    // Create the manager database.
    manager = KEXTManagerCreate(&bCallback, &mCallbacks, NULL, NULL, NULL, &printError, &printMessage, sSafeBoot, &error);
    if ( !manager ) {
        fprintf(stderr, "Manager not created.\n");
        return false;
    }

    for (i = 0, count = CFArrayGetCount(libList); i < count; i++) {
        scanpath = CFArrayGetValueAtIndex(libList, i);
        url = URLCreateAbsoluteWithPath(scanpath);
        // Now scan in all the bundles in the extensions directory.
        error = KEXTManagerScanPath(manager, url);
        CFRelease(url);
        if (error != kKEXTReturnSuccess) {
            if (sVerbose) {
                CFStringRef errMsg = CFStringCreateWithFormat(
                                        kCFAllocatorDefault,
                                        NULL,
                                        CFSTR("Error scanning path - %@"),
                                        scanpath);
                KEXTError(error, errMsg);
            }
        };
    }

    return true;
}

static void PromptForLoading(void * val, void * context)
{
    KEXTPersonalityRef person;
    CFMutableArrayRef toLoad;
    Boolean boolval;

    person = val;
    toLoad = context;

    boolval = true;
    if ( sInteractive ) {
        CFStringRef name;

        name = KEXTPersonalityGetProperty(person, CFSTR(kBundleIDKey));
        if ( name ) {
            CFStringRef message;
            
            message = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Load personality '%@'"), name);
            boolval = Prompt(message, true);
            CFRelease(message);
        }
    }

    if ( boolval ) {
        CFArrayAppendValue(toLoad, person);
    }
}

static void ArrayGetModuleList(void * val, void * context[])
{
    KEXTPersonalityRef person;
    KEXTModuleRef module;
    KEXTReturn error;
    CFMutableArrayRef modules;
    CFStringRef name;
    CFStringRef modName;
    CFStringRef message;

    person = val;
    modules = context[0];
    error = *(KEXTReturn *)context[1];

    if ( error != kKEXTReturnSuccess ) {
        return;
    }

    // Once we have the personality entity, we can
    // attempt to load it into the kernel...
    modName = KEXTPersonalityGetProperty(person, CFSTR(kBundleIDKey));
    if ( !modName ) {
        name = KEXTPersonalityGetProperty(person, CFSTR(kBundleIDKey));
        message = CFStringCreateWithFormat(kCFAllocatorDefault,
                                            NULL,
                                            CFSTR("Error: '%@' has no Module key."),
                                            name);
        CFShow(message);
        CFRelease(message);

        if ( sInteractive ) {
            if ( !Prompt(CFSTR("Continue"), true) ) {
                *(KEXTReturn *)(context[1]) = error;
                return;
            }
        }
        return;
    }

    module = KEXTManagerGetModule(manager, modName);
    if ( module ) {
        CFRange range;

        range = CFRangeMake(0, CFArrayGetCount(modules));
        if ( !CFArrayContainsValue(modules, range, module) ) {
            CFArrayAppendValue(modules, module);
        }
    }
}

static void ArrayLoadMods(void * val, void * context)
{
    KEXTModuleRef mod;
    KEXTReturn error;
    Boolean boolval;

    error = *(KEXTReturn *)context;
    if ( error != kKEXTReturnSuccess ) {
        return;
    }

    mod = val;
    
    boolval = true;
    if ( sInteractive ) {
        CFStringRef name;

        name = KEXTModuleGetProperty(mod, CFSTR(kBundleIDKey));
        if ( name ) {
            CFStringRef message;

            message = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Load module '%@'"), name);
            boolval = Prompt(message, true);
            CFRelease(message);
        }
    }

    if ( boolval ) {
        error = KEXTManagerLoadModule(manager, mod);
        if ( error != kKEXTReturnSuccess ) {
            KEXTError(error, CFSTR("Error loading module"));
            *(KEXTReturn *)context = error;
        }
    }
}

static KEXTReturn LoadAllModules(KEXTBundleRef bundle)
{
    CFArrayRef modules;
    CFRange range;
    KEXTReturn error;

    if ( !bundle ) {
        return kKEXTReturnBadArgument;
    }
    
    modules = KEXTManagerCopyModulesForBundle(manager, bundle);
    if ( !modules ) {
        return kKEXTReturnModuleNotFound;
    }

    error = kKEXTReturnSuccess;
    range = CFRangeMake(0, CFArrayGetCount(modules));
    CFArrayApplyFunction(modules, range, (CFArrayApplierFunction)ArrayLoadMods, &error);

    CFRelease(modules);

    return error;
}

static KEXTReturn LoadAllPersonalities(KEXTBundleRef bundle)
{
    CFArrayRef array;
    CFArrayRef configs;
    CFMutableArrayRef toLoad;
    CFMutableArrayRef modules;
    CFRange range;
    KEXTReturn error;
    void * context[2];

    error = kKEXTReturnSuccess;

    // Get the configurations associated with this bundle.
    configs = KEXTManagerCopyConfigsForBundle(manager, bundle);

    // Get the personality entities associated with
    // this particular bundle.  We use these keys to aquire\
    // personality entities from the database.
    array = KEXTManagerCopyPersonalitiesForBundle(manager, bundle);

    if ( !array && !configs ) {
        return kKEXTReturnPersonalityNotFound;
    }

    // This is the list of personalities and configurations to load.
    toLoad = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    modules = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    if ( configs ) {
        range = CFRangeMake(0, CFArrayGetCount(configs));
        CFArrayApplyFunction(configs, range, (CFArrayApplierFunction)PromptForLoading, toLoad);
        CFRelease(configs);
    }

    if ( array ) {
        range = CFRangeMake(0, CFArrayGetCount(array));
        CFArrayApplyFunction(array, range, (CFArrayApplierFunction)PromptForLoading, toLoad);
        CFRelease(array);
    }

    context[0] = modules;
    context[1] = &error;

    range = CFRangeMake(0, CFArrayGetCount(toLoad));
    CFArrayApplyFunction(toLoad, range, (CFArrayApplierFunction)ArrayGetModuleList, context);

    if ( error != kKEXTReturnSuccess ) {
        CFRelease(toLoad);
        CFRelease(modules);
        return error;
    }

    // Load all the modules.
    sInteractive = false;
    range = CFRangeMake(0, CFArrayGetCount(modules));
    CFArrayApplyFunction(modules, range, (CFArrayApplierFunction)ArrayLoadMods, &error);
    sInteractive = true;

    if ( error != kKEXTReturnSuccess ) {
        CFRelease(toLoad);
        CFRelease(modules);
        return error;
    }
    
    // We need to send all personalities together.
    error = KEXTManagerLoadPersonalities(manager, toLoad);
    CFRelease(toLoad);
    CFRelease(modules);

    return error;
}

int main (int argc, const char *argv[])
{
    int c;

    CFMutableArrayRef libList;
    CFURLRef abs;
    CFStringRef defaultLibDir = NULL;
    CFStringRef path;
    CFStringRef name;
    KEXTBundleRef bundle;
    KEXTReturn error;

    sCmdName = argv[0];

    name  = NULL;
    defaultLibDir = CFSTR(kDefaultSearchPath);
    libList = CFArrayCreateMutable
                (kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if (!libList) {
        fprintf(stderr, "%s: Not enough memory to run\n", sCmdName);
        exit(EX_SOFTWARE);
    }

    while ( (c = getopt(argc, (char **)argv, "eihvxp:L:")) != -1 ) {
        switch ( c ) {
        case 'e':
            defaultLibDir = NULL;
            break;

        case 'v':
            sVerbose = true;
            break;

        case 'x':
            sSafeBoot = true;
            break;

        case 'i':
            sInteractive = true;
            break;

        case 'h':
            usage(true);

        case 'L':
            if ( !optarg )
                usage(false);
            else if (strlen(optarg)) {
                path = CFStringCreateWithCString
                    (kCFAllocatorDefault, optarg, kCFStringEncodingASCII);
                CFArrayAppendValue(libList, path);
                CFRelease(path);
            }

        case 'p':
            if ( !optarg )
                usage(false);
            else
                name = CFStringCreateWithCString(kCFAllocatorDefault, optarg, kCFStringEncodingASCII);
            break;

        default:
            usage(false);
        }
    }
    argc -= optind;
    argv += optind;

    if (argc < 1)
        usage(false);

    if (defaultLibDir)
        CFArrayInsertValueAtIndex(libList, 0, defaultLibDir);

    path = CFStringCreateWithCString
            (kCFAllocatorDefault, argv[argc - 1], kCFStringEncodingASCII);
    abs = URLCreateAbsoluteWithPath(path);
    CFRelease(path);

    if ( !abs ) {
        fprintf(stderr, "Invalid path: %s.\n", argv[argc - 1]);
        CFRelease(abs);
        exit(-1);
    }

    if ( sVerbose ) {
        fprintf(stderr, "Examining: %s\n", argv[argc - 1]);
    }

    sAuthenticateAll = false;
    if ( !InitManager(libList) ) {
        fprintf(stderr, "Error initializing KEXT Manager.\n");
        exit(-1);
    }

    // Don't authenticate the target bundle, this
    // is just a convenience for developers.
    sAuthenticateAll = true;
    
    // Add the bundle to the database.
    error = KEXTManagerAddBundle(manager, abs);
    if ( error != kKEXTReturnSuccess ) {
        fprintf(stderr, "Error (%d) adding bundle to database.\n", error);
        exit(1);
    }
    // Re-enable the authentication.
    sAuthenticateAll = false;
    // Now, get the bundle entity from the database,
    // this is the handle we use for accessing bundle resources.
    bundle = KEXTManagerGetBundleWithURL(manager, abs);
    if ( !bundle ) {
        fprintf(stderr, "Bundle not found in database.\n");
        exit(EX_DATAERR);
    }

    // If no name was given, then assume all personalities should be loaded.
    error = LoadAllPersonalities(bundle);
    if ( error != kKEXTReturnSuccess ) {
        // No personalities are found, then this is probably
        // a kmod bundle.  Try loading just the modules.
        if ( error == kKEXTReturnPersonalityNotFound ) {
            // XXX -- Attempt to load modules.
            LoadAllModules(bundle);
        }
    }

    if ( manager ) {
        KEXTManagerRelease(manager);
    }

    if ( sVerbose ) {
        printf("Done.\n");
    }

    return 0;      // ...and make main fit the ANSI spec.
}
