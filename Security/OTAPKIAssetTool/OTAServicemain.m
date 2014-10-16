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

/* ==========================================================================
	main for the OTAService for iOS securityd
   ========================================================================== */

#import <Foundation/Foundation.h>
#import <xpc/private.h>
#import "OTAServiceApp.h"

int main(int argc, const char * argv[])
{
    // OTAServiceApp
    @autoreleasepool
    {		
        OTAServiceApp* ota_service_application = [[OTAServiceApp alloc] init:argc withArguments:argv];
		[ota_service_application checkInWithActivity];
		
		/* Spin a runloop so events can be delivered */
		CFRunLoopRun();
    }
    return 0;
}
        
        
        
