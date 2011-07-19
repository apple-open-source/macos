/*
	File:		MBCController.h
	Contains:	Managing the entire user interface
	Version:	1.0
	Copyright:	© 2002-2011 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCController.h,v $
		Revision 1.25  2011/03/12 23:43:53  neerache
		<rdar://problem/9079430> 11A390: Can't understand what Japanese voice (Kyoko Premium) says when pieces move at all.
		
		Revision 1.24  2010/04/24 01:57:10  neerache
		<rdar://problem/7641028> TAL: Chess doesn't reload my game
		
		Revision 1.23  2007/03/02 07:40:45  neerache
		Revise document handling & saving <rdar://problems/3776337&4186113>
		
		Revision 1.22  2007/03/01 23:51:26  neerache
		Offer option to speak human moves <rdar://problem/4038206>
		
		Revision 1.21  2004/08/16 07:49:23  neerache
		Support flexible voices, weaker levels, accessibility
		
		Revision 1.20  2003/07/07 08:46:52  neerache
		Localize Style Names
		
		Revision 1.19  2003/07/03 08:12:51  neerache
		Use sheets for saving (RADAR 3093283)
		
		Revision 1.18  2003/07/03 03:19:15  neerache
		Logarithmic time control, more UI tweaks
		
		Revision 1.17  2003/07/02 21:06:16  neerache
		Move about box into separate class/nib
		
		Revision 1.16  2003/06/30 05:16:30  neerache
		Transfer move execution to Controller
		
		Revision 1.15  2003/06/16 02:18:03  neerache
		Implement floating board
		
		Revision 1.14  2003/06/04 23:14:05  neerache
		Neater manipulation widget; remove obsolete graphics options
		
		Revision 1.13  2003/06/02 04:21:17  neerache
		Remove gameEnd:, fUseLight
		
		Revision 1.12  2003/05/27 03:13:57  neerache
		Rework game loading/saving code
		
		Revision 1.11  2003/05/24 20:29:25  neerache
		Add Game Info Window
		
		Revision 1.10  2003/04/24 23:22:02  neeri
		Implement persistent preferences, tweak UI
		
		Revision 1.9  2003/04/10 23:03:16  neeri
		Load positions
		
		Revision 1.8  2003/04/05 05:45:08  neeri
		Add PGN export
		
		Revision 1.7  2003/04/02 18:21:09  neeri
		Support saving games
		
		Revision 1.6  2003/03/28 01:31:07  neeri
		Support hints, last move
		
		Revision 1.5  2002/12/04 02:26:28  neeri
		Fix updating when style changes
		
		Revision 1.4  2002/10/15 22:49:40  neeri
		Add support for texture styles
		
		Revision 1.3  2002/10/08 22:56:23  neeri
		Engine logging, color preferences
		
		Revision 1.2  2002/09/12 17:55:18  neeri
		Introduce level controls
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
*/

#import <Cocoa/Cocoa.h>

#import "MBCBoard.h"

@class MBCBoard;
@class MBCBoardView;
@class MBCEngine;
@class MBCInteractivePlayer;
@class NSSpeechSynthesizer;

@interface MBCController : NSObject
{
	IBOutlet id		fMainWindow;
	IBOutlet id		fNewGamePane;
    IBOutlet id 	fGameVariant;
    IBOutlet id 	fPlayers;
	IBOutlet id		fPlayersSimple;
    IBOutlet id 	fSpeakMoves;
	IBOutlet id		fSpeakHumanMoves;
    IBOutlet id 	fListenForMoves;
	IBOutlet id		fSearchTime;
	IBOutlet id		fBoardStyle;
	IBOutlet id		fPieceStyle;
	IBOutlet id		fTakebackMenuItem;
	IBOutlet id		fLicense;
	IBOutlet id		fGameInfo;
	IBOutlet id		fGameInfoWindow;
	IBOutlet id 	fOpaqueView;
	IBOutlet id		fComputerVoice;
	IBOutlet id		fAlternateVoice;
#if HAS_FLOATING_BOARD
	IBOutlet id 	fFloatingView;
	IBOutlet id		fFloatingMenuItem;
#endif

	MBCBoard *				fBoard;
	MBCBoardView *			fView;
	MBCEngine *				fEngine;
	MBCInteractivePlayer *	fInteractive;
	NSMutableString *		fEngineBuffer;
	NSFileHandle *			fEngineLogFile;
	bool					fIsLogging;
	MBCVariant				fVariant;
	NSString *				fWhiteType;
	NSString *				fBlackType;
	NSDictionary *			fLastLoad;
	NSMutableDictionary * 	fStyleLocMap;
	NSSpeechSynthesizer *	fDefaultSynth;
	NSSpeechSynthesizer *	fAlternateSynth;
	NSDictionary *			fDefaultLocalization;
	NSDictionary *			fAlternateLocalization;
	NSDocument *			fDocument;
}

+ (MBCController *)controller;
- (id) init;
- (void) awakeFromNib;
- (IBAction) updateOptions:(id)sender;
- (IBAction) updateGraphicsOptions:(id)sender;
- (IBAction) updateSearchTime:(id)sender;
- (IBAction) updateStyles:(id)sender;
- (IBAction) newGame:(id)sender;
- (IBAction) startNewGame:(id)sender;
- (IBAction) cancelNewGame:(id)sender;
- (IBAction) profileDraw:(id)sender;
- (IBAction) takeback:(id)sender;
- (IBAction) toggleLogging:(id)sender;
- (IBAction) showHint:(id)sender;
- (IBAction) showLastMove:(id)sender;
- (IBAction) openGame:(id)sender;
- (IBAction) saveGame:(id)sender;
- (IBAction) saveGameAs:(id)sender;
- (IBAction) updateVoices:(id)sender;

#if HAS_FLOATING_BOARD
- (IBAction) toggleFloating:(id)sender;
#endif

- (void) startNewGame;

- (MBCBoard *)				board;
- (MBCBoardView *)			view;
- (MBCInteractivePlayer *)	interactive;
- (MBCEngine *)				engine;
- (NSWindow *)				gameInfoWindow;

- (void) logToEngine:(NSString *)text;
- (void) logFromEngine:(NSString *)text;

- (BOOL)	speakMoves;
- (BOOL)	speakHumanMoves;
- (BOOL)	listenForMoves;

- (BOOL) loadGame:(NSDictionary *)dict;
- (NSDictionary *) saveGameToDict;
- (BOOL) saveMovesTo:(NSString *)fileName;

- (NSString *) localizedStyleName:(NSString *)name;

- (NSSpeechSynthesizer *) defaultSynth;
- (NSSpeechSynthesizer *) alternateSynth;
- (NSDictionary *) defaultLocalization;
- (NSDictionary *) alternateLocalization;
- (void)loadVoiceMenu:(id)menu withSelectedVoice:(NSString *)voiceIdentifierToSelect;

- (void)setDocument:(NSDocument *)doc;

@end

@interface MBCDocumentController : NSDocumentController {
}

- (void) addDocument:(NSDocument *)doc;

@end

// Local Variables:
// mode:ObjC
// End:
