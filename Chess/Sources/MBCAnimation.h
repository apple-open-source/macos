/*
	File:		MBCAnimation.h
	Contains:	Basic infrastructure for animations.
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCAnimation.h,v $
		Revision 1.3  2003/07/07 08:39:59  neerache
		Tweak for slow hardware
		
		Revision 1.2  2003/06/15 19:03:50  neerache
		Smoother animations
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
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
