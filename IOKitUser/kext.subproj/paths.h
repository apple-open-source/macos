#ifndef __PATHS_H__
#define __PATHS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <CoreFoundation/CoreFoundation.h>

CFURLRef PATH_CopyCanonicalizedURL(CFURLRef anURL);
CFURLRef PATH_CopyCanonicalizedURLAndSetDirectory(
    CFURLRef anURL, Boolean isDirectory);

char *   PATH_CanonicalizedCStringForURL(CFURLRef anURL);

char * PATH_canonicalizeCStringPath(const char * path);

#ifdef __cplusplus
}
#endif

#endif __PATHS_H__
