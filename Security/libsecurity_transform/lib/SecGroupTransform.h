#ifndef __SECGROUPTRANSFORM__
#define __SECGROUPTRANSFORM__


#ifdef COM_APPLE_SECURITY_SANE_INCLUDES
#include "SecTransform.h"
#else
#include <Security/SecTransform.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern const CFStringRef kSecGroupTransformName;

// Group transforms are created by the SecTransformConnectTransforms function.

#ifdef __cplusplus
};
#endif

#endif
