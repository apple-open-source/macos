/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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


#include <SOSCircle/Regressions/SOSTestDevice.h>
#include "secd_regressions.h"
#include "SecdTestKeychainUtilities.h"

static int kTestTestCount = 581;

// Smash together identical items, but mute the devices every once in a while.
// (Simulating packet loss.)
static void smash(void) {
    __block int iteration=0;
    SOSTestDeviceListTestSync("smash", test_directive, test_reason, 0, true, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest) {
        if (iteration < 100 && iteration % 10 == 0) {
            SOSTestDeviceSetMute(source, !SOSTestDeviceIsMute(source));
        }
        return false;
    }, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest, SOSMessageRef message) {
        if (iteration++ < 200) {
            CFStringRef name = CFStringCreateWithFormat(NULL, NULL, CFSTR("smash-post-%d"), iteration);
            SOSTestDeviceAddGenericItem(source, name, name);
            SOSTestDeviceAddGenericItem(dest, name, name);
            CFReleaseNull(name);
            return true;
        }
        return false;
    }, CFSTR("alice"), CFSTR("bob"), NULL);
}

int secd_70_engine_smash(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    /* custom keychain dir */
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);

    smash();

    return 0;
}
