/*
	File:		MBCMoveAnimation.h
	Contains:	Animate a piece moving on the board
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.
*/

#import "MBCAnimation.h"
#import "MBCBoardView.h"

@interface MBCMoveAnimation : MBCAnimation {
	MBCPiece		fPiece;
	MBCMove *		fMove;
	MBCPosition		fFrom;
	MBCPosition		fDelta;
}

+ (id) moveAnimation:(MBCMove *)move board:(MBCBoard *)board view:(MBCBoardView *)view;
            
- (void) step: (float)pctDone;
- (void) endState;

- (void) dealloc;

@end

// Local Variables:
// mode:ObjC
// End:
