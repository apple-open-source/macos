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


	$RCSfile: gnuglue.h,v $
	Chess
	
	Copyright (c) 2000-2001 Apple Computer. All rights reserved.
*/

/* Glue software for gnuchess.c & Chess.app */

#import <Foundation/Foundation.h>

@class NSString;


/*
    constant definitions

	Note: These constants are the same as defined in gnuchess.h.
	This file is created for not importing gnuchess.h in main modules
	of Chess.app.
*/

/* players */
enum {
    WHITE = 0,
    BLACK,
    NEUTRAL
};

/* pieces */
enum {
    NO_PIECE = 0,
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING
};

/* board layout */
#define ROW_COUNT	8
#define COLUMN_COUNT	8
#define SQUARE_COUNT	(ROW_COUNT*COLUMN_COUNT)

/*
    function declarations
*/

/* invoked by gnuchess.c modules */
extern void OutputMove();
extern void SelectLevel();
extern void UpdateClocks();
extern void ElapsedTime( int ) ;
extern void SetTimeControl();
extern void ShowResults( short, unsigned short [], char );
extern void GameEnd( short );
extern void ClrScreen();
extern void UpdateDisplay( int, int, int, int );
extern void GetOpenings();
extern void ShowDepth( char );
extern void ShowCurrentMove( short, short, short );
extern void ShowSidetomove();
extern void ShowMessage( const char * );
extern void SearchStartStuff( short );

/* invoked by Chess.app modules */
extern void init_gnuchess();
extern void new_game();
extern void in_check();

extern void get_game ( NSString * );
extern int  save_game( NSString * );
extern int  list_game( NSString * );

extern void undo_move();
extern int  give_hint();

extern NSString *convert_rowcol( int, int, int, int, int );
extern BOOL verify_move( NSString * );

extern void select_move_start( int, int );
extern void select_move_end();
extern BOOL select_loop_end();
extern void select_loop();

extern void run_computer_game();
extern void stop_computer_game();
extern void select_computer_move();

extern int  current_player();
extern int  game_count();
extern int  move_time();
extern int  response_time();
extern void reset_response_time();
extern int  elapsed_time();
extern short *default_pieces();
extern short *default_colors();
extern short *current_pieces();
extern short *current_colors();

extern int  game_level();
extern void set_game_level( int );
extern void interpret_level( int, int *, int * );

extern void set_preferences();

extern void set_timeout( BOOL );
extern void set_game_queen( int );

extern NSString *copyright_text();
extern NSString *user_fullname();
extern void sleep_microsecs( unsigned );
extern int  floor_value( double );
