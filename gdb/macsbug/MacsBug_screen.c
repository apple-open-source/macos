/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                   MacsBug_screen.c                                   |
 |                                                                                      |
 |                      MacsBug physically screen display routines                      |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2001                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*

 This file contains all the stuff dealing with physically writing the MacsBug screen.
 This version is a "poor man's curses" implementation.  It keeps a screen image like
 curses does and updates only the lines that need updating when screen_refresh() is
 called (mimicking a curses refresh() call).  There are calls to screen_refresh() at
 the "proper" places in the MacsBug displayers to cause the screen refresh at the
 desired time.
 
 The implementation could fairly easily be replaced with curses but that could be
 overkill for what we need to do.  Basically we only handle the MacsBug history area.
 The side-bar and pc-areas are relatively small and have their own output optimizations.
 
 This file can also run without the curses simulation.  If screen_init() is not called
 (it's the counterpart to curses' initscr()) then we default to direct stdio output.
 It works, but the history screen updates are much slower.
 
 It should be noted that there may be potential problems using curses.  Our history
 area display code (the higher-level stuff that calls this low-level stuff) assumes its
 directly writing to an xterm terminal.  So it handles line clearing itself.  We don't
 need to do it here (BUT WE REQUIRE THAT LINE CLEARING BE DONE AT THE ENDS OF LINES
 and not at the start or it screws up the cursor positioning since they don't appear
 on the screen).  Further there can be color controls embedded in the lines (e.g.,
 red and normal color switching).  Curses doesn't support color and it's not clear at
 the time of this writing whether such characters embedded in the line confuse curses
 into wrapping lines prematurely.  Just thought I'd mention that.
 
 In the implementation here we just see if a row changes and write it if we do.  No
 further minimization of the writes is attempted like curses does.  Given that we know
 we're updating the history area, and it scrolls, it's probably as good as curses anyway
 with a lot smaller implementation!  The only thing that curses may be better at is
 the buffering (and then again, maybe not).  We don't do a setvbuf() because we can't
 do it before gdb does any prints.  And a setvbuf doesn't (or it shouldn't have any
 affect after any action is performed on the stream -- sigh :-( ).
*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "MacsBug.h"

/*--------------------------------------------------------------------------------------*/

static unsigned char *screen  = NULL;		/* ptr to our screen image		*/
static int           *row_len = NULL;		/* max col used on each row		*/

static unsigned char *prev_screen  = NULL;	/* screen appearence at last write	*/
static int           *prev_row_len = NULL;	/* max col used on each row		*/

static int           max_y;			/* max number of screen rows		*/
static int           max_x;			/* max nbr of cols (+ enough for ctls)	*/

static int	     origin_y;			/* origin of window within entire screen*/
static int	     origin_x;			/* (these are 1-relative)		*/

static int           cursor_y;			/* current cursor position		*/
static int           cursor_x;			/* (these are 1-relative)		*/

static int           saved_cursor_y; 		/* saved cursor position		*/
static int           saved_cursor_x;

#define SCREEN_ADDR(y, x) (screen + ((y)-1)*max_x + ((x) - 1))
#define SCREEN(y, x)      (*SCREEN_ADDR(y, x))

static int stdout_fd = -1;			/* a dup file descr STDOUT_FILENO	*/
static FILE *screen_stdout = NULL;		/* private stdout with a large buffer	*/

/*--------------------------------------------------------------------------------------*/

/*----------------------------------------------------*
 | write_char - write a character to the screen image |
 *----------------------------------------------------*
 
 A character is written at the cursor position (cursor_y, cursor_x) to the screen buffer
 which represents the physical screen.  The cursor is advanced by one.  We don't need to
 care about wrapping since the upper-level history area output code does that.  We
 allocated a virtual screen that is wide enough to hold any combination of actual and
 xterm control sequences thrown at it.
*/

static void write_char(c)
{
    if (cursor_x > row_len[cursor_y-1])
	row_len[cursor_y-1] = cursor_x;

    SCREEN(cursor_y, cursor_x++) = c;
}
    

/*---------------------------------------------------------*
 | screen_refresh - write the screen image to the terminal |
 *---------------------------------------------------------*
 
 All changes to the screen are accumulated in the screen buffer and not written to the
 actual terminal until this screen_refresh() is done.  We only write lines that changed
 since the last call (unless full_refresh is 1, in which case we rewrite ever row).  We
 assume the physical screen looks like we thing it looks like.
 
 This is the "guts" of our poor man's curses implementation.
*/

void screen_refresh(int full_refresh)
{
    int y, need_flush = 0;
    
    /* If we have no screen, this is a NOP.  Otherwise we only write the lines (rows) 	*/
    /* that changed since the last time it was written.  Further we only write from the	*/
    /* left edge of the history area since that's the only part of the screen we deal	*/
    /* with.										*/
    
    if (screen) {				/* if we have a screen...		*/
    	for (y = 0; y < max_y; ++y)		/* ...rewrite changed lines (rows)...	*/
	    if (full_refresh                  ||
	        row_len[y] != prev_row_len[y] ||
	        memcmp(screen + y*max_x, prev_screen + y*max_x, row_len[y]) != 0) {
    		fprintf(screen_stdout, GOTO, y + 1, origin_x);
		if (row_len[y] > 0)
		    fwrite(screen + y*max_x + origin_x - 1, 1, row_len[y] - origin_x + 1, screen_stdout);
		memcpy(prev_screen + y*max_x, screen + y*max_x, row_len[y]);
		prev_row_len[y] = row_len[y];
		need_flush = 1;
	    }
	
	if (need_flush)
	    fflush(screen_stdout);
    }
}


/*--------------------------------------------*
 | screen_init - initialize for screen output |
 *--------------------------------------------*
 
 This must be called before any screen output.  The screen coordinates (1-relative) of
 the history area are passed.  If there's currently an open screen we close it before
 creating a new one.
 
 Here we allocate a screen buffer and initialize it to all blanks.  The logical cursor
 for the image is positioned at (bottom,left), i.e., bottom left corner of history
 area.
*/

void screen_init(int top, int left, int bottom, int right)
{
    screen_close();				/* make sure "old" screen is closed	*/
    
    if (scroll_mode == 0)			/* if slow scrolling enabled...		*/
	return;					/* ...now using non-screen output form	*/
    
    get_screen_size(&max_rows, &max_cols);
    
    origin_y = top;				/* this is (1,1) of the history area	*/
    origin_x = left;
    
    /* The width (columns) assumes a worse possible (actually impossible) case where 	*/
    /* there were nothing but a line of xterm cursor positioning controls, i.e.,	*/
    /* "\033[r;cH" where r and c are the row and column screen coordinates and each is	*/
    /* a 3-digit number.  That's 10 bytes per column.					*/
    
    max_y = max_rows;				/* this many rows			*/
    max_x = max_cols + 10*max_cols;		/* worse possible case for width	*/
    
    screen       = gdb_malloc(max_y * max_x);	/* allocate the screen buffers...	*/
    prev_screen  = gdb_malloc(max_y * max_x);
    
    row_len	 = gdb_malloc(max_y * sizeof(int)); /* allocate the row size arrays...	*/
    prev_row_len = gdb_malloc(max_y * sizeof(int));
    
    memset(screen,      ' ', max_y * max_x);	/* init the screens to all blanks...	*/
    memset(prev_screen, ' ', max_y * max_x);
    
    memset(row_len,     0, max_y * sizeof(int));/* init the row sizes to all 0's	*/
    memset(prev_row_len,0, max_y * sizeof(int));
        
    cursor_y = bottom;				/* position the cursor just in case	*/
    cursor_x = left;
    
    #define TRY_SETVBUF 0
    #if TRY_SETVBUF
    stdout_fd = dup(STDOUT_FILENO);
    if (stdout_fd >= 0) {
	screen_stdout = fdopen(stdout_fd, "a");
	if (!screen_stdout) {
	    screen_stdout = stdout;
	    //close(stdout_fd); // ??
	    stdout_fd = -1;
	} else
	    //setvbuf(screen_stdout, NULL, _IOFBF, BUFSIZE);
	    //setvbuf(screen_stdout, NULL, _IOFBF, 2800);
	    setvbuf(screen_stdout, NULL, _IOFBF, 200);
    }
    #else
    screen_stdout = stdout;
    #endif
}


/*----------------------------------------*
 | screen_close - close screen for output |
 *----------------------------------------*

 This frees the space allocated by screen_init and closes the terminal for output.
*/

void screen_close(void)
{
    #if TRY_SETVBUF
    if (stdout_fd >= 0) {
    	fclose(screen_stdout);
	//close(stdout_fd); // ??
	stdout_fd = -1;
    }
    #endif
    
    if (screen) {
    	gdb_free(screen);
	screen = NULL;
    }
    
    if (prev_screen) {
    	gdb_free(prev_screen);
	prev_screen = NULL;
    }
    
    if (row_len) {
    	gdb_free(row_len);
	row_len = NULL;
    }
    
    if (prev_row_len) {
    	gdb_free(prev_row_len);
	prev_row_len = NULL;
    }
}


/*-----------------------------------------------------------------------*
 | screen_fflush - fflush() for ALL screen output of the MacsBug display |
 *-----------------------------------------------------------------------*
 
 ALL (without exception) output to stdout or stderr (stream) flushes are funneled 
 through here.  This provides us with a convenient place to do fancy terminal
 manipulations (e.g., curses or curses-like simulation).
 
 If there is no virtual screen, or a macsbug screen, or we're not writing to the history
 area (Screen_Area tells us which area we're writing to) then this routine simply does
 a stdio fflush() to the specified stream.
*/

void screen_fflush(FILE *stream, Screen_Area area)
{
    if (!screen || !macsbug_screen || area != HISTORY_AREA)
    	fflush(stream);
}


/*---------------------------------------------------------------------*
 | screen_fputs - fputs() for ALL screen output of the MacsBug display |
 *---------------------------------------------------------------------*
 
 ALL (without exception) output to stdout or stderr (stream) that uses fputs()-style
 output calls is funneled through this routine for disposition.  This provides us with
 a convenient place to do fancy terminal manipulations in the future (curses?).
 
 Note, it is assumed that all lines passing through here might have standard xterm
 controls embedded in them.  Further, the only controls that might be there are the
 ones defined in MacsBug.h (CLEAR_LINE, GOTO, etc.).  If there's going to be any fancy
 or non-standard use of the terminal then here is where we "play terminal" and interpret
 the xterm controls to map them into whatever else is desired.
 
 If there is no virtual screen, or a macsbug screen, or we're not writing to the history
 area (Screen_Area tells us which area we're writing to) then this routine simply does
 a stdio fputs() to the specified stream.
*/

void screen_fputs(char *line, FILE *stream, Screen_Area area)
{
    unsigned char c, *p, *yp, *xp;
    
    if (!line) 					/* saftey check, no line, no nothing!	*/
    	return;
    
    if (!screen || !macsbug_screen || area != HISTORY_AREA) {
    	fputs(line, stream);
	return;
    }
    
    /* From here on we're writing to a virtual screen which won't be written until a	*/
    /* screen_refresh() call is done.  Here we place the characters into a virtual 	*/
    /* screen at the current cursor coordinates (cursor_y, cursor_x).			*/
    
    /* There are three xterm controls we need to be sensitive to:			*/
    
    /*   1. ESC 7 and ESC 8 - save and restore the virtual cursor (respectively)	*/
    /*	 2. ESC 0 K         - clear to end of line (truncate row length to cursor_x)	*/
    /*	 3. ESC [ y ; x H   - cursor position (cursor_y = y, cursor_x = x)		*/
    
    /* All others are treated as normal characters (including color xterm controls).	*/
    /* Note that the save/restore cursor and clear to end of line are also output.  The	*/
    /* upper level history output makes no assumptions about what we do here.  It 	*/
    /* assumes its writing directly to the terminal.  So it takes care of line wrapping	*/
    /* and line clearing.  That's how clear to end of line controls get into the line	*/
    /* in the first place.  So we are required to output them.  The up side is that this*/
    /* makes our life hear simpler.							*/
    
    p = (unsigned char *)line - 1;		/* prepare to scan the line...		*/
    while ((c = *++p) != '\0') {		/* process the chars in the line...	*/
	if (c != ESC)				/* not ESC...				*/
	    write_char(c);			/* ...add at current cursor, bump cursor*/
	else if (*++p != '[') {			/* ESC ?				*/
	    if (*p == '7') {			/* ESC 7				*/
		saved_cursor_y = cursor_y;	/* ...save cursor			*/
	    	saved_cursor_x = cursor_x;
	    } else if (*p == '8') {		/* ESC 8				*/
	    	cursor_y = saved_cursor_y;	/* ...restore cursor			*/
		cursor_x = saved_cursor_x;
	    }
	    write_char(ESC);
	    write_char(*p);
	} else if (*(p+1) == '0' && *(p+2) == 'K') {/* ESC 0 K				*/
	    row_len[cursor_y-1] = cursor_x;	/* set row length to current cursor_x	*/
	    write_char(ESC);			/* ...but still add these to the line	*/
	    write_char('[');
	    write_char('0');
	    write_char('K');
	    p += 2;
	} else {				/* ESC [ y ; x H			*/ 
	    yp = p;				/* ...yp points at the '['		*/
	    xp = NULL;				/* ...xp will (should) point at the ';'	*/
	    while ((c=*++p) != '\0' && !isalpha(c)) /* ...find the end of the sequence	*/
		if (c == ';')
		    xp = p;			/* ...set xp to the ';' along the way	*/
	    if (xp && c == 'H') {		/* ...if we truly have ESC [ y ; x H	*/
		cursor_y = cursor_x = 0;	/* ...set cursor_y and cursor_x		*/
		while (++yp < xp)
		    cursor_y = cursor_y * 10 + (*yp - '0');
		while (++xp < p)
		    cursor_x = cursor_x * 10 + (*xp - '0');
	    } else {				/* ...if not ESC [ y ; x H		*/
		write_char(ESC);		/* ...treat sequence as normal chars	*/
		write_char('[');
		while (++yp <= p)
		    write_char(*yp);
	    }
	}
    }
}


/*--------------------------------------------------------*
 | screen_fprintf - fprintf()-style MacsBug screen output |
 *--------------------------------------------------------*
 
 All fprintf-style output to stdout or stderr (stream) is funneled through this routine
 which in turn funnels it through screen_fputs().
*/

void screen_fprintf(FILE *stream, Screen_Area area, char *format, ...)
{
    va_list ap;
    
    static char line[BUFSIZE + 1];
 
    va_start(ap, format);
    vsprintf(line, format, ap);
    va_end(ap);
   
    screen_fputs(line, stream, area);
}

//#define screen_fflush(stream) fflush(stream)
//#define screen_fputs(line, stream) fputs(line, stream)
//#define screen_fprintf(stream, format, args...) fprintf(stream, format, ##args)
