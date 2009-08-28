/*
	File:		MBCAbout.mm
	Contains:	Show the about box
	Version:	1.0
	Copyright:	© 2003 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCAbout.mm,v $
		Revision 1.2  2008/10/24 23:23:26  neerache
		Update small print
		
		Revision 1.1  2003/07/02 21:06:16  neerache
		Move about box into separate class/nib
		
*/

#import "MBCAbout.h"

@implementation MBCAbout

- (id)init
{
	self = [super initWithWindowNibName:@"About"];

	return self;
}

- (void)windowDidLoad
{
	[fVersion 
		setStringValue:
			[NSString stringWithFormat:[fVersion stringValue],
					  [[NSBundle mainBundle] 
						  objectForInfoDictionaryKey:@"CFBundleVersion"]]];

	[fLicense insertText:
				  [NSString stringWithContentsOfFile:
								[[NSBundle mainBundle] 
									pathForResource: @"COPYING" ofType: nil]]];
	[fLicense setEditable:NO];
	[fLicense scrollRangeToVisible:NSMakeRange(0,0)];
}

- (IBAction) downloadSource:(id)sender
{
	NSURL * url = 
		[NSURL 
			URLWithString:@"http://developer.apple.com/darwin/projects/misc/"];
	[[NSWorkspace sharedWorkspace] openURL:url];
}				

- (NSFont *) licenseFont
{
	return [NSFont userFixedPitchFontOfSize:10.0];
}

@end

// Local Variables:
// mode:ObjC
// End:
