/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 * 22-Mar-02 ebold created
 * Taken entirely from ryanc and epeyton's work in Battery Monitor MenuExtra
 *
 */
 
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/bootstrap.h>
#include <mach/mach.h>
#include <mach/mach_error.h>

#ifndef _IOPM_battery_h_
#define _IOPM_battery_h_
 
#define MAX_BATTERY_NUM		2
#define kBATTERY_HISTORY	30
 
 
/* CFDictionaryRef *_IOPMCalculateBatteryInfo(CFArrayRef batteries)
    Return type:
        Returns an array of MAX_BATTERY_NUM CFDicitonaryRefs
        A NULL entry indicates no battery is present
        On a multi-battery system, this dictionary represents the sum of charges and
        time remaining across all batteries.
        CFDictionary {
            CurrentCharge = CFNumber (0-100%)
            TimeRemaninig = CFNumber (seconds)
            IsCharging = CFBoolean (If set, Time remaining indicates "time until full", otherwise "time until empty")
            ExternalPowerConnected = CFBoolean
        }
*/
bool _IOPMCalculateBatteryInfo(CFArrayRef info, CFDictionaryRef *ret);

/* _IOPMCalculateBatterySetup 
    Initializes the global variables used by battery calculation routines...
*/
void _IOPMCalculateBatterySetup(void);


#endif _IOPM_battery_h_
 
/*
#import <AppKit/AppKit.h>

#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/bootstrap.h>
#include <mach/mach.h>
#include <mach/mach_error.h>

#import "BatteryView.h"

#define kSHOW_EXTENDED				NSLocalizedString(@"Show Info Panel",@"Menu Item")
#define kHIDE_EXTENDED				NSLocalizedString(@"Hide Info Panel",@"Menu Item")

#define kDRAINING				NSLocalizedString(@"remaining",@"status text")
#define kCHARGING				NSLocalizedString(@"until full",@"status text")
#define kFULL					NSLocalizedString(@"full",@"status text")
#define kNOBATTERY				NSLocalizedString(@"no battery",@"status text")

#define POSITION_KEY 		@"Position"
#define TRANS_KEY		@"Transparency"
#define TIME_KEY		@"Time"
#define QUIT_KEY		@"Quit"
#define DEBUG_KEY		@"NSDebugMode"
                                
#define DEFAULT_TRANS		@"1"
#define DEFAULT_TIME	 	@"0"
#define DEFAULT_POSITION	@"0"
#define DEFAULT_QUIT		@"0"
#define DEFAULT_DEBUG		@"0"

#define SYSTEM_UPDATE_TIME	1
#define MAX_BATTERY_NUM		2

#define kBATTERY_HISTORY	10
@interface Battery : NSObject
{
    id		timer;
    id		testButton;
    id		debugWindow;
    id		timeText;
    id		timeWindow;
    id		miniView;
    id		moreInfoMenu;
    id		infoText;
    id		warningImage;
    id		warningPanel;
    id		noTimeLeftWindow;
    
    
    int		numberOfBatteries;

    int		batLevel[MAX_BATTERY_NUM];
    int		flags[MAX_BATTERY_NUM];
    int		charge[MAX_BATTERY_NUM];
    int		capacity[MAX_BATTERY_NUM];

    float	_batteryHistory[MAX_BATTERY_NUM][kBATTERY_HISTORY];
    int		_historyCount,_historyLimit;
    float	_hoursRemaining[MAX_BATTERY_NUM];
    int		_state;
    unsigned int	avgCount;

    id 	_batteryView;
//    BatteryView *_batteryView;


    int			_warningDisplayed;
    int			_showTime;
    float		_transparency;
    mach_port_t		batteryPort;
    BOOL		_pmExists;
    BOOL		_testMode;
    BOOL		_debugPrefSet;
    int			_minimized;
    int			_displayTimePanel;
    NSDictionary 	*aboutBoxOptions;

}
- (void)setup;

- (void)updateTimer:(id)sender;

- (void)about:(id)sender;

- (void)preferencesChanged:(id)sender;

- (void)setBatteryLevel:(id)sender;

- (void)warnNonBatteryUser:(id)sender;
- (void)endWarning:(id)sender;
//Test Functions...Should be deleted eventually...
- (void)toggleMoreInfo:(id)sender;
- (void)toggleTestMode:(id)sender;
- (void)setBattery1Level:(id)sender;
- (void)setBattery2Level:(id)sender;
- (void)setBattery1Exists:(id)sender;
- (void)setBattery2Exists:(id)sender;
- (void)setBattery1Flags:(id)sender;
- (void)setBattery2Flags:(id)sender;
@end
*/