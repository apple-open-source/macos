/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
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

#include <notify.h>
#include <stdio.h>
#include <IOKit/platform/IOPlatformSupportPrivate.h>
#include "Platform.h"
#include "PrivateLib.h"
#include "PMSettings.h"
#include "SystemLoad.h"

__private_extern__ CFAbsoluteTime   get_SleepFromUserWakeTime();

__private_extern__ TCPKeepAliveStruct   *gTCPKeepAlive = NULL;

static  bool    pushConnectionActive = false;

#define kTCPWakeQuotaCountDefault               20
#define kTCPWakeQuotaIntervalSecDefault         3ULL*60ULL*60ULL



static void lazyAllocTCPKeepAlive(void)
{
    CFDictionaryRef         platformFeatures = NULL;
    CFNumberRef             expirationTimeout = NULL;
    
    if (gTCPKeepAlive) {
        return;
    }
    
    platformFeatures = _copyRootDomainProperty(CFSTR("IOPlatformFeatureDefaults"));
    if (platformFeatures)
    {
        gTCPKeepAlive = calloc(1, sizeof(TCPKeepAliveStruct));
        if (!gTCPKeepAlive) return;

        if (kCFBooleanTrue == CFDictionaryGetValue(platformFeatures,
                                                   kIOPlatformTCPKeepAliveDuringSleep))
        {
            if ((expirationTimeout = CFDictionaryGetValue(platformFeatures,
                                                          CFSTR("TCPKeepAliveExpirationTimeout"))))
            {
                CFNumberGetValue(expirationTimeout, kCFNumberLongType, &gTCPKeepAlive->overrideSec);
            }
            else {
                gTCPKeepAlive->overrideSec = kTCPKeepAliveExpireSecs; // set to a default value
            }
            gTCPKeepAlive->state = kActive;
        }
        else {
            gTCPKeepAlive->state = kNotSupported;
        }
        CFRelease(platformFeatures);
    }

    return;
}

__private_extern__ CFTimeInterval getTcpkaTurnOffTime( )
{

    if ((!gTCPKeepAlive) || (gTCPKeepAlive->state != kActive) ||
        (gTCPKeepAlive->ts_turnoff == 0)) 
        return 0;

    // Add additional 60 secs, just to make sure dispatch timer is expired 
    // when system wakes up for turning off
    return (gTCPKeepAlive->ts_turnoff + 60);
}

__private_extern__ void cancelTCPKeepAliveExpTimer( )
{

    if (gTCPKeepAlive && gTCPKeepAlive->expiration)
        dispatch_source_cancel(gTCPKeepAlive->expiration);
}
__private_extern__ void startTCPKeepAliveExpTimer( )
{
    if ((!gTCPKeepAlive) || (gTCPKeepAlive->state != kActive)) return;

    if (!gTCPKeepAlive->expiration) {
        gTCPKeepAlive->expiration = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0,
                                                           0, dispatch_get_main_queue());
        dispatch_source_set_event_handler(gTCPKeepAlive->expiration, ^{
                           gTCPKeepAlive->state = kInactive;
                           configAssertionType(kInteractivePushServiceType, false);
                           });
        dispatch_source_set_cancel_handler(gTCPKeepAlive->expiration, ^{
                           if (gTCPKeepAlive->expiration) {
                                dispatch_release(gTCPKeepAlive->expiration);
                                gTCPKeepAlive->expiration = 0;
                                gTCPKeepAlive->state = kActive;
                                gTCPKeepAlive->ts_turnoff = 0;
                           }
                           });
    }
    else {
        dispatch_suspend(gTCPKeepAlive->expiration);
    }

    gTCPKeepAlive->ts_turnoff = CFAbsoluteTimeGetCurrent() + gTCPKeepAlive->overrideSec;

    dispatch_source_set_timer(gTCPKeepAlive->expiration,
                              dispatch_walltime(NULL, gTCPKeepAlive->overrideSec * NSEC_PER_SEC),
                              DISPATCH_TIME_FOREVER, 0);
    dispatch_resume(gTCPKeepAlive->expiration);

}

__private_extern__
tcpKeepAliveStates_et  getTCPKeepAliveState(char *buf, int buflen)
{
    tcpKeepAliveStates_et   state = kInactive;

    lazyAllocTCPKeepAlive();
    
    if (!gTCPKeepAlive || (gTCPKeepAlive->state == kNotSupported))
    {
        if (buf) snprintf(buf, buflen, "unsupported");
        return kNotSupported;
    }

    if ((gTCPKeepAlive->state == kActive) && pushConnectionActive) {
        state = kActive;
        if (buf) snprintf(buf, buflen, "active");
    }
    else {
        state = kInactive;
        if (buf) snprintf(buf, buflen, "inactive");
    }
    return state;

}

__private_extern__ long getTCPKeepAliveOverrideSec( )
{
    if (gTCPKeepAlive)
        return gTCPKeepAlive->overrideSec;

    return 0;
}

__private_extern__ void enableTCPKeepAlive()
{
    if (!gTCPKeepAlive || (gTCPKeepAlive->state == kNotSupported))
        return;
    cancelTCPKeepAliveExpTimer();
    gTCPKeepAlive->state = kActive;

}


__private_extern__ void disableTCPKeepAlive()
{
    if (!gTCPKeepAlive || (gTCPKeepAlive->state == kNotSupported))
        return;
    cancelTCPKeepAliveExpTimer();
    gTCPKeepAlive->state = kInactive;

}

/* 
 * Evaluate TCP Keep Alive(Tcpka) expiration timer for Power source change
 * No expiration timer when on AC power source
 */
__private_extern__ void evalTcpkaForPSChange()
{
    static int  prevPwrSrc = -1;
    int         pwrSrc;

    pwrSrc = _getPowerSource();

    if (pwrSrc == prevPwrSrc)
        return; // If power source hasn't changed, there is nothing to do

    prevPwrSrc = pwrSrc;

    if (!gTCPKeepAlive || (gTCPKeepAlive->state == kNotSupported))
        return;

    if (getSessionUserActivity() == true) {
        // If user is active in this wake session, there's nothing to be done here
        // This case is handled when system is going to sleep
        return;
    }

    if (_getPowerSource() == kBatteryPowered) 
        startTCPKeepAliveExpTimer();
    else {
        cancelTCPKeepAliveExpTimer();
        if (gTCPKeepAlive->state == kInactive)
            gTCPKeepAlive->state = kActive;
    }

}

__private_extern__ void setPushConnectionState(bool active)
{
    pushConnectionActive = active;
}

__private_extern__ bool getPushConnectionState()
{
    return pushConnectionActive;
}
/*
 * Returns if WakeOnLan feature is allowed(true/false).
 * Checks if user has enabled it and also if thermal state allows it.
 */
__private_extern__ bool getWakeOnLanState()
{
    uint32_t thermalState = getSystemThermalState();
    int64_t value = 0;

    if ((thermalState == kIOPMThermalLevelWarning) || (thermalState == kIOPMThermalLevelTrap))
        return false;

    if ((GetPMSettingNumber(CFSTR(kIOPMWakeOnLANKey), &value) == kIOReturnSuccess) &&
        (value == 1)) 
        return true;

    return false;
}

