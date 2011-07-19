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
/* FakeUPSObject */

#import <Cocoa/Cocoa.h>

#import "BatteryFakerWindowController.h"
#import <IOKit/IOCFPlugin.h>
#import "../IOUPSPlugIn/IOUPSPlugin.h"

@class BatteryFakerWindowController;

@interface FakeUPSObject : NSObject
{
    IBOutlet id ACPresentCheck;
    IBOutlet id ChargingCheck;
    IBOutlet id ConfidenceMenu;
    IBOutlet id enabledUPSCheck;
    IBOutlet id healthMenu;
    IBOutlet id MaxCapCell;
    IBOutlet id NameField;
    IBOutlet id PercentageSlider;
    IBOutlet id PresentCheck;
    IBOutlet id StatusImage;
    IBOutlet id TimeToEmptyCell;
    IBOutlet id TimeToFullCell;
    IBOutlet id TransportMenu;

    NSMutableDictionary *properties;
    BatteryFakerWindowController *owner;
    
    bool            _UPSPlugInLoaded;
}

- (NSDictionary *)properties;

- (void)UIchange;

- (void)awake;

- (void)enableAllUIInterfaces:(bool)value;

@end
