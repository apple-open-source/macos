/*
	File:		MBCBoardViewMouse.h
	Contains:	Mouse handling for OpenGL chess board view
	Version:	1.0
	Copyright:	© 2002-2004 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCBoardViewMouse.h,v $
		Revision 1.4  2004/08/16 07:50:55  neerache
		Support accessibility
		
		Revision 1.3  2003/06/02 05:44:48  neerache
		Implement direct board manipulation
		
		Revision 1.2  2003/04/02 19:01:36  neeri
		Explore strategies to speed up dragging
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
*/

#import "MBCBoardView.h"

@interface MBCBoardView ( Mouse )

- (MBCPosition) mouseToPosition:(NSPoint)mouse;
- (void) mouseDown:(NSEvent *)event;
- (void) mouseMoved:(NSEvent *)event;
- (void) mouseUp:(NSEvent *)event;
- (void) dragAndRedraw:(NSEvent *)event forceRedraw:(BOOL)force;

@end

// Local Variables:
// mode:ObjC
// End:
