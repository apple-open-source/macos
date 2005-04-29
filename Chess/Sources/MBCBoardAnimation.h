/*
	File:		MBCBoardAnimation.h
	Contains:	Animate the board rotating by 180 degrees
	Version:	1.0
	Copyright:	© 2003 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCBoardAnimation.h,v $
		Revision 1.2  2003/06/15 19:03:50  neerache
		Smoother animations
		
		Revision 1.1  2003/05/05 21:23:53  neerache
		Add board animation, revise move animation
		
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
