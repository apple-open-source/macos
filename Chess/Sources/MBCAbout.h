/*
	File:		MBCAbout.h
	Contains:	Show the about box
	Version:	1.0
	Copyright:	© 2003 by Apple Computer, Inc., all rights reserved.
*/

#import <Cocoa/Cocoa.h>

@interface MBCAbout : NSWindowController
{
	IBOutlet id		fLicense;
	IBOutlet id		fVersion;
}

- (IBAction) downloadSource:(id)sender;

@end

// Local Variables:
// mode:ObjC
// End:
