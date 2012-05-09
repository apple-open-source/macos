/*
	File:		MBCMoveAnimation.mm
	Contains:	Animate a piece moving on the board
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.
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
