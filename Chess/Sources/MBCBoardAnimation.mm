/*
	File:		MBCBoardAnimation.mm
	Contains:	Animate the board rotating by 180 degrees
	Version:	1.0
	Copyright:	© 2003 by Apple Computer, Inc., all rights reserved.
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
