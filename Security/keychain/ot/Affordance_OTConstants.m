/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import "keychain/ot/Affordance_OTConstants.h"
#import "utilities/debugging.h"

// SecKeychainStaticPersistentRefs feature flag (for checking if static persistent refs are enabled) is enabled by defualt.The following utilities help in existing tests to set/unset this ff.
// Track the last override value, to suppress logging if it hasnt changed
static bool SecKeychainStaticPersistentRefsEnabledOverrideSet = false;
static bool SecKeychainStaticPersistentRefsEnabledOverride = false;
static BOOL persistentRefOverrideLastValue = false;
bool SecKeychainIsStaticPersistentRefsEnabled(void)
{
    if(SecKeychainStaticPersistentRefsEnabledOverrideSet) {

        if(persistentRefOverrideLastValue != SecKeychainStaticPersistentRefsEnabledOverride) {
            secnotice("octagon", "Static Persistent Refs are %@ (overridden)", SecKeychainStaticPersistentRefsEnabledOverride ? @"enabled" : @"disabled");
            persistentRefOverrideLastValue = SecKeychainStaticPersistentRefsEnabledOverride;
        }
        return SecKeychainStaticPersistentRefsEnabledOverride;
    }

    // SecKeychainStaticPersistentRefs ff is default enabled
    return true;
}

void SecKeychainSetOverrideStaticPersistentRefsIsEnabled(bool value)
{
    SecKeychainStaticPersistentRefsEnabledOverrideSet = true;
    SecKeychainStaticPersistentRefsEnabledOverride = value;
}
