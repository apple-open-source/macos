/*
	File:		MBCController.mm
	Contains:	The controller tying the various agents together
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

NSString * kMBCBoardAngle		= @"MBCBoardAngle";
NSString * kMBCBoardSpin		= @"MBCBoardSpin";
NSString * kMBCBoardStyle		= @"MBCBoardStyle";
NSString * kMBCListenForMoves	= @"MBCListenForMoves";
NSString * kMBCPieceStyle		= @"MBCPieceStyle";
NSString * kMBCSearchTime		= @"MBCSearchTime";
NSString * kMBCSpeakMoves		= @"MBCSpeakMoves";
NSString * kMBCFloatingBoard 	= @"MBCFloatingBoard";

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
	fLastSaved		= nil;
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
	
	//
	// Initialize agents (the board view will report to us itself)
	//
	fView		= nil;
	fBoard		= [[MBCBoard alloc] init];
	fEngine		= [[MBCEngine alloc] init];
	fInteractive= [[MBCInteractivePlayer alloc] initWithController:self];
	
	fIsLogging		= false;
	fEngineBuffer	= [[NSMutableString alloc] init];

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
	fView	= fFloatingMenuItem && [fFloatingMenuItem state] 
		? fFloatingView : fOpaqueView;

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
	int searchTime = [defaults integerForKey:kMBCSearchTime];
	[fEngine setSearchTime:searchTime];
	[fSearchTime setFloatValue:log(searchTime) / log(kMBCSearchTimeBase)];
#if HAS_FLOATING_BOARD
	[fFloatingMenuItem setState:[defaults integerForKey:kMBCFloatingBoard]];
#endif
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
}

- (IBAction) updateSearchTime:(id)sender
{
	int					searchTime	= 
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

- (void)startNewGame
{
	[fLastSaved autorelease];
	fLastSaved = nil;
	[fBoard startGame:fVariant];
	[self startGame];
}

- (IBAction)profileDraw:(id)sender
{
	NSLog(@"Draw Begin\n");
	[fView profileDraw];
	NSLog(@"Draw End\n");
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
		creat([log cString], 0666);
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
}

- (IBAction) showLastMove:(id)sender
{
	[fView showLastMove:[fBoard lastMove]];
}

- (NSWindowController *) windowController
{
	NSWindow *				window 	= [fView window];
	NSWindowController * 	ctrl 	= [window windowController];
	if (!ctrl) {
		ctrl = [[NSWindowController alloc] initWithWindow:window];
		[window setWindowController:ctrl];
	}
	return ctrl;
}

- (IBAction) openGame:(id)sender
{
	[[NSDocumentController sharedDocumentController]
		openDocument:sender];
}

- (IBAction) saveGame:(id)sender
{
	NSDocument * doc = [[MBCDocument alloc] initWithController:self];	

	if (fLastSaved)
		[doc setFileName:fLastSaved];
	[doc setFileType:@"game"];
	[doc saveDocument:sender];
	[fLastSaved autorelease];	
	fLastSaved = [[doc fileName] retain];
	[fGameInfo performSelector:@selector(updateTitle:) withObject:self afterDelay:0.010];
}

- (IBAction) saveGameAs:(id)sender
{
	NSDocument * doc = [[MBCDocument alloc] initWithController:self];	

	[doc setFileType:@"game"];
	[doc saveDocument:sender];
	[fLastSaved autorelease];	
	fLastSaved = [[doc fileName] retain];
}

- (IBAction) saveMoves:(id)sender
{
	NSDocument * doc = [[MBCDocument alloc] initWithController:self];	

	[doc setFileType:@"moves"];
	[doc saveDocumentAs:sender];
	[fGameInfo updateTitle:self];
}

- (IBAction) toggleFloating:(id)sender
{
	[[fView window] orderOut:self];
	int newState =	![fFloatingMenuItem state];
	[fFloatingMenuItem setState:newState];
	NSUserDefaults * 	defaults 	= [NSUserDefaults standardUserDefaults];
	[defaults setBool:newState forKey:kMBCFloatingBoard];
	[self setBoardView:NO];
}

- (BOOL) loadGame:(NSString *)fileName fromDict:(NSDictionary *)dict
{
	[fLastSaved autorelease];	
	fLastSaved = [fileName retain];
	[fLastLoad release];
	fLastLoad = [dict retain]; // So we can store values
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
	[fGameInfo performSelector:@selector(updateTitle:) withObject:self afterDelay:0.010];

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
	FILE * f = fopen([fileName cString], "w");
		
	NSString * header = [fGameInfo pgnHeader];
	NSData *	encoded = [header dataUsingEncoding:NSISOLatin1StringEncoding
								  allowLossyConversion:YES];
	fwrite([encoded bytes], 1, [encoded length], f);

	//
	// Add variant tag (nonstandard, but used by xboard & Co.)
	//
	if (fVariant != kVarNormal)
		fprintf(f, "[Variant \"%s\"]\n", [sVariants[fVariant] cString]);
	//
	// Mark nonhuman players
	//
	if (![fWhiteType isEqual: kMBCHumanPlayer]) 
		fprintf(f, "[WhiteType: \"%s\"]\n", [fWhiteType cString]);
	if (![fBlackType isEqual: kMBCHumanPlayer]) 
		fprintf(f, "[BlackType: \"%s\"]\n", [fBlackType cString]);

	[fBoard saveMovesTo:f];
	
	fputc('\n', f);
	fputs([[fGameInfo pgnResult] cString], f);
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
	if (debug & 2)
		[self toggleLogging:self];
	if (debug & 8)
		sleep(30);
	if (!(debug & 16)) {
		[[fFloatingMenuItem menu] removeItem:fFloatingMenuItem];
		[[fFloatingView window] release];
		fFloatingMenuItem	= nil;
		fFloatingView		= nil;
	}
}

- (void)applicationWillTerminate:(NSNotification *)n
{
	if (fView) {
		NSUserDefaults * defaults 	= [NSUserDefaults standardUserDefaults];

		[defaults setFloat:fView->fElevation forKey:kMBCBoardAngle];
		[defaults setFloat:fView->fAzimuth 	 forKey:kMBCBoardSpin];
	}

	[fView endGame];
	[fEngine shutdown];
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

@end

// Local Variables:
// mode:ObjC
// End:
