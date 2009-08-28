/*
	File:		MBCController.mm
	Contains:	The controller tying the various agents together
	Version:	1.0
	Copyright:	© 2002-2008 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCController.mm,v $
		Revision 1.47  2008/11/20 23:13:11  neerache
		<rdar://problem/5937079> Chess Save menu items/dialogs are incorrect
		<rdar://problem/6328581> Update Chess copyright statements
		
		Revision 1.46  2008/10/24 22:21:06  neerache
		<rdar://problem/5747583> Chess - turning on/off Allow Player to Speak Moves requires application relaunch to take effect
		
		Revision 1.45  2008/04/22 19:47:41  neerache
		<rdar://problem/5750936> Adoption of Clean / Dirty API by Chess
		
		Revision 1.44  2007/03/02 07:40:46  neerache
		Revise document handling & saving <rdar://problems/3776337&4186113>
		
		Revision 1.43  2007/03/01 23:51:26  neerache
		Offer option to speak human moves <rdar://problem/4038206>
		
		Revision 1.42  2007/03/01 19:53:31  neerache
		Update move window on load <rdar://problem/3852844>
		
		Revision 1.41  2007/01/17 06:10:12  neerache
		Make last move / hint speakable <rdar://problem/4510483>
		
		Revision 1.40  2007/01/16 08:29:39  neerache
		Log from beginning
		
		Revision 1.39  2007/01/16 03:55:02  neerache
		TTS works again in LP64 <rdar://problem/4899456>
		
		Revision 1.38  2006/07/27 04:06:05  neerache
		Disable TTS for LP64 <rdar://problem/4654447>
		
		Revision 1.37  2004/12/20 09:39:29  neerache
		Implement self test (RADAR 3590419 / Feature 8905)
		
		Revision 1.36  2004/09/08 00:35:49  neerache
		Deal with non-ASCII characters in file names
		
		Revision 1.35  2004/08/16 07:49:23  neerache
		Support flexible voices, weaker levels, accessibility
		
		Revision 1.34  2003/08/13 21:23:11  neerache
		Open games double clicked in the finder (RADAR 2811246)
		
		Revision 1.33  2003/08/11 22:55:41  neerache
		Loading was unreliable (RADAR 2811246)
		
		Revision 1.32  2003/08/05 23:39:04  neerache
		Remove floating board (RADAR 3361896)
		
		Revision 1.31  2003/07/07 23:50:58  neerache
		Make tuner work again
		
		Revision 1.30  2003/07/07 09:16:42  neerache
		Textured windows are too slow for low end machines, disable
		
		Revision 1.29  2003/07/07 08:46:52  neerache
		Localize Style Names
		
		Revision 1.28  2003/07/03 08:12:51  neerache
		Use sheets for saving (RADAR 3093283)
		
		Revision 1.27  2003/07/03 03:19:15  neerache
		Logarithmic time control, more UI tweaks
		
		Revision 1.26  2003/07/02 21:06:16  neerache
		Move about box into separate class/nib
		
		Revision 1.25  2003/06/30 05:16:30  neerache
		Transfer move execution to Controller
		
		Revision 1.24  2003/06/16 05:28:32  neerache
		Added move generation facility
		
		Revision 1.23  2003/06/16 02:18:03  neerache
		Implement floating board
		
		Revision 1.22  2003/06/05 08:31:26  neerache
		Added Tuner
		
		Revision 1.21  2003/06/04 23:14:05  neerache
		Neater manipulation widget; remove obsolete graphics options
		
		Revision 1.20  2003/06/02 05:44:48  neerache
		Implement direct board manipulation
		
		Revision 1.19  2003/06/02 04:21:17  neerache
		Remove gameEnd:, fUseLight
		
		Revision 1.18  2003/05/27 03:13:57  neerache
		Rework game loading/saving code
		
		Revision 1.17  2003/05/24 20:29:49  neerache
		Add game info, improve diagnostics
		
		Revision 1.16  2003/05/02 01:14:25  neerache
		Debug hook
		
		Revision 1.15  2003/04/29 00:02:58  neerache
		Stash our stuff in Application Support
		
		Revision 1.14  2003/04/28 22:14:13  neerache
		Let board, not engine, handle last move
		
		Revision 1.13  2003/04/25 16:37:00  neerache
		Clean automake build
		
		Revision 1.12  2003/04/24 23:22:02  neeri
		Implement persistent preferences, tweak UI
		
		Revision 1.11  2003/04/10 23:03:17  neeri
		Load positions
		
		Revision 1.10  2003/04/05 05:45:08  neeri
		Add PGN export
		
		Revision 1.9  2003/04/02 18:21:09  neeri
		Support saving games
		
		Revision 1.8  2003/03/28 01:31:07  neeri
		Support hints, last move
		
		Revision 1.7  2002/12/04 02:26:28  neeri
		Fix updating when style changes
		
		Revision 1.6  2002/10/15 22:49:40  neeri
		Add support for texture styles
		
		Revision 1.5  2002/10/08 22:56:23  neeri
		Engine logging, color preferences
		
		Revision 1.4  2002/09/13 23:57:05  neeri
		Support for Crazyhouse display and mouse
		
		Revision 1.3  2002/09/12 17:55:18  neeri
		Introduce level controls
		
		Revision 1.2  2002/08/26 23:11:17  neeri
		Switched to Azimuth/Elevation based Camera positioning model
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
*/

#import "MBCController.h"
#import "MBCBoard.h"
#import "MBCEngine.h"
#import "MBCBoardView.h"
#import "MBCInteractivePlayer.h"
#import "MBCDocument.h"
#import "MBCGameInfo.h"
#import "MBCBoardAnimation.h"
#import "MBCMoveAnimation.h"

#ifdef CHESS_TUNER
#import "MBCTuner.h"
#endif

#include <math.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>

#import <CoreFoundation/CFLogUtilities.h>

NSString * kMBCBoardAngle		= @"MBCBoardAngle";
NSString * kMBCBoardSpin		= @"MBCBoardSpin";
NSString * kMBCBoardStyle		= @"MBCBoardStyle";
NSString * kMBCListenForMoves	= @"MBCListenForMoves";
NSString * kMBCPieceStyle		= @"MBCPieceStyle";
NSString * kMBCSearchTime		= @"MBCSearchTime";
NSString * kMBCSpeakMoves		= @"MBCSpeakMoves";
NSString * kMBCSpeakHumanMoves	= @"MBCSpeakHumanMoves";
NSString * kMBCDefaultVoice		= @"MBCDefaultVoice";
NSString * kMBCAlternateVoice 	= @"MBCAlternateVoice";

//
// Base of logarithmic search time slider
//
const double kMBCSearchTimeBase	= 2.0;

@implementation MBCController

+ (void)initialize
{
	NSDictionary * defaults = 
		[NSDictionary dictionaryWithContentsOfFile:
						  [[NSBundle mainBundle] 
							  pathForResource:@"Defaults" ofType:@"plist"]];
	[[NSUserDefaults standardUserDefaults] registerDefaults: defaults];
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (void)copyOpeningBook:(NSString *)book
{
	NSFileManager * mgr = [NSFileManager defaultManager];
	NSString *		from;
	
	if ([mgr fileExistsAtPath:book]) 
		return;	/* Opening book already exists */
	from = [[NSBundle mainBundle] pathForResource:book ofType:@""];
	if (![mgr fileExistsAtPath:from])
		return; /* We don't have this book at all */
	[mgr copyPath:from toPath:book handler:nil];
}

static NSString * sVariants[] = {
	@"normal", @"crazyhouse", @"suicide", @"losers"
};
static const char * sVariantChars	= "nzsl";

- (void)copyOpeningBooks:(MBCVariant)variant
{
	NSString * vstring = sVariants[variant];
	char	   vchar   = sVariantChars[variant];

	[self copyOpeningBook:[NSString stringWithFormat:@"%@.opn", vstring]];
	[self copyOpeningBook:[NSString stringWithFormat:@"%cbook.db", vchar]];
}

static id	sInstance;

+ (MBCController *)controller
{
	return sInstance;
}

- (id) init
{
	sInstance 		= self;
	fEngineLogFile	= nil;
	fLastLoad		= nil;
	fVariant		= kVarNormal;
	fWhiteType		= kMBCHumanPlayer;
	fBlackType		= kMBCEnginePlayer;
	fStyleLocMap	= [[NSMutableDictionary alloc] initWithCapacity:10];

	//
	// Increase stack size limit to 8M so sjeng doesn't crash in search
	//
	struct rlimit rl;

	getrlimit(RLIMIT_STACK, &rl);
	rl.rlim_cur = 8192*1024;
	setrlimit(RLIMIT_STACK, &rl);

	//
	// Operate from ~/Library/Application Support/Chess
	//
	NSFileManager * mgr		= [NSFileManager defaultManager];
	NSArray * 	libPath 	= 
		NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, 
											NSUserDomainMask, 
											YES);
	NSString*	appSuppDir	=
		[[libPath objectAtIndex:0] 
			stringByAppendingPathComponent:@"Application Support"];
	[mgr createDirectoryAtPath:appSuppDir attributes:nil];
	NSString*	ourDir 	= 
		[appSuppDir stringByAppendingPathComponent:@"Chess"];
	[mgr createDirectoryAtPath:ourDir attributes:nil];
	[mgr changeCurrentDirectoryPath:ourDir];

	//
	// Copy the opening books
	//
	[self copyOpeningBooks:kVarNormal];
	[self copyOpeningBooks:kVarCrazyhouse];
	[self copyOpeningBooks:kVarSuicide];
	[self copyOpeningBooks:kVarLosers];
	[self copyOpeningBook:@"sjeng.rc"];


	//
	// We're responsible for executing all confirmed moves
	//
	[[NSNotificationCenter defaultCenter] 
		addObserver:self
		selector:@selector(executeMove:)
		name:MBCWhiteMoveNotification
		object:nil];
	[[NSNotificationCenter defaultCenter] 
		addObserver:self
		selector:@selector(executeMove:)
		name:MBCBlackMoveNotification
		object:nil];
	[[NSNotificationCenter defaultCenter] 
		addObserver:self
		selector:@selector(commitMove:)
		name:MBCEndMoveNotification
		object:nil];
	[[NSNotificationCenter defaultCenter] 
		addObserver:self
		selector:@selector(didTakeback:)
		name:MBCTakebackNotification
		object:nil];
	
	fIsLogging		= false;
	fEngineBuffer	= [[NSMutableString alloc] init];

	//
	// Turn on logging if desired
	//
	int debug = 0;

	if (getenv("MBC_DEBUG"))
		debug = atoi(getenv("MBC_DEBUG"));
	if (debug & 2)
		[self toggleLogging:self];
	
	//
	// Initialize agents (the board view will report to us itself)
	//
	fView		= nil;
	fBoard		= [[MBCBoard alloc] init];
	fEngine		= [[MBCEngine alloc] init];
	fInteractive= [[MBCInteractivePlayer alloc] initWithController:self];

	return self;
}

//
// We can't be sure whether the board or the controller is first awake
//
- (void) syncViewWithController
{
	[self updateStyles:self];
	[self updateGraphicsOptions:self];
	NSWindow * window = [fView window];
	[window makeFirstResponder:fView];
	[window setAcceptsMouseMovedEvents:YES];
	[window makeKeyAndOrderFront:self];
}

- (void) setBoardView:(BOOL)startGame
{
#if HAS_FLOATING_BOARD
	fView	= fFloatingMenuItem && [fFloatingMenuItem state] 
		? fFloatingView : fOpaqueView;
#else
	fView 	= fOpaqueView;
#endif

	NSUserDefaults * 	defaults 	= [NSUserDefaults standardUserDefaults];

	fView->fElevation 	= [defaults floatForKey:kMBCBoardAngle];
	fView->fAzimuth 	= [defaults floatForKey:kMBCBoardSpin];

	if (startGame)
		[self startNewGame];

	[self syncViewWithController];
}

- (NSString *) localizedStyleName:(NSString *)name
{
	NSString * loc = NSLocalizedString(name, @"");

	return loc;
}

- (NSString *) unlocalizedStyleName:(NSString *)name
{
	NSString * unloc = [fStyleLocMap objectForKey:name];

	return unloc ? unloc : name;
}

- (IBAction) updateStyles:(id)sender;
{
	NSString *			boardStyle	= 
		[self unlocalizedStyleName:[fBoardStyle titleOfSelectedItem]];
	NSString *			pieceStyle  = 
		[self unlocalizedStyleName:[fPieceStyle titleOfSelectedItem]];

	NSUserDefaults * 	defaults 	= [NSUserDefaults standardUserDefaults];

	[defaults setObject:boardStyle forKey:kMBCBoardStyle];
	[defaults setObject:pieceStyle forKey:kMBCPieceStyle];
	[defaults synchronize];

	[fView setStyleForBoard:boardStyle pieces:pieceStyle];
#ifdef CHESS_TUNER
	[MBCTuner loadStyles];
#endif
}

- (MBCBoard *) board
{
	return fBoard;
}

- (MBCBoardView *) view
{
	return fView;
}

- (MBCInteractivePlayer *) interactive
{	
	return fInteractive;
}

- (MBCEngine *) engine
{
	return fEngine;
}

- (BOOL) speakMoves
{
	return [fSpeakMoves intValue];
}

- (BOOL) speakHumanMoves
{
	return [fSpeakHumanMoves intValue];
}

- (BOOL) listenForMoves
{
	return [fListenForMoves intValue];
}


- (void)awakeFromNib
{
#ifdef CHESS_TUNER
	[MBCTuner makeTuner];
#endif
	NSRect bounds = [fMainWindow frame];
	[fMainWindow setAspectRatio: bounds.size];

	[fBoardStyle removeAllItems];
	[fPieceStyle removeAllItems];
	[fStyleLocMap removeAllObjects];

	NSFileManager *	fileManager = [NSFileManager defaultManager];
	NSString	  * stylePath	= 
		[[[NSBundle mainBundle] resourcePath] 
			stringByAppendingPathComponent:@"Styles"];
	NSEnumerator  * styles 		= 
		[[fileManager directoryContentsAtPath:stylePath] objectEnumerator];
	while (NSString * style = [styles nextObject]) {
		NSString * locStyle = [self localizedStyleName:style];
		[fStyleLocMap setObject:style forKey:locStyle];
		NSString * s = [stylePath stringByAppendingPathComponent:style];
		if ([fileManager fileExistsAtPath:
			[s stringByAppendingPathComponent:@"Board.plist"]]
		)
			[fBoardStyle addItemWithTitle:locStyle];
		if ([fileManager fileExistsAtPath:
			[s stringByAppendingPathComponent:@"Piece.plist"]]
		)
			[fPieceStyle addItemWithTitle:locStyle];
	}
	NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
	[fBoardStyle selectItemWithTitle:
					 [self localizedStyleName:
							   [defaults objectForKey:kMBCBoardStyle]]];
	[fPieceStyle selectItemWithTitle:
					 [self localizedStyleName:
							   [defaults objectForKey:kMBCPieceStyle]]];
	[fListenForMoves setIntValue:[defaults boolForKey:kMBCListenForMoves]];
	[fSpeakMoves setIntValue:[defaults boolForKey:kMBCSpeakMoves]];
	[fSpeakHumanMoves setIntValue:[defaults boolForKey:kMBCSpeakHumanMoves]];
	int searchTime = [defaults integerForKey:kMBCSearchTime];
	[fEngine setSearchTime:searchTime];
	if (searchTime < 0)
		[fSearchTime setFloatValue:searchTime];
	else
		[fSearchTime setFloatValue:log(searchTime) / log(kMBCSearchTimeBase)];
#if HAS_FLOATING_BOARD
	[fFloatingMenuItem setState:[defaults integerForKey:kMBCFloatingBoard]];
#endif
	fDefaultSynth	= 
		[[NSSpeechSynthesizer alloc] 
			initWithVoice:[defaults objectForKey:kMBCDefaultVoice]];
	fAlternateSynth	= 
		[[NSSpeechSynthesizer alloc] 
			initWithVoice:[defaults objectForKey:kMBCAlternateVoice]];

	[self loadVoiceMenu:fComputerVoice 
		  withSelectedVoice:[defaults objectForKey:kMBCDefaultVoice]];
	[self loadVoiceMenu:fAlternateVoice 
		  withSelectedVoice:[defaults objectForKey:kMBCAlternateVoice]];
}

- (IBAction)updateGraphicsOptions:(id)sender
{
    [fView needsUpdate];
}

- (IBAction) updateOptions:(id)sender
{
	NSUserDefaults * 	defaults 	= [NSUserDefaults standardUserDefaults];

	[defaults setBool:[fListenForMoves intValue]
			  forKey:kMBCListenForMoves];
	[defaults setBool:[fSpeakMoves intValue]
			  forKey:kMBCSpeakMoves];
	[defaults setBool:[fSpeakHumanMoves intValue]
			  forKey:kMBCSpeakHumanMoves];
	
	[fInteractive updateNeedMouse:sender];
}

- (IBAction) updateSearchTime:(id)sender
{
	float				rawTime		= [sender floatValue];
	int					searchTime	= rawTime < 0 ? (int)floor(rawTime) :
		(int)pow(kMBCSearchTimeBase, [sender floatValue]);
	NSUserDefaults * 	defaults 	= [NSUserDefaults standardUserDefaults];

	[defaults setInteger:searchTime forKey:kMBCSearchTime];
	[fEngine setSearchTime:searchTime];
}

- (IBAction) newGame:(id)sender
{
	NSWindow * window = [fView window];
	if (![window isVisible]) {
		if (![window isMiniaturized])
			[self startNewGame];
		[window makeKeyAndOrderFront:self];
	}

	[NSApp beginSheet:fNewGamePane
		   modalForWindow:window
		   modalDelegate:nil
		   didEndSelector:nil
		   contextInfo:nil];
    [NSApp runModalForWindow:fNewGamePane];
	[NSApp endSheet:fNewGamePane];
	[fNewGamePane orderOut:self];
}

- (IBAction)startNewGame:(id)sender
{
	MBCVariant	variant;
	int			players;
	
	variant = static_cast<MBCVariant>([fGameVariant indexOfSelectedItem]);
	players = [fPlayers indexOfSelectedItem];

	fVariant	=	variant;
	switch (players) {
	case 0:	// Human vs. Human
		fWhiteType	= kMBCHumanPlayer;
		fBlackType	= kMBCHumanPlayer;
		break;
	case 1:	// Human vs. Computer
		fWhiteType	= kMBCHumanPlayer;
		fBlackType	= kMBCEnginePlayer;
		break;
	case 2:	// Computer vs. Human
		fWhiteType	= kMBCEnginePlayer;
		fBlackType	= kMBCHumanPlayer;
		break;
	case 3:	// Computer vs. Computer
		fWhiteType	= kMBCEnginePlayer;
		fBlackType	= kMBCEnginePlayer;
		break;
	}
	[self startNewGame];
	[NSApp stopModal];
}

- (IBAction)cancelNewGame:(id)sender
{
	[NSApp stopModal];
}

- (void) startGame
{
	MBCSide	human;
	MBCSide engine;

	if ([fWhiteType isEqual:kMBCHumanPlayer])
		if ([fBlackType isEqual:kMBCHumanPlayer]) {
			human	= kBothSides;
			engine  = kNeitherSide;
		} else {
			human	= kWhiteSide;
			engine	= kBlackSide;
		}
	else 
		if ([fBlackType isEqual:kMBCHumanPlayer]) {
			human	= kBlackSide;
			engine  = kWhiteSide;
		} else {
			human	= kNeitherSide;
			engine  = kBothSides;
		}
		
	[fView startGame:fVariant playing:human];
	[fEngine startGame:fVariant playing:engine];
	[fInteractive startGame:fVariant playing:human];	
	[fGameInfo startGame:fVariant playing:human];

	[fView needsUpdate];
}

- (MBCDocument *)document
{
	//
	// We don't really track changes
	//
	MBCDocument * doc = [[[NSDocumentController sharedDocumentController] documents] objectAtIndex:0];
	[doc updateChangeCount:NSChangeDone];

	return doc;
}

- (void)startNewGame
{
	[[NSDocumentController sharedDocumentController] newDocument:self];
	[fBoard startGame:fVariant];
	[self startGame];
}

- (IBAction)profileDraw:(id)sender
{
	NSLog(@"Draw Begin\n");
	[fView profileDraw];
	NSLog(@"Draw End\n");
}

//
// Built in self test (RADAR 3590419 / Feature 8905)
//
- (void)application:(NSApplication *)sender runTest:(unsigned int)testToRun duration:(NSTimeInterval)duration
{
	int		 	iteration = 0;
	NSDate * 	startTest = [NSDate date];

	do {
		//
		// Maintain our own autorelease pool so heap size does not grow 
		// excessively.
		//
		NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
		CFLogTest(0, 
				  CFSTR("Iteration:%d Message: Running test 0 [Draw Board]"), 
				  ++iteration);
		[fView profileDraw];
		[pool release];
	} while (-[startTest timeIntervalSinceNow] < duration);
}

- (IBAction)toggleLogging:(id)sender
{
	if ((fIsLogging = !fIsLogging) && !fEngineLogFile) {
		NSFileManager * mgr		= [NSFileManager defaultManager];
		NSArray * 	libPath 	= 
			NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, 
												NSUserDomainMask, 
												YES);
		NSString*	logDir	=
			[[libPath objectAtIndex:0] 
				stringByAppendingPathComponent:@"Logs"];
		[mgr createDirectoryAtPath:logDir attributes:nil];
		NSString * log	= [logDir stringByAppendingPathComponent:@"Chess.log"];
		creat([log fileSystemRepresentation], 0666);
		fEngineLogFile = [[NSFileHandle fileHandleForWritingAtPath:log] retain];
	}
}

- (void) writeLog:(NSString *)text
{
	[fEngineLogFile writeData:[text dataUsingEncoding:NSASCIIStringEncoding]];
}

- (void) logToEngine:(NSString *)text
{
	if (fIsLogging) {
		NSString * decorated =
			[NSString stringWithFormat:@">>> %@\n", 
					  [[text componentsSeparatedByString:@"\n"] 
						  componentsJoinedByString:@"\n>>> "]];
		[self writeLog:decorated];
	} 
}

- (void) logFromEngine:(NSString *)text
{
	if (fIsLogging) {
		[self writeLog:text];
	}
}

- (IBAction)takeback:(id)sender
{
	[fEngine takeback];
}

- (void) didTakeback:(NSNotification *)n
{
	[fView unselectPiece];
	[fView hideMoves];
	[fBoard undoMoves:2];
}

- (void) executeMove:(NSNotification *)notification
{
	MBCMove *    move 	= reinterpret_cast<MBCMove *>([notification object]);

	[fBoard makeMove:move];
	[fView unselectPiece];
	[fView hideMoves];	

	if (move->fAnimate)
		[MBCMoveAnimation moveAnimation:move board:fBoard view:fView];
	else
		[[NSNotificationQueue defaultQueue] 
			enqueueNotification:
				[NSNotification 
					notificationWithName:MBCEndMoveNotification
					object:move]
			postingStyle: NSPostWhenIdle];	
}

- (void) commitMove:(NSNotification *)notification
{
	[fBoard commitMove];
	[fView hideMoves];

	if ([fWhiteType isEqual:kMBCHumanPlayer] 
	 && [fBlackType isEqual:kMBCHumanPlayer]
	 && [fView facing] != kNeitherSide
	) {
		//
		// Rotate board
		//
		[MBCBoardAnimation boardAnimation:fView];
	}
}

- (IBAction) showHint:(id)sender
{
	[fView showHint:[fEngine lastPonder]];
	[fInteractive announceHint:[fEngine lastPonder]];
}

- (IBAction) showLastMove:(id)sender
{
	[fView showLastMove:[fBoard lastMove]];
	[fInteractive announceLastMove:[fBoard lastMove]];
}

- (IBAction) openGame:(id)sender
{
	[[NSDocumentController sharedDocumentController]
		openDocument:sender];
}

- (IBAction) saveGame:(id)sender
{
	[[self document] saveDocument:sender];
}

- (IBAction) saveGameAs:(id)sender
{
	[[self document] saveDocumentAs:sender];
}

#if HAS_FLOATING_BOARD
- (IBAction) toggleFloating:(id)sender
{
	[[fView window] orderOut:self];
	int newState =	![fFloatingMenuItem state];
	[fFloatingMenuItem setState:newState];
	NSUserDefaults * 	defaults 	= [NSUserDefaults standardUserDefaults];
	[defaults setBool:newState forKey:kMBCFloatingBoard];
	[self setBoardView:NO];
}
#endif

- (BOOL) loadGame:(NSString *)fileName fromDict:(NSDictionary *)dict
{
	[fLastLoad autorelease];
	fLastLoad = [dict retain]; // So we can store key values
	NSString * v = [dict objectForKey:@"Variant"];
	
	for (fVariant = kVarNormal; ![v isEqual:sVariants[fVariant]]; )
		fVariant = static_cast<MBCVariant>(fVariant+1);
	
    fWhiteType = [dict objectForKey:@"WhiteType"];
    fBlackType = [dict objectForKey:@"BlackType"];

	[fBoard reset];
	NSString *	fen		= [dict objectForKey:@"Position"];
	NSString *	holding	= [dict objectForKey:@"Holding"];
	NSString * 	moves	= [dict objectForKey:@"Moves"];

	[fBoard setFen:fen holding:holding moves:moves];
	[fEngine setGame:fVariant fen:fen holding:holding moves:moves];
	[fGameInfo setInfo:dict];
	[fGameInfo performSelector:@selector(updateMoves:) withObject:nil afterDelay:0.010];
	[fGameInfo performSelector:@selector(updateTitle:) withObject:self afterDelay:0.050];

	[self startGame];

	return YES;
}

- (NSDictionary *) saveGameToDict
{
	NSMutableDictionary * dict = 
		[NSMutableDictionary dictionaryWithObjectsAndKeys:
								 sVariants[fVariant], @"Variant",
							 fWhiteType, @"WhiteType",
							 fBlackType, @"BlackType",
							 [fBoard fen], @"Position",
							 [fBoard holding], @"Holding",
							 [fBoard moves], @"Moves",
							 nil];
	[dict addEntriesFromDictionary:[fGameInfo getInfo]];

	return dict;
}

- (BOOL) saveMovesTo:(NSString *)fileName
{
	FILE * f = fopen([fileName fileSystemRepresentation], "w");
		
	NSString * header = [fGameInfo pgnHeader];
	NSData *	encoded = [header dataUsingEncoding:NSISOLatin1StringEncoding
								  allowLossyConversion:YES];
	fwrite([encoded bytes], 1, [encoded length], f);

	//
	// Add variant tag (nonstandard, but used by xboard & Co.)
	//
	if (fVariant != kVarNormal)
		fprintf(f, "[Variant \"%s\"]\n", [sVariants[fVariant] UTF8String]);
	//
	// Mark nonhuman players
	//
	if (![fWhiteType isEqual: kMBCHumanPlayer]) 
		fprintf(f, "[WhiteType: \"%s\"]\n", [fWhiteType UTF8String]);
	if (![fBlackType isEqual: kMBCHumanPlayer]) 
		fprintf(f, "[BlackType: \"%s\"]\n", [fBlackType UTF8String]);

	[fBoard saveMovesTo:f];
	
	fputc('\n', f);
	fputs([[fGameInfo pgnResult] UTF8String], f);
	fputc('\n', f);

	fclose(f);

	return YES;
}

- (BOOL)applicationShouldOpenUntitledFile:(NSApplication *)sender
{
	return NO;
}

- (BOOL)application:(NSApplication *)app openFile:(NSString *)filename
{
	[self setBoardView:NO];
	return [[NSDocumentController sharedDocumentController]
			   openDocumentWithContentsOfFile:filename display:YES]
		!= nil;
}

- (void)applicationDidFinishLaunching:(NSNotification *)n
{
	if (!fView)
		[self setBoardView:YES];

	int debug = 0;

	if (getenv("MBC_DEBUG"))
		debug = atoi(getenv("MBC_DEBUG"));
	if (debug & 4) 
		NSLog(@"Chess finished starting\n");			
	if (debug & 1)
		[self profileDraw:self];
	if (debug & 8)
		sleep(30);
#if HAS_FLOATING_BOARD
	if (!(debug & 16)) {
		[[fFloatingMenuItem menu] removeItem:fFloatingMenuItem];
		[[fFloatingView window] release];
		fFloatingMenuItem	= nil;
		fFloatingView		= nil;
	}
#endif
}

- (void)applicationWillTerminate:(NSNotification *)n
{
	NSUserDefaults * defaults 	= [NSUserDefaults standardUserDefaults];
	[fView endGame];
	[fEngine shutdown];
	[defaults synchronize];
}

- (BOOL)validateMenuItem:(id <NSMenuItem>)menuItem
{
	if (menuItem != fTakebackMenuItem)
		return YES;
	else 
		return [fBoard canUndo] &&
		 ([fWhiteType isEqual:kMBCHumanPlayer] 
		  || [fBlackType isEqual:kMBCHumanPlayer]
		  );
}

const int kNumFixedMenuItems = 2;

- (NSSpeechSynthesizer *)defaultSynth
{
	return fDefaultSynth;
}

- (NSSpeechSynthesizer *)alternateSynth
{
	return fAlternateSynth;
}

- (void)loadVoiceMenu:(id)menu withSelectedVoice:(NSString *)voiceIdentifierToSelect 
{
    NSString *		voiceIdentifier 		= NULL;
    NSEnumerator *	voiceEnumerator 		= 
		[[NSSpeechSynthesizer availableVoices] objectEnumerator];
    UInt32			curMenuItemIndex 		= kNumFixedMenuItems;
    UInt32			menuItemIndexToSelect	= 0;
    while (voiceIdentifier = [voiceEnumerator nextObject]) {
        [menu addItemWithTitle:[[NSSpeechSynthesizer attributesForVoice:voiceIdentifier] objectForKey:NSVoiceName]];
        
        if (voiceIdentifierToSelect && [voiceIdentifier isEqualToString:voiceIdentifierToSelect])
            menuItemIndexToSelect = curMenuItemIndex;
        
        curMenuItemIndex++;
    }
    
    // Select the desired menu item.
    [menu selectItemAtIndex:menuItemIndexToSelect];
}

- (IBAction) updateVoices:(id)sender;
{
	NSUserDefaults *defaults 	= [NSUserDefaults standardUserDefaults];
	NSString *		defaultID	= nil;
	NSString * 		alternateID	= nil;
	NSArray *		voices		= [NSSpeechSynthesizer availableVoices];
	int				defaultIdx	= [fComputerVoice indexOfSelectedItem];
	int				alternateIdx= [fAlternateVoice indexOfSelectedItem];

	if (defaultIdx)
		defaultID	= [voices objectAtIndex:defaultIdx-kNumFixedMenuItems];
	if (alternateIdx)
		alternateID	= [voices objectAtIndex:alternateIdx-kNumFixedMenuItems];

	[fDefaultSynth setVoice:defaultID];
	[fAlternateSynth setVoice:alternateID];

	[defaults setObject:defaultID forKey:kMBCDefaultVoice];
	[defaults setObject:alternateID forKey:kMBCAlternateVoice];
}

@end

@implementation MBCDocumentController 

- (id)init
{
	[super init];

	[self newDocument:self];

	return self;
}

- (void) addDocument:(NSDocument *)doc
{
	//
	// There can only be one!
	//
	[[self documents] makeObjectsPerformSelector:@selector(close)];

	[super addDocument:doc];
}

- (void) removeDocument:(NSDocument *)doc
{
	[super removeDocument:doc];
}

- (BOOL) hasEditedDocuments
{
	//	
	// Prevent review on quitting
	//
	return NO;
}

@end

// Local Variables:
// mode:ObjC
// End:
