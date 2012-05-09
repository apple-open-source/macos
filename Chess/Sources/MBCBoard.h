/*
	File:		MBCBoard.h
	Contains:	Fundamental move and board classes.
	Version:	1.0
	Copyright:	© 2002-2011 by Apple Computer, Inc., all rights reserved.
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
