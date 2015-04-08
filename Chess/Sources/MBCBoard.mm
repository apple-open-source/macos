/*
	File:		MBCBoard.mm
	Contains:	Implementation of fundamental board and move classes
	Copyright:	© 2002-2011 by Apple Inc., all rights reserved.

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
#import "MBCEngineCommands.h"
#import "MBCMoveGenerator.h"
#import "MBCPlayer.h"
#import "MBCDocument.h"

#import <string.h>
#include <ctype.h>

NSString *  gVariantName[] = {
	@"normal", @"crazyhouse", @"suicide", @"losers", nil
};

const char  gVariantChar[]	= "nzsl";

const MBCSide gHumanSide[]  = {
    kBothSides, kWhiteSide, kBlackSide, kNeitherSide, kBothSides
};

const MBCSide gEngineSide[] = {
    kNeitherSide, kBlackSide, kWhiteSide, kBothSides, kNeitherSide
};

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

+ (BOOL)compactMoveIsWin:(MBCCompactMove)move
{
    switch (move >> 24) {
    case kCmdWhiteWins:
    case kCmdBlackWins:
        return YES;
    default:
        return NO;
    }
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
    NSString * origin       = [self origin];
    NSString * operation    = [self operation];
    NSString * destination  = [self destinationForTitle:YES];
    NSString * check        = [self check];
    NSString * text;
    if ([origin length] || [destination length])
        text = [NSString localizedStringWithFormat:NSLocalizedString(@"title_move_fmt", "%@%@%@"),
                origin, operation, destination];
    else 
        text = operation;
    if ([check length])
        text = [NSString localizedStringWithFormat:NSLocalizedString(@"title_check_fmt", @"%@%@"),
                text, check];
    
    return text;
}

- (NSString *) origin
{
	switch (fCommand) {
	case kCmdMove:
	case kCmdPMove:
		if (fCastling != kNoCastle)
			return @"";
		else 
			return [NSString localizedStringWithFormat:NSLocalizedString(@"move_origin_fmt", @"%@%c%c"),
					[self pieceLetter:fPiece forDrop:NO],
					Col(fFromSquare), Row(fFromSquare)+'0'];
	case kCmdDrop:
	case kCmdPDrop:
		return [NSString localizedStringWithFormat:NSLocalizedString(@"drop_origin_fmt", @"%@"),
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
	return [NSString localizedStringWithFormat:NSLocalizedString(@"operation_fmt", @"%C"), op];
}

- (NSString *) destinationForTitle:(BOOL)forTitle
{
    NSString * check = [self check];
    NSString * text;
	if (fCastling != kNoCastle && fCastling != kUnknownCastle)
		return check;
	else if (fPromotion)
		text = [NSString localizedStringWithFormat:NSLocalizedString(@"promo_dest_fmt", @"%c%c=@%"),
                    Col(fToSquare), Row(fToSquare)+'0', [self pieceLetter:fPromotion forDrop:NO]];
	else
		text = [NSString localizedStringWithFormat:NSLocalizedString(@"move_dest_fmt", @"%c%c"),
                    Col(fToSquare), Row(fToSquare)+'0']; 
    if ([check length])
        return [NSString localizedStringWithFormat:NSLocalizedString(@"dest_check_fmt", @"%@ %@"),
                text, check];
    else 
        return text;
}

- (NSString *) destination
{
    return [self destinationForTitle:NO];
}

- (NSString *) check
{
    if (fCheckMate)
        return NSLocalizedString(@"move_is_checkmate", @"­");
    else if (fCheck)
        return NSLocalizedString(@"move_is_check", @"+");
    else
        return @"";
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

bool MBCPieces::NoPieces(MBCPieceCode color)
{
    color = (MBCPieceCode)Opposite(Color(color));
    
    return fInHand[color+QUEEN]  == 1
        && fInHand[color+BISHOP] == 2
        && fInHand[color+KNIGHT] == 2
        && fInHand[color+ROOK]   == 2
        && fInHand[color+PAWN]   == 8;
}

@implementation MBCBoard

- (id)init
{
    if (self = [super init])
        fObservers = [[NSMutableArray alloc] init];
    
    return self;
}

- (void)removeChessObservers
{
    NSNotificationCenter * notificationCenter = [NSNotificationCenter defaultCenter];
    [fObservers enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
        [notificationCenter removeObserver:obj];
    }];
    [fObservers removeAllObjects];
}

- (void)dealloc
{
    [self removeChessObservers];
    [fObservers release];
    [fMoves release];
    [super dealloc];
}

- (void)setDocument:(id)doc
{
    fDocument   = doc;
	fMoves      = nil;

	[self resetWithVariant:[doc variant]];
    
    [self removeChessObservers];
    [fObservers addObject:
     [[NSNotificationCenter defaultCenter]
        addObserverForName:MBCGameLoadNotification object:doc
        queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
            NSDictionary * dict    = [note userInfo];
            NSString *     fen     = [dict objectForKey:@"Position"];
            NSString *     holding = [dict objectForKey:@"Holding"];
            NSString *     moves   = [dict objectForKey:@"Moves"];
            fVariant               = [doc variant];

            if (fen || moves)
                [self setFen:fen holding:holding moves:moves];
        }]];
}

- (void) resetWithVariant:(MBCVariant)variant
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
	fVariant	= variant;
	fMoveClock	= 0;

	[fMoves release];
	fMoves	= [[NSMutableArray alloc] init];

	fPromotion[0] = QUEEN;
	fPromotion[1] = QUEEN;
}

- (void) startGame:(MBCVariant)variant
{
	[self resetWithVariant:variant];
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
            MBCAbort([NSString localizedStringWithFormat:@"Board consistency check: %d %d\n", i, inventory[i]],
                     fDocument);
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
		if (--inHand[piece] < 0)
            MBCAbort([NSString localizedStringWithFormat:@"Dropping non-existent %c", 
                      sPieceChar[Piece(move->fPiece)]], 
                     fDocument);
    
	}
	board[toSquare] = piece;

	//
	// Record the move made in undo buffer
	//
	[fMoves addObject:move];
    
    //
    // Is the move a check?
    //
    if (fVariant != kVarSuicide) {
        MBCMoveGenerator	checkChecker(nil, fVariant, 0);
        if ((move->fCheck = checkChecker.InCheck(!([fMoves count] & 1), fCurPos)))
            move->fCheckMate = checkChecker.InCheckMate(!([fMoves count] & 1), fCurPos);
    }
    
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

- (MBCSide)sideOfMove:(MBCMove *)move
{
    switch (move->fCommand) {
    case kCmdWhiteWins:
        return kWhiteSide;
    case kCmdBlackWins:
        return kBlackSide;
    case kCmdDraw:
        return ([fMoves count] & 1) ? kWhiteSide : kBlackSide;
    default:
        return Color(move->fPiece)==kWhitePiece ? kWhiteSide : kBlackSide;
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
	snprintf(p, 32, " %d %lu", fMoveClock, ([fMoves count]/2)+1);

	return [NSString stringWithUTF8String:pos+1];
}

- (NSString *) holding
{
	char 	pos[128];
	char * 	p	= pos;

	*p++ = '[';
	for (MBCPiece piece = White(KING); piece <= Black(PAWN); ++piece) {
		for (int count = fCurPos.fInHand[piece]; count-- > 0; )
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
		[self resetWithVariant:fVariant];
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
	fPrvPos = fCurPos;

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
	return [fMoves count] ? [fMoves lastObject] : nil;
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

- (MBCMoveCode) outcome
{
    if (![fMoves count])
        return kCmdNull;
    
    MBCMove * lastMove = (MBCMove *)[fMoves lastObject];
    if (lastMove->fCheckMate) {
        if (fVariant == kVarLosers)
            return ([fMoves count] & 1) ? kCmdBlackWins : kCmdWhiteWins;
        else
            return ([fMoves count] & 1) ? kCmdWhiteWins : kCmdBlackWins;
    }
    if (fVariant == kVarSuicide || fVariant == kVarLosers) {
        if (!lastMove->fVictim) 
            return kCmdNull;
        MBCPiece color = Opposite(Color(lastMove->fVictim));
        if (fVariant == kVarSuicide && !fCurPos.fInHand[color+KING])
            return kCmdNull;
        if (fCurPos.NoPieces(Color(lastMove->fVictim))) 
            return ([fMoves count] & 1) ? kCmdBlackWins : kCmdWhiteWins;
    }
    if (fVariant != kVarSuicide) {
        MBCMoveGenerator	checkChecker(nil, fVariant, 0);
        if (checkChecker.InStaleMate(!([fMoves count] & 1), fCurPos))
            return fVariant != kVarLosers ? kCmdDraw
            : ([fMoves count] & 1) ? kCmdBlackWins : kCmdWhiteWins;
        if (fCurPos.NoPieces(kWhitePiece) && fCurPos.NoPieces(kBlackPiece))
            return kCmdDraw;
    }
    
    return kCmdNull;
}

NSString * LocalizedString(NSDictionary * localization, NSString * key, NSString * fallback)
{
	NSString * value = [[localization valueForKey:@"strings"] valueForKey:key];
    
	return value ? value : fallback;
}

NSString * LocalizedStringWithFormat(NSDictionary * localization, NSString * format, ...)
{
    va_list args;
    va_start(args, format);
    NSString * s = [[NSString alloc] initWithFormat:format locale:[localization valueForKey:@"locale"]
                                          arguments:args];
    va_end(args);
    
    return [s autorelease];
}

BOOL OldSquares(NSString * fmtString)
{
	/* We used to specify squares as "%c %d", now we use "%@ %@". To avoid
     breakage during the transition, we allow both 
     */
	NSRange r = [fmtString rangeOfString:@"%c"];
	if (r.length)
		return YES;
	r = [fmtString rangeOfString:@"$c"];
	if (r.length)
		return YES;	
    
	return NO;
}

static NSString *	sPieceName[] = {
	@"", @"king", @"queen", @"bishop", @"knight", @"rook", @"pawn"
};

static NSString * 	sFileKey[] = {
	@"file_a", @"file_b", @"file_c", @"file_d", @"file_e", @"file_f", @"file_g", @"file_h"
};

static NSString * 	sFileDefault[] = {
	@"A", @"B", @"C", @"D", @"E", @"F", @"G", @"H"
};

#define LOC_FILE(f) LOC(sFileKey[(f)-'a'], sFileDefault[(f)-'a'])

static NSString * 	sRankKey[] = {
	@"rank_1", @"rank_2", @"rank_3", @"rank_4", @"rank_5", @"rank_6", @"rank_7", @"rank_8"
};

static NSString * 	sRankDefault[] = {
	@"1", @"2", @"3", @"4", @"5", @"6", @"7", @"8"
};

#define LOC_RANK(r) LOC(sRankKey[(r)-1], sRankDefault[(r)-1])

- (NSString *)stringFromMove:(MBCMove *)move withLocalization:(NSDictionary *)localization
{
    switch (move->fCommand) {
    case kCmdDrop: {
        NSString * format  	= LOC(@"drop_fmt", @"%@ %c %d.");
        NSString * pkey 	= [NSString stringWithFormat:@"%@_d", sPieceName[Piece(move->fPiece)]];
        NSString * pdef 	= [NSString stringWithFormat:@"drop @%@", sPieceName[Piece(move->fPiece)]];
        NSString * ploc 	= LOC(pkey, pdef);
        char	   col  	= Col(move->fToSquare);
        int		   row  	= Row(move->fToSquare);
        if (OldSquares(format)) 
            return LocalizedStringWithFormat(localization, format, ploc, toupper(col), row);
        else
            return LocalizedStringWithFormat(localization, format, ploc, LOC_FILE(col), LOC_RANK(row));
    }
    case kCmdPMove:
    case kCmdMove: {
        MBCPiece	piece;
        MBCPiece	victim;
        MBCPiece	promo;

        if (move->fCastling == kUnknownCastle)
            move->fCastling = [self tryCastling:move];
        switch (move->fCastling) {
        case kCastleQueenside:
            return LOC(@"qcastle_fmt", @"Castle [[emph +]]queen side.");
        case kCastleKingside:
            return LOC(@"kcastle_fmt", @"Castle [[emph +]]king side.");
        default: 
            if (move->fPiece) { // Move already executed
                piece 	= move->fPiece;
                victim	= move->fVictim;
            } else {
                piece = fCurPos.fBoard[move->fFromSquare];
                victim= fCurPos.fBoard[move->fToSquare];
            }
            promo	= move->fPromotion;
            NSString * pname = LOC(sPieceName[Piece(piece)], sPieceName[Piece(piece)]);
            char	   fcol  = Col(move->fFromSquare);
            int	   frow  = Row(move->fFromSquare);
            char	   tcol  = Col(move->fToSquare);
            int	   trow  = Row(move->fToSquare);
            if (promo) {
                NSString * format = victim
                ? LOC(@"cpromo_fmt", @"%@ %c %d takes %c %d %@.")
                : LOC(@"promo_fmt", @"%@ %c %d to %c %d %@.");
                NSString * pkey  = [NSString stringWithFormat:@"%@_p", sPieceName[Piece(promo)]];
                NSString * pdef  = [NSString stringWithFormat:@"promoting to %@", sPieceName[Piece(promo)]];
                NSString * ploc  = LOC(pkey, pdef);
                
                if (OldSquares(format))
                    return LocalizedStringWithFormat(localization, format, pname,
                            toupper(fcol), frow, toupper(tcol), trow, 
                            ploc);
                else
                    return LocalizedStringWithFormat(localization, format, pname,
                            LOC_FILE(fcol), LOC_RANK(frow), LOC_FILE(tcol), LOC_RANK(trow),
                            ploc);
            } else {
                NSString * format = victim
                ? LOC(@"cmove_fmt", @"%@ %c %d takes %c %d.")
                : LOC(@"move_fmt", @"%@ %c %d to %c %d.");
                
               if (OldSquares(format))
                    return LocalizedStringWithFormat(localization, format, pname,
                            toupper(fcol), frow, toupper(tcol), trow);
                else
                    return LocalizedStringWithFormat(localization, format, pname,
                            LOC_FILE(fcol), LOC_RANK(frow), LOC_FILE(tcol), LOC_RANK(trow));
            }
        }}
    case kCmdWhiteWins:
        switch (fVariant) {
        default:
            if ([fMoves count] && ((MBCMove *)[fMoves lastObject])->fCheckMate)
                return LOC(@"check_mate", @"[[emph +]]Check mate!");
            //
            // Fall through
            //
        case kVarSuicide:
        case kVarLosers:
            return LOC(@"white_win", @"White wins!");
        }
    case kCmdBlackWins:
        switch (fVariant) {
        default:
            if ([fMoves count] && ((MBCMove *)[fMoves lastObject])->fCheckMate)
                return LOC(@"check_mate", @"[[emph +]]Check mate!");
            //
            // Fall through
            //
        case kVarSuicide:
        case kVarLosers:
            return LOC(@"black_win", @"Black wins!");
        }
    case kCmdDraw:
        return LOC(@"draw", @"The game is a draw!");
    default:
        return @"";
	}
}

- (NSString *)extStringFromMove:(MBCMove *)move withLocalization:(NSDictionary *)localization
{
    NSString * basic = [self stringFromMove:move withLocalization:localization];
    NSString * ext;
    
    if (move->fCheck || move->fCheckMate) {
        NSString * fmt  = LOC(@"has_check_fmt", @"%@, %@");
        NSString * check= move->fCheckMate ? LOC(@"check_mate", @"check mate!") : LOC(@"check", @"check!");
        
        ext = LocalizedStringWithFormat(localization, fmt, basic, check);
    } else {
        NSString * fmt  = LOC(@"no_check_fmt", @"%@.");
        
        ext = LocalizedStringWithFormat(localization, fmt, basic);
    }
    
    static NSRegularExpression * sFilter;
    if (!sFilter)
        sFilter = [[NSRegularExpression alloc] initWithPattern:@"\\[\\[.*?\\]\\]" options:0 error:nil];
    return [sFilter stringByReplacingMatchesInString:ext options:0 
                                               range:NSMakeRange(0, [ext length]) withTemplate:@""];
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
