//
//  IOHIDPreferences.h
//  IOKitUser
//
//  Created by AB on 10/25/19.
//

#ifndef IOHIDPreferences_h
#define IOHIDPreferences_h

#include <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN
CF_IMPLICIT_BRIDGING_ENABLED

typedef enum {
    IOHIDPreferencesOptionNone,
    IOHIDPreferencesOptionSingletonConnection
} IOHIDPreferencesOption;

/*! These APIs are XPC wrapper around corresponding CFPreferences API. https://developer.apple.com/documentation/corefoundation/preferences_utilities
 * XPC HID Preferences APIs are currently available for macOS only .
 */
CF_EXPORT
void IOHIDPreferencesSet(CFStringRef key, CFTypeRef __nullable value, CFStringRef user, CFStringRef host, CFStringRef domain);

CF_EXPORT
void IOHIDPreferencesSetMultiple(CFDictionaryRef __nullable keysToSet , CFArrayRef __nullable keysToRemove, CFStringRef user, CFStringRef host, CFStringRef domain);

CF_EXPORT
CFTypeRef __nullable IOHIDPreferencesCopy(CFStringRef key, CFStringRef user, CFStringRef host, CFStringRef domain);

CF_EXPORT
CFDictionaryRef __nullable IOHIDPreferencesCopyMultiple(CFArrayRef __nullable keys, CFStringRef user, CFStringRef host, CFStringRef domain);

CF_EXPORT
void IOHIDPreferencesSynchronize(CFStringRef user, CFStringRef host, CFStringRef domain);

CF_EXPORT
CFTypeRef __nullable IOHIDPreferencesCopyDomain(CFStringRef key, CFStringRef domain);

CF_EXPORT
void IOHIDPreferencesSetDomain(CFStringRef key,  CFTypeRef __nullable value, CFStringRef domain);

#pragma mark -
#pragma mark -

CF_EXPORT
CFTypeRef __nullable IOHIDPreferencesCreateInstance(IOHIDPreferencesOption option);

CF_EXPORT
void IOHIDPreferencesSetForInstance(CFTypeRef hidPreference, CFStringRef key, CFTypeRef __nullable value, CFStringRef user, CFStringRef host, CFStringRef domain);

CF_EXPORT
void IOHIDPreferencesSetMultipleForInstance(CFTypeRef hidPreference, CFDictionaryRef __nullable keysToSet , CFArrayRef __nullable keysToRemove, CFStringRef user, CFStringRef host, CFStringRef domain);

CF_EXPORT
CFTypeRef __nullable IOHIDPreferencesCopyForInstance(CFTypeRef hidPreference, CFStringRef key, CFStringRef user, CFStringRef host, CFStringRef domain);

CF_EXPORT
CFDictionaryRef __nullable IOHIDPreferencesCopyMultipleForInstance(CFTypeRef hidPreference, CFArrayRef __nullable keys, CFStringRef user, CFStringRef host, CFStringRef domain);

CF_EXPORT
void IOHIDPreferencesSynchronizeForInstance(CFTypeRef hidPreference, CFStringRef user, CFStringRef host, CFStringRef domain);

CF_EXPORT
CFTypeRef __nullable IOHIDPreferencesCopyDomainForInstance(CFTypeRef hidPreference, CFStringRef key, CFStringRef domain);

CF_EXPORT
void IOHIDPreferencesSetDomainForInstance(CFTypeRef hidPreference, CFStringRef key,  CFTypeRef __nullable value, CFStringRef domain);

CF_IMPLICIT_BRIDGING_DISABLED
CF_ASSUME_NONNULL_END

__END_DECLS

#endif /* IOHIDPreferences_h */
