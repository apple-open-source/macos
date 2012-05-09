/*
  File:		MBCBoardViewDraw.h
  Contains:	Accessibility navigation for chess board
  Version:	1.0
  Copyright:	© 2004 by Apple Computer, Inc., all rights reserved.
*/

#import "MBCBoardView.h"

@interface MBCBoardAccessibilityProxy : NSObject
{
	MBCBoardView *	fView;
	MBCSquare		fSquare;
}

+ (id) proxyWithView:(MBCBoardView *)view square:(MBCSquare)square;
- (id) initWithView:(MBCBoardView *)view square:(MBCSquare)square;

@end

@interface MBCBoardView (Accessibility)

- (NSString *) describeSquare:(MBCSquare)square;
- (void) selectSquare:(MBCSquare)square;

@end

// Local Variables:
// mode:ObjC
// End:
