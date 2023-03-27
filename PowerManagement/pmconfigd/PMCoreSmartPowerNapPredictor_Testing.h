/*
* Copyright (c) 2022 Apple Computer, Inc. All rights reserved.
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
//
//  PMCoreSmartPowerNapPredictor_Testing.h
//  PowerManagement
//
//  Created by Prateek Malhotra on 12/7/22.
//

#ifndef PMCoreSmartPowerNapPredictor_Testing_h
#define PMCoreSmartPowerNapPredictor_Testing_h

#import <Foundation/Foundation.h>
#import "PMCoreSmartPowerNapPredictor.h"

@interface PMCoreSmartPowerNapPredictor(Testing)
@property BOOL feature_enabled;
@property (readonly) BOOL in_coresmartpowernap;
@property (readonly) BOOL session_interrupted;
@property (readonly) BOOL should_reenter;
@property BOOL current_useractive;
@property BOOL skipEndOfSessionTimer;
@property int max_interruptions;
@property double max_interruption_duration;
@property int interruption_cooloff;
@property double interruption_session_duration;
@property NSDate *interruption_session_start;
@property NSDate *full_session_start_time;
@property NSDate *cumulative_interruption_session_start;
@property double cumulative_interruption_session_duration;
@property NSDate *predicted_end_time;


- (void)logNotEngaging;
- (void)initializeTrialClient;
- (void)updateTrialFactors;
@end



#endif /* PMCoreSmartPowerNapPredictor_Testing_h */
