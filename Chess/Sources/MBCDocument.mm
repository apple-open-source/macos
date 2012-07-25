/*
	File:		MBCDocument.mm
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

#import "MBCDocument.h"
#import "MBCController.h"
#import "MBCPlayer.h"
#import "MBCEngine.h"
#import "MBCGameInfo.h"
#import "MBCUserDefaults.h"
#import "MBCBoardWin.h"

NSString * const sProperties[] = {
    kMBCBoardStyle, kMBCPieceStyle,
    kMBCDefaultVoice, kMBCAlternateVoice,
    kMBCSpeakMoves, kMBCSpeakHumanMoves, kMBCListenForMoves,
    kMBCSearchTime, kMBCBoardAngle, kMBCBoardSpin, kMBCShowGameLog,
    NULL
};


static void MBCEndTurn(GKTurnBasedMatch *match, NSData * matchData)
{
    GKTurnBasedParticipant * opponent;
    
    for (opponent in [match participants])
        if (opponent != [match currentParticipant])
            break;
    
    [match endTurnWithNextParticipant:opponent matchData:matchData 
                    completionHandler:^(NSError *error) {
                    }];
}

@implementation MBCDocument

@synthesize board, variant, players, match, properties, offerDraw, ephemeral, needNewGameSheet,
    disallowSubstitutes, invitees;

+ (BOOL) autosavesInPlace
{
    return YES;
}

- (BOOL) boolForKey:(NSString *)key;
{
    return [[properties valueForKey:key] boolValue];
}

- (NSInteger) integerForKey:(NSString *)key
{
    return [[properties valueForKey:key] intValue];
}

- (float) floatForKey:(NSString *)key
{
    return [[properties valueForKey:key] floatValue];    
}

- (id) objectForKey:(NSString *)key
{
    id result = [properties valueForKey:key];
    if (!result && ([key isEqual:@"White"] || [key isEqual:@"Black"]))
        result = NSLocalizedString(@"gc_automatch", @"GameCenter Automatch");
    return result;
}

- (BOOL) brandNewGame
{
    return ![properties valueForKey:@"White"] && ![properties valueForKey:@"Black"];
}

- (id) valueForUndefinedKey:(NSString *)key
{
    return [self objectForKey:key];
}

- (void) setObject:(id)value forKey:(NSString *)key
{
    [self setValue:value forUndefinedKey:key];
}

- (void) setValue:(id)value forUndefinedKey:(NSString *)key
{
    if (value == [NSNull null])
        value = nil;
    [self willChangeValueForKey:key];
    [[NSUserDefaults standardUserDefaults] setObject:value forKey:key];
    [properties setValue:value forKey:key];
    [self didChangeValueForKey:key];
    [self updateChangeCount:NSChangeDone];
}

- (void)updateSearchTime
{
    id curSearchTime = [properties valueForKey:kMBCSearchTime];
    id minSearchTime = [properties valueForKey:kMBCMinSearchTime];
    
    if (minSearchTime && [minSearchTime intValue] > [curSearchTime intValue])
        [properties setValue:curSearchTime forKey:kMBCMinSearchTime];
    [[[[self windowControllers] objectAtIndex:0] engine] setSearchTime:[curSearchTime intValue]];
}

+ (BOOL)coinFlip
{
    static BOOL sLastFlip = arc4random() & 1;
    
    return sLastFlip = !sLastFlip;
}

+ (BOOL)processNewMatch:(GKTurnBasedMatch *)match variant:(MBCVariant)variant side:(MBCSideCode)side
    document:(MBCDocument *)doc
{
    NSPropertyListFormat    format;
    NSDictionary *          gameData        = !match.matchData ? nil :
        [NSPropertyListSerialization propertyListWithData:match.matchData options:0 format:&format error:nil];
    MBCController *         controller      = [NSApp delegate];
    NSString *              localPlayerID   = controller.localPlayer.playerID;
    if (!gameData) { 
        //
        // Brand new game, pick a side
        //
        NSString *       playerIDKey;
        switch (side) {
        case kPlayWhite:
            playerIDKey = @"WhitePlayerID";
            break;
        case kPlayBlack:
            playerIDKey = @"BlackPlayerID";
            break;
        default:
            playerIDKey = [self coinFlip] ? @"WhitePlayerID" : @"BlackPlayerID";
            break;
        }
        gameData = [NSDictionary dictionaryWithObjectsAndKeys:
                    localPlayerID, playerIDKey,
                    gVariantName[variant], @"Variant",
                    kMBCHumanPlayer, @"WhiteType", kMBCHumanPlayer, @"BlackType",
                    nil];
        if ([playerIDKey isEqual:@"BlackPlayerID"]) {
            //
            // We picked black, it's our opponent's turn
            //
            MBCEndTurn(match, [NSPropertyListSerialization 
                               dataFromPropertyList:gameData
                                            format:NSPropertyListXMLFormat_v1_0
                                    errorDescription:nil]);
        } else {
            //
            // Push our choices to the server
            //
            [match endTurnWithNextParticipant:match.currentParticipant
                                    matchData:[NSPropertyListSerialization 
                                               dataFromPropertyList:gameData
                                               format:NSPropertyListXMLFormat_v1_0
                                               errorDescription:nil]
                            completionHandler:^(NSError *error) {}];
        }
    } else if (![localPlayerID isEqual:[gameData objectForKey:@"WhitePlayerID"]]
           &&  ![localPlayerID isEqual:[gameData objectForKey:@"BlackPlayerID"]]
    ) {
        //
        // We're the second participant, pick the side that hasn't been picked yet
        //
        BOOL       playingWhite = ![gameData objectForKey:@"WhitePlayerID"];
        NSMutableDictionary * newGameData = [gameData mutableCopy];
        [newGameData removeObjectForKey:@"PeerID"]; // Legacy key
        [newGameData setObject:localPlayerID forKey:(playingWhite ? @"WhitePlayerID" : @"BlackPlayerID")];
        gameData = newGameData;
    } 
    BOOL createDoc = !doc;
    if (createDoc)
        doc = [[MBCDocument alloc] init];
    [doc initWithMatch:match game:gameData];
    if (createDoc) {
        [doc makeWindowControllers];
        [doc showWindows]; 
    }
    
    return YES;
}

- (void)dealloc
{
    [properties release];
    [super dealloc];
}

- (id)init
{
    //
    // If we have any implicitly opened document that did not get used yet, we reuse one
    //
    if (![self disallowSubstitutes])
        for (MBCDocument * doc in [[NSDocumentController sharedDocumentController] documents])
            if ([doc ephemeral]) {
                [self release];
                self = doc;
                [self setNeedNewGameSheet:NO];
                
                return self;
            }
    if (self = [super init]) {
        [self setEphemeral:YES];
        NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
        properties  = [[NSMutableDictionary alloc] init];
        for (int i = 0; sProperties[i]; ++i)
            [properties setValue:[defaults objectForKey:sProperties[i]]
                          forKey:sProperties[i]];
    }
    return self;
}

- (id)initForNewGameSheet:(NSArray *)guests
{
    if (self = [self init]) {
        NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
        if (![[NSApp delegate] localPlayer] && (MBCPlayers)[defaults integerForKey:kMBCNewGamePlayers] == kHumanVsGameCenter)
            [defaults setInteger:kHumanVsComputer forKey:kMBCNewGamePlayers];
        [self setEphemeral:NO];
        [self setNeedNewGameSheet:YES];
        [self setDisallowSubstitutes:YES];
        [self setInvitees:guests];
        [[NSDocumentController sharedDocumentController] addDocument:self];
    }
    
    return self;
}

- (id)initWithType:(NSString *)typeName error:(NSError **)outError
{
    if (self = [super initWithType:typeName error:outError]) {
        NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
        players     = (MBCPlayers)[defaults integerForKey:kMBCNewGamePlayers];
        switch (players) {
        case kHumanVsHuman:
        case kHumanVsComputer:
            //
            // Always accept these
            //
            break;
        case kComputerVsHuman:
        case kComputerVsComputer:
            //
            // Accept it for explicit creation, but reset default
            //
            [defaults setInteger:kHumanVsComputer forKey:kMBCNewGamePlayers];
            break;
        default:
            //
            // Never create a GameCenter game for an "untitled" action
            //
            players = kHumanVsComputer;
            break;
        }
        variant     = (MBCVariant)[defaults integerForKey:kMBCNewGameVariant];
        if (players == kHumanVsComputer || players == kComputerVsHuman)
            [properties setValue:[properties valueForKey:kMBCSearchTime] forKey:kMBCMinSearchTime];
        [[NSNotificationQueue defaultQueue]
         enqueueNotification:[NSNotification notificationWithName:MBCGameStartNotification
                                                           object:self]
                postingStyle:NSPostWhenIdle];
        [self updateChangeCount:NSChangeCleared];
    }
    return self;
}

- (void)updateChangeCount:(NSDocumentChangeType)change
{
    if ([board numMoves] || [self remoteSide] != kNeitherSide) {
        [self setEphemeral:NO];
    
        [super updateChangeCount:change];
    }
}

- (void)canCloseDocumentWithDelegate:(id)delegate shouldCloseSelector:(SEL)sel contextInfo:(void *)contextInfo
{
    //
    // Never ask to save GameCenter games (we autosave them, though)
    //
    if (match) {
        NSInvocation * invoc = 
            [NSInvocation invocationWithMethodSignature:[delegate methodSignatureForSelector:sel]];
        [invoc setTarget:delegate];
        [invoc setSelector:sel];
        [invoc setArgument:&self atIndex:2];
        BOOL shouldClose = YES;
        [invoc setArgument:&shouldClose atIndex:3];
        [invoc setArgument:&contextInfo atIndex:4];
        [invoc invoke];
    } else {
        [super canCloseDocumentWithDelegate:delegate shouldCloseSelector:sel contextInfo:contextInfo];
    }
}

- (void)postMatchOutcomeNotification
{
    MBCMoveCode cmd;
    NSString *  localPlayerID = [[NSApp delegate] localPlayer].playerID;
    for (GKTurnBasedParticipant * p in match.participants)
        if ([p.playerID isEqual:localPlayerID]) {
            if (p.matchOutcome == GKTurnBasedMatchOutcomeTied)
                cmd = kCmdDraw;
            else if (p.matchOutcome == GKTurnBasedMatchOutcomeWon)
                cmd = localWhite ? kCmdWhiteWins : kCmdBlackWins;
            else
                cmd = localWhite ? kCmdBlackWins : kCmdWhiteWins;
            break;
        }
    [[NSNotificationQueue defaultQueue]
     enqueueNotification:[NSNotification notificationWithName:MBCGameEndNotification
                                                       object:self
                                                     userInfo:(id)[MBCMove moveWithCommand:cmd]]
     postingStyle:NSPostWhenIdle];
}

- (BOOL)checkForEndOfGame
{
    bool firstIsDone = [[match.participants objectAtIndex:0] matchOutcome] != GKTurnBasedMatchOutcomeNone;
    bool secondIsDone = [[match.participants objectAtIndex:1] matchOutcome] != GKTurnBasedMatchOutcomeNone;
    
    if (firstIsDone != secondIsDone) {
        GKTurnBasedMatchOutcome outcome;
        if ([[match.participants objectAtIndex:secondIsDone] matchOutcome] == GKTurnBasedMatchOutcomeWon)
            outcome = GKTurnBasedMatchOutcomeLost;
        else 
            outcome = GKTurnBasedMatchOutcomeWon;
        [[match.participants objectAtIndex:firstIsDone] setMatchOutcome:outcome];
    }
    if ((firstIsDone || secondIsDone) && match.status != GKTurnBasedMatchStatusEnded
        && [match.currentParticipant.playerID isEqual:[[NSApp delegate] localPlayer].playerID]
    )
        [match endMatchInTurnWithMatchData:[self matchData] completionHandler:^(NSError *error) {}];
    
    return firstIsDone || secondIsDone;
}

- (void) updateMatchForEndOfGame:(MBCMoveCode)cmd
{
    if ([match.currentParticipant.playerID isEqual:[[NSApp delegate] localPlayer].playerID]) {
        //
        // Participant whose turn it is updates the match
        //
        GKTurnBasedMatchOutcome whiteOutcome;
        GKTurnBasedMatchOutcome blackOutcome;
        
        if (cmd == kCmdDraw) {
            whiteOutcome    = GKTurnBasedMatchOutcomeTied;
            blackOutcome    = GKTurnBasedMatchOutcomeTied;
        } else if (cmd == kCmdWhiteWins) {
            whiteOutcome    = GKTurnBasedMatchOutcomeWon;
            blackOutcome    = GKTurnBasedMatchOutcomeLost;
        } else {
            whiteOutcome    = GKTurnBasedMatchOutcomeLost;
            blackOutcome    = GKTurnBasedMatchOutcomeWon;
        }
        for (GKTurnBasedParticipant * participant in match.participants)
            if ([participant.playerID isEqual:[properties objectForKey:@"WhitePlayerID"]])
                [participant setMatchOutcome:whiteOutcome];
            else 
                [participant setMatchOutcome:blackOutcome];
        [match endMatchInTurnWithMatchData:[self matchData] completionHandler:^(NSError *error) {}];
    }
}

- (id)initWithMatch:(GKTurnBasedMatch *)gkMatch game:(NSDictionary *)gameData
{
    if (self = [self init]) {
        [self setDisallowSubstitutes:YES];
        [self setInvitees:nil];
        [self setMatch:gkMatch];
        [self loadGame:gameData];
        localWhite = [[[NSApp delegate] localPlayer].playerID isEqual:[properties objectForKey:@"WhitePlayerID"]];
        NSDictionary * localProps = [properties objectForKey:(localWhite ? @"WhiteProperties" : @"BlackProperties")];
        if (localProps)
            [properties addEntriesFromDictionary:localProps];
        [self updateChangeCount:NSChangeCleared];
        [[NSDocumentController sharedDocumentController] addDocument:self];
    } 
    return self;
}

- (NSString *)windowNibName
{
    return @"Board";
}

- (void)makeWindowControllers
{
    if ([[self windowControllers] count]) {
        //
        // Reused document
        //
        [[[self windowControllers] objectAtIndex:0] adjustLogView];
    } else {
        MBCBoardWin * windowController = [[MBCBoardWin alloc] initWithWindowNibName:[self windowNibName]];
        [self addWindowController:windowController];
        [windowController release];
    }
}

- (MBCSide) humanSide;
{
    if (!match)
        return gHumanSide[players];
    else 
        return localWhite ? kWhiteSide : kBlackSide;
}

- (BOOL) humanTurn
{
    if ([self gameDone])
        return NO;
    switch ([self humanSide]) {
    case kBothSides:
        return YES;
    case kWhiteSide:
        return !([board numMoves] & 1);
    case kBlackSide:
        return ([board numMoves] & 1);
    case kNeitherSide:
        return NO;
    }
}

- (BOOL) gameDone
{
    if (NSString * result = [properties objectForKey:@"Result"])
        return ![result isEqual:@"*"];
    else 
        return NO;
}

- (MBCSide) engineSide;
{
    return gEngineSide[players];
}

- (MBCSide) remoteSide
{
    if (!match)
        return kNeitherSide;
    else 
        return localWhite ? kBlackSide : kWhiteSide;
}

- (NSString *)nonLocalPlayerID
{
    if (!match)
        return nil;
    NSString * localPlayerID = [[NSApp delegate] localPlayer].playerID;
    for (GKTurnBasedParticipant * p in match.participants)
        if (![localPlayerID isEqual:p.playerID])
            return p.playerID;
    return nil;
}

- (BOOL) loadGame:(NSDictionary *)dict
{
    [self setEphemeral:NO];
    
	NSString * v = [dict objectForKey:@"Variant"];
	
	for (variant = kVarNormal; gVariantName[variant] && ![v isEqual:gVariantName[variant]]; )
		variant = static_cast<MBCVariant>(variant+1);
	if (!gVariantName[variant])
		variant = kVarNormal;
	
    NSString * whiteType = [dict objectForKey:@"WhiteType"];
    NSString * blackType = [dict objectForKey:@"BlackType"];
    
    if ([whiteType isEqual:kMBCHumanPlayer])
        if ([blackType isEqual:kMBCHumanPlayer])
            players = kHumanVsHuman;
        else
            players = kHumanVsComputer;
    else if ([blackType isEqual:kMBCHumanPlayer])
        players = kComputerVsHuman;
    else
        players = kComputerVsComputer;
    properties = [dict mutableCopy];
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
    for (int i = 0; sProperties[i]; ++i)
        if (![properties valueForKey:sProperties[i]])
            [properties setValue:[defaults objectForKey:sProperties[i]]
                          forKey:sProperties[i]];

    [[NSNotificationQueue defaultQueue]
     enqueueNotification:[NSNotification notificationWithName:MBCGameLoadNotification
                                                       object:self userInfo:dict]
     postingStyle:NSPostWhenIdle];
    [[NSNotificationQueue defaultQueue]
     enqueueNotification:[NSNotification notificationWithName:MBCGameStartNotification
                                                       object:self]
     postingStyle:NSPostWhenIdle];
    
    if (match)
        dispatch_async(dispatch_get_main_queue(), ^{
            [self updateMatchForRemoteMove];
        });
   
	return YES;
}

- (BOOL)readFromData:(NSData *)docData ofType:(NSString *)docType error:(NSError **)outError
{
	NSPropertyListFormat	format;
	NSDictionary *			gameData = 
		[NSPropertyListSerialization propertyListWithData:docData options:0 format:&format error:outError];
    if (NSString * matchID = [gameData objectForKey:@"MatchID"]) {
        [[NSApp delegate] loadMatch:matchID];
        if (outError)
            *outError = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:nil];
        
        return NO;
    }
    
	[self loadGame:gameData];
    [self updateChangeCount:NSChangeCleared];

	return YES;
}

- (NSArray *)writableTypesForSaveOperation:(NSSaveOperationType)saveOperation
{
	//
	// Don't filter out PGN, even though we're not an editor for that type
	//
	return [[self class] writableTypes];
}

- (void) parseName:(NSString *)fullName intoFirst:(NSString **)firstName
			  last:(NSString **)lastName
{
	// 
	// Get name as UTF8. If the name is longer than 99 bytes, the last
	// character might be bad if it's non-ASCII. What sane person would
	// have such a long name anyway?
	//
	char	   n[100];
	strlcpy(n, [fullName UTF8String], 100);
	
	char * first 	= n+strspn(n, " \t"); 	// Beginning of first name
	char * last;
	char * nb1  	= NULL;			 		// Beginning of last word
	char * ne1  	= NULL;			 		// End of last word
	char * nb2  	= NULL;			 		// Beginning of last but one word
	char * ne2  	= NULL;			 		// End of last but two word
	char * ne3  	= NULL;            		// End of last but three word
    
	nb1	  = first;
	ne1   = nb1+strcspn(nb1, " \t");
    
	for (char * n; (n = ne1+strspn(ne1, " \t")) && *n; ) {
		ne3	= ne2; 
		nb2 = nb1;
		ne2 = ne1;
		nb1	= n;
		ne1 = nb1+strcspn(nb1, " \t");
	}
    
	if (ne3 && *nb2 >= 'a' && *nb2 <= 'z') { 
		//
		// Name has at least 3 words and last but one is 
		// lowercase, as in Ludwig van Beethoven
		//
		last = nb2;
		*ne3 = 0;
	} else if (ne2) {
		//
		// Name has at least two words. If 3 or more, last but one is 
		// uppercase as in John Wayne Miller
		//
		last = nb1;
		*ne2 = 0;
	} else {		// Name is single word
		last = ne1;
		*ne1 = 0;
	}
	*firstName	= [NSString stringWithUTF8String:first];
	*lastName	= [NSString stringWithUTF8String:last];
}

- (NSString *)pgnHeader
{
	NSString * 		wf;
	NSString *		wl;
	NSString *		bf;
	NSString * 		bl;
	[self parseName:[properties objectForKey:@"White"] intoFirst:&wf last:&wl];
	[self parseName:[properties objectForKey:@"Black"] intoFirst:&bf last:&bl];
	NSString *		humanw	=
    [NSString stringWithFormat:@"%@, %@", wl, wf];
	NSString *		humanb	=
    [NSString stringWithFormat:@"%@, %@", bl, bf];
	NSString * 		engine 		= 
    [NSString stringWithFormat:@"Apple Chess %@",
     [[NSBundle mainBundle] 
      objectForInfoDictionaryKey:@"CFBundleVersion"]];
	NSString * white;
	NSString * black;
	switch ([self engineSide]) {
    case kBlackSide:
        white 	= humanw;
        black 	= engine;
        break;
    case kWhiteSide:
        white 	= engine;
        black	= humanb;
        break;
    case kNeitherSide:
        white	= humanw;
        black	= humanb;
        
        break;
    case kBothSides:
        white	= engine;
        black	= engine;
        break;
	}
    
	//
	// PGN uses a standard format that is NOT localized
	//
	NSString * format = 
    [NSString stringWithUTF8String:
     "[Event \"%@\"]\n"
     "[Site \"%@, %@\"]\n"
     "[Date \"%@\"]\n"
     "[Round \"-\"]\n"
     "[White \"%@\"]\n"
     "[Black \"%@\"]\n"
     "[Result \"%@\"]\n"
     "[Time \"%@\"]\n"];
	return [NSString stringWithFormat:format,
            [properties objectForKey:@"Event"],
            [properties objectForKey:@"City"],
            [properties objectForKey:@"Country"],
            [properties objectForKey:@"StartDate"], 
            white, black, 
            [properties objectForKey:@"Result"], 
            [properties objectForKey:@"StartTime"]];
}

- (NSString *)pgnResult
{
	return [properties objectForKey:@"Result"];
}

- (void) saveMovesToFile:(FILE *)f
{
	NSString *  header = [self pgnHeader];
	NSData *	encoded = [header dataUsingEncoding:NSISOLatin1StringEncoding
                            allowLossyConversion:YES];
	fwrite([encoded bytes], 1, [encoded length], f);
    
	//
	// Add variant tag (nonstandard, but used by xboard & Co.)
	//
	if (variant != kVarNormal)
		fprintf(f, "[Variant \"%s\"]\n", [gVariantName[variant] UTF8String]);
	//
	// Mark nonhuman players
	//
    MBCSide engineSide = gEngineSide[players];
    
	if (SideIncludesWhite(engineSide)) 
		fprintf(f, "[WhiteType: \"program\"]\n");
	if (SideIncludesBlack(engineSide)) 
		fprintf(f, "[BlackType: \"program\"]\n");
    
	[board saveMovesTo:f];
	
	fputc('\n', f);
	fputs([[self pgnResult] UTF8String], f);
	fputc('\n', f);
}

- (BOOL) saveMovesTo:(NSString *)fileName
{
	FILE * f = fopen([fileName fileSystemRepresentation], "w");

    [self saveMovesToFile:f];
    
	fclose(f);
    
	return YES;
}

- (BOOL)writeToURL:(NSURL *)fileURL ofType:(NSString *)docType error:(NSError **)outError
{
	BOOL res;

	if ([docType isEqualToString:@"com.apple.chess.pgn"])
		res = [self saveMovesTo:[fileURL path]];
	else
		res = [super writeToURL:fileURL ofType:docType error:outError];

	return res;
}

- (NSDocument *)duplicateAndReturnError:(NSError **)outError
{
    if (match) {
        if (outError)
            *outError = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadUnsupportedSchemeError userInfo:
                         [NSDictionary dictionaryWithObjectsAndKeys:NSLocalizedString(@"cant_duplicate_gc", @"Cannot duplicate GameCenter games"), NSLocalizedDescriptionKey, nil]];
        return nil;
    }
    return [super duplicateAndReturnError:outError];
}

- (NSMutableDictionary *) saveGameToDict
{
    MBCSide     humanSide   = gHumanSide[players];
    NSString *  whiteType   = SideIncludesWhite(humanSide) ? kMBCHumanPlayer : kMBCEnginePlayer;
    NSString *  blackType   = SideIncludesBlack(humanSide) ? kMBCHumanPlayer : kMBCEnginePlayer;
	NSMutableDictionary * dict = [properties mutableCopy];
    [dict addEntriesFromDictionary:
     [NSDictionary dictionaryWithObjectsAndKeys:
      gVariantName[variant], @"Variant",
      whiteType, @"WhiteType",
      blackType, @"BlackType",
      [board fen], @"Position",
      [board holding], @"Holding",
      [board moves], @"Moves",
      nil]];
    
	return [dict autorelease];
}

- (NSData *)dataRepresentationOfType:(NSString *)aType
{
    NSMutableDictionary * game = [self saveGameToDict];
    if (match)
        [game setObject:match.matchID forKey:@"MatchID"];
	return [NSPropertyListSerialization 
			   dataFromPropertyList:game
			   format: NSPropertyListXMLFormat_v1_0
			   errorDescription:nil];
}

- (NSData *)matchData
{
    NSMutableDictionary * game = [self saveGameToDict];
    NSArray * keys             = [game allKeys];
    NSMutableDictionary * local= [NSMutableDictionary dictionary];
    for (NSString * key in keys)
        if ([[key substringToIndex:3] isEqual:@"MBC"]) {
            [local setObject:[game objectForKey:key] forKey:key];
            [game removeObjectForKey:key];
        }
    [game setObject:local forKey:(localWhite ? @"WhiteProperties" : @"BlackProperties")];
    
    return [NSPropertyListSerialization 
            dataFromPropertyList:game
            format: NSPropertyListXMLFormat_v1_0
            errorDescription:nil];
}

+ (NSURL *)casualGameSaveLocation
{
	NSString *	savePath = [[[NSFileManager defaultManager] currentDirectoryPath] stringByAppendingPathComponent:@"Casual.game"];
	
	return [NSURL fileURLWithPath:savePath];
}

- (BOOL)readFromURL:(NSURL *)absoluteURL ofType:(NSString *)typeName error:(NSError **)outError
{
	BOOL loaded = [super readFromURL:absoluteURL ofType:typeName error:outError];
	if (loaded && [absoluteURL isEqual:[MBCDocument casualGameSaveLocation]]) // Upgrade legacy autosave
		[self setFileURL:nil];
	
	return loaded;
}

- (BOOL) canTakeback
{
    return [board canUndo] && players != kComputerVsComputer
        && (!match || [match.currentParticipant.playerID isEqual:[[NSApp delegate] localPlayer].playerID]);
}

- (void) setBoard:(MBCBoard *)b
{
    board = b;
    [board setDocument:self];
}

- (void) updateMatchForLocalMove
{
    if (offerDraw) {
        if (![properties objectForKey:@"Request"])
            [properties setObject:@"Draw" forKey:@"Request"];
        [self setOfferDraw:NO];
    }
    MBCEndTurn(match, [self matchData]);
}

- (void) updateMatchForRemoteMove
{
    if ([self checkForEndOfGame]) {
        [self postMatchOutcomeNotification];
    } else {            
        NSPropertyListFormat	format;
        NSDictionary *			gameData = 
        [NSPropertyListSerialization propertyListWithData:match.matchData options:0 format:&format error:nil];
        if (![properties objectForKey:@"BlackPlayerID"] && [gameData objectForKey:@"BlackPlayerID"])
            [properties setObject:[gameData objectForKey:@"BlackPlayerID"] forKey:@"BlackPlayerID"];
        if (![properties objectForKey:@"WhitePlayerID"] && [gameData objectForKey:@"WhitePlayerID"])
            [properties setObject:[gameData objectForKey:@"WhitePlayerID"] forKey:@"WhitePlayerID"];
        if ([[gameData objectForKey:@"Request"] isEqual:@"Takeback"]) {
            [[[self windowControllers] objectAtIndex:0] requestTakeback];
        } else if (NSString * response = [gameData objectForKey:@"Response"]) {
            [properties removeObjectForKey:@"Request"];
            [[[self windowControllers] objectAtIndex:0] handleRemoteResponse:response];
        } else if ([gameData objectForKey:@"Moves"]) {
            NSArray *             moves = [[gameData objectForKey:@"Moves"] componentsSeparatedByString:@"\n"];
            if ([moves count]-1 > [board numMoves]) {
                MBCMove *             lastMove = [MBCMove moveFromEngineMove:[moves objectAtIndex:[moves count]-2]];
                [[NSNotificationCenter defaultCenter] 
                 postNotificationName:(localWhite ? MBCUncheckedBlackMoveNotification : MBCUncheckedWhiteMoveNotification)
                 object:self userInfo:(id)lastMove];
            }
            if ([[gameData objectForKey:@"Request"] isEqual:@"Draw"]) {
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 200*NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
                    [[[self windowControllers] objectAtIndex:0] requestDraw];
                });
            }
        }
    }
    [properties removeObjectForKey:@"Request"];
    [properties removeObjectForKey:@"Response"];
}

- (void) offerTakeback
{
    [properties setObject:@"Takeback" forKey:@"Request"];
    [self updateMatchForLocalMove];
}

- (void) allowTakeback:(BOOL)allow
{
    [properties setObject:(allow ? @"Takeback" : @"NoTakeback") forKey:@"Response"];
    [self updateMatchForLocalMove];
}

- (void) resign
{
    NSString *  localPlayerID = [[NSApp delegate] localPlayer].playerID;
    BOOL        wasOurTurn;
    for (GKTurnBasedParticipant * p in match.participants) 
        if ([p.playerID isEqual:localPlayerID]) {
            [p setMatchOutcome:GKTurnBasedMatchOutcomeLost];
            wasOurTurn = match.currentParticipant == p;
            break;
        }
    
    [self updateMatchForRemoteMove];
    
    if (!wasOurTurn)
       [match participantQuitOutOfTurnWithOutcome:GKTurnBasedMatchOutcomeLost
                            withCompletionHandler:^(NSError *error){}];
}

@end

void 
MBCAbort(NSString * message, MBCDocument * doc)
{
    fprintf(stderr, "%s in game %p\n", [message UTF8String], doc);
    abort();
}


// Local Variables:
// mode:ObjC
// End:
