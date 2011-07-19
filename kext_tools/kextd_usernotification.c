/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <IOKit/kext/kextmanager_types.h>
#include <IOKit/kext/OSKextPrivate.h>

#include "kextd_usernotification.h"

// xxx - do we want a new log activity flag?

#ifdef NO_CFUserNotification

void kextd_raise_notification(
    CFStringRef alertHeader,
    CFArrayRef  alertMessageArray)
{
    return;
}

#else

SCDynamicStoreRef       sSysConfigDynamicStore            = NULL;
uid_t                   sConsoleUser                      = (uid_t)-1;
CFRunLoopSourceRef      sNotificationQueueRunLoopSource   = NULL;  // must release
CFUserNotificationRef   sCurrentNotification              = NULL;  // must release
CFRunLoopSourceRef      sCurrentNotificationRunLoopSource = NULL;  // must release
CFMutableArrayRef       sPendedNonsecureKextPaths         = NULL;  // must release
CFMutableDictionaryRef  sNotifiedNonsecureKextPaths       = NULL;  // must release

static void _sessionDidChange(
    SCDynamicStoreRef store,
    CFArrayRef        changedKeys,
    void            * info);

void _checkNotificationQueue(void * info);
void _notificationDismissed(
    CFUserNotificationRef userNotification,
    CFOptionFlags         responseFlags);

/*******************************************************************************
*******************************************************************************/
ExitStatus startMonitoringConsoleUser(
    KextdArgs    * toolArgs,
    unsigned int * sourcePriority)
{
    ExitStatus         result                 = EX_OSERR;
    CFStringRef        consoleUserName        = NULL;  // must release
    CFStringRef        consoleUserKey         = NULL;  // must release
    CFMutableArrayRef  keys                   = NULL;  // must release
    CFRunLoopSourceRef sysConfigRunLoopSource = NULL;  // must release
    CFRunLoopSourceContext sourceContext;

    sSysConfigDynamicStore = SCDynamicStoreCreate(
        kCFAllocatorDefault, CFSTR(KEXTD_SERVER_NAME),
        _sessionDidChange, /* context */ NULL);
    if (!sSysConfigDynamicStore) {
        OSKextLogMemError();
        goto finish;

    }

    consoleUserName = SCDynamicStoreCopyConsoleUser(sSysConfigDynamicStore,
        &sConsoleUser, NULL);
    if (!consoleUserName) {
        sConsoleUser = (uid_t)-1;
        OSKextLog(/* kext */ NULL,
            kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
            "No user logged in at kextd startup.");
    } else {
    OSKextLog(/* kext */ NULL,
        kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
        "User %d logged in at kextd startup.", sConsoleUser);
    }

    consoleUserKey = SCDynamicStoreKeyCreateConsoleUser(kCFAllocatorDefault);
    if (!consoleUserKey) {
        OSKextLogMemError();
        goto finish;
    }
    keys = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if (!keys) {
        OSKextLogMemError();
        goto finish;
    }

    CFArrayAppendValue(keys, consoleUserKey);
    SCDynamicStoreSetNotificationKeys(sSysConfigDynamicStore, keys,
        /* patterns */ NULL);

    sysConfigRunLoopSource = SCDynamicStoreCreateRunLoopSource(
        kCFAllocatorDefault, sSysConfigDynamicStore, 0);
    if (!sysConfigRunLoopSource) {
        OSKextLogMemError();
        goto finish;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), sysConfigRunLoopSource,
        kCFRunLoopCommonModes);

    bzero(&sourceContext, sizeof(CFRunLoopSourceContext));
    sourceContext.version = 0;
    sourceContext.perform = _checkNotificationQueue;
    sNotificationQueueRunLoopSource = CFRunLoopSourceCreate(kCFAllocatorDefault,
        (*sourcePriority)++, &sourceContext);
    if (!sNotificationQueueRunLoopSource) {
       OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
           "Failed to create alert run loop source.");
        goto finish;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), sNotificationQueueRunLoopSource,
        kCFRunLoopDefaultMode);

    if (!createCFMutableArray(&sPendedNonsecureKextPaths,
        &kCFTypeArrayCallBacks)) {
        OSKextLogMemError();
        goto finish;
    }

    if (!createCFMutableDictionary(&sNotifiedNonsecureKextPaths)) {
        OSKextLogMemError();
        goto finish;
    }
    
    result = EX_OK;

finish:
    SAFE_RELEASE(consoleUserName);
    SAFE_RELEASE(consoleUserKey);
    SAFE_RELEASE(keys);
    SAFE_RELEASE(sysConfigRunLoopSource);

    return result;
}

/*******************************************************************************
*******************************************************************************/
void stopMonitoringConsoleUser(void)
{
    SAFE_RELEASE(sSysConfigDynamicStore);

    SAFE_RELEASE(sNotificationQueueRunLoopSource);
    SAFE_RELEASE(sPendedNonsecureKextPaths);
    SAFE_RELEASE(sNotifiedNonsecureKextPaths);

    return;
}

/*******************************************************************************
*******************************************************************************/
static void _sessionDidChange(
    SCDynamicStoreRef store,
    CFArrayRef        changedKeys,
    void            * info)
{
    CFStringRef consoleUserName = NULL;  // must release
    uid_t       oldUser         = sConsoleUser;

   /* If any users are logged on via fast user switching, logging out to
    * loginwindow sets the console user to 0 (root). We can't do a reset
    * until all users have fully logged out, in which case
    * SCDynamicStoreCopyConsoleUser() returns NULL.
    */
    consoleUserName = SCDynamicStoreCopyConsoleUser(sSysConfigDynamicStore,
        &sConsoleUser, NULL);
    if (!consoleUserName) {
        if (oldUser != (uid_t)-1) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
                "User %d logged out.", oldUser);
        }
        sConsoleUser = (uid_t)-1;
        resetUserNotifications(/* dismissAlert */ true);
        goto finish;
    }

   /* Sometimes we'll get >1 notification on a user login, so make sure
    * the old & new uid are different.
    */
    if (sConsoleUser != (uid_t)-1 && oldUser != sConsoleUser) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
            "User %d logged in.", sConsoleUser);
        CFRunLoopSourceSignal(sNotificationQueueRunLoopSource);
        CFRunLoopWakeUp(CFRunLoopGetCurrent());
    }

finish:
    SAFE_RELEASE(consoleUserName);

    return;
}

/*******************************************************************************
*******************************************************************************/
void resetUserNotifications(Boolean dismissAlert)
{
    OSKextLog(/* kext */ NULL,
        kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
        "Resetting user notifications.");

    if (dismissAlert) {

       /* Release any reference to the current user notification.
        */
        if (sCurrentNotification) {
            CFUserNotificationCancel(sCurrentNotification);
            CFRelease(sCurrentNotification);
            sCurrentNotification = NULL;
        }

        if (sCurrentNotificationRunLoopSource) {
            CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
                sCurrentNotificationRunLoopSource,
                kCFRunLoopDefaultMode);
            CFRelease(sCurrentNotificationRunLoopSource);
            sCurrentNotificationRunLoopSource = NULL;
        }
    }

   /* Clear the record of which kexts the user has been told are insecure.
    * If extensions folders have been modified, who knows which kexts are changed?
    * If user is logging out, logging back in will get the same alerts.
    */
    CFArrayRemoveAllValues(sPendedNonsecureKextPaths);
    CFDictionaryRemoveAllValues(sNotifiedNonsecureKextPaths);

    return;
}

/*******************************************************************************
*******************************************************************************/
void _checkNotificationQueue(void * info __unused)
{
    CFStringRef       kextPath          = NULL;  // do not release
    CFMutableArrayRef alertMessageArray = NULL;  // must release

    OSKextLog(/* kext */ NULL,
        kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
        "Checking user notification queue.");

    if (sConsoleUser == (uid_t)-1 || sCurrentNotificationRunLoopSource) {
        goto finish;
    }

    if (CFArrayGetCount(sPendedNonsecureKextPaths)) {
        kextPath = (CFStringRef)CFArrayGetValueAtIndex(
            sPendedNonsecureKextPaths, 0);
        alertMessageArray = CFArrayCreateMutable(
            kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        if (!kextPath || !alertMessageArray) {
            goto finish;
        }
        
       /* This is the localized format string for the alert message.
        */
        CFArrayAppendValue(alertMessageArray,
            CFSTR("The system extension \""));
        CFArrayAppendValue(alertMessageArray, kextPath);
        CFArrayAppendValue(alertMessageArray,
            CFSTR("\" was installed improperly and cannot be used. "
                  "Please try reinstalling it, or contact the product's vendor "
                  "for an update."));

        kextd_raise_notification(CFSTR("System extension cannot be used"),
            alertMessageArray);
    }

finish:
    SAFE_RELEASE(alertMessageArray);
    if (kextPath) {
        CFArrayRemoveValueAtIndex(sPendedNonsecureKextPaths, 0);
    }

    return;
}

/****************************************************************************
*******************************************************************************/
Boolean recordNonsecureKexts(CFArrayRef kextList)
{
    Boolean     result             = false;
    CFStringRef nonsecureKextPath  = NULL;  // must release
    CFIndex     count, i;

    if (kextList && (count = CFArrayGetCount(kextList))) {
        for (i = 0; i < count; i ++) {
            OSKextRef checkKext = (OSKextRef)CFArrayGetValueAtIndex(kextList, i);

            SAFE_RELEASE_NULL(nonsecureKextPath);

            if (OSKextIsAuthentic(checkKext)) {
                continue;
            }
            nonsecureKextPath = copyKextPath(checkKext);
            if (!nonsecureKextPath) {
                OSKextLogMemError();
                goto finish;
            }
            if (!CFDictionaryGetValue(sNotifiedNonsecureKextPaths,
                nonsecureKextPath)) {
                
                CFArrayAppendValue(sPendedNonsecureKextPaths,
                    nonsecureKextPath);
                CFDictionarySetValue(sNotifiedNonsecureKextPaths,
                    nonsecureKextPath, kCFBooleanTrue);
                    
                result = true;
            }
        }
    }

finish:
    SAFE_RELEASE(nonsecureKextPath);

    if (result) {
        CFRunLoopSourceSignal(sNotificationQueueRunLoopSource);
        CFRunLoopWakeUp(CFRunLoopGetCurrent());
    }
    return result;
}

/*******************************************************************************
*******************************************************************************/
void kextd_raise_notification(
    CFStringRef alertHeader,
    CFArrayRef  alertMessageArray)
{
    CFMutableDictionaryRef alertDict               = NULL;  // must release
    CFURLRef               iokitFrameworkBundleURL = NULL;  // must release
    SInt32                 userNotificationError   = 0;

    OSKextLog(/* kext */ NULL,
        kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
        "Raising user notification.");

    if (sConsoleUser == (uid_t)-1) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
            "No logged in user.");
        goto finish;
    }

    alertDict = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!alertDict) {
        goto finish;
    }

    iokitFrameworkBundleURL = CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault,
        CFSTR("/System/Library/Frameworks/IOKit.framework"),
        kCFURLPOSIXPathStyle, true);
    if (!iokitFrameworkBundleURL) {
        goto finish;
    }

    CFDictionarySetValue(alertDict, kCFUserNotificationLocalizationURLKey,
        iokitFrameworkBundleURL);
    CFDictionarySetValue(alertDict, kCFUserNotificationAlertHeaderKey,
        alertHeader);
    CFDictionarySetValue(alertDict, kCFUserNotificationDefaultButtonTitleKey,
        CFSTR("OK"));
    CFDictionarySetValue(alertDict, kCFUserNotificationAlertMessageKey,
        alertMessageArray);

    sCurrentNotification = CFUserNotificationCreate(kCFAllocatorDefault,
        0 /* time interval */, kCFUserNotificationCautionAlertLevel,
        &userNotificationError, alertDict);
    if (!sCurrentNotification) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag, 
            "Can't create user notification - %d",
            (int)userNotificationError);
        goto finish;
    }

     sCurrentNotificationRunLoopSource = CFUserNotificationCreateRunLoopSource(
         kCFAllocatorDefault, sCurrentNotification,
         &_notificationDismissed, /* order */ 5 /* xxx - cheesy! */);
    if (!sCurrentNotificationRunLoopSource) {
        CFRelease(sCurrentNotification);
        sCurrentNotification = NULL;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), sCurrentNotificationRunLoopSource,
        kCFRunLoopDefaultMode);

finish:
    SAFE_RELEASE(alertDict);
    SAFE_RELEASE(iokitFrameworkBundleURL);

    return;
}

/*******************************************************************************
*******************************************************************************/
void _notificationDismissed(
    CFUserNotificationRef userNotification,
    CFOptionFlags         responseFlags)
{

    if (sCurrentNotification) {
        CFRelease(sCurrentNotification);
        sCurrentNotification = NULL;
    }

    if (sCurrentNotificationRunLoopSource) {
        CFRunLoopRemoveSource(CFRunLoopGetCurrent(), sCurrentNotificationRunLoopSource,
            kCFRunLoopDefaultMode);
        CFRelease(sCurrentNotificationRunLoopSource);
        sCurrentNotificationRunLoopSource = NULL;
    }

    CFRunLoopSourceSignal(sNotificationQueueRunLoopSource);
    CFRunLoopWakeUp(CFRunLoopGetCurrent());

    return;
}

#endif
