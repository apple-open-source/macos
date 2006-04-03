/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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


#import <Cocoa/Cocoa.h>

int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    
    
    // Check what Mac OS X version the user is running, and load the appropriate nib file. Prober requires
    // Mac OS X version 10.2 at the minimum, but some features require 10.3.
    long result;
    Gestalt( gestaltSystemVersion, &result );
    
    if ( result < 0x1020 ) {
        NSRunCriticalAlertPanel(@"Mac OS X Version error", @"USB Prober requires Mac OS X version 10.2 or newer to run.", @"Okay", nil, nil);
        exit(0);
    } else if ( result < 0x1030 ) {
        [NSBundle loadNibNamed:@"MainMenu-Jaguar" owner:NSApp];
    } else {
        [NSBundle loadNibNamed:@"MainMenu" owner:NSApp];
    }
    
    [NSApp run];
    
    [pool release];
    
    return 0;
    
    //return NSApplicationMain(argc, argv);
}
