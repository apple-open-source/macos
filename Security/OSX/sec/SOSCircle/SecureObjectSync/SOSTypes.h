/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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

#ifndef _SECURITY_SOSTYPES_H_
#define _SECURITY_SOSTYPES_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

/*
 Reasons
 */

typedef enum SyncWithAllPeersReason {
    kSyncWithAllPeersOtherFail = 0,
    kSyncWithAllPeersSuccess,
    kSyncWithAllPeersLocked,
} SyncWithAllPeersReason;

typedef enum HandleIDSMessageReason {
    kHandleIDSMessageDontHandle = 0,
    kHandleIDSMessageNotReady,
    kHandleIDSMessageSuccess,
    kHandleIDSMessageLocked,
    kHandleIDSmessageDeviceIDMismatch
} HandleIDSMessageReason;


/*
 View Result Codes
 */
enum {
    kSOSCCGeneralViewError    = 0,
    kSOSCCViewMember          = 1,
    kSOSCCViewNotMember       = 2,
    kSOSCCViewNotQualified    = 3,
    kSOSCCNoSuchView          = 4,
    kSOSCCViewPending         = 5,
    kSOSCCViewAuthErr         = 6,
};
typedef int SOSViewResultCode;


/*
 View Action Codes
 */
enum {
    kSOSCCViewEnable          = 1,
    kSOSCCViewDisable         = 2,
    kSOSCCViewQuery           = 3,
};
typedef int SOSViewActionCode;

/*
 SecurityProperty Result Codes
 */
enum {
    kSOSCCGeneralSecurityPropertyError    = 0,
    kSOSCCSecurityPropertyValid           = 1,
    kSOSCCSecurityPropertyNotValid        = 2,
    kSOSCCSecurityPropertyNotQualified    = 3,
    kSOSCCNoSuchSecurityProperty          = 4,
    kSOSCCSecurityPropertyPending         = 5,
};
typedef int SOSSecurityPropertyResultCode;


/*
 SecurityProperty Action Codes
 */
enum {
    kSOSCCSecurityPropertyEnable          = 1,
    kSOSCCSecurityPropertyDisable         = 2,
    kSOSCCSecurityPropertyQuery           = 3,
};
typedef int SOSSecurityPropertyActionCode;

__END_DECLS

#endif
