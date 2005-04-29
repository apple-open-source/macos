/*
	File:		MBCTuner.h
	Contains:	Manage a window to set graphics options
	Version:	1.0
	Copyright:	© 2003 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCTuner.mm,v $
		Revision 1.3  2004/07/10 04:53:29  neerache
		Tweak visuals
		
		Revision 1.2  2003/06/15 19:05:29  neerache
		Add light experiment window
		
		Revision 1.1  2003/06/05 08:31:26  neerache
		Added Tuner
		
*/

#import "MBCTuner.h"
#import "MBCBoardView.h"
#import "MBCBoardViewDraw.h"
#import "MBCBoardViewTextures.h"
#import "MBCController.h"

static MBCTuner *	sTuner;

@implementation MBCTunerStyle

- (void) updateFrom:(MBCDrawStyle *)drawStyle
{
	[fDiffuse setFloatValue:drawStyle->fDiffuse];
	[fSpecular setFloatValue:drawStyle->fSpecular];
	[fShininess setFloatValue:drawStyle->fShininess];
	[fAlpha setFloatValue:drawStyle->fAlpha];
}

- (void) updateTo:(MBCDrawStyle *)drawStyle
{
	drawStyle->fDiffuse		= [fDiffuse floatValue];
	drawStyle->fSpecular	= [fSpecular floatValue];
	drawStyle->fShininess	= [fShininess floatValue];
	drawStyle->fAlpha		= [fAlpha floatValue];
}

@end

@implementation MBCTuner 

+ (void) makeTuner
{
	//
	// Chess Tuner is intended to be run from inside the build directory
	// We create a styles link blindly
	//
	NSString * bndl	= [[NSBundle mainBundle] bundlePath]; 
	NSString * path = [bndl stringByDeletingLastPathComponent]; // .../build
	path			= [path stringByDeletingLastPathComponent]; // ...
	path			= [path stringByAppendingPathComponent:@"Styles"];
	bndl			= [bndl stringByAppendingPathComponent:
								@"Contents/Resources/Styles"];
	[[NSFileManager defaultManager] createSymbolicLinkAtPath:bndl
									pathContent:path];
										
	sTuner = [[MBCTuner alloc] init];
}

- (void) loadStyles
{
	fView	= [[MBCController controller] view];
	[fWhitePieceStyle updateFrom:fView->fPieceDrawStyle[0]];
	[fBlackPieceStyle updateFrom:fView->fPieceDrawStyle[1]];
	[fWhiteBoardStyle updateFrom:fView->fBoardDrawStyle[0]];
	[fBlackBoardStyle updateFrom:fView->fBoardDrawStyle[1]];
	[fBorderStyle updateFrom:fView->fBorderDrawStyle];
	[fBoardReflectivity setFloatValue:fView->fBoardReflectivity];
	[fLabelIntensity setFloatValue:fView->fLabelIntensity];
	[fLightPosX	setFloatValue:fView->fLightPos[0]];
	[fLightPosY	setFloatValue:fView->fLightPos[1]];
	[fLightPosZ	setFloatValue:fView->fLightPos[2]];
	[fAmbient setFloatValue:fView->fAmbient];
}

+ (void) loadStyles
{
	[sTuner loadStyles];
}

+ (void) saveStyles
{
}

- (id) init
{
	self = [super initWithWindowNibName:@"Tuner"];
	[[self window] orderFront:self];
	return self;
}

- (IBAction) updateWhitePieceStyle:(id)sender
{
	[fWhitePieceStyle updateTo:fView->fPieceDrawStyle[0]];
	[fView setNeedsDisplay:YES];
}

- (IBAction) updateBlackPieceStyle:(id)sender
{
	[fBlackPieceStyle updateTo:fView->fPieceDrawStyle[1]];
	[fView setNeedsDisplay:YES];
}

- (IBAction) updateWhiteBoardStyle:(id)sender
{
	[fWhiteBoardStyle updateTo:fView->fBoardDrawStyle[0]];
	[fView setNeedsDisplay:YES];
}

- (IBAction) updateBlackBoardStyle:(id)sender
{
	[fBlackBoardStyle updateTo:fView->fBoardDrawStyle[1]];
	[fView setNeedsDisplay:YES];
}

- (IBAction) updateBoardStyle:(id)sender
{
	[fBorderStyle updateTo:fView->fBorderDrawStyle];	
	fView->fBoardReflectivity	= [fBoardReflectivity floatValue];
	fView->fLabelIntensity		= [fLabelIntensity floatValue];
	[fView setNeedsDisplay:YES];
}

- (IBAction) savePieceStyles:(id)sender
{
	[fView savePieceStyles];
}

- (IBAction) saveBoardStyles:(id)sender
{
	[fView saveBoardStyles];
}

static const char * sLightParams =
 "\tfloat   light_ambient		= %5.3ff\n"
 "\tGLfloat light_pos[4] 		= {%4.1ff, %4.1ff, %4.1ff, 1.0};\n";

- (IBAction) updateLight:(id)sender
{
	fView->fLightPos[0]	= [fLightPosX floatValue];
	fView->fLightPos[1]	= [fLightPosY floatValue];
	fView->fLightPos[2]	= [fLightPosZ floatValue];
	fView->fAmbient		= [fAmbient floatValue];
	[fView setNeedsDisplay:YES];
	[fLightParams setStringValue:
					  [NSString stringWithFormat:
									[NSString stringWithCString:sLightParams],
								fView->fAmbient,
								fView->fLightPos[0],
								fView->fLightPos[1],
								fView->fLightPos[2]]];
}

@end

// Local Variables:
// mode:ObjC
// End:
