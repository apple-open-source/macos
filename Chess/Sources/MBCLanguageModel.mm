/*
	File:		MBCLanguageModel.mm
	Contains:	Build and interpret speech recognition language model
	Copyright:	© 2003-2011 by Apple Inc., all rights reserved.

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

#import "MBCLanguageModel.h"
#import "MBCEngineCommands.h"
#import "MBCDebug.h"

static const char sTakeback[] = "take back move";
static const char sUndo[]     = "undo";

static const char * sPieceName[] = {
	"", "king", "queen", "bishop", "knight", "rook", "pawn"
};

inline SRefCon SR(MBCCompactMove move)
{
	return SRefCon(move); 
}

@implementation MBCLanguageModel

- (void) addTo:(SRLanguageObject)base languageObject:(SRLanguageObject)add
{
	SRAddLanguageObject(base, add);
	SRReleaseObject(add);
}

- (id) initWithRecognitionSystem:(SRRecognitionSystem)system
{
	fSystem	= system;

	SRNewLanguageModel(fSystem, &fToModel, "to", 2);
	SRAddText(fToModel, "to", 2, 0);
	SRAddText(fToModel, "takes", 5, 0);

	SRNewPath(fSystem, &fPromotionModel);
	SRAddText(fPromotionModel, "promoting to", 12, 0);

	SRLanguageModel promoPieces;
	SRNewLanguageModel(fSystem, &promoPieces, "promotion", 9);
	SRAddText(promoPieces, "queen", 5, 	SR(MBCEncodeMove("a1a1q", 0)));
	SRAddText(promoPieces, "bishop", 6, SR(MBCEncodeMove("a1a1b", 0))); 
	SRAddText(promoPieces, "knight", 6, SR(MBCEncodeMove("a1a1n", 0))); 
	SRAddText(promoPieces, "rook", 4, 	SR(MBCEncodeMove("a1a1r", 0))); 
	SRAddText(promoPieces, "king", 4, 	SR(MBCEncodeMove("a1a1k", 0))); // Suicide
	[self addTo:fPromotionModel languageObject:promoPieces];

	Boolean opt = true;
	SRSetProperty(fPromotionModel, kSROptional, &opt, 1);

	return self;
}

- (SRLanguageObject) movesFrom:(MBCSquare)from to:(MBCBoardMask)mask 
						  pawn:(BOOL)pawn
{
	char 	move[5];
	move[0]		= Col(from);
	move[1]		= '0'+Row(from);
	move[2]		= 0;
	move[4]		= 0;

	SRPath	model;
	SRNewPath(fSystem, &model);
	SRAddText(model, move, 2, 0);
	SRAddLanguageObject(model, fToModel);

	SRLanguageModel	destinations;
	NSString *		pieceDest = [NSString stringWithFormat:@"%s to", move];
	SRNewLanguageModel(fSystem, &destinations, 
					   [pieceDest UTF8String], [pieceDest length]);
	if (MBCDebug::DumpLanguageModels()) {
		const char * seenDest = "";
		fprintf(stderr, "<%c%dmoves> =", Col(from), Row(from));
		for (MBCSquare to = Square("a1"); to<=Square("h8"); ++to)
			if (mask & (1llu << to)) {
				fprintf(stderr, " %s%c%d%s", seenDest, Col(to), Row(to),
						(pawn && (Row(to)==1 || Row(to)==8)) 
						? " <promotion>" : "");
				seenDest = "| ";
			}
		fprintf(stderr, " ;\n");
	}
	for (MBCSquare to = Square("a1"); to<=Square("h8"); ++to)
		if (mask & (1llu << to)) {
			move[2]	= Col(to);
			move[3]	= '0'+Row(to);
			if (pawn && (Row(to)==1 || Row(to)==8)) {
				SRPath	path;
				SRNewPath(fSystem, &path);
				SRAddText(path, move+2, 2, SR(MBCEncodeMove(move, 0)));
				SRAddLanguageObject(path, fPromotionModel);
				[self addTo:destinations languageObject:path];
			} else {
				SRAddText(destinations, move+2, 2, SR(MBCEncodeMove(move, 0)));
			}
		}
	[self addTo:model languageObject:destinations];

	return model;
}

- (SRLanguageObject) movesForPiece:(MBCPiece)piece
{
	SRPath			model;
	SRNewPath(fSystem, &model);
	SRAddText(model, sPieceName[piece], strlen(sPieceName[piece]), 0);

	SRLanguageModel	origins;
	NSString *		pieceOrigins = [NSString stringWithFormat:@"%s from",
											 sPieceName[piece]];
	SRNewLanguageModel(fSystem, &origins, 
					   [pieceOrigins UTF8String], [pieceOrigins length]);

	MBCPieceMoves *	moves	= fMoves->fMoves+piece;
	if (MBCDebug::DumpLanguageModels()) {
		fprintf(stderr, "<%smoves> = \n", sPieceName[piece]);
		for (int i = 0; i<moves->fNumInstances; ++i)
			fprintf(stderr, " %c %c%d <to> <%c%dmoves>\n", 
					i ? '|' : ' ', Col(moves->fFrom[i]), Row(moves->fFrom[i]),
					Col(moves->fFrom[i]), Row(moves->fFrom[i]));
		fprintf(stderr, " ;\n");
	}
	for (int i = 0; i<moves->fNumInstances; ++i)
		[self addTo:origins
			  languageObject:[self movesFrom:moves->fFrom[i] to:moves->fTo[i] 
								   pawn:piece==PAWN]];
	[self addTo:model languageObject:origins];
	
	return model;
}

- (SRLanguageObject) castles
{
	char move[5];
	MBCSquare king = fMoves->fMoves[KING].fFrom[0];
	move[0] = Col(king);
	move[1] = '0'+Row(king);
	move[3] = '0'+Row(king);
	move[4] = 0;
	
	if (MBCDebug::DumpLanguageModels()) {
		fprintf(stderr, "<castles> = castle <castlesides> side ;\n");
		fprintf(stderr, "<castlesides> =%s%s ;\n",
				fMoves->fCastleKingside ? " king" : "",
				fMoves->fCastleQueenside ? " queen" : "");
	}

	SRPath			model;
	SRNewPath(fSystem, &model);
	SRAddText(model, "castle", 6, 0);	

	SRLanguageModel	sides;
	SRNewLanguageModel(fSystem, &sides, "castle", 6); 

	if (fMoves->fCastleKingside) {
		move[2]	= 'g';
		SRAddText(sides, "king", 4, SR(MBCEncodeMove(move, 0)));
	}
	if (fMoves->fCastleQueenside) {
		move[2]	= 'c';
		SRAddText(sides, "queen", 5, SR(MBCEncodeMove(move, 0)));
	}
   
	[self addTo:model languageObject:sides];
	SRAddText(model, "side", 4, 0);

	return model;
}

- (SRLanguageObject) pawnDrops
{
	char drop[5];
	drop[0] = fMoves->fWhiteMoves ? 'P' : 'p';
	drop[1] = '@';
	drop[4] = 0;
	
	if (MBCDebug::DumpLanguageModels()) {	
		const char *	seenDest = "";
		fprintf(stderr, "<pawndrops> = drop pawn at <pawnat> ;\n");
		fprintf(stderr, "<pawnat> =");
		for (MBCSquare to = Square("a1"); to<=Square("h8"); ++to)
			if (fMoves->fPawnDrops & (1llu << to)) {
				fprintf(stderr, " %s%c%d", seenDest, Col(to), Row(to));
				seenDest = "| ";
			}
		fprintf(stderr, " ;\n");
	}

	SRPath			model;
	SRNewPath(fSystem, &model);
	SRAddText(model, "drop pawn at", 12, 0);	

	SRLanguageModel	destinations;
	SRNewLanguageModel(fSystem, &destinations, "pawnat", 6); 
   
	for (MBCSquare to = Square("a1"); to<=Square("h8"); ++to)
		if (fMoves->fPawnDrops & (1llu << to)) {
			drop[2]	= Col(to);
			drop[3]	= '0'+Row(to);
			SRAddText(destinations, drop+2, 2, SR(MBCEncodeDrop(drop, 0)));
		}
	[self addTo:model languageObject:destinations];

	return model;
}

- (SRLanguageObject) pieceDrops
{
	const char * 	pieceSym	= " KQBNRP  kqbnrp ";
	int				color 		= fMoves->fWhiteMoves ? 0 : 8;
	char drop[5];
	
	if (MBCDebug::DumpLanguageModels()) {	
		const char *	seenPiece= "";
		const char *	seenDest = "";
		fprintf(stderr, "<piecedrops> = drop <droppiece> at <pieceat> ;\n");
		fprintf(stderr, "<droppiece> = ");
		for (MBCPiece piece = QUEEN; piece < PAWN; ++piece)
			if (fMoves->fDroppablePieces & (1 << piece)) {
				fprintf(stderr, " %s%s", seenPiece, sPieceName[piece]);
				seenPiece = "| ";
			}
		fprintf(stderr, "<pieceat> =");
		for (MBCSquare to = Square("a1"); to<=Square("h8"); ++to)
			if (fMoves->fPieceDrops & (1llu << to)) {
				fprintf(stderr, " %s%c%d", seenDest, Col(to), Row(to));
				seenDest = "| ";
			}
		fprintf(stderr, " ;\n");
	}

	SRPath			model;
	SRNewPath(fSystem, &model);
	SRAddText(model, "drop", 4, 0);	

	SRLanguageModel	pieces;
	SRNewLanguageModel(fSystem, &pieces, "droppiece", 9); 

	strcpy(drop, " @a1");
	for (MBCPiece piece = QUEEN; piece < PAWN; ++piece)
		if (fMoves->fDroppablePieces & (1 << piece)) {
			drop[0] = pieceSym[piece | color];
			SRAddText(pieces, sPieceName[piece], strlen(sPieceName[piece]),
					  SR(MBCEncodeDrop(drop, 0)));
		}

	SRLanguageModel	destinations;
	SRNewLanguageModel(fSystem, &destinations, "pieceat", 7);    

	strcpy(drop, " @a1");
	for (MBCSquare to = Square("a1"); to<=Square("h8"); ++to)
		if (fMoves->fPawnDrops & (1llu << to)) {
			drop[2]	= Col(to);
			drop[3]	= '0'+Row(to);
			SRAddText(destinations, drop+2, 2, SR(MBCEncodeDrop(drop, 0)));
		}

	[self addTo:model languageObject:pieces];
	SRAddText(model, "at", 2, 0);		
	[self addTo:model languageObject:destinations];

	return model;
}

- (void) buildLanguageModel:(SRLanguageModel)model 
				  fromMoves:(MBCMoveCollection *)moves
				   takeback:(BOOL)takeback
{
	if (MBCDebug::DumpLanguageModels()) {
		fprintf(stderr, "<to> = to | takes;\n");
		fprintf(stderr, "<promotion> = promoting to <promoPiece>;\n");
		fprintf(stderr, "<promoPiece> = queen | bishop | knight | rook | king;\n"); 
	}

	fMoves		= moves;
	SREmptyLanguageObject(model);
	
	if (MBCDebug::DumpLanguageModels()) {
		bool	seenPiece = false;

		fprintf(stderr, "\n<moves> = \n");
		for (MBCPiece piece = KING; piece <= PAWN; ++piece)
			if (moves->fMoves[piece].fNumInstances) {
				fprintf(stderr, " %c %s <%smoves>\n", seenPiece ? '|' : ' ',
						sPieceName[piece], sPieceName[piece]);
				seenPiece = true;
			}
		fprintf(stderr, " ;\n");
	}
	for (MBCPiece piece = KING; piece <= PAWN; ++piece)
		if (moves->fMoves[piece].fNumInstances)
			[self addTo:model languageObject:[self movesForPiece:piece]];
	if (moves->fCastleKingside || moves->fCastleQueenside)
		[self addTo:model languageObject:[self castles]];

	if (moves->fPawnDrops)
		[self addTo:model languageObject:[self pawnDrops]];
	if (moves->fPieceDrops)
		[self addTo:model languageObject:[self pieceDrops]];		

	if (takeback) {
		MBCCompactMove takeback = MBCEncodeTakeback();

		SRAddText(model, sTakeback, strlen(sTakeback), 	SR(takeback));
		SRAddText(model, sUndo, 	strlen(sUndo), 		SR(takeback));
	}
}

- (MBCMove *) recognizedMove:(SRRecognitionResult)result
{
	SRPath	resultPhrase;
	Size 	len = sizeof(resultPhrase);

	len = sizeof(resultPhrase);
	if (!SRGetProperty (result, kSRPathFormat, &resultPhrase, &len)) {
		MBCCompactMove	move	= 0;
		long			numWords;
		SRCountItems(resultPhrase, &numWords);

		//
		// We build up our move by ORing the individual refCons, which neatly
		// combines the semantic content of the contributing elements
		//
		while (numWords--) {
			SRLanguageObject 	resultWord;
			MBCCompactMove		wordMove;

			SRGetIndexedItem(resultPhrase, &resultWord, numWords);
			len = 4;
			if (!SRGetProperty(resultWord, kSRRefCon, &wordMove, &len))
				move |= wordMove;
			
			SRReleaseObject(resultWord);
		}
		SRReleaseObject (resultPhrase);

		return [MBCMove moveFromCompactMove:move];
	}
	return nil;
}

@end

// Local Variables:
// mode:ObjC
// End:
