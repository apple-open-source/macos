/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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

#include "PMtestLib.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <stdio.h>

#define overrideSetting         kIOPMPrioritizeNetworkReachabilityOverSleepKey

int main(int argc, char *argv[])
{
    int j = 1;
    CFMutableDictionaryRef      ov = NULL;
    CFNumberRef         overrideValue = NULL;
    
	PMTestInitialize("Testing IOPMOverrideDefaultPMPreferences API", "com.appl.powermanagement.test");
	
	PMTestLog("These tests must be run as root to exercise IOKit API, though they may PASS if not run as root.\n");
    
	PMTestLog("Test #1 override setting with per-power source values.\n");
	PMTestLog("		Overriding \"%s\" with value %d\n", overrideSetting, j);
	
    ov = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
    overrideValue = CFNumberCreate(0, kCFNumberIntType, &j);
	
	CFMutableDictionaryRef a, b, c;
	
	a = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	b = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	c = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
	CFDictionarySetValue(a, CFSTR(overrideSetting), overrideValue);
	CFDictionarySetValue(b, CFSTR(overrideSetting), overrideValue);
	CFDictionarySetValue(c, CFSTR(overrideSetting), overrideValue);
	
	CFDictionarySetValue(ov, CFSTR(kIOPMACPowerKey), a);
	CFDictionarySetValue(ov, CFSTR(kIOPMBatteryPowerKey), b);
	CFDictionarySetValue(ov, CFSTR(kIOPMUPSPowerKey), c);
	
	CFRelease(a);
	CFRelease(b);
	CFRelease(c);

    IOPMOverrideDefaultPMPreferences(ov);
	
	CFRelease(ov);
	
	PMTestPass("Called IOPMOverrideDefaultPMPreferences - but this test does not check the output.");
	
	PMTestLog("Test #2 override the setting globally (i.e. no different values across power sources).\n");
	PMTestLog("		Overriding \"%s\" with value %d\n", overrideSetting, j);

	CFMutableDictionaryRef ov2 = NULL;
    ov2 = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	CFDictionarySetValue(ov, CFSTR(overrideSetting), overrideValue);
    
    IOPMOverrideDefaultPMPreferences(ov2);

	CFRelease(ov2);
	PMTestPass("Called IOPMOverrideDefaultPMPreferences - but this test does not check the output.");

	return 0;
};