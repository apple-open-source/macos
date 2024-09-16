/*
	File:		MBCBoardView.h
	Contains:	Displays and manipulates an OpenGL chess board
	Copyright:	© 2003-2024 by Apple Inc., all rights reserved.

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
	of Apple Inc. may be used to endorse or promote products
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
#import "MBCBoardCommon.h"
#import "MBCBoardViewInterface.h"
#import <sys/time.h>

extern MBCPieceCode gInHandOrder[];

@class MBCInteractivePlayer;
@class MBCDrawStyle;
@class MBCBoardWin;

@interface MBCBoardView : NSOpenGLView <MBCBoardViewInterface>
{
    MBCBoardWin *         fController;
    MBCInteractivePlayer *fInteractive;
    MBCBoard *            fBoard;
    MBCSquare             fPickedSquare;
    MBCPiece              fSelectedPiece;
    MBCSquare             fSelectedSquare;
    MBCSquare             fSelectedDest;
    MBCPosition           fSelectedPos;
    MBCPosition           fLastSelectedPos;
    float                 fRawAzimuth;
    NSPoint               fOrigMouse;
    NSPoint               fCurMouse;
    struct timeval		    fLastRedraw;
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
	int						fMaxFSAA;
	int						fCurFSAA;
	int						fLastFSAASize;
	bool					fNeedStaticModels;
	bool					fIsPickingFormat;
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
    NSTrackingArea *       fTrackingArea;
}

//
// Properties and methods to provide access to ChessTuner
//
@property (nonatomic, assign) float azimuth;
@property (nonatomic, assign) float elevation;
@property (nonatomic, assign) float boardReflectivity;
@property (nonatomic, assign) float labelIntensity;
@property (nonatomic, assign) float ambient;

- (MBCDrawStyle *)boardDrawStyleAtIndex:(NSUInteger)index;
- (MBCDrawStyle *)pieceDrawStyleAtIndex:(NSUInteger)index;
- (MBCDrawStyle *)borderDrawStyle;
- (MBCDrawStyle *)selectedPieceDrawStyle;

- (void)setLightPosition:(vector_float3)lightPosition;
- (vector_float3)lightPosition;

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
// Fall back to less memory hungry graphics format
//
- (void) pickPixelFormat:(BOOL)afterFailure;

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
- (void) showMoveAsHint:(MBCMove *)move;
- (void) showMoveAsLast:(MBCMove *)move;
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
