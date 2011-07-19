/*
	File:		main.m
	Contains:	Chess main program
	Version:	1.0
	Copyright:	© 2002-2010 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: main.m,v $
		Revision 1.6  2010/10/08 00:15:23  neerache
		Need autorelease pool while registering defaults
		
		Revision 1.5  2010/10/07 23:07:02  neerache
		<rdar://problem/8352405> [Chess]: Ab-11A250: BIDI: RTL: Incorrect alignement for strings in cells in Came log
		
		Revision 1.4  2010/01/18 18:37:16  neerache
		<rdar://problem/7297328> Deprecated methods in Chess, part 1
		
		Revision 1.3  2003/10/29 22:39:31  neerache
		Add tools & clean up copyright references for release
		
*/

#import <Cocoa/Cocoa.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>

int main(int argc, const char *argv[])
{
	while (argv[1])
		putenv((char *)*++argv);
	const char * debug = getenv("MBC_DEBUG");
	if (debug && atoi(debug) & 4)
		NSLog(@"Chess starting\n");
	//
	// We set defaults that influence NSApplication init, so we need to run now
	//
	NSAutoreleasePool * autoreleasePool = [[NSAutoreleasePool alloc] init];
	NSDictionary * defaults = 
	[NSDictionary dictionaryWithContentsOfFile:
	 [[NSBundle mainBundle] 
	  pathForResource:@"Defaults" ofType:@"plist"]];
	[[NSUserDefaults standardUserDefaults] registerDefaults: defaults];
	[[NSUserDefaults standardUserDefaults] synchronize];
	[autoreleasePool drain];
    return NSApplicationMain(argc, argv);
}
