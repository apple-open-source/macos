#ifndef _KCLIST_MAIN_H
#define _KCLIST_MAIN_H

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/kext/OSKext.h>

#include <getopt.h>
#include <sysexits.h>

#include <IOKit/kext/OSKext.h>

#include "kext_tools_util.h"
#include "kernelcache.h"

#pragma mark Basic Types & Constants
/*******************************************************************************
* Constants
*******************************************************************************/

enum {
    kKclistExitOK          = EX_OK,

    // don't think we use it
    kKclistExitUnspecified = 11,

    // don't actually exit with this, it's just a sentinel value
    kKclistExitHelp        = 33,
};

#pragma mark Command-line Option Definitions
/*******************************************************************************
* Command-line options. This data is used by getopt_long_only().
*
* Options common to all kext tools are in kext_tools_util.h.
*******************************************************************************/

#define kOptArch   'a'

#define kOptChars  "a:h"

int longopt = 0;

struct option sOptInfo[] = {
    { kOptNameHelp,                  no_argument,        NULL,     kOptHelp },
    { kOptNameArch,                  required_argument,  NULL,     kOptArch },

    { NULL, 0, NULL, 0 }  // sentinel to terminate list
};

typedef struct {
    char             * kernelcachePath;
    CFMutableSetRef    kextIDs;
    const NXArchInfo * archInfo;
} KclistArgs;

#pragma mark Function Prototypes
/*******************************************************************************
* Function Prototypes
*******************************************************************************/
ExitStatus readArgs(
    int            * argc,
    char * const  ** argv,
    KclistArgs  * toolArgs);
ExitStatus addBundleIdentifier(
    KclistArgs * toolArgs,
    char * ident);
ExitStatus checkArgs(KclistArgs * toolArgs);
void listPrelinkedKexts(KclistArgs * toolArgs, CFPropertyListRef kcInfoPlist);
void printKextInfo(CFDictionaryRef kextPlist);

void usage(UsageLevel usageLevel);

#endif /* _KCLIST_MAIN_H */
