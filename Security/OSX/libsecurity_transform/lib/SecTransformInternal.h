#ifndef __SECTRANSFORM_INTERNAL__
#define __SECTRANSFORM_INTERNAL__

#include <Security/SecTransform.h>

#ifdef __cplusplus
extern "C" {
#endif

CFErrorRef SecTransformConnectTransformsInternal(SecGroupTransformRef groupRef, SecTransformRef sourceTransformRef, CFStringRef sourceAttributeName,
														 SecTransformRef destinationTransformRef, CFStringRef destinationAttributeName)
API_DEPRECATED("SecTransform is no longer supported", macos(10.7, 13.0), ios(NA, NA), tvos(NA, NA), watchos(NA, NA), macCatalyst(NA, NA));

// note:  if destinationTransformRef is orphaned (i.e. left with nothing connecting to it and connecting to nothing, it will be removed
// from the group.
CFErrorRef SecTransformDisconnectTransforms(SecTransformRef destinationTransformRef, CFStringRef destinationAttributeName,
														 SecTransformRef sourceTransformRef, CFStringRef sourceAttributeName)
API_DEPRECATED("SecTransform is no longer supported", macos(10.7, 13.0), ios(NA, NA), tvos(NA, NA), watchos(NA, NA), macCatalyst(NA, NA));

SecTransformRef SecGroupTransformFindLastTransform(SecGroupTransformRef groupTransform)
API_DEPRECATED("SecTransform is no longer supported", macos(10.7, 13.0), ios(NA, NA), tvos(NA, NA), watchos(NA, NA), macCatalyst(NA, NA));

SecTransformRef SecGroupTransformFindMonitor(SecGroupTransformRef groupTransform)
API_DEPRECATED("SecTransform is no longer supported", macos(10.7, 13.0), ios(NA, NA), tvos(NA, NA), watchos(NA, NA), macCatalyst(NA, NA));
bool SecGroupTransformHasMember(SecGroupTransformRef groupTransform, SecTransformRef transform)
API_DEPRECATED("SecTransform is no longer supported", macos(10.7, 13.0), ios(NA, NA), tvos(NA, NA), watchos(NA, NA), macCatalyst(NA, NA));

CF_EXPORT
CFStringRef SecTransformDotForDebugging(SecTransformRef transformRef)
API_DEPRECATED("SecTransform is no longer supported", macos(10.7, 13.0), ios(NA, NA), tvos(NA, NA), watchos(NA, NA), macCatalyst(NA, NA));
    
#ifdef __cplusplus
};
#endif

#endif
