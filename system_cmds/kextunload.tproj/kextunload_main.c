
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <IOKit/IOCFSerialize.h>
#include <mach/mach_types.h>

#include <IOKit/kext/KEXTManager.h>

static Boolean sVerbose = false;
static Boolean sAuthenticateAll = false;

static KEXTManagerRef manager = NULL;
static const char * sArgv0;
static mach_port_t sMasterPort;

#define kBundleIDKey       "CFBundleIdentifier"


#if 0
// never were used
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

// These don't seem to be used
#if 0
#define kInfoMacOS         "Info-macos"
#define kInfoMacOSType     "xml"
#endif 0

#define defaultPath CFSTR(kDefaultSearchPath)


static void usage(Boolean help)
{
    printf("Usage: %s [-h] [-m modulename] [-c classname] [kextpath]\n", sArgv0);
    exit(-1);
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
                                            CFSTR("Error (%) Authentication failed: %@"),
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

// Initialize the KEXTManager.
static Boolean InitManager(void)
{
    CFURLRef url;
    KEXTReturn error;
    KEXTManagerBundleLoadingCallbacks bCallback = {
        0, authenticate, NULL, NULL, NULL, NULL, NULL,
    };
    KEXTManagerModuleLoadingCallbacks mCallbacks = {
        0, NULL, NULL, NULL, NULL, NULL,
    };

    // Give the manager the default path to the Extensions folder.
    // This is needed for dependency matching.
    url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, defaultPath, kCFURLPOSIXPathStyle, true);

    // Create the manager database.
    manager = KEXTManagerCreate(&bCallback, &mCallbacks, NULL, NULL, NULL, NULL, NULL, false, &error);
    if ( !manager ) {
        printf("Manager not created.\n");
        return false;
    }

    // Now scan in all the bundles in the extensions directory.
    if ( (error = KEXTManagerScanPath(manager, url)) != kKEXTReturnSuccess ) {
        if ( sVerbose ) {
            KEXTError(error, CFSTR("Error scanning path"));
        }
        return false;
    };

    return true;
}


static KEXTReturn KEXTUnloadAllModules(KEXTBundleRef bundle)
{
    CFArrayRef modules;
    kern_return_t kr;
    KEXTModuleRef mod;
    char buf[256];
    unsigned int i;
    CFIndex count;

    if ( !bundle ) {
        return kKEXTReturnBadArgument;
    }
    
    modules = KEXTManagerCopyModulesForBundle(manager, bundle);
    if ( !modules ) {
        return kKEXTReturnModuleNotFound;
    }
    
    for( i = 0, count = CFArrayGetCount(modules); i < count; i++) {
        CFStringRef name;

        mod = (KEXTModuleRef) CFArrayGetValueAtIndex( modules, i);
        name = KEXTModuleGetProperty( mod, CFSTR(kBundleIDKey));

        if( CFStringGetCString( name, buf, sizeof(buf), kCFStringEncodingMacRoman)) {
   
            kr = IOCatalogueTerminate( sMasterPort, kIOCatalogModuleUnload, buf );
            printf("IOCatalogueTerminate(Module %s) [%x]\n", buf, kr);
        }
    }

    CFRelease(modules);

    return kKEXTReturnSuccess;
}

KEXTReturn KEXTUnload( const char * cPath )
{
    CFURLRef url;
    CFURLRef abs;
    CFStringRef path;
    KEXTBundleRef bundle;
    KEXTReturn error;

    path = CFStringCreateWithCString(kCFAllocatorDefault, cPath, kCFStringEncodingASCII);
    url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path, kCFURLPOSIXPathStyle, true);
    CFRelease(path);
    
    abs = CFURLCopyAbsoluteURL(url);
    CFRelease(url);

    if ( !abs ) {
        printf("Invalid path: %s.\n", cPath);
        CFRelease(abs);
        exit(-1);
    }

    sAuthenticateAll = false;
    if ( !InitManager() ) {
        printf("Error initializing KEXT Manager.\n");
        exit(-1);
    }

    // Don't authenticate the target bundle, this
    // is just a convenience for developers.
    sAuthenticateAll = true;
    
    // Add the bundle to the database.
    error = KEXTManagerAddBundle(manager, abs);
    if ( error != kKEXTReturnSuccess ) {
        printf("Error (%d) adding bundle to database.\n", error);
        exit(1);
    }
    // Re-enable the authentication.
    sAuthenticateAll = false;
    // Now, get the bundle entity from the database,
    // this is the handle we use for accessing bundle resources.
    bundle = KEXTManagerGetBundleWithURL(manager, abs);
    if ( !bundle ) {
        printf("Bundle not found in database.\n");
        exit(1);
    }

    error = KEXTUnloadAllModules(bundle);

    if ( manager ) {
        KEXTManagerRelease(manager);
    }

    return( error );
}

int main (int argc, const char *argv[])
{
    int c;
    const char * name  = NULL;
    int command = 0;
    KEXTReturn error;
    kern_return_t kr;

    // Obtain the I/O Kit communication handle.
    kr = IOMasterPort(bootstrap_port, &sMasterPort);
    if( kr != KERN_SUCCESS)
        exit(-1);

    sArgv0 = argv[0];

    if (argc < 2)
        usage(false);

    while ( (c = getopt(argc, (char **)argv, "hm:c:")) != -1 ) {
        switch ( c ) {

            case 'h':
                usage(true);

            case 'c':
                command = kIOCatalogServiceTerminate;
                if ( !optarg )
                    usage(false);
                else
                    name = optarg;
                break;

            case 'm':
                command = kIOCatalogModuleUnload;
                if ( !optarg )
                    usage(false);
                else
                    name = optarg;
                break;

            default:
                usage(false);
        }
    }

    argc -= optind;
    argv += optind;
    if (argc >= 1) {
        name = argv[argc - 1];
        error = KEXTUnload( name );
    }

    if( command >= kIOCatalogModuleUnload) {
        kr = IOCatalogueTerminate( sMasterPort, command, (char *) name );
        printf("IOCatalogueTerminate(%s %s) [%x]\n",
            (command == kIOCatalogModuleUnload) ? "Module" : "Class",
            name, kr);
    }
    printf("done.\n");

    return 0;      // ...and make main fit the ANSI spec.
}
