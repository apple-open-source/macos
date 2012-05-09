/*
	File:		MBCEngine.h
	Contains:	An agent representing the chess playing engine
	Version:	1.0
	Copyright:	© 2002-2010 by Apple Computer, Inc., all rights reserved.
*/

#import "MBCPlayer.h"

//
// MBCEngine is an instance of MBCPlayer, but it also serves other
// purposes like move generation and checking.
//
@interface MBCEngine : MBCPlayer <NSPortDelegate>
{
	NSTask * 		fEngineTask;	// The chess engine
	NSFileHandle * 	fToEngine;		// Writing to the engine
	NSFileHandle * 	fFromEngine;	// Reading from the engine
	NSPipe * 		fToEnginePipe;
	NSPipe * 		fFromEnginePipe;
    NSRunLoop * 	fMainRunLoop;	
	NSPort * 		fEngineMoves;	// Moves parsed from engine
	NSPortMessage * fMove;			// 	... the move
	MBCMove * 		fLastMove;		// Last move played by player
	MBCMove * 		fLastPonder;	// Last move pondered by engine
	MBCMove * 		fLastEngineMove;// Last move played by engine
	MBCSide 		fLastSide;		// Side of player
	bool 			fThinking;		// Engine currently thinking
	bool 			fWaitForStart;	// Wait for StartGame command
	bool 			fSetPosition;	// Position set up already
	bool 			fTakeback;		// Pending takeback
	bool 			fEngineEnabled;	// Engine moves enabled?
	bool 			fNeedsGo;		// Engine needs explicit start
	MBCSide 		fSide;			// What side(s) engine is playing
	int 			fSearchTime;	// Thinking time per move
	NSTimeInterval 	fDontMoveBefore;// Delay next engine move
}

- (id) init;
- (void) shutdown;
- (void) startGame:(MBCVariant)variant playing:(MBCSide)sideToPlay;
- (void) setSearchTime:(int)time;
- (MBCMove *) lastPonder;
- (MBCMove *) lastEngineMove;
- (void) setGame:(MBCVariant)variant fen:(NSString *)fen holding:(NSString *)holding moves:(NSString *)moves;
- (void) takeback;

//
// Hooked up internally
//
- (void) opponentMoved:(NSNotification *)notification;

//
// Used internally
//
- (void) runEngine:(id)sender;
- (void) enableEngineMoves:(BOOL)enable;
- (void) handlePortMessage:(NSPortMessage *)message;
- (void) writeToEngine:(NSString *)string;
- (void) interruptEngine;
- (void) flipSide;
- (NSString *) notificationForSide;
- (NSString *) squareToCoord:(MBCSquare)square;

@end

// Local Variables:
// mode:ObjC
// End:
