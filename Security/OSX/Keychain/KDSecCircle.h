/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#import <Security/SecureObjectSync/SOSCloudCircle.h>
#import <Foundation/Foundation.h>

@interface KDSecCircle : NSObject

@property (readonly) BOOL isInCircle;
@property (readonly) BOOL isOutOfCircle;

@property (readonly) SOSCCStatus rawStatus;

@property (readonly) NSString *status;
@property (readonly) NSError *error;

// Both of these are arrays of KDCircelPeer objects
@property (readonly) NSArray *peers;
@property (readonly) NSArray *applicants;

-(void)addChangeCallback:(dispatch_block_t)callback;
-(id)init;

// these are "try to", and may (most likely will) not complete by the time they return
-(void)enableSync;
-(void)disableSync;
-(void)rejectApplicantId:(NSString*)applicantId;
-(void)acceptApplicantId:(NSString*)applicantId;

@end
