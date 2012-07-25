/*
	File:		MBCMoveGenerator.h
	Contains:	Generate all legal moves from a position
	Copyright:	© 2003 by Apple Inc., all rights reserved.

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

/*
 * An MBCMoveGenerator generates all legal moves from a position (for various
 * variants and various definitions of "legality" and communicates them to 
 * an object of a class derived from MBCMoveBuilder. 
 */
@protocol MBCMoveBuilder 

- (void) startMoveList:(BOOL)white;
- (void) startUnambiguousMoves;
- (void) endMoveList;
- (void) validMove:(MBCPiece)piece from:(MBCSquare)from to:(MBCSquare)to;
- (void) validMove:(MBCPiece)piece from:(MBCSquare)from to:(MBCSquare)to 
	capturing:(MBCPiece)victim;
- (void) validDrop:(MBCPiece)piece at:(MBCSquare)at;
- (void) validCastle:(MBCPiece)king kingSide:(BOOL)kingSide;

@end

//
// An MBCMoveCounter just counts legal moves
//
@interface MBCMoveCounter : NSObject <MBCMoveBuilder> {
	int		fCount;
	bool	fCounting;
}

- (int)count;

@end

//
// An MBCDebugMoveBuilder prints legal moves
//
@interface MBCDebugMoveBuilder : NSObject <MBCMoveBuilder> {
	bool				fUnambiguous;
	NSMutableArray *	fMoves;
	NSMutableArray *	fUnambiguousMoves;
	NSMutableArray *	fDrops;
}

+ (id)debugMoveBuilder;

@end

typedef uint64_t MBCBoardMask;
//
// An MBCPieceMoves collects all legal moves for one piece type
//
struct MBCPieceMoves {
	int				fNumInstances; // Max. 16 (Pawns in crazyhouse)
	MBCSquare		fFrom[16];
	MBCBoardMask	fTo[16];
};
//
// An MBCMoveCollection collects all legal moves
//
struct MBCMoveCollection {
	MBCPieceMoves	fMoves[7];
	MBCPieceMoves	fUnambiguousMoves[7];
	MBCBoardMask	fPawnDrops;
	MBCBoardMask	fPieceDrops;
	char			fDroppablePieces;
	bool			fCastleKingside;
	bool			fCastleQueenside;
	bool			fWhiteMoves;

	void AddMove(bool unambig, MBCPiece piece, MBCSquare from, MBCSquare to);
	void AddDrop(MBCPiece piece, MBCSquare at);
	void AddCastle(bool kingSide);
};

//
// An MBCMoveCollector collects all legal moves in an MBCMoveCollection
//
@interface MBCMoveCollector : NSObject <MBCMoveBuilder> {
	bool				fUnambiguous;
	MBCMoveCollection	fCollection;
};

- (MBCMoveCollection *) collection;

@end

class MBCMoveGenerator {
public:
	MBCMoveGenerator(id <MBCMoveBuilder> builder, MBCVariant variant, long flags);
	void SetVariant(MBCVariant variant);
	void Generate(bool white, const MBCPieces & position);
	void Ambiguities(MBCSquare from, MBCSquare to, const MBCPieces & position);
	bool InCheck(bool white, const MBCPieces & position);
    bool InCheckMate(bool white, const MBCPieces & position);
    bool InStaleMate(bool white, const MBCPieces & position);
private:
	bool	TryMove(MBCPiece piece, MBCSquare from, MBCSquare to);
	bool	TryMove(MBCPiece piece, MBCSquare from, int dCol, int dRow);
	void	TryMoves(MBCPiece piece, MBCSquare from, int dCol, int dRow);
	void	TryMoves(MBCPiece piece, MBCSquare from);
	void    TryCastle();
	void	TryDrops();
	void	TryMoves(bool unambiguous);

	id <MBCMoveBuilder> 	fBuilder;
	long					fFlags;
	MBCVariant				fVariant;
	MBCPieceCode			fColor;
	MBCPiece				fPieceFilter;
	MBCSquare				fTargetFilter;
	const MBCPieces	*		fPosition;
	bool					fUnambiguous;
	uint8_t					fTargetUsed[64];
	uint8_t					fTargetAmbiguous[64];
};

//
// An MBCCheckCounter counts moves that don't leave the king in check
//
@interface MBCCheckCounter : MBCMoveCounter {
    bool                  fWhite;
    bool                  fCanCastle;
    MBCMoveGenerator *    fGenerator;
    MBCPieces             fPosition;
}

- (id)initForWhite:(BOOL)white variant:(MBCVariant)variant position:(const MBCPieces *)pos canCastle:(BOOL)canCastle;

@end

// Local Variables:
// mode:ObjC
// End:
