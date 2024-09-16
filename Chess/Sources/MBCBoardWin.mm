/*
    File:		MBCBoardWin.mm
    Contains:	Manage the board window
    Copyright:	© 2003-2024 by Apple Inc., all rights reserved.

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

#import "MBCBoardWin.h"
#import "MBCBoardView.h"
#import "MBCBoardMTLView.h"
#import "MBCPlayer.h"
#import "MBCEngine.h"
#import "MBCDocument.h"
#import "MBCGameInfo.h"
#import "MBCMoveAnimation.h"
#import "MBCBoardAnimation.h"
#import "MBCInteractivePlayer.h"
#import "MBCRemotePlayer.h"
#import "MBCUserDefaults.h"
#import "MBCController.h"
#import "MBCMetalRenderer.h"


#include <SystemConfiguration/SystemConfiguration.h>
#include <UserNotifications/UserNotifications.h>
#import <Metal/Metal.h>
#import <sys/sysctl.h>

#define MAIN_WINDOW_CONTENT_ASPECT_RATIO 4.f / 3.f

@implementation MBCBoardWin

@synthesize gameView, gameMTLView, gameNewSheet, logContainer, logView, board, engine, interactive, playersPopupMenu;
@synthesize gameInfo, remote, logViewRightEdgeConstraint, dialogController;
@synthesize primarySynth, alternateSynth, primaryLocalization, alternateLocalization;

- (void)removeChessObservers
{
    if (![fObservers count])
        return;
    
    NSNotificationCenter * notificationCenter = [NSNotificationCenter defaultCenter];
    
    [fObservers enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
        [notificationCenter removeObserver:obj];
    }];
    
    [notificationCenter removeObserver:self name:MBCWhiteMoveNotification object:nil];
    [notificationCenter removeObserver:self name:MBCBlackMoveNotification object:nil];
    [notificationCenter removeObserver:self name:MBCGameEndNotification object:nil];
    [notificationCenter removeObserver:self name:MBCEndMoveNotification object:nil];
    
    MBCDocument *   document    = [self document];
    [document removeObserver:self forKeyPath:kMBCDefaultVoice];
    [document removeObserver:self forKeyPath:kMBCAlternateVoice];
    [document removeObserver:self forKeyPath:kMBCBoardStyle];
    [document removeObserver:self forKeyPath:kMBCPieceStyle];
    [document removeObserver:self forKeyPath:kMBCListenForMoves];
    
    [fObservers removeAllObjects];
}

- (void)dealloc
{
    [fCurAnimation cancel];
    [self removeChessObservers];
    [fObservers release];
    [primaryLocalization release];
    [alternateLocalization release];
    [fMetalRenderer release];
    [super dealloc];
}

- (NSView<MBCBoardViewInterface> *)renderView
{
    if ([MBCBoardWin isRenderingWithMetal]) {
        return (NSView<MBCBoardViewInterface> *)gameMTLView;
    }
    
    return (NSView<MBCBoardViewInterface> *)gameView;
}

- (void)endAnimation
{
    fCurAnimation = nil;
}

- (void)windowDidLoad
{
    [super windowDidLoad];
    
    if ([MBCBoardWin isRenderingWithMetal]) {
        // Constrainig aspect ratio to avoid Metal drawable stretching during resize.
        self.window.contentAspectRatio = NSMakeSize(MAIN_WINDOW_CONTENT_ASPECT_RATIO, 1.f);
        
        // Remove the OpenGL view and release.
        [gameView removeFromSuperview];
        gameView = nil;
        
        [self initializeForMetalRendering];
    } else {
        // Using OpenGL, remove the MTKView and release
        [gameMTLView removeFromSuperview];
        gameMTLView = nil;
    }

    if (!fObservers)
        fObservers = [[NSMutableArray alloc] init];
    
    MBCDocument *   document    = [self document];
    [document setBoard:board];
    [engine setDocument:document];
    [interactive setDocument:document];
    [gameInfo setDocument:document];
    [remote setDocument:document];

    [self removeChessObservers];
    NSNotificationCenter * notificationCenter = [NSNotificationCenter defaultCenter];
    [fObservers addObject:
        [notificationCenter
         addObserverForName:MBCGameLoadNotification object:document 
         queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
        NSLog(@"MBCGameLoadNotification");
             NSDictionary * dict    = [note userInfo];
             NSString *     fen     = [dict objectForKey:@"Position"];
             NSString *     holding = [dict objectForKey:@"Holding"];
             NSString *     moves   = [dict objectForKey:@"Moves"];
     
             if (fen.length > 0 || moves.length > 0)
                 [engine setGame:[document variant] fen:fen holding:holding moves:moves];
         }]];
    [fObservers addObject:
        [notificationCenter
         addObserverForName:MBCGameStartNotification object:document 
         queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
             MBCVariant     variant  = [document variant];
             NSLog(@"MBCGameStartNotfication with variant %d", variant);
             [[self renderView] startGame:variant playing:[document humanSide]];
             [engine setSearchTime:[document integerForKey:kMBCSearchTime]];
             [engine startGame:variant playing:[document engineSide]];
             [interactive startGame:variant playing:[document humanSide]];	
             [gameInfo startGame:variant playing:[document humanSide]];
             if (document.match)
                 [remote startGame:variant playing:[document remoteSide]];
         }]];  
    [fObservers addObject:
        [notificationCenter
         addObserverForName:MBCTakebackNotification object:document 
         queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
             [[self renderView] unselectPiece];
             [[self renderView] hideMoves];
             [board undoMoves:2];
         }]];  
	[notificationCenter
     addObserver:self
     selector:@selector(executeMove:)
     name:MBCWhiteMoveNotification
     object:document];
	[notificationCenter 
     addObserver:self
     selector:@selector(executeMove:)
     name:MBCBlackMoveNotification
     object:document];
	[notificationCenter 
     addObserver:self
     selector:@selector(gameEnded:)
     name:MBCGameEndNotification
     object:document];
	[notificationCenter 
     addObserver:self
     selector:@selector(commitMove:)
     name:MBCEndMoveNotification
     object:document];
    [document addObserver:self forKeyPath:kMBCDefaultVoice options:NSKeyValueObservingOptionNew context:nil];
    [document addObserver:self forKeyPath:kMBCAlternateVoice options:NSKeyValueObservingOptionNew context:nil];
    [document addObserver:self forKeyPath:kMBCBoardStyle options:NSKeyValueObservingOptionNew context:nil];
    [document addObserver:self forKeyPath:kMBCPieceStyle options:NSKeyValueObservingOptionNew context:nil];
    [document addObserver:self forKeyPath:kMBCListenForMoves options:NSKeyValueObservingOptionNew context:nil];
    
    if (![MBCBoardWin isRenderingWithMetal]) {
        // If rendering with Metal, the following has already been done in initializeForMetalRendering
        [self renderView].elevation            = [document floatForKey:kMBCBoardAngle];
        [self renderView].azimuth              = [document floatForKey:kMBCBoardSpin];
        
        [[self renderView] setStyleForBoard:[document objectForKey:kMBCBoardStyle]
                                     pieces:[document objectForKey:kMBCPieceStyle]];
    }
    
    [self setShouldCascadeWindows:NO];
    NSWindow * window = [self window];
    if ([[self document] match])
        [window setFrameAutosaveName:[NSString stringWithFormat:@"Match %@\n", document.match.matchID]];
    if (![document boolForKey:kMBCShowGameLog])
        [self hideLogContainer:self];
    [window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
	[window makeFirstResponder:[self renderView]];
	[window makeKeyAndOrderFront:self];
    [fObservers addObject:
        [notificationCenter
         addObserverForName:NSWindowWillCloseNotification object:window
         queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
             //
             //     Due to a plethora of mutual observers, circular references prevent
             //     proper deallocation unless we remove all of observers first.
             //
             [fCurAnimation endState];
             [board removeChessObservers];
             [engine removeChessObservers];
             [engine shutdown];
             [gameInfo removeChessObservers];
             [gameInfo setDocument:nil];
             [interactive removeChessObservers];
             [interactive removeController];
             [remote removeChessObservers];
             [self removeChessObservers];
             [self stopRecordingIfNeeded];
             if (gameView.superview) {
                 [gameView setNeedsDisplay:NO];
                 [gameView removeFromSuperview];
                 gameView = nil;
             }
             if (gameMTLView.superview) {
                 gameMTLView.delegate = nil;
                 [gameMTLView removeFromSuperview];
                 gameMTLView = nil;
             }
         }]];
    if ([document needNewGameSheet]) {
        usleep(500000);
        //shows the settings and start/cancel modal
        [self showNewGameSheet];
    }
}

- (void)windowWillLoad {
    //SharePlay
    currentSharePlayMoveStringCount = 0;
    if ([MBCUserDefaults isSharePlayEnabled] == YES) {
        //Setting the shareplay message delegate to this new window
        if ([[MBCSharePlayManager sharedInstance] connected] == NO) {
            NSLog(@"MBCSharePlayManager set BoardWindowDelegate to Self");
            [MBCSharePlayManager sharedInstance].boardWindowDelegate = self;
            [UNUserNotificationCenter currentNotificationCenter].delegate = self;
            [[MBCSharePlayManager sharedInstance] setTotalMoves:0];
        }
    }
}

- (void)windowWillClose:(NSNotification *)notification {
    NSLog(@"MBCBoardWin: windowWillClose");
    //send info that we will close
    if([[MBCSharePlayManager sharedInstance] connected] == YES && [MBCSharePlayManager sharedInstance].boardWindowDelegate == self) {
        [[MBCSharePlayManager sharedInstance] leaveSession];
    }
}

- (void)windowDidBecomeMain:(NSNotification *)notification
{
    if ([self listenForMoves]) {
        [interactive allowedToListen:YES];
    }
	[[self renderView] needsUpdate];
}

- (void)windowDidResignMain:(NSNotification *)notification
{
    if ([self listenForMoves]) {
        [interactive allowedToListen:NO];
    }
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if ([keyPath isEqual:kMBCDefaultVoice]) {
        [primarySynth release];             primarySynth            = nil;
        [primaryLocalization release];      primaryLocalization     = nil;
    } else if ([keyPath isEqual:kMBCAlternateVoice]) {
        [alternateSynth release];           alternateSynth          = nil;
        [alternateLocalization release];    alternateLocalization   = nil;
    } else if ([keyPath isEqual:kMBCListenForMoves]) {
        [interactive allowedToListen:[self listenForMoves]];
    } else {
        [[self renderView] setStyleForBoard:[[self document] objectForKey:kMBCBoardStyle]
                                     pieces:[[self document] objectForKey:kMBCPieceStyle]];
    }
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
	if ([menuItem action] == @selector(takeback:)) {
		return [[self document] canTakeback];
    } else if ([menuItem action] == @selector(toggleLogView:)) {
        BOOL logViewVisible = ! [logView isHiddenOrHasHiddenAncestor];
        [menuItem setState:(logViewVisible ? NSOnState : NSOffState)];
    } else if ([menuItem action] == @selector(toggleEdgeNotation:)) {
        BOOL edgeNotationVisible = gameMTLView.drawEdgeNotationLabels;
        [menuItem setState:(edgeNotationVisible ? NSOnState : NSOffState)];
    }
    return YES;
}

- (NSString *)windowTitleForDocumentDisplayName:(NSString *)displayName
{
    return [gameInfo gameTitle];
}

- (IBAction)takeback:(id)sender
{
    if ([[MBCSharePlayManager sharedInstance] connected] == YES && [[MBCSharePlayManager sharedInstance] boardWindowDelegate] == self) {
        [[MBCSharePlayManager sharedInstance] sendTakeBackMessage];
    }
    if ([[self document] match]) {
        [gameInfo willChangeValueForKey:@"gameTitle"];
        [[self document] offerTakeback];
        [gameInfo didChangeValueForKey:@"gameTitle"];
    } else {
        [engine takeback];
    }
}

typedef void (^MBCAlertCallback)(NSInteger returnCode);

- (void) endAlertSheet:(NSAlert *)alert returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
    MBCAlertCallback callback = (MBCAlertCallback)contextInfo;
    callback(returnCode);
    Block_release(callback);
}

- (void) requestTakeback
{
    NSAlert * alertSheet = 
        [NSAlert alertWithMessageText:NSLocalizedString(@"takeback_request_text", @"Opp wants takeback") 
                        defaultButton:NSLocalizedString(@"takeback_request_yes", @"OK") 
                      alternateButton:NSLocalizedString(@"takeback_request_no", @"No")
                          otherButton:nil informativeTextWithFormat:@""];
    for (NSButton * button in [alertSheet buttons])
        [button setKeyEquivalent:@""];
    [alertSheet beginSheetModalForWindow:[self window] modalDelegate:self
                          didEndSelector:@selector(endAlertSheet:returnCode:contextInfo:)
                             contextInfo:Block_copy(
    ^(NSInteger returnCode) {
        MBCController * controller = (MBCController *)[NSApp delegate];
        if (returnCode == NSAlertFirstButtonReturn) {
            [engine takeback];
            [controller setValue:100.0 forAchievement:@"AppleChess_Merciful"];
            [[self document] allowTakeback:YES];
        } else {
            [controller setValue:100.0 forAchievement:@"AppleChess_Cry_me_a_River"];
            [[self document] allowTakeback:NO];
        }
    })];
}

- (void) requestDraw
{
    NSAlert * alertSheet = 
    [NSAlert alertWithMessageText:NSLocalizedString(@"draw_request_text", @"Opp wants draw")        
                    defaultButton:NSLocalizedString(@"draw_request_yes", @"OK") 
                  alternateButton:NSLocalizedString(@"draw_request_no", @"No")
                      otherButton:nil informativeTextWithFormat:@""];
    for (NSButton * button in [alertSheet buttons])
        [button setKeyEquivalent:@""];
    [alertSheet beginSheetModalForWindow:[self window] modalDelegate:self
                          didEndSelector:@selector(endAlertSheet:returnCode:contextInfo:)
                             contextInfo:Block_copy(
        ^(NSInteger returnCode) {
            MBCController * controller = (MBCController *)[NSApp delegate];
            if (returnCode == NSAlertFirstButtonReturn) {
                [[NSNotificationCenter defaultCenter] 
                 postNotificationName:MBCGameEndNotification
                 object:[self document] userInfo:[MBCMove moveWithCommand:kCmdDraw]];
            } else {
                [controller setValue:100.0 forAchievement:@"AppleChess_Not_So_Fast"];
            }
        })];
}

- (void)handleRemoteResponse:(NSString *)response
{
    [gameInfo willChangeValueForKey:@"gameTitle"];
    if ([response isEqual:@"Takeback"]) {
        [engine takeback];
    } else if ([response isEqual:@"NoTakeback"]) {
        NSAlert * alertSheet = 
            [NSAlert alertWithMessageText:NSLocalizedString(@"takeback_refused", @"Opp refused") 
                            defaultButton:NSLocalizedString(@"takeback_refused_ok", @"OK") 
                          alternateButton:nil otherButton:nil 
            informativeTextWithFormat:@""];
        [alertSheet beginSheetModalForWindow:[self window] modalDelegate:self
                              didEndSelector:@selector(endAlertSheet:returnCode:contextInfo:)
                                 contextInfo:Block_copy(^(NSInteger returnCode) {})];
    }
    [gameInfo didChangeValueForKey:@"gameTitle"];
}

- (void) showNewGameSheet
{
    if (![GKLocalPlayer localPlayer].isAuthenticated || !isInternetConnection()) {
        [playersPopupMenu removeItemAtIndex:4];
    }

    if ([[self document] invitees])
        [self runMatchmakerPanel];
    else 
        [NSApp beginSheet:gameNewSheet modalForWindow:[self window] 
            modalDelegate:nil didEndSelector:nil contextInfo:nil];
}

static BOOL isInternetConnection ()
{
    BOOL returnValue = NO;
        
    struct sockaddr zeroAddress;
    memset(&zeroAddress, 0, sizeof(zeroAddress));
    zeroAddress.sa_len = sizeof(zeroAddress);
    zeroAddress.sa_family = AF_INET;
    
    SCNetworkReachabilityRef reachabilityRef = SCNetworkReachabilityCreateWithAddress(NULL, (const struct sockaddr*)&zeroAddress);
    
    
    if (reachabilityRef != NULL)
    {
        SCNetworkReachabilityFlags flags = 0;
        
        if(SCNetworkReachabilityGetFlags(reachabilityRef, &flags))
        {
            BOOL isReachable = ((flags & kSCNetworkFlagsReachable) != 0);
            BOOL connectionRequired = ((flags & kSCNetworkFlagsConnectionRequired) != 0);
            returnValue = (isReachable && !connectionRequired) ? YES : NO;
        }
        
        CFRelease(reachabilityRef);
    }
    
    return returnValue;
    
}

uint32_t sAttributesForSides[] = {
    0xFFFF0000,
    0x0000FFFF,
    0xFFFFFFFF
};

- (void) runMatchmakerPanel
{
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
    GKMatchRequest *matchRequest = [[[GKMatchRequest alloc] init] autorelease];
	matchRequest.minPlayers = 2;
	matchRequest.maxPlayers = 2;
	matchRequest.playerGroup = [defaults integerForKey:kMBCNewGameVariant];
    matchRequest.playerAttributes = sAttributesForSides[[defaults integerForKey:kMBCNewGameSides]];
	matchRequest.recipients = nil;
	
	GKTurnBasedMatchmakerViewController *shadkhan = [[[GKTurnBasedMatchmakerViewController alloc] initWithMatchRequest:matchRequest] autorelease];
	shadkhan.turnBasedMatchmakerDelegate = self;
    shadkhan.showExistingMatches = YES;
	[dialogController presentViewController:shadkhan];
}

- (IBAction)startNewGame:(id)sender
{
    [(NSUserDefaultsController *)[NSUserDefaultsController sharedUserDefaultsController] save:self];
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
    [defaults setInteger:[self searchTime] forKey:kMBCSearchTime];
    [NSApp endSheet:gameNewSheet];
    [gameNewSheet orderOut:self];
    if ([defaults integerForKey:kMBCNewGamePlayers] == kHumanVsGameCenter) {
        [self runMatchmakerPanel];
    } else {
        NSError * error;
        [[self document] initWithType:@"com.apple.chess.game" error:&error];
        [[self document] setEphemeral:NO]; // Explicitly opened, so not ephemeral        
    }
    [self willChangeValueForKey:@"hideSpeakMoves"];
    [self willChangeValueForKey:@"hideSpeakHumanMoves"];
    [self willChangeValueForKey:@"hideEngineProperties"];
    [self willChangeValueForKey:@"hideRemoteProperties"];
    [self didChangeValueForKey:@"hideSpeakMoves"];
    [self didChangeValueForKey:@"hideSpeakHumanMoves"];
    [self didChangeValueForKey:@"hideEngineProperties"];
    [self didChangeValueForKey:@"hideRemoteProperties"];
    
    // The way new documents are created in Chess will give consecutive new games
    // the same draft name if no moves are made. Setting the display name to nil
    // will force the name to be updated with incremented numbers in draft name.
    [[self document] setDisplayName:nil];
    [self synchronizeWindowTitleWithDocumentName];
}

- (IBAction)cancelNewGame:(id)sender
{
    [[NSUserDefaultsController sharedUserDefaultsController] revert:self];
    [NSApp endSheet:gameNewSheet];
    [gameNewSheet orderOut:self];
    [self close];
}

- (IBAction)resign:(id)sender
{
    [[self document] resign];
}

- (IBAction) showHint:(id)sender
{
	[[self renderView] showMoveAsHint:[engine lastPonder]];
	[interactive announceHint:[engine lastPonder]];
}

- (IBAction) showLastMove:(id)sender
{
	[[self renderView] showMoveAsLast:[board lastMove]];
	[interactive announceLastMove:[board lastMove]];
}

// Called when our animate-out animation is done
// While the logView is not visible by virtue of it being outside the window,
// hiding it ensures it won't interact with (for example) the key view loop.
- (void)hideLogContainer:(id)now {
    if (now)
        [logViewRightEdgeConstraint setConstant:NSWidth([logContainer frame])];
    [logContainer setHidden:YES];
}

- (IBAction) toggleLogView:(id)sender
{
    // Make sure that another animation isn't going to hide this at some point in the future.
    [[self class] cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideLogContainer:) object:nil];
    MBCDocument * doc       = [self document];
    BOOL currentlyShowing   = [doc boolForKey:kMBCShowGameLog];
    [doc setValue:[NSNumber numberWithBool:!currentlyShowing] forKey:kMBCShowGameLog];
    if (!currentlyShowing) {
        // We want to make it visible immediately so the user can see it animate it.
        [logContainer setHidden:NO];
        [[logViewRightEdgeConstraint animator] setConstant:0];
    } else {
        [[logViewRightEdgeConstraint animator] setConstant:NSWidth([logContainer frame])];
        // We want to keep it visible up until the end, so the user can see the animation.
        [self performSelector:@selector(hideLogContainer:) withObject:nil afterDelay:[[NSAnimationContext currentContext] duration]];
    }
}

- (void)adjustLogViewForReusedWindow
{
    //
    // Show or hide game log if necessary if window was reused
    //
    MBCDocument *   document    = [self document];
    NSWindow *      window      = [self window];
    if ([document needNewGameSheet])
        [self showNewGameSheet];
    else if ([document boolForKey:kMBCShowGameLog] == [logContainer isHidden]) {
        [document setValue:[NSNumber numberWithBool:![logContainer isHidden]] forKey:kMBCShowGameLog];
        [self toggleLogView:self];
    }
    if ([document match])
        [window setFrameAutosaveName:[NSString stringWithFormat:@"Match %@\n", document.match.matchID]];
}

- (void) gameEnded:(NSNotification *)notification
{
    NSLog(@"gameEnded");
	MBCMove *    move 	= reinterpret_cast<MBCMove *>([notification userInfo]);
    
	[board makeMove:move];
    
    BOOL weWon = NO;
    if (move->fCommand == kCmdWhiteWins && SideIncludesWhite([[self document] humanSide]))
        weWon = YES;
    if (move->fCommand == kCmdBlackWins && SideIncludesBlack([[self document] humanSide]))
        weWon = YES;
    if (weWon) {
        MBCController * controller      = (MBCController *)[NSApp delegate];
        if ([[self document] engineSide] != kNeitherSide && [[self document] integerForKey:kMBCMinSearchTime] >= 0) 
            [controller setValue:100.0 forAchievement:@"AppleChess_Luddite"];
        if ([[self document] remoteSide] != kNeitherSide) {
            [controller setValue:100.0 forAchievement:@"AppleChess_King_of_the_Cloud"];
            NSUserDefaults *    defaults    = [NSUserDefaults standardUserDefaults];
            NSDictionary *      victories   = [defaults objectForKey:kMBCGCVictories];
            if ([victories count] < 10) {
                NSMutableDictionary * v = victories 
                    ? [victories mutableCopy] : [[NSMutableDictionary alloc] init];
                for (GKTurnBasedParticipant * p in [[self document] match].participants)
                    if (![p.player.playerID isEqual:[controller localPlayer].playerID])
                        [v setObject:[NSNumber numberWithBool:YES] forKey:p.player.playerID];
                victories = [v autorelease];
            }
            [defaults setObject:victories forKey:kMBCGCVictories];
            if ([victories count] == 10)
                [controller setValue:100.0 forAchievement:@"AppleChess_Battle_Royal"];
        }
        if ([[self document] variant] == kVarSuicide || [[self document] variant] == kVarLosers)
            if ([[self board] numMoves] < 39)
                [controller setValue:100.0 forAchievement:@"AppleChess_Lightning_Loser"];
    }
}

- (void)updateAchievementsForMove:(MBCMove *)move
{
    BOOL            humanMove;
    MBCVariant      variant         = [[self document] variant];
    BOOL            notAntiChess    = variant == kVarNormal || variant == kVarCrazyhouse;
    MBCController * controller      = (MBCController *)[NSApp delegate];
    MBCPieceCode    ourColor        = Color(move->fPiece);
    MBCPieceCode    oppColor        = MBCPieceCode(Opposite(ourColor));
    
    if (ourColor == kWhitePiece)
        humanMove = SideIncludesWhite([[self document] humanSide]);
    else 
        humanMove = SideIncludesBlack([[self document] humanSide]);
    
    if (humanMove && notAntiChess) {
        MBCPieces * curPos = [[self board] curPos];
        if (move->fCheck)
            [controller setValue:100.0 forAchievement:@"AppleChess_Checker"];
        if (move->fEnPassant)
            [controller setValue:100.0 forAchievement:@"AppleChess_Sidestepped"];
        if (move->fPromotion) {
            if (Piece(move->fPromotion) == QUEEN)
                [controller setValue:100.0 forAchievement:@"AppleChess_Promotional_Value"];
            else
                [controller setValue:100.0 forAchievement:@"AppleChess_Promotional_Discount"];
        }
        if (move->fCommand == kCmdMove && Piece(move->fPiece) == PAWN
         && (labs((int)Row(move->fFromSquare)-(int)Row(move->fToSquare)) == 2)
        )
            [controller setValue:100.0 forAchievement:@"AppleChess_One_Step_Beyond"];
        if (move->fVictim && variant == kVarNormal) {
            if (Piece(move->fVictim) == PAWN || Promoted(move->fVictim))
                if (curPos->fInHand[ourColor+PAWN] == 5)
                    [controller setValue:100.0 forAchievement:@"AppleChess_Pawnbroker"];
            if (Piece(move->fVictim) == KNIGHT)
                if (curPos->fInHand[ourColor+KNIGHT] == 2)
                    [controller setValue:100.0 forAchievement:@"AppleChess_Pikeman"];
            if (curPos->NoPieces(oppColor))
                [controller setValue:100.0 forAchievement:@"AppleChess_Take_no_Prisoners"];
        }
        if (move->fCheckMate) {
            if ([[self board] numMoves] < 19)
                [controller setValue:100.0 forAchievement:@"AppleChess_Blitz"];
            if (variant == kVarNormal) {
                int materialBalance = 
                    (curPos->fInHand[ourColor+QUEEN] -curPos->fInHand[oppColor+QUEEN]) * 9
                  + (curPos->fInHand[ourColor+ROOK]  -curPos->fInHand[oppColor+ROOK])  * 5
                  + (curPos->fInHand[ourColor+KNIGHT]-curPos->fInHand[oppColor+KNIGHT])* 3
                  + (curPos->fInHand[ourColor+BISHOP]-curPos->fInHand[oppColor+BISHOP])* 3
                  + (curPos->fInHand[ourColor+PAWN]  -curPos->fInHand[oppColor+PAWN])  * 1;
                if (materialBalance <= -9)
                    [controller setValue:100.0 forAchievement:@"AppleChess_Last_Ditch_Effort"];
            } else if (variant == kVarCrazyhouse) {
                if (move->fCommand == kCmdDrop)
                    [controller setValue:100.0 forAchievement:@"AppleChess_Aerial_Attack"];
            }
        }
        if (move->fCastling != kNoCastle) {
            NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
            int sides = [defaults integerForKey:kMBCCastleSides] | move->fCastling;
            [defaults setInteger:sides forKey:kMBCCastleSides];
            if (sides == (kCastleKingside|kCastleQueenside))
                [controller setValue:100.0 forAchievement:@"AppleChess_Duck_and_Cover"];
        }
    }
}

- (void) executeMove:(NSNotification *)notification
{
    NSLog(@"MBCBoardWin executeMove");
	MBCMove *    move 	= reinterpret_cast<MBCMove *>([notification userInfo]);

	[board makeMove:move];
	[[self renderView] unselectPiece];
	[[self renderView] hideMoves];
	[[self document] updateChangeCount:NSChangeDone];
    [self updateAchievementsForMove:move];
    
    if (move->fAnimate){
        NSLog(@"Move Animation");
		fCurAnimation = [MBCMoveAnimation moveAnimation:move board:board view:[self renderView]];
        
    } else {
        NSLog(@"Posting EndMoveNotification");
		[[NSNotificationQueue defaultQueue] 
         enqueueNotification:
         [NSNotification 
          notificationWithName:MBCEndMoveNotification
          object:[self document] userInfo:(id)move]
         postingStyle: NSPostWhenIdle];
    }
    
    if ([[self document] engineSide] == kNeitherSide)
        if (MBCMoveCode cmd = [[self board] outcome]) {
            NSLog(@"Posting GameEndNotification");
            [[NSNotificationQueue defaultQueue] 
             enqueueNotification:
             [NSNotification 
              notificationWithName:MBCGameEndNotification
              object:[self document] 
              userInfo:[MBCMove moveWithCommand:cmd]]
             postingStyle: NSPostWhenIdle];
        }
}

- (void) commitMove:(NSNotification *)notification
{
    NSLog(@"MBCBoardWin CommitMove=%d", [[MBCSharePlayManager sharedInstance] connected]);
    if ([[MBCSharePlayManager sharedInstance] connected] == YES && [MBCSharePlayManager sharedInstance].boardWindowDelegate == self) {
        [MBCSharePlayManager sharedInstance].totalMoves++;
    }
	[board commitMove];
	[[self renderView] hideMoves];
	[[self document] updateChangeCount:NSChangeDone];
    
    if ([[self document] humanSide] == kBothSides && [[self renderView] facing] != kNeitherSide
        && ([[MBCSharePlayManager sharedInstance] boardWindowDelegate] != self || [[MBCSharePlayManager sharedInstance] connected] == NO)) {
		//
		// Rotate board
		//
        NSLog(@"MBCBoardWin CommitMove Rotate Board");
		fCurAnimation = [MBCBoardAnimation boardAnimation:[self renderView]];
	}
}

- (BOOL)listenForMoves
{
    return [[self document] boolForKey:kMBCListenForMoves];
}

- (BOOL)speakMoves
{
    return [[self document] boolForKey:kMBCSpeakMoves];
}

- (BOOL)speakHumanMoves
{
    return [[self document] boolForKey:kMBCSpeakHumanMoves];
}

- (NSString *)speakOpponentTitle
{
    if ([[self document] match]) 
        return NSLocalizedString(@"gc_opponent", @"Speak Opponent Moves");
    else
        return NSLocalizedString(@"engine_opponent", @"Speak Computer Moves");
}

- (IBAction) updatePlayers:(id)sender
{
    [self willChangeValueForKey:@"hideEngineStrength"];
    [self willChangeValueForKey:@"hideNewGameSides"];
    [self didChangeValueForKey:@"hideEngineStrength"];
    [self didChangeValueForKey:@"hideNewGameSides"];
}

- (BOOL) hideEngineStrength
{
    NSUserDefaults * userDefaults = [NSUserDefaults standardUserDefaults];
    
    switch ([userDefaults integerForKey:kMBCNewGamePlayers]) {
        case kHumanVsHuman:
        case kHumanVsGameCenter:
            return YES;
        default:
            return NO;
    }
}

- (BOOL) hideNewGameSides
{
    NSUserDefaults * userDefaults = [NSUserDefaults standardUserDefaults];
    
    switch ([userDefaults integerForKey:kMBCNewGamePlayers]) {
        case kHumanVsGameCenter:
            return NO;
        default:
            return YES;
    }
}

- (BOOL)hideSpeakMoves
{
    return [[self document] humanSide] == kBothSides;
}

- (BOOL)hideSpeakHumanMoves
{
    return [[self document] engineSide] == kBothSides;
}

- (BOOL)hideEngineProperties
{
    return [[self document] engineSide] == kNeitherSide;
}

- (BOOL)hideRemoteProperties
{
    return [[self document] remoteSide] == kNeitherSide;
}

- (NSString *)voiceIDForKey:(NSString *)key
{
    NSString * voiceID = [[self document] objectForKey:key];
    
    return [voiceID length] ? voiceID : nil;
}

- (NSSpeechSynthesizer *)copySpeechSynthesizerForKey:(NSString *)key
{
    NSString *voiceID = [self voiceIDForKey:key];
    NSSpeechSynthesizer *speechSynthesizer = [[NSSpeechSynthesizer alloc] initWithVoice:voiceID];
    
    // The following change is being done based on comments and suggestion in <rdar://problem/44522764>.
    // The old default alternative voice "Vicky" has been deprecated and the closest match to it is "victoria".
    // However, existing users who might have previously downloaded "Vicky" should still be able to use it since they have it on their machine and is set as a voice in their user defaults.
    // The logic below allows that and if the synthesizer cannot be created for "Vicky", it tries it with the voice "Victoria".
    if (!speechSynthesizer && [voiceID isEqualToString:@"com.apple.speech.synthesis.voice.Vicky"]) {
        // For existing users who do not have the voice "Vicky" already downloaded, the above call will return nil as "Vicky" has been discontinued.
        // Use the fallback voice instead.
        NSLog(@"Voice 'Vicky' could not be synthesized. Retrying using voice 'Victoria'.");
        speechSynthesizer = [[NSSpeechSynthesizer alloc] initWithVoice:@"com.apple.speech.synthesis.voice.Victoria"];
        if (!speechSynthesizer) {
            // If fallback voice also does not exist, we will return the default voice.
            NSLog(@"Voice 'Victoria' could not be synthesized. Retrying using voice the default voice.");
            speechSynthesizer = [[NSSpeechSynthesizer alloc] initWithVoice:nil];
        }
    }
    return speechSynthesizer;
}

- (NSSpeechSynthesizer *)primarySynth
{
    if (!primarySynth)
        primarySynth = [self copySpeechSynthesizerForKey:kMBCDefaultVoice];
    return primarySynth;
}

- (NSSpeechSynthesizer *)alternateSynth
{
    if (!alternateSynth)
        alternateSynth = [self copySpeechSynthesizerForKey:kMBCAlternateVoice];
    return alternateSynth;
}

- (NSDictionary *)copyLocalizationForKey:(NSString *)key
{
    NSString * voice        = [self voiceIDForKey:key];
    NSString * localeID     = [[NSSpeechSynthesizer attributesForVoice:voice]
                                valueForKey:NSVoiceLocaleIdentifier];
	if (!localeID)
		return nil;
    
    NSLocale * locale       = [[[NSLocale alloc] initWithLocaleIdentifier:localeID] autorelease];
	NSBundle * mainBundle   = [NSBundle mainBundle];
	NSArray  * preferred    = [NSBundle preferredLocalizationsFromArray:[mainBundle localizations]
														 forPreferences:[NSArray arrayWithObject:localeID]];
	if (!preferred)
		return nil;
    
	for (NSString * tryLocale in preferred)
		if (NSURL * url = [mainBundle URLForResource:@"Spoken" withExtension:@"strings"
                                        subdirectory:nil localization:tryLocale]
			)
			return [[NSDictionary alloc] initWithObjectsAndKeys:
                    [NSDictionary dictionaryWithContentsOfURL:url], @"strings",
                    locale, @"locale", nil];
	return nil;
}

- (NSDictionary *)primaryLocalization
{
    if (!primaryLocalization)
        primaryLocalization = [self copyLocalizationForKey:kMBCDefaultVoice];
    return primaryLocalization;
}

- (NSDictionary *)alternateLocalization
{
    if (!alternateLocalization)
        alternateLocalization = [self copyLocalizationForKey:kMBCAlternateVoice];
    return alternateLocalization;
}

- (NSString *)engineStrengthForTime:(int)time {
    switch (time) {
        case 1:
            return NSLocalizedString(@"fixed_depth_mode", @"Computer thinks 1 move ahead");
            break;
        case 2:
        case 3:
            return [NSString localizedStringWithFormat:NSLocalizedString(@"fixed_depths_mode", @"Computer thinks %d moves ahead"), time];
            break;
        case 4:
            return NSLocalizedString(@"fixed_time_mode", @"Computer thinks 1 second per move");
            break;
        default:
            return [NSString localizedStringWithFormat:NSLocalizedString(@"fixed_times_mode", @"Computer thinks %d seconds per move"), [MBCEngine secondsForTime:time]];
            break;
    }
}

- (int)searchTime
{
    return [[self document] integerForKey:kMBCSearchTime];
}

- (NSString *)engineStrength
{
    return [self engineStrengthForTime:[self searchTime]];
}

+ (NSSet *) keyPathsForValuesAffectingEngineStrength
{
    return [NSSet setWithObject:@"document.MBCSearchTime"];
}

- (IBAction)showPreferences:(id)sender
{
    [gameInfo editPreferencesForWindow:[self window] hidePiecesStyle:[MBCBoardWin isRenderingWithMetal]];
}

- (void)setAngle:(float)angle spin:(float)spin
{
    [[self document] setObject:[NSNumber numberWithFloat:angle] forKey:kMBCBoardAngle];
    [[self document] setObject:[NSNumber numberWithFloat:spin] forKey:kMBCBoardSpin];
}

- (IBAction) profileDraw:(id)sender
{
    timeval startTime;
    gettimeofday(&startTime, NULL);
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        timeval endTime;
        [[self renderView] profileDraw];
        gettimeofday(&endTime, NULL);
        double elapsed = endTime.tv_sec-startTime.tv_sec
        +0.000001*(endTime.tv_usec-startTime.tv_usec);
        NSLog(@"Profiling took %4.2fs, %4.0fms per frame",
              elapsed, elapsed*10.0);
    });
}

- (BOOL) hideSharePlayProperties {
    return ![MBCUserDefaults isSharePlayEnabled];
}

- (void)checkEdgeNotationVisibilityForReusedWindow {
    MBCDocument *document = [self document];
    if ([document boolForKey:kMBCShowEdgeNotation] != gameMTLView.drawEdgeNotationLabels) {
        // Stored value from document not equal to what is shown in view, update the view.
        gameMTLView.drawEdgeNotationLabels = [document boolForKey:kMBCShowEdgeNotation];
    }
}

- (IBAction)toggleEdgeNotation:(id)sender {
    MBCDocument * document = [self document];
    BOOL currentlyShowing = [document boolForKey:kMBCShowEdgeNotation];
    [document setValue:[NSNumber numberWithBool:!currentlyShowing] forKey:kMBCShowEdgeNotation];
    
    // Enable or disable rendering of the edge notation labels
    gameMTLView.drawEdgeNotationLabels = !currentlyShowing;
    
    // Need to redraw to update labels.
    [gameMTLView needsUpdate];
}

#pragma mark -
#pragma mark GKTurnBasedMatchmakerViewControllerDelegate
// The user has cancelled
- (void)turnBasedMatchmakerViewControllerWasCancelled:(GKTurnBasedMatchmakerViewController *)vc 
{
	[dialogController dismiss:vc];
    [NSApp stopModal];
    if ([[self document] invitees])
        [self close];
    else
        [self showNewGameSheet];
}

// Matchmaking has failed with an error
- (void)turnBasedMatchmakerViewController:(GKTurnBasedMatchmakerViewController *)vc didFailWithError:(NSError *)error 
{
    [self turnBasedMatchmakerViewControllerWasCancelled:vc];
}

// A turned-based match has been found, the game should start
- (void)turnBasedMatchmakerViewController:(GKTurnBasedMatchmakerViewController *)vc didFindMatch:(GKTurnBasedMatch *)match {
	[dialogController dismiss:vc];
    [NSApp stopModal];
    
    MBCController *appDelegate = (MBCController *)[[NSApplication sharedApplication] delegate];
    [appDelegate startNewOnlineGame:match withDocument:[self document]];
}

// Called when a users chooses to quit a match and that player has the current turn.  The developer should call playerQuitInTurnWithOutcome:nextPlayer:matchData:completionHandler: on the match passing in appropriate values.  They can also update matchOutcome for other players as appropriate.
- (void)turnBasedMatchmakerViewController:(GKTurnBasedMatchmakerViewController *)vc playerQuitForMatch:(GKTurnBasedMatch *)match {
    for (GKTurnBasedParticipant * participant in[match participants])
        if ([participant.player.playerID isEqual:[[(MBCController *)[NSApp delegate] localPlayer] playerID]])
            [participant setMatchOutcome:GKTurnBasedMatchOutcomeQuit];
        else
            [participant setMatchOutcome:GKTurnBasedMatchOutcomeWon];
    
    [match endMatchInTurnWithMatchData:[NSData data] completionHandler:^(NSError *error) {        
    }];
}

#pragma mark -
#pragma mark GKAchievementViewControllerDelegate

- (IBAction)showAchievements:(id)sender
{
    // Create an achievementVC, set the delegate and present the view from GKDiaglogController
    GKGameCenterViewController *vc = [[[GKGameCenterViewController alloc] init] autorelease];
    vc.viewState = GKGameCenterViewControllerStateAchievements;
    vc.gameCenterDelegate = self;
    
    [[GKDialogController sharedDialogController] presentViewController:vc];
}

- (void)gameCenterViewControllerDidFinish:(GKGameCenterViewController *)gameCenterViewController
{
    if (gameCenterViewController) {
        [[GKDialogController sharedDialogController] dismiss:gameCenterViewController];
    }
}

#pragma mark - MBCSharePlayManagerBoardWindowDelegate
- (void)connectedToSharePlaySessionWithNumParticipants:(NSInteger)numParticipants {
    NSLog(@"Number of Participants Currently in Session %d", (int)numParticipants);
    dispatch_async(dispatch_get_main_queue(), ^{
        [[self document] setValue:@"SharePlay" forKey:@"White"];
        [[self document] setValue:@"Session" forKey:@"Black"];
    });
    [[MBCSharePlayManager sharedInstance] setConnected:YES];
}

- (void)receivedStartSelectionMessageWithMessage:(StartSelectionMessage *)message {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSLog(@"In dispatch startSelectionWithoutShare");
        MBCSquare square = message.square;
        [self.interactive startSelectionWithoutShare:square];
    });
    
}

- (void)receivedEndSelectionMessageWithMessage:(EndSelectionMessage *)message {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSLog(@"In dispatch startSelectionWithoutShare");
        MBCSquare square = message.square;
        BOOL animate = message.animate;
        [self.interactive endSelectionWithoutShare:square animate:animate];
    });
}

- (void)sendNotificationForGameEnded {
    UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
    [center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert + UNAuthorizationOptionSound)
       completionHandler:^(BOOL granted, NSError * _Nullable error) {
          // Enable or disable features based on authorization.
    }];
    UNMutableNotificationContent* content = [[UNMutableNotificationContent alloc] init];
    content.title = [NSString localizedUserNotificationStringForKey:@"Game Over" arguments:nil];
    content.body = [NSString localizedUserNotificationStringForKey:@"A Player Has Left the Session, Ending the Chess Game Now"
            arguments:nil];
    // Create the request object.
    UNNotificationRequest* request = [UNNotificationRequest
           requestWithIdentifier:@"ChessGameEnds" content:content trigger:nil];
    [center addNotificationRequest:request withCompletionHandler:^(NSError * _Nullable error) {
       if (error != nil) {
           NSLog(@"%@", error.localizedDescription);
       }
    }];
}

- (SharePlayBoardStateMessage *)createBoardStateMessage {
    SharePlayBoardStateMessage *boardState = [[SharePlayBoardStateMessage alloc] initWithFen:[board fen] holding:[board holding] moves:[board moves] numMoves:[[MBCSharePlayManager sharedInstance] totalMoves]];
    NSLog(@"MBCBoardWin: sendBoardStateMesage fen:%@ moves:%@ holding:%@", boardState.fen, boardState.moves, boardState.holding);
    return boardState;
}

- (void)receivedBoardStateMessageWithFen:(NSString *)fen moves:(NSString *)moves holding:(NSString *)holding {
    NSLog(@"MBCBoardWin: receivedBoardStateMessageWithMessage fen:%@ moves:%@ holding:%@", fen, moves, holding);
    if (currentSharePlayMoveStringCount < moves.length) {
        currentSharePlayMoveStringCount = moves.length;
        [[MBCSharePlayManager sharedInstance] setTotalMoves:(int)[moves componentsSeparatedByString:@"\n"].count - 1];
        NSLog(@"MBCBoardWin: %d, movecount", (int)[moves componentsSeparatedByString:@"\n"].count);
        dispatch_async(dispatch_get_main_queue(), ^{
            [engine setGame:[[self document] variant] fen:fen holding:holding moves:moves];
            [board setFen:fen holding:holding moves:moves];
            [self.interactive setLastSide:([moves componentsSeparatedByString:@"\n"].count) & 1 ? kWhiteSide : kBlackSide];
        });
    } else {
        NSLog(@"MBCBoardWin: current board is more ahead, skipping update");
    }
}

- (void) sessionDidEnd {
    dispatch_async(dispatch_get_main_queue(), ^{
        [[self document] close];
        [[self window] performClose:nil];
    });
}

- (void)receivedTakeBackMessage {
    NSLog(@"MBCBoardWin: ReceivedTakeBackMessage");
    dispatch_async(dispatch_get_main_queue(), ^{
        if ([[self document] match]) {
            [gameInfo willChangeValueForKey:@"gameTitle"];
            [[self document] offerTakeback];
            [gameInfo didChangeValueForKey:@"gameTitle"];
        } else {
            [engine takeback];
        }
    });
}

#pragma mark - UNNotificationCenterDelegate
- (void)userNotificationCenter:(UNUserNotificationCenter *)center
        willPresentNotification:(UNNotification *)notification
        withCompletionHandler:(void (^)(UNNotificationPresentationOptions options))completionHandler {
   // Update the app interface directly.
    NSLog(@"UNNotificationCenterDelegate callback");
    completionHandler(UNNotificationPresentationOptionBanner);
}

#pragma mark - Screen Recording

- (void)stopRecordingIfNeeded {
    MBCController *controller = (MBCController *)[NSApp delegate];
    [controller stopRecordingForWindow:self.window];
}

#pragma mark - Metal Rendering

/*!
 @abstract isRunningTranslatedWithRosetta
 @discussion This method returns the value 0 for a native process, 1 for a translated process, and -1 when an error occurs.
 */
