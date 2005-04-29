/*
	File:		MBCMoveGenerator.h
	Contains:	Generate all legal moves from a position
	Version:	1.0
	Copyright:	© 2003 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCMoveGenerator.h,v $
		Revision 1.4  2003/07/14 23:19:56  neerache
		Record color of generated moves
		
		Revision 1.3  2003/07/02 21:00:07  neerache
		Added MBCMoveCollection
		
		Revision 1.2  2003/06/30 04:58:22  neerache
		Make MBCMoveBuilder a protocol, rename methods to be clearer
		
		Revision 1.1  2003/06/16 05:28:32  neerache
		Added move generation facility
		
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

// Local Variables:
// mode:ObjC
// End:
