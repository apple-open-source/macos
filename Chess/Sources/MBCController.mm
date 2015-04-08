/*
	File:		MBCController.mm
	Contains:	The controller tying the various agents together
	Copyright:	© 2002-2011 by Apple Inc., all rights reserved.

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

#import "MBCController.h"
#import "MBCBoard.h"
#import "MBCEngine.h"
#import "MBCBoardView.h"
#import "MBCBoardViewMouse.h"
#import "MBCInteractivePlayer.h"
#import "MBCDocument.h"
#import "MBCGameInfo.h"
#import "MBCUserDefaults.h"
#import "MBCDebug.h"

#ifdef CHESS_TUNER
#import "MBCTuner.h"
#endif

#include <math.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>

#import <CoreFoundation/CFLogUtilities.h>

@implementation MBCController

@synthesize localPlayer, logMouse, dumpLanguageModels;

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

- (void)copyOpeningBooks:(MBCVariant)variant
{
	NSString * vstring = gVariantName[variant];
	char	   vchar   = gVariantChar[variant];

	[self copyOpeningBook:[NSString stringWithFormat:@"%@.opn", vstring]];
	[self copyOpeningBook:[NSString stringWithFormat:@"%cbook.db", vchar]];
}

- (id) init
{
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

    fMatchesToLoad      = [[NSMutableArray alloc] init];
    
	return self;
}

- (void)awakeFromNib
{
#ifdef CHESS_TUNER
	[MBCTuner makeTuner];
#endif
}

- (IBAction) newGame:(id)sender
{
    MBCDocument * doc = [[MBCDocument alloc] initForNewGameSheet:nil];
    [doc makeWindowControllers];
    [doc showWindows];
    [doc autorelease];
}

- (BOOL) hideDebugMenu
{
    return !MBCDebug::ShowDebugMenu();
}

- (BOOL) logMouse
{
    return MBCDebug::LogMouse();
}

- (void) setLogMouse:(BOOL)logIt
{
    MBCDebug::SetLogMouse(logIt);
}

- (BOOL) dumpLanguageModels
{
    return MBCDebug::DumpLanguageModels();
}

- (void) setDumpLanguageModels:(BOOL)dumpEm
{
    MBCDebug::SetDumpLanguageModels(dumpEm);
}

#ifdef TODO
//
// Built in self test
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
#endif

- (BOOL)applicationShouldOpenUntitledFile:(NSApplication *)sender
{
	return YES;
}

- (MBCDocument *)documentForMatch:(GKTurnBasedMatch *)match
{
    for (MBCDocument * doc in [[NSDocumentController sharedDocumentController] documents])
        if ([[doc.match matchID] isEqual:[match matchID]]) {
            //
            // The match object passed in by event handlers is more up to date than the one we 
            // had on file, so replace that one
            //
            doc.match = match;
            
            return doc;
        }
    return nil;
}

- (void)startNewOnlineGame:(GKTurnBasedMatch *)match withDocument:(MBCDocument *)doc
{
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
    MBCVariant       variant  = (MBCVariant)[defaults integerForKey:kMBCNewGameVariant];
    MBCSideCode      side     = (MBCSideCode)[defaults integerForKey:kMBCNewGameSides];
    [match loadMatchDataWithCompletionHandler:^(NSData *matchData, NSError *error) {
        MBCDocument * existingDoc = nil;
        if (!error && !(existingDoc = [self documentForMatch:match])) {
            [MBCDocument processNewMatch:match variant:variant side:side document:doc];
        } else { 
            [doc close];
            [existingDoc showWindows];
        }
    }];
}

- (void)loadMatch:(NSString *)matchID
{
    if (fMatchesToLoad)
        [fMatchesToLoad addObject:matchID];
    else if (fExistingMatches)
        for (GKTurnBasedMatch * match in fExistingMatches) 
            if ([match.matchID isEqual:matchID]) {
                if (![self documentForMatch:match])
                    [self startNewOnlineGame:match withDocument:nil];
                break;
            }
}

- (void)loadMatches
{
    // We should always check to make sure we have a local player before loading any matches
    
    if (self.localPlayer.isAuthenticated) {
        
        [GKTurnBasedMatch loadMatchesWithCompletionHandler:^(NSArray *matches, NSError *error) {
            if (!error) {
                [fExistingMatches autorelease];
                fExistingMatches = [matches retain];
            }
            if (fMatchesToLoad) {
                NSArray * matchesToLoad = [fMatchesToLoad autorelease];
                fMatchesToLoad          = nil;
                for (NSString * matchID in matchesToLoad)
                    [self loadMatch:matchID];
            }
            
            // When we load all the matches for the local player, we only want to load the chess board windows for the matches where it is the local player's turn.
            for (GKTurnBasedMatch * match in fExistingMatches)
                if ([match.currentParticipant.player.playerID isEqual:localPlayer.playerID])
                    [self loadMatch:match.matchID];
        }];
    }
}

- (void)setValue:(float)value forAchievement:(NSString *)ident
{
    GKAchievement * achievement = [fAchievements objectForKey:ident];
    if (!achievement) {
        achievement = [[[GKAchievement alloc] initWithIdentifier:ident] autorelease];
        [fAchievements setObject:achievement forKey:ident];
    }
    achievement.showsCompletionBanner = achievement.percentComplete < 100.0;
    achievement.percentComplete = value;
    
    [GKAchievement reportAchievements:[[NSArray alloc] initWithObjects:achievement, nil] withCompletionHandler:^(NSError *error) {
        if (error) {
            // we should report the error here
        }
    }];
}

- (void)loadAchievements
{
    [GKAchievement loadAchievementsWithCompletionHandler:^(NSArray *achievements, NSError *error) {
        fAchievements = [[NSMutableDictionary alloc] initWithCapacity:[achievements count]];
        for (GKAchievement * achievement in achievements) 
            [fAchievements setObject:achievement forKey:achievement.identifier];
    }];
}

- (void)applicationDidFinishLaunching:(NSNotification *)n
{
    [[NSDocumentController sharedDocumentController] setAutosavingDelay:3.0];
    
    [GKLocalPlayer localPlayer].authenticateHandler = ^(NSViewController *viewController, NSError *error) {
        if (viewController) {
            // if we get a view controller, that means we aren't authenticated and we should show that view controller.
    
        }
        if (error) {
            //if we get an error, we should log that error out and deal the authentication with an alert
            if ([GKLocalPlayer localPlayer].isAuthenticated) {
                [self setLocalPlayer:[GKLocalPlayer localPlayer]];
                [self loadAchievements];
                [self loadMatches];
                
                // we need to register the local player for listeners so we can react to events that we see
                [[GKLocalPlayer localPlayer] registerListener:self];
            }
        }
        else {
            // else we set the local player and set the TB event handler
            [self setLocalPlayer:[GKLocalPlayer localPlayer]];
            [self loadAchievements];
            [self loadMatches];
            
            // we need to register the local player for listeners so we can react to events that we see
            [[GKLocalPlayer localPlayer] registerListener:self];
        }
    };

}

- (void)applicationWillBecomeActive:(NSNotification *)aNotification
{
    //We should do a few things when the applications becomes active
    
}

// Adhering to HI guidlines, our application should terminate when all the windows have closed.

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return YES;
}

- (void)updateApplicationBadge
{
    static NSUInteger sPrevOurTurn = 0;
    unsigned ourTurn = 0;
    for (MBCDocument * doc in [[NSDocumentController sharedDocumentController] documents])
        if ([doc nontrivialHumanTurn])
            ++ourTurn;
    NSDockTile * tile = [NSApp dockTile];
    if (ourTurn)
        [tile setBadgeLabel:[NSString localizedStringWithFormat:@"%u", ourTurn]];
    else 
        [tile setBadgeLabel:@""];
    if (ourTurn > sPrevOurTurn)
        [NSApp requestUserAttention:NSInformationalRequest];
    sPrevOurTurn = ourTurn;
}

#pragma mark - GKTurnBasedEventListener Protocol

// didRequestMatchWithPlayers
// ============================
// This is called when the localPlayer sends a request for a match a list of players
// player = The localplayer who issued the request for match
// playerIDsToInvite = The list of requested players ID
// ============================

-(void)player:(GKPlayer *)player didRequestMatchWithPlayers:(NSArray *)playerIDsToInvite
{
    MBCDocument * doc = [[MBCDocument alloc] initForNewGameSheet:playerIDsToInvite];
    [doc makeWindowControllers];
    [doc showWindows];
    [doc autorelease];

}

// receiveTurnEventForMatch
// ============================
// This is called when the localplayer receives a turn event for a match
// player = The localplayer who received the turn even for match
// match = The match which has the turn event request
// didBecomeActive = The bool for the turn to become active
// ============================

-(void)player:(GKPlayer *)player receivedTurnEventForMatch:(GKTurnBasedMatch *)match didBecomeActive:(BOOL)didBecomeActive
{
    if (MBCDocument * doc = [self documentForMatch:match]) {
        [doc showWindows];
        [doc updateMatchForRemoteMove];
    } else
        [MBCDocument processNewMatch:match variant:kVarNormal side:kPlayEither document:nil];
}

// matchEnded
// ============================
// This is called when the localPlayer match has ended
// player = The localplayer who's match just ended
// match = The match in question
// ============================

-(void)player:(GKPlayer *)player matchEnded:(GKTurnBasedMatch *)match
{
    if (MBCDocument * doc = [self documentForMatch:match]) {
        [doc showWindows];
        [doc updateMatchForRemoteMove];
    } else
        [MBCDocument processNewMatch:match variant:kVarNormal side:kPlayEither document:nil];
}

@end

