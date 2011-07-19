/*
	File:		MBCAbout.mm
	Contains:	Show the about box
	Version:	1.0
	Copyright:	© 2003-2010 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCAbout.mm,v $
		Revision 1.5  2010/12/12 20:25:33  neerache
		<rdar://problem/8759896> "Download Source Code" button links to defunct page
		
		Revision 1.4  2010/12/11 00:49:09  neerache
		<rdar://problem/8672916> [Chess]:AB:11A316: Incorrect alignment for about window
		
		Revision 1.3  2010/01/18 18:37:16  neerache
		<rdar://problem/7297328> Deprecated methods in Chess, part 1
		
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

	[fLicense setString:
				  [NSString stringWithContentsOfURL:
								[[NSBundle mainBundle] 
									URLForResource: @"COPYING" withExtension:nil]
										   encoding:NSUTF8StringEncoding
											  error:nil]];
	[fLicense alignLeft:self];
	[fLicense setEditable:NO];
	[fLicense scrollRangeToVisible:NSMakeRange(0,0)];
}

- (IBAction) downloadSource:(id)sender
{
	NSURL * url = 
		[NSURL 
			URLWithString:@"http://www.opensource.apple.com/source/Chess/"];
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
