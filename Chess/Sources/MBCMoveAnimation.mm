/*
	File:		MBCMoveAnimation.mm
	Contains:	Animate a piece moving on the board
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCMoveAnimation.mm,v $
		Revision 1.4  2003/06/15 19:03:50  neerache
		Smoother animations
		
		Revision 1.3  2003/05/24 20:28:27  neerache
		Address race conditions between ploayer and engine
		
		Revision 1.2  2003/05/05 21:23:53  neerache
		Add board animation, revise move animation
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
*/

#import "MBCMoveAnimation.h"
#import "MBCPlayer.h"

#include <math.h>

@implementation MBCMoveAnimation

+ (id) moveAnimation:(MBCMove *)move board:(MBCBoard *)board view:(MBCBoardView *)view 
{
    MBCMoveAnimation * a = [[MBCMoveAnimation alloc] init];
    a->fMove	= [move retain];
    a->fPiece	= move->fCommand == kCmdDrop 
		? move->fPiece : [board oldContents:move->fFromSquare];

    [a runWithTime:1.0 view:view];
    
    return self;
}

- (void) startState
{
	[super startState];

	if (fMove->fCommand == kCmdDrop)
		fFrom = [fView squareToPosition:fMove->fPiece+kInHandSquare];
	else
		fFrom	= [fView squareToPosition:fMove->fFromSquare];
    fDelta	= [fView squareToPosition:fMove->fToSquare] - fFrom;

	[fView selectPiece:fPiece at:fMove->fFromSquare to:fMove->fToSquare];
	[fView moveSelectionTo:&fFrom];
}
            
- (void) step: (float)pctDone
{
	MBCPosition	pos = fFrom;
	pos[0] 		   += pctDone*fDelta[0];
	pos[2] 		   += pctDone*fDelta[2];

	[fView moveSelectionTo:&pos];
}

- (void) endState
{
	[fView unselectPiece];
	[[NSNotificationQueue defaultQueue] 
		enqueueNotification:
			[NSNotification 
				notificationWithName:MBCEndMoveNotification
				object:fMove]
		postingStyle: NSPostWhenIdle];
	[super endState];
}

- (void) dealloc
{
	[fMove release];
	[super dealloc];
}

@end

// Local Variables:
// mode:ObjC
// End:
