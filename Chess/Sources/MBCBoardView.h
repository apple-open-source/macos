/*
	File:		MBCBoardView.h
	Contains:	Displays and manipulates an OpenGL chess board
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCBoardView.h,v $
		Revision 1.27  2004/08/16 07:50:55  neerache
		Support accessibility
		
		Revision 1.26  2004/07/10 04:53:29  neerache
		Tweak visuals
		
		Revision 1.25  2003/07/17 23:30:07  neerache
		Don't need CenterOfGravity info any longer
		
		Revision 1.24  2003/07/14 23:21:49  neerache
		Move promotion defaults into MBCBoard
		
		Revision 1.23  2003/07/07 23:50:42  neerache
		Work around a graphics bug
		
		Revision 1.22  2003/07/07 08:47:53  neerache
		Switch to textured main window
		
		Revision 1.21  2003/06/30 05:16:30  neerache
		Transfer move execution to Controller
		
		Revision 1.20  2003/06/15 21:13:09  neerache
		Adjust lights, fix animation, work on other drawing issues
		
		Revision 1.19  2003/06/05 08:31:26  neerache
		Added Tuner
		
		Revision 1.18  2003/06/04 23:14:05  neerache
		Neater manipulation widget; remove obsolete graphics options
		
		Revision 1.17  2003/06/04 09:25:47  neerache
		New and improved board manipulation metaphor
		
		Revision 1.16  2003/06/02 05:44:48  neerache
		Implement direct board manipulation
		
		Revision 1.15  2003/06/02 04:21:40  neerache
		Start implementing drawing styles for board elements
		
		Revision 1.14  2003/05/23 03:22:16  neerache
		Add FPS computation
		
		Revision 1.13  2003/05/05 23:50:40  neerache
		Tweak appearance, add border, add animations
		
		Revision 1.12  2003/05/02 01:16:33  neerache
		Simplify drawing methods
		
		Revision 1.11  2003/04/28 22:13:25  neerache
		Eliminate drawBoardPlane
		
		Revision 1.10  2003/04/25 22:26:23  neerache
		Simplify mouse model, fix startup bug
		
		Revision 1.9  2003/04/24 23:20:35  neeri
		Support pawn promotions
		
		Revision 1.8  2003/04/10 23:03:16  neeri
		Load positions
		
		Revision 1.7  2003/03/28 01:31:07  neeri
		Support hints, last move
		
		Revision 1.6  2002/12/04 02:30:50  neeri
		Experiment (unsuccessfully so far) with ways to speed up piece movement
		
		Revision 1.5  2002/10/15 22:49:39  neeri
		Add support for texture styles
		
		Revision 1.4  2002/10/08 23:02:54  neeri
		Rotated board, changeable colors
		
		Revision 1.3  2002/09/13 23:57:05  neeri
		Support for Crazyhouse display and mouse
		
		Revision 1.2  2002/08/26 23:11:17  neeri
		Switched to Azimuth/Elevation based Camera positioning model
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
*/

#import "MBCBoard.h"
#import <sys/time.h>

struct MBCColor {
    GLfloat	color[4];

	operator GLfloat *() { return color; }

	NSColor *	GetColor() const;
	void		SetColor(NSColor * newColor);
};

struct MBCPosition {
    GLfloat 	pos[3];

	operator GLfloat *() 				{ return pos; 		}
	GLfloat & operator[](int i) 		{ return pos[i];	}
	GLfloat   operator[](int i) const 	{ return pos[i];	}
	float     FlatDistance(const MBCPosition & other) 
		{
			return hypot(pos[0]-other[0], pos[2]-other[2]);
		}
};

MBCPosition operator-(const MBCPosition & a, const MBCPosition & b);

extern MBCPieceCode gInHandOrder[];
const float kInHandPieceX 		= 51.0f;
const float kInHandPieceZOffset	=  3.0f;
const float	kInHandPieceSize	=  8.0f;
const float	kPromotionPieceX	= 50.0f;
const float kPromotionPieceZ	= 35.0f;
const float kBoardRadius		= 40.0f;
const float kBorderWidth		=  6.25f;
const float kMinElevation		= 10.0f;
const float kMaxElevation		= 80.0f;

