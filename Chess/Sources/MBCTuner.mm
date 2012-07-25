/*
	File:		MBCTuner.h
	Contains:	Manage a window to set graphics options
	Copyright:	© 2003-2010 by Apple Inc., all rights reserved.

	IMPORTANT: This Apple software is supplied to you by Apple Computer,
	Inc.  ("Apple") in consideration of your agreement to the following
	terms, and your use, installation, modification or redistribution of
	this Apple software constitutes acceptance of these terms.  If you do
	not agree with these terms, please do not use, install, modify or
	redistribute this Apple software.
	
	In consideration of your agreement to abide by the following terms,
	and subject to these terms, Apple grants you a personal, non-exclusive
	license, under Apple's copyrights in this original Apple software (the
	"Apple Software"), to use, reproduce, modify and redistribute the
	Apple Software, with or without modifications, in source and/or binary
	forms; provided that if you redistribute the Apple Software in its
	entirety and without modifications, you must retain this notice and
	the following text and disclaimers in all such redistributions of the
	Apple Software.  Neither the name, trademarks, service marks or logos
	of Apple Inc. may be used to endorse or promote products
	derived from the Apple Software without specific prior written
	permission from Apple.  Except as expressly stated in this notice, no
	other rights or licenses, express or implied, are granted by Apple
	herein, including but not limited to any patent rights that may be
	infringed by your derivative works or by other works in which the
	Apple Software may be incorporated.
	
	The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
	MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
	THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND
	FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS
	USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
	
	IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT,
	INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE,
	REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE,
	HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING
	NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
									[NSString stringWithUTF8String:sLightParams],
								fView->fAmbient,
								fView->fLightPos[0],
								fView->fLightPos[1],
								fView->fLightPos[2]]];
}

@end

// Local Variables:
// mode:ObjC
// End:
