/*
 *  PMDynamicStore.c
 *  PowerManagement
 *
 *  Created by Ethan Bold on 1/13/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>
#include <CoreFoundation/CoreFoundation.h>
#include "PMStore.h"
#include "PrivateLib.h"


/* TBD
typedef (void *)PMStoreKeysChangedCallBack(void *param, CFArrayRef keys);
__private_extern__ void PMStoreRequestCallBack(void *param, (PMStoreKeysChangedCallBack *)callback, CFArrayRef keys);
*/

static CFMutableDictionaryRef   gPMStore = NULL;
SCDynamicStoreRef               gSCDynamicStore = NULL;

static void PMDynamicStoreDisconnectCallBack(SCDynamicStoreRef store, void *info __unused);

/* dynamicStoreNotifyCallBack
 * defined in pmconfigd.c
 */
__private_extern__ void dynamicStoreNotifyCallBack(
    SCDynamicStoreRef   store,
    CFArrayRef          changedKeys,
    void                *info);


void PMStoreLoad()
{
    gPMStore = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    gSCDynamicStore = SCDynamicStoreCreate(0, CFSTR("powerd"), dynamicStoreNotifyCallBack, NULL);

    if (gSCDynamicStore) {
        SCDynamicStoreSetDispatchQueue(gSCDynamicStore, _getPMMainQueue());
    }

    SCDynamicStoreSetDisconnectCallBack(gSCDynamicStore, PMDynamicStoreDisconnectCallBack);
}

bool PMStoreSetValue(CFStringRef key, CFTypeRef value)
{
    CFTypeRef lastValue = NULL;

    if (!key || !value || !gPMStore)
        return false;

    if (!isA_CFString(key)) {
        return false;
    }

    lastValue = CFDictionaryGetValue(gPMStore, key);

    if (lastValue && CFEqual(lastValue, value)) {
        return true;
    }

    CFDictionarySetValue(gPMStore, key, value);
    return SCDynamicStoreSetValue(gSCDynamicStore, key, value);
}

bool PMStoreRemoveValue(CFStringRef key)
{
    if (!key || !isA_CFString(key)) {
        return false;
    }

    CFDictionaryRemoveValue(gPMStore, key);
    return SCDynamicStoreRemoveValue(gSCDynamicStore, key);
}

static void PMDynamicStoreDisconnectCallBack(
    SCDynamicStoreRef           store,
    void                        *info __unused)
{
    assert (store == gSCDynamicStore);

    SCDynamicStoreSetMultiple(gSCDynamicStore, gPMStore, NULL, NULL);
}
