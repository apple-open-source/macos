/*
	File:		MBCInteractivePlayer.mm
	Contains:	An agent representing a local human player
	Copyright:	© 2002-2003 Apple Computer, Inc. All rights reserved.

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
	of Apple Computer, Inc. may be used to endorse or promote products
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

pascal OSErr HandleSpeechDoneAppleEvent (const AppleEvent *theAEevt, AppleEvent* reply, long refcon)
{
	long				actualSize;
	DescType			actualType;
	OSErr				status = 0;
	OSErr				recStatus = 0;
	SRRecognitionResult	recResult = 0;
	
	status = AEGetParamPtr(theAEevt,keySRSpeechStatus,typeShortInteger,
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
							  reinterpret_cast<long>(self), false);
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
			selector:@selector(switchSides:)
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
			selector:@selector(switchSides:)
			name:MBCBlackMoveNotification
			object:nil];
		break;
	case kBothSides:
		[[NSNotificationCenter defaultCenter] 
			addObserver:self
			selector:@selector(switchSides:)
			name:MBCWhiteMoveNotification
			object:nil];
		[[NSNotificationCenter defaultCenter] 
			addObserver:self
			selector:@selector(switchSides:)
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

const char *	sPieceName[] = {
	"", "king", "queen", "bishop", "knight", "rook", "pawn"
};

- (NSString *)stringFromMove:(MBCMove *)move
{
	switch (move->fCommand) {
	case kCmdDrop:
		return [NSString stringWithFormat:@"Drop %s at %c%d.",
						 sPieceName[Piece(move->fPiece)],
						 Col(move->fToSquare), Row(move->fToSquare)];
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
			piece 	= [board oldContents:move->fFromSquare];
			victim	= [board oldContents:move->fToSquare];
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
		return @"[[emph +]]Check mate!";
	case kCmdBlackWins:
		return @"[[emph +]]Check mate!";
	case kCmdDraw:
		return @"The game is a draw!";		
	default:
		return @"";
	}
}

- (void) speakMove:(NSNotification *)notification
{
	if ([fController speakMoves]) {
		MBCMove * 	move = reinterpret_cast<MBCMove *>([notification object]);
		NSString *	text = [self stringFromMove:move];

		Str255	str;
		memcpy(str+1, [text cString], str[0]=[text cStringLength]);
		//
		// We only wait for speech to end before speaking the next move
		// to allow a maximum in concurrency.
		//
		while (SpeechBusy() > 0)
			;
		SpeakString(str);
	}
}

- (void) opponentMoved:(NSNotification *)notification
{
	[self speakMove:notification];
	[self switchSides:notification];
}

- (void) startSelection:(MBCSquare)square
{
	MBCPiece	piece;
	
	if (square > kInHandSquare) {
		piece = square-kInHandSquare;
		if (fVariant!=kVarCrazyhouse || ![[fController board] curInHand:piece])
			piece = EMPTY;
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

- (void) endSelection:(MBCSquare)square
{
	if (fFromSquare == square || square > kSyntheticSquare) {
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
	move->fAnimate		= NO;	// Move already made on board

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
