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


	$RCSfile: gnuglue.m,v $
	Chess
	
	Copyright (c) 2000-2001 Apple Computer. All rights reserved.
*/

#import <AppKit/AppKit.h>		// NSBeep

/* own interface */
#import "gnuglue.h"

/* types, constants & external variables */
#import "gnuchess.h"

/* Application */
#import "Chess.h"	// NSApp

/* UNIX/C functions */
#import <stdio.h>	// fopen, fclose, fprintf, fscanf
#import <signal.h>	// signal
#import <time.h>	// time
#import <math.h>	// floor
#import <libc.h>	// getuid, usleep
#import <pwd.h>		// getpwuid, passwd

#ifdef PROFILE
#import <libc.h>	// times, tms
static struct tms  tmbuf1, tmbuf2;
#endif

/* castles */
#define NO_CASTLE     0x0
#define LEFT_CASTLE   0x1
#define RIGHT_CASTLE  0x2

/* external functions (gnuchess.c) */
extern void gnuchess_main_init();
extern void NewGame();
extern void algbr();
extern int  VerifyMove();
extern int  castle();
extern int  distance();
extern void InitializeStats();
extern int  SqAtakd();
extern void SelectMoveStart();
extern int  SelectMoveEnd();
extern void SelectLoop();

/* static variables */
static struct MoveInfo  move_info;
static struct BookEntry	*book_entries;
static short		GameQueens[240];

/* private functions */

static NSString * computerMove()
{
    if( mvstr1[0] )
	return [NSString stringWithCString: (const char *) mvstr1];
    return nil;
}

/* (not used)
static NSString * opponentMove()
{
    if( mvstr2[0] )
	return [NSString stringWithCString: (const char *) mvstr2];
    return nil;
}
*/

static void convert_move( move, row1, col1, row2, col2, castle_flag )
NSString *move;
int *row1, *col1;
int *row2, *col2;
int *castle_flag;
{
    int type = no_piece;
    if( [move isEqual: @"o-o"] )
	*castle_flag = RIGHT_CASTLE;
    else if( [move isEqual: @"o-o-o"] )
	*castle_flag = LEFT_CASTLE;
    else {
	char *algbr = (char *) [move cString];
	if( algbr && *algbr ) {
	    *col1 = algbr[0] - 'a';
	    *row1 = algbr[1] - '0' - 1;
	    *col2 = algbr[2] - 'a';
	    *row2 = algbr[3] - '0' - 1;
	    *castle_flag = NO_CASTLE;
	    type = [NSApp pieceTypeAt: *row1 : *col1];
	}
    }
    if( type ) {
#ifdef NeXT_DEBUG
	NSLog( @"Piece type: %d", type );
#endif
	if( type == king ) {
	    if( *col1 == 4 && *col2 == 6 )
		*castle_flag = RIGHT_CASTLE;
	    else if( *col1 == 4 && *col2 == 2 )
		*castle_flag = LEFT_CASTLE;
	}
    }
    return;
}

static const char * sPieceChars[] = {
    "",
    "",
    "N",
    "B",
    "R",
    "Q",
    "K"
};

static NSString * _convert_rowcol( row1, col1, row2, col2, type )
int row1, col1;
int row2, col2;
int type;
{
    char algbr [6];
    if( type == king && col1 == 4 ) {
	if( col2 == 6 )
	    return @"o-o";
	if( col2 == 2 )
	    return @"o-o-o";
    }
    sprintf(algbr, "%s%c%c%c%c", sPieceChars[type], 
	    'a' + col1, '0' + row1 + 1, 
	    'a' + col2, '0' + row2 + 1);

    return [NSString stringWithCString: (const char *) algbr];
}

