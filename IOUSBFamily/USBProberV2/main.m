/*
 * Copyright © 1998-2012 Apple Inc.  All rights reserved.
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
	
	if ( ( [args count] > 2 ) && (!busProbe && !showHelp) )
	{
		showHelp = true;
	}

	if ( !busProbe && !showHelp )
	{
		[NSApplication sharedApplication];
        [NSBundle loadNibNamed:@"MainMenu" owner:NSApp];
		
		[NSApp run];
	}
	else
	{
	    BusProbeController *ctr = [[BusProbeController alloc] init];
		[ctr dumpToTerminal:args showHelp:showHelp];
	}
	
    [pool release];
    
    return 0;
    
    //return NSApplicationMain(argc, argv);
}