+ (int)isRunningTranslatedWithRosetta {
    int ret = 0;
    size_t size = sizeof(ret);
    if (sysctlbyname("sysctl.proc_translated", &ret, &size, NULL, 0) == -1) {
        if (errno == ENOENT) {
            return 0;
        }
        return -1;
    }
    return ret;
}

/*!
 @abstract isRenderingWithMetal
 @discussion Will determine whether or not Metal is enabled, running on Apple Silicon device, and running native (not translated with Rosetta).
 @return Yes if Metal render is enabled, running on Apple Silicon device, and running native.
 */
+ (BOOL)isRenderingWithMetal {
    static dispatch_once_t onceToken;
    static BOOL sRenderingWithMetal;
    dispatch_once(&onceToken, ^{
        BOOL isAppleSiliconDevice = [MBCMetalRenderer.defaultMTLDevice supportsFamily:MTLGPUFamilyApple2];
        BOOL isNativeProcess = ([self isRunningTranslatedWithRosetta] == 0);
        sRenderingWithMetal = [MBCUserDefaults isMetalRenderingEnabled] && isAppleSiliconDevice && isNativeProcess;
    });
    return sRenderingWithMetal;
}

- (void)initializeForMetalRendering {
    // Black bars to be displayed above/below Metal view when game view
    // does not fill window height while maintaining view aspect ratio.
    self.mtlBackingView.layer.backgroundColor = [NSColor.blackColor CGColor];
    
    id<MTLDevice> device = MBCMetalRenderer.defaultMTLDevice;
    gameMTLView.device = device;
    
    fMetalRenderer = [[MBCMetalRenderer alloc] initWithDevice:device mtkView:gameMTLView];
    [fMetalRenderer drawableSizeWillChange:gameMTLView.preferredDrawableSize];
    gameMTLView.renderer = fMetalRenderer;
    
    MBCDocument *document = [self document];
    gameMTLView.elevation = [document floatForKey:kMBCBoardAngle];
    gameMTLView.azimuth = [document floatForKey:kMBCBoardSpin];
    gameMTLView.drawEdgeNotationLabels = [document boolForKey:kMBCShowEdgeNotation];
    gameMTLView.aspectConstraint.constant = MAIN_WINDOW_CONTENT_ASPECT_RATIO;
    [gameMTLView setStyleForBoard:[document objectForKey:kMBCBoardStyle]
                           pieces:[document objectForKey:kMBCPieceStyle]];
    
    gameMTLView.delegate = self;
    
    // By default the View menu item is hidden.
    NSMenuItem *viewMenu = [[NSApp mainMenu] itemAtIndex:3];
    viewMenu.hidden = NO;
    
    // Remove the default "Show Tab Bar" and "Show All Tabs" menu items from View menu.
    NSWindow.allowsAutomaticWindowTabbing = NO;
}

- (void)windowDidEndLiveResize:(NSNotification *)notification {
    // Stopped live resize, need to update the Metal renderer's textures
    if (gameMTLView) {
        [self mtkView:gameMTLView drawableSizeWillChange:gameMTLView.drawableSize];
    }
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    if (self.window.inLiveResize) {
        // Do not call drawableSizeWillChange on renderer until live resize is completed by user.
        // Will do a lot of unneccessary MTLTexture generation during all the calls made while
        // resizing the window
        return;
    }
    [fMetalRenderer drawableSizeWillChange:size];
}

- (void)drawInMTKView:(MTKView *)view {
    if (self.window.inLiveResize) {
        // Do not redraw metal content until live resize is completed by user. This leads
        // to a lot of memory usage which may result in app using too much memory.
        return;
    }
    [gameMTLView drawMetalContent];
}

@end
