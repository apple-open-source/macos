/*
	File:		MBCEngine.mm
	Contains:	An agent representing the sjeng chess engine
	Version:	1.0
	Copyright:	© 2002-2007 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCEngine.mm,v $
		Revision 1.21  2007/03/02 20:03:57  neerache
		Fix undo timing problems <rdar://problem/4139329>
		
		Revision 1.20  2007/01/16 08:29:20  neerache
		Reset search depth when switching to time based level
		
		Revision 1.19  2007/01/16 03:55:02  neerache
		TTS works again in LP64 <rdar://problem/4899456>
		
		Revision 1.18  2004/09/11 02:03:23  neerache
		Implement delays to humanize engine
		
		Revision 1.17  2004/08/16 07:48:14  neerache
		Add weaker levels
		
		Revision 1.16  2003/08/15 00:26:46  neerache
		Reset deferred takeback flag (RADAR 3373117)
		
		Revision 1.15  2003/08/13 21:22:01  neerache
		Execute deferred takebacks (RADAR 3373117)
		
		Revision 1.14  2003/07/18 22:14:26  neerache
		Disable pondering during drag to improve interactive performance (RADAR 2736549)
		
		Revision 1.13  2003/07/07 08:49:01  neerache
		Improve startup time
		
		Revision 1.12  2003/06/30 05:15:06  neerache
		Use proper move generator instead of engine
		
		Revision 1.11  2003/05/27 07:25:09  neerache
		Restart engine when loading
		
		Revision 1.10  2003/05/27 03:13:57  neerache
		Rework game loading/saving code
		
		Revision 1.9  2003/05/24 20:28:27  neerache
		Address race conditions between ploayer and engine
		
		Revision 1.8  2003/04/24 23:21:40  neeri
		Fix takebacks
		
		Revision 1.7  2003/04/10 23:03:17  neeri
		Load positions
		
		Revision 1.6  2003/04/05 05:45:08  neeri
		Add PGN export
		
		Revision 1.5  2003/03/28 01:29:53  neeri
		Support hints, last move
		
		Revision 1.4  2002/10/08 22:54:59  neeri
		Engine logging, better autorelease
		
		Revision 1.3  2002/09/13 23:57:06  neeri
		Support for Crazyhouse display and mouse
		
		Revision 1.2  2002/09/12 17:55:18  neeri
		Introduce level controls
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
*/

#import "MBCEngine.h"
#import "MBCEngineCommands.h"
#import "MBCController.h"

#include <unistd.h>
#include <algorithm>

//
// Paradoxically enough, moving as quickly as possible is 
// not necessarily desirable. Users tend to get frustrated 
// once they realize how little time their Mac really spends 
// to crush them at low levels. In the interest of promoting 
// harmonious Human - Machine relations, we enforce minimum
// response times.
//
const NSTimeInterval kInteractiveDelay	= 2.0;
const NSTimeInterval kAutomaticDelay	= 4.0;

using std::max;

@implementation MBCEngine

- (id) init
{
	fEngineEnabled	= false;
	fSetPosition	= false;
	fTakeback		= false;
	fNeedsGo		= false;
	fLastMove	 	= nil;
	fLastPonder	 	= nil;
	fLastEngineMove	= nil;
	fDontMoveBefore	= [NSDate timeIntervalSinceReferenceDate];
	fMainRunLoop = [NSRunLoop currentRunLoop];
	fEngineMoves = [[NSPort port] retain];
	[fEngineMoves setDelegate:self];
	fMove = [[NSPortMessage alloc] initWithSendPort: fEngineMoves
								   receivePort: fEngineMoves
								   components: [NSArray array]];
	[self enableEngineMoves:YES];
	fEngineTask 	= [[NSTask alloc] init];
	fToEnginePipe	= [[NSPipe alloc] init];
	fFromEnginePipe = [[NSPipe alloc] init];
	[fEngineTask setStandardInput:fToEnginePipe];
	[fEngineTask setStandardOutput:fFromEnginePipe];
	[fEngineTask setLaunchPath:
					 [[NSBundle mainBundle] pathForResource:@"sjeng" 
											ofType:@"ChessEngine"]];
	[fEngineTask setArguments: [NSArray arrayWithObject:@"sjeng (Chess Engine)"]];
	[self performSelector:@selector(launchEngine:) withObject:nil afterDelay:0.001];
	fToEngine		= [fToEnginePipe fileHandleForWriting];
	fFromEngine		= [fFromEnginePipe fileHandleForReading];
	[NSThread detachNewThreadSelector:@selector(runEngine:) toTarget:self
			  withObject:nil];
	[self writeToEngine:@"xboard\nconfirm_moves\n"];

	return self;
}

