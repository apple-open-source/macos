#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include "ChessListener.h"

/*
 * Copied from gnuglue.h
 */

/* players */
enum {
    WHITE = 0,
    BLACK,
    NEUTRAL
};

/* pieces */
enum {
    NO_PIECE = 0,
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING
};

struct CLCoord {
	explicit CLCoord(int coord) : fCoord(coord), fCol(coord & 7), fRow(coord >> 3) {}
	explicit CLCoord(int column, int row) : fCol(column), fRow(row) 
		{	fCoord = (fRow|fCol) & 0xFFFFFFF8 ? -1 : (fRow << 3) | column; }
	explicit CLCoord(const char * name) : fCol(name[0]-'a'), fRow(name[1]-'1') 
		{	fCoord = (fRow|fCol) & 0xFFFFFFF8 ? -1 : (fRow << 3) | fCol; }
	
	int fCoord;
	int fCol;
        int fRow;
	
	char	ColLetter()	const { return 'a'+fCol; }
	char	RowLetter()	const { return '1'+fRow; }
};

class CLMoveBuilder {
public:
	virtual void StartMoveList();
	virtual void Move(int piece, const CLCoord & fromCoord, const CLCoord & toCoord, bool take, bool omitFrom) = 0;
	virtual void EndMoveList();
	
	virtual ~CLMoveBuilder();
};

void CLMoveBuilder::StartMoveList()
{
}

void CLMoveBuilder::EndMoveList()
{
}

CLMoveBuilder::~CLMoveBuilder()
{
}

class CLDebugMoveBuilder : public CLMoveBuilder {
public:
	CLDebugMoveBuilder(CLMoveBuilder * nextBuilder) : fNextBuilder(nextBuilder) {}
	
	virtual void StartMoveList();
	virtual void Move(int piece, const CLCoord & fromCoord, const CLCoord & toCoord, bool take, bool omitFrom);
	virtual void EndMoveList();
private:
	CLMoveBuilder * 	fNextBuilder;
};

void CLDebugMoveBuilder::Move(int piece, const CLCoord & fromCoord, const CLCoord & toCoord, bool take, bool omitFrom)
{
	static char * pieceName[] = {"", "Pawn", "Knight", "Bishop", "Rook", "Queen", "King"};

	if (omitFrom)
		return;
	if (piece == KING && fromCoord.fCol==4 && !(toCoord.fCol&1) && toCoord.fRow==fromCoord.fRow)
		if (toCoord.fCol==2)
			fprintf(stderr, "castle%s\n", omitFrom ? "" : " queen side");
		else
			fprintf(stderr, "castle%s\n", omitFrom ? "" : " king side");  
	else if (!omitFrom)
		fprintf(stderr, "[%s] %c%c %s %c%c\n", pieceName[piece], 
			fromCoord.ColLetter(), fromCoord.RowLetter(),
			(take ? "( takes | to )" : "to"),
			toCoord.ColLetter(), toCoord.RowLetter());
	else
		fprintf(stderr, "%s %s %c%c\n", pieceName[piece], 
			(take ? "( takes | to )" : "to"),
			toCoord.ColLetter(), toCoord.RowLetter());
	
	if (fNextBuilder)
		fNextBuilder->Move(piece, fromCoord, toCoord, take, omitFrom);
}

void CLDebugMoveBuilder::StartMoveList()
{
	fprintf(stderr, "----- Legal moves:\n");
	if (fNextBuilder)
		fNextBuilder->StartMoveList();
}

void CLDebugMoveBuilder::EndMoveList()
{
	fprintf(stderr, "-----\n");
	if (fNextBuilder)
		fNextBuilder->EndMoveList();
}

class CLSRMoveBuilder : public CLMoveBuilder {
public:
	CLSRMoveBuilder();
	~CLSRMoveBuilder();
	
	virtual void StartMoveList();
	virtual void Move(int piece, const CLCoord & fromCoord, const CLCoord & toCoord, bool take, bool omitFrom);
	virtual void EndMoveList();
	
	bool				OK()			{ return fOK;			}
	SRRecognitionSystem	RecSystem()		{ return fRecSystem;	}
	SRRecognizer		Recognizer()	{ return fRecognizer;	}
	void				StartListening();
	void				StopListening();
private:
	SRRecognitionSystem	fRecSystem;
	SRRecognizer		fRecognizer;
	SRLanguageModel		fModel;
	SRLanguageModel		fTakesModel;
	SRWord				fToModel;
	SRWord				fPieceModel[7];
	SRWord				fCastleModel;
	SRPhrase			fUndoModel;
	SRPhrase		    fKingSideModel;
	SRPhrase			fQueenSideModel;
	SRWord				fColModel[8];
	SRWord				fRowModel[8];
	bool				fOK;
	bool				fListening;
	
	void				MakeWord(SRWord * word, const char * text, bool optional);
	void				MakePhrase(SRPhrase * phrase, const char * text, bool optional);
	void				MakeAlt(SRLanguageModel * alt, const char * t1, const char * t2, bool optional);
	void				MakeHelp();
};

CLSRMoveBuilder::CLSRMoveBuilder()
{
	const char * SvA 	= getenv("CHESS_SPEED");
	unsigned short speed= SvA ? atoi(SvA) : 25;  
	long refCon 		= -1;

	fOK 		= false;
	fListening	= false;
	
	if (SROpenRecognitionSystem (&fRecSystem, kSRDefaultRecognitionSystemID))
		goto failRecSystem;
	if (SRNewRecognizer(fRecSystem, &fRecognizer, kSRDefaultSpeechSource))
		goto failRecognizer;
	SRSetProperty(fRecognizer, kSRSpeedVsAccuracyParam, &speed, sizeof(speed));

	SRNewLanguageModel(fRecSystem, &fModel, "<moves>", 7);

	MakeWord(&fToModel, 		"to", 		false);
	
	MakeWord(fPieceModel+1,		"pawn",		false);
	MakeWord(fPieceModel+2,		"knight",	false);
	MakeWord(fPieceModel+3,		"bishop",	false);
	MakeWord(fPieceModel+4,		"rook",		false);
	MakeWord(fPieceModel+5,		"queen",	false);
	MakeWord(fPieceModel+6,		"king",		false);
	
	MakeWord(&fCastleModel, "castle", false);
	MakePhrase(&fKingSideModel, "king side", false);
	MakePhrase(&fQueenSideModel, "queen side", false);
	MakePhrase(&fUndoModel, "take back move", false);
	SRSetProperty(fUndoModel, kSRRefCon, &refCon, sizeof(refCon));
	
	MakeWord(fColModel+0, "a", false);
	MakeWord(fColModel+1, "b", false);
	MakeWord(fColModel+2, "c", false);
	MakeWord(fColModel+3, "d", false);
	MakeWord(fColModel+4, "e", false);
	MakeWord(fColModel+5, "f", false);
	MakeWord(fColModel+6, "g", false);
	MakeWord(fColModel+7, "h", false);

	MakeWord(fRowModel+0, "1",   false);
	MakeWord(fRowModel+1, "2",   false);
	MakeWord(fRowModel+2, "3", false);
	MakeWord(fRowModel+3, "4",  false);
	MakeWord(fRowModel+4, "5",  false);
	MakeWord(fRowModel+5, "6",   false);
	MakeWord(fRowModel+6, "7", false);
	MakeWord(fRowModel+7, "8", false);
	
	MakeAlt(&fTakesModel, "takes", "to", false);
	MakeHelp();

	fOK = true;
	return;
failRecognizer:
	SRCloseRecognitionSystem(fRecSystem);
failRecSystem:
	;
}

CLSRMoveBuilder::~CLSRMoveBuilder()
{
	if (!fOK)
		return;
	if (fListening) 
		SRStopListening(fRecognizer);
	SRReleaseObject(fModel);
	SRReleaseObject(fTakesModel);
	SRReleaseObject(fToModel);
	for (int piece = 1; piece<7; ++piece) {
		SRReleaseObject(fPieceModel[piece]);
	}
	for (int rowcol = 0; rowcol<8; ++rowcol) {
		SRReleaseObject(fRowModel[rowcol]);
		SRReleaseObject(fColModel[rowcol]);
	}
	SRReleaseObject(fCastleModel);
	SRReleaseObject(fKingSideModel);
	SRReleaseObject(fQueenSideModel);
	SRReleaseObject(fUndoModel);
	SRReleaseObject(fRecognizer);
	SRCloseRecognitionSystem(fRecSystem);
}

void CLSRMoveBuilder::StopListening()
{
	if (fListening) 
		SRStopListening(fRecognizer);
	fListening = false;
}

void CLSRMoveBuilder::StartMoveList()
{
	StopListening();
	SREmptyLanguageObject(fModel);
	SRAddLanguageObject(fModel, fUndoModel);
}

void CLSRMoveBuilder::Move(int piece, const CLCoord & fromCoord, const CLCoord & toCoord, bool take, bool omitFrom)
{
	if (omitFrom)
		return;

	SRPath	path;
	SRNewPath(fRecSystem, &path);
	
	OSType	refCon = 
		(fromCoord.ColLetter() << 24)
	  | (fromCoord.RowLetter() << 16)
	  | (  toCoord.ColLetter() << 8)
	  |    toCoord.RowLetter();
	  
	SRSetProperty (path, kSRRefCon, &refCon, sizeof(refCon));

	if (piece == KING && fromCoord.fCol==4 && !(toCoord.fCol&1) && toCoord.fRow==fromCoord.fRow) {  // Castle
		SRAddLanguageObject(path, fCastleModel);
		if (!omitFrom)
			SRAddLanguageObject(path, toCoord.fCol==6 ? fKingSideModel : fQueenSideModel);
	} else {
		if (omitFrom) {
			SRAddLanguageObject(path, fPieceModel[piece]);
		} else {
			SRAddLanguageObject(path, fPieceModel[piece]);
			SRAddLanguageObject(path, fColModel[fromCoord.fCol]);
			SRAddLanguageObject(path, fRowModel[fromCoord.fRow]);
		}
		SRAddLanguageObject(path, take ? fTakesModel : fToModel);
		SRAddLanguageObject(path, fColModel[toCoord.fCol]);
		SRAddLanguageObject(path, fRowModel[toCoord.fRow]);
	}
	SRAddLanguageObject(fModel, path);
}

void CLSRMoveBuilder::StartListening()
{
	if (!fListening)
		SRStartListening(fRecognizer);
	fListening = true;	
}

void CLSRMoveBuilder::EndMoveList()
{
	SRSetLanguageModel(fRecognizer, fModel);
	StartListening();
}

void CLSRMoveBuilder::MakeWord(SRWord * word, const char * text, bool optional)
{	
	SRNewWord(fRecSystem, word, text, strlen(text));
	if (optional) {
		Boolean opt = true;
		SRSetProperty (*word, kSROptional, &opt, sizeof(Boolean));
	}
}

void CLSRMoveBuilder::MakePhrase(SRPhrase * phrase, const char * text, bool optional)
{	
	SRNewPhrase(fRecSystem, phrase, text, strlen(text));
	if (optional) {
		Boolean opt = true;
		SRSetProperty (*phrase, kSROptional, &opt, sizeof(Boolean));
	}
}

void CLSRMoveBuilder::MakeAlt(SRLanguageModel * alt, const char * t1, const char * t2, bool optional)
{
	SRNewLanguageModel(fRecSystem, alt, "", 0);
	SRWord w;
	SRNewWord(fRecSystem, &w, t1, strlen(t1));
	SRAddLanguageObject(*alt, w);
	SRReleaseObject(w);
	SRNewWord(fRecSystem, &w, t2, strlen(t2));
	SRAddLanguageObject(*alt, w);
	SRReleaseObject(w);
	if (optional) {
		Boolean opt = true;
		SRSetProperty (*alt, kSROptional, &opt, sizeof(Boolean));
	}
}

static CFDataRef	sHelpData;

void CL_SetHelp(unsigned len, const void * data)
{
	sHelpData = CFDataCreate(NULL, (const UInt8 *)data, len);
}

void CLSRMoveBuilder::MakeHelp()
{
	CFMutableDictionaryRef	dict = 
		(CFMutableDictionaryRef)
		   CFPropertyListCreateFromXMLData(NULL, sHelpData, kCFPropertyListMutableContainers, NULL);
	ProcessSerialNumber	psn;
	GetCurrentProcess(&psn);
	CFNumberRef	num = CFNumberCreate(NULL, kCFNumberSInt32Type, &psn.highLongOfPSN);
	CFDictionaryAddValue(dict, CFSTR("ProcessPSNHigh"), num);
	CFRelease(num);
	num = CFNumberCreate(NULL, kCFNumberSInt32Type, &psn.lowLongOfPSN);
	CFDictionaryAddValue(dict, CFSTR("ProcessPSNLow"), num);
	CFRelease(num);
	CFDataRef finalData = CFPropertyListCreateXMLData(NULL, dict);
	if (finalData) {
		SRSetProperty(fRecognizer, 'cdpl', CFDataGetBytePtr(finalData), CFDataGetLength(finalData));
		CFRelease(finalData);
	}
	CFRelease(dict);
}

class CLMoveGenerator {
public:
	CLMoveGenerator(CLMoveBuilder * builder) : fBuilder(builder) {}
	
	void Generate(int color, short pieces[], short colors[]);
private:
	bool	TryMove(int piece, const CLCoord & from, const CLCoord & to);
	bool	TryMove(int piece, const CLCoord & from, int dCol, int dRow)
				{ return TryMove(piece, from, CLCoord(from.fCol+dCol, from.fRow+dRow)); }
	void	TryMoves(int piece, const CLCoord & from, int dCol, int dRow);
	void	TryMoves(int piece, const CLCoord & from);
	void	TryMoves(bool omitFrom);
	void    TryCastle();

	CLMoveBuilder *	fBuilder;
	int				fColor;
	short *			fPieces;
	short *			fColors;
	bool			fOmitFrom;
	short			fTargetUsed[64];
	short			fTargetAmbiguous[64];
};

void CLMoveGenerator::Generate(int color, short pieces[], short colors[])
{
	fBuilder->StartMoveList();
	
	fColor	= 	color;
	fPieces	=	pieces;
	fColors = 	colors;
	memset(fTargetUsed, 0, 64*sizeof(short));
	memset(fTargetAmbiguous, 0, 64*sizeof(short));
	
	TryMoves(false);
	TryMoves(true);
	TryCastle();

	fBuilder->EndMoveList();
}

void CLMoveGenerator::TryMoves(bool omitFrom)
{
	fOmitFrom = omitFrom;
	
	for (int i = 0; i<64; ++i)
		if (fColors[i] == fColor)
			TryMoves(fPieces[i], CLCoord(i));
}

void CLMoveGenerator::TryMoves(int piece, const CLCoord & from)
{
	switch (piece) {
	case PAWN: {
		int dir = fColor == WHITE ? 1 : -1;
		int orig= fColor == WHITE ? 1 : 6;
		
		if (TryMove(piece, from, 0, dir)	// Single step always permitted
		 && from.fRow == orig				// How about a double step?
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

void CLMoveGenerator::TryMoves(int piece, const CLCoord & from, int dCol, int dRow)
{
	CLCoord	to(from);
	
	do {
		to	= CLCoord(to.fCol+dCol, to.fRow+dRow);
	} while (TryMove(piece, from, to));
}

bool CLMoveGenerator::TryMove(int piece, const CLCoord & from, const CLCoord & to)
{	
	int	coord = to.fCoord;
	
	if (coord < 0)
		return false;	// Field does not exist
	
	int color = fColors[coord];
	
	if (color == fColor)
		return false; 	// Field is blocked by own piece
	
	bool take = (color == !fColor); // Field occupied by opponent's piece
	
	if (piece == PAWN) // Pawns move straight, capture diagonally
		if (from.fCol != to.fCol) { // Attempted capture
			if (!take) { // Field is empty, try en passant
				if (from.fRow != (fColor == WHITE ? 4 : 3)) // Must be double step away from opponent's origin
					return false;
				CLCoord	epField(to.fCol, from.fRow);
				int epCoord = epField.fCoord;
				if (fColors[epCoord] != !fColor || fPieces[epCoord] != PAWN) // Must be opponent's pawn
					return false;
				take = true; 	// En passant
			} 
		} else if (take) // Straight move is blocked
			return false;
	
	int pieceMask = 1 << piece;
	
	if (fOmitFrom) {
		// 
		// Simplify language model
		//
		if (fTargetAmbiguous[coord] & pieceMask) // Amiguous move, don't do it
			;
		else
			fBuilder->Move(piece, from, to, take, fOmitFrom);
	} else {
		fTargetAmbiguous[coord] |= fTargetUsed[coord] & pieceMask;
		fTargetUsed[coord]      |= pieceMask;
		
		fBuilder->Move(piece, from, to, take, fOmitFrom);
	}
	
	return !take;	// Don't move further after capture
}

void CLMoveGenerator::TryCastle()
{
	int kingCoord;
	int kingRookCoord;
	int queenRookCoord;

	if (fColor == WHITE) {
		kingCoord 		= 4;
		kingRookCoord 	= 7;
		queenRookCoord	= 0;
	} else {
		kingCoord 		= 60;
		kingRookCoord 	= 63;
		queenRookCoord	= 56;	
	}

	if (fColors[kingCoord] != fColor) // King not in original position
		return;
	
	bool kingSide = false;
	bool queenSide= false;

	if (fColors[kingRookCoord] == fColor && fPieces[kingRookCoord] == ROOK) { // Rook in position
		kingSide = true;
		for (int i = kingCoord+1; i<kingRookCoord; ++i)
			if (fColors[i] != NEUTRAL)
				kingSide = false;
	}

	if (fColors[queenRookCoord] == fColor && fPieces[queenRookCoord] == ROOK) { // Rook in position
		queenSide = true;
		for (int i = queenRookCoord+1; i<kingCoord; ++i)
			if (fColors[i] != NEUTRAL)
				queenSide = false;
	}

	if (kingSide) {
		if (queenSide) {
			fBuilder->Move(KING, CLCoord(kingCoord), CLCoord(kingCoord+2), false, false);
			fBuilder->Move(KING, CLCoord(kingCoord), CLCoord(kingCoord-2), false, false);
		} else {
			fBuilder->Move(KING, CLCoord(kingCoord), CLCoord(kingCoord+2), false, false);
			fBuilder->Move(KING, CLCoord(kingCoord), CLCoord(kingCoord+2), false, true);
		}
	} else if (queenSide) {
		fBuilder->Move(KING, CLCoord(kingCoord), CLCoord(kingCoord-2), false, false);
		fBuilder->Move(KING, CLCoord(kingCoord), CLCoord(kingCoord-2), false, true);
	}
}

static void ProcessResult (OSErr origStatus, SRRecognitionResult recResult)
{
	OSErr				status = origStatus;
	Size				len;
	SRLanguageModel		resultLM, subLM;
	char				refCon[5];

	if (!status && recResult) {
		len = sizeof(resultLM);
		status = SRGetProperty (recResult, kSRLanguageModelFormat, &resultLM, &len);
		if (!status) {
			status = SRGetIndexedItem (resultLM, &subLM, 0);
			if (!status) {
				len = 4;
				status = SRGetProperty (subLM, kSRRefCon, &refCon, &len);
				if (!status) {
					refCon[4] = 0;
					CL_MakeMove(refCon);
				}
					
				//	release subelement when done with it
				SRReleaseObject (subLM);
			}
			
			//	release resultLM fetched above when done with it
			SRReleaseObject (resultLM);
		}
	}

	if (!origStatus) SRReleaseObject (recResult);
}

pascal OSErr HandleSpeechDoneAppleEvent (const AppleEvent *theAEevt, AppleEvent* reply, long refcon)
{
	long				actualSize;
	DescType			actualType;
	OSErr				status = 0;
	OSErr				recStatus = 0;
	SRRecognitionResult	recResult = 0;
	
		/* Get status */
	status = AEGetParamPtr(theAEevt,keySRSpeechStatus,typeShortInteger,
					&actualType, (Ptr)&recStatus, sizeof(status), &actualSize);

		/* Get result */
	if (!status && !recStatus)
		status = AEGetParamPtr(theAEevt,keySRSpeechResult,typeSRSpeechResult,
					&actualType, (Ptr)&recResult, sizeof(SRRecognitionResult), &actualSize);
					
		/* Process result */
	if (!status)
		status = recStatus;
	ProcessResult (status, recResult);

	return status;
}

static CLSRMoveBuilder	*	gBuilder;
static CLMoveGenerator *	gGenerator;

static void CL_Init()
{
  gBuilder = new CLSRMoveBuilder;

#ifdef CHESS_DEBUG
  if (getenv("CHESS_DEBUG")) 
	  gGenerator = new CLMoveGenerator(new CLDebugMoveBuilder(gBuilder));
  else	
#endif
	  gGenerator = new CLMoveGenerator(gBuilder);

  if (gBuilder->OK()) {
    AEInstallEventHandler(kAESpeechSuite, kAESpeechDone, 
			  NewAEEventHandlerUPP(HandleSpeechDoneAppleEvent), 0, false);
    short myModes = kSRHasFeedbackHasListenModes;
    SRSetProperty (gBuilder->Recognizer(), kSRFeedbackAndListeningModes, &myModes, sizeof (myModes));
  }
}

void CL_Listen(short color, short pieces[], short colors[])
{
	if (!gBuilder)
		CL_Init();
	gGenerator->Generate(color, pieces, colors);
}

void CL_DontListen()
{
	if (gBuilder)
		gBuilder->StopListening();
}
