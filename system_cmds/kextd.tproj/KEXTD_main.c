#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include "KEXTD.h"

#define TIMER_PERIOD_S		10
#define DEFAULT_SEARCH_PATH	"/System/Library/Extensions/"

static const char * arg0 = NULL;

static void usage(void)
{
    printf("usage: %s [-v] [-d] [-x] [-b bootlevel]\n", arg0);
    exit(1);
}

int main (int argc, const char *argv[])
{
    KEXTDRef kextd;
    KEXTReturn error;
    KEXTBootlevel bootlevel;
    CFURLRef url;
    CFArrayRef array;
    CFIndex period;
    Boolean safeBoot;
    Boolean beVerbose;
    Boolean enableTimer;
    Boolean debug;
    const void * vals[1];
    int c;

    arg0 = argv[0];
    period = TIMER_PERIOD_S;
    debug = false;
    safeBoot = false;
    beVerbose = false;
    enableTimer = false;
    bootlevel = kKEXTBootlevelNormal;

    while ( (c = getopt(argc, (char **)argv, "xvdb:")) != -1 ) {
        switch ( c ) {

            case 'x':
                safeBoot = true;
                break;
            case 'd':
                debug = true;
                break;
#if TIMERSOURCE
            case 'p':
                if ( !optarg ) {
                    usage();
                }
                else {
                    period = strtoul(optarg, NULL, 0);
                    if ( period > 0 )
                        enableTimer = true;
                }
                break;
#endif

            case 'b':
                if ( !optarg ) {
                    usage();
                }
                bootlevel = strtoul(optarg, NULL, 0);
                if ( bootlevel > 0x1f ) {
                    usage();
                }
                break;

            default:
                usage();
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 0) {
        usage();
    }



    url = CFURLCreateWithFileSystemPath(NULL, CFSTR(DEFAULT_SEARCH_PATH), kCFURLPOSIXPathStyle, true);
    if ( !url ) {
        printf("Error opening: %s.\n", DEFAULT_SEARCH_PATH);
        exit(1);
    }
    vals[0] = url;
    array = CFArrayCreate(NULL, vals, 1, &kCFTypeArrayCallBacks);
    CFRelease(url);
    kextd = KEXTDCreate(array, &error);
    CFRelease(array);
    if ( !kextd ) {
        KEXTError(error, CFSTR("Error creating kextd"));
        exit(error);
    }
    
    KEXTDRegisterHelperCallbacks(kextd, NULL);
#if TIMERSOURCE
    error = KEXTDStartMain(kextd, beVerbose, safeBoot, debug, enableTimer, period, bootlevel);
#else
    error = KEXTDStartMain(kextd, beVerbose, safeBoot, debug, bootlevel);
#endif
    if ( error != kKEXTReturnSuccess ) {
        KEXTDFree(kextd);
        KEXTError(error, CFSTR("Error starting kextd"));
        exit(error);
    }
    KEXTDFree(kextd);
    
    return error;
}
