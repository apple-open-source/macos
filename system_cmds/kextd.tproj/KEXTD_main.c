#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include "KEXTD.h"

#define TIMER_PERIOD_S		10
#define DEFAULT_SEARCH_PATH	"/System/Library/Extensions/"

static const char * arg0 = NULL;

static void usage(void)
{
    printf("usage: %s [-v] [-d] [-x] [-j] [-b bootlevel] [-f dirpath]\n", arg0);
    exit(1);
}

int main (int argc, const char *argv[])
{
    KEXTDRef kextd;
    KEXTReturn error;
    KEXTBootlevel bootlevel;
    CFStringRef str;
    CFURLRef url;
    CFMutableArrayRef array;
    CFIndex period;
    Boolean safeBoot;
    Boolean beVerbose;
    Boolean enableTimer;
    Boolean debug;
    Boolean cdMKextBoot;
    int c;

    arg0 = argv[0];
    period = TIMER_PERIOD_S;
    debug = false;
    safeBoot = false;
    beVerbose = false;
    enableTimer = false;
    cdMKextBoot = false;
    bootlevel = kKEXTBootlevelNormal;

    url = CFURLCreateWithFileSystemPath(NULL, CFSTR(DEFAULT_SEARCH_PATH), kCFURLPOSIXPathStyle, true);
    if ( !url ) {
        printf("Error opening: %s.\n", DEFAULT_SEARCH_PATH);
        exit(1);
    }
    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(array, url);
    CFRelease(url);

    while ( (c = getopt(argc, (char **)argv, "xvdjb:f:")) != -1 ) {
        switch ( c ) {

            case 'x':
                safeBoot = true;
                break;
            case 'v':
                beVerbose = true;
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

            case 'j':
                cdMKextBoot = true;
                break;
                
            case 'f':
                if ( !optarg ) {
                    usage();
                }
                str = CFStringCreateWithCString(NULL, optarg, kCFStringEncodingNonLossyASCII);
                if ( str )
                    url = CFURLCreateWithFileSystemPath(NULL, str, kCFURLPOSIXPathStyle, true);
                else
                    url = NULL;
                if ( !url ) {
                    printf("Error opening: %s.\n", optarg);
                } else {
                    CFArrayAppendValue(array, url);
                    CFRelease(url);
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

    kextd = KEXTDCreate(array, &error);
    CFRelease(array);
    if ( !kextd ) {
        KEXTError(error, CFSTR("Error creating kextd"));
        exit(error);
    }
    
    KEXTDRegisterHelperCallbacks(kextd, NULL);
#if TIMERSOURCE
    error = KEXTDStartMain(kextd, beVerbose, safeBoot, debug, enableTimer, period, bootlevel, cdMKextBoot);
#else
    error = KEXTDStartMain(kextd, beVerbose, safeBoot, debug, bootlevel, cdMKextBoot);
#endif
    if ( error != kKEXTReturnSuccess ) {
        KEXTDFree(kextd);
        KEXTError(error, CFSTR("Error starting kextd"));
        exit(error);
    }
    KEXTDFree(kextd);
    
    return error;
}
