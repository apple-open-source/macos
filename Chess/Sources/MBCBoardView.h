/*
	File:		MBCBoardView.h
	Contains:	Displays and manipulates an OpenGL chess board
	Copyright:	© 2002-2003 Apple Computer, Inc. All rights reserved.

	IMPORTANT: This Apple software is supplied to you by Apple Computer,
	Inc.  ("Apple") in consideration of your agreement to the following
	terms, and your use, installation, modification or redistribution of
	this Apple software constitutes acceptance of these terms.  If you do
	not agree with these terms, please do not use, install, modify or
	redistribute this Apple software.
	
	In consideration of your agreement to abide by the following terms,
	and subject to these terms, Apple grants you a personal, non-exclusive
	license, under Apple's copyrights in this original Apple software (the
	"Apple Software"), to use, reproduce, modify and redistribute the
	Apple Software, with or without modifications, in source and/or binary
	forms; provided that if you redistribute the Apple Software in its
	entirety and without modifications, you must retain this notice and
	the following text and disclaimers in all such redistributions of the
	Apple Software.  Neither the name, trademarks, service marks or logos
	of Apple Computer, Inc. may be used to endorse or promote products
	derived from the Apple Software without specific prior written
	permission from Apple.  Except as expressly stated in this notice, no
	other rights or licenses, express or implied, are granted by Apple
	herein, including but not limited to any patent rights that may be
	infringed by your derivative works or by other works in which the
	Apple Software may be incorporated.
	
	The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
	MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
	THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND
	FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS
	USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
	
	IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT,
	INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE,
	REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE,
	HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING
	NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
const float kInHandPieceX 		= 49.0f;
const float kInHandPieceZOffset	=  3.0f;
const float	kInHandPieceSize	=  6.0f;
const float	kPromotionPieceX	= 50.0f;
const float kPromotionPieceZ	= 35.0f;
const float kBoardRadius		= 40.0f;
const float kBorderWidth		=  5.0f;
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
