/* Glue software for gnuchess.c & Chess.app */

#import <Foundation/NSObjCRuntime.h>

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
