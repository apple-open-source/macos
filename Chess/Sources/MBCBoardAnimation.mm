/*
	File:		MBCBoardAnimation.mm
	Contains:	Animate the board rotating by 180 degrees
	Version:	1.0
	Copyright:	© 2003 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCBoardAnimation.mm,v $
		Revision 1.3  2003/06/15 19:03:50  neerache
		Smoother animations
		
		Revision 1.2  2003/06/02 05:44:48  neerache
		Implement direct board manipulation
		
		Revision 1.1  2003/05/05 21:23:53  neerache
		Add board animation, revise move animation
		
*/
#import "MBCBoardAnimation.h"

#include <math.h>

@implementation MBCBoardAnimation

+ (id) boardAnimation:(MBCBoardView *)view 
{
    MBCBoardAnimation * a = [[MBCBoardAnimation alloc] init];

	a->fFromAzimuth	= view->fAzimuth;
	a->fToAzimuth	= fmod(a->fFromAzimuth + 180.0f, 360.0f);
	a->fDelta		= 180.0f;
    [a runWithTime:2.0 view:view];
    
    return self;
}
            
- (void) step: (float)pctDone
{
	fView->fAzimuth = fFromAzimuth+fDelta*pctDone;
	[fView needsUpdate];
}

- (void) endState
{
	fView->fAzimuth = fToAzimuth;
	[super endState];
}

- (void) dealloc
{
	[super dealloc];
}

@end

// Local Variables:
// mode:ObjC
// End:
