/*
	File:		MBCGameInfo.h
	Contains:	Managing information about the current game
	Version:	1.0
	Copyright:	© 2003-2010 by Apple Computer, Inc., all rights reserved.
*/

#import <Cocoa/Cocoa.h>

#import "MBCBoard.h"

@class MBCBoard;

@interface MBCGameInfo : NSObject
{
	IBOutlet id		fInfoWindow;
	IBOutlet id		fMainWindow;
	IBOutlet id		fShowMoveInTitle;
	IBOutlet id		fEditSheet;
	IBOutlet id		fMatchup;
	IBOutlet id		fWhite;
	IBOutlet id		fBlack;
	IBOutlet id		fCity;
	IBOutlet id		fCountry;
	IBOutlet id		fEvent;
	IBOutlet id		fMoveList;

	MBCBoard *		fBoard;
	MBCSide			fHuman;
	bool			fTitleNeedsUpdate;
	bool			fSetInfo;
	int				fRows;
	MBCSide			fSideToPlay;
	NSString *		fOutcome;
	NSString *		fWhiteName;
	NSString *		fBlackName;
	NSString * 		fStartDate;
	NSString * 		fStartTime;
	NSString * 		fResult;
}

- (NSDictionary *)getInfo;
- (void)setInfo:(NSDictionary *)dict;
- (void)startGame:(MBCVariant)variant playing:(MBCSide)sideToPlay;
- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;
- (IBAction) editInfo:(id)sender;
- (IBAction) cancelInfo:(id)sender;
- (IBAction) updateInfo:(id)sender;
- (IBAction) updateTitle:(id)sender;
+ (void) parseName:(NSString *)fullName intoFirst:(NSString **)firstName
			  last:(NSString **)lastName;
- (NSString *)pgnHeader;
- (NSString *)pgnResult;

@end

// Local Variables:
// mode:ObjC
// End:
