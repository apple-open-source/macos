/*
	File:		MBCInteractivePlayer.mm
	Contains:	An agent representing a local human player
	Copyright:	© 2002-2012 by Apple Inc., all rights reserved.

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

#import "MBCInteractivePlayer.h"
#import "MBCEngine.h"
#import "MBCBoardView.h"
#import "MBCBoardWin.h"
#import "MBCLanguageModel.h"
#import "MBCController.h"
#import "MBCDocument.h"

#import <ApplicationServices/ApplicationServices.h>
#include <dispatch/dispatch.h>
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
    SRRecognizer        recognizer;
	
	status = AEGetParamPtr(theAEevt,keySRSpeechStatus,typeSInt16,
					&actualType, (Ptr)&recStatus, sizeof(status), &actualSize);
	if (!status)
		status = recStatus;

	if (!status)
		status = AEGetParamPtr(theAEevt,keySRRecognizer,
							   typeSRRecognizer, &actualType, 
							   (Ptr)&recognizer,
							   sizeof(SRRecognizer), &actualSize);
	if (!status)
		status = AEGetParamPtr(theAEevt,keySRSpeechResult,
							   typeSRSpeechResult, &actualType, 
							   (Ptr)&recResult,
							   sizeof(SRRecognitionResult), &actualSize);
    if (!status) {
        Size sz = sizeof(refcon);
        status = SRGetProperty(recognizer, kSRRefCon, &refcon, &sz);
    }
	if (!status) {
		[reinterpret_cast<MBCInteractivePlayer *>(refcon) 	
						 recognized:recResult];
		SRReleaseObject(recResult);
	}

	return status;
}

void SpeakStringWhenReady(NSSpeechSynthesizer * synth, NSString * text)
{
    static NSSpeechSynthesizer  * sLastSynth;
    static NSMutableArray       * sSynthQueue;
    
    if (synth) {
        if (!sSynthQueue)
            sSynthQueue = [[NSMutableArray alloc] initWithCapacity:1];
        [sSynthQueue addObject:[NSArray arrayWithObjects:synth, text, nil]];
    }
    if (sLastSynth) {
        if ([sLastSynth isSpeaking]) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, [sSynthQueue count] ? 100*NSEC_PER_MSEC : NSEC_PER_SEC), dispatch_get_main_queue(), ^{
                SpeakStringWhenReady(nil, nil);
            });
            return;
        } else {
            [sLastSynth release];
            sLastSynth = nil;
        }
    }
    if ([sSynthQueue count]) {
        NSArray *   job = [sSynthQueue objectAtIndex:0];
 
        sLastSynth = [[job objectAtIndex:0] retain];
        [sLastSynth startSpeakingString:[job objectAtIndex:1]];
        [sSynthQueue removeObjectAtIndex:0];
    }
}

@implementation MBCInteractivePlayer

- (void) makeSpeechHelp
{
	NSPropertyListFormat	format;

	NSString * path = 
		[[NSBundle mainBundle] pathForResource: @"SpeechHelp" ofType: @"plist"];
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

- (void) initSR
{
	if (SROpenRecognitionSystem(&fRecSystem, kSRDefaultRecognitionSystemID))
		return;
	SRNewRecognizer(fRecSystem, &fRecognizer, kSRDefaultSpeechSource);
    SRSetProperty(fRecognizer, kSRRefCon, &self, sizeof(self));
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

- (void) updateNeedMouse:(id)arg
{
	BOOL	wantMouse;

	if (fLastSide == kBlackSide)
		wantMouse = fSide == kWhiteSide || fSide == kBothSides;
	else
		wantMouse = fSide == kBlackSide || fSide == kBothSides;

    if (wantMouse && [fDocument gameDone])
        wantMouse = NO;
    
	[[fController gameView] wantMouse:wantMouse];
    [[NSApp delegate] updateApplicationBadge];

	if ([fController listenForMoves]) {
		//
		// Work with speech recognition
		//
		if (wantMouse) {
			if (fStartingSR) {
					; // Current starting, will update later
			} else if (!fRecSystem) {
                static dispatch_once_t  sInitOnce;
                static dispatch_queue_t sInitQueue;
                dispatch_once(&sInitOnce, ^{
                    sInitQueue = dispatch_queue_create("InitSR", DISPATCH_QUEUE_SERIAL);
                    AEInstallEventHandler(kAESpeechSuite, kAESpeechDone, 
                                          NewAEEventHandlerUPP(HandleSpeechDoneAppleEvent), 
                                          NULL, false);
                });
				fStartingSR = true;
                dispatch_async(sInitQueue, ^{
                    [self initSR];
                });
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

- (void)allowedToListen:(BOOL)allowed
{
    [self updateNeedMouse:self];
    if (fRecSystem && !allowed)
        SRStopListening(fRecognizer);
}

- (void) removeChessObservers
{
    if (!fHasObservers)
        return;
    
    NSNotificationCenter * notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter removeObserver:self name:MBCWhiteMoveNotification object:nil];
    [notificationCenter removeObserver:self name:MBCBlackMoveNotification object:nil];
    [notificationCenter removeObserver:self name:MBCIllegalMoveNotification object:nil];
    [notificationCenter removeObserver:self name:MBCTakebackNotification object:nil];
    [notificationCenter removeObserver:self name:MBCGameEndNotification object:nil];
    [fDocument removeObserver:self forKeyPath:@"Result"];
    
    fHasObservers = NO;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    [self updateNeedMouse:self];
}

- (void)dealloc
{
    [self removeChessObservers];
    [fSpeechHelp release];
    [fLanguageModel release];
    [super dealloc];
}

- (void) startGame:(MBCVariant)variant playing:(MBCSide)sideToPlay
{
	fVariant	=   variant;
	fLastSide	=	
		([[fController board] numMoves] & 1) 
		? kWhiteSide : kBlackSide;

    [self removeChessObservers];
    NSNotificationCenter * notificationCenter = [NSNotificationCenter defaultCenter];
	switch (fSide = sideToPlay) {
	case kWhiteSide:
		[notificationCenter 
			addObserver:self
			selector:@selector(humanMoved:)
			name:MBCWhiteMoveNotification
			object:fDocument];
		[notificationCenter 
			addObserver:self
			selector:@selector(opponentMoved:)
			name:MBCBlackMoveNotification
			object:fDocument];
		break;
	case kBlackSide:
		[notificationCenter 
			addObserver:self
			selector:@selector(opponentMoved:)
			name:MBCWhiteMoveNotification
			object:fDocument];
		[notificationCenter 
			addObserver:self
			selector:@selector(humanMoved:)
			name:MBCBlackMoveNotification
			object:fDocument];
		break;
	case kBothSides:
		[notificationCenter 
			addObserver:self
			selector:@selector(humanMoved:)
			name:MBCWhiteMoveNotification
			object:fDocument];
		[notificationCenter 
			addObserver:self
			selector:@selector(humanMoved:)
			name:MBCBlackMoveNotification
			object:fDocument];
		break;
	case kNeitherSide:
		[notificationCenter 
			addObserver:self
			selector:@selector(opponentMoved:)
			name:MBCWhiteMoveNotification
			object:fDocument];
		[notificationCenter 
			addObserver:self
			selector:@selector(opponentMoved:)
			name:MBCBlackMoveNotification
			object:fDocument];
		break;
	}
	[notificationCenter 
		addObserver:self
		selector:@selector(reject:)
		name:MBCIllegalMoveNotification
		object:fDocument];
	[notificationCenter 
		addObserver:self
		selector:@selector(takeback:)
		name:MBCTakebackNotification
		object:fDocument];
	[notificationCenter 
		addObserver:self
		selector:@selector(gameEnded:)
		name:MBCGameEndNotification
		object:fDocument];
    [fDocument addObserver:self forKeyPath:@"Result" options:NSKeyValueObservingOptionNew context:nil];
    fHasObservers = YES;

	[self updateNeedMouse:self];
}

- (void) reject:(NSNotification *)n
{
	NSBeep();
	[[fController gameView] unselectPiece];
}

- (void) takeback:(NSNotification *)n
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self updateNeedMouse:self];
    });
}

- (void) switchSides:(NSNotification *)n
{
	fLastSide	= 	fLastSide==kBlackSide ? kWhiteSide : kBlackSide;

	[self updateNeedMouse:self];
}

- (BOOL)useAlternateSynthForMove:(MBCMove *)move
{
    if (fSide == kBothSides || fSide == kNeitherSide)
        return [[fController board] sideOfMove:move] == kBlackSide;
    else
        return fSide == [[fController board] sideOfMove:move];
}

- (NSString *)stringFromMove:(MBCMove *)move
{
	NSDictionary * localization = [self useAlternateSynthForMove:move] 
		? [fController alternateLocalization]
		: [fController primaryLocalization];

    return [[fController board] stringFromMove:move withLocalization:localization];
}

- (NSString *)stringForCheck:(MBCMove *)move
{
	NSDictionary * localization = [self useAlternateSynthForMove:move] 
        ? [fController alternateLocalization]
        : [fController primaryLocalization];
    
    return LOC(@"check", @"Check!");
}

- (void) speakMove:(MBCMove *)move text:(NSString *)text check:(BOOL)check
{
	NSSpeechSynthesizer * synth = [self useAlternateSynthForMove:move] 
		? [fController alternateSynth]
		: [fController primarySynth];
    
    if (!check || (move->fCheck && !move->fCheckMate))
        SpeakStringWhenReady(synth, text);
        if (!check && move->fCheck)
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 200*NSEC_PER_MSEC), 
                           dispatch_get_main_queue(), ^{
                [self speakMove:move text:[self stringForCheck:move] check:YES];
            });
 }

- (void) speakMove:(NSNotification *)notification
{
	MBCMove * 	move = reinterpret_cast<MBCMove *>([notification userInfo]);
    
	NSString *	text = [self stringFromMove:move];

	[self speakMove:move text:text check:NO];
}

- (void) gameEnded:(NSNotification *)notification
{
    MBCSide humanSide   = [fDocument humanSide];
    BOOL    wasHumanMove;
    if (humanSide == kBothSides) {
        wasHumanMove = YES;
    } else if (humanSide == kNeitherSide) {
        wasHumanMove = NO;
    } else {
        MBCMove * 	move = reinterpret_cast<MBCMove *>([notification userInfo]);
        wasHumanMove    = [[fController board] sideOfMove:move] == humanSide;
    }
    if (wasHumanMove ? [fController speakHumanMoves] : [fController speakMoves]) 
        if (![fDocument gameDone]) // Game was not previously finished
            [self speakMove:notification];
}

- (void) speakMove:(MBCMove *) move withWrapper:(NSString *)wrapper
{
	if (move && ([fController speakHumanMoves] || [fController speakMoves])) {
		NSString *	text = [self stringFromMove:move];
		NSString *  wrapped = 
			[NSString stringWithFormat:wrapper, text];
	
		[self speakMove:move text:wrapped check:NO];
	}
}

- (void) announceHint:(MBCMove *) move
{
	if (!move)
		return;

	NSDictionary * localization = [self useAlternateSynthForMove:move] 
		? [fController alternateLocalization]
		: [fController primaryLocalization];

	[self speakMove:move withWrapper:LOC(@"suggest_fmt", @"I would suggest \"%@\"")];
}

- (void) announceLastMove:(MBCMove *) move
{
	if (!move)
		return;

	NSDictionary * localization = [self useAlternateSynthForMove:move] 
		? [fController alternateLocalization]
		: [fController primaryLocalization];

	[self speakMove:move withWrapper:LOC(@"last_move_fmt", @"The last move was \"%@\"")];
}

- (void) opponentMoved:(NSNotification *)notification
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if ([fController speakMoves]) 
            [self speakMove:notification];
        [self switchSides:notification];
    });
}

- (void) humanMoved:(NSNotification *)notification
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if ([fController speakHumanMoves]) 
            [self speakMove:notification];
        [self switchSides:notification];
    });
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
		[[fController gameView] selectPiece:piece at:square];
	}
}

- (void) endSelection:(MBCSquare)square animate:(BOOL)animate
{
	if (fFromSquare == square) {
		[[fController gameView] clickPiece];

		return;
	} else if (square > kSyntheticSquare) {
		[[fController gameView] unselectPiece];
		
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
	 object:fDocument userInfo:(id)move];
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
             object:fDocument userInfo:(id)move];
		}
	}
}

@end

// Local Variables:
// mode:ObjC
// End:
 
