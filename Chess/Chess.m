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


	$RCSfile: Chess.m,v $
	Chess
	
	Copyright (c) 2000-2001 Apple Computer. All rights reserved.
*/

#import <AppKit/AppKit.h>

// own interface
#import "Chess.h"

// components
#import "Board.h"
#import "Board3D.h"
#import "Clock.h"	// not used
#import "ResponseMeter.h"
#import "ChessListener.h"

// portability layer
#import "gnuglue.h"

#ifdef CHESS_DEBUG
static int          sCDInit;
static const char * sCD;
#define chess_debug(x) if (sCD || (!sCDInit++ && (sCD = getenv("CHESS_DEBUG")))) NSLog x; else 0 
#else
#define chess_debug(x)
#endif

#define NEW @"newGame:"
#define OPEN @"openGame:"
#define SAVE @"saveGame:"
#define SAVEAS @"saveAsGame:"
#define LIST @"listGame:"

void PScompositerect(float x, float y, float w, float h, int op);

void CL_MakeMove(const char * move)
{
	if (move[0] == (char)-1) {
		[NSApp undoMove:NSApp];
	} else {
		int r = move[1]-'1';
		int c = move[0]-'a';
		int r2= move[3]-'1';
		int c2= move[2]-'a';

		[NSApp makeMoveFrom: r : c to: r2 : c2];
	}
}

void CL_ScheduleInit()
{
	[NSThread 
		detachNewThreadSelector:@selector(initListener:) toTarget:NSApp withObject:[NSRunLoop currentRunLoop]];
}

// Chess class implementations

@implementation Chess

+ (void)initialize
{
    init_gnuchess();

    return;
}

- (void)finishLaunching
{
	float red, green, blue, alpha;
	NSScanner * scanner;

    [super finishLaunching];

	defaults = [NSUserDefaults standardUserDefaults];

	[defaults registerDefaults: 
				  [NSDictionary dictionaryWithObjectsAndKeys:
									@"0 0 0 1", @"WhiteColor",
								@"0 0 0 1", @"BlackColor",
								[levelSlider objectValue], @"Level",
								@"NO", @"BothSides", 
								@"YES", @"PlayerHasWhite",
								@"YES", @"SpeechRecognition",
								NULL]];

    gameBoard = board3D;
    [board2D retain];
    [board2D removeFromSuperview];

	scanner = [NSScanner scannerWithString:[defaults objectForKey:@"WhiteColor"]];
	[scanner scanFloat:&red];
	[scanner scanFloat:&green];
	[scanner scanFloat:&blue];
	[scanner scanFloat:&alpha];
    white_color = [[NSColor colorWithCalibratedRed:red green:green blue:blue alpha:1.0] retain];
	[whiteColorWell setColor:white_color];

	scanner = [NSScanner scannerWithString:[defaults objectForKey:@"BlackColor"]];
	[scanner scanFloat:&red];
	[scanner scanFloat:&green];
	[scanner scanFloat:&blue];
	[scanner scanFloat:&alpha];
    black_color = [[NSColor colorWithCalibratedRed:red green:green blue:blue alpha:1.0] retain];
	[blackColorWell setColor:black_color];

	[levelSlider setIntValue:[defaults integerForKey:@"Level"]];
	
	if ([defaults boolForKey:@"BothSides"])
		[gamePopup selectItemAtIndex:2];	
	else if ([defaults boolForKey:@"PlayerHasWhite"])
		[gamePopup selectItemAtIndex:0];
	else
		[gamePopup selectItemAtIndex:1];

	prefs.useSR = [defaults boolForKey:@"SpeechRecognition"];
	[srCheckBox setState:prefs.useSR];

	[self setWhiteColor: self];
	[self setBlackColor: self];
    [self renderColors: self];		// default color pieces

	[self chooseSide: whiteSideName];
	[self levelSliding: levelSlider];
    [self setPreferences: self];	// default preferences

    dirtyGame = NO;
    menusEnabled = YES;
	
	{
		NSString *	path	= 
			[[NSBundle mainBundle] pathForResource: @"SpeechHelp" ofType: @"xml"];
		NSData *	help 	= 
			[NSData dataWithContentsOfFile:path];
		CL_SetHelp([help length], [help bytes]);
	}

    [self newGame: self];

    return;
}

/*
    MainMenu responders
*/

- (void)info: (id)sender
{
    NSShowSystemInfoPanel ([NSDictionary dictionary]);
}

- (void)showGnuDisclaimer: (id)sender
{
    id  text = [infoScroll documentView];
    NSString  *string = [text string];
    if( ! string || [string isEqual: @""] ) {
        string = copyright_text();
        [text setString: string];
        [text sizeToFit];
		//	[text setFont:[NSFont fontWithName: @"Times" size: (float)14.0]];
        [infoScroll display];
    }
    [infoPanel makeKeyAndOrderFront: sender];
    return;
}
/* Start a new game */

- (void)newGame: (id)sender
{
    if (dirtyGame) {
        if (![self alertPanelForGameChange]) return;
    }
    
    finished = 0;
    undoCount = hintCount = forceCount = 0;

    new_game();

    [self setTitle];
    [self displayResponseMeter: WHITE];
    [self displayResponseMeter: BLACK];

    if( prefs.bothsides ) {
		[startButton setEnabled: YES];
		CL_ShutDown();
    } else {
		[startButton setEnabled: NO];
		if( prefs.computer == WHITE )
			[self selectMove: WHITE iop: 1];
        else if (prefs.useSR)
            CL_Listen(WHITE, current_pieces(), current_colors());
		if (!prefs.useSR)
			CL_ShutDown();
    }
    dirtyGame = NO;
    if (![boardWindow isKeyWindow]) [boardWindow makeKeyAndOrderFront:self];
    return;
}

/* Read a saved game */

- (void)openGame: (id)sender
{
    id  op = [NSOpenPanel openPanel];
    finished = 0;
    [op setRequiredFileType: @"chess"];
    if( [op runModal] == NSOKButton ) {
		[filename release];
		filename = [[op filename] retain];
		get_game( filename );
    }
    dirtyGame = NO;
    if (![boardWindow isVisible]) [boardWindow makeKeyAndOrderFront:self];
    return;
}
  
/* Save a game */

- (void)listGame: (id)sender
{
    NSSavePanel	*sp = [NSSavePanel savePanel];
    [sp setRequiredFileType: nil];
    if( [sp runModal] == NSOKButton )
		list_game( [sp filename] );
    return;
}

- (void)saveGame: (id)sender
{
    if ( filename && ! [filename isEqual: @""] )  
		save_game( filename );
    else
		[self saveAsGame: sender];
    dirtyGame = NO;
    return;
}

- (void)saveAsGame: (id)sender
{
    NSSavePanel	*sp = [NSSavePanel savePanel];
    [sp setRequiredFileType: @"chess"];
    if( [sp runModal] == NSOKButton ) {
		[filename release];
		filename = [[sp filename] retain];
		save_game( filename );
    }
    dirtyGame = NO;
    return;
}

// Added this because it seems a little more Mac like.  If you try to start
// a new game or open a game while a game is in progress it asks you if you
// would like to save the current game first.
- (BOOL)alertPanelForGameChange
{
    int button;
    button = NSRunAlertPanel( nil,
							  NSLocalizedString(@"Would you like to save your current game first?",nil),
							  NSLocalizedString(@"Save",nil), NSLocalizedString(@"No",nil),
                              NSLocalizedString(@"Cancel",nil), nil );

    switch( button ){
	case -1:  return NO;
	case 0:  return YES;
	default:  [self saveGame:self];  return YES;
    }
}
- (void)closeGame: (id)sender
{
    if (dirtyGame) {
        if (![self alertPanelForGameChange]) return;
    }
    dirtyGame = NO;
    [boardWindow orderOut:self];
}
/* Give the player a hint */

- (void)hint: (id)sender
{
    if( give_hint() ){
		hintCount++;
		[self setTitle];
    }
    else
		(void)NSRunAlertPanel( nil, NSLocalizedString(@"no_hint",nil), nil, nil, nil ); 
    return;
}

- (void)showPosition: (id)sender
{
    [gameBoard highlightSquareAt: currentRow : currentCol];
    [gameBoard flashSquareAt: currentRow : currentCol]; 
    return;
}

/* Undo last two half moves */

- (void)undoMove: (id)sender
{
    if( game_count() >= 0 ){
		undo_move();
		undo_move();
		undoCount++;
		[self setTitle];
		if (prefs.useSR)
			CL_Listen(prefs.opponent, current_pieces(), current_colors());
		else
			CL_ShutDown();
    }
    else
		(void)NSRunAlertPanel( nil, NSLocalizedString(@"no_undo",nil), nil, nil, nil ); 
    return;
}

- (void)view2D: (id)sender
{
	[menu2D setState: NSOnState];
	[menu3D setState: NSOffState];
    if( gameBoard == board3D ) {
		id      v = [gameBoard superview];
		//	NSRect  b = [v bounds];
		short  *pieces = current_pieces();
		short  *colors = current_colors();

		[gameBoard retain];
		[gameBoard removeFromSuperview];

		//	[v lockFocus];
		//	PSgsave();
		//	PSsetgray( NSBlack );
		//	PSrectfill( (float)0.0, (float)0.0, b.size.width, b.size.height );
		//	PSgrestore();
		//	[v unlockFocus];

		[v addSubview: board2D];
		[board2D release];
		gameBoard = board2D;
		[gameBoard layoutBoard: pieces color: colors];

		[self disableClockPanel];
    }
    return;
}

- (void)view3D: (id)sender
{
	[menu2D setState: NSOffState];
	[menu3D setState: NSOnState];
    if( gameBoard == board2D ) {
		id  v = [gameBoard superview];
		short  *pieces = current_pieces();
		short  *colors = current_colors();

		[gameBoard retain];
		[gameBoard removeFromSuperview];

		[v addSubview: board3D];
		[board3D release];
		gameBoard = board3D;
		[gameBoard layoutBoard: pieces color: colors];

		[self enableClockPanel];
    }
    return;
}

- (void)print: (id)sender
{
    [gameBoard print: sender];
    return;
}

/*
    ClockPanel responders
*/

- (void)setWhiteColor: (id)sender
{
    NSString	*path;
    NSImage	*image1;
    NSImage	*image2;
    NSRect  r;
    NSPoint pt;

    path   = [[NSBundle mainBundle] pathForImageResource: @"3d_white_sample"];
    image1 = [[NSImage alloc] initWithContentsOfFile: path];
    image2 = [whiteSample image];
    r.origin = NSZeroPoint;
    r.size   = [image2 size];
    pt = NSZeroPoint;

    [image2 lockFocus];
    [image1 compositeToPoint: pt fromRect: r operation: NSCompositeCopy];
    [image2 unlockFocus];

    [image1 lockFocus];
    [[whiteColorWell color] set];
    PScompositerect( pt.x, pt.y,
					 r.size.width, r.size.height, NSCompositePlusDarker );
    [image1 unlockFocus];

    [image2 lockFocus];
    [image1 compositeToPoint: pt fromRect: r operation: NSCompositeSourceAtop];
    [image2 unlockFocus];

    [whiteSample display];
    [image1 release];

    if( ! [white_color isEqual: [whiteColorWell color]] )
		[colorSetButton setEnabled: YES];
    return;
}

