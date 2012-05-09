/*
	File:		MBCInteractivePlayer.mm
	Contains:	An agent representing a local human player
	Version:	1.0
	Copyright:	© 2002-2011 by Apple Computer, Inc., all rights reserved.
*/

#import "MBCInteractivePlayer.h"
#import "MBCBoardView.h"
#import "MBCController.h"
#import "MBCLanguageModel.h"

#import <ApplicationServices/ApplicationServices.h>

//
// Private selector to set the help text in the speech feedback window
//
#ifndef kSRCommandsDisplayCFPropListRef
#define kSRCommandsDisplayCFPropListRef	'cdpl'
#endif

pascal OSErr HandleSpeechDoneAppleEvent (const AppleEvent *theAEevt, AppleEvent* reply, SRefCon refcon)
{
	long				actualSize;
	DescType			actualType;
	OSErr				status = 0;
	OSErr				recStatus = 0;
	SRRecognitionResult	recResult = 0;
	
	status = AEGetParamPtr(theAEevt,keySRSpeechStatus,typeSInt16,
					&actualType, (Ptr)&recStatus, sizeof(status), &actualSize);
	if (!status)
		status = recStatus;

	if (!status)
		status = AEGetParamPtr(theAEevt,keySRSpeechResult,
							   typeSRSpeechResult, &actualType, 
							   (Ptr)&recResult,
							   sizeof(SRRecognitionResult), &actualSize);
	if (!status) {
		[reinterpret_cast<MBCInteractivePlayer *>(refcon) 	
						 recognized:recResult];
		SRReleaseObject(recResult);
	}

	return status;
}

NSString * LocalizedString(NSDictionary * localization, NSString * key, NSString * fallback)
{
	NSString * value = [localization valueForKey:key];

	return value ? value : fallback;
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

#define LOC(key, fallback) LocalizedString(localization, key, fallback)

@implementation MBCInteractivePlayer

- (id) initWithController:(MBCController *)controller
{
	[super init];

	fController	= controller;
	fRecSystem	= 0;
	fRecognizer = 0;
	fSpeechHelp	= 0;
	fStartingSR = false;

	return self;
}

- (void) makeSpeechHelp
{
	NSPropertyListFormat	format;

	NSString * path = 
		[[NSBundle mainBundle] pathForResource: @"SpeechHelp" ofType: @"xml"];
	NSData *	help 	= 
		[NSData dataWithContentsOfFile:path];
	NSMutableDictionary * prop = 
		[NSPropertyListSerialization 
			propertyListFromData: help
			mutabilityOption: NSPropertyListMutableContainers
			format: &format
			errorDescription:nil];
	ProcessSerialNumber	psn;
	GetCurrentProcess(&psn);
	[prop setObject:[NSNumber numberWithLong:psn.highLongOfPSN] 
		  forKey:@"ProcessPSNHigh"];
	[prop setObject:[NSNumber numberWithLong:psn.lowLongOfPSN] 
		  forKey:@"ProcessPSNLow"];
	fSpeechHelp =
		[[NSPropertyListSerialization 
			 dataFromPropertyList:prop
			 format: NSPropertyListXMLFormat_v1_0
			 errorDescription:nil]
			retain];
}

- (void) updateNeedMouse:(id)arg
{
	BOOL	wantMouse;

	if (fLastSide == kBlackSide)
		wantMouse = fSide == kWhiteSide || fSide == kBothSides;
	else
		wantMouse = fSide == kBlackSide || fSide == kBothSides;

	[[fController view] wantMouse:wantMouse];

	if ([fController listenForMoves]) {
		//
		// Work with speech recognition
		//
		if (wantMouse) {
			if (fStartingSR) {
					; // Current starting, will update later
			} else if (!fRecSystem) {
				fStartingSR = true;
				[NSThread detachNewThreadSelector:@selector(initSR:) 
						  toTarget:self withObject:nil];
			} else {
				if (!fSpeechHelp) {
					[self makeSpeechHelp];
					SRSetProperty(fRecognizer, kSRCommandsDisplayCFPropListRef,
					  [fSpeechHelp bytes], [fSpeechHelp length]);
				}

				SRStopListening(fRecognizer);
				MBCMoveCollector * moves = [MBCMoveCollector new];
				MBCMoveGenerator generateMoves(moves, fVariant, 0);
				generateMoves.Generate(fLastSide==kBlackSide,
									   *[[fController board] curPos]);
				[fLanguageModel buildLanguageModel:fModel 
								fromMoves:[moves collection]
								takeback:[[fController board] canUndo]];
				SRSetLanguageModel(fRecognizer, fModel);
				SRStartListening(fRecognizer);	
				[moves release];
			}
		} else if (fRecSystem) 
			SRStopListening(fRecognizer);
	} else if (fRecSystem && !fStartingSR) {
		// 	
		// Time to take the recognition system down
		//
		SRStopListening(fRecognizer);
		[fLanguageModel release];
		SRReleaseObject(fRecognizer);
		SRCloseRecognitionSystem(fRecSystem);
		fRecSystem	=	0;
	}
}

- (void) initSR:(id)arg
{
	if (!fRecognizer) // very first time
		AEInstallEventHandler(kAESpeechSuite, kAESpeechDone, 
							  NewAEEventHandlerUPP(HandleSpeechDoneAppleEvent), 
							  reinterpret_cast<SRefCon>(self), false);
	if (SROpenRecognitionSystem(&fRecSystem, kSRDefaultRecognitionSystemID))
		return;
	SRNewRecognizer(fRecSystem, &fRecognizer, kSRDefaultSpeechSource);
	short modes = kSRHasFeedbackHasListenModes;
	SRSetProperty(fRecognizer, kSRFeedbackAndListeningModes, &modes, sizeof(short));
	SRNewLanguageModel(fRecSystem, &fModel, "<moves>", 7);
	fLanguageModel = 
		[[MBCLanguageModel alloc] initWithRecognitionSystem:fRecSystem];
	if (fSpeechHelp)
		SRSetProperty(fRecognizer, kSRCommandsDisplayCFPropListRef,
					  [fSpeechHelp bytes], [fSpeechHelp length]);
	fStartingSR = false;
	[self performSelectorOnMainThread:@selector(updateNeedMouse:)
		  withObject:self waitUntilDone:NO];
}

- (void) startGame:(MBCVariant)variant playing:(MBCSide)sideToPlay
{
	fVariant	=   variant;
	fLastSide	=	
		([[[MBCController controller] board] numMoves] & 1) 
		? kWhiteSide : kBlackSide;
	
	[[NSNotificationCenter defaultCenter] removeObserver:self];

	switch (fSide = sideToPlay) {
	case kWhiteSide:
		[[NSNotificationCenter defaultCenter] 
			addObserver:self
			selector:@selector(humanMoved:)
			name:MBCWhiteMoveNotification
			object:nil];
		[[NSNotificationCenter defaultCenter] 
			addObserver:self
			selector:@selector(opponentMoved:)
			name:MBCBlackMoveNotification
			object:nil];
		break;
	case kBlackSide:
		[[NSNotificationCenter defaultCenter] 
			addObserver:self
			selector:@selector(opponentMoved:)
			name:MBCWhiteMoveNotification
			object:nil];
		[[NSNotificationCenter defaultCenter] 
			addObserver:self
			selector:@selector(humanMoved:)
			name:MBCBlackMoveNotification
			object:nil];
		break;
	case kBothSides:
		[[NSNotificationCenter defaultCenter] 
			addObserver:self
			selector:@selector(humanMoved:)
			name:MBCWhiteMoveNotification
			object:nil];
		[[NSNotificationCenter defaultCenter] 
			addObserver:self
			selector:@selector(humanMoved:)
			name:MBCBlackMoveNotification
			object:nil];
		break;
	case kNeitherSide:
		[[NSNotificationCenter defaultCenter] 
			addObserver:self
			selector:@selector(opponentMoved:)
			name:MBCWhiteMoveNotification
			object:nil];
		[[NSNotificationCenter defaultCenter] 
			addObserver:self
			selector:@selector(opponentMoved:)
			name:MBCBlackMoveNotification
			object:nil];
		break;
	}
	[[NSNotificationCenter defaultCenter] 
		addObserver:self
		selector:@selector(reject:)
		name:MBCIllegalMoveNotification
		object:nil];
	[[NSNotificationCenter defaultCenter] 
		addObserver:self
		selector:@selector(takeback:)
		name:MBCTakebackNotification
		object:nil];
	[[NSNotificationCenter defaultCenter] 
		addObserver:self
		selector:@selector(speakMove:)
		name:MBCGameEndNotification
		object:nil];

	[self updateNeedMouse:self];
}

- (void) reject:(NSNotification *)n
{
	NSBeep();
	[[fController view] unselectPiece];
}

- (void) takeback:(NSNotification *)n
{
	[self updateNeedMouse:self];
}

- (void) switchSides:(NSNotification *)n
{
	fLastSide	= 	fLastSide==kBlackSide ? kWhiteSide : kBlackSide;

	[self updateNeedMouse:self];
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

- (BOOL)useAlternateSynthForMove:(MBCMove *)move
{
	BOOL blackMove = Color(move->fPiece)==kBlackPiece;
	BOOL altIsBlack= fSide == kNeitherSide || fSide == kBothSides || fSide == kBlackSide;

	return blackMove == altIsBlack;
}

- (NSString *)stringFromMove:(MBCMove *)move
{
	NSDictionary * localization = [self useAlternateSynthForMove:move] 
		? [fController alternateLocalization]
		: [fController defaultLocalization];

	switch (move->fCommand) {
	case kCmdDrop: {
		NSString * format  	= LOC(@"drop_fmt", @"%@ %c %d.");
		NSString * pkey 	= [NSString stringWithFormat:@"%@_d", sPieceName[Piece(move->fPiece)]];
		NSString * pdef 	= [NSString stringWithFormat:@"drop @% at", sPieceName[Piece(move->fPiece)]];
		NSString * ploc 	= LOC(pkey, pdef);
		char	   col  	= Col(move->fToSquare);
		int		   row  	= Row(move->fToSquare);
		if (OldSquares(format)) 
			return [NSString stringWithFormat:format, ploc, toupper(col), row];
		else
			return [NSString stringWithFormat:format, ploc, LOC_FILE(col), LOC_RANK(row)];
	}
	case kCmdPMove:
	case kCmdMove: {
		MBCBoard *	board = [fController board];
		MBCPiece	piece;
		MBCPiece	victim;
		MBCPiece	promo;

		if (!move->fCastling)
			[board tryCastling:move];
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
				piece 	= [board oldContents:move->fFromSquare];
				victim	= [board oldContents:move->fToSquare];
			}
			promo	= move->fPromotion;
			NSString * pname = LOC(sPieceName[Piece(piece)], sPieceName[Piece(piece)]);
			char	   fcol  = Col(move->fFromSquare);
			int		   frow  = Row(move->fFromSquare);
			char	   tcol  = Col(move->fToSquare);
			int		   trow  = Row(move->fToSquare);
			if (promo) {
				NSString * format = victim
					? LOC(@"cpromo_fmt", @"%@ %c %d takes %c %d %@.")
					: LOC(@"promo_fmt", @"%@ %c %d to %c %d %@.");
				NSString * pkey  = [NSString stringWithFormat:@"%@_p", sPieceName[Piece(promo)]];
				NSString * pdef  = [NSString stringWithFormat:@"promoting to %@", sPieceName[Piece(promo)]];
				NSString * ploc  = LOC(pkey, pdef);

				if (OldSquares(format))
					return [NSString stringWithFormat:format, pname,
									 toupper(fcol), frow, toupper(tcol), trow, 
									 ploc];
				else
					return [NSString stringWithFormat:format, pname,
									 LOC_FILE(fcol), LOC_RANK(frow), LOC_FILE(tcol), LOC_RANK(trow),
									 ploc];
			} else {
				NSString * format = victim
					? LOC(@"cmove_fmt", @"%@ %c %d takes %c %d.")
					: LOC(@"move_fmt", @"%@ %c %d to %c %d.");

				if (OldSquares(format))
					return [NSString stringWithFormat:format, pname,
									 toupper(fcol), frow, toupper(tcol), trow];
				else
					return [NSString stringWithFormat:format, pname,
									 LOC_FILE(fcol), LOC_RANK(frow), LOC_FILE(tcol), LOC_RANK(trow)];
			}
		}}
	case kCmdWhiteWins:
		switch (fVariant) {
		case kVarSuicide:
		case kVarLosers:
			return LOC(@"white_win", @"White wins!");
		default:
			return LOC(@"check_mate", @"[[emph +]]Check mate!");
		}
	case kCmdBlackWins:
		switch (fVariant) {
		case kVarSuicide:
		case kVarLosers:
			return LOC(@"black_win", @"Black wins!");
		default:
			return LOC(@"check_mate", @"[[emph +]]Check mate!");
		}
	case kCmdDraw:
		return LOC(@"draw", @"The game is a draw!");
	default:
		return @"";
	}
}

- (void) speakMove:(MBCMove *)move text:(NSString *)text
{
	//
	// We only wait for speech to end before speaking the next move
	// to allow a maximum in concurrency.
	//
	while (SpeechBusy() > 0)
		;
	NSSpeechSynthesizer * synth = [self useAlternateSynthForMove:move] 
		? [fController alternateSynth]
		: [fController defaultSynth];

	[synth startSpeakingString:text];
}

- (void) speakMove:(NSNotification *)notification
{
	MBCMove * 	move = reinterpret_cast<MBCMove *>([notification object]);
	NSString *	text = [self stringFromMove:move];

	[self speakMove:move text:text];
}

- (void) speakMove:(MBCMove *) move withWrapper:(NSString *)wrapper
{
	if (move && ([fController speakHumanMoves] || [fController speakMoves])) {
		NSString *	text = [self stringFromMove:move];
		NSString *  wrapped = 
			[NSString stringWithFormat:wrapper, text];
	
		[self speakMove:move text:wrapped];
	}
}

- (void) announceHint:(MBCMove *) move
{
	if (!move)
		return;

	NSDictionary * localization = [self useAlternateSynthForMove:move] 
		? [fController alternateLocalization]
		: [fController defaultLocalization];

	[self speakMove:move withWrapper:LOC(@"suggest_fmt", @"I would suggest \"%@\"")];
}

- (void) announceLastMove:(MBCMove *) move
{
	if (!move)
		return;

	NSDictionary * localization = [self useAlternateSynthForMove:move] 
		? [fController alternateLocalization]
		: [fController defaultLocalization];

	[self speakMove:move withWrapper:LOC(@"last_move_fmt", @"The last move was \"%@\"")];
}

- (void) opponentMoved:(NSNotification *)notification
{
	if ([fController speakMoves]) 
		[self speakMove:notification];
	[self switchSides:notification];
}

- (void) humanMoved:(NSNotification *)notification
{
	if ([fController speakHumanMoves]) 
		[self speakMove:notification];
	[self switchSides:notification];
}

- (void) startSelection:(MBCSquare)square
{
	MBCPiece	piece;
	
	if (square > kInHandSquare) {
		piece = square-kInHandSquare;
		if (fVariant!=kVarCrazyhouse || ![[fController board] curInHand:piece])
			return;
	} else if (square == kWhitePromoSquare || square == kBlackPromoSquare)
		return;
	else
		piece = [[fController board] oldContents:square];

	if (!piece)
		return;

	if (Color(piece) == (fLastSide==kBlackSide ? kWhitePiece : kBlackPiece)) {
		fFromSquare	=  square;
		[[fController view] selectPiece:piece at:square];
	}
}

- (void) endSelection:(MBCSquare)square animate:(BOOL)animate
{
	if (fFromSquare == square) {
		[[fController view] clickPiece];

		return;
	} else if (square > kSyntheticSquare) {
		[[fController view] unselectPiece];
		
		return;
	}

	MBCMove *	move = [MBCMove moveWithCommand:kCmdMove];

	if (fFromSquare > kInHandSquare) {
		move->fCommand = kCmdDrop;
		move->fPiece   = fFromSquare-kInHandSquare;
	} else {
		move->fFromSquare	= fFromSquare;
	}
	move->fToSquare		= square;
	move->fAnimate		= animate;

	//
	// Fill in promotion info
	//
	[[fController board] tryPromotion:move];

	[[NSNotificationCenter defaultCenter] 
	 postNotificationName:
	 (fLastSide==kBlackSide 
	  ? MBCUncheckedWhiteMoveNotification
	  : MBCUncheckedBlackMoveNotification)
	 object:move];
}

- (void) recognized:(SRRecognitionResult)result
{
	if (MBCMove * move = [fLanguageModel recognizedMove:result]) {
		if (move->fCommand == kCmdUndo) {
			[fController takeback:self];
		} else {
			//
			// Fill in promotion info if missing
			//
			[[fController board] tryPromotion:move];

			NSString * notification;			
			if (fLastSide==kBlackSide)
				notification = MBCUncheckedWhiteMoveNotification;
			else
				notification = MBCUncheckedBlackMoveNotification;
			[[NSNotificationCenter defaultCenter] 
				postNotificationName:notification
				object:move];
		}
	}
}

@end

// Local Variables:
// mode:ObjC
// End:
 
