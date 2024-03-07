/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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
 * EAPWiFiUtil.m
 * - C wrapper functions over ObjC interface to CoreWiFi Framework
 */

#import <Foundation/Foundation.h>
#import <CoreWiFi/CoreWiFi.h>
#if TARGET_OS_IPHONE
#include "EAPOLSIMPrefsManage.h"
#endif /* TARGET_OS_IPHONE */
#include "EAPLog.h"
#include "EAPWiFiUtil.h"

static Boolean S_wifi_power_state;

static void
EAPWiFiHandlePowerStatusChange(Boolean powered_on)
{
    /*
     * increment the generation ID in SC prefs so eapclient would know
     * that wifi power was toggled from ON to OFF and it should not
     * use the SIM specific stored info.
     * So turning WiFi power off is similar to ejecting SIM as both actions
     * lead to tearing down the 802.1X connection and incrementing the
     * generation ID.
     */
    if (S_wifi_power_state == TRUE && powered_on == FALSE) {
	EAPLOG_FL(LOG_INFO, "WiFi power is turned off");
	EAPOLSIMGenerationIncrement();
    }
    S_wifi_power_state = powered_on;
}

static void
EAPWiFiHandleCWFEvents(CWFInterface *cwfInterface, CWFEvent *event)
{
    @autoreleasepool {
	switch (event.type) {
	    case CWFEventTypePowerChanged:
	    {
		Boolean on = cwfInterface.powerOn ? TRUE : FALSE;
		EAPLOG_FL(LOG_DEBUG, "power state changed to %s", on ? "ON" : "OFF");
		EAPWiFiHandlePowerStatusChange(on);
	    }
	    default:
		break;
	}
    }
    return;
}

void
EAPWiFiMonitorPowerStatus(void)
{
#if TARGET_OS_IOS || TARGET_OS_WATCH
    @autoreleasepool {
	dispatch_queue_t 	queue = dispatch_queue_create("EAP WiFi Interface Queue", NULL);
	CWFInterface 		*cwfInterface = [[CWFInterface alloc] init];

	cwfInterface.eventHandler = ^( CWFEvent *cwfEvent ) {
	    dispatch_async(queue, ^{
		EAPWiFiHandleCWFEvents(cwfInterface, cwfEvent);
	    });
	};
	dispatch_async(queue, ^{
	    [cwfInterface activate];
	    S_wifi_power_state = cwfInterface.powerOn ? TRUE : FALSE;
	    [cwfInterface startMonitoringEventType:CWFEventTypePowerChanged error:nil];
	});
    }
#endif
    return;
}