@class MBCController;
@class MBCInteractivePlayer;
@class MBCDrawStyle;

@interface MBCBoardView : NSOpenGLView
{
	MBCController *			fController;
	MBCInteractivePlayer *	fInteractive;
    MBCBoard *  			fBoard;
	MBCSquare				fPickedSquare;
	MBCPiece				fSelectedPiece;
	MBCSquare				fSelectedSquare;
	MBCSquare				fSelectedDest;
	MBCPosition				fSelectedPos;
	MBCPosition				fSelectedStartPos;
	MBCPosition				fLastSelectedPos;
	float					fRawAzimuth;
	NSPoint					fOrigMouse;
	NSPoint					fCurMouse;
	struct timeval			fLastRedraw;
@public
	float					fAzimuth;
	float					fElevation;
	float					fBoardReflectivity;
	float					fLabelIntensity;
	float					fAmbient;
	MBCDrawStyle	*		fBoardDrawStyle[2];
	MBCDrawStyle	*		fPieceDrawStyle[2];
	MBCDrawStyle 	*		fBorderDrawStyle;
	MBCDrawStyle 	*		fSelectedPieceDrawStyle;
	GLfloat					fLightPos[4];
@private
	GLuint					fNumberTextures[8];
	GLuint					fLetterTextures[8];
	bool					fHasFSAA;
	bool					fIsFloating;
	bool					fWantMouse;
	bool					fNeedPerspective;
	bool					fInAnimation;
	bool					fInBoardManipulation;
	MBCVariant				fVariant;
	MBCSide					fSide;
	MBCSide					fPromotionSide;
	NSDictionary *			fBoardAttr;
	NSDictionary *			fPieceAttr;
	NSString *				fBoardStyle;
	NSString *				fPieceStyle;
	MBCMove	*				fHintMove;
	MBCMove	*				fLastMove;
	NSCursor *				fHandCursor;
	NSCursor *				fArrowCursor;
	MBCPiece				fLastPieceDrawn;
}

//
// Basic view routines
//
- (id) initWithFrame:(NSRect)rect;
- (void) awakeFromNib;
- (void) drawRect:(NSRect)rect;

- (void) startGame:(MBCVariant)variant playing:(MBCSide) side;
- (void) drawNow;			// Redraw immediately
- (void) profileDraw;		// Redraw in a tight loop
- (void) needsUpdate;		// Perspective changed
- (void) endGame;     		// Clean up the previous game
- (void) startAnimation;	// Start animation
- (void) animationDone;		// Animation finished

//
// Change textures
//
- (void) setStyleForBoard:(NSString *)boardStyle pieces:(NSString *)pieceStyle;

//
// Selection manipulation
//
- (void) selectPiece:(MBCPiece)piece at:(MBCSquare)square;
- (void) selectPiece:(MBCPiece)piece at:(MBCSquare)square to:(MBCSquare)dest;
- (void) moveSelectionTo:(MBCPosition *)position;
- (void) unselectPiece;
- (void) clickPiece;

//
// Show hints and last moves
//
- (void) showHint:(MBCMove *)move;
- (void) showLastMove:(MBCMove *)move;
- (void) hideMoves;

//
// Translate between squares and positions
//
- (MBCSquare) 	positionToSquare:(const MBCPosition *)position;
- (MBCSquare) 	positionToSquareOrRegion:(const MBCPosition *)position;
- (MBCPosition)	squareToPosition:(MBCSquare)square;
- (NSRect)	    approximateBoundsOfSquare:(MBCSquare)square;
- (void) snapToSquare:(MBCPosition *)position;
- (MBCSide) 	facing;			// What player are we facing?
- (BOOL) 		facingWhite;	// Are we facing white?

//
// Pass mouse clicks on to interactive player?
//
- (void) wantMouse:(BOOL)wantIt;

@end

// Local Variables:
// mode:ObjC
// End:
