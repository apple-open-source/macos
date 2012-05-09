/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 * 29-Aug-02 ebold created
 *
 */
#ifndef _PMAssertions_h_
#define _PMAssertions_h_

#include <IOKit/pwr_mgt/IOPM.h>

/* ExternalMedia assertion
 * This assertion is only defined here in PM configd. 
 * It can only be asserted by PM configd; not by other user processes.
 */
#define _kIOPMAssertionTypeExternalMediaCStr    "ExternalMedia"
#define _kIOPMAssertionTypeExternalMedia        CFSTR(_kIOPMAssertionTypeExternalMediaCStr)


/* IOPMAssertion levels
 * 
 * Each assertion type has a corresponding bitfield index, here.
 * Also as found under the "Bitfields" property in assertion CFDictionaries.
 */
enum {
    // These must be consecutive integers beginning at 0
    kHighPerfIndex                  = 0,        // 1
    kPreventIdleIndex               = 1,        // 2
    kDisableInflowIndex             = 2,        // 4
    kInhibitChargeIndex             = 3,        // 8
    kDisableWarningsIndex           = 4,        // 16
    kPreventDisplaySleepIndex       = 5,        // 32
    kEnableIdleIndex                = 6,        // 64
    kNoRealPowerSourcesDebugIndex   = 7,        // 128
    kPreventSleepIndex              = 8,        // 256
    kExternalMediaIndex             = 9,        // 512
    kDeclareUserActivity            = 10,       // 1024
    kPushServiceTaskAssertion       = 11,       // 2048
    // Make sure this is the last enum element, as it tells us the total
    // number of elements in the enum definition
    kIOPMNumAssertionTypes      
};

 
__private_extern__ void PMAssertions_prime(void);
__private_extern__ void createOnBootAssertions(void);

__private_extern__ IOReturn _IOPMSetActivePowerProfilesRequiresRoot(
                                CFDictionaryRef which_profile, 
                                int uid, 
                                int gid);
                        
__private_extern__ IOReturn _IOPMAssertionCreateRequiresRoot(
                                mach_port_t task_port, 
                                char *nameCStr,
                                char *assertionCStr,
                                int level, 
                                int *assertion_id);

__private_extern__ void _TaskPortInvalidatedCallout(CFMachPortRef port, void *info);

__private_extern__ void _PMAssertionsDriverAssertionsHaveChanged(uint32_t changedDriverAssertions);

__private_extern__ void _ProxyAssertions(const struct IOPMSystemCapabilityChangeParameters *capArgs);


__private_extern__ void PMAssertions_TurnOffAssertions_ApplePushServiceTask(void);

__private_extern__ IOReturn InternalCreateAssertion(
                                CFDictionaryRef properties, 
                                IOPMAssertionID *outID);

__private_extern__ void InternalReleaseAssertion(
                                IOPMAssertionID *outID);

__private_extern__ void InternalEvaluateAssertions(void);

__private_extern__ void evalAllUserActivityAssertions(unsigned int dispSlpTimer);

__private_extern__ CFMutableDictionaryRef	_IOPMAssertionDescriptionCreate(
                            CFStringRef AssertionType, 
                            CFStringRef Name, 
                            CFStringRef Details,
                            CFStringRef HumanReadableReason,
                            CFStringRef LocalizationBundlePath,
                            CFTimeInterval Timeout,
                            CFStringRef TimeoutBehavior);

#endif