- (void)setBlackColor: (id)sender
{
    NSString	*path;
    NSImage	*image1;
    NSImage	*image2;
    NSRect  r;
    NSPoint pt;

    path   = [[NSBundle mainBundle] pathForImageResource: @"3d_black_sample"];
    image1 = [[NSImage alloc] initWithContentsOfFile: path];
    image2 = [blackSample image];
    r.origin = NSZeroPoint;
    r.size   = [image2 size];
    pt = NSZeroPoint;

    [image2 lockFocus];
    [image1 compositeToPoint: pt fromRect: r operation: NSCompositeCopy];
    [image2 unlockFocus];

    [image1 lockFocus];
    [[blackColorWell color] set];
    PScompositerect( pt.x, pt.y,
					 r.size.width, r.size.height, NSCompositePlusDarker );
    [image1 unlockFocus];

    [image2 lockFocus];
    [image1 compositeToPoint: pt fromRect: r operation: NSCompositeSourceAtop];
    [image2 unlockFocus];

    [blackSample display];
    [image1 release];

    if( ! [black_color isEqual: [blackColorWell color]] )
		[colorSetButton setEnabled: YES];
    return;
}

- (void)renderColors: (id)sender
{
    NSString  *path;
    NSImage   *image1;
    NSImage   *image2;
    NSRect  r;
    NSPoint pt;

    if( gameBoard != board3D )
        return;

    path   = [[NSBundle mainBundle] pathForImageResource: @"3d_pieces"];
    image1 = [[NSImage alloc] initWithContentsOfFile: path];
    image2 = [gameBoard piecesBitmap];
    r.origin = NSZeroPoint;
    r.size   = [image2 size];
    pt = NSZeroPoint;

    if( ! [white_color isEqual: [whiteColorWell color]] ) {
		float red, green, blue, alpha;		
		[white_color release];
		white_color = [[whiteColorWell color] retain];
		[white_color getRed:&red green:&green blue:&blue alpha:&alpha];
		[defaults setObject:[NSString stringWithFormat:@"%1.3f %1.3f %1.3f %1.3f", red, green, blue, alpha] forKey:@"WhiteColor"];
    }
    if( ! [black_color isEqual: [blackColorWell color]] ) {
		float red, green, blue, alpha;		
		[black_color release];
		black_color = [[blackColorWell color] retain];
		[black_color getRed:&red green:&green blue:&blue alpha:&alpha];
		[defaults setObject:[NSString stringWithFormat:@"%1.3f %1.3f %1.3f %1.3f", red, green, blue, alpha] forKey:@"BlackColor"];
    }

    [image2 lockFocus];
    [image1 compositeToPoint: pt fromRect: r operation: NSCompositeCopy];
    [image2 unlockFocus];

    [image1 lockFocus];
    [white_color set];
    PScompositerect( pt.x, pt.y,
					 (float)336.0, r.size.height, NSCompositePlusDarker );
    [black_color set];
    PScompositerect( (float)336.0, pt.y,
					 (float)336.0, r.size.height, NSCompositePlusDarker );
    [image1 unlockFocus];

    [image2 lockFocus];
    [image1 compositeToPoint: pt fromRect: r operation: NSCompositeSourceAtop];
    [image2 unlockFocus];

    [gameBoard display];
    [image1 release];

    [colorSetButton setEnabled: NO];
    return;
}

- (void)startGame: (id)sender
{
    if( [sender state] == 1 ) {
		// This is the loop that makes the computer play.  It can be terminated
		// by several conditions.  The "Stop" button may be clicked, cmd-. may
		// be pressed, or the game may end.

		[self setMainMenuEnabled: NO];
		[self disablePrefPanel];
		[self disableClockPanel];
		run_computer_game();
		[sender setState: 0];
    }
    else {
		stop_computer_game();
		[self enableClockPanel];
		[self enablePrefPanel];
		[self setMainMenuEnabled: YES];
    }
    return;
}

- (void)forceMove: (id)sender
{
    set_timeout( YES );
    return;
}

/*
    PrefPanel responders
*/

/* Change the text displayed below the level slider to indicate
   what the level means */

- (void)levelSliding: (id)sender
{
    NSString  *format, *string;
    int  moves, minutes;
    int  level = [sender intValue];

    interpret_level( level, &moves, &minutes );

    if( moves > 1 )
		format = NSLocalizedString( @"%d moves in %d minutes", nil );
    else
		format = NSLocalizedString( @"%d move in %d minutes", nil );
    string = [NSString stringWithFormat: format, moves, minutes];
    [levelText setStringValue: string];

    if( level != game_level() )
		[prefSetButton setEnabled: YES];
    return;
}

/* Set the text fields next to the side matrices */

- (void)chooseSide: (id)sender
{
	switch ([gamePopup indexOfSelectedItem]) {
	case 0: /* Human vs. Computer */
		[whiteSideName setStringValue: NSFullUserName()];
		[blackSideName setStringValue: NSLocalizedString(@"Computer",nil)];
		if ( !( !prefs.bothsides && prefs.computer == BLACK))
			[prefSetButton setEnabled: YES];
		break;
	case 1: /* Computer vs. Human */
		[whiteSideName setStringValue: NSLocalizedString(@"Computer",nil)];
		[blackSideName setStringValue: NSFullUserName()];
		if ( !( !prefs.bothsides && prefs.computer == WHITE))
			[prefSetButton setEnabled: YES];
		break;
	case 2: /* Computer vs. Computer */
		[whiteSideName setStringValue: NSLocalizedString(@"Computer",nil)];
		[blackSideName setStringValue: NSLocalizedString(@"Computer",nil)];
		if ( !prefs.bothsides )
			[prefSetButton setEnabled: YES];
		break;
	}
	if (prefs.useSR != [srCheckBox state])
		[prefSetButton setEnabled: YES];		
	if( ! [prefs.white_name isEqual: [whiteSideName stringValue]]  )
		[prefSetButton setEnabled: YES];
	if( ! [prefs.black_name isEqual: [blackSideName stringValue]] )
		[prefSetButton setEnabled: YES];
    return;
}

/* TextField delegate */

- (void)controlTextDidBeginEditing: (NSNotification *)notification
{
    id  txField = [notification object];
    if( txField == whiteSideName || txField == blackSideName ) {
		[prefSetButton setEnabled: YES];
    }
    return;
}

/* Actually set the preferences */

- (void)setPreferences: (id)sender
{
    int  button;
    int  level = [levelSlider intValue];

    set_game_level ( level );
    interpret_level( level, &prefs.time_cntl_moves, &prefs.time_cntl_minutes );

	switch ([gamePopup indexOfSelectedItem]) {
	case 0:
		prefs.bothsides = NO;
		prefs.opponent  = WHITE;
		prefs.computer  = BLACK;
		break;
	case 1:
		prefs.bothsides = NO;
		prefs.opponent  = BLACK;
		prefs.computer  = WHITE;
		break;
	case 2:
		prefs.bothsides = YES;
		prefs.opponent  = WHITE;
		prefs.computer  = BLACK;
		break;
    }

	if (prefs.useSR && ![srCheckBox state])
		CL_DontListen();
	prefs.useSR = [srCheckBox state];
    prefs.cheat = YES;		// always YES?

    if( ! [prefs.white_name isEqual: [whiteSideName stringValue]] ) {
		[prefs.white_name release];
		prefs.white_name = [[whiteSideName stringValue] retain];
		[whiteClockText setStringValue: prefs.white_name];
    }
    if( ! [prefs.black_name isEqual: [blackSideName stringValue]] ) {
		[prefs.black_name release];
		prefs.black_name = [[blackSideName stringValue] retain];
		[blackClockText setStringValue: prefs.black_name];
    }

    set_preferences( &prefs );
    [prefSetButton setEnabled: NO];

    if( sender == self )	return;		// default setting

	[defaults setInteger:[levelSlider intValue] forKey:@"Level"];
	[defaults setBool:prefs.bothsides forKey:@"BothSides"];
	[defaults setBool:prefs.computer  forKey:@"PlayerHasWhite"];
	[defaults setBool:prefs.useSR forKey:@"SpeechRecognition"];
	
    reset_response_time();
    [self displayResponseMeter: WHITE];
    [self displayResponseMeter: BLACK];

    button = NSRunAlertPanel( nil, NSLocalizedString(@"new_game",nil), NSLocalizedString(@"Yes",nil), NSLocalizedString(@"No",nil), nil );
    if ( button == NSAlertDefaultReturn ) {
        dirtyGame = NO;
        [self newGame: self];
    } else {
		if( prefs.bothsides )
			[startButton setEnabled: YES];
		else {
			[startButton setEnabled: NO];
			if( current_player() == WHITE && prefs.computer == WHITE )
				[self selectMove: WHITE iop: 1];
			else if( current_player() == BLACK && prefs.computer == BLACK )
				[self selectMove: BLACK iop: 1];
		}
    }
    return;
}

/*
    invoked by Board.m & Board3D.m
*/

- (BOOL)bothsides
{
    return prefs.bothsides;
}

- (int)finished
{
    return finished;
}

- (void)finishedAlert
{
    NSString *msg;
    chess_debug(( @"finished %d", finished ));
    switch( finished ){
	case DRAW_GAME     :  msg = @"draw_game";  break;
	case WHITE_MATE    :  msg = @"black_win";  break;
	case BLACK_MATE    :  msg = @"white_win";  break;
	case OPPONENT_MATE :  msg = @"you_win";    break;
	default            :  return;
    }
    (void)NSRunAlertPanel( nil, NSLocalizedString(msg,nil), nil, nil, nil );
    return;
}

- (BOOL)makeMoveFrom: (int)row1 : (int)col1 to: (int)row2 : (int)col2
{ 
    NSString *move;
    short oldguy, newguy;
    BOOL verified;
    short  *pieces, *colors;

    dirtyGame = YES;
    oldguy = [gameBoard pieceAt: row1 : col1];
    move = convert_rowcol( row1, col1, row2, col2, oldguy );

    verified = verify_move( move );
    if( ! verified ) {
		[self setTitleMessage: @"Illegal move"];
		NSBeep();
		return( NO );
    }

    newguy = [gameBoard pieceAt: row2 : col2];
    if ( (newguy == QUEEN) && (oldguy == PAWN) ) {
		chess_debug (( @"pawn becomes queen..." ));
		set_game_queen( PAWN );
    }
    else
		set_game_queen( NO_PIECE );

    chess_debug(( @"<<< opponent move time %d move %@", move_time(), move ));
    [self updateClocks: prefs.opponent];
    in_check();
    PSWait();

    pieces = current_pieces();
    colors = current_colors();
    [gameBoard layoutBoard: pieces color: colors];
	//  [gameBoard display];
    PSWait();

    select_computer_move();
    return( YES );
}

/*
    invoked by gnuglue.m
*/

- (void)peekAndGetLeftMouseDownEvent
{
    NSEvent *event;
    if( event = [NSApp nextEventMatchingMask: NSLeftMouseDownMask untilDate: [NSDate date] inMode: NSEventTrackingRunLoopMode dequeue: NO] ) {
		event = [NSApp nextEventMatchingMask: NSLeftMouseDownMask untilDate: [NSDate date] inMode: NSEventTrackingRunLoopMode dequeue: YES];
		[NSApp sendEvent: event];
    }
    return;
}

- (void)selectMove: (int)side iop: (int)iop
{
    if( side == WHITE )
		[self setTitleMessage: @"White's move"];
    else
		[self setTitleMessage: @"Black's move"];

    [self setMainMenuEnabled: NO];
    [self disablePrefPanel];
    [self disableClockPanel];
    [gameBoard setEnabled: NO];
    [forceButton setEnabled: YES];
	if (prefs.useSR)
		CL_DontListen();
    PSWait();

    select_move_start( side, iop );
    while( ! select_loop_end() )
		select_loop();
    select_move_end();

    [self setTitle];

    [self updateClocks: prefs.computer];
    in_check();
    PSWait();

    [forceButton setEnabled: NO];
    [gameBoard setEnabled: YES];
    [self enableClockPanel];
    [self enablePrefPanel];
    [self setMainMenuEnabled: YES];

	if (prefs.useSR)
		CL_Listen(!side, current_pieces(), current_colors());
	else
		CL_ShutDown();

    return;
}

- (void)setFinished: (int)flag
{
    finished = flag;
    [self finishedAlert];
    return;
}

- (void)movePieceFrom: (int)row1 : (int)col1 to: (int)row2 : (int)col2
{
    NSString *move;
    short oldguy, newguy;
    short  *pieces = current_pieces();
    short  *colors = current_colors();

    oldguy = [gameBoard pieceAt: row1 : col1];
    move = convert_rowcol( row1, col1, row2, col2, oldguy );
    chess_debug( (@">>> computer move time %d move %@", move_time(), move) );

    dirtyGame = YES;
    [gameBoard highlightSquareAt: row1 : col1];
    [gameBoard slidePieceFrom: row1 : col1 to: row2 : col2];
    [self storePosition: row2 : col2];
    [gameBoard layoutBoard: pieces color: colors];
    PSWait();
    [gameBoard highlightSquareAt: row2 : col2];
    newguy = [gameBoard pieceAt: row2 : col2];
    if ( (newguy == QUEEN) && (oldguy == PAWN) ) {
		chess_debug (( @"computer pawn becomes queen..." ));
		set_game_queen( PAWN );
    }
    else
		set_game_queen( NO_PIECE );
    return;
}

- (void)updateBoard
{
    short  *pieces = current_pieces();
    short  *colors = current_colors();
    [gameBoard layoutBoard: pieces color: colors];
    [gameBoard display];
    return;
}

- (int)pieceTypeAt: (int)row : (int)col
{
    return [gameBoard pieceAt: row : col];
}

- (void)highlightSquareAt: (int)row : (int)col
{
    [gameBoard highlightSquareAt: row : col];
    return;
}

- (void)displayResponseMeter: (int)side
{
    if( [clockPanel isVisible] ) {
		if( side == WHITE )
			[whiteMeter display];
		else
			[blackMeter display];
		PSWait();
    }
    return;
}

- (void)fillResponseMeter: (int)side
{
    if( [clockPanel isVisible] ) {
		if( side == WHITE )
			[whiteMeter displayFilled];
		else
			[blackMeter displayFilled];
		PSWait();
    }
    return;
}

- (void)setTitleMessage: (NSString *)msg
{
    NSMutableString *buf;
    [self setTitle];
    buf = [NSMutableString stringWithCapacity: (unsigned)0];
    [buf appendString: [boardWindow title]];
    [buf appendString: NSLocalizedString(@"   :   ", nil)];
    [buf appendString: NSLocalizedString(msg, nil)];
    [boardWindow setTitle: buf];
    return;
}

- (BOOL)canFinishGame
{
    int sts = NSRunAlertPanel( nil, NSLocalizedString(@"exit_chess",nil), NSLocalizedString(@"Yes",nil), NSLocalizedString(@"No",nil), nil );
    return ( sts == NSAlertDefaultReturn ) ? YES : NO;
}

/*
    Support methods
*/

- (void)setTitle
	/*
  Change the board windows title to display the number of cheat commands
  issued.
*/
{
    NSMutableString *str = [NSMutableString stringWithCapacity: 0];
    if( undoCount || hintCount ) {
		[str appendString: NSLocalizedString(@"Chess:  ", nil)];
		if( undoCount == 1 )
			[str appendString: NSLocalizedString(@"1 Undo  ",  nil)];
		else if( undoCount )
			[str appendFormat: NSLocalizedString(@"%d Undos  ",nil),undoCount];
		if( hintCount == 1 )
			[str appendString: NSLocalizedString(@"1 Hint", nil)];
		else if( hintCount )
			[str appendFormat: NSLocalizedString(@"%d Hints",nil),hintCount];
    }
    else
		[str appendString: NSLocalizedString(@"Chess", nil)];
    [boardWindow setTitle: str];
    return;
}

