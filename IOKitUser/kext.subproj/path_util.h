#if !__LP64__

#ifndef __PATH_UTIL_H__
#define __PATH_UTIL_H__

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <CoreFoundation/CoreFoundation.h>

CFURLRef PATH_CopyCanonicalizedURL(CFURLRef anURL);
CFURLRef PATH_CopyCanonicalizedURLAndSetDirectory(
    CFURLRef anURL, Boolean isDirectory);

char * PATH_CanonicalizedCStringForURL(CFURLRef anURL);

char * PATH_canonicalizeCStringPath(const char * path);

__END_DECLS

#endif __PATH_UTIL_H__
#endif // !__LP64__
