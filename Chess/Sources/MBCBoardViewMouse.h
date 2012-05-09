/*
	File:		MBCBoardViewMouse.h
	Contains:	Mouse handling for OpenGL chess board view
	Version:	1.0
	Copyright:	© 2002-2010 by Apple Computer, Inc., all rights reserved.
*/

#import "MBCBoardView.h"

@interface MBCBoardView ( Mouse )

- (NSRect) approximateBoundsOfSquare:(MBCSquare)square;
- (MBCPosition) mouseToPosition:(NSPoint)mouse;
- (void) mouseDown:(NSEvent *)event;
- (void) mouseMoved:(NSEvent *)event;
- (void) mouseUp:(NSEvent *)event;
- (void) dragAndRedraw:(NSEvent *)event forceRedraw:(BOOL)force;

@end

// Local Variables:
// mode:ObjC
// End:
