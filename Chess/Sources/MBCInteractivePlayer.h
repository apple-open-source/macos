/*
	File:		MBCInteractivePlayer.h
	Contains:	An agent representing a local human player
	Version:	1.0
	Copyright:	© 2002-2007 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCInteractivePlayer.h,v $
		Revision 1.9  2007/01/17 06:10:13  neerache
		Make last move / hint speakable <rdar://problem/4510483>
		
		Revision 1.8  2004/08/16 07:50:55  neerache
		Support accessibility
		
		Revision 1.7  2003/07/17 23:30:38  neerache
		Add Speech recognition help
		
		Revision 1.6  2003/07/14 23:22:50  neerache
		Move to much smarter speech recognition model
		
		Revision 1.5  2003/06/30 05:02:32  neerache
		Use proper move generator instead of engine
		
		Revision 1.4  2003/05/24 20:25:25  neerache
		Eliminate compact moves for most purposes
		
		Revision 1.3  2003/04/24 23:20:35  neeri
		Support pawn promotions
		
		Revision 1.2  2002/09/13 23:57:06  neeri
		Support for Crazyhouse display and mouse
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
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
