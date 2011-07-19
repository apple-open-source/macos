/*
	File:		MBCGameInfo.h
	Contains:	Managing information about the current game
	Version:	1.0
	Copyright:	© 2003-2010 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCGameInfo.h,v $
		Revision 1.4  2010/10/07 23:07:02  neerache
		<rdar://problem/8352405> [Chess]: Ab-11A250: BIDI: RTL: Incorrect alignement for strings in cells in Came log
		
		Revision 1.3  2003/06/12 07:27:10  neerache
		Reorganize preferences window, Add simpler title style
		
		Revision 1.2  2003/05/27 03:13:57  neerache
		Rework game loading/saving code
		
		Revision 1.1  2003/05/24 20:29:25  neerache
		Add Game Info Window
		
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
