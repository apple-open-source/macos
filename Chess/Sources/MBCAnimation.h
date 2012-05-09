/*
	File:		MBCAnimation.h
	Contains:	Basic infrastructure for animations.
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.
*/

#import <Foundation/Foundation.h>

#import <sys/time.h>

@class MBCBoardView;

@interface MBCAnimation : NSObject {
	struct timeval	fStart;
	float			fTime;
	float			fLastElapsed;
@protected
	MBCBoardView *	fView;
}

- (void) runWithTime:(float)seconds view:(MBCBoardView *)view;

+ (void) cancelCurrentAnimation;
 
@end

@interface MBCAnimation ( Clients )

- (void) startState;
- (void) step: (float)pctDone;
- (void) endState;

@end

// Local Variables:
// mode:ObjC
// End:
