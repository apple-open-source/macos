/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

/*!
 @header SOSViews.h - views
 */

#ifndef _sec_SOSViews_
#define _sec_SOSViews_

#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>

__BEGIN_DECLS

// Internal only views, do not export.
extern const CFStringRef kSOSViewKeychainV0;
extern const CFStringRef kSOSViewKeychainV0_tomb;
extern const CFStringRef kSOSViewBackupBagV0_tomb;
extern const CFStringRef kSOSViewWiFi_tomb;
extern const CFStringRef kSOSViewAutofillPasswords_tomb;
extern const CFStringRef kSOSViewSafariCreditCards_tomb;
extern const CFStringRef kSOSViewiCloudIdentity_tomb;
extern const CFStringRef kSOSViewOtherSyncable_tomb;

typedef struct __OpaqueSOSView {
    CFRuntimeBase _base;
    CFStringRef label;
    CFMutableDictionaryRef ringnames;
} *SOSViewRef;


typedef enum {
    kViewSetAll,
    kViewSetDefault,
    kViewSetInitial,
    kViewSetAlwaysOn,
    kViewSetV0,
    kViewSetRequiredForBackup
} ViewSetKind;

CFMutableSetRef SOSViewCopyViewSet(ViewSetKind setKind);



CFSetRef SOSViewsGetV0ViewSet(void);
CFSetRef SOSViewsGetV0SubviewSet(void);
CFSetRef SOSViewsGetV0BackupViewSet(void);
CFSetRef SOSViewsGetV0BackupBagViewSet(void);

bool SOSViewsIsV0Subview(CFStringRef viewName);

// Basic interfaces to change and query views
SOSViewResultCode SOSViewsEnable(SOSPeerInfoRef pi, CFStringRef viewname, CFErrorRef *error);
bool SOSViewSetEnable(SOSPeerInfoRef pi, CFSetRef viewSet);
SOSViewResultCode SOSViewsDisable(SOSPeerInfoRef pi, CFStringRef viewname, CFErrorRef *error);
bool SOSViewSetDisable(SOSPeerInfoRef pi, CFSetRef viewSet);
SOSViewResultCode SOSViewsQuery(SOSPeerInfoRef pi, CFStringRef viewname, CFErrorRef *error);

CFSetRef SOSViewsGetAllCurrent(void);
void SOSViewsForEachDefaultEnabledViewName(void (^operation)(CFStringRef viewName));

// Test constraints
void SOSViewsSetTestViewsSet(CFSetRef testViewNames);


static inline bool SOSPeerInfoIsViewPermitted(SOSPeerInfoRef peerInfo, CFStringRef viewName) {
    SOSViewResultCode viewResult = SOSViewsQuery(peerInfo, viewName, NULL);
    
    return kSOSCCViewMember == viewResult || kSOSCCViewPending == viewResult || kSOSCCViewNotMember == viewResult;
}

const char *SOSViewsXlateAction(SOSViewActionCode action);
__END_DECLS

#endif /* defined(_sec_SOSViews_) */
