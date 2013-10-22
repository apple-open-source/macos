/*
	File:		MBCDocument.h
	Contains:	Document representing a Chess game
	Copyright:	© 2003-2012 by Apple Inc., all rights reserved.

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

#import "MBCBoard.h"

@interface MBCDocument : NSDocument
{
    MBCBoard *              board;
    MBCVariant              variant;
    MBCPlayers              players;
    NSMutableDictionary *   properties;
    BOOL                    localWhite;
    BOOL                    disallowSubstitutes;
}

@property (nonatomic,assign)    MBCBoard *              board;
@property (nonatomic,readonly)  MBCVariant              variant;
@property (nonatomic,readonly)  MBCPlayers              players;
@property (nonatomic,retain)    GKTurnBasedMatch *      match;
@property (nonatomic,assign,readonly)  NSMutableDictionary *   properties;
@property (nonatomic)           BOOL                    offerDraw;
@property (nonatomic)           BOOL                    ephemeral;
@property (nonatomic)           BOOL                    needNewGameSheet;
@property (nonatomic)           BOOL                    disallowSubstitutes;
@property (nonatomic,retain)    NSArray *               invitees;

+ (BOOL) processNewMatch:(GKTurnBasedMatch *)match variant:(MBCVariant)variant side:(MBCSideCode)side
                document:(MBCDocument *)doc;
- (id) initWithMatch:(GKTurnBasedMatch *)match game:(NSDictionary *)gameData;
- (id) initForNewGameSheet:(NSArray *)invitees;
- (BOOL) canTakeback;
- (BOOL) boolForKey:(NSString *)key;
- (NSInteger) integerForKey:(NSString *)key;
- (float) floatForKey:(NSString *)key;
- (id) objectForKey:(NSString *)key;
- (void) setObject:(id)value forKey:(NSString *)key;
- (void) updateMatchForLocalMove;
- (void) updateMatchForRemoteMove;
- (void) updateMatchForEndOfGame:(MBCMoveCode)cmd;
- (MBCSide) humanSide;
- (MBCSide) engineSide;
- (MBCSide) remoteSide;
- (void) offerTakeback;
- (void) allowTakeback:(BOOL)allow;
- (void) resign;
- (void) updateSearchTime;
- (BOOL) nontrivialHumanTurn;
- (BOOL) gameDone;
- (BOOL) brandNewGame;
- (NSString *)nonLocalPlayerID;

@end

void MBCAbort(NSString * message, MBCDocument * doc);

// Local Variables:
// mode:ObjC
// End:
