/*
 * Copyright (c) 2012 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <err.h>
#include "preferences.h"
#include "plog.h"

SCPreferencesRef				gPrefs = NULL;

static SCPreferencesContext		prefsContext = { 0, NULL, NULL, NULL, NULL };

static void
prefscallout (SCPreferencesRef           prefs,
			  SCPreferencesNotification  notificationType,
			  void                      *context)
{
	if ((notificationType & kSCPreferencesNotificationApply) != 0) {
		// other prefs here
		plogreadprefs();
	}
	
	return;
}

void
prefsinit (void)
{
	if (!gPrefs) {
		if ((gPrefs = SCPreferencesCreate(0, CFSTR("racoon"), CFSTR("com.apple.ipsec.plist")))) {
			if (SCPreferencesSetCallback(gPrefs, prefscallout, &prefsContext)) {
				if (!SCPreferencesSetDispatchQueue(gPrefs, dispatch_get_main_queue())) {
					errx(1, "failed to initialize dispatch queue.\n");
				}
			}
		}
	}
}

