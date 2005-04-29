/*
	File:		main.m
	Contains:	Chess main program
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: main.m,v $
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
		putenv(*++argv);
	const char * debug = getenv("MBC_DEBUG");
	if (debug && atoi(debug) & 4)
		NSLog(@"Chess starting\n");
    return NSApplicationMain(argc, argv);
}
