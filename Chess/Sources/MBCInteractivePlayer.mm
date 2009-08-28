/*
	File:		MBCInteractivePlayer.mm
	Contains:	An agent representing a local human player
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCInteractivePlayer.mm,v $
		Revision 1.17  2008/10/24 23:23:08  neerache
		Add missing static declaration
		
		Revision 1.16  2007/03/01 23:51:26  neerache
		Offer option to speak human moves <rdar://problem/4038206>
		
		Revision 1.15  2007/01/17 06:10:13  neerache
		Make last move / hint speakable <rdar://problem/4510483>
		
		Revision 1.14  2007/01/17 05:20:25  neerache
		Proper win message in suicide/losers <rdar://problem/3485192>
		
		Revision 1.13  2006/05/19 21:09:33  neerache
		Fix 64 bit compilation errors
		
		Revision 1.12  2004/08/16 07:48:48  neerache
		Support flexible voices, accessibility
		
		Revision 1.11  2003/07/17 23:30:38  neerache
		Add Speech recognition help
		
		Revision 1.10  2003/07/14 23:22:50  neerache
		Move to much smarter speech recognition model
		
		Revision 1.9  2003/07/07 08:49:01  neerache
		Improve startup time
		
		Revision 1.8  2003/06/30 05:02:32  neerache
		Use proper move generator instead of engine
		
		Revision 1.7  2003/05/24 20:25:25  neerache
		Eliminate compact moves for most purposes
		
		Revision 1.6  2003/04/24 23:20:35  neeri
		Support pawn promotions
		
		Revision 1.5  2002/10/08 22:12:38  neeri
		Beep on rejected move
		
		Revision 1.4  2002/09/13 23:57:06  neeri
		Support for Crazyhouse display and mouse
		
		Revision 1.3  2002/09/12 17:46:46  neeri
		Introduce dual board representation, in-hand pieces
		
		Revision 1.2  2002/08/26 23:14:40  neeri
		Weed out non-moves
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
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

static const char *	sPieceName[] = {
	"", "king", "queen", "bishop", "knight", "rook", "pawn"
};

- (NSString *)stringFromMove:(MBCMove *)move
{
	switch (move->fCommand) {
	case kCmdDrop:
		return [NSString stringWithFormat:@"Drop %s at %c%d.",
						 sPieceName[Piece(move->fPiece)],
						 Col(move->fToSquare), Row(move->fToSquare)];
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
			return @"Castle [[emph +]]queen side.";
		case kCastleKingside:
			return @"Castle [[emph +]]king side.";
		default:
			if (move->fPiece) { // Move already executed
				piece 	= move->fPiece;
				victim	= move->fVictim;
			} else {
				piece 	= [board oldContents:move->fFromSquare];
				victim	= [board oldContents:move->fToSquare];
			}
			promo	= move->fPromotion;
			if (promo)
				return [NSString stringWithFormat:@"%s %c%d %s %c%d promoting to %s.",
								 sPieceName[Piece(piece)],
								 Col(move->fFromSquare), Row(move->fFromSquare),
								 (victim ? "takes" : "to"),
								 Col(move->fToSquare), Row(move->fToSquare),
								 sPieceName[Piece(promo)]];
			else
				return [NSString stringWithFormat:@"%s %c%d %s %c%d.",
								 sPieceName[Piece(piece)],
								 Col(move->fFromSquare), Row(move->fFromSquare),
								 (victim ? "takes" : "to"),
								 Col(move->fToSquare), Row(move->fToSquare)];
		}}
	case kCmdWhiteWins:
		switch (fVariant) {
		case kVarSuicide:
		case kVarLosers:
			return @"White wins!";
		default:
			return @"[[emph +]]Check mate!";
		}
	case kCmdBlackWins:
		switch (fVariant) {
		case kVarSuicide:
		case kVarLosers:
			return @"Black wins!";
		default:
			return @"[[emph +]]Check mate!";
		}
	case kCmdDraw:
		return @"The game is a draw!";		
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
	NSSpeechSynthesizer * synth;
	BOOL blackMove = Color(move->fPiece)==kBlackPiece;
	BOOL altIsBlack= fSide == kNeitherSide || fSide == kBothSides || fSide == kBlackSide;
	if (blackMove == altIsBlack)
		synth = [fController alternateSynth];
	else
		synth = [fController defaultSynth];

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
	[self speakMove:move withWrapper:@"I would suggest \"%@\""];
}

- (void) announceLastMove:(MBCMove *) move
{
	[self speakMove:move withWrapper:@"The last move was \"%@\""];
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
 