- (void)storePosition: (int) row : (int) col
{
    currentRow = row;
    currentCol = col;
    return;
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    //[item action]
    if (!menusEnabled) {
        NSString *selector = NSStringFromSelector([item action]);
        if ([selector isEqual:NEW]) return NO;
        else if ([selector isEqual:OPEN]) return NO;
        else if ([selector isEqual:SAVE]) return NO;
        else if ([selector isEqual:SAVEAS]) return NO;
        else if ([selector isEqual:LIST]) return NO;
        else return YES;
    }
    return YES;
}
- (void)setMainMenuEnabled: (BOOL)flag
{
    menusEnabled = flag;
    return;
}

- (void)enablePrefPanel
{
    [levelSlider setEnabled: YES];
    [levelText setTextColor: [NSColor blackColor]];
    [gamePopup setEnabled: YES];
    [whiteSideName setEnabled: YES];
    [blackSideName setEnabled: YES];
    if( [levelSlider intValue] != game_level() ||
		! [prefs.white_name isEqual: [whiteSideName stringValue]] ||
		! [prefs.black_name isEqual: [blackSideName stringValue]] ) {
		[prefSetButton setEnabled: YES];
    }
    return;
}

- (void)disablePrefPanel
{
    [levelSlider setEnabled: NO];
    [levelText setTextColor: [NSColor darkGrayColor]];
    [gamePopup setEnabled: NO];
    [whiteSideName setEnabled: NO];
    [blackSideName setEnabled: NO];
    [prefSetButton setEnabled: NO];
    return;
}

- (void)enableClockPanel
{
    if( gameBoard == board3D ) {
		[whiteColorWell setEnabled: YES];
		[blackColorWell setEnabled: YES];
		if( ! [white_color isEqual: [whiteColorWell color]] ||
			! [black_color isEqual: [blackColorWell color]] ) {
			[colorSetButton setEnabled: YES];
		}
    }
    return;
}

- (void)disableClockPanel
{
    [whiteColorWell setEnabled: NO];
    [blackColorWell setEnabled: NO];
    [colorSetButton setEnabled: NO];
    return;
}

- (int)whiteTime
{
    return whiteTime;
}

- (int)blackTime
{
    return blackTime;
}

- (void)updateClocks: (int)side
{
    if( ! blackClock || ! whiteClock )
		return;
    if( side == WHITE ) {
		whiteTime += move_time();
		if( [clockPanel isVisible] ) {
			[whiteClock setSeconds: whiteTime];
			[whiteClock display];
		}
    }
    else {
		blackTime += move_time();
		if( [clockPanel isVisible] ) {
			[blackClock setSeconds: blackTime];
			[blackClock display];
		}
    }
    return;
}

/*
    Application delegate
*/

- (int)application: (NSApplication *)sender openFile: (NSString *)path withType: (NSString *)type
{
    chess_debug( (@"Open file: %@ type: %@", path, type) );
    if( type && [type isEqual: @"chess"] ) {
		[filename release];
		filename = [path retain];
		get_game( filename );
		return (int)YES;
    }
    return (int)NO;
}

NSTimer * sFinishTimer;

- (void)initListener: (NSRunLoop *)mainRunLoop
{
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	sFinishTimer = [[NSTimer timerWithTimeInterval:0.0 target:self 
							 selector:@selector(finishInitListener:) 
							 userInfo:nil repeats:FALSE] retain];
	CL_Init();
	[mainRunLoop addTimer:sFinishTimer forMode:NSDefaultRunLoopMode];
	
	[pool release];
}

- (void)finishInitListener: (NSRunLoop *)mainRunLoop
{
	CL_FinishInit();
	[sFinishTimer release];
}

@end
