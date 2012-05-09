/*
	File:		MBCController.h
	Contains:	Managing the entire user interface
	Version:	1.0
	Copyright:	© 2002-2011 by Apple Computer, Inc., all rights reserved.
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
