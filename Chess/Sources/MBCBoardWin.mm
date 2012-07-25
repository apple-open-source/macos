/*
    File:		MBCBoardWin.mm
    Contains:	Manage the board window
    Copyright:	© 2002-2012 by Apple Inc., all rights reserved.

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

@implementation MBCBoardWin

@synthesize gameView, gameNewSheet, logContainer, logView, board, engine, interactive;
@synthesize gameInfo, remote, logViewRightEdgeConstraint, dialogController;
@synthesize primarySynth, alternateSynth, primaryLocalization, alternateLocalization;

- (void)removeChessObservers
{
    if (!fHasObservers)
        return;
    
    NSNotificationCenter * notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter removeObserver:self name:MBCGameLoadNotification object:nil];
    [notificationCenter removeObserver:self name:MBCGameStartNotification object:nil];
    [notificationCenter removeObserver:self name:MBCTakebackNotification object:nil];
    [notificationCenter removeObserver:self name:MBCWhiteMoveNotification object:nil];
    [notificationCenter removeObserver:self name:MBCBlackMoveNotification object:nil];
    [notificationCenter removeObserver:self name:MBCGameEndNotification object:nil];
    [notificationCenter removeObserver:self name:MBCEndMoveNotification object:nil];
    MBCDocument *   document    = [self document];
    [document removeObserver:self forKeyPath:kMBCDefaultVoice];
    [document removeObserver:self forKeyPath:kMBCAlternateVoice];
    [document removeObserver:self forKeyPath:kMBCBoardStyle];
    [document removeObserver:self forKeyPath:kMBCPieceStyle];
    
    fHasObservers = NO;
}

- (void)dealloc
{
    [self removeChessObservers];
    [super dealloc];
}

- (void)windowDidLoad
{
    [super windowDidLoad];

    MBCDocument *   document    = [self document];
    [document setBoard:board];
    [engine setDocument:document];
    [interactive setDocument:document];
    [gameInfo setDocument:document];
    [remote setDocument:document];

    [self removeChessObservers];
    NSNotificationCenter * notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter
     addObserverForName:MBCGameLoadNotification object:document 
     queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
         NSDictionary * dict    = [note userInfo];
         NSString *     fen     = [dict objectForKey:@"Position"];
         NSString *     holding = [dict objectForKey:@"Holding"];
         NSString *     moves   = [dict objectForKey:@"Moves"];
 
         if (fen || moves)
             [engine setGame:[document variant] fen:fen holding:holding moves:moves];
    }];    
    [notificationCenter
     addObserverForName:MBCGameStartNotification object:document 
     queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
         MBCVariant     variant  = [document variant];
         
         [gameView startGame:variant playing:[document humanSide]];
         [engine setSearchTime:[document integerForKey:kMBCSearchTime]];
         [engine startGame:variant playing:[document engineSide]];
         [interactive startGame:variant playing:[document humanSide]];	
         [gameInfo startGame:variant playing:[document humanSide]];
         if (document.match)
             [remote startGame:variant playing:[document remoteSide]];
     }];  
    [notificationCenter
     addObserverForName:MBCTakebackNotification object:document 
     queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
         [gameView unselectPiece];
         [gameView hideMoves];
         [board undoMoves:2];
     }];  
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
    fHasObservers = YES;
    
	gameView->fElevation            = [document floatForKey:kMBCBoardAngle];
	gameView->fAzimuth              = [document floatForKey:kMBCBoardSpin];
    
    [gameView setStyleForBoard:[document objectForKey:kMBCBoardStyle] pieces:[document objectForKey:kMBCPieceStyle]];
    
    [self setShouldCascadeWindows:NO];
    NSWindow * window = [self window];
    if ([[self document] match])
        [window setFrameAutosaveName:[NSString stringWithFormat:@"Match %@\n", document.match.matchID]];
    if (![document boolForKey:kMBCShowGameLog])
        [self hideLogContainer:self];
    [window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
	[window makeFirstResponder:gameView];
	[window makeKeyAndOrderFront:self];
    if ([document needNewGameSheet]) {
        usleep(500000);
        [self showNewGameSheet];
    }
}

- (void)close
{
    if (![logView superview])
        [logView release];
    [super close];
}

- (void)windowDidBecomeMain:(NSNotification *)notification
{
    if ([self listenForMoves])
        [interactive allowedToListen:YES];
	[gameView setNeedsDisplay:YES];
}

- (void)windowDidResignMain:(NSNotification *)notification
{
    if ([self listenForMoves])
        [interactive allowedToListen:NO];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if ([keyPath isEqual:kMBCDefaultVoice]) {
        [primarySynth release];             primarySynth            = nil;
        [primaryLocalization release];      primaryLocalization     = nil;
    } else if ([keyPath isEqual:kMBCAlternateVoice]) {
        [alternateSynth release];           alternateSynth          = nil;
        [alternateLocalization release];    alternateLocalization   = nil;
    } else {
        [gameView setStyleForBoard:[[self document] objectForKey:kMBCBoardStyle] 
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
        return YES;
    } else
        return YES;
}

- (NSString *)windowTitleForDocumentDisplayName:(NSString *)displayName
{
    return [gameInfo gameTitle];
}

- (IBAction)takeback:(id)sender
{
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
        MBCController * controller = [NSApp delegate];
        if (returnCode == NSAlertDefaultReturn) {
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
            MBCController * controller = [NSApp delegate];
            if (returnCode == NSAlertDefaultReturn) {
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
    if ([[self document] invitees]) 
        [self runMatchmakerPanel];
    else 
        [NSApp beginSheet:gameNewSheet modalForWindow:[self window] 
            modalDelegate:nil didEndSelector:nil contextInfo:nil];
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
	matchRequest.playersToInvite = nil;
	
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
	[gameView showMoveAsHint:[engine lastPonder]];
	[interactive announceHint:[engine lastPonder]];
}

- (IBAction) showLastMove:(id)sender
{
	[gameView showMoveAsLast:[board lastMove]];
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

- (void) adjustLogView
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
	MBCMove *    move 	= reinterpret_cast<MBCMove *>([notification userInfo]);
    
	[board makeMove:move];
    
    BOOL weWon = NO;
    if (move->fCommand == kCmdWhiteWins && SideIncludesWhite([[self document] humanSide]))
        weWon = YES;
    if (move->fCommand == kCmdBlackWins && SideIncludesBlack([[self document] humanSide]))
        weWon = YES;
    if (weWon) {
        MBCController * controller      = [NSApp delegate];
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
                    if (![p.playerID isEqual:[controller localPlayer].playerID])
                        [v setObject:[NSNumber numberWithBool:YES] forKey:p.playerID];
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
    MBCController * controller      = [NSApp delegate];
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
        if (move->fPromotion)
            if (Piece(move->fPromotion) == QUEEN)
                [controller setValue:100.0 forAchievement:@"AppleChess_Promotional_Value"];
            else
                [controller setValue:100.0 forAchievement:@"AppleChess_Promotional_Discount"];
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
	MBCMove *    move 	= reinterpret_cast<MBCMove *>([notification userInfo]);

	[board makeMove:move];
	[gameView unselectPiece];
	[gameView hideMoves];
	[[self document] updateChangeCount:NSChangeDone];
    [self updateAchievementsForMove:move];
    
	if (move->fAnimate)
		[MBCMoveAnimation moveAnimation:move board:board view:gameView];
	else 
		[[NSNotificationQueue defaultQueue] 
         enqueueNotification:
         [NSNotification 
          notificationWithName:MBCEndMoveNotification
          object:[self document] userInfo:(id)move]
         postingStyle: NSPostWhenIdle];
	
    if ([[self document] engineSide] == kNeitherSide)
        if (MBCMoveCode cmd = [[self board] outcome])
            [[NSNotificationQueue defaultQueue] 
             enqueueNotification:
             [NSNotification 
              notificationWithName:MBCGameEndNotification
              object:[self document] 
              userInfo:[MBCMove moveWithCommand:cmd]]
             postingStyle: NSPostWhenIdle];
}

- (void) commitMove:(NSNotification *)notification
{
	[board commitMove];
	[gameView hideMoves];
	[[self document] updateChangeCount:NSChangeDone];
    
    if ([[self document] humanSide] == kBothSides
        && [gameView facing] != kNeitherSide
    ) {
		//
		// Rotate board
		//
		[MBCBoardAnimation boardAnimation:gameView];
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
    return [[NSSpeechSynthesizer alloc] initWithVoice:[self voiceIDForKey:key]];
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

- (NSString *)engineStrengthForTime:(int)time
{
    switch (time) {
        case -3:
            return NSLocalizedString(@"fixed_depth_mode", @"Computer thinks 1 move ahead");
        case -2:
        case -1:
            return [NSString localizedStringWithFormat:NSLocalizedString(@"fixed_depths_mode", @"Computer thinks %d moves ahead"), 4+time];
        case 0:
            return NSLocalizedString(@"fixed_time_mode", @"Computer thinks 1 second per move");
        default:
            return [NSString localizedStringWithFormat:NSLocalizedString(@"fixed_times_mode", @"Computer thinks %d seconds per move"), [MBCEngine secondsForTime:time]];
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
    [gameInfo editPreferencesForWindow:[self window]];
}

- (void)setAngle:(float)angle spin:(float)spin
{
    [[self document] setObject:[NSNumber numberWithFloat:angle] forKey:kMBCBoardAngle];
    [[self document] setObject:[NSNumber numberWithFloat:spin] forKey:kMBCBoardSpin];
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
	[[NSApp delegate] startNewOnlineGame:match withDocument:[self document]];
}

// Called when a users chooses to quit a match and that player has the current turn.  The developer should call playerQuitInTurnWithOutcome:nextPlayer:matchData:completionHandler: on the match passing in appropriate values.  They can also update matchOutcome for other players as appropriate.
- (void)turnBasedMatchmakerViewController:(GKTurnBasedMatchmakerViewController *)vc playerQuitForMatch:(GKTurnBasedMatch *)match {
    for (GKTurnBasedParticipant * participant in[match participants])
        if ([[participant playerID] isEqual:[[[NSApp delegate] localPlayer] playerID]])
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
    GKAchievementViewController * achievements = [[GKAchievementViewController alloc] init];
    achievements.achievementDelegate = self;
    [dialogController presentViewController:achievements];
}

- (void)achievementViewControllerDidFinish:(GKAchievementViewController *)vc
{
    [dialogController dismiss:vc];
}


@end
