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

#include "MBCMoveGenerator.h"
#include <algorithm>

@implementation MBCMoveCounter

- (int)count
{
	return fCount;
}

- (void) startMoveList:(BOOL)white
{
	fCount 		= 0;
	fCounting	= true;
}

- (void) startUnambiguousMoves
{
	fCounting = false;
}

- (void) endMoveList
{
}


- (void) validMove:(MBCPiece)piece from:(MBCSquare)from to:(MBCSquare)to
{
    fCount += fCounting;
}

- (void) validMove:(MBCPiece)piece from:(MBCSquare)from to:(MBCSquare)to 
  capturing:(MBCPiece) victim
{
	fCount += fCounting;
}

- (void) validDrop:(MBCPiece)piece at:(MBCSquare)at
{
    fCount += fCounting;
}

- (void) validCastle:(MBCPiece)king kingSide:(BOOL)kingSide;
{
    fCount += fCounting;
}

@end

@implementation MBCCheckCounter

- (id)initForWhite:(BOOL)white variant:(MBCVariant)variant position:(const MBCPieces *)pos canCastle:(BOOL)canCastle
{
    if (self = [super init]) {
        fWhite     = white;
        fCanCastle = canCastle;
        fGenerator = new MBCMoveGenerator(nil, variant, 0);
        fPosition  = *pos;
    }
    
    return self;
}

- (void)dealloc
{
    delete fGenerator;
    [super dealloc];
}

- (void) validMove:(MBCPiece)piece from:(MBCSquare)from to:(MBCSquare)to
{
    if (fCounting) {
        MBCPieces newPos    = fPosition;
        newPos.fBoard[to]   = newPos.fBoard[from];
        newPos.fBoard[from] = EMPTY;
        
        if (!fGenerator->InCheck(fWhite, newPos))
            fCount += 1;
    }
}

- (void) validMove:(MBCPiece)piece from:(MBCSquare)from to:(MBCSquare)to 
         capturing:(MBCPiece) victim
{
	[self validMove:piece from:from to:to];
}

- (void) validDrop:(MBCPiece)piece at:(MBCSquare)at
{
    if (fCounting) {
        MBCPieces newPos    = fPosition;
        newPos.fBoard[at]   = piece;
        
        if (!fGenerator->InCheck(fWhite, newPos))
            fCount += 1;
    }
}

- (void) validCastle:(MBCPiece)king kingSide:(BOOL)kingSide;
{
    if (fCounting && fCanCastle) {
        MBCPieces newPos    = fPosition;
        char      kingOrig  = 'e';
        char      kingDest  = kingSide ? 'g' : 'c';
        char      rookOrig  = kingSide ? 'h' : 'a';
        char      rookDest  = kingSide ? 'f' : 'd';
        unsigned  rank      = fWhite ? 1 : 8;
        std::swap(newPos.fBoard[Square(kingOrig, rank)], newPos.fBoard[Square(kingDest, rank)]);
        std::swap(newPos.fBoard[Square(rookOrig, rank)], newPos.fBoard[Square(rookDest, rank)]);
        
        if (!fGenerator->InCheck(fWhite, newPos))
            fCount += 1;
    }
}

@end

@implementation MBCDebugMoveBuilder

+ (id)debugMoveBuilder
{
	return [[[MBCDebugMoveBuilder alloc] init] autorelease];
}

- (void) startMoveList:(BOOL)white
{
	fUnambiguous		= false;
	fMoves				= [[NSMutableArray alloc] init];
	fUnambiguousMoves 	= [[NSMutableArray alloc] init];
	fDrops				= [[NSMutableArray alloc] init];
}

- (void) startUnambiguousMoves
{
	fUnambiguous		= true;
}

- (void) endMoveList
{
	NSLog(@"Moves:       %@\n", 
		  [fMoves componentsJoinedByString:@" "]);
	NSLog(@"Unambiguous: %@\n", 
		  [fUnambiguousMoves componentsJoinedByString:@" "]);
	if ([fDrops count])
		NSLog(@"Drops:       %@\n", 
			  [fDrops componentsJoinedByString:@" "]);
}

const char * sPieces = " KQBNRP";

- (void) validMove:(MBCPiece)piece from:(MBCSquare)from to:(MBCSquare)to
{
	if (fUnambiguous)
		[fUnambiguousMoves 
			addObject: [NSString stringWithFormat:@"%c%c%d",
								 sPieces[piece], 
								 Col(to), Row(to)]];
	else
		[fMoves 
			addObject: [NSString stringWithFormat:@"%c%c%d-%c%d",
								 sPieces[piece], 
								 Col(from), Row(from),
								 Col(to), Row(to)]];
}

- (void) validMove:(MBCPiece)piece from:(MBCSquare)from to:(MBCSquare)to 
	capturing:(MBCPiece)victim
{
	if (fUnambiguous)
		[fUnambiguousMoves 
			addObject: [NSString stringWithFormat:@"%cx%c%d",
								 sPieces[piece], 
								 Col(to), Row(to)]];
	else
		[fMoves 
			addObject: [NSString stringWithFormat:@"%c%c%dx%c%d",
								 sPieces[piece], 
								 Col(from), Row(from),
								 Col(to), Row(to)]];
}

- (void) validDrop:(MBCPiece)piece at:(MBCSquare)at
{
	[fDrops addObject: [NSString stringWithFormat:@"%c@%c%d",
								 sPieces[piece], 
								 Col(at), Row(at)]];	
}

- (void) validCastle:(MBCPiece)king kingSide:(BOOL)kingSide;
{
	[(fUnambiguous ? fUnambiguousMoves : fMoves)
		addObject:kingSide ? @"O-O" : @"O-O-O"];
}

@end

void MBCMoveCollection::AddMove(
		bool unambig, MBCPiece piece, MBCSquare from, MBCSquare to)
{
	MBCPieceMoves & moves = 
		(unambig ? fUnambiguousMoves : fMoves)[Piece(piece)];
	int instance;
	for (instance = 0; instance < moves.fNumInstances; ++instance)
		if (moves.fFrom[instance] == from)
			goto foundInstance;
	moves.fFrom[moves.fNumInstances++] = from;
 foundInstance:
	moves.fTo[instance] |= (1llu << to);
}

void MBCMoveCollection::AddDrop(MBCPiece piece, MBCSquare at)
{
	if ((piece = MBCPiece(piece)) == PAWN) {
		fPawnDrops			|= (1llu << at);
	} else {	
		fPieceDrops 		|= (1llu << at);
		fDroppablePieces	|= (1 << piece);
	}
}

void MBCMoveCollection::AddCastle(bool kingSide)
{
	if (kingSide)
		fCastleKingside = true;
	else
		fCastleQueenside = true;
}

@implementation MBCMoveCollector

- (MBCMoveCollection *) collection
{
	return &fCollection;
}

- (void) startMoveList:(BOOL)white
{
	fUnambiguous				= false;
	memset(&fCollection, 0, sizeof(MBCMoveCollection));
	fCollection.fWhiteMoves		= white;
}

- (void) startUnambiguousMoves
{
	fUnambiguous	= true;
}

- (void) endMoveList
{
}

- (void) validMove:(MBCPiece)piece from:(MBCSquare)from to:(MBCSquare)to
{
	fCollection.AddMove(fUnambiguous, piece, from, to);
}

- (void) validMove:(MBCPiece)piece from:(MBCSquare)from to:(MBCSquare)to
	capturing:(MBCPiece)victim
{
	fCollection.AddMove(fUnambiguous, piece, from, to);
}

- (void) validDrop:(MBCPiece)piece at:(MBCSquare)at
{
	fCollection.AddDrop(piece, at);
}

- (void) validCastle:(MBCPiece)king kingSide:(BOOL)kingSide
{
	fCollection.AddCastle(kingSide);
}

@end

MBCMoveGenerator::MBCMoveGenerator(id <MBCMoveBuilder> builder, 
								   MBCVariant variant,
								   long flags)
	: fBuilder(builder), fFlags(flags), fVariant(variant),
	 fPieceFilter(EMPTY), fTargetFilter(kInvalidSquare)
{
}

void MBCMoveGenerator::SetVariant(MBCVariant variant)
{
	fVariant = variant;
}

void MBCMoveGenerator::Generate(bool white, const MBCPieces & position)
{
	[fBuilder startMoveList:white];
	
	fColor		=	white ? kWhitePiece : kBlackPiece;
	fPosition	=	&position;	

	memset(fTargetUsed, 0, 64*sizeof(uint8_t));
	memset(fTargetAmbiguous, 0, 64*sizeof(uint8_t));
	
	TryMoves(false);

	[fBuilder startUnambiguousMoves];

	TryMoves(true);

	[fBuilder endMoveList];
}

void MBCMoveGenerator::Ambiguities(MBCSquare from, MBCSquare to, 
								   const MBCPieces & position)
{
	MBCPiece p	= position.fBoard[from];
	
	fPieceFilter	= What(p);
	fTargetFilter	= to;
	
	Generate(Color(p) == kWhitePiece, position);

	fPieceFilter	= EMPTY;
	fTargetFilter	= kInvalidSquare;
}

bool MBCMoveGenerator::InCheck(bool white, const MBCPieces & position)
{
    if (fVariant == kVarSuicide)
        return false;
    
	id 					saveBuilder = fBuilder;
	MBCMoveCounter *	counter		= [[MBCMoveCounter alloc] init];
	MBCPiece 			king		= (white?kWhitePiece:kBlackPiece) | KING;

	fBuilder		= counter;

	for (MBCSquare i = Square('a', 1); i<=Square('h', 8); ++i)
		if (What(position.fBoard[i]) == king) {
			fTargetFilter	= i;
			Generate(!white, position);
			fTargetFilter 	= kInvalidSquare;
			
			break;
		}

	bool res = [counter count] > 0;
	fBuilder = saveBuilder;
	[counter release];

	return res;
}

bool MBCMoveGenerator::InCheckMate(bool white, const MBCPieces & position)
{
    if (!InCheck(white, position))
        return NO;
    
	id 					saveBuilder   = fBuilder;
	MBCMoveCounter *	counter		  = 
        [[MBCCheckCounter alloc] initForWhite:white variant:fVariant position:&position canCastle:NO];
	fBuilder                        = counter;
    
	Generate(white, position);
    
	bool res = ![counter count];
	fBuilder = saveBuilder;
	[counter release];
    
	return res;
}

bool MBCMoveGenerator::InStaleMate(bool white, const MBCPieces & position)
{
    if (InCheck(white, position))
        return NO;
    
	id 					saveBuilder   = fBuilder;
	MBCMoveCounter *	counter		  = 
        [[MBCCheckCounter alloc] initForWhite:white variant:fVariant position:&position canCastle:YES];
	fBuilder                        = counter;
    
	Generate(white, position);
    
	bool res = ![counter count];
	fBuilder = saveBuilder;
	[counter release];
    
	return res;
}

void MBCMoveGenerator::TryMoves(bool unambiguous)
{
	fUnambiguous	= unambiguous;
	
	for (MBCSquare i = Square('a', 1); i<=Square('h', 8); ++i) {
		MBCPiece piece = fPosition->fBoard[i];
		if (fPieceFilter ? What(piece) == fPieceFilter 
			: (piece && Color(piece) == fColor)
		)
			TryMoves(Piece(piece), i);
	}
	if (fTargetFilter == kInvalidSquare) {
		if (fVariant != kVarSuicide)
			TryCastle();
		if (fVariant == kVarCrazyhouse)
			TryDrops();
	}
}

void MBCMoveGenerator::TryMoves(MBCPiece piece, MBCSquare from)
{
	switch (piece) {
	case PAWN: {
		int 	 dir = fColor == kWhitePiece ? 1 : -1;
		unsigned orig= fColor == kWhitePiece ? 2 : 7;
		
		if (TryMove(piece, from, 0, dir)	// Single step always permitted
		 && Row(from) == orig				// How about a double step?
		)
			TryMove(piece, from, 0, 2*dir);// Double step
		TryMove(piece, from, -1, dir);		// Capture left
		TryMove(piece, from,  1, dir);		// Capture right
		break; }
	case ROOK:
		TryMoves(piece, from,  1,  0);
		TryMoves(piece, from, -1,  0);
		TryMoves(piece, from,  0,  1);
		TryMoves(piece, from,  0, -1);
		break;
	case KNIGHT:
		TryMove(piece, from,  1,  2);
		TryMove(piece, from,  2,  1);
		TryMove(piece, from,  2, -1);
		TryMove(piece, from,  1, -2);
		TryMove(piece, from, -1, -2);
		TryMove(piece, from, -2, -1);
		TryMove(piece, from, -2,  1);
		TryMove(piece, from, -1,  2);
		break;
	case BISHOP:
		TryMoves(piece, from,  1,  1);
		TryMoves(piece, from,  1, -1);
		TryMoves(piece, from, -1, -1);
		TryMoves(piece, from, -1,  1);
		break;
	case QUEEN:
		TryMoves(piece, from,  1,  0);
		TryMoves(piece, from, -1,  0);
		TryMoves(piece, from,  0,  1);
		TryMoves(piece, from,  0, -1);
		TryMoves(piece, from,  1,  1);
		TryMoves(piece, from,  1, -1);
		TryMoves(piece, from, -1, -1);
		TryMoves(piece, from, -1,  1);
		break;
	case KING:
		TryMove(piece, from,  1,  0);
		TryMove(piece, from, -1,  0);
		TryMove(piece, from,  0,  1);
		TryMove(piece, from,  0, -1);
		TryMove(piece, from,  1,  1);
		TryMove(piece, from,  1, -1);
		TryMove(piece, from, -1, -1);
		TryMove(piece, from, -1,  1);

		break;
	}
}

void MBCMoveGenerator::TryMoves(MBCPiece piece, MBCSquare from, 
								int dCol, int dRow)
{
	int dc	= 0;
	int dr	= 0;

	while (TryMove(piece, from, (dc += dCol), (dr += dRow)))
		;
}

bool MBCMoveGenerator::TryMove(MBCPiece piece, MBCSquare from,
							   int dCol, int dRow)
{
	char col	= Col(from)+dCol;
	int	 row	= Row(from)+dRow;

	if (col < 'a' || col > 'h' || row < 1 || row > 8)
		return false;
	else
		return TryMove(piece, from, Square(col, row));
}

bool MBCMoveGenerator::TryMove(MBCPiece piece, MBCSquare from, MBCSquare to)
{
	MBCPiece victim = fPosition->fBoard[to];

	if (fTargetFilter != kInvalidSquare && to != fTargetFilter)
		return !victim; // Try again if square was clear

	if (victim && Color(victim) == fColor)
		return false;	// Field is blocked by own piece
	
	if (piece == PAWN) // Pawns move straight, capture diagonally
		if (Col(from) != Col(to)) { // Attempted capture
			if (!victim) // Field is empty, try en passant
				if (fPosition->fEnPassant == to) // Yup
					victim = Opposite(fColor) | PAWN;
				else
					return false;
		} else if (victim) // Straight move is blocked
			return false;
	
	uint8_t pieceMask = 1 << piece;
	
	if (fUnambiguous) {
		// 
		// Simplify language model
		//
		if (fTargetAmbiguous[to] & pieceMask) // Amiguous move, don't do it
			return !victim; // Don't move further after capture
	} else {
		fTargetAmbiguous[to] |= fTargetUsed[to] & pieceMask;
		fTargetUsed[to]      |= pieceMask;
	}
	if (victim)
		[fBuilder validMove:piece from:from to:to capturing:victim];
	else 
		[fBuilder validMove:piece from:from to:to];
	
	return !victim;	// Don't move further after capture
}

void MBCMoveGenerator::TryCastle()
{
	int row	= fColor == kWhitePiece ? 1 : 8;

	MBCPiece  king			= fColor | KING;
	MBCPiece  rook			= fColor | ROOK;
	MBCSquare kingPos 		= Square('e', row);
	MBCSquare kingRookPos	= Square('h', row);
	MBCSquare queenRookPos	= Square('a', row);

	if (fPosition->fBoard[kingPos] != king) // King moved
		return;
	
	bool kingSide = fPosition->fBoard[kingRookPos]==rook 
		&& !fPosition->fBoard[Square('g', row)]
		&& !fPosition->fBoard[Square('f', row)];
	bool queenSide= fPosition->fBoard[queenRookPos]==rook 
		&& !fPosition->fBoard[Square('b', row)]
		&& !fPosition->fBoard[Square('c', row)]
		&& !fPosition->fBoard[Square('d', row)];

	if (fUnambiguous && kingSide && queenSide)
		return;

	if (kingSide)
		[fBuilder validCastle:king kingSide:YES];
	if (queenSide)
		[fBuilder validCastle:king kingSide:NO];
}

void MBCMoveGenerator::TryDrops()
{
	for (MBCPiece p = QUEEN; p <= PAWN; ++p) {
		MBCPiece piece = fColor | p;
		int	pawn	   = p==PAWN;
		if (!fPosition->fInHand[piece])
			continue;
		for (MBCSquare i = Square('a', 1+pawn); i <= Square('h', 8-pawn); ++i)
			if (!fPosition->fBoard[i])
				[fBuilder validDrop:piece at:i];
	}
}

// Local Variables:
// mode:ObjC
// End:
