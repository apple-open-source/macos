/*
	File:		MBCBoardViewTextures.h
	Contains:	Texture handling for OpenGL chess board view.
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCBoardViewTextures.h,v $
		Revision 1.4  2003/06/05 08:31:26  neerache
		Added Tuner
		
		Revision 1.3  2003/06/02 04:21:40  neerache
		Start implementing drawing styles for board elements
		
		Revision 1.2  2002/10/15 22:49:40  neeri
		Add support for texture styles
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
*/
/* MBCBoardViewTextures */

#import "MBCBoardView.h"

@interface MBCBoardView ( Textures )

- (void) loadColors;
- (void) loadStyles;

#ifdef CHESS_TUNER
- (void) savePieceStyles;
- (void) saveBoardStyles;
#endif

@end

// Local Variables:
// mode:ObjC
// End:
