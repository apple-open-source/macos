/*
	File:		MBCMoveAnimation.h
	Contains:	Animate a piece moving on the board
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCMoveAnimation.h,v $
		Revision 1.3  2003/06/15 19:03:50  neerache
		Smoother animations
		
		Revision 1.2  2003/05/05 21:23:53  neerache
		Add board animation, revise move animation
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
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
