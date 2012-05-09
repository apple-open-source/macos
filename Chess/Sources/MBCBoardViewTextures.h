/*
	File:		MBCBoardViewTextures.h
	Contains:	Texture handling for OpenGL chess board view.
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.
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
