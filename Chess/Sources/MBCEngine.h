/*
	File:		MBCEngine.h
	Contains:	An agent representing the chess playing engine
	Version:	1.0
	Copyright:	© 2002-2010 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCEngine.h,v $
		Revision 1.12  2010/01/18 18:37:16  neerache
		<rdar://problem/7297328> Deprecated methods in Chess, part 1
		
		Revision 1.11  2004/10/20 18:31:55  neerache
		Fix typo in comment (RADAR 3846101)
		
		Revision 1.10  2004/09/11 02:03:23  neerache
		Implement delays to humanize engine
		
		Revision 1.9  2003/06/30 05:15:06  neerache
		Use proper move generator instead of engine
		
		Revision 1.8  2003/05/27 07:25:09  neerache
		Restart engine when loading
		
		Revision 1.7  2003/05/27 03:13:57  neerache
		Rework game loading/saving code
		
		Revision 1.6  2003/05/24 20:28:27  neerache
		Address race conditions between ploayer and engine
		
		Revision 1.5  2003/04/24 23:21:40  neeri
		Fix takebacks
		
		Revision 1.4  2003/04/10 23:03:17  neeri
		Load positions
		
		Revision 1.3  2003/03/28 01:29:53  neeri
		Support hints, last move
		
		Revision 1.2  2002/09/12 17:55:18  neeri
		Introduce level controls
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
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
