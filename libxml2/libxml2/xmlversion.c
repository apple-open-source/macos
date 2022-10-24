/*
 * xmlversion.c: libxml2 utility functions for versioning
 *
 * For license and disclaimer see the license and disclaimer of
 * libxml2.
 *
 * ddkilzer@apple.com
 */

#include "xmlversionInternal.h"

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#include <mach-o/dyld_priv.h>

#if defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 160000 \
    || defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 130000 \
    || defined(__TV_OS_VERSION_MIN_REQUIRED) && __TV_OS_VERSION_MIN_REQUIRED >= 160000 \
    || defined(__WATCH_OS_VERSION_MIN_REQUIRED) && __WATCH_OS_VERSION_MIN_REQUIRED >= 90000
#define LIBXML_LINKED_ON_OR_AFTER_MACOS13_IOS16_WATCHOS9_TVOS16
#else
#undef LIBXML_LINKED_ON_OR_AFTER_MACOS13_IOS16_WATCHOS9_TVOS16
#endif

#endif

bool linkedOnOrAfterFall2022OSVersions(void)
{
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    return true;
#elif defined(LIBXML_LINKED_ON_OR_AFTER_MACOS13_IOS16_WATCHOS9_TVOS16)
    static bool result;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        result = dyld_program_minos_at_least(dyld_fall_2022_os_versions);
    });
    return result;
#else
    return false;
#endif
}
