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

		$Log: MBCTuner.h,v $
		Revision 1.3  2004/07/10 04:53:29  neerache
		Tweak visuals
		
		Revision 1.2  2003/06/15 19:05:29  neerache
		Add light experiment window
		
		Revision 1.1  2003/06/05 08:31:26  neerache
		Added Tuner
		
*/

#import <Cocoa/Cocoa.h>

@class MBCDrawStyle;
@class MBCBoardView;

@interface MBCTunerStyle : NSObject {
	IBOutlet id fDiffuse;
	IBOutlet id	fSpecular;
	IBOutlet id fShininess;
	IBOutlet id fAlpha;
};

- (void) updateFrom:(MBCDrawStyle *)drawStyle;
- (void) updateTo:(MBCDrawStyle *)drawStyle;

@end

@interface MBCTuner : NSWindowController
{
	MBCBoardView *				fView;
	IBOutlet MBCTunerStyle *	fWhitePieceStyle;
	IBOutlet MBCTunerStyle * 	fBlackPieceStyle;
	IBOutlet MBCTunerStyle * 	fWhiteBoardStyle;
	IBOutlet MBCTunerStyle * 	fBlackBoardStyle;
	IBOutlet MBCTunerStyle * 	fBorderStyle;
	IBOutlet id					fBoardReflectivity;
	IBOutlet id					fLabelIntensity;
	IBOutlet id					fLightPosX;
	IBOutlet id					fLightPosY;
	IBOutlet id					fLightPosZ;
	IBOutlet id					fAmbient;
	IBOutlet id					fLightParams;
}

+ (void) makeTuner;
+ (void) loadStyles;

- (IBAction) updateWhitePieceStyle:(id)sender;
- (IBAction) updateBlackPieceStyle:(id)sender;
- (IBAction) updateWhiteBoardStyle:(id)sender;
- (IBAction) updateBlackBoardStyle:(id)sender;
- (IBAction) updateBoardStyle:(id)sender;
- (IBAction) savePieceStyles:(id)sender;
- (IBAction) saveBoardStyles:(id)sender;
- (IBAction) updateLight:(id)sender;

@end

// Local Variables:
// mode:ObjC
// End:
