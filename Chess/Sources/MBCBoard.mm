/*
	File:		MBCBoard.mm
	Contains:	Implementation of fundamental board and move classes
	Version:	1.0
	Copyright:	© 2002-2010 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCBoard.mm,v $
		Revision 1.22  2010/10/07 23:07:02  neerache
		<rdar://problem/8352405> [Chess]: Ab-11A250: BIDI: RTL: Incorrect alignement for strings in cells in Came log
		
		Revision 1.21  2010/09/16 22:23:40  neerache
		<rdar://problem/8352405> [Chess]: Ab-11A250: BIDI: RTL: Incorrect alignement for strings in cells in Came log
		
		Revision 1.20  2008/08/27 21:37:41  neerache
		<rdar://problem/6138230> HARDENING: Replace unsafe string functions in Chess (53 found)
		
		Revision 1.19  2007/03/03 01:13:16  neerache
		Fix warnings
		
		Revision 1.18  2007/03/02 21:10:58  neerache
		Move save/load fixes <rdar://problem/4366230>
		
		Revision 1.17  2007/03/02 20:31:44  neerache
		Don't corrupt promotion piece names <rdar://problem/4366230>
		
		Revision 1.16  2003/07/14 23:21:49  neerache
		Move promotion defaults into MBCBoard
		
		Revision 1.15  2003/06/30 05:00:11  neerache
		Add UnknownCastle, new... methods
		
		Revision 1.14  2003/06/16 05:28:32  neerache
		Added move generation facility
		
		Revision 1.13  2003/06/02 05:44:48  neerache
		Implement direct board manipulation
		
		Revision 1.12  2003/05/27 03:13:57  neerache
		Rework game loading/saving code
		
		Revision 1.11  2003/05/24 20:25:25  neerache
		Eliminate compact moves for most purposes
		
		Revision 1.10  2003/04/28 22:14:13  neerache
		Let board, not engine, handle last move
		
		Revision 1.9  2003/04/24 23:20:35  neeri
		Support pawn promotions
		
		Revision 1.8  2003/04/10 23:03:16  neeri
		Load positions
		
		Revision 1.7  2003/04/05 05:45:08  neeri
		Add PGN export
		
		Revision 1.6  2003/04/02 18:19:50  neeri
		Support saving board state
		
		Revision 1.5  2003/03/28 01:31:31  neeri
		Support hints, last move
		
		Revision 1.4  2002/09/13 23:57:05  neeri
		Support for Crazyhouse display and mouse
		
		Revision 1.3  2002/09/12 17:46:46  neeri
		Introduce dual board representation, in-hand pieces
		
		Revision 1.2  2002/08/26 23:09:44  neeri
		[MBCBoard makeMove:] needs to ignore everything except moves and drops
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
*/
#import "MBCBoard.h"
#import "MBCEngineCommands.h"
#import "MBCMoveGenerator.h"

#import <string.h>
#include <ctype.h>

MBCPiece Captured(MBCPiece victim)
{
	victim = Opposite(victim & ~kPieceMoved);
	if (Promoted(victim))	// Captured promoted pieces revert to pawns
		return Matching(victim, PAWN);
	else
		return victim;
}

static const char * sPieceChar = " KQBNRP";

@implementation MBCMove

- (id) initWithCommand:(MBCMoveCode)command;
{
	fCommand	=	command;
	fFromSquare	=	kInvalidSquare;
	fToSquare	=	kInvalidSquare;
	fPiece		=	EMPTY;
	fPromotion	= 	EMPTY;
	fVictim		= 	EMPTY;
	fCastling	=	kUnknownCastle;
	fEnPassant	= 	NO;
	fAnimate	=	YES;
	
	return self;
}

+ (id) newWithCommand:(MBCMoveCode)command
{
	return [[MBCMove alloc] initWithCommand:command];
}

+ (id) moveWithCommand:(MBCMoveCode)command
{
	return [[MBCMove newWithCommand:command] autorelease];
}

- (id) initFromCompactMove:(MBCCompactMove)move
{
	[self initWithCommand:MBCMoveCode(move >> 24)];
	
	switch (fCommand) {
	case kCmdMove:	
	case kCmdPMove:
		fFromSquare	= (move >> 16) & 0xFF;
		fToSquare	= (move >> 8)  & 0xFF;
		fPromotion	= move & 0xFF;
		break;
	case kCmdDrop:
	case kCmdPDrop:
		fToSquare	= (move >> 8)  & 0xFF;
		fPiece		= move & 0xFF;
		break;
	default:
		break;
	}

	return self;
}

+ (id) newFromCompactMove:(MBCCompactMove)move
{
	return [[MBCMove alloc] initFromCompactMove:move];
}

+ (id) moveFromCompactMove:(MBCCompactMove)move
{
	return [[MBCMove newFromCompactMove:move] autorelease];
}

- (id) initFromEngineMove:(NSString *)engineMove
{
	const char * piece	= " KQBNRP  kqbnrp ";
	const char * move	= [engineMove UTF8String];

	if (move[1] == '@') {
		[self initWithCommand:kCmdDrop];
		fPiece		= static_cast<MBCPiece>(strchr(piece, move[0])-piece);
		fToSquare	= Square(move+2);
	} else {
		[self initWithCommand:kCmdMove];
		fFromSquare	= Square(move);
		fToSquare	= Square(move+2);
		if (move[4])
			fPromotion	= static_cast<MBCPiece>(strchr(piece, move[4])-piece);
	}
	
	return self;
}

+ (id) newFromEngineMove:(NSString *)engineMove
{
	return [[MBCMove alloc] initFromEngineMove:engineMove];
}

+ (id) moveFromEngineMove:(NSString *)engineMove
{
	return [[MBCMove newFromEngineMove:engineMove] autorelease];
}

NSString * sPieceLetters[] = {
	@"",
	@"king_letter",
	@"queen_letter",
	@"bishop_letter",
	@"knight_letter",
	@"rook_letter",
	@"pawn_letter"
};

- (NSString *) pieceLetter:(MBCPiece)piece forDrop:(BOOL)drop
{
	piece = Piece(piece);
	if (!drop && piece==PAWN)
		return @" ";
	else
		return NSLocalizedString(sPieceLetters[piece], 
								 "Piece Letter");
}

- (NSString *) localizedText
{
	return [NSString stringWithFormat:NSLocalizedString(@"title_move_fmt", "%@ %@ %@"),
			[self origin], [self operation], [self destination]];
}

- (NSString *) origin
{
	switch (fCommand) {
	case kCmdMove:
	case kCmdPMove:
		if (fCastling != kNoCastle)
			return @"";
		else 
			return [NSString stringWithFormat:NSLocalizedString(@"move_origin_fmt", @"%@%c%c"),
					[self pieceLetter:fPiece forDrop:NO],
					Col(fFromSquare), Row(fFromSquare)+'0'];
	case kCmdDrop:
	case kCmdPDrop:
		return [NSString stringWithFormat:NSLocalizedString(@"drop_origin_fmt", @"%@"),
				[self pieceLetter:fPiece forDrop:YES]];
	default:
		return @"";
	}
}

- (NSString *) operation
{
	UniChar op = fVictim ? 0x00D7 : '-';
	switch (fCommand) {
	case kCmdMove:
	case kCmdPMove:
		switch (fCastling) {
		case kCastleQueenside:
			return @"0 - 0 - 0";
		case kCastleKingside:
			return @"0 - 0";
		default:
			break;
		}
		break;
	case kCmdDrop:
	case kCmdPDrop:
		op = '@';
		break;
	default:
		op = ' ';
		break;
	}
	return [NSString stringWithFormat:NSLocalizedString(@"operation_fmt", @"%C"), op];
}

- (NSString *) destination
{
	if (fCastling != kNoCastle && fCastling != kUnknownCastle)
		return @"";
	else if (fPromotion)
		return [NSString stringWithFormat:NSLocalizedString(@"promo_dest_fmt", @"%c%c=@%"),
				Col(fToSquare), Row(fToSquare)+'0', [self pieceLetter:fPromotion forDrop:NO]];
	else
		return [NSString stringWithFormat:NSLocalizedString(@"move_dest_fmt", @"%c%c"),
				Col(fToSquare), Row(fToSquare)+'0'];
}

- (NSString *) engineMove
{
	const char * piece	= " KQBNRP  kqbnrp ";

#define SQUARETOCOORD(sq) 	Col(sq), Row(sq)+'0'

	switch (fCommand) {
	case kCmdMove:
		if (fPromotion) 
			return [NSString stringWithFormat:@"%c%c%c%c%c\n", 
							 SQUARETOCOORD(fFromSquare),
							 SQUARETOCOORD(fToSquare),
							 piece[fPromotion&15]];
		else
			return [NSString stringWithFormat:@"%c%c%c%c\n", 
							 SQUARETOCOORD(fFromSquare),
							 SQUARETOCOORD(fToSquare)];
	case kCmdDrop:
		return [NSString stringWithFormat:@"%c@%c%c\n", 
						 piece[fPiece&15],
						 SQUARETOCOORD(fToSquare)];
		break;
	default:
		return nil;
	}	
}

@end

@implementation MBCBoard

- (id) init
{
	fMoves	= nil;

	[self reset];
        
	return self;
}

- (void) reset
{
	memset(fCurPos.fBoard, EMPTY, 64);
	memset(fCurPos.fInHand, 0, 16);

	/* White pieces */
	fCurPos.fBoard[Square('a',1)] = White(ROOK);
	fCurPos.fBoard[Square('b',1)] = White(KNIGHT);
	fCurPos.fBoard[Square('c',1)] = White(BISHOP);
	fCurPos.fBoard[Square('d',1)] = White(QUEEN);
	fCurPos.fBoard[Square('e',1)] = White(KING);
	fCurPos.fBoard[Square('f',1)] = White(BISHOP);
	fCurPos.fBoard[Square('g',1)] = White(KNIGHT);
	fCurPos.fBoard[Square('h',1)] = White(ROOK);
	fCurPos.fBoard[Square('a',2)] = White(PAWN);
	fCurPos.fBoard[Square('b',2)] = White(PAWN);
	fCurPos.fBoard[Square('c',2)] = White(PAWN);
	fCurPos.fBoard[Square('d',2)] = White(PAWN);
	fCurPos.fBoard[Square('e',2)] = White(PAWN);
	fCurPos.fBoard[Square('f',2)] = White(PAWN);
	fCurPos.fBoard[Square('g',2)] = White(PAWN);
	fCurPos.fBoard[Square('h',2)] = White(PAWN);

	/* Black pieces */
	fCurPos.fBoard[Square('a',7)] = Black(PAWN);
	fCurPos.fBoard[Square('b',7)] = Black(PAWN);
	fCurPos.fBoard[Square('c',7)] = Black(PAWN);
	fCurPos.fBoard[Square('d',7)] = Black(PAWN);
	fCurPos.fBoard[Square('e',7)] = Black(PAWN);
	fCurPos.fBoard[Square('f',7)] = Black(PAWN);
	fCurPos.fBoard[Square('g',7)] = Black(PAWN);
	fCurPos.fBoard[Square('h',7)] = Black(PAWN);
	fCurPos.fBoard[Square('a',8)] = Black(ROOK);
	fCurPos.fBoard[Square('b',8)] = Black(KNIGHT);
	fCurPos.fBoard[Square('c',8)] = Black(BISHOP);
	fCurPos.fBoard[Square('d',8)] = Black(QUEEN);
	fCurPos.fBoard[Square('e',8)] = Black(KING);
	fCurPos.fBoard[Square('f',8)] = Black(BISHOP);
	fCurPos.fBoard[Square('g',8)] = Black(KNIGHT);
	fCurPos.fBoard[Square('h',8)] = Black(ROOK);

	fPrvPos		= fCurPos;
	fVariant	= kVarNormal;
	fMoveClock	= 0;

	[fMoves release];
	fMoves	= [[NSMutableArray alloc] init];

	fPromotion[0] = QUEEN;
	fPromotion[1] = QUEEN;
}

- (void) startGame:(MBCVariant)variant
{
	[self reset];
	fVariant	= variant;
}

- (MBCPiece) curContents:(MBCSquare)square
{
	return fCurPos.fBoard[square];
}

- (MBCPiece) oldContents:(MBCSquare)square
{
	return fPrvPos.fBoard[square];
}

- (int) curInHand:(MBCPiece)piece
{
	return fCurPos.fInHand[piece];
}

- (int) oldInHand:(MBCPiece)piece
{
	return fPrvPos.fInHand[piece];
}

//
// After every move, pieces on the board and in hand have to balance out
//
- (void) consistencyCheck
{
	char	inventory[8];

	memset(inventory, 0, 8);
	
	inventory[PAWN]		= 16;
	inventory[ROOK]		=  4;
	inventory[KNIGHT]	=  4;
	inventory[BISHOP]	=  4;
	inventory[QUEEN]	=  2;
	inventory[KING]		=  2;

	for (MBCPiece * p = fCurPos.fBoard; p < fCurPos.fBoard+64; ++p) {
		MBCPiece piece = *p;
		--inventory[Promoted(piece) ? PAWN : Piece(piece)];
	}
	for (int i = 1; i < 8; ++i) {
		inventory[i] -= fCurPos.fInHand[i]+fCurPos.fInHand[i|kBlackPiece];
		if (inventory[i]) 
			NSLog(@"Board consistency check: %d %d\n", i, inventory[i]);
	}
#if 0
	MBCMoveGenerator	logMoves([MBCDebugMoveBuilder debugMoveBuilder], 
								 fVariant, 0);
	logMoves.Generate(!([fMoves count] & 1), fCurPos);
	if (logMoves.InCheck(!([fMoves count] & 1), fCurPos)) 
		NSLog(@"Check!");
#endif
}

- (void) makeMove:(MBCMove *)move
{	
	//
	// Ignore everything except moves & drops
	//
	if (move->fCommand != kCmdMove && move->fCommand != kCmdDrop)
		return;

	//
	// Make the move on the board
	//
	MBCSquare	toSquare	= move->fToSquare;
	MBCSquare	fromSquare	= move->fFromSquare;
	MBCPiece * 	board		= fCurPos.fBoard;
	char *		inHand		= fCurPos.fInHand;
	MBCPiece 	piece 		= move->fPromotion;

	if (move->fCommand == kCmdMove) {
		move->fPiece = board[fromSquare];
		[self tryCastling:move];
		[self tryPromotion:move];
		if (!piece) { 
			//
			// Not a pawn promotion, piece stays the same
			//
			piece = move->fPiece;
		} else { 
			//
			// Pawn promotion
			//
			move->fPromotion = 
				Piece(piece) | Color(move->fPiece) | kPromoted;
			piece = move->fPromotion | kPieceMoved;
		}
		if (MBCPiece victim = board[toSquare]) {
			//
			// Record captured piece
			//
			move->fVictim = victim;
			++inHand[Captured(victim)];
		} else if (Piece(piece) == PAWN && Col(fromSquare) != Col(toSquare)) {
			//
			// En passant capture
			//
			MBCSquare victimSquare  = Square(Col(toSquare), Row(fromSquare));
			MBCPiece  victim		= board[victimSquare];
			move->fVictim 			= victim;
			++inHand[Captured(victim)];
			board[victimSquare] 	= EMPTY;
			move->fEnPassant		= YES;
		}
		if (move->fVictim || Piece(move->fPiece) == PAWN)
			fMoveClock	= 0;
		else 
			++fMoveClock;
		board[fromSquare] = EMPTY;
		unsigned row = Row(toSquare);
		switch (move->fCastling) {
		case kUnknownCastle:
		case kNoCastle:
			break;
		case kCastleQueenside:
			board[Square('d', row)] = board[Square('a', row)] | kPieceMoved;
			board[Square('a', row)] = EMPTY;
			break;
		case kCastleKingside:
			board[Square('f', row)] = board[Square('h', row)] | kPieceMoved;
			board[Square('h', row)] = EMPTY;
			break;
		}
		piece			   |= kPieceMoved;
		if (!move->fVictim && Piece(piece) == PAWN && 
			abs(Row(fromSquare)-Row(toSquare))==2
		)
			fCurPos.fEnPassant	= Square(Col(fromSquare), 
										 (Row(fromSquare)+Row(toSquare))/2);
		else
			fCurPos.fEnPassant	= kInvalidSquare;
	} else { 
		//
		// Drop, deplete in hand pieces. A dropped piece is not considered
		// to have moved yet.
		//
		piece	= move->fPiece;
		--inHand[piece];
	}
	board[toSquare] = piece;

	//
	// Record the move made in undo buffer
	//
	[fMoves addObject:move];

	[self consistencyCheck];
}

- (MBCCastling) tryCastling:(MBCMove *)move
{
	move->fCastling			= kNoCastle;

	MBCSquare   fromSquare 	= move->fFromSquare;
	MBCPiece * 	board		= fCurPos.fBoard;
	MBCPiece	king 		= move->fPiece;

	if (Piece(king) != KING || king & kPieceMoved) 
		return kNoCastle;
	MBCPieceCode 	kingColor = Color(king);
	unsigned 		row = Row(move->fToSquare);
	if (Row(fromSquare) != row)
		return kNoCastle;
		
	//
	// These comparisons will fail if the rook has kPieceMoved set, which
	// they should.
	//
	switch (Col(move->fToSquare)) {
	case 'c':	// Queenside castle
		if (board[Square('a', row)] != Matching(kingColor, ROOK)
		 || board[Square('b', row)] != EMPTY
		 || board[Square('c', row)] != EMPTY
		 || board[Square('d', row)] != EMPTY
		)
			return kNoCastle;
		else
			return move->fCastling = kCastleQueenside;
	case 'g':	// Kingside castle
		if (board[Square('h', row)] != Matching(kingColor, ROOK)
		 || board[Square('g', row)] != EMPTY
		 || board[Square('f', row)] != EMPTY
		)
			return kNoCastle;
		else
			return move->fCastling = kCastleKingside;
	default:
		return kNoCastle;
	}
}

- (void)tryPromotion:(MBCMove *)move
{
	if (move->fCommand == kCmdMove 
	  && Piece(fCurPos.fBoard[move->fFromSquare]) == PAWN
    )
		//
		// Possibly a promotion where we need to fill in the piece
		//
		switch (Row(move->fToSquare)) {
		case 1:
			//
			// Black promotion
			//
			if (!move->fPromotion)
				move->fPromotion = fPromotion[0];
			break;
		case 8:
			//
			// White promotion
			//
			if (!move->fPromotion)
				move->fPromotion = fPromotion[1]; 
			break;
		default:
			move->fPromotion = EMPTY;
			break;
		}
}

- (BOOL) reachFromCol:(char)fromCol row:(unsigned)fromRow
			 deltaCol:(int)colDelta row:(int)rowDelta
				steps:(int)steps
{
	if (steps < 0)
		steps = -steps;
	while (--steps > 0) 
		if (fCurPos.fBoard[Square(fromCol += colDelta, fromRow += rowDelta)])
			return NO; // Occupied square in between

	return YES;
}

- (BOOL) reachDiagonalFromCol:(char)fromCol row:(unsigned)fromRow
						toCol:(char)toCol row:(unsigned)toRow
{
	int colDiff	= toCol - fromCol;
	int rowDiff	= (int)toRow - (int)fromRow;

	if (colDiff != rowDiff && colDiff != -rowDiff)
		return NO;	// Not on same diagonal

	return [self reachFromCol:fromCol row:fromRow 
				 deltaCol:(colDiff<0 ? -1 : 1) row:(rowDiff<0 ? -1 : 1)
				 steps:colDiff];
}

- (BOOL) reachStraightFromCol:(char)fromCol row:(unsigned)fromRow
						toCol:(char)toCol row:(unsigned)toRow
{
	if (fromRow==toRow)
		return [self reachFromCol:fromCol row:fromRow 
					 deltaCol:(toCol<fromCol ? -1 : 1) row:0 
					 steps:toCol-fromCol];
	else if (fromCol==toCol)
		return [self reachFromCol:fromCol row:fromRow 
					 deltaCol:0 row:(toRow<fromRow ? -1 : 1)
					 steps:toRow-fromRow];
	else
		return NO;
}

- (MBCUnique) disambiguateMove:(MBCMove *)move
{
	MBCSquare 	from	= move->fFromSquare;
	unsigned	fromRow	= Row(from);
	char		fromCol	= Col(from);
	MBCSquare 	to		= move->fToSquare;
	unsigned	toRow	= Row(to);
	char		toCol	= Col(to);
	MBCPiece 	piece	= fCurPos.fBoard[from];
	MBCUnique	unique	= 0;

	for (char col = 'a'; col < 'i'; ++col)
		for (unsigned row = 1; row<9; ++row) {
			if (col == fromCol && row == fromRow)
				continue;	// Same as from square
			if (col == toCol && row == toRow)
				continue;	// Same as to square
			if ((fCurPos.fBoard[Square(col, row)] ^ piece) & 15) 
				continue;	// Not a matching piece
			switch (Piece(piece)) {
			case PAWN: 
				//
				// The only column ambiguities can exist with captures
				//
				if (fromRow == row && toCol-fromCol == col-toCol)
					break;	
				continue;
			case KING: // Multiple kings? Only in suicide
				if (col>=toCol-1 && col<=toCol+1 && row>=toRow-1 && row<=toRow+1)
					break;
				continue;
			case KNIGHT: 
				if (col == toCol-1 || col == toCol+1) {
					if (row == toRow-2 || row == toRow+2)
						break;
				} else if (col == toCol-2 || col == toCol+2) {
					if (row == toRow-1 || row == toRow+1)
						break;
				}
				continue;
			case BISHOP:
				if ([self reachDiagonalFromCol:col row:row toCol:toCol row:toRow])
					break;
				continue;
			case QUEEN:
				if ([self reachDiagonalFromCol:col row:row toCol:toCol row:toRow])
					break;
				// Fall through
			case ROOK:
				if ([self reachStraightFromCol:col row:row toCol:toCol row:toRow])
					break;
				continue;
			default:
				continue;
			}
			unique	|= kMatchingPieceExists;
			if (row == fromRow)
				unique	|= kMatchingPieceOnSameRow;
			if (col == fromCol)
				unique	|= kMatchingPieceOnSameCol;
		}	
	return unique;
}

- (bool) undoMoves:(int)numMoves
{
	if ((int)[fMoves count]<numMoves)
		return false;

	if (fMoveClock < numMoves)
		fMoveClock = 0;
	else
		fMoveClock -= numMoves;

	while (numMoves-- > 0) {
		MBCMove *  	move 		= [fMoves lastObject];
		MBCPiece * 	board		= fCurPos.fBoard;
		char *		inHand		= fCurPos.fInHand;
	
		if (move->fCommand == kCmdMove) {
			board[move->fFromSquare] = move->fPiece;
			unsigned row = Row(move->fToSquare);
			switch (move->fCastling) {
			case kUnknownCastle:	
			case kNoCastle:	
				break;
			case kCastleQueenside:
				board[Square('a', row)] = 
					board[Square('d', row)] & ~kPieceMoved;
				board[Square('d', row)] = EMPTY;
				break;
			case kCastleKingside:
				board[Square('h', row)] = 
					board[Square('f', row)] & ~kPieceMoved;
				board[Square('f', row)] = EMPTY;
				break;
			}
		} else 
			++inHand[move->fPiece];
		board[move->fToSquare] = EMPTY;
		if (MBCPiece victim = move->fVictim) {
			MBCSquare victimSquare =
				move->fEnPassant 
				? Square(Col(move->fToSquare), Row(move->fFromSquare))
				: move->fToSquare;
			board[victimSquare] = victim;
			--inHand[Captured(victim)];
		}

		[fMoves removeLastObject];

		[self consistencyCheck];
	}
	fPrvPos = fCurPos;

	return true;
}

- (void) commitMove
{
	fPrvPos = fCurPos;
}

- (NSString *) fen
{
	char 	pos[128];
	char * 	p	= pos;

	*p++ = ' ';
	for (MBCSquare rank = 64; rank; rank -= 8) {
		for (MBCSquare square = rank-8; square < rank; ++square) 
			if (MBCPiece piece = fCurPos.fBoard[square])
				*p++ = " KQBNRP  kqbnrp "[What(piece)];
			else if (isdigit(p[-1]))
				++p[-1];
			else
				*p++ = '1';
		if (rank > 8)
			*p++ = '/';
	}
	*p++ = ' ';
	*p++ = ([fMoves count]&1) ? 'b' : 'w';
	*p++ = ' ';
	if (fCurPos.fBoard[Square('e', 1)] == White(KING)) {
		if (fCurPos.fBoard[Square('h', 1)] == White(ROOK))
			*p++ = 'K';
		if (fCurPos.fBoard[Square('a', 1)] == White(ROOK))
			*p++ = 'Q';
	}
	if (fCurPos.fBoard[Square('e', 8)] == Black(KING)) {
		if (fCurPos.fBoard[Square('h', 8)] == Black(ROOK))
			*p++ = 'k';
		if (fCurPos.fBoard[Square('a', 8)] == Black(ROOK))
			*p++ = 'q';
	}
	if (p[-1] == ' ')
		*p++ = '-';
	*p++ = ' ';
	*p++ = '-';
	if ([fMoves count]) {
		MBCMove *  move = [fMoves lastObject];
		if ((move->fPiece & (7|kPieceMoved)) == PAWN
			&& (Row(move->fToSquare) & 6) == 4
		) {
		    p[-1] = Col(move->fToSquare);
			*p++  = Row(move->fToSquare) == 4 ? '3' : '6';
		}
	}
	snprintf(p, 32, " %d %d", fMoveClock, ([fMoves count]/2)+1);

	return [NSString stringWithUTF8String:pos+1];
}

- (NSString *) holding
{
	char 	pos[128];
	char * 	p	= pos;

	*p++ = '[';
	for (MBCPiece piece = White(KING); piece <= Black(PAWN); ++piece) {
		for (int count = fCurPos.fInHand[piece]; count--; )
			*p++ = " KQBNRP "[Piece(piece)];
		if (piece == 8) {
			strcpy(p, "] [");
			p += 3;
		}
	}
	strcpy(p, "]");

	return [NSString stringWithUTF8String:pos];
}

- (NSString *)moves
{	
	NSMutableString * 	moves 		= [NSMutableString stringWithCapacity:200];
	int 				numMoves 	= [fMoves count];

	for (int m = 0; m<numMoves; ++m) {
		MBCMove *  move = [fMoves objectAtIndex:m];
		[moves appendString:[move engineMove]];
	}
	return moves;
}

- (void) setFen:(NSString *)fen holding:(NSString *)holding 
	moves:(NSString *)moves
{
	if (moves) {
		//
		// We prefer to restore the game by replaying the moves
		//
		[self reset];
		NSArray * 		m = [moves componentsSeparatedByString:@"\n"];
		NSEnumerator *	e = [m objectEnumerator];
		while (NSString * move = [e nextObject]) 
			if ([move length])
				[self makeMove:[MBCMove moveFromEngineMove:move]];
		if (![fen isEqual:[self fen]]) 
			NSLog(@"FEN Mismatch, Expected: <%@> Got <%@>\n",
				  fen, [self fen]);
	} else {
		const char * s = [fen UTF8String];
		MBCPiece *   b = fCurPos.fBoard+56;
		MBCPiece	 p;

		memset(fCurPos.fBoard, 0, 64);
		while (isspace(*s))
			++s;
		do {
			switch (*s++) {
			case 'K':
				p = White(KING) | kPieceMoved;
				break;
			case 'Q':
				p = White(QUEEN);
				break;
			case 'B':
				p = White(BISHOP);
				break;
			case 'N':
				p = White(KNIGHT);
				break;
			case 'R':
				p = White(ROOK) | kPieceMoved;
				break;
			case 'P':
				p = White(PAWN) | kPieceMoved;
				break;
			case 'k':
				p = Black(KING) | kPieceMoved;
				break;
			case 'q':
				p = Black(QUEEN);
				break;
			case 'b':
				p = Black(BISHOP);
				break;
			case 'n':
				p = Black(KNIGHT);
				break;
			case 'r':
				p = Black(ROOK) | kPieceMoved;
				break;
			case 'p':
				p = Black(PAWN) | kPieceMoved;
				break;
			case '8':
			case '7':
			case '6':
			case '5':
			case '4':
			case '3':
			case '2':
			case '1':
				p =  EMPTY;
				b += s[-1]-'0';
				if (!((b-fCurPos.fBoard) & 7))
					b -= 16; // Start previous rank
				break;
			case '/':
			default:
				p = EMPTY;
				break;
			}
			if (p) {
				*b++ = p;
				if (!((b-fCurPos.fBoard) & 7))
					b -= 16; // Start previous rank
			}
		} while (b >= fCurPos.fBoard);

		while (isspace(*s))
			++s;

		if (*s++ == 'b')	
			[fMoves addObject:[MBCMove moveWithCommand:kCmdNull]];
		
		while (isspace(*s))
			++s;
		
		while (!isspace(*s))
			switch (*s++) {
			case 'K':
				fCurPos.fBoard[4] &= ~kPieceMoved;
				fCurPos.fBoard[7] &= ~kPieceMoved;
				break;
			case 'Q':
				fCurPos.fBoard[4] &= ~kPieceMoved;
				fCurPos.fBoard[0] &= ~kPieceMoved;
				break;
			case 'k':
				fCurPos.fBoard[60] &= ~kPieceMoved;
				fCurPos.fBoard[63] &= ~kPieceMoved;
				break;
			case 'q':
				fCurPos.fBoard[60] &= ~kPieceMoved;
				fCurPos.fBoard[56] &= ~kPieceMoved;
				break;
			}

		while (isspace(*s))
			++s;

		if (*s == '-')
			fCurPos.fBoard[Square(*s, s[1]-'0')] &= ~kPieceMoved;
		s += 2;

		while (isspace(*s))
			++s;

		fMoveClock = 0;
		while (isdigit(*s))
			fMoveClock = 10*fMoveClock + *s++ - '0';

		memset(fCurPos.fInHand, 0, 16);

		s = [holding UTF8String];

		s = strchr(s, '[');
		if (!s)
			return;

		do {
			switch (*++s) {
			case 'Q':
				p	= White(QUEEN);
				break;
			case 'B':
				p	= White(BISHOP);
				break;
			case 'N':
				p	= White(KNIGHT);
				break;
			case 'R':
				p 	= White(ROOK);
				break;
			case 'P':
				p	= White(PAWN);
				break;
			default:
				p	= 0;
				break;
			}
			if (p)
				++fCurPos.fInHand[p];
		} while (p);

		s = strchr(s, '[');
		if (!s)
			return;
		
		do {
			switch (*++s) {
			case 'Q':
				p	= Black(QUEEN);
				break;
			case 'B':
				p	= Black(BISHOP);
				break;
			case 'N':
				p	= Black(KNIGHT);
				break;
			case 'R':
				p	= Black(ROOK);
				break;
			case 'P':
				p	= Black(PAWN);
				break;
			default:
				p 	= 0;
				break;
			}
			if (p)
				++fCurPos.fInHand[p];
		} while (p);
	}
	fPrvPos = fCurPos;
}

- (BOOL) saveMovesTo:(FILE *)f
{
	NSArray * existingMoves = [fMoves copy];
	int moves = [fMoves count];
	
	//
	// Reset board so we can disambiguate moves
	//
	[self undoMoves:moves];

	//
	// Now retrace the moves
	//
	for (int m = 0; m<moves; ++m) {
		if (!(m&1)) {
			if (!(m%10))
				fputc('\n', f);
			fprintf(f, "%d. ", (m / 2)+1);
		}
		MBCMove *  move = [existingMoves objectAtIndex:m];

		if (move->fCommand == kCmdDrop) { // Drop, never ambiguous
			fprintf(f, "%c@%c%d ", sPieceChar[Piece(move->fPiece)], 
					Col(move->fToSquare), Row(move->fToSquare));
		} else { // Move, may be ambiguous
			MBCPiece  p = Piece(fCurPos.fBoard[move->fFromSquare]);

			if (p==PAWN) { // Pawn moves look a bit different
				if (move->fVictim) // Capture
					fprintf(f, "%cx%c%d", Col(move->fFromSquare),
							Col(move->fToSquare), Row(move->fToSquare));
				else // Move
					fprintf(f, "%c%d", Col(move->fToSquare), 
							Row(move->fToSquare));
				if (move->fPromotion) // Promotion?
					fprintf(f, "=%c ", sPieceChar[Piece(move->fPromotion)]);
				else
					fputc(' ', f);
			} else if (move->fCastling != kNoCastle) {
				if (move->fCastling == kCastleQueenside)
					fputs("O-O-O ", f);
				else
					fputs("O-O ", f);
			} else {
				MBCUnique u = [self disambiguateMove:move];
				fputc(sPieceChar[p], f);
				if (u) {
					if (u != (kMatchingPieceExists|kMatchingPieceOnSameCol))
						fputc(Col(move->fFromSquare), f);
					if (u & kMatchingPieceOnSameCol)
						fputc('0'+Row(move->fFromSquare), f);
				}
				if (move->fVictim) // Capture
					fputc('x', f);
				fprintf(f, "%c%d ", Col(move->fToSquare), Row(move->fToSquare));
			}
		}
		
		[self makeMove: move];
	}
	[existingMoves release];

	return YES;
}

- (BOOL) canPromote:(MBCSide)side
{
	MBCPiece 	piece;
	unsigned	rank;

	if (side == kBlackSide != ([fMoves count]&1))
		return NO;

	if (side == kBlackSide) {
		piece	= Black(PAWN);
		rank	= 2;
	} else {
		piece	= White(PAWN);
		rank 	= 7;
	}

	for (char file = 'a'; file < 'i'; ++file)
		if (What(fCurPos.fBoard[Square(file, rank)]) == piece)
			return YES;

	return NO;
}

- (BOOL) canUndo
{
	return [fMoves count] > 1;
}

- (MBCMove *) lastMove
{
	return [fMoves lastObject];
}

- (int) numMoves
{
	return [fMoves count];
}

- (MBCMove *) move:(int)index
{
	return (index < (int)[fMoves count]) ? [fMoves objectAtIndex:index] : nil;
}

- (MBCPieces *) curPos
{
	return &fCurPos;
}

- (MBCPiece) defaultPromotion:(BOOL)white
{
	return fPromotion[white];
}

- (void) setDefaultPromotion:(MBCPiece)piece for:(BOOL)white
{
	fPromotion[white] = piece;
}

@end

inline MBCCompactMove EncodeCompactMove(
			 MBCMoveCode cmd, MBCSquare from, MBCSquare to, MBCPiece piece)
{
	return (cmd << 24) | (from << 16) | (to << 8) | piece;
}

inline MBCCompactMove EncodeCompactCommand(MBCMoveCode cmd)
{
	return cmd << 24;
}

MBCCompactMove MBCEncodeMove(const char * mv, int ponder)
{
	const char * 	piece	= " kqbnrp ";
	const char * 	p;
	MBCPiece		promo	= EMPTY;

	if (mv[4] && (p = strchr(piece, mv[4])))
		promo	= p-piece;

	return EncodeCompactMove(ponder ? kCmdPMove : kCmdMove,
							 Square(mv+0), Square(mv+2), promo);
}

MBCCompactMove MBCEncodeDrop(const char * drop, int ponder)
{
	const char * 	piece	= " KQBNRP  kqbnrp ";
	
	return EncodeCompactMove(ponder ? kCmdPDrop : kCmdDrop,
							 0, Square(drop+2),
							 strchr(piece, drop[0])-piece);
}

MBCCompactMove MBCEncodeIllegal()
{
	return EncodeCompactCommand(kCmdUndo);
}

MBCCompactMove MBCEncodeLegal()
{
	return EncodeCompactCommand(kCmdMoveOK);
}

MBCCompactMove MBCEncodePong()
{
	return EncodeCompactCommand(kCmdPong);
}

MBCCompactMove MBCEncodeStartGame()
{
	return EncodeCompactCommand(kCmdStartGame);
}

MBCCompactMove MBCEncodeWhiteWins()
{
	return EncodeCompactCommand(kCmdWhiteWins);
}

MBCCompactMove MBCEncodeBlackWins()
{
	return EncodeCompactCommand(kCmdBlackWins);
}

MBCCompactMove MBCEncodeDraw()
{
	return EncodeCompactCommand(kCmdDraw);
}

MBCCompactMove MBCEncodeTakeback()
{
	return EncodeCompactCommand(kCmdUndo);
}

// Local Variables:
// mode:ObjC
// End:
