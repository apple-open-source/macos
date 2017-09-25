/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>

#if __OBJC2__

@interface SFAnalyticsLogger : NSObject <NSURLSessionDelegate>

+ (instancetype)logger;

+ (NSInteger)fuzzyDaysSinceDate:(NSDate*)date;

- (void)logSuccessForEventNamed:(NSString*)eventName;
- (void)logHardFailureForEventNamed:(NSString*)eventName withAttributes:(NSDictionary*)attributes;
- (void)logSoftFailureForEventNamed:(NSString*)eventName withAttributes:(NSDictionary*)attributes;

- (void)noteEventNamed:(NSString*)eventName;

// --------------------------------
// Things below are for subclasses

// Override to create a concrete logger instance
@property (readonly, class) NSString* databasePath;

// Storing dates
- (void)setDateProperty:(NSDate*)date forKey:(NSString*)key;
- (NSDate*)datePropertyForKey:(NSString*)key;

- (NSDictionary*)extraValuesToUploadToServer;
- (NSString*)sysdiagnoseStringForEventRecord:(NSDictionary*)eventRecord;

// --------------------------------
// Things below are for utilities to drive and/or test the system

- (NSString*)getSysdiagnoseDumpWithError:(NSError**)error;
- (NSData*)getLoggingJSONWithError:(NSError**)error;
- (BOOL)forceUploadWithError:(NSError**)error;

// --------------------------------
// Things below are for unit testing

@property (readonly) dispatch_queue_t splunkLoggingQueue;
@property (readonly) NSURL* splunkUploadURL;
@property (readonly) NSString* splunkTopicName;
@property (readonly) NSURL* splunkBagURL;
@property (readonly) BOOL allowsInsecureSplunkCert;
@property BOOL ignoreServerDisablingMessages;

@end

#endif