static unsigned short convert_string( string, side )
NSString *string;
int  side;
{
    unsigned short move = 0;
    if( [string isEqual: @"o-o-o"] )
	move = ( side == black ) ? 0x3C3A : 0x0402;
    else if( [string isEqual: @"o-o"] )
	move = ( side == black ) ? 0x3C3E : 0x0406;
    else {
	char *algbr = (char *)[string cString];
	if( algbr && *algbr ) {
	    int r1, r2, c1, c2;
	    c1 = algbr[0] - 'a';
	    r1 = algbr[1] - '1';
	    c2 = algbr[2] - 'a';
	    r2 = algbr[3] - '1';
	    move = (unsigned short)((locn[r1][c1] << 8) + locn[r2][c2]);
	}
    }
    return move;
}

/* (not used)
static NSZone * create_zone()
{
    unsigned pageSize    = NSPageSize();
    unsigned granularity = pageSize;
    BOOL     canFree     = NO;		// no dynamic free
    NSZone *zone = NSCreateZone( pageSize, granularity, canFree );
    if( zone )
	return zone;

    NSLog( @"Zone cannot be created: size = %u", pageSize );
    return (NSZone *) NULL;
}
*/

static struct BookEntry * create_book_entry ( moves )
NSArray *moves;
{
    unsigned moveCount = [moves count];
    if( moveCount ) {
	struct BookEntry *entry;
	unsigned short *movep;
	unsigned entrySize = sizeof(struct BookEntry);
	unsigned moveSize  = (moveCount + 1) * sizeof(unsigned short);
	NSZone *zone = NSDefaultMallocZone();	// ??
//	NSZone *zone = [self zone];
 
	entry = (struct BookEntry *)NSZoneMalloc( zone, entrySize );
	movep = (unsigned short *)  NSZoneMalloc( zone, moveSize );
	if( entry && movep ) {
	    unsigned index;
//	    entry->next = book_entries;
	    entry->mv = movep;
	    for( index = 0; index < moveCount; index++ )
		*(movep++) = [[moves objectAtIndex:index] unsignedShortValue];
	    *movep = 0;
	    return entry;
	}

	NSLog( @"Memory cannot be allocated: zone = %p", (void *)zone );
    }
    return (struct BookEntry *)NULL;
}

/*
   Functions invoked by gnuchess.c modules
*/

void OutputMove()
{
    int  row1, col1, row2, col2, castle_flag;
    NSString  *move = computerMove();

#ifdef NeXT_DEBUG
    NSLog( @"OutputMove: Computer move is %@, mask %#04x", move, root->flags );
#endif
    if( root->flags & draw )
	[NSApp setFinished: DRAW_GAME];
    [NSApp fillResponseMeter: player];

    if( ! move || [move isEqual: @""] ) {
#ifdef NeXT_DEBUG
	NSLog( @"NO COMPUTER MOVE" );
#endif
	return;
    }
    convert_move( move, &row1, &col1, &row2, &col2, &castle_flag );

    [NSApp movePieceFrom: row1 : col1 to: row2 : col2];
    switch (castle_flag ) {
    case NO_CASTLE:
	break;
    case LEFT_CASTLE:
	[NSApp movePieceFrom: row1 : 0 to: row2 : 3];
	break;
    case RIGHT_CASTLE:
	[NSApp movePieceFrom: row1 : 7 to: row2 : 5];
	break;
    }

#ifdef NeXT_DEBUG
    NSLog( @"White has %hd pieces", PieceCnt[white] );
    NSLog( @"Black has %hd pieces", PieceCnt[black] );
#endif
    return;
}

void SelectLevel()
{
#ifdef NeXT_DEBUG
    NSLog( @"SelectLevel" );
#endif
    return;
}

void UpdateClocks()
{
#ifdef NeXT_DEBUG
    NSLog( @"UpdateClocks" );
#endif
    return;
}

void ElapsedTime( int iop ) 
/* 
   Determine the time that has passed since the search was started. If 
   the elapsed time exceeds the target (ResponseTime+ExtraTime) then set 
   timeout to true which will terminate the search. 
*/
{
    et = time((long *)0) - time0;
    if( et < 0 )
	et = 0;
    ETnodes += 50;
    if( ! iop )
	[NSApp displayResponseMeter: player];

#ifdef NeXT_DEBUG
    NSLog( @"ResponseTime %ld, ExtraTime %ld, Sdepth %hd, iop %d, et %ld, et0 %ld", ResponseTime, ExtraTime, Sdepth, iop, et, et0 );
#endif

    if( et > et0 || iop == 1 ) {
	if( et > ResponseTime+ExtraTime && Sdepth > 1 )
	    timeout = true;
	et0 = et;
	if( iop == 1 ) {
	    time0 = time((long *)0);
	    et0 = 0;
	}

#ifdef PROFILE
	(void)times( &tmbuf2 );
	cputimer = 100 * (tmbuf2.tms_utime - tmbuf1.tms_utime) / HZ;
	if( cputimer > 0 )
	    evrate = (100 * NodeCnt) / (cputimer + 100 * ft);
	else
	    evrate = 0;
#endif

	ETnodes = NodeCnt + 50;
	UpdateClocks();
    }

    [NSApp peekAndGetLeftMouseDownEvent];
    return;
}

void SetTimeControl()
{
    short  moves;
    long   clock;
#ifdef NeXT_DEBUG
    NSLog( @"SetTimeControl" );
#endif

    if( TCflag ) {
	moves = TCmoves;
	clock = 60 * (long)TCminutes;
    }
    else {
	moves = 0;
	clock = 0;
	Level = 60 * (long)TCminutes;
    }
    TimeControl.moves[white] = TimeControl.moves[black] = moves;
    TimeControl.clock[white] = TimeControl.clock[black] = clock;

    et = 0;
    ElapsedTime(1);
    return;
}

void ShowResults( short score, unsigned short bstline[], char ch )
{
#ifdef NeXT_DEBUG
    NSLog( @"ShowResults:  score = %hd    %@", score, computerMove() );
#endif
    return;
}

void GameEnd( short score )
{
    int gameStatus = 0;

    NSBeep();
    if( root->flags & draw )
	gameStatus = DRAW_GAME;
    else if( score == 9998 ) {
	NSLog( @"score %hd, winner %hd", score, winner );
	gameStatus = ( winner == black ) ? BLACK_MATE : WHITE_MATE;
    }
    else if( score == -9999 ) {
	if( bothsides ) {
	    NSLog( @"score %hd, winner %hd", score, winner );
	    if( winner != -1 )
		gameStatus = ( winner == white ) ? WHITE_MATE : BLACK_MATE;
	    else
		gameStatus = WHITE_MATE;
	}
	else
	    gameStatus = OPPONENT_MATE;
    } 
    [NSApp setFinished: gameStatus];
    return;
}

void ClrScreen()
{
#ifdef NeXT_DEBUG
    NSLog( @"ClrScreen" );
#endif
    return;
}

void UpdateDisplay( int f, int t, int flag, int iscastle )
{
#ifdef NeXT_DEBUG
    NSLog( @"UpdateDisplay:  from %d, to %d, flag %#04x, iscastle %d, InChk %hd", f, t, flag, iscastle, InChk );
#endif
    [NSApp updateBoard];
    return;
}

void GetOpenings()

/*
   Read in the Opening Book file and parse the algebraic notation for a 
   move into an unsigned integer format indicating the from and to 
   square. Create a linked list of opening lines of play, with 
   entry->next pointing to the next line and entry->move pointing to a 
   chunk of memory containing the moves. More Opening lines of up to 256 
   half moves may be added to gnuchess.book. 
*/

{
    NSBundle *bundle;
    NSString *path;
    NSString *book;
    NSScanner *scanner;
    NSMutableArray *moveList;
    int  side;

    if( book_entries ) {
	Book = book_entries;
	return;
    }

    bundle = [NSBundle mainBundle];
    path = [bundle pathForResource: @"gnuchess" ofType: @"book"];
    book = [NSString stringWithContentsOfFile: path];
    scanner = [NSScanner scannerWithString: book];
    if( ! book || ! scanner )
	return;

    moveList = [NSMutableArray arrayWithCapacity: 0];
    side = white;
//  book_entries = NULL;

    while( ! [scanner isAtEnd] ) {
	NSString *textLine;
	if( ! [scanner scanUpToString: @"\n" intoString: &textLine] )
	    continue;

	if( [textLine isEqual: @"\n"] || [textLine hasPrefix: @"!"] ) {
	    struct BookEntry *entry = create_book_entry( moveList );
	    if( entry ) {
		entry->next = book_entries;
		book_entries = entry; 
		side = white;
		[moveList removeAllObjects];
	    }
	}
	else {
	    unsigned idx;
	    NSArray *substrings = [textLine componentsSeparatedByString: @" "];
	    for( idx = 0; idx < [substrings count]; idx++ ) {
		unsigned short move;
		NSString *string = (NSString *)[substrings objectAtIndex: idx];
		if( [string isEqual: @""] )
		    continue;
		move = convert_string( string, side );
		if( move ) {
		    NSNumber *number = [NSNumber numberWithUnsignedShort:move];
		    [moveList addObject: (id)number];
		}
		side = otherside[side];
	    }
	}
    }

    Book = book_entries;
    return;
}

void ShowDepth( char ch )
{
#ifdef NeXT_DEBUG
    NSLog( @"ShowDepth:  %hd%c   max %hd", Sdepth, ch, MaxSearchDepth );
#endif
    return;
}

void ShowCurrentMove( short pnt, short f, short t )
{
#ifdef NeXT_DEBUG
    algbr( f, t, false );
    NSLog( @"ShowCurrentMove:  (%2hd) %@", pnt, computerMove() );
#endif
    return;
}

void ShowSidetomove()
{
#ifdef NeXT_DEBUG
    NSString *colorStr = ( player == white ) ? @"WHITE" : @"BLACK";
    NSLog( @"ShowSidetomove:  %2d:  %@", (int)(1+(GameCnt+1)/2), colorStr );
#endif
    return;
}

static void show_message( NSString *str )
{
#ifdef NeXT_DEBUG
    NSLog( @"ShowMessage:  %@", str );
#endif
    [NSApp setTitleMessage: str];
    return;
}

void ShowMessage( const char *s )
{
    show_message( [NSString stringWithCString: s] );
    return;
}

static void ExitChess()
{
    [NSApp terminate: NSApp];
    exit(0);
}

static void Die()
{
    if( [NSApp canFinishGame] ) {
	signal( SIGINT,  SIG_IGN );
	signal( SIGQUIT, SIG_IGN );
	ExitChess();
    }
    return;
}

static void TerminateSearch()
{
    signal( SIGINT,  SIG_IGN );
    signal( SIGQUIT, SIG_IGN );
    timeout = true;
    signal( SIGINT,  Die );	/* Die() */
    signal( SIGQUIT, Die );
    return;
}

void SearchStartStuff( short side )
{
    signal( SIGINT,  TerminateSearch );	/* TerminateSearch() */
    signal( SIGQUIT, TerminateSearch );
    return;
}

/*
   Functions invoked by Chess.app modules
*/

void init_gnuchess ()
{
    gnuchess_main_init();
    return;
}

void new_game ()
{
    NewGame();
    return;
}

void in_check ()
{
    int incheck = -1;
    if( SqAtakd( PieceList[computer][0], opponent ) )
	incheck = computer;
    if( SqAtakd( PieceList[opponent][0], computer ) )
	incheck = opponent;
    if( incheck == black )
	show_message( @"Black is in check" );
    else if( incheck == white )
	show_message( @"White is in check" );
    return;
}

void get_game ( NSString *filename )
{
    NSFileManager  *fileMgr;
    const char  *path;
    FILE  *fd;
    int  sq;
    int  c;

    fileMgr = [NSFileManager defaultManager];
    if( ! [fileMgr fileExistsAtPath: filename] ) {
	NSLog( @"file `%@' does not exist.", filename );
	return;
    }
    if( ! [fileMgr isReadableFileAtPath: filename] ) {
	NSLog( @"file `%@' is not readable.", filename );
	return;
    }
    path = [filename cString];
    if( ! path || ! *path ) {
	NSLog( @"filename `%@' has empty CString.", filename );
	return;
    }
    fd = fopen( path, "r" );
    if( ! fd ) {
	NSLog( @"file `%s' cannot be opened.", path );
	return;
    }

    (void)fscanf( fd, "%hd%hd", &computer, &opponent );
    (void)fscanf( fd, "%hd",    &Game50 );
    (void)fscanf( fd, "%hd%hd", &castld[white], &castld[black] );
    (void)fscanf( fd, "%hd%hd", &kingmoved[white], &kingmoved[black] );
    (void)fscanf( fd, "%hd%hd", &TCflag, &OperatorTime );
    (void)fscanf( fd, "%ld",    &TimeControl.clock[white] );
    (void)fscanf( fd, "%ld",    &TimeControl.clock[black] );
    (void)fscanf( fd, "%hd",    &TimeControl.moves[white] );
    (void)fscanf( fd, "%hd",    &TimeControl.moves[black] );

    for( sq = 0; sq < 64; sq++ ) {
	unsigned short m;
	(void)fscanf( fd, "%hu" , &m );
	board[sq] = (short)( m >> 8 );
	color[sq] = (short)( m & 0xff );
	if( ! color[sq] ) 
	    color[sq] = neutral;
	else
	    color[sq]--;
    }
    [NSApp updateBoard];

    GameCnt = -1;
    c = '?';
    while( c != EOF ) {
	struct GameRec *game = &GameList[++GameCnt];
	c = fscanf( fd,"%hu%hd%hd%ld%hd%hd%hd",
			&game->gmove, &game->score, &game->depth, &game->nodes,
			&game->time,  &game->piece, &game->color );
	if( ! game->color )
	    game->color = neutral;
	else
	    (game->color)--;
    }

    GameCnt--;
    if( TimeControl.clock[white] > (long)0 )
	TCflag = true;
    computer--;
    opponent--;

    (void)fclose( fd );

    InitializeStats();
    UpdateDisplay( 0, 0, 1, 0 );
    Sdepth = 0;
    return;
}

int save_game ( NSString *filename )
{
    NSFileManager  *fileMgr;
    const char  *path;
    FILE  *fd;
    int  sq;
    int  i;

    fileMgr = [NSFileManager defaultManager];
    if( [fileMgr fileExistsAtPath: filename] ) {
	if( ! [fileMgr isWritableFileAtPath: filename] ) {
	    NSLog( @"file `%@' exists and is not writable.", filename );
	    return( 0 );
	}
    }
    path = [filename cString];
    if( ! path || ! *path ) {
	NSLog( @"filename `%@' has empty CString.", filename );
	return( 0 );
    }
    fd = fopen( path, "w" );
    if( ! fd ) {
	NSLog( @"file `%s' cannot be opened.", path );
	return( 0 );
    }

    (void)fprintf( fd, "%hd %hd ",  computer+1, opponent+1 );
    (void)fprintf( fd, "%hd\n",     Game50 );
    (void)fprintf( fd, "%hd %hd ",  castld[white], castld[black] );
    (void)fprintf( fd, "%hd %hd\n", kingmoved[white], kingmoved[black] );
    (void)fprintf( fd, "%hd %hd\n", TCflag, OperatorTime );
    (void)fprintf( fd, "%ld ",      TimeControl.clock[white] );
    (void)fprintf( fd, "%ld ",      TimeControl.clock[black] );
    (void)fprintf( fd, "%hd ",      TimeControl.moves[white] );
    (void)fprintf( fd, "%hd\n",     TimeControl.moves[black] );

    for( sq = 0; sq < 64; sq++ ) {
	unsigned short m = ( color[sq] == neutral ) ? 0 : color[sq] + 1;
	m += 256 * board[sq];
	(void)fprintf( fd, "%hu\n", m );
    }

    for( i = 0; i <= GameCnt; i++ ) {
	struct GameRec *game = &GameList[i];
	short clr = ( game->color == neutral ) ? 0 : game->color + 1;
	(void)fprintf( fd, "%hu %hd %hd %ld %hd %hd %hd\n",
			game->gmove, game->score, game->depth, game->nodes,
			game->time,  game->piece, clr );
    }

    (void)fclose( fd );
    return( 1 );
}

int list_game ( NSString *filename )
{
    NSFileManager  *fileMgr;
    const char  *path;
    FILE  *fd;
    int  i;

    fileMgr = [NSFileManager defaultManager];
    if( [fileMgr fileExistsAtPath: filename] ) {
	if( ! [fileMgr isWritableFileAtPath: filename] ) {
	    NSLog( @"file `%@' exists and is not writable.", filename );
	    return( 0 );
	}
    }
    path = [filename cString];
    if( ! path || ! *path ) {
	NSLog( @"filename `%@' has empty CString.", filename );
	return( 0 );
    }
    fd = fopen( path, "w" );
    if( ! fd ) {
	NSLog( @"file `%s' cannot be opened.", path );
	return( 0 );
    }

    (void)fprintf( fd, "\n" );
    (void)fprintf( fd, "       score  depth  nodes  time" );
    (void)fprintf( fd, "         " );
    (void)fprintf( fd, "       score  depth  nodes  time");
    (void)fprintf( fd, "\n" );

    for( i = 0; i <= GameCnt; i++ ) {
	struct GameRec *game = &GameList[i];
	short from = (short)( game->gmove >> 8 );
	short to   = (short)( game->gmove & 0xff );
	algbr( from, to, false );
	if( ! (i % 2) )
	    (void)fprintf( fd, "\n" );
	else
	    (void)fprintf( fd, "         " );
	(void)fprintf( fd, "%5s  %5hd     %2hd %6ld %5hd",
		mvstr1, game->score, game->depth, game->nodes, game->time );
    }
    (void)fprintf( fd, "\n" );
    (void)fprintf( fd, "\n" );

    (void)fclose( fd );
    return( 1 );
} 

void undo_move ()
/*
   Undo the most recent half-move.
*/
{
    struct GameRec *game = &GameList[GameCnt];
    short from = (short)( game->gmove >> 8 );
    short to   = (short)( game->gmove & 0xff );

    if( board[to] == king && distance(to, from) > 1 ) {
	(void)castle( game->color, from, to, (short)2 );
    }
    else {
	board[from] = board[to];
	color[from] = color[to];
	board[to]   = game->piece;
	color[to]   = game->color;
	if ( GameQueens[GameCnt] )
	    board[from] = GameQueens[GameCnt];
	if (board[from] == king)
	    --kingmoved[color[from]];
    }
    if( TCflag )
	++TimeControl.moves[color[from]];

    GameCnt--;
    mate = false;
    Sdepth = 0;

    if ( [NSApp finished] )
	[NSApp setFinished: 0];
    UpdateDisplay( 0, 0, 1, 0 );
    InitializeStats();
    return;
}

int give_hint ()
{
    if( hint ) {
	short from = (short)( hint >> 8 );
	short to   = (short)( hint & 0xff );
	algbr( from, to, false );
#ifdef NeXT_DEBUG
	NSLog( @"hint: %@", computerMove() );
#endif
	[NSApp highlightSquareAt: (int)(from / 8) : (int)(from % 8)];
	[NSApp highlightSquareAt: (int)(to   / 8) : (int)(to   % 8)];
    }
    return (int) hint;
}

NSString * convert_rowcol ( row1, col1, row2, col2, type )
int row1, col1;
int row2, col2;
int type;
{
    return _convert_rowcol( row1, col1, row2, col2, type );
}

BOOL verify_move( move )
NSString *move;
{
    unsigned short mv;
    char *str = (char *)[move cString];
    int verify = VerifyMove( str+(strlen(str)>4 && str[0] != 'o'), (short)0, &mv );
    return ( verify == true ) ? YES : NO;
}

void select_move_start ( side, iop )
int  side;
int  iop;
{
    move_info.side  = (short) side;
    move_info.alpha = (short) 0;
    move_info.beta  = (short) 0;
    move_info.iop   = (short) iop;
    move_info.rpt   = (short) 0;
    Sdepth = 0;
    SelectMoveStart( &move_info );
    return;
}

void select_move_end ()
{
    (void)SelectMoveEnd( &move_info );
    return;
}

BOOL select_loop_end ()
{
    return (BOOL)(timeout || Sdepth >= MaxSearchDepth);
}

void select_loop ()
{
    SelectLoop( &move_info );
    return;
}

void run_computer_game ()
{
    quit = false;
    while( ! mate && ! quit && ! [NSApp finished] )
	[NSApp selectMove: player iop: 1];
    quit = true;
    timeout = true;
    return;
}

void stop_computer_game ()
{
    quit = true;
    timeout = true;
    return;
}

void select_computer_move ()
{
    Sdepth = 0;
    ft = 0;
    if( ! quit && ! mate && ! force )
	[NSApp selectMove: computer iop: 1];
    return;
}

int current_player ()
{
    return (int)player;
}

int game_count ()
{
    return (int)GameCnt;
}

int move_time ()
{
    return (int)GameList[GameCnt].time;
}

int response_time ()
{
    return (int)( ResponseTime + ExtraTime );
}

void reset_response_time ()
{
    ResponseTime = 0;
    ExtraTime = 0;
    return;
}

int elapsed_time ()
{
    return (int)et;
}

short *default_pieces ()
{
    return Stboard;
}

short *default_colors ()
{
    return Stcolor;
}

short *current_pieces ()
{
    return board;
}

short *current_colors ()
{
    return color;
}

int  game_level ()
{
    return (int) Level;
}

void set_game_level ( level )
int  level;
{
    Level = (long) level;
    return;
}

void interpret_level ( level, moves, minutes )
int  level;
int  *moves;
int  *minutes;
{
    switch( level ) {
	case 1  : *moves = 60; *minutes = 5;   break;
	case 2  : *moves = 60; *minutes = 15;  break;
	case 3  : *moves = 60; *minutes = 30;  break;
	case 4  : *moves = 40; *minutes = 30;  break;
	case 5  : *moves = 40; *minutes = 60;  break;
	case 6  : *moves = 40; *minutes = 120; break;
	case 7  : *moves = 40; *minutes = 240; break;
	case 8  : *moves = 1;  *minutes = 15;  break;
	case 9  : *moves = 1;  *minutes = 60;  break;
	case 10 : *moves = 1;  *minutes = 600; break;
	default : *moves = 0;  *minutes = 0;   break;
    }
    return;
}

void set_preferences ( prefs )
struct Preferences *prefs;
{
    if( prefs ) {
	TCmoves   = (short)prefs->time_cntl_moves;
	TCminutes = (short)prefs->time_cntl_minutes;
	TCflag    = (short)( TCmoves > 1 );
	SetTimeControl();

	bothsides = (short)prefs->bothsides;
	opponent  = (short)prefs->opponent;
	computer  = (short)prefs->computer;
    }
    return;
}

void set_timeout( flag )
BOOL  flag;
{
    timeout = (short)flag;
    return;
}

void set_game_queen( piece )
int  piece;
{
    GameQueens[GameCnt] = (short)piece;
    return;
}

NSString *copyright_text ()
{
    NSBundle *bundle = [NSBundle mainBundle];
    NSString *path   = [bundle pathForResource: @"COPYING" ofType: nil];
    if( path ) {
	NSString *string = [NSString stringWithContentsOfFile: path];
	if( string )
	    return string;
    }
    return nil;
}

void sleep_microsecs ( microsecs )
unsigned microsecs;
{
    (void) usleep( microsecs );
    return;
}

int  floor_value ( value )
double  value;
{
    return (int) floor( value );
}

// Local Variables:
// tab-width: 8
// End:
