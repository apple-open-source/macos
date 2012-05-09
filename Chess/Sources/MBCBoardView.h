/*
	File:		MBCBoardView.h
	Contains:	Displays and manipulates an OpenGL chess board
	Version:	1.0
	Copyright:	© 2002-2010 by Apple Computer, Inc., all rights reserved.
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
	char					fKeyBuffer;
	float					fAnisotropy;
	GLint					fNumSamples;
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
