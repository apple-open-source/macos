/*
 * Copyright (c) 2016 Apple Inc. All rights reserved.
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
 *  EAPOLSIMPrefsManage.c
 * - routines to manage preferences for SIM 
 * - genration id is stored in System Preferences so eapolclient can know whether
 *   to use the information or not.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <TargetConditionals.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFPreferences.h>
#include <SystemConfiguration/SCValidation.h>
#include "symbol_scope.h"
#include "myCFUtil.h"
#include "EAPLog.h"
#include "EAPOLSIMPrefsManage.h"

#define kEAPOLSIMPrefsManageID		CFSTR("com.apple.eapol.sim.generation.plist")
#define kEAPOLSIMPrefsProcName		CFSTR("EAPOLSIMPrefsManage")
#define kEAPOLSIMPrefsGenIDKey		CFSTR("SIMGenerationID")

void
EAPOLSIMGenerationIncrement(void) {
    SCPreferencesRef	prefs = NULL;
    CFNumberRef		num = NULL;
    uint32_t 		value = 1;

    prefs = SCPreferencesCreate(NULL, kEAPOLSIMPrefsProcName, kEAPOLSIMPrefsManageID);
    if (prefs == NULL) {
	EAPLOG(LOG_NOTICE,
	       "SCPreferencesCreate failed, %s",
	       SCErrorString(SCError()));
	return;
    }

    num = SCPreferencesGetValue(prefs, kEAPOLSIMPrefsGenIDKey);
    num = isA_CFNumber(num);
    if (num != NULL) {
	CFNumberGetValue(num, kCFNumberSInt32Type, &value);
	++value;
    } 
    num = CFNumberCreate(NULL, kCFNumberSInt32Type, &value);	
    SCPreferencesSetValue(prefs, kEAPOLSIMPrefsGenIDKey, num);
    SCPreferencesCommitChanges(prefs);
    my_CFRelease(&num);
    my_CFRelease(&prefs);
    return;
}

UInt32
EAPOLSIMGenerationGet(void) {
    uint32_t 		ret_value = 0;
    SCPreferencesRef	prefs = NULL;
    CFNumberRef		num = NULL;

    prefs = SCPreferencesCreate(NULL, kEAPOLSIMPrefsProcName, kEAPOLSIMPrefsManageID);
    if (prefs == NULL) {
	EAPLOG(LOG_NOTICE,
	       "SCPreferencesCreate failed, %s",
	       SCErrorString(SCError()));
	       return 0;
    }
    num = SCPreferencesGetValue(prefs, kEAPOLSIMPrefsGenIDKey);
    num = isA_CFNumber(num);
    if (num == NULL) {
	goto done;;
    }
    CFNumberGetValue(num, kCFNumberSInt32Type, &ret_value);

done:
    my_CFRelease(&prefs);
    return ret_value;
}







