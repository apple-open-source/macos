#import <AppKit/NSApplication.h>
#import <Foundation/NSUserDefaults.h>

@class NSString;
@class NSColor;

// Preferences structure
struct Preferences {
    int   time_cntl_moves;
    int   time_cntl_minutes;
    int   opponent;
    int   computer;
	BOOL  useSR;
    BOOL  bothsides;
    BOOL  cheat;
    NSString  *white_name;
    NSString  *black_name;
};

// game status
#define WHITE_MATE	1
#define BLACK_MATE	2
#define OPPONENT_MATE	3
#define DRAW_GAME	4

//
// PS routines without prototypes
//
void PScountframebuffers(int *count);
void PSmoveto(float x, float y);
void PSrmoveto(float x, float y);
void PSarc(float x, float y, float r, float angle1, float angle2);
void PSarcn(float x, float y, float r, float angle1, float angle2);
void PSarct(float x1, float y1, float x2, float y2, float r);
void PSflushgraphics(void);
void PSrectclip(float x, float y, float w, float h);
void PSrectfill(float x, float y, float w, float h);
void PSrectstroke(float x, float y, float w, float h);
void PSfill(void);
void PSeofill(void);
void PSstroke(void);
void PSstrokepath(void);
void PSinitclip(void);
void PSclip(void);
void PSeoclip(void);
void PSclippath(void);
void PSlineto(float x, float y);
void PSrlineto(float x, float y);
void PScurveto(float x1, float y1, float x2, float y2, float x3, float y3);
void PSrcurveto(float x1, float y1, float x2, float y2, float x3, float y3);
void PScurrentpoint(float *x, float *y);
void PSsetlinecap(int linecap);
void PSsetlinejoin(int linejoin);
void PSsetlinewidth(float width);
void PSsetgray(float gray);
void PSsetrgbcolor(float r, float g, float b);
void PSsetcmykcolor(float c, float m, float y, float k);
void PSsetalpha(float a);
void PStranslate(float x, float y);
void PSrotate(float angle);
void PSscale(float x, float y);
void PSconcat(const float m[]);
void PSsethalftonephase(int x, int y);
void PSnewpath(void);
void PSclosepath(void);
void PScomposite(float x, float y, float w, float h, int gstateNum, float dx, float dy, int op);
void PScompositerect(float x, float y, float w, float h, int op);
void PSshow(const char *s);
void PSashow(float w, float h, const char *s);

// Chess class interfaces

@interface Chess : NSApplication
{
// nib components
    id  boardWindow;
    id  clockPanel;
    id  prefPanel;
    id  infoPanel;
	id	menu2D;
	id  menu3D;

// BoardWindow
    id  board2D;
    id  board3D;

// ClockPanel
    id  whiteSample;
    id  whiteClockText;
    id  whiteClock;		// no connection
    id  whiteColorWell;
    id  whiteMeter;
    id  blackSample;
    id  blackClockText;
    id  blackClock;		// no connection
    id  blackColorWell;
    id  blackMeter;
    id  colorSetButton;
    id  startButton;
    id  forceButton;

// PrefPanel
    id  levelSlider;
    id  levelText;
    id  prefSetButton;
	id  gamePopup;
    id  whiteSideName;
    id  blackSideName;
	id  srCheckBox;

// InfoPanel
    id  infoScroll;

// game board
    id   gameBoard;		// board2D or board3D
    int  currentRow;
    int  currentCol;

// opened/saved file
    NSString  *filename;

// game status
    int  finished;
    int  undoCount;
    int  hintCount;
    int  forceCount;
    BOOL  dirtyGame;
    BOOL menusEnabled;

// preferences
    struct Preferences  prefs;
	NSUserDefaults *	defaults;

// player colors
    NSColor  *white_color;
    NSColor  *black_color;

// moving time
    int  whiteTime;
    int  blackTime;

    
}

// MainMenu responders
- (void)info: (id)sender;
- (void)newGame: (id)sender;
- (void)openGame: (id)sender;
- (void)listGame: (id)sender;
- (void)saveGame: (id)sender;
- (void)saveAsGame: (id)sender;
- (void)hint: (id)sender;
- (void)showPosition: (id)sender;
- (void)undoMove: (id)sender;
- (void)view2D: (id)sender;
- (void)view3D: (id)sender;
- (void)print: (id)sender;
- (BOOL)alertPanelForGameChange;

// ClockPanel responders
- (void)setWhiteColor: (id)sender;
- (void)setBlackColor: (id)sender;
- (void)renderColors: (id)sender;
- (void)startGame: (id)sender;
- (void)forceMove: (id)sender;

// PrefPanel responders
- (void)levelSliding: (id)sender;
- (void)chooseSide: (id)sender;
- (void)setPreferences: (id)sender;

// invoked by Board.m & Board3D.m
- (BOOL)bothsides;
- (int)finished;
- (void)finishedAlert;
- (BOOL)makeMoveFrom: (int)row1 : (int)col1 to: (int)row2 : (int)col2;

// invoked by gnuglue.m
- (void)peekAndGetLeftMouseDownEvent;
- (void)selectMove: (int)side iop: (int)iop;
- (void)setFinished: (int)flag;
- (void)movePieceFrom: (int)row1 : (int)col1 to: (int)row2 : (int)col2;
- (void)updateBoard;
- (int)pieceTypeAt: (int)row : (int)col;
- (void)highlightSquareAt: (int)row : (int)col;
- (void)displayResponseMeter: (int)side;
- (void)fillResponseMeter: (int)side;
- (void)setTitleMessage: (NSString *)msg;
- (BOOL)canFinishGame;

// support methods
- (void)setTitle;
- (void)storePosition: (int) row : (int) col;

- (void)setMainMenuEnabled: (BOOL)flag;

- (void)enablePrefPanel;
- (void)disablePrefPanel;

- (void)enableClockPanel;
- (void)disableClockPanel;

- (int)whiteTime;
- (int)blackTime;
- (void)updateClocks: (int)side;

@end
