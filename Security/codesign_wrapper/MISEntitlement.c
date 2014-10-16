#include "MISEntitlement.h"

static const CFStringRef kEntitlementAllValuesAllowed = CFSTR("*");

static Boolean whitelistArrayAllowsEntitlementValue(CFArrayRef whitelist, CFStringRef value)
{
    Boolean allowed = false;

    CFIndex i, count = CFArrayGetCount(whitelist);
    for (i = 0; (i < count) && (allowed == false); i++) {
        CFStringRef item = (CFStringRef) CFArrayGetValueAtIndex(whitelist, i);
        if (CFGetTypeID(item) == CFStringGetTypeID()) {

            CFIndex len = CFStringGetLength(item);
            if (len > 0) {
                if (CFStringGetCharacterAtIndex(item, len-1) != '*') {

                    /* Not a wildcard, must be an exact match */
                    allowed = CFStringCompare(item, value, 0) == kCFCompareEqualTo;
                } else {

                    /* Last character is a wildcard - do some matching */
                    CFStringRef wildcardPrefix = CFStringCreateWithSubstring(kCFAllocatorDefault, item, CFRangeMake(0, len-1));
                    allowed = CFStringHasPrefix(value, wildcardPrefix);
                    CFRelease(wildcardPrefix);
                }
            }
        } else {

            /* Unexpected item in whitelist - bail */
            break;
        }
    }

    return allowed;
}

Boolean MISEntitlementDictionaryAllowsEntitlementValue(CFDictionaryRef entitlements, CFStringRef entitlement, CFTypeRef value)
{
    Boolean allowsEntitlement = false;

    /* NULL is never a valid entitlement value */
    if (value != NULL) {

        /* Make sure the entitlement is present */
        CFTypeRef storedValue = CFDictionaryGetValue(entitlements, entitlement);
        if (storedValue != NULL) {

            /*
             * Handling depends on the type
             * If the value matches our constant, the entitlement is permitted
             * to have any value.
             * If the value in the dictionary is a boolean, then the entitlement
             * value must be a boolean with the same value
             * If the value in the dictionary is an array (of strings), then the
             * entitlement must be either one of those strings OR an array that
             * includes only present strings
             */
            if (CFEqual(storedValue, kEntitlementAllValuesAllowed) == true) {

                /* XXX: Does this need to restrict the value to some types */
                allowsEntitlement = true;
            } else if (CFGetTypeID(storedValue) == CFBooleanGetTypeID()) {
                allowsEntitlement = CFEqual(storedValue, value);
            } else if (CFGetTypeID(storedValue) == CFStringGetTypeID()) {
                if (CFGetTypeID(value) == CFStringGetTypeID()) {
                    CFArrayRef array = CFArrayCreate(kCFAllocatorDefault, (const void **) &storedValue, 1, &kCFTypeArrayCallBacks);
                    allowsEntitlement = whitelistArrayAllowsEntitlementValue(array, (CFStringRef) value);
                    CFRelease(array);
                }
            } else if (CFGetTypeID(storedValue) == CFArrayGetTypeID()) {

                /* value is either a single string or array of strings */
                if (CFGetTypeID(value) == CFStringGetTypeID()) {
                    allowsEntitlement = whitelistArrayAllowsEntitlementValue((CFArrayRef) storedValue, (CFStringRef) value);
                } else if (CFGetTypeID(value) == CFArrayGetTypeID()) {

                    /*
                     * Assume allowed, will set back to false if we encounter
                     * elements that are not permitted
                     */
                    allowsEntitlement = true;

                    /* Make sure each element is a string and in the array */
                    CFIndex i, count = CFArrayGetCount((CFArrayRef) value);
                    for (i = 0; (i < count) && (allowsEntitlement == true); i++) {
                        CFTypeRef element = CFArrayGetValueAtIndex((CFArrayRef) value, i);
                        if (CFGetTypeID(element) == CFStringGetTypeID()) {
                            allowsEntitlement = whitelistArrayAllowsEntitlementValue((CFArrayRef) storedValue, (CFStringRef) element);
                        } else {
                            allowsEntitlement = false;
                        }
                    }
                }
            }
        }
    }

    return allowsEntitlement;
}
