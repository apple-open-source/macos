//
//  sc-102-xx
//  sec
//
//  Created by John Hurley 10/16/12.
//  Copyright 2012 Apple Inc. All rights reserved.
//

/*
    This test is a simple test of SOSCloudKeychainUserNotification to show
    an OK/Cancel dialog to the user and get the response. Run with:
    
        /AppleInternal/Applications/SecurityTests.app/SecurityTests sc_102_cfusernotification -v
         
*/

#include <AssertMacros.h>
#include <CoreFoundation/CFUserNotification.h>
#include <dispatch/dispatch.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>

#include "SOSCircle_regressions.h"
#include "SOSCloudKeychainClient.h"
#include "SOSCloudKeychainConstants.h"

#if TARGET_OS_EMBEDDED && !TARGET_IPHONE_SIMULATOR
#include <MobileGestalt.h>
#endif

static void tests(void)
{
//    CFStringRef messageToUser = CFSTR("OK to sync with world?");
//    CFStringRef messageToUser = CFSTR("Allow “Emily‘s iPad to use your iCloud Keychain?");
#if !TARGET_IPHONE_SIMULATOR
#if TARGET_OS_EMBEDDED
    CFStringRef our_peer_id = (CFStringRef)MGCopyAnswer(kMGQUserAssignedDeviceName, NULL);
#else
    CFStringRef our_peer_id = CFSTR("🔥💩");
#endif
#else
    CFStringRef our_peer_id = CFSTR("Emily‘s iPad");
#endif

    CFStringRef messageToUser = CFStringCreateWithFormat(kCFAllocatorDefault, 0, CFSTR("Allow “%@” to use your iCloud Keychain?"), our_peer_id);
    dispatch_queue_t processQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_group_t work_group = dispatch_group_create();

    // Prep the group for exitting the whole shebang.
    dispatch_group_enter(work_group);
    dispatch_group_notify(work_group, processQueue, ^
        {
            printf("Exiting via dispatch_group_notify; all work done\n");
            CFRunLoopStop(CFRunLoopGetMain());
        //  exit(0);
        });

    SOSCloudKeychainUserNotification(messageToUser, processQueue, ^ (CFDictionaryRef returnedValues, CFErrorRef error)
        {
            uint64_t flags = 0;
            pass("Reply from SOSCloudKeychainUserNotification: %@", returnedValues);
            CFStringRef nfkey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeyNotificationFlags, kCFStringEncodingUTF8);
            CFTypeRef cfflags = returnedValues ? CFDictionaryGetValue(returnedValues, nfkey) : NULL;
            if (cfflags && (CFGetTypeID(cfflags) == CFNumberGetTypeID()))
                CFNumberGetValue(cfflags, kCFNumberSInt64Type, &flags);
            CFReleaseSafe(nfkey);
            
            // flags is not actually a mask
            if (flags == kCFUserNotificationDefaultResponse)
                pass("OK button pressed");
            else
            if (flags == kCFUserNotificationCancelResponse)
                pass("Cancel button pressed");
            else
            if (flags == kCFUserNotificationAlternateResponse)
                pass("Alternate button pressed");
            else
                pass("Flags: %#llx", flags);

            ok(error == NULL, "SOSCloudKeychainPutObjectsInCloud [error: %@:]", error);
            dispatch_group_leave(work_group);
        });

    pass("Dialog is up for device \"%@\"", our_peer_id);
    printf("Dialog is up\n");
    dispatch_group_wait(work_group, DISPATCH_TIME_FOREVER);
    CFRunLoopRun();                 // Wait for it...
    pass("Exit from run loop");
}

static int kUNTestCount = 5;

int sc_102_cfusernotification(int argc, char *const *argv)
{
    plan_tests(kUNTestCount);
    tests();

	return 0;
}
