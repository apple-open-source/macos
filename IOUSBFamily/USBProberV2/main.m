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
#import "BusProbeController.h"

int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	NSArray *args = [[NSProcessInfo processInfo] arguments];	

	NSEnumerator *enm = [args objectEnumerator];
    id word;
	bool busProbe = false; bool showHelp = false;
	
    while (word = [enm nextObject]) 
	{
		if ( [word caseInsensitiveCompare:[NSString stringWithUTF8String:"--busprobe"]] == NSOrderedSame )
		{
			busProbe = true;
		}
		else if ( [word caseInsensitiveCompare:[NSString stringWithUTF8String:"--help"]] == NSOrderedSame )
		{
			showHelp = true;
		}
    }
	
	if( ( [args count] > 2 ) && (!busProbe && !showHelp) )
	{
		showHelp = true;
	}

	if ( !busProbe && !showHelp )
	{
		[NSApplication sharedApplication];
		
		
		SInt32 result;
		Gestalt( gestaltSystemVersion, &result );
		
	#if defined(MAC_OS_X_VERSION_10_5) 
		// Prevent executables built under Leopard from running
		// on pre-Leopard systems. If we don't, the app will eventually
		// crash because it'll try to load some IOKit stuff newly
		// introduced in (and only avaialble on) Leopard.
		// We don't really need to strictly enforce the requirement that
		// versions of USB Prober built under 10.5 can only run on machines
		// running versions 10.5 of the OS or later - but the price of allowing
		// that degree of backward compatibility is compiler warnings at
		// build time, complaining about the use of a deprecated IOKit API.
		if ( result < 0x1050 ) {
		  NSRunCriticalAlertPanel(@"Mac OS X Version error", @"This version of USB Prober was built under Mac OS X version 10.5, and so it requires Mac OS X version 10.5 or newer to run.", @"Okay", nil, nil);
			exit(0);
		} else {
			[NSBundle loadNibNamed:@"MainMenu" owner:NSApp];
		}
	#else
		// Check what Mac OS X version the user is running, and load the appropriate nib file. Prober requires
		// Mac OS X version 10.2 at the minimum, but some features require 10.3.
		if ( result < 0x1020 ) {
			NSRunCriticalAlertPanel(@"Mac OS X Version error", @"USB Prober requires Mac OS X version 10.2 or newer to run.", @"Okay", nil, nil);
			exit(0);
		} else if ( result < 0x1030 ) {
			[NSBundle loadNibNamed:@"MainMenu-Jaguar" owner:NSApp];
		} else {
			[NSBundle loadNibNamed:@"MainMenu" owner:NSApp];
		}
	#endif // MAC_OS_X_VERSION_10_5
		
		[NSApp run];
	}
	else
	{
	    BusProbeController *ctr = [[BusProbeController alloc] init];
		[ctr dumpToTerminal:args :showHelp];
		[ctr release];
	}
	
    [pool release];
    
    return 0;
    
    //return NSApplicationMain(argc, argv);
}
