/*
 * MISEntitlements.h
 *   Constants and interfaces for dealing with entitlements
 */

#ifndef MISENTITLEMENT_H
#define MISENTITLEMENT_H

#include <MISBase.h>

#include <CoreFoundation/CoreFoundation.h>


/* Return an array of all known entitlements */
MIS_EXPORT CFArrayRef MISEntitlementCopyAllEntitlements(void);

/*
 * Check if the entitlement dictionary permits an entitlement to have the
 * given value
 */
Boolean MISEntitlementDictionaryAllowsEntitlementValue(CFDictionaryRef entitlements, CFStringRef entitlement, CFTypeRef value);

#endif
