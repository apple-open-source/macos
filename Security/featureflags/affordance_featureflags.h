/*
 * Copyright (c) 2021-2024 Apple Inc. All Rights Reserved.
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
 @header affordance_featureflags.h - For functions related to default enabled feature flags used in tests
 */
// This file is mainly used to keep the default enabled feature flags related functionalities. It ensures that existing tests can use it as per requirements.

#ifndef _SECURITYD_AFFORDANCE_FEATUREFLAGS_H_
#define _SECURITYD_AFFORDANCE_FEATUREFLAGS_H_

#include <stdbool.h>

#ifdef    __cplusplus
extern "C" {
#endif

/// Indicates if change tracking is enabled for shared items. If `true`, changes
/// to shared items will be fetched and uploaded automatically.
bool KCSharingIsChangeTrackingEnabled(void);

/// Enables or disables change tracking for shared items. This is exposed for
/// testing only.
void KCSharingSetChangeTrackingEnabled(bool enabled);

/// Resets the change tracking state to the default.
void KCSharingClearChangeTrackingEnabledOverride(void);

#ifdef    __cplusplus
}
#endif

#endif // _SECURITYD_AFFORDANCE_FEATUREFLAGS_H_
