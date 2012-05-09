/*
	File:		MBCAnimation.mm
	Contains:	General animation infrastructure.
	Version:	1.0
	Copyright:	© 2002-2010 by Apple Computer, Inc., all rights reserved.
*/

#import "MBCAnimation.h"
#import "MBCBoardView.h"

#include <algorithm>

using std::min;

@implementation MBCAnimation

static id	sCurAnimation = nil;

+ (void) cancelCurrentAnimation
{
	if (sCurAnimation) {
		[sCurAnimation endState];
		sCurAnimation = nil;
	}
}

- (void) scheduleNextStep
{
	[self performSelector:@selector(doStep:) withObject:nil afterDelay:0.005];
}

- (void) startState 		
{
	sCurAnimation = self;
	[fView startAnimation];
}

- (void) step: (float)pctDone	{}

- (void) endState			
{
	[fView animationDone];
	sCurAnimation = nil;
}

- (void) doStep:(id)arg
{
	struct timeval now;

	gettimeofday(&now, NULL);
	float	elapsedTime			= 
		now.tv_sec - fStart.tv_sec 
		+ 0.000001f * (now.tv_usec - fStart.tv_usec);
	float	elapsed				= min(elapsedTime/fTime, 1.0f);
	//
	// Prevent excessive jerks on slow hardware
	//
	if (elapsed-fLastElapsed > 0.5f)
		elapsed = 1.0f;
	[self step:elapsed];
	fLastElapsed = elapsed;
	if (elapsed >= 1.0f) {
		[self endState];
		[self autorelease];
		[fView setNeedsDisplay:YES];
	} else {
		[self scheduleNextStep];
		[fView drawNow];
	}
}

- (void) runWithTime:(float)seconds view:(MBCBoardView *)view
{
	gettimeofday(&fStart, NULL);
	fTime			= seconds;
	fView			= view;
	fLastElapsed	= 0.0f;
    [self startState];
	[self doStep:nil];
}

@end

// Local Variables:
// mode:ObjC
// End:
