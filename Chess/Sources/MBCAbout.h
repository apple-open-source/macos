/*
	File:		MBCAbout.h
	Contains:	Show the about box
	Version:	1.0
	Copyright:	© 2003 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCAbout.h,v $
		Revision 1.1  2003/07/02 21:06:16  neerache
		Move about box into separate class/nib
		
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
