/*
	File:		MBCController.mm
	Contains:	The controller tying the various agents together
	Version:	1.0
	Copyright:	© 2002-2011 by Apple Computer, Inc., all rights reserved.
*/

#import "MBCController.h"
#import "MBCBoard.h"
#import "MBCEngine.h"
#import "MBCBoardView.h"
#import "MBCBoardViewMouse.h"
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

- (void)copyOpeningBook:(NSString *)book
{
	NSFileManager * mgr = [NSFileManager defaultManager];
	NSString *		from;
	
	if ([mgr fileExistsAtPath:book]) 
		return;	/* Opening book already exists */
	from = [[NSBundle mainBundle] pathForResource:book ofType:@""];
	if (![mgr fileExistsAtPath:from])
		return; /* We don't have this book at all */
	[mgr copyItemAtPath:from toPath:book error:nil];
}

static NSString * sVariants[] = {
	@"normal", @"crazyhouse", @"suicide", @"losers", nil
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
	NSURL *			appSupport	= 
	[[mgr URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask 
		appropriateForURL:nil create:YES error:nil] URLByAppendingPathComponent:@"Chess"];
	NSString*	appSuppDir	= [appSupport path];
	[mgr createDirectoryAtPath:appSuppDir withIntermediateDirectories:YES attributes:nil error:nil];
	[mgr changeCurrentDirectoryPath:appSuppDir];

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
	if (!fView)
		[self setBoardView:NO];
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

- (NSWindow *) gameInfoWindow
{
	return fGameInfoWindow;
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

- (NSDictionary *)localizationForVoice:(NSString *)voice
{
	NSString * localeID 	= [[NSSpeechSynthesizer attributesForVoice:voice]
								  valueForKey:NSVoiceLocaleIdentifier];
	if (!localeID)
		return nil;

	NSBundle * mainBundle	= [NSBundle mainBundle];
	NSArray  * preferred    = [NSBundle preferredLocalizationsFromArray:[mainBundle localizations]
														 forPreferences:[NSArray arrayWithObject:localeID]];
	if (!preferred)
		return nil;

	for (NSString * tryLocale in preferred)
		if (NSURL * url = [mainBundle URLForResource:@"Spoken" withExtension:@"strings"
										 subdirectory:nil localization:tryLocale]
			)
			return [NSDictionary dictionaryWithContentsOfURL:url];
	return nil;
}

- (void)awakeFromNib
{
#ifdef CHESS_TUNER
	[MBCTuner makeTuner];
#endif
	[fMainWindow setAspectRatio:NSMakeSize(740.0, 680.0)];

	[fBoardStyle removeAllItems];
	[fPieceStyle removeAllItems];
	[fStyleLocMap removeAllObjects];

	NSFileManager *	fileManager = [NSFileManager defaultManager];
	NSString	  * stylePath	= 
		[[[NSBundle mainBundle] resourcePath] 
			stringByAppendingPathComponent:@"Styles"];
	NSEnumerator  * styles 		= 
		[[fileManager contentsOfDirectoryAtPath:stylePath error:nil] objectEnumerator];
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
	NSString * defaultVoice	= [defaults objectForKey:kMBCDefaultVoice];
	if (![NSSpeechSynthesizer attributesForVoice:defaultVoice]) {
		defaultVoice		= [[defaults volatileDomainForName:NSRegistrationDomain]
								  objectForKey:kMBCDefaultVoice];
		if (![NSSpeechSynthesizer attributesForVoice:defaultVoice]) 
			defaultVoice	= nil;
	}	
	fDefaultSynth			= [[NSSpeechSynthesizer alloc] initWithVoice:defaultVoice];
	fDefaultLocalization 	= [[self localizationForVoice:defaultVoice] retain];

	NSString * altVoice		= [defaults objectForKey:kMBCAlternateVoice];
	if (![NSSpeechSynthesizer attributesForVoice:altVoice]) {
		altVoice			= [[defaults volatileDomainForName:NSRegistrationDomain]
								  objectForKey:kMBCAlternateVoice];
		if (![NSSpeechSynthesizer attributesForVoice:altVoice]) 
			altVoice	= nil;
	}	
	fAlternateSynth			= [[NSSpeechSynthesizer alloc] initWithVoice:altVoice];
	fAlternateLocalization 	= [[self localizationForVoice:altVoice] retain];

	[self loadVoiceMenu:fComputerVoice 	withSelectedVoice:defaultVoice];
	[self loadVoiceMenu:fAlternateVoice withSelectedVoice:altVoice];
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
		NSURL *			libLog	= 
			[[mgr URLForDirectory:NSLibraryDirectory inDomain:NSUserDomainMask 
				  appropriateForURL:nil create:YES error:nil] URLByAppendingPathComponent:@"Logs"];
		NSString*		logDir	= [libLog path];
		[mgr createDirectoryAtPath:logDir withIntermediateDirectories:YES attributes:nil error:nil];
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
	[fDocument updateChangeCount:NSChangeDone];

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
	[fDocument updateChangeCount:NSChangeDone];

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
	[fDocument saveDocument:sender];
	[fMainWindow setDocumentEdited:NO];
}

- (IBAction) saveGameAs:(id)sender
{
	[fDocument saveDocumentAs:sender];
	[fMainWindow setDocumentEdited:NO];
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

- (BOOL) loadGame:(NSDictionary *)dict
{
	if (!fView)
		[self setBoardView:NO];

	[fLastLoad release];
	fLastLoad = [dict retain]; // So we can store key values
	NSString * v = [dict objectForKey:@"Variant"];
	
	for (fVariant = kVarNormal; sVariants[fVariant] && ![v isEqual:sVariants[fVariant]]; )
		fVariant = static_cast<MBCVariant>(fVariant+1);
	if (!sVariants[fVariant])
		fVariant = kVarNormal;
	
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
	return YES;
}

- (BOOL)applicationOpenUntitledFile:(NSApplication *)sender
{
	NSError * error;
	[self setBoardView:NO];
	if (![[NSDocumentController sharedDocumentController] 
		 openDocumentWithContentsOfURL:[MBCDocument casualGameSaveLocation] 
							display:YES error:&error])
			[self setBoardView:YES];
	
	return YES;
}

- (BOOL)application:(NSApplication *)app openFile:(NSString *)filename
{
	NSError * error;
	[self setBoardView:NO];
	return [[NSDocumentController sharedDocumentController]
			openDocumentWithContentsOfURL:[NSURL fileURLWithPath:filename] display:YES error:&error]
		!= nil;
}

- (void)applicationDidFinishLaunching:(NSNotification *)n
{
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

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
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

- (NSDictionary *) defaultLocalization
{
	return fDefaultLocalization;
}

- (NSDictionary *) alternateLocalization
{
	return fAlternateLocalization;
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

	if (defaultIdx > 0)
		defaultID	= [voices objectAtIndex:defaultIdx-kNumFixedMenuItems];
	if (alternateIdx > 0)
		alternateID	= [voices objectAtIndex:alternateIdx-kNumFixedMenuItems];

	[fDefaultSynth setVoice:defaultID];
	[fAlternateSynth setVoice:alternateID];

	[fDefaultLocalization autorelease];
	fDefaultLocalization = [[self localizationForVoice:defaultID] retain];
	[fAlternateLocalization autorelease];
	fAlternateLocalization = [[self localizationForVoice:alternateID] retain];

	[defaults setObject:defaultID forKey:kMBCDefaultVoice];
	[defaults setObject:alternateID forKey:kMBCAlternateVoice];
	
	NSSpeechSynthesizer *	selectedSynth = nil;
	if (sender == fComputerVoice) {
		selectedSynth	= fDefaultSynth;
	} else if (sender == fAlternateVoice) {
		selectedSynth	= fAlternateSynth;
	}
	if (selectedSynth) {
		NSString *  demoText	= 
			[[NSSpeechSynthesizer attributesForVoice:[selectedSynth voice]] 
			 objectForKey:NSVoiceDemoText];
		if (demoText)
			[selectedSynth startSpeakingString:demoText];
	}
}

- (void)setDocument:(NSDocument *)doc
{
	fDocument = doc;
	if ([[fMainWindow windowController] document] != fDocument) {
		[fDocument addWindowController:[fMainWindow windowController]];
		if (fDocument) {
			if (fLastLoad) {
				if (![fDocument fileURL])
					[fDocument updateChangeCount:NSChangeDone];
			} else if (!fView) {
				[self setBoardView:NO];
				[fBoard startGame:fVariant];
				[self startGame];
			}
		}
	}
}

- (NSSize)window:(NSWindow *)window willUseFullScreenContentSize:(NSSize)size
{
	return size;
}
@end

@implementation MBCDocumentController 

- (id)init
{
	[super init];

	return self;
}

- (void) addDocument:(NSDocument *)doc
{
	//
	// There can only be one!
	//
	[[self documents] makeObjectsPerformSelector:@selector(close)];

	[super addDocument:doc];
	[[MBCController controller] setDocument:doc];
	[self setAutosavingDelay:5.0];
}

- (void) removeDocument:(NSDocument *)doc
{
	[[MBCController controller] setDocument:nil];
	[[NSNotificationQueue defaultQueue] 
		dequeueNotificationsMatching:[NSNotification notificationWithName:MBCEndMoveNotification object:nil] 
	  coalesceMask:NSNotificationCoalescingOnName];
	[super removeDocument:doc];
}

- (void) noteNewRecentDocumentURL:(NSURL *)absoluteURL
{
	//
	// Should never mention casual game location in recent documents
	//
	if (![absoluteURL isEqual:[MBCDocument casualGameSaveLocation]])
		[super noteNewRecentDocumentURL:absoluteURL];
}

@end

// Local Variables:
// mode:ObjC
// End:
