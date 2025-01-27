#ifndef __SECTRANSFORM_INTERNAL__
#define __SECTRANSFORM_INTERNAL__

#include <Security/SecTransform.h>

#ifdef __cplusplus
extern "C" {
#endif

CFErrorRef SecTransformConnectTransformsInternal(SecGroupTransformRef groupRef, SecTransformRef sourceTransformRef, CFStringRef sourceAttributeName,
														 SecTransformRef destinationTransformRef, CFStringRef destinationAttributeName)
API_DEPRECATED("SecTransform is no longer supported", macos(10.7, 13.0)) API_UNAVAILABLE(ios, tvos, watchos, macCatalyst);

// note:  if destinationTransformRef is orphaned (i.e. left with nothing connecting to it and connecting to nothing, it will be removed
// from the group.
CFErrorRef SecTransformDisconnectTransforms(SecTransformRef destinationTransformRef, CFStringRef destinationAttributeName,
														 SecTransformRef sourceTransformRef, CFStringRef sourceAttributeName)
API_DEPRECATED("SecTransform is no longer supported", macos(10.7, 13.0)) API_UNAVAILABLE(ios, tvos, watchos, macCatalyst);

SecTransformRef SecGroupTransformFindLastTransform(SecGroupTransformRef groupTransform)
API_DEPRECATED("SecTransform is no longer supported", macos(10.7, 13.0)) API_UNAVAILABLE(ios, tvos, watchos, macCatalyst);

SecTransformRef SecGroupTransformFindMonitor(SecGroupTransformRef groupTransform)
API_DEPRECATED("SecTransform is no longer supported", macos(10.7, 13.0)) API_UNAVAILABLE(ios, tvos, watchos, macCatalyst);
bool SecGroupTransformHasMember(SecGroupTransformRef groupTransform, SecTransformRef transform)
API_DEPRECATED("SecTransform is no longer supported", macos(10.7, 13.0)) API_UNAVAILABLE(ios, tvos, watchos, macCatalyst);

CF_EXPORT
CFStringRef SecTransformDotForDebugging(SecTransformRef transformRef)
API_DEPRECATED("SecTransform is no longer supported", macos(10.7, 13.0)) API_UNAVAILABLE(ios, tvos, watchos, macCatalyst);
    
#ifdef __cplusplus
};
#endif

#endif
