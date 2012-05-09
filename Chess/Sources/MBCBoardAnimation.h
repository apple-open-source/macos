/*
	File:		MBCBoardAnimation.h
	Contains:	Animate the board rotating by 180 degrees
	Version:	1.0
	Copyright:	© 2003 by Apple Computer, Inc., all rights reserved.
*/

#import "MBCAnimation.h"
#import "MBCBoardView.h"

@interface MBCBoardAnimation : MBCAnimation {
	float			fFromAzimuth;
	float			fToAzimuth;
	float			fDelta;
}

+ (id) boardAnimation:(MBCBoardView *)view;
            
- (void) step: (float)pctDone;
- (void) endState;

- (void) dealloc;

@end

// Local Variables:
// mode:ObjC
// End:
