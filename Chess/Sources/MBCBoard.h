/*
	File:		MBCBoard.h
	Contains:	Fundamental move and board classes.
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

#import "MBCBoardEnums.h"
#import <OpenGL/gl.h>
#import <Cocoa/Cocoa.h>
#import <stdio.h>

extern NSString *   gVariantName[];
extern const char   gVariantChar[];

extern const MBCSide gHumanSide[];
extern const MBCSide gEngineSide[];

//
// MBCMove - A move
//
@interface MBCMove : NSObject
{
@public
    MBCMoveCode		fCommand;		// Command
    MBCSquare		fFromSquare;	// Starting square of piece if move
    MBCSquare		fToSquare;		// Finishing square if move or drop
    MBCPiece		fPiece;			// Moved or dropped piece
    MBCPiece		fPromotion;		// Pawn promotion piece
    MBCPiece		fVictim;		// Captured piece, set by [board makeMove]
    MBCCastling		fCastling;		// Castling move, set by [board makeMove]
    BOOL			fEnPassant;		// En passant, set by [board makeMove]
    BOOL           fCheck;        // Check, set by [board makeMove]
    BOOL           fCheckMate;    // Checkmate, set asynchronously
    BOOL 			fAnimate;		// Animate on board
}

- (id) initWithCommand:(MBCMoveCode)command;
+ (id) newWithCommand:(MBCMoveCode)command;
+ (id) moveWithCommand:(MBCMoveCode)command;
- (id) initFromCompactMove:(MBCCompactMove)move;
+ (id) newFromCompactMove:(MBCCompactMove)move;
+ (id) moveFromCompactMove:(MBCCompactMove)move;
- (id) initFromEngineMove:(NSString *)engineMove;
+ (id) newFromEngineMove:(NSString *)engineMove;
+ (id) moveFromEngineMove:(NSString *)engineMove;
+ (BOOL) compactMoveIsWin:(MBCCompactMove)move;

- (NSString *) localizedText;
- (NSString *) engineMove;
- (NSString *) origin;
- (NSString *) operation;
- (NSString *) destination;
- (NSString *) check;

@end

//
// MBCPieces - The full position representation
//
struct MBCPieces {
	MBCPiece		fBoard[64];
	char			fInHand[16];
	MBCSquare		fEnPassant;	// Current en passant square, if any
    
    bool            NoPieces(MBCPieceCode color);
};

//
// MBCBoard - The game board
//
@interface MBCBoard : NSObject
{
	MBCPieces			fCurPos;
	MBCPieces			fPrvPos;
	int					fMoveClock;
	MBCVariant			fVariant;
	NSMutableArray *	fMoves;
	MBCPiece			fPromotion[2];
    NSMutableArray *    fObservers;
    id                  fDocument;
}

- (void)        removeChessObservers;
- (void) 		setDocument:(id)doc;
- (void)		startGame:(MBCVariant)variant;
- (MBCPiece)	curContents:(MBCSquare)square;	// Read contents of a square
- (MBCPiece)	oldContents:(MBCSquare)square;	// Read contents of a square
- (int)			curInHand:(MBCPiece)piece;		// # of pieces to drop
- (int)			oldInHand:(MBCPiece)piece;		// # of pieces to drop
- (void) 		makeMove:(MBCMove *)move; 		// Move pieces and record
- (MBCCastling) tryCastling:(MBCMove *)move;
- (void)		tryPromotion:(MBCMove *)move;
- (MBCSide)    sideOfMove:(MBCMove *)move;
- (MBCUnique) 	disambiguateMove:(MBCMove *)move;
- (bool) 		undoMoves:(int)numMoves;
- (void) 		commitMove;						// Save position
- (NSString *)	fen;							// Position in FEN notation
- (NSString *)	holding;                        // Pieces held
- (NSString *) 	moves;							// Moves in engine format
- (void)        setFen:(NSString *)fen holding:(NSString *)holding 
				moves:(NSString *)moves;
- (BOOL)		saveMovesTo:(FILE *)f;
- (BOOL) 		canPromote:(MBCSide)side;
- (BOOL)	    canUndo;
- (MBCMove *)   lastMove;
- (int) 		numMoves;
- (MBCMove *)   move:(int)index;
- (MBCPieces *) curPos;
- (MBCPiece)	defaultPromotion:(BOOL)white;
- (void)		setDefaultPromotion:(MBCPiece)piece for:(BOOL)white;
- (MBCMoveCode)outcome;
- (NSString *) stringFromMove:(MBCMove *)move withLocalization:(NSDictionary *)localization;
- (NSString *) extStringFromMove:(MBCMove *)move withLocalization:(NSDictionary *)localization;

@end

NSString * LocalizedString(NSDictionary * localization, NSString * key, NSString * fallback);

#define LOC(key, fallback) LocalizedString(localization, key, fallback)


// Local Variables:
// mode:ObjC
// End:
