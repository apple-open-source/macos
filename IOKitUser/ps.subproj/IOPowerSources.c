/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * HISTORY
 *
 */



#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include "IOPSKeys.h"
#include "IOPowerSources.h"

#ifndef kIOPSDynamicStorePathKey
#define kIOPSDynamicStorePathKey kIOPSDynamicStorePath
#endif

/***
 Returns a blob of Power Source information in an opaque CFTypeRef. Clients should
 not actually look directly at data in the CFTypeRef - they should use the accessor
 functions IOPSCopyPowerSourcesList and IOPSGetPowerSourceDescription, instead.
 Returns NULL if errors were encountered.
 Return: Caller must CFRelease() the return value when done.
***/
CFTypeRef IOPSCopyPowerSourcesInfo(void) {
    SCDynamicStoreRef   store = NULL;
    CFStringRef         ps_match = NULL;
    CFMutableArrayRef   ps_arr = NULL;
    CFDictionaryRef     power_sources = NULL;
    
    // Open connection to SCDynamicStore
    store = SCDynamicStoreCreate(kCFAllocatorDefault, 
                CFSTR("IOKit Power Source Copy"), NULL, NULL);
    if(!store) return NULL;
     
    // Create regular expression to match all Power Sources
    ps_match = SCDynamicStoreKeyCreate(kCFAllocatorDefault, CFSTR("%@%@/%@"),
                kSCDynamicStoreDomainState, CFSTR(kIOPSDynamicStorePathKey), kSCCompAnyRegex);
    if(!ps_match) return NULL;
    ps_arr = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if(!ps_arr) return NULL;
    CFArrayAppendValue(ps_arr, ps_match);
    CFRelease(ps_match);
    
    // Copy multiple Power Sources into dictionary
    power_sources = SCDynamicStoreCopyMultiple(store, NULL, ps_arr);
    
    // Release SCDynamicStore
    CFRelease(ps_arr);
    CFRelease(store);
    
    // Return CFDictionary as opaque CFTypeRef
    return (CFTypeRef)power_sources;
}

/***
 Arguments - Takes the CFTypeRef returned by IOPSCopyPowerSourcesInfo()
 Returns a CFArray of Power Source handles, each of type CFTypeRef.
 The caller shouldn't look directly at the CFTypeRefs, but should use
 IOPSGetPowerSourceDescription on each member of the CFArray.
 Returns NULL if errors were encountered.
 Return: Caller must CFRelease() the returned CFArray.
***/
CFArrayRef IOPSCopyPowerSourcesList(CFTypeRef blob) {
    int                 count;
    void                **keys;
    void                **values;
    CFArrayRef          arr;

    // Check that the argument is actually a CFDictionary
    if( !(blob && (CFGetTypeID(blob)==CFDictionaryGetTypeID())) ) 
        return NULL;

    // allocate buffers for keys and values
    count = CFDictionaryGetCount((CFDictionaryRef)blob);    
    keys = (void **)malloc(count * sizeof(void *));
    values = (void **)malloc(count * sizeof(void *));
    if(!(keys && values))
        return NULL;

    // Get keys and values from CFDictionary
    CFDictionaryGetKeysAndValues((CFDictionaryRef)blob, keys, values);
    
    // Create CFArray from keys
    arr = CFArrayCreate(kCFAllocatorDefault, keys, count, &kCFTypeArrayCallBacks);
    
    // free keys and values
    free(keys);
    free(values);
    
    // Return CFArray
    return arr;
}

/***
 Arguments - Takes one of the CFTypeRefs in the CFArray returned by
 IOPSCopyPowerSourcesList
 Returns a CFDictionary with specific information about the power source.
 See IOKit.framework/Headers/ups/IOUPSKeys.h for specific fields.
 Return: Caller should not CFRelease the returned CFArray
***/
CFDictionaryRef IOPSGetPowerSourceDescription(CFTypeRef blob, CFTypeRef ps) {
    // Check that the info is a CFDictionary
    if( !(blob && (CFGetTypeID(blob)==CFDictionaryGetTypeID())) ) 
        return NULL;
    
    // Check that the Power Source is a CFString
    if( !(ps && (CFGetTypeID(ps)==CFStringGetTypeID())) ) 
        return NULL;
    
    // Extract the CFDictionary of Battery Info
    // and return
    return CFDictionaryGetValue(blob, ps);
}

/***
 Support structures and functions for IOPSNotificationCreateRunLoopSource
***/
typedef struct {
    IOPowerSourceCallbackType	callback;
    void			*context;
} user_callback_context;

typedef struct {
    SCDynamicStoreRef		store;
    CFRunLoopSourceRef		SCDSrls;
} my_cfrls_context;

/* SCDynamicStoreCallback */
void my_dynamic_store_call(SCDynamicStoreRef store, CFArrayRef keys, void *ctxt) {
    user_callback_context	*c = (user_callback_context *)ctxt;
    IOPowerSourceCallbackType cb;

    // Check that the callback is a valid pointer
    if(!c) return;
    cb = c->callback;
    if(!cb) return;
    
    // Execute callback
    (*cb)(c->context);
}

static void
rlsSchedule(void *info, CFRunLoopRef rl, CFStringRef mode)
{
	my_cfrls_context	*c = (my_cfrls_context *)info;
    CFRunLoopAddSource(CFRunLoopGetCurrent(), c->SCDSrls, mode);
	return;
}


static void
rlsCancel(void *info, CFRunLoopRef rl, CFStringRef mode)
{
	my_cfrls_context	*c = (my_cfrls_context *)info;
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), c->SCDSrls, mode);
	return;
}

static void
rlsRelease(void *info, CFRunLoopRef rl, CFStringRef mode)
{
	my_cfrls_context	*c = (my_cfrls_context *)info;
    printf("rlsRelease called\n"); fflush(stdout);
	return;
}

static void
rlsRetain(void *info, CFRunLoopRef rl, CFStringRef mode)
{
	my_cfrls_context	*c = (my_cfrls_context *)info;
    printf("rlsRelease called\n"); fflush(stdout);
	return;
}

/***
 Returns a CFRunLoopSourceRef that notifies the caller when power source
 information changes.
 Arguments:
    IOPowerSourceCallbackType callback - A function to be called whenever any power source is added, removed, or changes
    void *context - Any user-defined pointer, passed to the IOPowerSource callback.
 Returns NULL if there were any problems.
 Caller must CFRelease() the returned value.
***/
CFRunLoopSourceRef IOPSNotificationCreateRunLoopSource(IOPowerSourceCallbackType callback, void *context) {
    SCDynamicStoreRef   store = NULL;
    CFStringRef         ps_match = NULL;
    CFMutableArrayRef   ps_arr = NULL;
    CFRunLoopSourceRef  SCDrls = NULL;
    // For the source we're creating:
    CFRunLoopSourceRef	ourSource = NULL;
    CFRunLoopSourceContext *rlsContext = NULL;
    SCDynamicStoreContext	*scdsctxt = NULL;

    user_callback_context		*callback_state = NULL;
    my_cfrls_context			*runloop_state = NULL;
    
    // Save the state of the user's callback
    callback_state = malloc(sizeof(user_callback_context));
    callback_state->context = context;
    callback_state->callback = callback;
    
    scdsctxt = (SCDynamicStoreContext *)malloc(sizeof(SCDynamicStoreContext));
    bzero(scdsctxt, sizeof(SCDynamicStoreContext));
    scdsctxt->info = callback_state;
    
    // Open connection to SCDynamicStore. User's callback as context.
    store = SCDynamicStoreCreate(kCFAllocatorDefault, 
                CFSTR("IOKit Power Source Copy"), my_dynamic_store_call, (void *)scdsctxt);
    if(!store) return NULL;
     
    // Create regular expression to match all Power Sources
    ps_match = SCDynamicStoreKeyCreate(kCFAllocatorDefault, CFSTR("%@%@/%@"), kSCDynamicStoreDomainState, CFSTR(kIOPSDynamicStorePath), kSCCompAnyRegex);
    if(!ps_match) return NULL;
    ps_arr = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if(!ps_arr) return NULL;
    CFArrayAppendValue(ps_arr, ps_match);
    CFRelease(ps_match);   
    
    // Set up regular expression notifications
    SCDynamicStoreSetNotificationKeys(store, NULL, ps_arr);
    CFRelease(ps_arr);

    // Obtain the CFRunLoopSourceRef from this SCDynamicStoreRef session
    SCDrls = SCDynamicStoreCreateRunLoopSource(kCFAllocatorDefault, store, 0);
    runloop_state = malloc(sizeof(my_cfrls_context));
    runloop_state->store = store;
    runloop_state->SCDSrls = SCDrls;
    
    // Setup the CFRunLoopSource context for the return-value CFRLS
    // Install my hooks into schedule/cancel/perform here
    rlsContext = (CFRunLoopSourceContext *)malloc(sizeof(CFRunLoopSourceContext));
    bzero(rlsContext, sizeof(CFRunLoopSourceContext));
    rlsContext->version         = 0;
    rlsContext->info            = (void *)runloop_state;
    rlsContext->schedule        = rlsSchedule;
    rlsContext->cancel          = rlsCancel;

    // Create the RunLoopSource
    ourSource = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, rlsContext);    
    
    return ourSource;
}
