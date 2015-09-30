//
//  utilities
//
//  Copyright © 2015 Apple Inc. All rights reserved.
//

#include "SecInternalReleasePriv.h"


#include <dispatch/dispatch.h>
#include <AssertMacros.h>
#include <strings.h>

#if TARGET_OS_EMBEDDED
#include <MobileGestalt.h>
#else
#include <sys/utsname.h>
#endif


bool
SecIsInternalRelease(void)
{
    static bool isInternal = false;

    return isInternal;
}

bool SecIsProductionFused(void) {
    static bool isProduction = true;
#if TARGET_OS_EMBEDDED
    static dispatch_once_t once = 0;

    dispatch_once(&once, ^{
        CFBooleanRef productionFused = MGCopyAnswer(kMGQSigningFuse, NULL);
        if (productionFused) {
            if (CFEqual(productionFused, kCFBooleanFalse)) {
                isProduction = false;
            }
            CFRelease(productionFused);
        }
    });
#endif
    return isProduction;
}