- (void) launchEngine:(id)arg
{
	[fEngineTask launch];
}

- (void) shutdown
{
	//
	// Since there is only one Engine per app run, we don't bother
	// deallocating all the resources.
	//
	[self writeToEngine:@"?exit\n"];
}

- (void) writeToEngine:(NSString *)string
{
	NSData * data = [string dataUsingEncoding:NSASCIIStringEncoding];

	[[MBCController controller] logToEngine:string];
	[fToEngine writeData:data];
}

- (void) interruptEngine
{
	[self writeToEngine:@"?"];
}

- (void) setSearchTime:(int)time
{
	fSearchTime = time;
	if (fSearchTime < 0)
		[self writeToEngine:[NSString stringWithFormat:@"sd %d\n", 
									  4+fSearchTime]];
	else
		[self writeToEngine:[NSString stringWithFormat:@"sd 40\nst %d\n", 
									  fSearchTime]];
}

- (MBCMove *) lastPonder
{
	return fLastPonder;
}

- (MBCMove *) lastEngineMove
{
	return fLastEngineMove;
}

- (void) runEngine:(id) sender
{
    unsigned			cmd;
	NSAutoreleasePool * pool  = [[NSAutoreleasePool alloc] init];

    [[[NSThread currentThread] threadDictionary] 
		setObject:fFromEngine forKey:@"InputHandle"];

    while (cmd = yylex()) {
		[fMove setMsgid:cmd];
		[fMove sendBeforeDate:[NSDate distantFuture]];
		[pool release];
		pool  = [[NSAutoreleasePool alloc] init];
    }
}

- (void) enableEngineMoves:(BOOL)enable
{
	if (enable != fEngineEnabled)
		if (fEngineEnabled = enable) 
			[fMainRunLoop addPort:fEngineMoves forMode:NSDefaultRunLoopMode];
		else
			[fMainRunLoop removePort:fEngineMoves forMode:NSDefaultRunLoopMode];
}

- (void) takebackNow
{
	[fLastPonder release];
	fLastPonder = nil;
	[self writeToEngine:@"remove\n"];

	[[NSNotificationCenter defaultCenter] 
		postNotificationName:MBCTakebackNotification
		object:nil];		
}

- (void) executeMove:(MBCMove *) move;
{
	[self flipSide];
	[fLastPonder release];
	fLastPonder = nil;
	[fLastEngineMove release];
	fLastEngineMove	= [move retain];
	[[NSNotificationCenter defaultCenter] 
		postNotificationName:[self notificationForSide]
		object:move];		
}

- (void) handlePortMessage:(NSPortMessage *)message
{
	MBCMove	* move = [MBCMove moveFromCompactMove:[message msgid]];

	if (fWaitForStart) { // Suppress all commands until next start
		if (move->fCommand == kCmdStartGame) {
			fWaitForStart = false;
		}
		return;
	}
	//
	// Otherwise, handle move confirmations or rejections here and
	// broadcast the rest of the moves
	//
	switch (move->fCommand) {
	case kCmdUndo:
		//
		// Last unchecked move was rejected
		//
		fThinking = false;
		[[NSNotificationCenter defaultCenter] 
			postNotificationName:MBCIllegalMoveNotification
			object:move];
		break;
	case kCmdMoveOK:
		if (fLastMove) { // Ignore confirmations of game setup moves
			[self flipSide];
			//
			// Suspend processing until move performed on board
			//
			[self enableEngineMoves:NO]; 
			[[NSNotificationCenter defaultCenter] 
				postNotificationName:[self notificationForSide]
				object:fLastMove];
			if (fNeedsGo) {
				fNeedsGo	= false;
				[self writeToEngine:@"go\n"];
			}
		}
		break;
	case kCmdPMove:
	case kCmdPDrop:
		[fLastPonder release];
		fLastPonder	=	[move retain];
		break;
	case kCmdWhiteWins:
	case kCmdBlackWins:
	case kCmdDraw:
		[[NSNotificationCenter defaultCenter] 
			postNotificationName:MBCGameEndNotification
			object:move];
		break;
	default:
		if (fSide == kBothSides) 
			[self writeToEngine:@"go\n"]; // Trigger next move
		else
			fThinking = false;
		//	
		// After the engine moved, we defer further moves until the
		// current move is executed on the board
		//
		[self enableEngineMoves:NO];
		
		NSTimeInterval now = [NSDate timeIntervalSinceReferenceDate];
		[self performSelector:@selector(executeMove:) withObject:move 
			  afterDelay: fDontMoveBefore-now];

		if (fSide == kBothSides)
			fDontMoveBefore = max(now,fDontMoveBefore)+kAutomaticDelay;

		break;
	}
}

- (void) flipSide
{
	fLastSide = (fLastSide == kBlackSide) ? kWhiteSide : kBlackSide;
}

- (NSString *) notificationForSide
{
	return (fLastSide==kWhiteSide) 
		? MBCWhiteMoveNotification
		: MBCBlackMoveNotification;
}

- (void) initGame:(MBCVariant)variant
{
	[self writeToEngine:@"?new\n"];
	switch (variant) {
	case kVarCrazyhouse:
		[self writeToEngine:@"variant crazyhouse\n"];
		break;
	case kVarSuicide:
		[self writeToEngine:@"variant suicide\n"];
		break;
	case kVarLosers:
		[self writeToEngine:@"variant losers\n"];
		break;
	default:
		// Regular Chess
		break;
	}
	[self setSearchTime:fSearchTime];
	fTakeback = false;
}

- (void) setGame:(MBCVariant)variant fen:(NSString *)fen holding:(NSString *)holding moves:(NSString *)moves
{
	[self initGame:variant];

	fSetPosition	= true;
	[fLastMove release];
	fLastMove		= nil;

	const char * s = [fen cString];
	while (isspace(*s))
		++s;
	while (!isspace(*s))
		++s;
	while (isspace(*s))
		++s;
	fLastSide	= *s == 'w' ? kBlackSide : kWhiteSide;

	if (moves) {
		[self writeToEngine:@"force\n"];		
		[self writeToEngine:moves];
	} else {
		if (*s == 'b')
			[self writeToEngine:@"black\n"];
	
		[self writeToEngine:
				  [NSString stringWithFormat:@"setboard %@\n", fen]];
	
		if (variant == kVarCrazyhouse)
			[self writeToEngine:
					  [NSString stringWithFormat:@"holding %@\n", holding]];
	}
}

- (void) startGame:(MBCVariant)variant playing:(MBCSide)sideToPlay
{
	if (!fSetPosition) {
		[self initGame:variant];
		fLastSide = kBlackSide;
	} else {
		fNeedsGo	 = sideToPlay != kNeitherSide;
		fSetPosition = false;
	}

	[[NSNotificationCenter defaultCenter] removeObserver:self];
	switch (fSide = sideToPlay) {
	case kWhiteSide:
		[[NSNotificationCenter defaultCenter] 
			addObserver:self
			selector:@selector(opponentMoved:)
			name:MBCUncheckedBlackMoveNotification
			object:nil];
		break;
	case kBothSides:
		[self writeToEngine:@"go\n"];
		fThinking = true;
		break;
	case kNeitherSide:
		[[NSNotificationCenter defaultCenter] 
			addObserver:self
			selector:@selector(opponentMoved:)
			name:MBCUncheckedWhiteMoveNotification
			object:nil];
		[[NSNotificationCenter defaultCenter] 
			addObserver:self
			selector:@selector(opponentMoved:)
			name:MBCUncheckedBlackMoveNotification
			object:nil];
		[self writeToEngine:@"force\n"];
		fThinking = false;
		break;
	default:
		// Engine plays black
		[[NSNotificationCenter defaultCenter] 
			addObserver:self
			selector:@selector(opponentMoved:)
			name:MBCUncheckedWhiteMoveNotification
			object:nil];
		break;
	}
	if (fSide == kWhiteSide || fSide == kBlackSide) 
		if (fThinking = (fSide != fLastSide)) {
			fNeedsGo	= false;
			[self writeToEngine:@"go\n"];
		}

	[[NSNotificationCenter defaultCenter] 
		addObserver:self
		selector:@selector(moveDone:)
		name:MBCEndMoveNotification
		object:nil];
	[self enableEngineMoves:YES];
	fWaitForStart	= true;	// Suppress further moves until start
}

- (void) moveDone:(NSNotification *)notification
{
	[fLastPonder release];
	fLastPonder = nil;
	if (fTakeback) {
		fTakeback = false;
		[self takebackNow];
	}
	[self enableEngineMoves:YES];
}

- (void) takeback
{
	if (fThinking) {
		//
		// Defer
		//
		fTakeback = true;
		[self interruptEngine];
	} else if (!fEngineEnabled) {
		//
		// Move yet to be executed
		//
		fTakeback = true; 
	} else 
		[self takebackNow];
}

- (id)retain
{
	return [super retain];
}

- (void) opponentMoved:(NSNotification *)notification
{
	//
	// Got a human move, ask engine to verify it
	//
	const char * piece	= " KQBNRP  kqbnrp ";
	MBCMove *    move 	= reinterpret_cast<MBCMove *>([notification object]);

	[fLastMove release];
	fLastMove	= [move retain];

	switch (move->fCommand) {
	case kCmdMove:
		if (move->fPromotion) 
			[self writeToEngine:
					  [NSString stringWithFormat:@"%@%@%c\n", 
								[self squareToCoord:move->fFromSquare],
								[self squareToCoord:move->fToSquare],
								piece[move->fPromotion]]];
		else
			[self writeToEngine:
					  [NSString stringWithFormat:@"%@%@\n", 
								[self squareToCoord:move->fFromSquare],
								[self squareToCoord:move->fToSquare]]];
		fThinking = fSide != kNeitherSide;
		break;
	case kCmdDrop:
		[self writeToEngine:
				  [NSString stringWithFormat:@"%c@%@\n", 
							piece[move->fPiece],
							[self squareToCoord:move->fToSquare]]];
		fThinking = fSide != kNeitherSide;
		break;
	default:
		break;
	}	
	fDontMoveBefore	= [NSDate timeIntervalSinceReferenceDate]+kInteractiveDelay;
}

- (NSString *) squareToCoord:(MBCSquare)square
{
	const char * 	row 	= "12345678";
	const char * 	col 	= "abcdefgh";

	return [NSString stringWithFormat:@"%c%c", 
					 col[square % 8], row[square / 8]];
}

@end

void MBCIgnoredText(const char * text)
{
	// fprintf(stderr, "* %s", text);
}

int MBCReadInput(char * buf, int max_size)
{
	NSFileHandle *	f	=
		[[[NSThread currentThread] threadDictionary] 
			objectForKey:@"InputHandle"];

	ssize_t sz = read([f fileDescriptor], buf, max_size);
	if (sz > 0)
		[[MBCController controller] 
			logFromEngine: [NSString stringWithCString:buf length:sz]];
	return sz;
}

// Local Variables:
// mode:ObjC
// End:
