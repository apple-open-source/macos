/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <libc.h>
#include <stdlib.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>

#define USE_SYSTEMCONFIGURATION_PUBLIC_APIS
#include <SystemConfiguration/SystemConfiguration.h>

#include "Configuration.h"

#ifdef _SCDYNAMICSTORE_H // use the new APIS

static int getBoolSettingForKey(CFStringRef key)
{
        SCPreferencesRef   session;
        CFTypeRef       value;
        int 		retVal = -1;

        session = SCPreferencesCreate(NULL,
                         	CFSTR("autodiskmount"),
                         	CFSTR("autodiskmount.xml"));
        if (!session) {
                retVal = -1;
        } else {
                value = SCPreferencesGetValue(session, key);

                if (!value) {
                        retVal = -1;
                } else {
                    // set the check box
                    if (CFEqual(value, kCFBooleanTrue)) {
                            retVal = 1;
                    } else {
                            retVal = 0;
                    }
                }

                CFRelease(session);
        }

        return retVal;
}

#else // old API's  (Pre Puma)

static int getBoolSettingForKey(CFStringRef key)
{
        SCPSessionRef   session;
        SCPStatus       status;
        CFTypeRef       value;
        int 		retVal = -1;

        status = SCPOpen(&session,
                                CFSTR("autodiskmount"),
                                CFSTR("autodiskmount.xml"),
                                0);
        if (status != SCP_OK) {
                retVal = -1;
        } else {
                status = SCPGet(session, key, &value);

                if (status == SCP_NOKEY) {
                        retVal = -1;
                } else {
                    // set the check box
                    if (CFEqual(value, kCFBooleanTrue)) {
                            retVal = 1;
                    } else {
                            retVal = 0;
                    }
                }

                status = SCPClose(&session);
        }

        return retVal;
}


#endif

int mountWithoutConsoleUser(void)
{
        int setting = getBoolSettingForKey(CFSTR("AutomountDisksWithoutUserLogin"));

        if (setting == 1 || setting == 0) {
                return setting;
        }

        return FALSE;
}

int canMountRemovableDisks(void)
{
        int setting = getBoolSettingForKey(CFSTR("AutomountDisks"));

        if (setting == 1 || setting == 0) {
                return setting;
        }

        return FALSE;
}

int consoleDevicesAreOwnedByMountingUser(void)
{
        int setting = getBoolSettingForKey(CFSTR("ModifyDevEntriesForRemovable"));

        if (setting == 1 || setting == 0) {
                return setting;
        }

        return TRUE;
}

int shouldFsckReadOnlyMedia(void)
{
        int setting = getBoolSettingForKey(CFSTR("ShouldFsckReadOnlyMedia"));

        if (setting == 1 || setting == 0) {
                return setting;
        }

        return FALSE;

}

int strictRemovableMediaSettings(void)
{
        int setting = getBoolSettingForKey(CFSTR("StrictRemovableMediaPermissions"));

        if (setting == 1 || setting == 0) {
                return setting;
        }

        return FALSE;

}

int attemptMountRemovableMediaSuid(void)
{
        int setting = getBoolSettingForKey(CFSTR("AttemptToMountRemovableMediaSuid"));

        if (setting == 1 || setting == 0) {
                return setting;
        }

        return FALSE;

}

int attemptMountFixedMediaSuid(void)
{
        int setting = getBoolSettingForKey(CFSTR("AttemptToMountFixedMediaSuid"));

        if (setting == 1 || setting == 0) {
                return setting;
        }

        return FALSE;

}

int attemptMountRemovableMediaDev(void)
{
        int setting = getBoolSettingForKey(CFSTR("AttemptToMountRemovableMediaDev"));

        if (setting == 1 || setting == 0) {
                return setting;
        }

        return FALSE;

}

int attemptMountFixedMediaDev(void)
{
        int setting = getBoolSettingForKey(CFSTR("AttemptToMountFixedMediaDev"));

        if (setting == 1 || setting == 0) {
                return setting;
        }

        return FALSE;

}
int attemptMountRemovableMediaExe(void)
{
        int setting = getBoolSettingForKey(CFSTR("AttemptToMountRemovableMediaExe"));

        if (setting == 1 || setting == 0) {
                return setting;
        }

        return TRUE;

}

int attemptMountFixedMediaExe(void)
{
        int setting = getBoolSettingForKey(CFSTR("AttemptToMountFixedMediaExe"));

        if (setting == 1 || setting == 0) {
                return setting;
        }

        return TRUE;

}