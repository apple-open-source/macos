/*
 IMPORTANT: This Apple software is supplied to you by Apple Computer,
 Inc. ("Apple") in consideration of your agreement to the following terms,
 and your use, installation, modification or redistribution of this Apple
 software constitutes acceptance of these terms.  If you do not agree with
 these terms, please do not use, install, modify or redistribute this Apple
 software.
 
 In consideration of your agreement to abide by the following terms, and
 subject to these terms, Apple grants you a personal, non-exclusive
 license, under Apple’s copyrights in this original Apple software (the
 "Apple Software"), to use, reproduce, modify and redistribute the Apple
 Software, with or without modifications, in source and/or binary forms;
 provided that if you redistribute the Apple Software in its entirety and
 without modifications, you must retain this notice and the following text
 and disclaimers in all such redistributions of the Apple Software.
 Neither the name, trademarks, service marks or logos of Apple Computer,
 Inc. may be used to endorse or promote products derived from the Apple
 Software without specific prior written permission from Apple. Except as
 expressly stated in this notice, no other rights or licenses, express or
 implied, are granted by Apple herein, including but not limited to any
 patent rights that may be infringed by your derivative works or by other
 works in which the Apple Software may be incorporated.
 
 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES
 NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE
 IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION
 ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
 
 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
 MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND
 WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT
 LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY
 OF SUCH DAMAGE.


	$RCSfile: Chess.h,v $
	Chess
	
	Copyright (c) 2000-2001 Apple Computer. All rights reserved.
*/

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

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
- (void)initListener: (NSRunLoop *)mainRunLoop;
- (void)finishInitListener: (NSRunLoop *)mainRunLoop;

@end
