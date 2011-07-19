/*
	File:		MBCBoard.h
	Contains:	Fundamental move and board classes.
	Version:	1.0
	Copyright:	© 2002-2011 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCBoard.h,v $
		Revision 1.19  2011/04/11 16:42:03  neerache
		<rdar://problem/9220767> 11A419: Speaking moves is a mix of Russian/English
		
		Revision 1.18  2011/03/12 23:43:53  neerache
		<rdar://problem/9079430> 11A390: Can't understand what Japanese voice (Kyoko Premium) says when pieces move at all.
		
		Revision 1.17  2010/10/07 23:07:02  neerache
		<rdar://problem/8352405> [Chess]: Ab-11A250: BIDI: RTL: Incorrect alignement for strings in cells in Came log
		
		Revision 1.16  2003/07/14 23:21:49  neerache
		Move promotion defaults into MBCBoard
		
		Revision 1.15  2003/06/30 05:00:11  neerache
		Add UnknownCastle, new... methods
		
		Revision 1.14  2003/06/16 05:28:32  neerache
		Added move generation facility
		
		Revision 1.13  2003/06/04 09:25:47  neerache
		New and improved board manipulation metaphor
		
		Revision 1.12  2003/06/02 05:44:48  neerache
		Implement direct board manipulation
		
		Revision 1.11  2003/05/27 03:13:57  neerache
		Rework game loading/saving code
		
		Revision 1.10  2003/05/24 20:25:25  neerache
		Eliminate compact moves for most purposes
		
		Revision 1.9  2003/04/28 22:14:13  neerache
		Let board, not engine, handle last move
		
		Revision 1.8  2003/04/24 23:20:35  neeri
		Support pawn promotions
		
		Revision 1.7  2003/04/10 23:03:16  neeri
		Load positions
		
		Revision 1.6  2003/04/05 05:45:08  neeri
		Add PGN export
		
		Revision 1.5  2003/04/02 18:19:50  neeri
		Support saving board state
		
		Revision 1.4  2003/03/28 01:31:31  neeri
		Support hints, last move
		
		Revision 1.3  2002/09/13 23:57:05  neeri
		Support for Crazyhouse display and mouse
		
		Revision 1.2  2002/09/12 17:46:46  neeri
		Introduce dual board representation, in-hand pieces
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
*/

#import <OpenGL/gl.h>
#import <Cocoa/Cocoa.h>
#import <stdio.h>

enum MBCVariant {
	kVarNormal,
	kVarCrazyhouse,
	kVarSuicide,
	kVarLosers
};

enum MBCUniqueCode {
	kMatchingPieceExists 	= 1,
	kMatchingPieceOnSameRow	= 2,
	kMatchingPieceOnSameCol = 4,
};
typedef int MBCUnique;

enum MBCPieceCode {
	EMPTY = 0, 
	KING, QUEEN, BISHOP, KNIGHT, ROOK, PAWN,
	kWhitePiece = 0,
	kBlackPiece = 8,
	kPromoted	= 16,
	kPieceMoved	= 32
};
typedef unsigned char MBCPiece;

inline MBCPiece White(MBCPieceCode code) { return kWhitePiece | code; }
inline MBCPiece Black(MBCPieceCode code) { return kBlackPiece | code; }
inline MBCPieceCode Piece(MBCPiece piece){ return (MBCPieceCode)(piece&7); }
inline MBCPieceCode Color(MBCPiece piece){ return (MBCPieceCode)(piece&8); }
inline MBCPieceCode What(MBCPiece piece) { return (MBCPieceCode)(piece&15);} 
inline MBCPiece Matching(MBCPiece piece, MBCPieceCode code) 
                                         { return (piece & 8) | code; }
inline MBCPiece Opposite(MBCPiece piece) { return piece ^ 8; }
inline MBCPieceCode Promoted(MBCPiece piece) 
                                         { return (MBCPieceCode)(piece & 16); }
inline MBCPieceCode PieceMoved(MBCPiece piece) 
                                         { return (MBCPieceCode)(piece & 32); }

enum MBCMoveCode { 
	kCmdNull, 
	kCmdMove, 		kCmdDrop, 		kCmdUndo, 
	kCmdWhiteWins, 	kCmdBlackWins, 	kCmdDraw,
	kCmdPong, 		kCmdStartGame,
	kCmdPMove,		kCmdPDrop, 
	kCmdMoveOK
};

typedef unsigned char MBCSquare;
enum {
	kSyntheticSquare	= 0x70,
    kWhitePromoSquare	= 0x71,
	kBlackPromoSquare	= 0x72,
	kBorderRegion		= 0x73,
	kInHandSquare  		= 0x80,
	kInvalidSquare 		= 0xFF
};

inline unsigned 	Row(MBCSquare square)		   
                       { return 1+(square>>3); 			}
inline char 		Col(MBCSquare square)		   
                       { return 'a'+(square&7);			}
inline MBCSquare	Square(char col, unsigned row) 
                       { return ((row-1)<<3)|(col-'a'); }
inline MBCSquare	Square(const char * colrow) 
                       { return ((colrow[1]-'1')<<3)|(colrow[0]-'a'); }

enum MBCCastling {
	kUnknownCastle, kCastleQueenside, kCastleKingside, kNoCastle
};

enum MBCSide {
	kWhiteSide, kBlackSide, kBothSides, kNeitherSide
};

//
// A compact move has a very short existence and is only used in places
// where the information absolutely has to be kept to 32 bits.
//
typedef unsigned MBCCompactMove;

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

- (NSString *) localizedText;
- (NSString *) engineMove;
- (NSString *) origin;
- (NSString *) operation;
- (NSString *) destination;

@end

//
// MBCPieces - The full position representation
//
struct MBCPieces {
	MBCPiece		fBoard[64];
	char			fInHand[16];
	MBCSquare		fEnPassant;	// Current en passant square, if any
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
}

- (id) 			init;
- (void) 		reset;
- (void)		startGame:(MBCVariant)variant;
- (MBCPiece)	curContents:(MBCSquare)square;	// Read contents of a square
- (MBCPiece)	oldContents:(MBCSquare)square;	// Read contents of a square
- (int)			curInHand:(MBCPiece)piece;		// # of pieces to drop
- (int)			oldInHand:(MBCPiece)piece;		// # of pieces to drop
- (void) 		makeMove:(MBCMove *)move; 		// Move pieces and record
- (MBCCastling) tryCastling:(MBCMove *)move;
- (void)		tryPromotion:(MBCMove *)move;
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

@end

// Local Variables:
// mode:ObjC
// End:
