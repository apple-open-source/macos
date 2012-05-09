/*
	File:		MBCAbout.mm
	Contains:	Show the about box
	Version:	1.0
	Copyright:	© 2003-2010 by Apple Computer, Inc., all rights reserved.
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
