#ifndef __SECTRANSFORM_INTERNAL__
#define __SECTRANSFORM_INTERNAL__


#ifdef __cplusplus
extern "C" {
#endif

#include "SecTransform.h"


CFErrorRef SecTransformConnectTransformsInternal(SecGroupTransformRef groupRef, SecTransformRef sourceTransformRef, CFStringRef sourceAttributeName,
														 SecTransformRef destinationTransformRef, CFStringRef destinationAttributeName);

// note:  if destinationTransformRef is orphaned (i.e. left with nothing connecting to it and connecting to nothing, it will be removed
// from the group.
CFErrorRef SecTransformDisconnectTransforms(SecTransformRef destinationTransformRef, CFStringRef destinationAttributeName,
														 SecTransformRef sourceTransformRef, CFStringRef sourceAttributeName);

SecTransformRef SecGroupTransformFindLastTransform(SecGroupTransformRef groupTransform);
SecTransformRef SecGroupTransformFindMonitor(SecGroupTransformRef groupTransform);
bool SecGroupTransformHasMember(SecGroupTransformRef groupTransform, SecTransformRef transform);

CF_EXPORT
CFStringRef SecTransformDotForDebugging(SecTransformRef transformRef);


    
#ifdef __cplusplus
};
#endif

#endif
