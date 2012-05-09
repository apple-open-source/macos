/*
	File:		MBCInteractivePlayer.h
	Contains:	An agent representing a local human player
	Version:	1.0
	Copyright:	© 2002-2010 by Apple Computer, Inc., all rights reserved.
*/

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

#import "MBCPlayer.h"
#import "MBCMoveGenerator.h"

@class MBCController;
@class MBCLanguageModel;

//
// MBCInteractivePlayer represents humans playing locally
//
@interface MBCInteractivePlayer : MBCPlayer 
{
	MBCController *			fController;
	MBCLanguageModel *		fLanguageModel;
	MBCSide					fLastSide;
	MBCSide					fSide;
	MBCVariant				fVariant;
	MBCSquare				fFromSquare;
	bool					fStartingSR;
	SRRecognitionSystem		fRecSystem;
	SRRecognizer			fRecognizer;
	SRLanguageModel			fModel;
	NSData *				fSpeechHelp;
}

- (id) initWithController:(MBCController *)controller;
- (void) startGame:(MBCVariant)variant playing:(MBCSide)sideToPlay;
- (void) updateNeedMouse:(id)arg;

//
// The board view translates coordinates into board squares and handles
// dragging.
//
- (void) startSelection:(MBCSquare)square;
- (void) endSelection:(MBCSquare)square animate:(BOOL)animate;

//
// If we recognize a move, we have to broadcast it
//
- (void) recognized:(SRRecognitionResult)result;

//
// Announce hint / last move
//
- (void) announceHint:(MBCMove *) move;
- (void) announceLastMove:(MBCMove *) move;

@end

// Local Variables:
// mode:ObjC
// End:
