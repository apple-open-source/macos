//
// Copyright (c) 2023 Apple Inc. All rights reserved.
//

#ifndef DARWIN_DIRECTORY_ENABLED_H
#define DARWIN_DIRECTORY_ENABLED_H

#include <TargetConditionals.h>
#include <os/feature_private.h>
#include <os/variant_private.h>

OS_ALWAYS_INLINE
static inline bool
_darwin_directory_enabled(void)
{
#ifdef DARWIN_DIRECTORY_AVAILABLE
#if TARGET_OS_OSX
    return os_feature_enabled_simple(DarwinDirectory, LibinfoLookups_macOS, false) &&
           os_variant_is_darwinos("com.apple.DarwinDirectory");
#else
    return os_feature_enabled_simple(DarwinDirectory, LibinfoLookups, false);
#endif // if TARGET_OS_OSX
#endif // ifdef DARWIN_DIRECTORY_AVAILABLE
    return false;
}

#endif // DARWIN_DIRECTORY_ENABLED_H
