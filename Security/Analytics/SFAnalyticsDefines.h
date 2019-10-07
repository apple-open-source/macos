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

#ifndef SFAnalyticsDefines_h
#define SFAnalyticsDefines_h

#if __OBJC2__

extern NSString* const SFAnalyticsTableSuccessCount;
extern NSString* const SFAnalyticsTableHardFailures;
extern NSString* const SFAnalyticsTableSoftFailures;
extern NSString* const SFAnalyticsTableSamples;
extern NSString* const SFAnalyticsTableNotes;

extern NSString* const SFAnalyticsColumnSuccessCount;
extern NSString* const SFAnalyticsColumnHardFailureCount;
extern NSString* const SFAnalyticsColumnSoftFailureCount;
extern NSString* const SFAnalyticsColumnSampleValue;
extern NSString* const SFAnalyticsColumnSampleName;

extern NSString* const SFAnalyticsEventTime;
extern NSString* const SFAnalyticsEventType;
extern NSString* const SFAnalyticsEventTypeErrorEvent;
extern NSString* const SFAnalyticsEventErrorDestription;
extern NSString* const SFAnalyticsEventClassKey;

// Helpers for logging NSErrors
extern NSString* const SFAnalyticsAttributeErrorUnderlyingChain;
extern NSString* const SFAnalyticsAttributeErrorDomain;
extern NSString* const SFAnalyticsAttributeErrorCode;

extern NSString* const SFAnalyticsAttributeLastUploadTime;

extern NSString* const SFAnalyticsUserDefaultsSuite;

extern char* const SFAnalyticsFireSamplersNotification;

/* Internal Topic Names */
extern NSString* const SFAnalyticsTopicCloudServices;
extern NSString* const SFAnalyticsTopicKeySync;
extern NSString* const SFAnalyticsTopicTrust;
extern NSString* const SFAnalyticsTopicTransparency;

typedef NS_ENUM(NSInteger, SFAnalyticsEventClass) {
    SFAnalyticsEventClassSuccess,
    SFAnalyticsEventClassHardFailure,
    SFAnalyticsEventClassSoftFailure,
    SFAnalyticsEventClassNote
};

extern NSString* const SFAnalyticsTableSchema;

// We can only send this many events in total to splunk per upload
extern NSUInteger const SFAnalyticsMaxEventsToReport;

extern NSString* const SFAnalyticsErrorDomain;

#endif /* __OBJC2__ */

#endif /* SFAnalyticsDefines_h */
