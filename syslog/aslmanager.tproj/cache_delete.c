/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <CacheDelete/CacheDelete.h>
#include <CoreFoundation/CoreFoundation.h>
#include <asl.h>
#include "daemon.h"

/* CacheDelete ID (stored in cache delete plist). Must match suggested CacheDelete id naming conventions. */
#define CACHE_DELETE_ID "com.apple.activity_tracing.cache-delete"

#define CFSTR_FROM_DICT(dict, key) ({ \
    void *strRef = NULL; \
    if (dict != NULL) { \
        strRef = (void *)CFDictionaryGetValue(dict, key); \
        if ((strRef == NULL) || (CFStringGetTypeID() != CFGetTypeID(strRef))) strRef = NULL; \
    } \
    (CFStringRef)strRef; \
})

#define INT64_FROM_DICT(dict, key) ({ \
    int64_t value = 0; \
    if (dict != NULL) { \
        void *numRef = (void *)CFDictionaryGetValue(dict, key); \
        if (numRef && (CFNumberGetTypeID() == CFGetTypeID(numRef))) {\
            if (!CFNumberGetValue(numRef, kCFNumberSInt64Type, &value)) value = 0; \
        } \
    } \
    value; \
})

static int64_t
_purgeable(void)
{
	size_t psize = 0;
	int status = cache_delete_task(true, &psize);
	if (status == 0) return (uint64_t)psize;
	return 0;
}

static int64_t
_purge(int64_t purge_amount_bytes, CacheDeleteUrgency urgency)
{
	size_t curr_size, new_size;
	curr_size = 0;

	int status = cache_delete_task(true, &curr_size);
	if (status != 0) return 0;

	new_size = curr_size - purge_amount_bytes;

	status = cache_delete_task(false, &new_size);
	if (status == 0) return new_size;

	return 0;
}

static bool
_volume_contains_cached_data(CFStringRef volume)
{
    return true;
}


static CFDictionaryRef
_handle_cache_delete_with_urgency(CFDictionaryRef info, CacheDeleteUrgency urgency, bool purge)
{
	xpc_transaction_begin();

    uint64_t amount_requested = INT64_FROM_DICT(info, CFSTR(CACHE_DELETE_AMOUNT_KEY));
    CFStringRef volume_requested = CFSTR_FROM_DICT(info, CFSTR(CACHE_DELETE_VOLUME_KEY));

    CFMutableDictionaryRef result = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (result == NULL)
	{
        goto bail;
    }
	else if (volume_requested == NULL)
	{
        goto bail;
    }
    
    /* TODO: CFStringGetCStringPtr can return NULL */
//	debug_log(ASL_LEVEL_DEBUG, "CacheDelete request (purge=%d, urgency=%d, volume=%s, amount=%llu).", (int)urgency, CFStringGetCStringPtr(volume_requested, kCFStringEncodingUTF8), amount_requested);

    int64_t amount_purged = 0;
    
    if (_volume_contains_cached_data(volume_requested))
	{
        if (purge)
		{
            amount_purged = _purge(amount_requested, urgency);
//			debug_log(ASL_LEVEL_WARNING, "Purged %lld bytes.", amount_purged);
        }
		else
		{
            amount_purged = _purgeable();
//			debug_log(ASL_LEVEL_WARNING, "%lld bytes of purgeable space.", amount_purged);
        }
    }
    
    CFNumberRef amount_purged_obj = CFNumberCreate(NULL, kCFNumberSInt64Type, &amount_purged);
    if (amount_purged_obj != NULL)
	{
        CFDictionaryAddValue(result, CFSTR(CACHE_DELETE_AMOUNT_KEY), amount_purged_obj);
        CFRelease(amount_purged_obj);
    }
    
bail:

	xpc_transaction_end();
   return result;
}

bool
cache_delete_register(void)
{
    return CacheDeleteRegisterInfoCallbacks(CFSTR(CACHE_DELETE_ID), ^CFDictionaryRef(CacheDeleteUrgency urgency, CFDictionaryRef info) {
        /* Purgeable Space Request */
        return _handle_cache_delete_with_urgency(info, urgency, false);
    }, ^CFDictionaryRef(CacheDeleteUrgency urgency, CFDictionaryRef info) {
        /* Purge Request */
        return _handle_cache_delete_with_urgency(info, urgency, true);
    }, NULL, NULL) == 0;
}
