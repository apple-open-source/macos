#ifndef __SECNULLTRANSFORM__
#define __SECNULLTRANSFORM__


#ifdef COM_APPLE_SECURITY_ACTUALLY_BUILDING_LIBRARY
#include "SecTransform.h"
#else
#include <Security/SecTransform.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern const CFStringRef kSecNullTransformName;

typedef CFTypeRef SecNullTransformRef
API_DEPRECATED("SecTransform is no longer supported", macos(10.7, 13.0)) API_UNAVAILABLE(ios, tvos, watchos, macCatalyst);

SecNullTransformRef SecNullTransformCreate()
API_DEPRECATED("SecTransform is no longer supported", macos(10.7, 13.0)) API_UNAVAILABLE(ios, tvos, watchos, macCatalyst);

#ifdef __cplusplus
};
#endif

#endif
