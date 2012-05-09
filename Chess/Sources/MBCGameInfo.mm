/*
	File:		MBCGameInfo.mm
	Contains:	Managing information about the current game
	Version:	1.0
	Copyright:	© 2003-2010 by Apple Computer, Inc., all rights reserved.
*/

#import "MBCGameInfo.h"
#import "MBCController.h"
#import "MBCPlayer.h"

//
// Private Framework
//
#import <GeoKit/GeoKit.h>

#include <sys/types.h>
#include <regex.h>
#include <algorithm>

static NSTextTab * MakeTab(NSTextTabType type, float location)
{
	return [[[NSTextTab alloc] initWithType:type location:location] 
			   autorelease];
}

@implementation MBCGameInfo

NSString * kMBCGameCity			= @"MBCGameCity";
NSString * kMBCGameCountry		= @"MBCGameCountry";
NSString * kMBCHumanFirst		= @"MBCHumanFirst";
NSString * kMBCHumanLast		= @"MBCHumanLast";
NSString * kMBCGameEvent		= @"MBCGameEvent";
NSString * kMBCShowMoveInTitle 	= @"MBCShowMoveInTitle";

+ (void) parseName:(NSString *)fullName intoFirst:(NSString **)firstName
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

+ (void)initialize
{
	//
	// Parse the user name into last and first name
	//
	NSString * humanFirst;
	NSString * humanLast;

	[MBCGameInfo parseName:NSFullUserName() 
				 intoFirst:&humanFirst last:&humanLast];

	// 
	// Get the city we might be in. This technique uses a private API and may not
	// work in future revisions of Mac OS X. 
	//
	// PGN wants IOC codes for countries, which we're too lazy to convert.
	//
	NSDictionary*	cityInfoDict = [[NSUserDefaults standardUserDefaults] objectForKey:@"com.apple.preferences.timezone.selected_city"];    
	GEOCity *		cityInfo = cityInfoDict ? [GEOCity cityWithDumpDictionary:cityInfoDict] : nil;
	NSString *		city 	= cityInfo ? [cityInfo displayName] : @"?";
	NSString *		country	= cityInfo ? [[cityInfo country] displayName] : @"?";

	NSString * event = 
		[NSLocalizedString(@"casual_game", @"Casual Game") retain];

	NSDictionary * defaults = 
		[NSDictionary 
			dictionaryWithObjectsAndKeys:
				humanFirst, kMBCHumanFirst,
				 humanLast, kMBCHumanLast,
			          city, kMBCGameCity,
   			       country, kMBCGameCountry,
			         event, kMBCGameEvent,
			nil];
	[[NSUserDefaults standardUserDefaults] registerDefaults: defaults];
}

+ (void)restoreWindowWithIdentifier:(NSString *)itemID state:(NSCoder *)coder completionHandler:(void (^)(NSWindow *, NSError *))handler {
    handler([[MBCController controller] gameInfoWindow], NULL);
}

- (id) init
{
	[[NSNotificationCenter defaultCenter] 
		addObserver:self
		selector:@selector(updateMoves:)
		name:MBCEndMoveNotification
		object:nil];
	[[NSNotificationCenter defaultCenter] 
		addObserver:self
		selector:@selector(takeback:)
		name:MBCTakebackNotification
		object:nil];
	[[NSNotificationCenter defaultCenter] 
		addObserver:self
		selector:@selector(updateMoves:)
		name:MBCIllegalMoveNotification
		object:nil];
	[[NSNotificationCenter defaultCenter] 
		addObserver:self
		selector:@selector(gameEnd:)
		name:MBCGameEndNotification
		object:nil];

	fOutcome	= nil;
	fWhiteName	= nil;
	fBlackName	= nil;
	fStartDate	= nil;
	fStartTime	= nil;
	fResult		= nil;
	fSetInfo	= false;

	return self;
}

- (void)awakeFromNib
{
	NSUserDefaults *defaults 	= [NSUserDefaults standardUserDefaults];

	[fShowMoveInTitle setIntValue:[defaults boolForKey:kMBCShowMoveInTitle]];

	fRows	= 0;
	fBoard	= [[MBCController controller] board];

	if ([fInfoWindow respondsToSelector:@selector(setRestorationClass:)])
		[fInfoWindow setRestorationClass:[self class]];
}

- (void) updateMoves:(NSNotification *)notification
{
	[fMoveList reloadData];
	[fMoveList setNeedsDisplay:YES];
	fTitleNeedsUpdate = true;

	int rows = [self numberOfRowsInTableView:fMoveList]; 
	if (rows != fRows) {
		fRows = rows;
		[fMoveList scrollRowToVisible:rows-1];
	}
}

- (void) takeback:(NSNotification *)notification
{
	[fOutcome release];
	fOutcome 	= nil;
	[fResult release];
	fResult		= [@"*" retain];
	[self updateMoves:notification];
}

- (void) gameEnd:(NSNotification *)notification
{
	MBCMove *    move 	= reinterpret_cast<MBCMove *>([notification object]);

	[fResult autorelease];
	[fOutcome autorelease];
	switch (move->fCommand) {
	case kCmdWhiteWins:
		fResult		= @"1-0";
		fOutcome	= NSLocalizedString(@"white_win_msg", @"White wins");
		break;
	case kCmdBlackWins:
		fResult		= @"0-1";
		fOutcome 	= NSLocalizedString(@"black_win_msg", @"Black wins");
		break;
	case kCmdDraw:
		fResult		= @"1/2-1/2";
		fOutcome 	= NSLocalizedString(@"draw_msg", @"Draw");
		break;
	default:
		[fResult retain];
		[fOutcome retain];
		return;
	}
	[fResult retain];
	[fOutcome retain];
	[self updateTitle:nil];
}

- (NSDictionary *)getInfo
{
	NSUserDefaults *defaults 	= [NSUserDefaults standardUserDefaults];

	return [NSDictionary dictionaryWithObjectsAndKeys:
							 fWhiteName, @"White",
						 fBlackName, @"Black",
						 fStartDate, @"StartDate",
						 fStartTime, @"StartTime",
						 fResult, 	 @"Result",
						 [defaults stringForKey:kMBCGameCity], 	  @"City",
						 [defaults stringForKey:kMBCGameCountry], @"Country",
						 [defaults stringForKey:kMBCGameEvent],   @"Event",
						 nil];
}

- (void)setInfo:(NSDictionary *)dict
{
	NSUserDefaults *defaults 	= [NSUserDefaults standardUserDefaults];
	NSString * 		human		=
		[NSString stringWithFormat:@"%@ %@",
				  [defaults stringForKey:kMBCHumanFirst],
				  [defaults stringForKey:kMBCHumanLast]];
	NSString * 		engine 		= NSLocalizedString(@"engine_player", @"Computer");

	fSetInfo	= true;
	[fWhiteName release];
	[fBlackName release];
	if (NSString * white = [dict objectForKey:@"White"])
		fWhiteName 	= [white retain];
	else if ([[dict objectForKey:@"WhiteType"] isEqual:kMBCHumanPlayer])
		fWhiteName 	= [human retain];
	else
		fWhiteName 	= [engine retain];
	if (NSString * black = [dict objectForKey:@"Black"])
		fBlackName 	= [black retain];
	else if ([[dict objectForKey:@"BlackType"] isEqual:kMBCHumanPlayer])
		fBlackName 	= [human retain];
	else
		fBlackName 	= [engine retain];
	[fStartDate release];
	[fStartTime release];
	[fResult	release];
	fStartDate = [[dict objectForKey:@"StartDate"] retain];
	fStartTime = [[dict objectForKey:@"StartTime"] retain];
	fResult	   = [[dict objectForKey:@"Result"] retain];
	if (NSString * city = [dict objectForKey:@"City"])
		if (![city isEqual:[defaults stringForKey:kMBCGameCity]])
			[defaults setObject:city forKey:kMBCGameCity];
	if (NSString * country = [dict objectForKey:@"Country"])
		if (![country isEqual:[defaults stringForKey:kMBCGameCountry]])
			[defaults setObject:country forKey:kMBCGameCountry];
	if (NSString * event = [dict objectForKey:@"Event"])
		[defaults setObject:event forKey:kMBCGameEvent];
		
	[fOutcome release];
	fOutcome = nil;
	if ([fResult isEqual:@"1-0"])
		fOutcome	= NSLocalizedString(@"white_win_msg", @"White wins");
	if ([fResult isEqual:@"0-1"])
		fOutcome 	= NSLocalizedString(@"black_win_msg", @"Black wins");
	else if ([fResult isEqual:@"1/2-1/2"]) 
		fOutcome 	= NSLocalizedString(@"draw_msg", @"Draw");
	[fOutcome retain];
}

- (void) startGame:(MBCVariant)variant playing:(MBCSide)sideToPlay
{
	[NSObject cancelPreviousPerformRequestsWithTarget:self];
	if (!fSetInfo) {
		NSUserDefaults *defaults 	= [NSUserDefaults standardUserDefaults];
		NSString * 		human		=
			[NSString stringWithFormat:@"%@ %@",
					  [defaults stringForKey:kMBCHumanFirst],
					  [defaults stringForKey:kMBCHumanLast]];
		NSString * 		engine 		= NSLocalizedString(@"engine_player", @"Computer");

		[fWhiteName release];
		[fBlackName release];
		switch (fHuman = fSideToPlay = sideToPlay) {
		case kWhiteSide:
			fWhiteName 	= [human retain];
			fBlackName 	= [engine retain];
			break;
		case kBlackSide:
			fWhiteName 	= [engine retain];
			fBlackName 	= [human retain];
			break;
		case kBothSides:
			fWhiteName 	= [human retain];
			fBlackName 	= [human retain];
			fHuman	   	= kWhiteSide;
			break;
		case kNeitherSide:
			fWhiteName 	= [engine retain];
			fBlackName 	= [engine retain];
			break;
		}
		[fStartDate release];
		[fStartTime release];
		[fResult release];
		NSDate * now	= [NSDate date];
		fStartDate		= [[now descriptionWithCalendarFormat:@"%Y.%m.%d"
								timeZone:nil locale:nil] retain];
		fStartTime		= [[now descriptionWithCalendarFormat:@"%H:%M:%S"
								timeZone:nil locale:nil] retain];
		fResult			= [@"*" retain];
		[fOutcome release];
		fOutcome = nil;
	} else
		fSetInfo	= false;

	[self updateMoves:nil];
	[self updateTitle:nil];
}

- (IBAction) editInfo:(id)sender
{
	NSUserDefaults * 	defaults 	= [NSUserDefaults standardUserDefaults];
	[fWhite setStringValue:fWhiteName];
	[fBlack setStringValue:fBlackName];
	[fCity setStringValue:[defaults stringForKey:kMBCGameCity]];
	[fCountry setStringValue:[defaults stringForKey:kMBCGameCountry]];
	[fEvent setStringValue:[defaults stringForKey:kMBCGameEvent]];

	switch (fSideToPlay) {
	case kWhiteSide:
		[fWhite setEditable:YES];
		[fBlack setSelectable:NO];
		break;
	case kBlackSide:
		[fWhite setSelectable:NO];
		[fBlack setEditable:YES];
		break;
	case kBothSides:
		[fWhite setEditable:YES];
		[fBlack setEditable:YES];
		break;
	case kNeitherSide:
		[fWhite setSelectable:NO];
		[fBlack setSelectable:NO];
		break;
	}

	[NSApp beginSheet:fEditSheet
		   modalForWindow:fInfoWindow
		   modalDelegate:nil
		   didEndSelector:nil
		   contextInfo:nil];
    [NSApp runModalForWindow:fEditSheet];
	[NSApp endSheet:fEditSheet];
	[fEditSheet orderOut:self];
	[fInfoWindow makeKeyAndOrderFront:self];
}

- (IBAction) cancelInfo:(id)sender
{
	[NSApp stopModal];
}

- (IBAction) updateInfo:(id)sender
{
	NSUserDefaults * 	defaults 	= [NSUserDefaults standardUserDefaults];
	NSString *			human;

	fWhiteName = [[fWhite stringValue] retain];
	fBlackName = [[fBlack stringValue] retain];

	if (fHuman == kWhiteSide) 
		human = fWhiteName;
	else if (fHuman == kBlackSide) 
		human = fBlackName;
	else 
		human = nil; // Both or neither are humans, no default to learn here

	if (human) {
		NSString *	humanFirst;
		NSString * 	humanLast;

		[MBCGameInfo parseName:human
					 intoFirst:&humanFirst last:&humanLast];
		[defaults setObject:humanFirst forKey:kMBCHumanFirst];
		[defaults setObject:humanLast forKey:kMBCHumanLast];
	}

	NSString * cityName		= [fCity stringValue];
	if (![cityName isEqual:[defaults stringForKey:kMBCGameCity]])
		[defaults setObject:[fCity stringValue] forKey:kMBCGameCity];
	NSString * countryName	= [fCountry stringValue];
	if (![countryName isEqual:[defaults stringForKey:kMBCGameCountry]])
		[defaults setObject:[fCountry stringValue] forKey:kMBCGameCountry];
	[defaults setObject:[fEvent stringValue] forKey:kMBCGameEvent];

	[self updateTitle:nil];
	[NSApp stopModal];
}

- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
	return ([fBoard numMoves]+1) / 2;
}

- (id)tableView:(NSTableView *)v objectValueForTableColumn:(NSTableColumn *)col row:(int)row
{
	//
	// Defer title update until table gets redrawn
	//
	if (fTitleNeedsUpdate) {
		[self performSelector:@selector(updateTitle:) withObject:nil
			  afterDelay:0.01];
		fTitleNeedsUpdate = false;
	}

	NSString * 		ident 	= [col identifier];
	if ([ident isEqual:@"Move"]) {
		return [NSNumber numberWithInt:row+1];
	} else {
		NSArray * identComp = [ident componentsSeparatedByString:@":"];
		return [[fBoard move: row*2+[[identComp objectAtIndex:0] isEqual:@"Black"]] 
				valueForKey:[identComp objectAtIndex:1]];
	}
}

- (BOOL)tableView:(NSTableView *)aTableView shouldSelectRow:(NSInteger)rowIndex
{
	return NO; /* Disallow all selections */
}

- (IBAction) updateTitle:(id)sender
{
	NSString * 		move;
	int		 		numMoves = [fBoard numMoves];		

	if (sender) {
		NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

		[defaults setBool:
					  [fShowMoveInTitle intValue] forKey:kMBCShowMoveInTitle];
	}
	
	if (numMoves && [fShowMoveInTitle intValue]) {
		NSNumber * moveNum = [NSNumber numberWithInt:(numMoves+1)/2];
		NSString * moveStr = [NSNumberFormatter localizedStringFromNumber:moveNum 
															  numberStyle:NSNumberFormatterDecimalStyle];
		move = 	[NSString stringWithFormat:NSLocalizedString(@"title_move_line_fmt", @"%@. %@%@"),
						  moveStr, numMoves&1 ? @"":@"... ",
						  [[fBoard lastMove] localizedText]];
	} else if (!numMoves) {
		move =	NSLocalizedString(@"new_msg", @"New Game");
	} else if (numMoves & 1) {
		move = 	NSLocalizedString(@"black_move_msg", @"Black to move");
	} else {
		move = 	NSLocalizedString(@"white_move_msg", @"White to move");
	}

	NSString * title =
		[NSString stringWithFormat:NSLocalizedString(@"game_title_fmt", @"%@ - %@   (%@)"), 
				  fWhiteName, fBlackName, move];
	if (fOutcome)
		title = [NSString stringWithFormat:NSLocalizedString(@"game_outcome_fmt", @"%@   %@"), title, fOutcome];
	[fMainWindow setTitle:title];
	unichar emdash = 0x2014;
	[fMatchup setStringValue:
				  [NSString stringWithFormat:@"%@\n%@\n%@", 
							fWhiteName, 
							[NSString stringWithCharacters:&emdash length:1],
							fBlackName]];
	fTitleNeedsUpdate = false;
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)proposedFrameSize
{
	[fMoveList setNeedsDisplay:YES];

	return proposedFrameSize;
}

- (NSString *)pgnHeader
{
	NSUserDefaults *defaults 	= [NSUserDefaults standardUserDefaults];
	NSString * 		wf;
	NSString *		wl;
	NSString *		bf;
	NSString * 		bl;
	[MBCGameInfo parseName:fWhiteName intoFirst:&wf last:&wl];
	[MBCGameInfo parseName:fBlackName intoFirst:&bf last:&bl];
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
	switch (fSideToPlay) {
	case kWhiteSide:
		white 	= humanw;
		black 	= engine;
		break;
	case kBlackSide:
		white 	= engine;
		black	= humanb;
		break;
	case kBothSides:
		white	= humanw;
		black	= humanb;
		
		break;
	case kNeitherSide:
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
					 [defaults stringForKey:kMBCGameEvent],
					 [defaults stringForKey:kMBCGameCity],
					 [defaults stringForKey:kMBCGameCountry],
					 fStartDate, white, black, fResult, fStartTime];
}

- (NSString *)pgnResult
{
	return fResult;
}

@end

@interface MBCBoardWindowController : NSWindowController
{
}

- (void)synchronizeWindowTitleWithDocumentName;

@end

@implementation MBCBoardWindowController

- (void)synchronizeWindowTitleWithDocumentName
{
}

@end

// Local Variables:
// mode:ObjC
// End:
