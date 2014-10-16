/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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
//  CKDUserInteraction.m
//  CloudKeychainProxy
//

#import <CoreFoundation/CFUserNotification.h>
#import <utilities/debugging.h>

#import "CKDUserInteraction.h"
#import "SOSARCDefines.h"

static CKDUserInteraction *sharedInstance = nil;
static CKDUserInteractionBlock completion;

#define XPROXYUISCOPE "proxy-ui"

static void userNotificationCallback(CFUserNotificationRef userNotification, CFOptionFlags responseFlags)
{
	
/*  
    kCFUserNotificationDefaultResponse		= 0,
    kCFUserNotificationAlternateResponse	= 1,
    kCFUserNotificationOtherResponse		= 2,
    kCFUserNotificationCancelResponse		= 3
*/

    CKDUserInteraction *sharedInstance = [CKDUserInteraction sharedInstance];
    CFDictionaryRef userResponses = CFUserNotificationGetResponseDictionary(userNotification);
		// CFOptionFlags are poorly named, since it's a single response value, not a mask

	secdebug(XPROXYUISCOPE, "sharedInstance: %@, rlsr: %@", sharedInstance, sharedInstance.runLoopSourceRef);
	secdebug(XPROXYUISCOPE, "userNotification responses: %@, flags: %#lx",userResponses, responseFlags);
    
    if (sharedInstance.runLoopSourceRef)
        CFRunLoopRemoveSource(CFRunLoopGetMain(), sharedInstance.runLoopSourceRef, kCFRunLoopDefaultMode);
    
    if (completion)  // sharedInstance.completion
	{
        secdebug(XPROXYUISCOPE, "Calling user completion routine");
        completion(userResponses, responseFlags);   // sharedInstance.completion
    }
    
    secdebug(XPROXYUISCOPE, "Releasing user completion routine");
//    Block_release(completion);   // sharedInstance.completion
    
/*
       if (responseFlags & kCFUserNotificationCancelResponse) {
                returnCode = kABNotifierUserCancelled;
        } else {
                fUsername = (CFStringRef)CFUserNotificationGetResponseValue(notification, kCFUserNotificationTextFieldValuesKey, 0);
                if(fUsername) CFRetain(fUsername);
                
                fPassword = (CFStringRef)CFUserNotificationGetResponseValue(notification, kCFUserNotificationTextFieldValuesKey, 1);
                if(fPassword) CFRetain(fPassword);
                
                if((response & CFUserNotificationCheckBoxChecked(0)))
*/

//    if (responseFlags == kCFUserNotificationCancelResponse || responseFlags == kCFUserNotificationDefaultResponse)
    CFRunLoopStop(CFRunLoopGetCurrent());
	secdebug(XPROXYUISCOPE, "exit");
}

@implementation CKDUserInteraction

+ (CKDUserInteraction *) sharedInstance
{
    if (!sharedInstance)
		sharedInstance = [[self alloc] init];

    return sharedInstance;
}

- (void)requestShowNotification:(NSDictionary *)infoForUserInfo completion:(CKDUserInteractionBlock)completionf
{
    __block CFOptionFlags flags = kCFUserNotificationCautionAlertLevel | kCFUserNotificationNoDefaultButtonFlag;
    CFTimeInterval timeout = 30.0;
    
//  completion = Block_copy(completionf);
    completion = completionf;
    
    CFStringRef headerStr = (__bridge CFStringRef)([infoForUserInfo objectForKey:(__bridge id)kCFUserNotificationAlertHeaderKey]);

    CFStringRef cancelStr = CFSTR("No way"); //CFStringCreateWithCString(kCFAllocatorDefault, cancel.c_str(), kCFStringEncodingUTF8);
    CFStringRef defaultStr = CFSTR("Sure"); //CFStringCreateWithCString(kCFAllocatorDefault, settings.c_str(), kCFStringEncodingUTF8);

    CFMutableDictionaryRef notifyDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    // Header and buttons
    CFDictionarySetValue(notifyDictionary, kCFUserNotificationAlertHeaderKey, headerStr);
    CFDictionarySetValue(notifyDictionary, kCFUserNotificationDefaultButtonTitleKey, defaultStr);
    CFDictionarySetValue(notifyDictionary, kCFUserNotificationAlternateButtonTitleKey, cancelStr);

    SInt32 error = 0;
    _userNotificationRef = CFUserNotificationCreate(kCFAllocatorDefault, timeout, flags, &error, notifyDictionary);

    if (_userNotificationRef)
    {
        _runLoopSourceRef = CFUserNotificationCreateRunLoopSource(kCFAllocatorDefault, _userNotificationRef, userNotificationCallback, 0);
        CFRunLoopAddSource(CFRunLoopGetMain(), _runLoopSourceRef, kCFRunLoopDefaultMode);
	}

}

@end

