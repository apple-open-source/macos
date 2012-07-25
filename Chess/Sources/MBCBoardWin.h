/*
    File:		MBCBoardWin.h
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

#import <Cocoa/Cocoa.h>
#import <GameKit/GameKit.h>

@class MBCBoard;
@class MBCBoardView;
@class MBCEngine;
@class MBCInteractivePlayer;
@class MBCGameInfo;
@class MBCRemotePlayer;

@interface MBCBoardWin : NSWindowController <NSWindowDelegate,
    GKAchievementViewControllerDelegate,
    GKTurnBasedMatchmakerViewControllerDelegate> 
{
    BOOL    fHasObservers;
}

@property (nonatomic, assign) IBOutlet MBCBoardView *           gameView;
@property (nonatomic, assign) IBOutlet NSPanel *                gameNewSheet;
@property (nonatomic, assign) IBOutlet NSBox *                  logContainer;
@property (nonatomic, assign) IBOutlet NSView *                 logView;
@property (nonatomic, assign) IBOutlet MBCBoard *               board;
@property (nonatomic, assign) IBOutlet MBCEngine *              engine;
@property (nonatomic, assign) IBOutlet MBCInteractivePlayer *   interactive;
@property (nonatomic, assign) IBOutlet MBCGameInfo *            gameInfo;
@property (nonatomic, assign) IBOutlet MBCRemotePlayer *        remote;
@property (nonatomic, assign) IBOutlet NSLayoutConstraint *     logViewRightEdgeConstraint;
@property (nonatomic, assign) IBOutlet GKDialogController *     dialogController;
@property (nonatomic, readonly) NSSpeechSynthesizer *           primarySynth;
@property (nonatomic, readonly) NSSpeechSynthesizer *           alternateSynth;
@property (nonatomic, readonly) NSDictionary *                  primaryLocalization;
@property (nonatomic, readonly) NSDictionary *                  alternateLocalization;

- (IBAction)takeback:(id)sender;
- (void) requestTakeback;
- (void) requestDraw;
- (IBAction)resign:(id)sender;
- (IBAction)showHint:(id)sender;
- (IBAction)showLastMove:(id)sender;
- (IBAction)toggleLogView:(id)sender;
- (IBAction) startNewGame:(id)sender;
- (IBAction) cancelNewGame:(id)sender;
- (IBAction) showAchievements:(id)sender;
- (void)adjustLogView;
- (BOOL)listenForMoves;
- (NSString *)speakOpponentTitle;
- (BOOL)speakMoves;
- (BOOL)speakHumanMoves;
- (IBAction) updatePlayers:(id)sender;
- (BOOL)hideEngineStrength;
- (BOOL)hideNewGameSides;
- (BOOL)hideSpeakMoves;
- (BOOL)hideSpeakHumanMoves;
- (BOOL)hideEngineProperties;
- (BOOL)hideRemoteProperties;
- (NSString *)engineStrength;
+ (NSSet *) keyPathsForValuesAffectingEngineStrength;
- (IBAction) showPreferences:(id)sender;
- (void)setAngle:(float)angle spin:(float)spin;
- (void)handleRemoteResponse:(NSString *)response;

@end
