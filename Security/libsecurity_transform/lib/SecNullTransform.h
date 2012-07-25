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

typedef CFTypeRef SecNullTransformRef;

SecNullTransformRef SecNullTransformCreate();

#ifdef __cplusplus
};
#endif

#endif
