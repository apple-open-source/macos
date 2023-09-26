//
//  archive_check_entitlement.c
//
//
//  Created by Justin Vreeland on 6/13/23.
//  Copyright © 2023 Apple Inc. All rights reserved.
//

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFDictionary.h>
#include <os/lock.h>
#include <Security/SecTask.h>

#ifdef DEBUG
#include <stdio.h>
#endif

#include "archive_check_entitlement.h"

#define LIBARCHIVE_FORMAT_ENTITLEMENT "com.apple.libarchive.formats"
#define LIBARCHIVE_FILTER_ENTITLEMENT "com.apple.libarchive.filters"

static os_unfair_lock  formats_lock = OS_UNFAIR_LOCK_INIT;
static os_unfair_lock  filters_lock = OS_UNFAIR_LOCK_INIT;
static CFMutableSetRef allowed_formats = NULL;
static CFMutableSetRef allowed_filters = NULL;
static bool formats_populated = false;
static bool filters_populated = false;

#define SAFE_RELEASE(x) { if (x) { CFRelease(x); x = NULL; } }

#define DEBUG 1

#ifdef DEBUG
static char *
get_c_string(CFStringRef cfstring)
{
	CFIndex size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstring), kCFStringEncodingUTF8);
	if (size == kCFNotFound) {
		return NULL;
	}

	char *buffer = (char *)calloc(size, sizeof(char));

	if (!buffer) {
		return NULL;
	}

	if (!CFStringGetCString(cfstring, buffer, size, kCFStringEncodingUTF8)) {
		free(buffer);
		return NULL;
	}

	return buffer;
}
#endif

void
archive_entitlement_cleanup(void)
{
	os_unfair_lock_lock(&formats_lock);
	SAFE_RELEASE(allowed_formats);
	formats_populated = false;
	os_unfair_lock_unlock(&formats_lock);

	os_unfair_lock_lock(&filters_lock);
	SAFE_RELEASE(allowed_filters);
	filters_populated = false;
	os_unfair_lock_unlock(&filters_lock);
}

static CFMutableSetRef
populate_entitlement_table(const char *entitlement_cstring)
{
	CFMutableSetRef working_set = NULL;

	CFStringRef entitlement = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, entitlement_cstring, kCFStringEncodingASCII, kCFAllocatorNull);
	SecTaskRef secTask = SecTaskCreateFromSelf(kCFAllocatorDefault);
	CFErrorRef error = NULL;

	CFArrayRef entitlements = (CFArrayRef)SecTaskCopyValueForEntitlement(secTask, entitlement, &error);
	SAFE_RELEASE(entitlement);
	SAFE_RELEASE(secTask);

	if (!entitlements) {
		if (!error) {
			return NULL;
		} else {
#ifdef DEBUG
			// Other entitlement related-error
			CFStringRef errorDescription = CFErrorCopyDescription(error);
			char *error_description = get_c_string(errorDescription);
			fprintf(stderr, "Error discovering entitlements, error: %s", error_description);
			SAFE_RELEASE(errorDescription);
			free(error_description);
#endif
			return NULL;
		}
	} else if (CFArrayGetTypeID() != CFGetTypeID(entitlements)) {
#ifdef DEBUG
		// The entitlements were of the wrong type
		CFStringRef typeID = CFCopyTypeIDDescription(CFGetTypeID(entitlements));
		char *type = get_c_string(typeID);
		SAFE_RELEASE(typeID);
		fprintf(stderr, "Error in type of entitlement expected: CFTypeArray got %s", type);
		SAFE_RELEASE(entitlements);
		free(type);
#endif
		return NULL;
	}

	working_set = CFSetCreateMutable(kCFAllocatorDefault, 15, &kCFTypeSetCallBacks);
	if (!working_set) {
		SAFE_RELEASE(entitlements);
		return NULL;
	}

	CFIndex ix, count = CFArrayGetCount(entitlements);
	for (ix = 0; ix < count; ix++) {
		CFStringRef format = (CFStringRef)CFArrayGetValueAtIndex(entitlements, ix);
		if (CFGetTypeID(format) != CFStringGetTypeID()) {
#ifdef DEBUG
			fprintf(stderr, "Unexpected non-string types in entitlement");
#endif
			continue;
		} else {
			CFSetAddValue(working_set, format);
		}
	}

	SAFE_RELEASE(entitlements);
	return working_set;
}

static bool
archive_allow_entitlement(const char *type, const char *entitlement, os_unfair_lock *lock, CFMutableSetRef *set, bool *populated)
{
	bool value = false;

	os_unfair_lock_lock(lock);
	CFMutableSetRef currentSet = set ? *set : NULL;
	bool isPopulated = populated ? *populated : false;

	if (!isPopulated) {
		currentSet = populate_entitlement_table(entitlement);
		isPopulated = true;
	}

	if (!currentSet) {
		value = true;
		goto cleanup;
	}

	CFStringRef string = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, type, kCFStringEncodingUTF8, kCFAllocatorNull);
	value = CFSetContainsValue(currentSet, string);
	SAFE_RELEASE(string);

cleanup:
	*set = currentSet;
	*populated = isPopulated;
	os_unfair_lock_unlock(lock);
	return value;
}

bool
archive_allow_entitlement_format(const char *format)
{
	return archive_allow_entitlement(format, LIBARCHIVE_FORMAT_ENTITLEMENT, &formats_lock, &allowed_formats, &formats_populated);
}

bool
archive_allow_entitlement_filter(const char *filter)
{
	return archive_allow_entitlement(filter, LIBARCHIVE_FILTER_ENTITLEMENT, &filters_lock, &allowed_filters, &filters_populated);
}
