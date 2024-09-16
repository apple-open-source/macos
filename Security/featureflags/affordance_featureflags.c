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

#include "affordance_featureflags.h"
#include <stdatomic.h>

typedef enum : int {
    KCSharingChangeTrackingEnabledState_DEFAULT,
    KCSharingChangeTrackingEnabledState_OVERRIDE_TRUE,
    KCSharingChangeTrackingEnabledState_OVERRIDE_FALSE,
} KCSharingChangeTrackingEnabledState;

static _Atomic(KCSharingChangeTrackingEnabledState) gSharingChangeTrackingEnabled = KCSharingChangeTrackingEnabledState_DEFAULT;

bool KCSharingIsChangeTrackingEnabled(void)
{
    KCSharingChangeTrackingEnabledState currentState = atomic_load_explicit(&gSharingChangeTrackingEnabled, memory_order_acquire);
    if (currentState != KCSharingChangeTrackingEnabledState_DEFAULT) {
        return currentState == KCSharingChangeTrackingEnabledState_OVERRIDE_TRUE;
    }

    // KCSharingAutomaticSyncing is default enabled
    return true;
}

void KCSharingSetChangeTrackingEnabled(bool enabled)
{
    KCSharingChangeTrackingEnabledState newState = enabled ? KCSharingChangeTrackingEnabledState_OVERRIDE_TRUE : KCSharingChangeTrackingEnabledState_OVERRIDE_FALSE;
    atomic_store_explicit(&gSharingChangeTrackingEnabled, newState, memory_order_release);
}

void KCSharingClearChangeTrackingEnabledOverride(void)
{
    atomic_store_explicit(&gSharingChangeTrackingEnabled, KCSharingChangeTrackingEnabledState_DEFAULT, memory_order_release);
}
