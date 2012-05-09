/*
	File:		main.m
	Contains:	Chess main program
	Version:	1.0
	Copyright:	© 2002-2010 by Apple Computer, Inc., all rights reserved.
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
