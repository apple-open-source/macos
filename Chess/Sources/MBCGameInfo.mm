/*
	File:		MBCGameInfo.mm
	Contains:	Managing information about the current game
	Copyright:	© 2003-2011 by Apple Inc., all rights reserved.

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

#import "MBCGameInfo.h"
#import "MBCController.h"
#import "MBCPlayer.h"
#import "MBCDocument.h"
#import "MBCUserDefaults.h"

#import <GameKit/GameKit.h>

//
// Private Framework
//
#import <GeoKit/GeoKit.h>

#include <sys/types.h>
#include <regex.h>
#include <algorithm>

NSArray *   sVoices;

@implementation MBCGameInfo

@synthesize document = fDocument;
@synthesize moveList = fMoveList;
@synthesize whiteEditable, blackEditable;

NSString * kMBCShowMoveInTitle  = @"MBCShowMoveInTitle";
//
// Obsolete: Parsing the human name serves no purpose and carries some risk
//
NSString * kMBCHumanFirst		= @"MBCHumanFirst";
NSString * kMBCHumanLast		= @"MBCHumanLast";

+ (void)initialize
{
	NSUserDefaults * 	userDefaults = [NSUserDefaults standardUserDefaults];
	NSString * humanName;

	//
	// Deal with legacy user name default representation
	//
	if ([userDefaults stringForKey:kMBCHumanFirst]) {
		humanName = [NSString stringWithFormat:@"%@ %@", 
							  [userDefaults stringForKey:kMBCHumanFirst],
							  [userDefaults stringForKey:kMBCHumanLast]];
		[userDefaults removeObjectForKey:kMBCHumanFirst];
		[userDefaults removeObjectForKey:kMBCHumanLast];
	} else
		humanName = NSFullUserName();

	// 
	// Get the city we might be in. 
	//
	// PGN wants IOC codes for countries, which we're too lazy to convert.
	//
	GEOCity *		cityInfo = [GEOCity systemCity];
	NSString *		city 	= cityInfo ? [cityInfo displayName] : @"?";
	NSString *		country	= cityInfo ? [[cityInfo country] displayName] : @"?";

	NSString * event = 
		[NSLocalizedString(@"casual_game", @"Casual Game") retain];

	NSDictionary * defaults = 
		[NSDictionary 
			dictionaryWithObjectsAndKeys:
				 humanName, kMBCHumanName,
			          city, kMBCGameCity,
   			       country, kMBCGameCountry,
			         event, kMBCGameEvent,
			nil];
	[userDefaults registerDefaults: defaults];
    sVoices = [[NSSpeechSynthesizer availableVoices] retain];
}

const int kNumFixedMenuItems = 2;

- (void)loadVoiceMenu:(id)menu
{
    for (NSString * voiceIdentifier in sVoices) 
        [menu addItemWithTitle:[[NSSpeechSynthesizer attributesForVoice:voiceIdentifier] objectForKey:NSVoiceName]];
}

- (NSString *)voiceAtIndex:(NSUInteger)menuIndex
{
    if (menuIndex < kNumFixedMenuItems)
        return nil;
    else 
        return [sVoices objectAtIndex:menuIndex-kNumFixedMenuItems];
}

- (NSUInteger)indexForVoice:(NSString *)voiceId
{
    return [voiceId length] ? [sVoices indexOfObject:voiceId]+kNumFixedMenuItems : 0;
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

- (void)awakeFromNib
{
    [self loadVoiceMenu:fPrimaryVoiceMenu];
    [self loadVoiceMenu:fAlternateVoiceMenu];

	fStyleLocMap = [[NSMutableDictionary alloc] init];
    [fBoardStyle removeAllItems];
    [fPieceStyle removeAllItems];
    
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
}

- (void) setPlayerAlias:(NSString *)playerID forKey:(NSString *)key
{
    if (!playerID)
        playerID = [fDocument nonLocalPlayerID];
    if (playerID)
        [GKPlayer loadPlayersForIdentifiers:[NSArray arrayWithObject:playerID]
            withCompletionHandler:^(NSArray *players, NSError *error) {
                if (!error) {
                    [fDocument setObject:[[players objectAtIndex:0] alias] forKey:key];
                    [self updateMoves:nil];
                }
            }];
}

- (void) removeChessObservers
{
    if (!fHasObservers)
        return;
    
    NSNotificationCenter * notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter removeObserver:self name:MBCEndMoveNotification object:nil];
    [notificationCenter removeObserver:self name:MBCTakebackNotification object:nil];
    [notificationCenter removeObserver:self name:MBCIllegalMoveNotification object:nil];
    [notificationCenter removeObserver:self name:MBCGameEndNotification object:nil];
    
    fHasObservers = NO;
}

- (void)dealloc
{
    [self removeChessObservers];
    [super dealloc];
}

- (void) startGame:(MBCVariant)variant playing:(MBCSide)sideToPlay
{
    [self removeChessObservers];
    NSNotificationCenter * notificationCenter = [NSNotificationCenter defaultCenter];
	[notificationCenter 
     addObserver:self
     selector:@selector(updateMoves:)
     name:MBCEndMoveNotification
     object:fDocument];
	[notificationCenter 
     addObserver:self
     selector:@selector(takeback:)
     name:MBCTakebackNotification
     object:fDocument];
	[notificationCenter 
     addObserver:self
     selector:@selector(updateMoves:)
     name:MBCIllegalMoveNotification
     object:fDocument];
	[notificationCenter 
     addObserver:self
     selector:@selector(gameEnd:)
     name:MBCGameEndNotification
     object:fDocument];
    fHasObservers = YES;

    //
    // Fill in missing properties
    //
    NSUserDefaults *        defaults    =   [NSUserDefaults standardUserDefaults];
    NSString *              human       =   [defaults stringForKey:kMBCHumanName];
    NSString *              engine      =   NSLocalizedString(@"engine_player", @"Computer");
    NSMutableDictionary *   props       =   fDocument.properties;
    BOOL                    gameCenter  =   [fDocument remoteSide] != kNeitherSide;
    
    [self setWhiteEditable: !gameCenter && SideIncludesWhite(sideToPlay)];
    [self setBlackEditable: !gameCenter && SideIncludesBlack(sideToPlay)];
    
    if (![props objectForKey:@"White"])
        if (gameCenter)
            [self setPlayerAlias:[props objectForKey:@"WhitePlayerID"] forKey:@"White"];
        else if (SideIncludesWhite(sideToPlay))
            [fDocument setObject:human forKey:@"White"];
        else 
            [fDocument setObject:engine forKey:@"White"];
    if (![props objectForKey:@"Black"])
        if (gameCenter)
            [self setPlayerAlias:[props objectForKey:@"BlackPlayerID"] forKey:@"Black"];
        else if (SideIncludesBlack(sideToPlay))
            [fDocument setObject:human forKey:@"Black"];
        else 
            [fDocument setObject:engine forKey:@"Black"];

    NSDate * now	= [NSDate date];
    if (![props objectForKey:@"StartDate"])
        [fDocument setObject:[now descriptionWithCalendarFormat:@"%Y.%m.%d" timeZone:nil locale:nil] 
                      forKey:@"StartDate"];
    if (![props objectForKey:@"StartTime"])
        [fDocument setObject:[now descriptionWithCalendarFormat:@"%H:%M:%S" timeZone:nil locale:nil] 
                      forKey:@"StartTime"];
    if (![props objectForKey:@"Result"])
        [fDocument setObject:@"*" forKey:@"Result"];
    if (![props objectForKey:@"City"])
        if (gameCenter)
            [fDocument setObject:NSLocalizedString(@"cloud_city", @"Game Center") forKey:@"City"];
        else
            [fDocument setObject:[defaults stringForKey:kMBCGameCity] forKey:@"City"];
    if (![props objectForKey:@"Country"])
        if (gameCenter)
            [fDocument setObject:NSLocalizedString(@"cloud_country", @"The Cloud") forKey:@"Country"];
        else
            [fDocument setObject:[defaults stringForKey:kMBCGameCountry] forKey:@"Country"];
    if (![props objectForKey:@"Event"])
        [fDocument setObject:[defaults stringForKey:kMBCGameEvent] forKey:@"Event"];

	fRows	= 0;
    
    [self updateMoves:nil];
}

- (void) updateMoves:(NSNotification *)notification
{
	[self willChangeValueForKey:@"gameTitle"];
    NSDictionary * props = fDocument.properties;
    if (![props objectForKey:@"White"])
        [self setPlayerAlias:[props objectForKey:@"WhitePlayerID"] forKey:@"White"];
    if (![props objectForKey:@"Black"])
        [self setPlayerAlias:[props objectForKey:@"BlackPlayerID"] forKey:@"Black"];
	[fMoveList reloadData];
	[fMoveList setNeedsDisplay:YES];

	int rows = [self numberOfRowsInTableView:fMoveList]; 
	if (rows != fRows) {
		fRows = rows;
		[fMoveList scrollRowToVisible:rows-1];
	}
	[self didChangeValueForKey:@"gameTitle"];
}

- (void) takeback:(NSNotification *)notification
{
    [fDocument setObject:@"*" forKey:@"Result"];

	[self updateMoves:notification];
}

- (void) gameEnd:(NSNotification *)notification
{
    dispatch_async(dispatch_get_main_queue(), ^{
        MBCMove *    move 	= reinterpret_cast<MBCMove *>([notification userInfo]);

        switch (move->fCommand) {
        case kCmdWhiteWins:
            [fDocument setObject:@"1-0" forKey:@"Result"];
            break;
        case kCmdBlackWins:
            [fDocument setObject:@"0-1" forKey:@"Result"];
            break;
        case kCmdDraw:
            [fDocument setObject:@"1/2-1/2" forKey:@"Result"];
            break;
        default:
            return;
        }
        [self updateMoves:notification];
    });
}

- (NSString *)outcome
{
    NSString * result = [fDocument objectForKey:@"Result"];
	if ([result isEqual:@"1-0"])
		return NSLocalizedString(@"white_win_msg", @"White wins");
	if ([result isEqual:@"0-1"])
		return NSLocalizedString(@"black_win_msg", @"Black wins");
	else if ([result isEqual:@"1/2-1/2"]) 
		return NSLocalizedString(@"draw_msg", @"Draw");
    
    return nil;
}

- (void)editProperties:(NSWindow *)sheet modalForWindow:(NSWindow *)window
{
    fEditedProperties = [[NSMutableDictionary alloc] init];
    
	[NSApp beginSheet:sheet
       modalForWindow:window
        modalDelegate:self
       didEndSelector:@selector(didEndSheet:returnCode:contextInfo:)
          contextInfo:nil];    
}

- (IBAction) editInfo:(id)sender
{
    [self editProperties:fEditSheet modalForWindow:[sender window]];
}

- (void)editPreferencesForWindow:(NSWindow *)window
{
    [fPrimaryVoiceMenu selectItemAtIndex:[self indexForVoice:[fDocument objectForKey:kMBCDefaultVoice]]];
    [fAlternateVoiceMenu selectItemAtIndex:[self indexForVoice:[fDocument objectForKey:kMBCAlternateVoice]]];
    [fBoardStyle selectItemWithTitle:[self localizedStyleName:[fDocument objectForKey:kMBCBoardStyle]]];
    [fPieceStyle selectItemWithTitle:[self localizedStyleName:[fDocument objectForKey:kMBCPieceStyle]]];
    
    [self editProperties:fPrefsSheet modalForWindow:window];
}

- (id)valueForUndefinedKey:(NSString *)key
{
    return [fDocument objectForKey:key];
}

- (void)setValue:(id)value forUndefinedKey:(NSString *)key
{
    if (![fEditedProperties objectForKey:key]) {
        id oldValue = [fDocument objectForKey:key];
        if (!oldValue)
            oldValue = [NSNull null];
        [fEditedProperties setObject:oldValue forKey:key];
    }

    [fDocument setValue:value forKey:key];
}

+ (NSSet *)keyPathsForValuesAffectingValueForKey:(NSString *)key
{
    if (isupper([key characterAtIndex:0]))
        return [NSSet setWithObject:[@"document." stringByAppendingString:key]];
    else 
        return [NSSet set];
}

- (void) didEndSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)ctx
{
    [sheet orderOut:self];
}

- (IBAction) cancelProperties:(id)sender
{
    //
    // Restore all the values that were changed
    //
	[NSApp endSheet:[sender window]];
    for (NSString * prop in fEditedProperties) 
        [fDocument setObject:[fEditedProperties objectForKey:prop] forKey:prop];
    [fEditedProperties release];
}

- (IBAction) updateProperties:(id)sender
{
    NSUserDefaults * 	defaults 	= [NSUserDefaults standardUserDefaults];
   
    //
    // Update defaults
    //
    for (NSString * edited in fEditedProperties) {
        id val = [fDocument objectForKey:edited];
        if ([edited isEqual:@"White"] 
         || ([edited isEqual:@"Black"] && ![fEditedProperties objectForKey:@"White"])
        )
            [defaults setObject:val forKey:kMBCHumanName]; 
        else if ([edited isEqual:@"City"] ||[edited isEqual:@"Country"] || [edited isEqual:@"Event"])
            [defaults setObject:val forKey:[@"MBCGame" stringByAppendingString:edited]];
        else if ([[edited substringToIndex:3] isEqual:@"MBC"])
            [defaults setObject:val forKey:edited];
    }
    if ([fEditedProperties objectForKey:kMBCSearchTime])
        [fDocument updateSearchTime];

	[NSApp endSheet:[sender window]];
    [fEditedProperties release];
}

- (IBAction) updateVoices:(id)sender;
{
    NSString * voice    = [self voiceAtIndex:[sender indexOfSelectedItem]];
    NSString * pvoice   = voice ? voice : @"";
    if ([sender tag])
        [self setValue:pvoice forKey:kMBCAlternateVoice];
    else
        [self setValue:pvoice forKey:kMBCDefaultVoice];

    NSSpeechSynthesizer *   selectedSynth = [[NSSpeechSynthesizer alloc] initWithVoice:voice];
    NSString *              demoText      = 
        [[NSSpeechSynthesizer attributesForVoice:[selectedSynth voice]] 
         objectForKey:NSVoiceDemoText];
    if (demoText)
        [selectedSynth startSpeakingString:demoText];
}

- (IBAction) updateStyles:(id)sender;
{
	NSString *			boardStyle	= 
        [self unlocalizedStyleName:[fBoardStyle titleOfSelectedItem]];
	NSString *			pieceStyle  = 
        [self unlocalizedStyleName:[fPieceStyle titleOfSelectedItem]];

	[self setValue:boardStyle forKey:kMBCBoardStyle];
	[self setValue:pieceStyle forKey:kMBCPieceStyle];
}

- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
	return ([fBoard numMoves]+1) / 2;
}

- (id)tableView:(NSTableView *)v objectValueForTableColumn:(NSTableColumn *)col row:(int)row
{
	NSString * 		ident 	= [col identifier];
	if ([ident isEqual:@"Move"]) {
		return [NSNumber numberWithInt:row+1];
	} else {
		NSArray * identComp = [ident componentsSeparatedByString:@"."];
		return [[fBoard move: row*2+[[identComp objectAtIndex:0] isEqual:@"Black"]] 
				valueForKey:[identComp objectAtIndex:1]];
	}
    return nil;
}

- (BOOL)tableView:(NSTableView *)aTableView shouldSelectRow:(NSInteger)rowIndex
{
	return NO; /* Disallow all selections */
}

- (NSString *)describeMove:(int)move
{
    NSDictionary * localization = nil;
    
    if (NSURL * url = [[NSBundle mainBundle] URLForResource:@"Spoken" withExtension:@"strings"
                                    subdirectory:nil]
    )
        localization = [NSDictionary dictionaryWithContentsOfURL:url];
    NSString * nr_fmt =
        NSLocalizedStringFromTable(@"move_table_nr", @"Spoken", @"Move %d");
    NSString * text = [NSString localizedStringWithFormat:nr_fmt, move];
    int white = (move-1)*2;
    if (white < [fBoard numMoves])
        text = [text stringByAppendingFormat:@"\n\n%@",
                [fBoard extStringFromMove:[fBoard move:white]
                         withLocalization:localization]];
    int black = move*2 - 1;
    if (black < [fBoard numMoves])
        text = [text stringByAppendingFormat:@"\n\n%@",
                [fBoard extStringFromMove:[fBoard move:black]
                         withLocalization:localization]];
    
    return text;
}

- (NSString *)gameTitle
{
    if (!fDocument || [fDocument brandNewGame])
        return @"";
    
	NSString * 		move;
	int		 		numMoves = [fBoard numMoves];		
	
	if (numMoves && [[NSUserDefaults standardUserDefaults] boolForKey:kMBCShowMoveInTitle]) {
		NSNumber * moveNum = [NSNumber numberWithInt:(numMoves+1)/2];
		NSString * moveStr = [NSNumberFormatter localizedStringFromNumber:moveNum 
															  numberStyle:NSNumberFormatterDecimalStyle];
		move = 	[NSString localizedStringWithFormat:NSLocalizedString(@"title_move_line_fmt", @"%@. %@%@"),
						  moveStr, numMoves&1 ? @"":@"\xE2\x80\xA6 ",
						  [[fBoard lastMove] localizedText]];
    } else if ([[fDocument objectForKey:@"Request"] isEqual:@"Takeback"]) {
        move = NSLocalizedString(@"takeback_msg", @"Takeback requested");
	} else if (!numMoves) {
		move =	NSLocalizedString(@"new_msg", @"New Game");
	} else if (numMoves & 1) {
		move = 	NSLocalizedString(@"black_move_msg", @"Black to move");
	} else {
		move = 	NSLocalizedString(@"white_move_msg", @"White to move");
	}

	if (NSString * outcome = [self outcome])
		move = outcome;
	NSString * title =
		[NSString localizedStringWithFormat:NSLocalizedString(@"game_title_fmt", @"%@ - %@   (%@)"), 
				  [fDocument objectForKey:@"White"], [fDocument objectForKey:@"Black"], move];
    
    return title;
}

@end

// Local Variables:
// mode:ObjC
// End:
