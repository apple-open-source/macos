/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                  MacsBug_display.c                                   |
 |                                                                                      |
 |                     MacsBug screen area display control routines                     |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2006                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*

 This file contains all the stuff dealing with simulating a MacsBug display screen.  All
 the output is logical however.  The physical output is done by the routines in
 MacsBug_screen.c.  Doing so allows us to isolate and possibly optimize the physical
 output to improve its performance.
*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/param.h>

#include "MacsBug.h"

/*--------------------------------------------------------------------------------------*/

/* REMINDER: ANY WRITES TO THE HISTORY_AREA THAT REQUIRE CLEAR_LINE MUST PLACE THE 	*/
/*           CLEAR_LINE AT THE END OF THE LINE AND NOT AT THE BEGINNING.  THIS IS DUE	*/
/*           TO THE WAY screen_fprintf() and screen_fputs() PROCESS CURSOR GOTO'S FOR	*/
/*           POSITIONING THE CURSOR.							*/

/*--------------------------------------------------------------------------------------*/

int target_arch    = 4;				/* target architecture (4/8 for 32/64)	*/

int macsbug_screen = 0;				/* !=0 ==> MacsBug screen is active	*/

GDB_FILE *macsbug_screen_stdout = NULL;		/* macsbug screen's stdout		*/
GDB_FILE *macsbug_screen_stderr = NULL;		/* macsbug screen's stderr		*/

int max_rows, max_cols;				/* current screen dimensions		*/

int continued_len = 0;				/* continued cmd line, length so far	*/
int continued_count = 0;			/* number of continued line segments	*/
char *continued_line_starts[MAX_CMD_LINES];	/* array of continued_line lines starts */
int  continued_segment_len[MAX_CMD_LINES];	/* length of each continued line segment*/
char continued_line[1024];			/* current continued command line	*/

int doing_set_prompt = 0;			/* currently doing our own set prompt	*/

int scroll_mode    = 1;				/* 1 ==> fast scroll, 0 ==> slow	*/
int need_CurApName = 1;				/* 1 ==> run issued, need new curApName	*/

FILE *log_stream   = NULL;			/* log file stream variable		*/
char *log_filename = NULL;			/* log filename (NULL if file closed)	*/

Special_Refresh_States immediate_flush;		/* Special_Refresh_States state switch	*/

static int current_pc_lines = 0;		/* nbr of lines in pc area (may be 1 	*/
						/* bigger than pc_area_lines if no sym	*/
						/* for pc area				*/

#define SCREEN_RESET()        write_line(RESET)
#define SCREEN_CLEAR_LINE()   write_line(CLEAR_LINE)
#define SCREEN_GOTO(row, col) write_line(GOTO, row, col)

static char prompt[300];			/* modified prompt with cursor position	*/
static int prompt_start = 0;			/* where in prompt original starts	*/

static GDB_ADDRESS previous_pc = 0;		/* previous pc displayed in pc area	*/
static int first_sidebar       = 1;
static int prev_running        = 1;

typedef struct History {			/* history line data:			*/
    struct History *next, *prev;		/*    kept as a doubly linked list	*/
    unsigned short flags;			/*    line flags			*/
    	#define ERR_LINE      0x0001		/*    stderr line (shown in red)	*/
    	#define PARTIAL_LINE1 0x0002		/*    new line and its a partial line	*/
    	#define PARTIAL_LINE2 0x0004		/*    additional partails after the 1st	*/
    char line[1];
} History;

static History *history;			/* head of history list			*/
static History *history_tail;			/* tail of history list			*/
static int history_cnt = 0;			/* number of lines i the list		*/
static History *screen_top;			/* list entry at the top of the screen	*/
static History *screen_bottom;			/* list entry at the bottom of screen	*/

static short history_top;			/* location of the history area		*/
static short history_left;
static short history_bottom;
static short history_right;
static short history_lines;			/* nbr of lines that fit in hostory area*/
static short history_wrap;			/* column after which a line wraps	*/

static short pc_top;				/* location of the pc area		*/
static short pc_left;
static short pc_bottom;
static short pc_right;
static short pc_lines;
static short pc_wrap;

static short cmd_top;				/* location of the command line area	*/
static short cmd_left;
static short cmd_bottom;
static short cmd_right;
static short cmd_lines;

static short side_bar_top;			/* location of the register side bar	*/
static short side_bar_left;
static short side_bar_bottom;
static short side_bar_right;

static short vertical_divider;			/* column to place vertical divider	*/
static short pc_top_divider;			/* row of pc area top divider		*/
static short pc_bottom_divider;			/* row of pc area bottom divider	*/

static GDB_HOOK *hook_stop = NULL;		/* hook-stop hander			*/

static GDB_ADDRESS   prev_pc;			/* reg values since last time displayed	*/
static GDB_ADDRESS   prev_lr;
static GDB_ADDRESS   prev_ctr;
static GDB_ADDRESS   prev_xer;
static GDB_ADDRESS   prev_mq;			/* ??					*/
static GDB_ADDRESS   prev_msr;
static GDB_ADDRESS   prev_gpr[32];

union {						/* gdb_get_register() could return a	*/
    unsigned long long cr_err;			/* error code as a long long.		*/
    unsigned long cr;				/* but the cr is always only 32-bits	*/
} prev_cr;

static unsigned long *prev_stack       = NULL;	/* stk values since last time displayed	*/
static char	     *prev_stack_color = NULL;	/* color state for prev_stack value	*/
static int           prev_stack_cnt    = 0;	/* nbr of entries in prev_stack[] array	*/

static char *curApName_title = "CurApName";	/* "CurApName" or "PID"			*/
static char side_bar_blanks[21];		/* blank entry for a side bar line	*/

#define BUFFER_OUTPUT 	1			/* controlls whether to buffer screen	*/

#if BUFFER_OUTPUT
static char output_buffer[BUFSIZE+1];		/* buffered screen's buffer		*/
static char *bp = output_buffer;		/* ptr to next free byte in buffer	*/
static int  buf_row = 0;			/* nbr of lines buffer represents	*/
#define END_OF_LINE ++buf_row			/* counts those lines			*/
#define FLUSH_BUFFER flush_buffer()		/* flush the buffer			*/	
#else
#define END_OF_LINE				/* nop if not buffering			*/
#define FLUSH_BUFFER screen_fflush(stdout, HISTORY_AREA)
#endif

static void sd(char *arg, int from_tty);	/* sd and su reference each other	*/

/*--------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------*
 | word_completion_cursor_control - word completion save/restore cursor |
 *----------------------------------------------------------------------*
 
 This routine is defined as a gdb_special_events() Gdb_Word_Completion_Cursor event to
 get control from word completion so that we may properly "time" when to save and
 restore the command line cursor.  We do it directly from here (by explicitly writing
 to stderr.  At save time cursor is on the command line and that's what we want to save.
 At restore time we will have displayed the choices in the history area so we make sure
 that display is flushed before we restore the cursor back to the command line.
*/

static void word_completion_cursor_control(int save_cursor)
{
    if (save_cursor)
    	fputs(SAVE_CURSOR, stderr);
    else {
    	screen_refresh(0);
	fprintf(stderr, GOTO CLEAR_LINE, cmd_bottom, cmd_left);
    	fputs(RESTORE_CURSOR, stderr);
    }
}


/*----------------------------------------------------------*
 | word_completion_query - display theword completion query |
 *----------------------------------------------------------*
 
 This routine is defined as a gdb_special_events() Gdb_Word_Completion_Query event to
 get control from word completion so that we can make sure the screen is properly
 refreshed after the query.
*/

static void word_completion_query(GDB_FILE *stream, char *query, ...)
{
    va_list ap;
    
    /* My original code attempted to put the completion query in the history area.	*/
    /* For the most part it worked but it did allow for a possibility to show a flaw	*/
    /* in the curses implementation.  If the query is done twice by, say hitting tab at	*/
    /* the start of a command line, the query is the same both times and in the same	*/
    /* place on the screen.  This confuses my curses stuff so that for some reason I'm	*/
    /* too lazy to analyze the cursor for the query prompt and maybe the query itself	*/
    /* seems to climb up the history area.  Rather than fight it I decided to move the	*/
    /* prompt out of the history area and into the bottom of the input area.  After all	*/
    /* you could argue this prompt is a form of input and also for the command line	*/
    /* input anyhow.  The prompt has no new line at the end so we can get away with 	*/
    /* putting in on this otherwise unused line (it's normally reserved for the return	*/
    /* the user hits after the command -- it doesn't count in the total area the user	*/
    /* specifies -- we always add it).							*/
    
    /* Because this prompt is now in this special place we have to make sure to clear 	*/
    /* it in word_completion_cursor_control() above.   This is a reminder because if I	*/
    /* ever revisit this code to figure out what happend (fat chance) I have to remove	*/
    /* that line clear (ok, I really don't, but what's the point of keeping it in?).	*/
    
    #if 0
    va_start(ap, query);
    gdb_vfprintf(stream, query, ap);
    va_end(ap);
   
    gdb_fflush(stream);
    screen_refresh(0);
    #else
    fprintf(stderr, GOTO COLOR_BOLD, cmd_bottom, cmd_left);
    va_start(ap, query);
    vfprintf(stderr, query, ap);
    va_end(ap);
    fprintf(stderr, COLOR_OFF);

    #endif
}

/*--------------------------------------------------------------------------------------*/

/*--------------------------------------------------*
 | get_screen_size - return the current screen size |
 *--------------------------------------------------*/

void get_screen_size(int *max_rows, int *max_cols)
{
    struct winsize size;
    
    if (!isatty(STDOUT_FILENO))
    	*max_rows = *max_cols = 0;
    else if (ioctl(STDOUT_FILENO, TIOCGWINSZ, (char *)&size) < 0)
    	gdb_error("Can't get window size");
    else {
    	*max_rows = size.ws_row;
    	*max_cols = size.ws_col;
    }
}


#if BUFFER_OUTPUT
/*------------------------------------------------------*
 | flush_buffer - flush the output buffer to the screen |
 *------------------------------------------------------*
 
 All the bytes in the output_buffer which was set by write_line() are written to the
 screen. 
*/

static void flush_buffer(void)
{
    if (bp > output_buffer) {			/* if there's something in the buffer...*/
    	*bp = '\0';				/* ...write it out			*/
    	
	#if 0					/* for debugging...			*/ 
    	{					/* there should be any writes outside	*/
	    int k58 = 0, nl  = 0;		/* the history area nor any newlines	*/
	    char *bp1;
    
	    for (bp1 = output_buffer; *bp1; ++bp1)
		if (*bp1 == 58)
		    k58 = 1;
		else if (*bp1 == '\n')
		    nl = 1;
	    if (k58)
		bp += sprintf(bp, "еее 58 еее");
	    if (nl)
		bp += sprintf(bp, "еее nl еее");
	}
    	#endif
	
	screen_fputs(output_buffer, stdout, HISTORY_AREA);
    	screen_fflush(stdout, HISTORY_AREA);
    }
    
    bp = output_buffer;
    buf_row = 0;
}
#endif


/*--------------------------------------------------------------------*
 | position_cursor_for_shell_input - set cursor to accept shell input |
 *--------------------------------------------------------------------*
 
 This is called whenever we know we are going back to the shell (e.g., when exiting gdb).
 We position the cursor to the bottom of the screen so that the shell lines will cause
 the screen to scroll up.  This is needed in enough places to warrant its own routine.
*/

void position_cursor_for_shell_input(void)
{
    struct winsize size;
    
    if (macsbug_screen) {
    	flush_buffer();
    	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, (char *)&size) >= 0)
    	    screen_fprintf(stderr, NO_AREA, GOTO "\n", size.ws_row, size.ws_col);
    }
}


/*-----------------------------------------------*
 | write_line - handle complete lines for output |
 *-----------------------------------------------*
 
 When an entire line is ready for output to the history area it is sent here.  It can
 also be used to explicitly write formatted lines to the history area.  Hence the
 printf-line parameter list.  Like printf() the function returns the number of characters
 written.
*/

static int write_line(char *fmt, ...)
{
    int     len, turn_off;
    va_list ap;
    char    c, *p, line[1024];
    
    va_start(ap, fmt);
    len = vsprintf(line, fmt, ap);		/* build the line			*/
    va_end(ap);
    
    /* Guarantee that no line exceeds the current history area width (history_wrap). 	*/
    /* Lines manufactured by write_to_history_area() guarantee this.  But once those 	*/
    /* lines are recorded on the history lines we could end up redisplaying them by a	*/
    /* screen refresh after a screen resize.  Then we run the risk of the terminal	*/
    /* wrapping lines longer than the resized screen width.  So we beat it to the punch	*/
    /* here by simply truncating the lines if they now exceed the current history width.*/
    
    /* Note, we have to be careful here about computing the length since the line could	*/
    /* contain xterm escape controls.  All our controls come it two flavors; ESC x, or	*/
    /* ESC [ ... y, where x is anything other than a '[' and y is any letter.  These 	*/
    /* don't count in the length for computing whether we need to wrap or not.  But they*/
    /* still must be retained of course.  If we do truncate the line and we saw the	*/
    /* ESC [ ... y sequence, we assume it's a color control and add COLOR_OFF at the end*/
    /* We never truncate in the middle of an escape sequence.				*/
    
    if (len > history_wrap) {			/* optimize for case where it all fits	*/
	len = turn_off = 0;			/* we need now to "manually" compute len*/
	p = line - 1;
	while ((c = *++p) != '\0') {		/* scan the line and count real chars...*/
	    if (c != ESC) {			/* ...handle normal characters...	*/
		if (++len > history_wrap)	/* ...if we reach the wrap point	*/
		    break;			/* ...end the scan			*/
	    } else {
	    	turn_off = 0;			/* ...assume line ends with ESC sequence*/
		if (*++p == '[') {		/* ...handleESC x and ESC [ ... y	*/
		    while ((c=*++p) && !isalpha(c)); /* ...ESC [ ... y			*/
		    if (!c || !*++p) 		/* ...if ESC sequence was end of line	*/
			break;			/* ...end the scan			*/
		    turn_off = 1;		/* ...ESC [ ... y not at end of line	*/
		    --p;			/* ...compensate for the peek ahead	*/
		}
	    }
	}
	
	if (turn_off) {				/* at end of line or the wrap point...	*/
	    strcpy(p, COLOR_OFF);		/* append COLOR_OFF if we need to	*/
	    p += strlen(COLOR_OFF);
	}
	*p = '\0';
	len = p - line;				/* this is the new length to write	*/
    }
        
    #if BUFFER_OUTPUT
    
    if (bp + len >= output_buffer + BUFSIZE)	/* flush if it doesn't fit in buffer	*/	
    	FLUSH_BUFFER;
    
    memcpy(bp, line, len);			/* save line in the buffer		*/
    bp += len;					/* next free byte in the buffer		*/
        
    #else
    
    screen_fputs(line, stdout, HISTORY_AREA);
   
    #endif
    
    return (len);				/* return nbr of bytes written		*/
}


/*---------------------------------------------------*
 | su [n] - scroll the history up n (default 1) line |
 *---------------------------------------------------*/

static void su(char *arg, int from_tty)
{
    int     i, n;
    History *h1;
    char    negated_arg[25];
    
    if (!macsbug_screen)
    	return;
    
    if (arg && *arg) {				/* if there's an arg, parse it		*/
    	n = gdb_get_int(arg);
	if (n == 0)				/* n = 0 is a NOP			*/
	    return;
	if (n < 0) {				/* n < 0 is a sd -n			*/
	    sprintf(negated_arg, "%d", -n);
	    sd(negated_arg, from_tty);
	    return;
	}
    } else
    	n = 1;
    
    get_screen_size(&max_rows, &max_cols);
    
    /* We only accept this operation if the recorded history exceeds the history area	*/
    /* and we are not already at the start of the history list.				*/
    
    if (history_cnt > history_lines) {
        if (screen_top && screen_bottom && screen_top->prev && screen_bottom->prev) {
    	    while (n--) {
	        if (screen_top->prev == NULL || screen_bottom->prev == NULL)
		    break;
		screen_top    = screen_top->prev;
		screen_bottom = screen_bottom->prev;
	    }
	    
            for (i = 1, h1 = screen_top; h1 && i <= history_lines; ++i, h1 = h1->next) {
		SCREEN_GOTO(i, history_left);
  	    	if (h1->flags & ERR_LINE)
  		    write_line(COLOR_RED "%s" COLOR_OFF, h1->line);
  	    	else
  		    write_line("%s", h1->line);
		SCREEN_CLEAR_LINE();
    	    	END_OF_LINE;
            }
            FLUSH_BUFFER;
         }
    }
}

#define SU_HELP \
"SU [n] -- Scroll MacsBug history up n (or 1) line(s)." \
"Same as SCROLL up [n]."


/*-----------------------------------------------------*
 | sd [n] - scroll the history down n (default 1) line |
 *-----------------------------------------------------*/

static void sd(char *arg, int from_tty)
{
    int     i, n;
    History *h1;
    char    negated_arg[25];
     
    if (!macsbug_screen)
    	return;
  
    if (arg && *arg) {				/* if there's an arg, parse it		*/
    	n = gdb_get_int(arg);
	if (n == 0)				/* n = 0 is a NOP			*/
	    return;
	if (n < 0) {				/* n < 0 is a su -n			*/
	    sprintf(negated_arg, "%d", -n);
	    su(negated_arg, from_tty);
	    return;
	}
    } else					/* default is one line			*/
    	n = 1;
    
    get_screen_size(&max_rows, &max_cols);
    
    /* We only accept this operation if the recorded history exceeds the history area	*/
    /* and we are not already at the bottom of the history list.			*/
    
    if (history_cnt > history_lines) {
        if (screen_top && screen_bottom && screen_top->next && screen_bottom->next) {
    	    while (n--) {
	        if (screen_top->next == NULL || screen_bottom->next == NULL)
		    break;
		screen_top    = screen_top->next;
		screen_bottom = screen_bottom->next;
	    }
            
	    for (i = 1, h1 = screen_top; h1 && i <= history_lines; ++i, h1 = h1->next) {
		SCREEN_GOTO(i, history_left);
  	    	if (h1->flags & ERR_LINE)
  		    write_line(COLOR_RED "%s" COLOR_OFF, h1->line);
  	    	else
  		    write_line("%s", h1->line);
		SCREEN_CLEAR_LINE();
    	    	END_OF_LINE;
              }
            FLUSH_BUFFER;
         }
    }
}

#define SD_HELP \
"SD [n] -- Scroll MacsBug history down n (or 1) line(s)." \
"Same as SCROLL down [n]."


/*---------------------------------------------------------------------------------------*
 | scroll up [n] | down [n] |fast | slow - scroll history up or down (also control mode) |
 *---------------------------------------------------------------------------------------*
 
 The history display is scrolled up or down as requested one line at a time.
 
 The fast/slow are unpublished parameters to toggle between the curses mode and brute
 force mode of scrolling.
*/

static void scroll(char *arg, int from_tty)
{
    int  argc;
    char *argv[5], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "scroll", &argc, argv, 4);
    if (argc < 2)
    	gdb_error("scroll up | down expected");
    if (argc > 3)
    	gdb_error("Too many scroll arguments");
    
    if (gdb_strcmpl(argv[1], "up"))
    	su((argc == 3) ? argv[2] : NULL, 0);
    else if (gdb_strcmpl(argv[1], "down"))
    	sd((argc == 3) ? argv[2] : NULL, 0);
    else if (gdb_strcmpl(argv[1], "fast")) {
    	if (macsbug_screen && scroll_mode == 0) {
	    scroll_mode = 1;
	    refresh(NULL, 0);
	} else
	    scroll_mode = 1;
    } else if (gdb_strcmpl(argv[1], "slow")) {
    	if (macsbug_screen && scroll_mode == 1) {
	    scroll_mode = 0;
	    refresh(NULL, 0);
	 } else
	     scroll_mode = 0;
    } else
    	gdb_error("scroll up | down expected");
}

#define SCROLL_HELP \
"SCROLL up [n] | down [n] -- Scroll MacsBug history up or down n (or 1) line(s)."


/*-----------------------------------------*
 | pgu [n] - page up n (or 1) history page |
 *-----------------------------------------*/

static void pgu(char *arg, int from_tty)
{
    char lines[25];
    
    sprintf(lines, "%ld", ((arg && *arg) ? gdb_get_long(arg) : 1) * history_lines);

    su(lines, from_tty);			/* scroll up n * history_lines lines	*/
}

#define PGU_HELP \
"PGU [n] -- Page MacsBug history up n (or 1) page(s).\n" \
"Same as PAGE up [n]."


/*-------------------------------------------*
 | pgd [n] - page down n (or 1) history page |
 *-------------------------------------------*/

static void pgd(char *arg, int from_tty)
{
    char lines[25];
    
    sprintf(lines, "%ld", ((arg && *arg) ? gdb_get_long(arg) : 1) * history_lines);

    sd(lines, from_tty);			/* scroll down n * history_lines lines	*/
}

#define PGD_HELP \
"PGD [n] -- Page MacsBug history down n (or 1) page(s).\n" \
"Same as PAGE down [n]."


/*-----------------------------------------------------*
 | page|pg up [n] | down [n] - page history up or down |
 *-----------------------------------------------------*/

static void page(char *arg, int from_tty)
{
    int  argc;
    char *argv[5], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "scroll", &argc, argv, 4);
    if (argc < 2)
    	gdb_error("page up | down expected");
    if (argc > 3)
    	gdb_error("Too many page arguments");
	
    if (gdb_strcmpl(argv[1], "up"))
    	pgu((argc == 3) ? argv[2] : NULL, from_tty);
    else if (gdb_strcmpl(argv[1], "down"))
    	pgd((argc == 3) ? argv[2] : NULL, from_tty);
    else
    	gdb_error("page up | down expected");
}

#define PAGE_HELP \
"PAGE up [n] | down [n] -- Page MacsBug history up or down n (or 1) page(s)."


/*---------------------------------*
 | LOG? - display current log file |
 *---------------------------------*/

static void which_log(char *arg, int from_tty)
{
    if (log_stream)
    	if (macsbug_screen)
	    gdb_printf("Logging to %s\n", log_filename);
	else
	    gdb_printf("Will be logging to %s when MacsBug screen is turned on\n", log_filename);
    else
	gdb_printf("No log file is open\n");
}


/*-------------------------------------------------*
 | LOG [-h] [filename | ?] - open/close a log file |
 *-------------------------------------------------*/

static void log_(char *arg, int from_tty)
{
    int     hopt, argc;
    History *h;
    FILE    *f;
    char    *filename, *argv[5], tmpCmdLine[1024];
    
    gdb_set_int("$__lastcmd__", 43);
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "log", &argc, argv, 4);
    if (argc > 3)
    	gdb_error("LOG [-h] [pathname | ?] -- wrong number of arguments");
    
    if (argc == 1) {
    	if (log_stream == NULL)
	    gdb_error("Log is already closed");
	else {
	    fclose(log_stream);
	    gdb_printf("Closing log\n");
	    log_stream = NULL;
	}
	return;
    }
    
    if (argc == 3) {
    	if (*argv[1] != '-' || !gdb_strcmpl(argv[1], "-h"))
	    gdb_error("Invalid log parameters");
	hopt = 1;
	filename = argv[2];
    } else if (*argv[1] == '?') {
    	which_log(NULL, 0);
	return;
    } else if (*argv[1] != '-') {
    	hopt = 0;
	filename = argv[1];
    } else
    	gdb_error("Invalid log parameters");
    
    if (log_stream)
    	gdb_error("Log file already open");
    
    log_filename = gdb_realloc(log_filename, strlen(filename) + 1);
    filename = gdb_tilde_expand(strcpy(log_filename, filename));
    
    f = fopen(filename, "a");
    gdb_free(filename);
    if (!f) 
    	gdb_error("Cannot open log file: %s", strerror(errno));
    
    if (macsbug_screen)
    	gdb_printf("Logging to %s\n", log_filename);
    else
    	gdb_printf("Will log to %s when MacsBug screen is turned on\n", log_filename);
    
    log_stream = f;			/* delayed until aobve message displays		*/
    if (hopt)
    	for (h = history; h; h = h->next)
	    fprintf(log_stream, "%s\n", h->line);
}

#define LOG_HELP \
"LOG [-h] [filename] -- Log all MacsBug output to a file.\n" 				\
"LOG without parameters turns logging off.  LOG with only a filename creates the\n" 	\
"log file with that filename.  LOG -h saves the scrollback history to the bottom\n"	\
"of the current screen to the log file.  LOG? reveals what log file is open."


/*----------------------------------------------------------*
 | write_to_history_area - write a line to the history area |
 *----------------------------------------------------------*
 
 This is where all history lines (data) are sent. The FILE *f is either stdout or stderr.
 If it's stderr we'll cause the line to appear in red.  The history (doubly-linked) list
 is built here also to allow up and down scrolling with the UP and DOWN commands.
 
 We may call this routine recursively to wrap long lines (unless wrap_lines is false) and
 also we may recursively call to handle a remaining partial line during a flush 
 operation (line == NULL for flush).
 
 This function is a output redirection filter.  As such it is not normally called for
 parital lines (lines that don't end with a newline).  However a partial line is 
 possible for the stream is flushed.  Note hoewever that we still expect a newline
 to be eventually written.  If it isn't then the partial line may not ever get written.
*/

static char *write_to_history_area(FILE *f, char *line, void *data)
{
    int     i, len, wrap, write_col, err = (f == stderr), partial_line;
    History *h, *h1;
    char    *src, *dst, detabbed_line[1024];
    
    /* A NULL line means we're flushing.  If the immediate_flush is not NORMAL_REFRESH	*/
    /* we also want to call screen_refresh() as well.  If immediate_flush is 		*/
    /* QUERY_REFRESH1 then this flush occurred before a query prompt so we set the	*/
    /* state to QUERY_REFRESH2 where it will remain until the prompt occurs.		*/
    
    if (!line) {
    	FLUSH_BUFFER;
	if (immediate_flush != NORMAL_REFRESH) {
	    screen_refresh(0);
	    if (immediate_flush == QUERY_REFRESH1)
	    	immediate_flush = QUERY_REFRESH2;
	}
   	return (NULL);
    }
        
    /* See if we have a parital line (i.e., a line with no newline at the end or a	*/
    /* completed line.  								*/
    
    len = strlen(line);
    if (len == 0)
        return (NULL);
    
    if (line[len-1] == '\n') {
    	line[--len] = '\0';			/* we don't need nor want the newline	*/
    	partial_line = 0;
    } else
    	partial_line = 1;
    
    if (history_tail && (history_tail->flags & (PARTIAL_LINE1|PARTIAL_LINE2))) {
    	wrap = history_wrap - strlen(history_tail->line);
    } else
    	wrap = history_wrap;
    
    /* Detab the line.  If we're going to be adding it to a tail that represents a	*/
    /* partial line, then the effective start column is the length of the partial lines.*/
    /* Otherwise it's 1.								*/
    
    if (history_tail && (history_tail->flags & (PARTIAL_LINE1|PARTIAL_LINE2)))
        i = strlen(history_tail->line);
    else
    	i = 1;
    
    src = line;					/* detab into detabbed_line...		*/
    dst = detabbed_line;
    while (*src)
        if (*src == '\t') {
            do {
        	*dst++ = ' ';
        	++i;
            } while ((i % tab_value) != 1);
            ++src;
        } else {
            *dst++ = *src++;
            ++i;
        }
    *dst = '\0';
        
    len = strlen(line = detabbed_line);
    
    /* Handle line wrapping.  Write chunks of line that fit in the history are leaving	*/
    /* the last chunk to be handled normally...						*/
    
    get_screen_size(&max_rows, &max_cols);
    
    if (len > wrap) {				/* if we need to wrap...		*/
    	if (!wrap_lines)			/* ...but wrapping is not wanted...	*/
	    line[wrap] = '\0';			/* ...just truncate			*/
    	else					/* ...wrap the line			*/
	    do {				
		char c1 = line[wrap];		/* ...temporarily truncate the line	*/
		char c2 = line[wrap+1];
		line[wrap]   = '\n';		/* ...this makes us think it's complete	*/
		line[wrap+1] = '\0';
		
		write_to_history_area(f, line, data); /* ...write a portion that fits	*/
		
		line[wrap]   = c1;		/* ...undo the damage			*/
		line[wrap+1] = c2;
		
		len  -= wrap;			/* ...get rid of the written portion	*/
		line += wrap;
	    } while (len > history_wrap);
    }
    
    /* Log the line if logging is on...							*/
    
    if (log_stream)
    	fprintf(log_stream, "%s\n", line);
    
    /* If we have a partial line and the history tail is also a partial line we replace	*/
    /* the tail with a new tail consisting of the concatenation of the old tail line 	*/
    /* and the new parial line.	Since the previous partial(s) for the tail have already	*/
    /* been written we set the write_col to be at the end of those previous partial(s).	*/
    
    /* If we have a new (possibly) partial line then the write_col is of course the 	*/
    /* left end of the command line.							*/
        
    if (history_tail && (history_tail->flags & (PARTIAL_LINE1|PARTIAL_LINE2))) {
        write_col = strlen(history_tail->line);
        
        if (len > 0) {				/* optimize for 0 len (was just '\n')	*/
            h = (History *)gdb_malloc(sizeof(History) + write_col + len);
            *h = *history_tail;			/* create a new replacement tail	*/
            strcpy(h->line, history_tail->line);
            strcpy(h->line+write_col, line);
    	
    	    if (history_tail->prev)		/* replace the tail with new tail that	*/
    	    	history_tail->prev->next = h;	/* contains the concatenation of the 	*/
    	    if (history_tail == history)	/* two partial lines			*/
    	    	history = h;
    	    if (screen_top == history_tail)
    	   	screen_top = h;
    	    if (screen_bottom == history_tail)
    	   	screen_bottom = h;
    	    gdb_free(history_tail);		/* free old tail			*/
    	    history_tail = h;
	}
	
	/* PARTIAL_LINE1 means we have the first partial of a new line.  PARTIAL_LINE2	*/
	/* means we have additional partials tacked on to the end of the first partial.	*/
	/* We're in here because we are tacking on additional partials.  So set the 	*/
	/* history tail flag to PARTIAL_LINE2.						*/
	
        history_tail->flags &= ~PARTIAL_LINE1;	/* this is no longer the 1st partial	*/
        history_tail->flags |= PARTIAL_LINE2;
	
	/* If the new line is itself a partial set the partial_line state variable to	*/
	/* indicate we are still concatenating partials (2).  If the new line is the 	*/
	/* last part of a set of partials (i.e., it did have a '\n') then set the state	*/
	/* to indicate that (3).  State 2 indicates that we are not to create a new	*/
	/* history list entry.  State 3 indicates that the tail is no longer a partial	*/
	/* line and that both the PARTIAL_LINE1 and PARTIAL_LINE2 flags should be	*/
	/* removed.  We leave them till later to allow checking below.			*/
	 
	partial_line = partial_line ? 2 : 3;
	write_col += history_left;		/* adjust for display			*/
    } else
    	write_col = history_left;
    
    /* If we don't have a partial line or we do but it's the first partial of a new	*/
    /* line then scroll the history area up to make room for the new line.  If its a	*/
    /* partial then the additional partials won't cause scrolling.			*/
    
    if (!partial_line || !history_tail || !(history_tail->flags & PARTIAL_LINE2)) {
    
	/* Make sure that all history lines beyond what is showing are freed.  We may 	*/
	/* not be currently at the end of the history list because an UP command was 	*/
	/* done to scroll up the history.						*/
	    
	if (screen_bottom) {			/* if there is any history...		*/ 
	    history_tail = screen_bottom;	/* ...free all beyond the bottom line	*/
	    h1 = screen_bottom->next;		/*    showing on the screen		*/
	    screen_bottom->next = NULL;
	    while (h1) {
		h = h1->next;
		gdb_free(h1);
		h1 = h;
		--history_cnt;
	    }
	}
	
	if (history_cnt) {			/* 0 when there's no history at all	*/
	    if (history_cnt < history_lines)/* all history lines will fit in area 	*/
		i = history_lines - history_cnt;/* ...start on this row		*/
	    else {				/* more history than fits in area...	*/
		screen_top = screen_top->next;	/* ...set new top screen pointer	*/
		i = 1;				/* ...of course we start on row 1	*/
	    }
	    
	    /* Scroll up the history one line writing the ones associated with stderr	*/
	    /* in red...								*/
	    
	    for (h1 = screen_top; h1; ++i, h1 = h1->next) {
		if (h1->flags & ERR_LINE)
		    write_line(GOTO COLOR_RED "%s" COLOR_OFF CLEAR_LINE, i, history_left, h1->line);
		else
		    write_line(GOTO "%s" CLEAR_LINE, i, history_left, h1->line);
		END_OF_LINE;
	    }
	}
		
	/* If the new line exceeds the amount of history we are to record, then age 	*/
	/* out the oldest (first) history line...					*/
	
	if (history_cnt >= max_history) {
	    h = history;
	    history = history->next;
	    history->prev = NULL;
	    gdb_free(h);
	} else
	    ++history_cnt;
    }
    
    /* Record the new line at the end of the history list (unless this is an additional	*/
    /* partial line to a tail that was itself a partial).  This list is doubly linked	*/
    /* to allow the up and down scroll commands to move in either direction.		*/
    
    if (partial_line < 2) {			/* not partial or 1st partial...	*/
        h = (History *)gdb_malloc(sizeof(History) + strlen(line));
        h->next = NULL;
        h->prev = history_tail;
        h->flags = err ? ERR_LINE : 0;
        if (partial_line)
            h->flags |= PARTIAL_LINE1;
        strcpy(h->line, line);
        
        if (history_cnt == 1)
            history = screen_top = h;
        else
            history_tail->next = h;
        history_tail = screen_bottom = h;
    } else if (history_tail && partial_line == 3) /* last partial for this tail...	*/
    	history_tail->flags &= ~(PARTIAL_LINE1|PARTIAL_LINE2);
    	
    /* Finally write the new line into the history area.  If it's a 1st partial or new	*/
    /* line write it "normally".  If it's an additional partial for the tail use the	*/
    /* write_col set eariler.  Due to the limitation of the curses-like screen 		*/
    /* management we must make sure that the SCREEN_CLEAR_LINE we do is at the end of	*/
    /* the line if that line starts a new line.  We still clear the end for partial	*/
    /* writes but they end up clobbering the clear that preceded it since the curses-	*/
    /* like support doesn't take into account the invisible controls taking up column	*/
    /* positions (that's the limitiation).						*/
    
    SCREEN_GOTO(history_bottom, write_col);
    
    if (err) {
    	write_line(COLOR_RED "%s" COLOR_OFF, line);
    	if (write_col == history_left)
	    SCREEN_CLEAR_LINE();
    	FLUSH_BUFFER;
	screen_refresh(0);
    } else {
    	write_line("%s", line);
    	if (write_col == history_left)
	    SCREEN_CLEAR_LINE();
    }
    END_OF_LINE;
    //FLUSH_BUFFER;
    
    /* If immediate_flush is not NORMAL_REFRESH then set we do an immediate flush and 	*/
    /* screen refresh.  This immediate_flush state switch used for queries to get the	*/
    /* prompt(s) out at the  proper time, i.e., before a read comes up for the prompt's	*/
    /* answer!  It's also used for things like the FILE command which show dots as a	*/
    /* progress indication which we want to see as they are produced.  The switch is 	*/
    /* set to PROGRESS_REFRESH for that case and reset by those commands when completed.*/
    /* The QUERY_REFRESH1 and QUERY_REFRESH2 states are set for queries and reset when 	*/
    /* the queries are completed.							*/
    
    /* I am now overriding all the flush controls and always flushing.  This produces	*/
    /* a more accurate simulation of gdb's output. Problems arose where we couldn't see	*/
    /* some progress (e.g., library loads) and breakpoint displays as they happened	*/
    /* because all the stuff we being buffered up until the next prompt.		*/
    
    if (1||immediate_flush != NORMAL_REFRESH) {
	FLUSH_BUFFER;
	screen_refresh(0);
    	//if (immediate_flush == QUERY_REFRESH1 || immediate_flush == QUERY_REFRESH2)
	//    immediate_flush = NORMAL_REFRESH;
    }
    
    return (NULL);
}


/*-------------------------------------------------------------------------------*
 | rewrite_bottom_line - rewrite the last (most recent) line in the history area |
 *-------------------------------------------------------------------------------*
 
 The history tail is changed to the specified line image.  If its for an error (err = 1)
 the lines is colored red.
*/

void rewrite_bottom_line(char *line, int err)
{
    if (macsbug_screen && history_tail) {
	if (err) {
	    write_line(GOTO COLOR_RED "%s" COLOR_OFF, history_bottom, history_left, history_tail->line);
	    history_tail->flags |= ERR_LINE;
	} else {
	    write_line(GOTO "%s", history_bottom, history_left, history_tail->line);
	    history_tail->flags &= ~ERR_LINE;
	}
	END_OF_LINE;
    }
}


/*-------------------------------------------------------------------*
 | disasm_pc_area_output - format_disasm_line() output stream filter |
 *-------------------------------------------------------------------*
 
 format_disasm_line() writes to a stream created by display_pc_area() whose output
 redirection filter is this function.  Here we take each formatted disassembly line
 and write it to the pc area at the coordinates defined by the data channel that 
 display_pc_area() defined.  The data channel points to data with the following layout.
 											*/
 typedef struct {			
    short row, col;				/* row, col to write disasm line to	*/
    int   remaining;				/* counts nbr of lines written 		*/
    int   add1line;				/* set if no symbol at top of pc area	*/
    DisasmData *disasm_info;			/* format_disasm_line data pointer	*/
 } Disasm_pc_data;
 											/*
 The line counter explicitly counts the number of lines the disassembly output. It is
 possible that a disassembled line is for a new function.  In that case the function
 name is output as an additional line.  While we account for this at the start of the
 pc area we need to make sure that additional function name outputs don't cause us to
 overrun the bottom of the pc area.

 We ignore any steam flush actions (line == NULL) and of course return NULL to not cause
 the low level stream output stuff to try to write the line we already wrote.  Note, we
 probably could let it write it if we just returned the formatted string with the 
 positioning control.  But why bother?
*/

static char *disasm_pc_area_output(FILE *f, char *line, void *data)
{
    Disasm_pc_data *pc_data = (Disasm_pc_data *)data;
    
    /* If the line has nothing on it it must be because there is no symbol to display	*/
    /* at the top of the pc area.  We make a special case of that by setting a flag to	*/
    /* allow us to add one additional line to fill up the entire pc area instead of 	*/
    /* wasting the top line of it with a blank line normally reserved for the symbol.	*/
    
    if (line) {
    	char *p = line - 1;
    	while (*++p && (*p == ' ' || *p == '\n')) ;
    	if (!*p)
    	    pc_data->add1line = 1;
    	else if (pc_data->remaining-- >= 0)
    	    screen_fprintf(stdout, PC_AREA, GOTO CLEAR_LINE "%s", pc_data->row++, pc_data->col, line);
    }
    
    pc_data->disasm_info->flags &= ~ALWAYS_SHOW_NAME;
    
    return (NULL);
}


/*---------------------------------------------------------------------*
 | display_pc_area - display the PC area (disassembly starting at $pc) |
 *---------------------------------------------------------------------*
 
 N lines are displayed in the pc aread, where N is determined by the SET command. The
 initial default is DEFAULT_PC_LINES.
*/

void display_pc_area(void)
{
    int  	    row;
    //unsigned long limit;
    DisasmData	   disasm_info;
    Disasm_pc_data pc_data;
    GDB_FILE	   *redirect_stdout;
    GDB_ADDRESS	   addr, current_pc;
    char 	   line[1024];
    
    get_screen_size(&max_rows, &max_cols);
    
    memset(line, '_', pc_wrap);
    line[pc_wrap] = '\0';
    screen_fprintf(stderr, PC_AREA, GOTO "%s", pc_top_divider,    pc_left, line);
    screen_fprintf(stderr, PC_AREA, GOTO "%s", pc_bottom_divider, pc_left, line);
   
    if (!gdb_target_running()) {
    	for (row = pc_top; row <= pc_bottom; ++row) {
	    screen_fprintf(stdout, PC_AREA, GOTO, row, pc_left);
	    if (row == pc_top)
	    	screen_fprintf(stdout, PC_AREA, COLOR_RED "Not Running" COLOR_OFF CLEAR_LINE);
	    else
	    	screen_fprintf(stdout, PC_AREA, CLEAR_LINE);
	}
	previous_pc = 0;
	screen_fflush(stdout, PC_AREA);
    	return;
    }
    
    /* No sense redrawing if the pc hasn't moved...					*/
    
    if (!gdb_get_register("$pc", &current_pc))
    	current_pc = -1LL;
    
    if (previous_pc == current_pc)
    	return;
    previous_pc = current_pc;
    
    /* Sometimes gdb will print an extra line at the start of the first x/i done in a	*/
    /* session.  For example, for Cocoa inferiors, it will print something like,	*/
    
    /*      Current language:  auto; currently objective-c				*/
    
    /* This will confuse the disassembly reformatting.  We can fake gdb out, however,	*/
    /* by giving it "x/0i 0".  It won't do anything if gdb has nothing additional to	*/
    /* say.  But if it does, the additional stuff is all it will say.  This will print	*/
    /* to the current output stream which is where we want it to go.			*/
    
    gdb_execute_command("x/0i 0");
        
    /* This is a little tricky.  We use format_disasm_line() as usual to format the	*/
    /* the disassembly lines.  It writes those lines using the disasm_info.stream which	*/
    /* we set here to cause that stream to use disasm_pc_area_output() above.  Thus two	*/
    /* output redirection filters are being used.  One (format_disasm_line()) for the 	*/
    /* stdout display for the x/i gdb disassembly command we issue, and the other 	*/
    /* (disasm_pc_area_output()) for the stream format_disasm_line() writes to.		*/
    
    disasm_info.pc        = current_pc;		/* these are for format_disasm_line()	*/
    disasm_info.max_width = pc_wrap;
    disasm_info.comm_max  = pc_wrap;
    disasm_info.flags	  = (ALWAYS_SHOW_NAME|FLAG_PC|DISASM_PC_AREA);
    disasm_info.stream    = gdb_open_output(stdout, disasm_pc_area_output, &pc_data);
    
    pc_data.row = pc_top;			/* for disasm_pc_area_output() to use	*/
    pc_data.col = pc_left;
    pc_data.remaining = pc_lines;		/* line cnt (funct names count as lines)*/
    pc_data.disasm_info = &disasm_info;		/* disasm_pc_area_output changes flags	*/ 
    pc_data.add1line = 0;			/* get's set if no symbol displayed	*/
    
    redirect_stdout = gdb_open_output(stdout, format_disasm_line, &disasm_info);
    gdb_redirect_output(redirect_stdout);	/* x/i will go thru format_disasm_line()*/
   
    addr  = disasm_info.pc;			/* always disassemble starting from $pc	*/
    //limit = addr + 4 * pc_lines;		/* disassemble pc_lines lines		*/
    
    disasm_info.addr = addr;
    gdb_execute_command("x/%di 0x%llx", pc_lines, (long long)addr);
    
    /* If we didn't show a function name because it wasn't available then we have room	*/
    /* to add one more line of disassembly into the pc area.  Set current_pc_lines to   */
    /* indicate how many lines we actually currently have in the pc area.  We need to	*/
    /* know this in order to refresh the pc area if a breakpoint is set on an address	*/
    /* currently displayed in there.  We must not change pc_lines.			*/
    
    if (pc_data.add1line) {
    	 gdb_execute_command("x/i 0x%llx", (long long)disasm_info.addr);
    	 current_pc_lines = pc_lines + 1;
    } else
    	current_pc_lines = pc_lines;
    	
    gdb_close_output(disasm_info.stream);	/* close both redirections we defined	*/
    gdb_close_output(redirect_stdout);
    
    /* As lines were disassembled the flags were set to indicate whether the instruction*/
    /* at $pc was a conditional branch and whether that branch will be taken.  We 	*/
    /* display that information here at the right end of the 1st disassembly line.	*/ 
    
    if (disasm_info.flags & (BRANCH_TAKEN|BRANCH_NOT_TAKEN)) {
	screen_fprintf(stderr, PC_AREA, GOTO CLEAR_LINE, pc_top, max_cols - 19);
	if (disasm_info.flags & BRANCH_TAKEN)
	    screen_fprintf(stdout, PC_AREA, COLOR_BLUE " (Will branch)     " COLOR_OFF);
	else if (disasm_info.flags & BRANCH_NOT_TAKEN)
	    screen_fprintf(stdout, PC_AREA, COLOR_BLUE " (Will not branch) " COLOR_OFF);
    }
    
    screen_fflush(stdout, PC_AREA);
}


#define NSArgv_Use_Redirect 0		/* controls which technique we use to get it	*/

#if NSArgv_Use_Redirect
/*-----------------------------------------------------------------*
 | check_for_NSArgv - filter to capture value ((char **)NXArgv)[0] |
 *-----------------------------------------------------------------*
 
 This is both a stdout and stderr output redirection filter for get_CurApName() below.
 It issues the command,
 
     printf "%s", ((char **)NXArgv)[0]
 
 whose output is filtered here.  If its stderr output we know we couldn't get the
 value so we set (char *)data to a null string.  If the output is to stdout then we 
 know we were successful and we have gotten the app name.  So we extract the root and
 copy it to (char *)data.  Of course get_CurApName() set up the data to point to a buffer
 when it created the redirection stream.
*/

static char *check_for_NSArgv(FILE *f, char *line, void *data)
{
    char *p;
    
    if (line)						/* skip flush operations	*/
	if (f == stderr)				/* if output to stderr...	*/
	    *(char *)data = '\0';			/* ...couldn't get value	*/
	else {						/* if output to stdout...	*/
	    p = strrchr(line, '/');			/* ...save root name		*/
	    p = (p == NULL) ? line : p + 1;
	    strcpy((char *)data, p);
	}
	
    return (NULL);
}
#endif


/*-------------------------------------------------------------------*
 | get_CurApName - get the current app name for the side bar display |
 *-------------------------------------------------------------------*
 
 This is used to get the current app name (and return a pointer to it -- it's kept in the
 curApName buffer statically defined in this file) of the program being run.  This is
 displayed in the side bar.
 
 The current app name should be in ((char **)NXArgv)[0] which is defined in c/c++ startup
 code.  But we don't take any chances.  We get the value by printing it and capturing
 the stdout/stderr output.  If we get stdout output we got it.  If stderr we don't.
 
 Implementation note: This used to be done from our run command handler and used to work
 in gdb 4.x.  But with gdb 5 it stopped working.  Apparently gdb is not yet ready to
 access NXArgv and errors out.  Error recovery at that time is also different so we get
 prompt problems.  The solution was to delay it until as late as possible, i.e., when the
 side bar needs it.  But that time we're well "up" and running.
*/
 
static char *get_CurApName(char *curApName)
{
    GDB_FILE *redirect_stdout, *redirect_stderr;
    
    #if NSArgv_Use_Redirect
    
    redirect_stdout = gdb_open_output(stdout, check_for_NSArgv, curApName);
    redirect_stderr = gdb_open_output(stderr, check_for_NSArgv, curApName);
    gdb_redirect_output(redirect_stdout);
    gdb_redirect_output(redirect_stderr);
    
    gdb_execute_command("printf \"%%s\", ((char **)NXArgv)[0]");
    
    gdb_close_output(redirect_stderr);
    gdb_close_output(redirect_stdout);
    
    #else
    
    /* Attempt a "set $__CurApName__=((char **)NXArgv)[0]".  If it succeeds, use	*/
    /* $__CurApName__ to set curApName...						*/
    
    /* Historical note:									*/
    
    /* The following code used to fail (sometimes) in gdb unless we did the command set	*/
    /* use-cached-symfiles 0.  It seemed the access to NXArgv is accessing incorrectly	*/
    /* mapped memory (a gdb bug).  We needed to do the set use-cached-symfiles 0 prior	*/
    /* to any FILE of ATTACH command.  So we did it during initialization. The .gdbinit	*/
    /* would equally be an ok place.							*/
    
    /* Time passes...									*/
    
    /* It looks like the problem has been fixed in gdb so the set command was removed	*/
    /* (actually commented out to keep it as a placeholder).				*/
    
    if (!gdb_eval_silent("$__CurApName__=((char**)NXArgv)[0]")) {
	char *p, *q, pathname[MAXPATHLEN+1];
	p = strrchr(q = gdb_get_string("$__CurApName__", pathname, MAXPATHLEN), '/');
	strcpy(curApName, p ? p + 1 : q);
	curApName_title = (target_arch == 4) ? " CurApName  " : "     CurApName      ";
    } else {
	int pid = gdb_target_pid();
	if (pid <= 0) {
	    *curApName = '\0';
	    curApName_title = (target_arch == 4) ? "            " : "                    ";
	} else {
	    sprintf(curApName, "%d", pid);
	    curApName_title = (target_arch == 4) ? "    PID     " : "        PID         ";
	}
    }
    
    #endif

    return (curApName);
}


/*------------------------------------------------------*
 | save_stack - save current top N entries of the stack |
 *------------------------------------------------------*
 
 This is called to set the prev_stack[] array with the top N entries of the stack.  N
 is a function of the max number of screen rows since this array is intended for 
 coloring the sidebar stack display.
 
 Note, it is called usually by save_all_regs().  But it will also be called when the
 window size changes since that operation affects the number of screen rows.
*/

void save_stack(int max_rows)
{
    int i;
    
    prev_stack_cnt = max_rows - MIN_SIDEBAR;
    i = prev_stack_cnt * sizeof(unsigned long);
    if (i <= 0) {
    	if (prev_stack) {
	    gdb_free(prev_stack);
	    prev_stack = NULL;
	    gdb_free(prev_stack_color);
	    prev_stack_color = NULL;
	}
    } else if (prev_stack == NULL)
    	prev_stack = (unsigned long *)gdb_malloc(i);
    else
    	prev_stack = (unsigned long *)gdb_realloc(prev_stack, i);
    
    if (prev_stack)
    	gdb_read_memory_from_addr(prev_stack, gdb_get_sp(), i, 1);
}


/*------------------------------------------------------*
 | save_all_regs - save current values of all registers |
 *------------------------------------------------------*

 Called to remember the values of the registers in the previous display.  Also records
 the values of the top N stack entries, where N is determined by how many will fit in
 the sidebar (could be 0) so the when the general registers are shown r31 will always
 end up on the last screen row.
*/

static void save_all_regs(void)
{
    int  i;
    char r[6];
    
    gdb_get_register("$pc",  &prev_pc);		/* save current register values		*/
    gdb_get_register("$lr",  &prev_lr);
    gdb_get_register("$ctr", &prev_ctr);
    gdb_get_register("$xer", &prev_xer);
    gdb_get_register("$cr",  &prev_cr.cr);
    //gdb_get_register("$mq",  &prev_mq);
    gdb_get_register("$ps",  &prev_msr);
    
    for (i = 0; i < 32; ++i) {
	sprintf(r, "$r%d", i);
	gdb_get_register(r, &prev_gpr[i]);
    }
    
    save_stack(max_rows);
}


/*----------------------------------------------------------------------*
 | __display_side_bar [left | right] - display the registers vertically |
 *----------------------------------------------------------------------*
 
 If "left" or nothing is specified the registers are displayed on the left side of the
 screen using the entire left side.  If "right" is specified they are displayed on the
 right side of the screen using the entire right side.

 The "left" and "right" displays has has the following general layout:
 
      SP     |                                                            |     SP
   12345678  |                                                            |  12345678
 xxx 12345678|                                                            |xxx 12345678
 xxx 12345678|                                                            |xxx 12345678
 xxx 12345678|                                                            |xxx 12345678
 xxx 12345678|                                                            |xxx 12345678
	     |                                                            |
  CurApName  |                                                            | CurApName
 12345678901╔|                                                            |12345678901╔
	     |                                                            |
 LR  12345678|                                                            |LR  12345678
 CR  12345678|                                                            |CR  12345678
	     |                                                            |
 CTR 12345678|                                                            |CTR 12345678
 XER 12345678|                                                            |XER 12345678
	     |                                                            |
 R0  12345678|                                                            |R0  12345678
 SP  12345678|                                                            |SP  12345678
 R2  12345678|                                                            |R2  12345678
     ...     |                                                            |    ...
 R31 12345678|                                                            |R31 12345678
 
 Or for the 64-bit architecture:
 
          SP         |                                            |         SP
   1234567812345678  |                                            |  1234567812345678
    xxx 12345678     |                                            |    xxx 12345678
    xxx 12345678     |                                            |    xxx 12345678
    xxx 12345678     |                                            |    xxx 12345678
    xxx 12345678     |                                            |    xxx 12345678
	             |                                            |
      CurApName      |                                            |     CurApName
 1234567890123456789╔|                                            |1234567890123456789╔
	             |                                            |
 LR  1234567812345678|                                            |LR  1234567812345678
 CR  12345678        |                                            |CR  12345678
	             |                                            |
 CTR 1234567812345678|                                            |CTR 1234567812345678
 XER 1234567812345678|                                            |XER 1234567812345678
	             |                                            |
 R0  1234567812345678|                                            |R0  1234567812345678
 SP  1234567812345678|                                            |SP  1234567812345678
 R2  1234567812345678|                                            |R2  1234567812345678
     ...             |                                            |    ...
 R31 1234567812345678|                                            |R31 1234567812345678
 
 Of course only the left or right sidebar is generated depending on the argument and the
 32/64 format depends on the global target_arch (4 or 8).  The number of top of stack
 entries is enough so that with "SP" at the top of the screen, "R31" is at the bottom.
 Only the 3 low-order hex digits of the stack address are shown with the stack entries.
 Lines of the stack display are sacrificed or added as needed to accommodate the screen
 row size so that R31 is always at the bottom of the screen.  At a minimum there may be
 no stack entries shown at all (but the "SP" title and it's value are always shown).
 
 Note, this is in the form of a gdb plugin command because it can be called by the
 __disasm command.  So it's exposed to the gdb command language as a "internal" command
 (hence the double underbar prefix just like __disasm).
*/

void __display_side_bar(char *arg, int from_tty)
{
    int           i, row, col, reg, saved_regs, lastcmd, left_col, bottom,
    		  cur_app_name, force_all_updates, changed, stack_rows;
    GDB_ADDRESS   pc, lr, ctr, msr, xer, mq, gpr[32];
    unsigned long *stack, xer32;
    char          *bar_left, *bar_right, r[6], centered_name[21], *blanks;
    
    union {					/* gdb_get_register() could return a	*/
    	unsigned long long cr_err;		/* error code as a long long.		*/
    	unsigned long cr;			/* but the cr is always only 32-bits	*/
    } cr;
    
    static char prev_sp_color, prev_pc_color, prev_lr_color, prev_ctr_color,
    		prev_msr_color, prev_cr_color, prev_xer_color, prev_mq_color,
		prev_gpr_color[32];
		
    static char curApName[MAXPATHLEN+1]      = {0};	/* CurApName (for the side bar)	*/
    static char prev_curApName[MAXPATHLEN+1] = {0};	/* previous CurApName		*/

    #if 0
    #define BAR COLOR_BOLD "|" COLOR_OFF
    #else
    #define BAR COLOR_OFF "|" 
    #endif
    
    if (!isatty(STDOUT_FILENO))			/* if we aren't writting to a terminal	*/
    	return;					/* ...what else can we do?		*/
    	
    get_screen_size(&max_rows, &max_cols);
    screen_fprintf(stdout, SIDE_BAR, SAVE_CURSOR);/* remember current cursor postion	*/
    
    if (need_CurApName) {			/* if 1st side bar after run command...	*/
    	need_CurApName = !gdb_target_running();	/* ...reset switch if target is running	*/
	get_CurApName(curApName);		/* ...and get the (new?) app name	*/
    }
    
    /* Check argument to see if regs go on the left[min] or right[min]...		*/
        
    if (!arg || !*arg || gdb_strcmpl(arg, "left")) {
    	if (!arg || !*arg) {
	    force_all_updates = 0;
	    if (!macsbug_screen)
		first_sidebar = 1;
	} else
	    force_all_updates = 1;
    	left_col  = side_bar_left;
	bar_left  = "";
	bar_right = BAR;
	blanks    = side_bar_blanks;
    } else if (gdb_strcmpl(arg, "right")) {
    	force_all_updates = 1;
	left_col  = max_cols - side_bar_right;
	bar_left  = BAR;
	bar_right = "";
	blanks    = CLEAR_LINE;
    } else
	gdb_error("__display_side_bar [left | right] expected, got \"%s\"", arg);
    
    bottom = max_rows;
    
    /* Not running is a special case...							*/
    
    if (!gdb_have_registers() || !gdb_target_running()) {
    	if (prev_running == 1) {
    	    prev_running = 0;
            for (row = 1; row <= bottom; ++row) {
    	    	screen_fprintf(stdout, SIDE_BAR, GOTO, row, left_col);
    	        if (row == 1)
    		    screen_fprintf(stdout, SIDE_BAR, "%s" COLOR_RED "%s" "%s", bar_left, 
    		    		    (target_arch == 4) ? "Not Running " : "    Not Running     ", bar_right);
    	    	else
    		    screen_fprintf(stdout, SIDE_BAR, "%s" "%s" "%s", bar_left, blanks, bar_right);
    	    }
    	}
	screen_fprintf(stdout, SIDE_BAR, RESTORE_CURSOR);
	screen_fflush(stdout, SIDE_BAR);
	first_sidebar = 1;
    	return;
    }
    
    prev_running = 1;
    
    /* Get current register values...							*/
    
    if (first_sidebar)			/* first time all regs are the "same"...	*/
	save_all_regs();
    
    if (force_all_updates)
    	first_sidebar = force_all_updates;
    
    gdb_get_register("$pc",  &pc); 	/* get current register values...		*/
    gdb_get_register("$lr",  &lr);
    gdb_get_register("$ctr", &ctr);
    gdb_get_register("$xer", &xer);
    gdb_get_register("$cr",  &cr.cr);	/* note, cr is always a 32-bit value		*/
    //gdb_get_register("$mq",  &mq);
    gdb_get_register("$ps",  &msr);
    
    for (i = 0; i < 32; ++i) {
	sprintf(r, "$r%d", i);
	gdb_get_register(r, &gpr[i]);
    }
     
    row = 1;
   
    /* Display the top N stack entries - enough so that the general regs r0-r31 will	*/
    /* always end up so that r31 is on the last screen row.				*/
    
    stack_rows = max_rows - MIN_SIDEBAR;/* get the top stack entries we need...		*/
    if (stack_rows != prev_stack_cnt)	
    	gdb_internal_error("Window size inconsistency dealing with number of stack entries");
    if (stack_rows > 0) {		
    	stack = (unsigned long *)gdb_malloc(stack_rows * sizeof(unsigned long));
    	gdb_read_memory_from_addr(stack, gdb_get_sp(), (stack_rows * sizeof(unsigned long)), 1);
	if (first_sidebar) {
	    prev_stack_color = (char *)gdb_malloc(stack_rows);
	    memset(prev_stack_color, 0, stack_rows);
	}
    } else {
    	stack = NULL;
	if (prev_stack_color) {
	    gdb_free(prev_stack_color);
	    prev_stack_color = NULL;
	}
    }
    
    if (target_arch == 4) {
	if (first_sidebar)
	    screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "     SP     " "%s", row, left_col,
	    		   	bar_left, bar_right);
	++row;
	
	changed = (prev_gpr[1] != gpr[1]);
	if (first_sidebar || changed || changed != prev_sp_color)
	    screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "  " "%s" "%.8lX" "  " "%s", row, left_col,
				   bar_left, COLOR_CHANGE(prev_sp_color = changed), 
				   (unsigned long)gpr[1], bar_right);
	++row;
	
	for (i = 0; i < stack_rows; ++i) {
	    changed = (prev_stack[i] != stack[i]);
	    if (first_sidebar || changed || changed != prev_stack_color[i])
		screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "%.3lX %s%.8lX" "%s", row, left_col,
				    bar_left, ((unsigned long)gpr[1] + 4*i) & 0xFFF, 
				    COLOR_CHANGE(prev_stack_color[i] = changed), stack[i],
				    bar_right);
	    ++row;
	}
    } else {
	if (first_sidebar)
	    screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "         SP         " "%s", row, left_col,
	    			bar_left, bar_right);
	++row;
	
	changed = (prev_gpr[1] != gpr[1]);
	if (first_sidebar || changed || changed != prev_sp_color) {
	    screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "  " "%s" "%.16llX" "  " "%s", row, left_col,
				   bar_left, COLOR_CHANGE(prev_sp_color = changed), 
				   (unsigned long long)gpr[1], bar_right);
	   if (!*bar_right)
	       screen_fprintf(stdout, SIDE_BAR, CLEAR_LINE);
	}
	++row;
	
	for (i = 0; i < stack_rows; ++i) {
	    changed = (prev_stack[i] != stack[i]);
	    if (first_sidebar || changed || changed != prev_stack_color[i])
		screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "    %.3llX %s%.8lX%s" "%s", row, left_col,
				    bar_left, ((unsigned long long)gpr[1] + 4*i) & 0xFFF, 
				    COLOR_CHANGE(prev_stack_color[i] = changed), stack[i],
				    *bar_right ? "    " : CLEAR_LINE, bar_right);
	    ++row;
	}
    }
    
    if (stack)
    	gdb_free(stack);
    
    if (first_sidebar)
    	screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "%s" "%s", row, left_col,
    				bar_left, blanks, bar_right);
    ++row;

    /* Print the CurApName if we have it...						*/
    
    if (first_sidebar || (*curApName && strcmp(curApName, prev_curApName) != 0)) {
    	strcpy(prev_curApName, curApName);
	if (*curApName) {
	    screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "%s" "%s", row++, left_col,
	    			bar_left, curApName_title, bar_right);
	    if ((i = strlen(curApName)) > side_bar_right) {
		char c1 = curApName[side_bar_right - 1];
		char c2 = curApName[side_bar_right];
		curApName[side_bar_right - 1] = '╔';
		curApName[side_bar_right] = '\0';
		screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "%s" "%s", row++, left_col,
					bar_left, curApName, bar_right);
		curApName[side_bar_right - 1] = c1;
		curApName[side_bar_right] = c2;
	    } else if (i == side_bar_right)
		screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "%s" "%s", row++, left_col,
					bar_left, curApName, bar_right);
	    else {
		memset(centered_name, ' ', side_bar_right);
		centered_name[side_bar_right] = '\0';
		memcpy(&centered_name[(side_bar_right - i)/2], curApName, i);
		screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "%s" "%s", row++, left_col,
					bar_left, centered_name, bar_right);
	    	if (!*bar_right)
	    	    screen_fprintf(stdout, SIDE_BAR, CLEAR_LINE);
	    }
    	} else {				/* no curApName, blank title and name	*/
    	    screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "%s" "%s", row++, left_col,
    	    				bar_left, blanks, bar_right);
    	    screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "%s" "%s", row++, left_col,
    	    				bar_left, blanks, bar_right);
	}
	screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "%s" "%s", row++, left_col,
				bar_left, blanks, bar_right);
    } else
    	row += 3;
    
    /* Print the special registers, in red if they different from previous...		*/
    
    changed = (prev_lr != lr);
    if (first_sidebar || changed || changed != prev_lr_color)
    	if (target_arch == 4)
	  screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "LR  " "%s" "%.8lX" "%s", row, left_col,
			   bar_left, COLOR_CHANGE(prev_lr_color = changed), (unsigned long)lr, bar_right);
	else
	  screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "LR  " "%s" "%.16llX" "%s", row, left_col,
			   bar_left, COLOR_CHANGE(prev_lr_color = changed), (unsigned long long)lr, bar_right);
    ++row;
    
    changed = (prev_cr.cr != cr.cr);
    if (first_sidebar || changed || changed != prev_cr_color) {
    	prev_cr_color = changed;
    	screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "CR  ", row, left_col, bar_left);
    	for (i = 28; i >= 0; i -= 4)
    	    screen_fprintf(stdout, SIDE_BAR, "%s" "%.1X", COLOR_CHANGE((prev_cr.cr>>i&15) != (cr.cr>>i&15)), (cr.cr>>i&15));
    	if (*bar_right) {
	    if (target_arch != 4)
		screen_fprintf(stdout, SIDE_BAR, "        ");
    	    screen_fprintf(stdout, SIDE_BAR, "%s", bar_right);
    	} else
    	    screen_fprintf(stdout, SIDE_BAR, CLEAR_LINE);
    }
    ++row;
    
    if (first_sidebar)
    	screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "%s" "%s", row, left_col,
    				bar_left, blanks, bar_right);
    ++row;

    if (target_arch == 4) {
	changed = (prev_ctr != ctr);
	if (first_sidebar || changed || changed != prev_ctr_color)
	    screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "CTR " "%s" "%.8lX" "%s", row, left_col, bar_left,
			    COLOR_CHANGE(prev_ctr_color = changed), (unsigned long)ctr, bar_right);
	++row;
    } else {
	changed = (prev_ctr != ctr);
	if (first_sidebar || changed || changed != prev_ctr_color)
	    screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "CTR " "%s" "%.16llX" "%s", row, left_col, bar_left,
			    COLOR_CHANGE(prev_ctr_color = changed), (unsigned long long)ctr, bar_right);
	++row;
    }
    
    #if 0
    changed = (prev_msr != msr);
    if (first_sidebar || changed || changed != prev_msr_color)
    	screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "MSR " "%s" "%.8X" "%s", row, left_col, bar_left,
    			COLOR_CHANGE(prev_msr_color = changed), msr, bar_right);
    ++row;
    
    changed = (prev_mq != mq);
    if (first_sidebar || changed || changed != prev_mq_color)
    	screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "MQ  " "%s" "%.8X" "%s", row, left_col, bar_left,
    			COLOR_CHANGE(prev_mq_color = changed), mq , bar_right);
    ++row;
    #endif
    
    changed = (prev_xer != xer);
    if (first_sidebar || changed || changed != prev_xer_color) {
    	prev_xer_color = changed;
    	xer32 = (unsigned long)xer;
    	screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "XER ", row, left_col, bar_left);
    	if (target_arch != 4)
    	    screen_fprintf(stdout, SIDE_BAR, "%.8lX", (unsigned long)((xer >> 32) & 0xFFFFFFFF));
    	screen_fprintf(stdout, SIDE_BAR, "%s" "%.1X" COLOR_OFF, COLOR_CHANGE((prev_xer>>28&0xF) != (xer32>>28&0xF)), xer32>>28&0xF);
    	screen_fprintf(stdout, SIDE_BAR, "%.3X", (xer32>>16&0xFFF));
    	screen_fprintf(stdout, SIDE_BAR, "%s" "%.2X", COLOR_CHANGE((prev_xer>>8&0xFF) != (xer32>>8&0xFF)), xer32>>8&0xFF);
    	screen_fprintf(stdout, SIDE_BAR, "%s" "%.2X", COLOR_CHANGE((prev_xer&0xFF) != (xer32&0xFF)), xer32&0xFF);
   	if (*bar_right)
	    screen_fputs(bar_right, stdout, SIDE_BAR);
    }
    ++row;
    
    if (first_sidebar)
    	screen_fprintf(stdout, SIDE_BAR, GOTO "%s" "%s" "%s", row, left_col,
    				bar_left, blanks, bar_right);
    ++row;
    
    /* Print the general regs at the bottom...						*/
    
    for (reg = 0; reg < 32; ++reg) {
    	changed = (prev_gpr[reg] != gpr[reg]);
    	if (first_sidebar || changed || changed != prev_gpr_color[reg]) {
       	    screen_fprintf(stdout, SIDE_BAR, GOTO "%s", row, left_col, bar_left);
       	    if (reg == 1)
       	    	screen_fprintf(stdout, SIDE_BAR, "SP  ");
       	    else
       	    	screen_fprintf(stdout, SIDE_BAR, "R%-2d ", reg);
       	    if (target_arch == 4)
		screen_fprintf(stdout, SIDE_BAR, "%s" "%.8lX" "%s", 
			       COLOR_CHANGE(prev_gpr_color[reg] = changed),
			       (unsigned long)gpr[reg], bar_right);
    	    else
		screen_fprintf(stdout, SIDE_BAR, "%s" "%.16llX" "%s", 
			       COLOR_CHANGE(prev_gpr_color[reg] = changed),
			       (unsigned long long)gpr[reg], bar_right);
    	}
    	++row;
    }
    
    screen_fprintf(stdout, SIDE_BAR, COLOR_OFF);
    
    screen_fprintf(stdout, SIDE_BAR, RESTORE_CURSOR);
    screen_fflush(stdout, SIDE_BAR);
    
    /* Save all the regs to detect differences the next time we call this routine...	*/
    
    if (!first_sidebar || force_all_updates) {
    	save_all_regs();
	if (force_all_updates)
	    first_sidebar = 0;
    } else 
	first_sidebar = 0;
}

#define __DISPLAY_SIDE_BAR_HELP \
"__DISPLAY_SIDE_BAR [left | right] -- display registers vertically.\n"			\
"Used to display the machine registers in a side bar vertically on the right\n"		\
"(\"right\") or left (\"left\") side of the screen. The default is the same as\n"	\
"specifying \"left\".\n"								\
"\n"											\
"This command is called as \"__DISPLAY_SIDE_BAR right\" when __DISASM is used\n"	\
"and the MacsBug screen is off or as \"__DISPLAY_SIDE_BAR\" (implied \"left\")\n"	\
"when the MacsBug screen is on.  Obviously you shouldn't use \"left\" when the\n"	\
"MacsBug screen is off since the last line will be clobbered by gdb's prompt.\n"	\
"It should Also be obvious that it shouldn't be explicitly called at all when\n"	\
"the MacsBug scren is on.\n"								\
" \n"											\
"Note, for each call to __DISPLAY_SIDE_BAR, the registers that changed since\n"		\
"the last call will be shown in red.  Also displayed is the target program name\n"	\
"being debugged (if available) and the top few entries of the stack."


/*---------------------------------------------------------------*
 | init_sidebar_and_pc_areas - init for new sidebar and pc areas |
 *---------------------------------------------------------------*
 
 This is called whenever we want to make sure we completely redraw the sidebar and
 pc areas.  For example, when a REFRESH is done or a RUN is done.
*/

void init_sidebar_and_pc_areas(void)
{
    previous_pc   = 0;
    prev_running  = 1;
    first_sidebar = 1;
    
    if (prev_stack) {
    	gdb_free(prev_stack);
	prev_stack = NULL;
    }
    
    if (prev_stack_color) {
    	gdb_free(prev_stack_color);
    	prev_stack_color = NULL;
    }
    
    prev_stack_cnt = 0;
}


/*---------------------------------------------------------------------------------------*
 | define_macsbug_screen_positions - define globals controlling MacsBug screen positions |
 *---------------------------------------------------------------------------------------*

 A MacsBug screen sort of looks like the following:

		     --------------------------------------------
		     |  r  |                                    |
		     |  e  |                                    |
		     |  g  |                                    |
		     |  i  |                                    |
		     |  s  |                                    |
		     |  t  |            history area            |
		     |  e  |                                    |
		     |  r  |                                    |
		     |     |                                    |
		     |  s  |                                    |
		     |  i  |                                    |
		     |  d  |------------------------------------|
		     |  e  |              pc area               |
		     |  b  |                                    |
		     |  a  |------------------------------------|
		     |  r  |         command line area          |
		     --------------------------------------------
  
 Based in the screen size and the number of lines in the pc area and the number of lines
 in the command line area we define the following:

   history_top, history_left,		these define the position of the history area
   history_bottom, history_right,
   history_lines, history_wrap

   pc_top, pc_left, pc_wrap,		these define the position of the pc area
   pc_bottom, pc_right, pc_lines

   cmd_top, cmd_left,			these define the position of the cmd line area
   cmd_bottom, cmd_right
   
   side_bar_top, side_bar_left,		these define the position of the reg. side bar
   side_bar_bottom, side_bar_right
   
   vertical_divider, pc_top_divider,	these define the divider lines positions
   pc_bottom_divider
 
 All values are 1-relative.
 
 Divider horizontal lines will always be placed at the top and bottom of the pc area
 and a vertical line separating the side bar.  Thus the pc top line is at row pc_top-1 
 (same as history_bottom+1) and the pc bottom line is at pc_bottom+1 (same as cmd_top-1).
 The side bar always is always at the left and extends vertically down the entire screen.
 Thus side_bar_top = 1, side_bar_left = 1, side_bar_bottom = screen_rows.  The side bar
 is a minimum width to display the registers with a 3-character identifier (e.g.,
 "MSR xxxxxxxx".  That's 12 characters.  Thus side_bar_right is always 12 and the
 vertical line is thus side_bar_right+12 which defines history_left, pc_left, and
 cmd_left as side_bar_right+2.
 
 The pc area is to be pc_lines long (not counting its divider lines at the top and
 bottom).  This dictates the top and bottom values for the history and command line
 areas.
 
 The command line area is cmd_area_lines + 1 lines long.  The extra line is to allow
 for a possible newline so it doesn't cause scrolling of the screen.  Of course the
 cmd_area_lines dictates the bottom of the pc area and the pc_lines in turn the bottom
 of the history area.
*/
 
void define_macsbug_screen_positions(short pc_area_lines, short cmd_area_lines)
{
    get_screen_size(&max_rows, &max_cols);
    
    side_bar_top      = 1;			/* side bar first 12 characters	of the	*/
    side_bar_bottom   = max_rows;		/* entire screen (20 fo 64 bit)		*/
    side_bar_left     = 1;
    side_bar_right    = (target_arch == 4) ? 12 : 20;
    
    cmd_top	      = max_rows-cmd_area_lines;/* cmd area is cmd_area_lines lines long*/
    cmd_bottom        = max_rows;		/* 2 lines for "long" commands and 1 	*/
    cmd_left          = side_bar_right + 2;	/* for the return typed to enter command*/
    cmd_right         = max_cols;
    
    pc_bottom	      = cmd_top - 2;		/* pc area is 2 lines up from cmd area	*/
    pc_top	      = pc_bottom-pc_area_lines;/* pc area is pc_lines+1 to allow for 	*/
    pc_left	      = side_bar_right + 2;	/* the function name + pc_lines lines	*/
    pc_right	      = max_cols;
    pc_wrap	      = max_cols - pc_left;
    
    history_top	      = 1;			/* history area is what's left		*/
    history_bottom    = pc_top - 2;
    history_left      = side_bar_right + 2;
    history_right     = max_cols;
    
    vertical_divider  = side_bar_right + 1;	/* divider lines separate the areas	*/
    pc_top_divider    = pc_top - 1;
    pc_bottom_divider = pc_bottom + 1;
    
    pc_lines	      = pc_area_lines;
    cmd_lines	      = cmd_area_lines;
    history_lines     = history_bottom - history_top + 1;
    history_wrap      = max_cols - history_left;
        
    #if 0
    gdb_printf("--------------------------------\n");
    gdb_printf("side_bar_top      = %d\n", side_bar_top);
    gdb_printf("side_bar_bottom   = %d\n", side_bar_bottom);
    gdb_printf("side_bar_left     = %d\n", side_bar_left);
    gdb_printf("side_bar_right    = %d\n\n", side_bar_right);
    
    gdb_printf("cmd_top           = %d\n", cmd_top);
    gdb_printf("cmd_bottom        = %d\n", cmd_bottom);
    gdb_printf("cmd_left          = %d\n", cmd_left);
    gdb_printf("cmd_right         = %d\n\n", cmd_right);
    
    gdb_printf("pc_bottom         = %d\n", pc_bottom);
    gdb_printf("pc_top            = %d\n", pc_top);
    gdb_printf("pc_left           = %d\n", pc_left);
    gdb_printf("pc_right          = %d\n\n", pc_right);
    gdb_printf("pc_wrap           = %d\n\n", pc_wrap);
    
    gdb_printf("history_top       = %d\n", history_top);
    gdb_printf("history_bottom    = %d\n", history_bottom);
    gdb_printf("history_left      = %d\n", history_left);
    gdb_printf("history_right     = %d\n\n", history_right);
    
    gdb_printf("vertical_divider  = %d\n", vertical_divider);
    gdb_printf("pc_top_divider    = %d\n", pc_top_divider);
    gdb_printf("pc_bottom_divider = %d\n\n", pc_bottom_divider);
    
    gdb_printf("pc_lines          = %d\n", pc_lines);
    gdb_printf("cmd_lines         = %d\n", cmd_lines);
    gdb_printf("history_lines     = %d\n", history_lines);
    gdb_printf("history_wrap      = %d\n", history_wrap);
    #endif
}


/*------------------------------------------------------------------------------*
 | refresh [p] [c] - redraw the MacsBug screen for p pc area lines, c cmd lines |
 *------------------------------------------------------------------------------*
 
 The default is to use the SET values pc_area_lines and cmd_area_lines.  All the full
 screen redrawing is initiated from here.  So this is not only called for the REFRESH
 command but also for initial setup, and signal recovery as well.
*/
 
void refresh(char *arg, int from_tty)
{
    int     argc, pc_lines, cmd_lines, i, old_prompt_start;
    History *h;
    char    *argv[5], set_prompt_cmd[1024], old_prompt[1024], tmpCmdLine[1024];;
    
    if (!macsbug_screen)
    	return;
   
    if (arg && *arg) {
    	gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "refresh", &argc, argv, 4);
	if (argc > 3)
	    gdb_error("refresh called with wrong number of arguments.");
	pc_lines = gdb_get_int(argv[1]);
	if (pc_lines < MIN_PC_LINES || pc_lines > MAX_PC_LINES)
	    gdb_error("screen pc area must be %d to %d lines long",
	    				MIN_PC_LINES, MAX_PC_LINES);
	if (argc == 3) {
	    cmd_lines = gdb_get_int(argv[2]);
	    if (cmd_lines < MIN_CMD_LINES || cmd_lines > MAX_CMD_LINES)
		gdb_error("screen command line area must be %d to %d lines long",
					MIN_CMD_LINES, MAX_CMD_LINES);
	} else
	    cmd_lines = cmd_area_lines;
    } else {
    	pc_lines  = pc_area_lines;
	cmd_lines = cmd_area_lines;
    }
    
    define_macsbug_screen_positions(pc_lines, cmd_lines);
    
    sprintf(side_bar_blanks, "%*c", side_bar_right, ' ');

    screen_fprintf(stderr, NO_AREA, RESET);
    screen_init(history_top, history_left, history_bottom, history_right);
    
    init_sidebar_and_pc_areas();
    
    #if BUFFER_OUTPUT
    bp = output_buffer;
    buf_row = 0;
    #endif
    
    need_CurApName = 1;
    
    __display_side_bar(NULL, 0);
    display_pc_area();
    
    /* Redisplay as much of the history as will fit in the (new) screen.  If it all	*/
    /* fits, great!  Show the bottom history_lines lines since that guides the possibly	*/
    /* new history top and bottom if the area sizes changed.				*/
    
    if (history_cnt) {
	for (i = history_lines, h = screen_bottom; i > 0 && h; h = h->prev, --i) {
	    if (h->flags & ERR_LINE)
		write_line(GOTO COLOR_RED "%s" COLOR_OFF, i, history_left, h->line);
	    else
		write_line(GOTO "%s", i, history_left, h->line);
	    END_OF_LINE;
	}
	screen_top = h ? h->next : history;
	FLUSH_BUFFER;
    }
    
    gdb_fflush(gdb_current_stdout);
    gdb_fflush(gdb_current_stderr);
    screen_refresh(1);
    
    /* Define the prompt with cursor positioning so that appears at the top left of the	*/
    /* command line area...								*/
    
    /* Need cursor setting in prompt or gdb's history (using the arrows) doesn't show	*/
    /* correctly.  It appears gdb uses the current prompt to show each of its history 	*/
    /* lines.										*/
   
    old_prompt_start = prompt_start;
    //prompt_start = sprintf(prompt, GOTO CLEAR_LINE, cmd_top, cmd_left);
    prompt_start = sprintf(prompt, GOTO, cmd_top, cmd_left);
    
    /* NOTE: The CLEAR_LINE at the need of the prompt has been removed because, for some*/
    /*       unknown reason, the input line is cleared when backspacing over the first	*/
    /*       character of the input line.  The input line needed to be 3 bytes larger	*/
    /*       than the prompt (with the GOTO CLEAR_LINE, and it's interesting that 	*/
    /*       CLEAR_LINE is "\033[0K" which is 3 bytes) for this to happen.  I think the */
    /*       problem is due to something that either rl_redisplay() or update_line() 	*/
    /*       (both in gdb's readline) are doing.  That code appears to adjust for the	*/
    /*       shortening line anyhow so it doesn't look like we need our own CLEAR_LINE 	*/
    /*       (but I sure would feel safer with it in).  But it is strange that this 	*/
    /*       problem only occurred when the line was "long enough" and only backspacing */
    /*       over the first character of the line.  It worked fine anywhere else and for*/
    /*       any length line.  Weird!  Personally I think there's an edge condition 	*/
    /*       error in either rl_redisplay() or update_line().  I think it's supposed to	*/
    /*       only redraw the portion of the line that requires redrawing.  Instead it 	*/
    /*       must be redrawing 3 bytes before and firing off our CLEAR_LINE.  But I am	*/
    /*       not sure.									*/
   
    if (old_prompt_start) {
    	gdb_get_prompt(old_prompt);
	strcpy(prompt + prompt_start, old_prompt + old_prompt_start);
    } else {
    	/* The following is done to try to protect ourselves from someone loading the	*/
	/* MacsBug plugins twice.  If MacsBug is already loaded and already set our	*/
	/* prompt with the GOTO prefix we just use actual prompt that follows it.	*/
	
	char *p1 = prompt + prompt_start;
	//char *p2 = strstr(gdb_get_prompt(p1), CLEAR_LINE);
	char *p2 = strstr(gdb_get_prompt(p1), GOTO);
	
	if (p2)
	    strcpy(p1, p2 + strlen(GOTO));
	    //strcpy(p1, p2 + strlen(CLEAR_LINE));
    }

    sprintf(set_prompt_cmd, "set prompt %s", prompt);
    doing_set_prompt = 1;
    gdb_execute_command(set_prompt_cmd);
    doing_set_prompt = 0;
}

#define REFRESH_HELP \
"REFRESH -- Refresh MascBug screen."


/*----------------------------------------------------------------------------------*
 | restore_current_prompt - reset the gdb prompt to the most recent setting we know |
 *----------------------------------------------------------------------------------*
 
 This is called when gdb is given back control from a signal handler.  For some reason
 the gdb prompt we are using for the macsbug screen gets clobbered by the sequence of
 events that occur at this time.  So this is called to force it back to the last know
 (and valid) value we are using for the prompt.
*/

void restore_current_prompt(void)
{
    char set_prompt_cmd[1024];
    
    if (macsbug_screen) {
	sprintf(set_prompt_cmd, "set prompt %s", prompt);
	doing_set_prompt = 1;
	gdb_execute_command(set_prompt_cmd);
	doing_set_prompt = 0;
    }
}

/*-------------------------------------------------------------------------------------*
 | force_pc_area_update - force the pc area to redisplay even if the pc hasn't changed |
 *-------------------------------------------------------------------------------------*
 
 Normally the pc area is not updated unless the pc has changed.  Call this routine to
 force the update anyhow.  This is used, for example, when there is a breakpoint setting
 change to one of the instructions shown in the pc area.
*/

void force_pc_area_update(void)
{
    previous_pc = 0;
    //display_pc_area();
}


/*------------------------------------------------------------------------------------*
 | fix_pc_area_if_necessary - update MacsBug screen pc area if anything changes there |
 *------------------------------------------------------------------------------------*
 
 This checks the address to see if it's in the range of the addresses currently in the
 pc area (if it currently being display) and forces the pc area to be redisplayed if
 it is.  This will show the changing status of, for example, breakpoints at the address.
 
 Note that we don't use the current pc_area_lines setting since it could be one line
 smaller than what's actually displayed there.  This can happen if there is no symbol
 to display.  We use that line normally reserved for the symbol for an extra disassembly
 line instead (why waste it with a blank line?).  The global current_pc_lines reflects
 the actual number of lines displayed (it's pc_area_lines or pc_area_lines+1).
*/

void fix_pc_area_if_necessary(GDB_ADDRESS address)
{
    GDB_ADDRESS pc;
    int         sz;
    
    if (macsbug_screen && gdb_target_running()) {
    	/* note, get_selected_frame() done as part of gdb_get_register() */
    	gdb_get_register("$pc", &pc);
	if (address >= pc && address < pc + (4*current_pc_lines))
	    force_pc_area_update();
    }
}


/*-------------------------------------------------------------------------------------*
 | update_macsbug_prompt - convert a normal prompt string to the MacsBug screen prompt |
 *-------------------------------------------------------------------------------------*

 The is called whenever the prompt string has been changed by a SET prompt (except when
 we're doing it ourselves, e.g., like we do here -- prevents some nasty recursion!).
 Here we modify the prompt to include our cursor positioning and line clearing.  We
 use a SET prompt here too to ensure all the strings that might be lying around in gdb
 get appropriately changed (tried using the pointer that SET gives -- didn't work).
*/
 
void update_macsbug_prompt(void)
{
    char *p, set_prompt_cmd[1024], prompt[1024];
    
    if (!doing_set_prompt && macsbug_screen) {
	//prompt_start = sprintf(prompt, GOTO CLEAR_LINE, cmd_top, cmd_left);
	prompt_start = sprintf(prompt, GOTO, cmd_top, cmd_left);
	gdb_get_prompt(prompt + prompt_start);
	sprintf(set_prompt_cmd, "set prompt %s", prompt);
	doing_set_prompt = 1;
	gdb_execute_command(set_prompt_cmd);
	doing_set_prompt = 0;
    }
}


/*------------------------------------------------------------------------------*
 | position_history_prompt - position history search prompt on the command line |
 *------------------------------------------------------------------------------*
 
 When the Macsbug screen is displayed and the user types CTLR-R then gdb displays the
 prompt "(reverse-i-search)" preceded by a '\r'.  This will effectively clobber that
 part of our side-bar if we don't modify the prompt to properly position it to the
 command line area.  So that's what we do here.
 
 This is a Gdb_History_Prompt special event which is called while in the gdb history
 search mode.  The gdb prompt (for example, "(reverse-i-search)") is prefixed with 
 the appropriate prompt cursor positioning so that even if gdb prefixes it with a
 '\r' the prompt will still end up where we want it.
 
 Remember, this routine is called just before echoing EACH character while in the
 history search mode.  So don't get too carried away with what we need to do in here!
*/

static char *position_history_prompt(char *history_prompt)
{
    int len;
    
    /* We do the prompt editing in our own private buffer which we return.  According	*/
    /* to "the rules" we don't want to directly edit the input buffer.			*/
    
    /* Note that gdb never does cursor positioning.  So just for safety, if by some 	*/
    /* weird case one of our positioning controls gets through to here (it shouldn't 	*/
    /* since this function is not called for the standard gdb prompt) we just return.	*/
    /* We do a simple test to detect this, i.e., just look for an escape (0x1b) as the	*/
    /* first prompt character.								*/
    
    static char *modified_prompt = NULL;
    static int  max_len          = 0;
    
    if (*history_prompt == 0x1b)
    	return (history_prompt);
    	
    len = strlen(history_prompt) + 128/*safety+padding+GOTO*/;
    	
    if (len > max_len)
    	modified_prompt = gdb_realloc(modified_prompt, max_len = len);
    
    sprintf(modified_prompt, GOTO "%s", cmd_top, cmd_left, history_prompt);
    
    return (modified_prompt);
}


/*--------------------------------------------------------*
 | forget_some_history - forget the first n history lines |
 *--------------------------------------------------------*
 
 If called we assume macsbug_screen is on and that the new count is at least as big as
 the history area line size.
*/

void forget_some_history(int n)
{
    History *h;
    
    if (history_cnt > n) {			/* keep what we got if smaller		*/
    	while (history && n--) {		/* delete first n entries of history	*/
	    h = history->next;
	    gdb_free(history);
	    history = h;
	    --history_cnt;
	}
	history->prev = NULL;
    }
}

/*--------------------------------------------------------------*
 | stop_hook - come here whenever gdb stop the target execution |
 *--------------------------------------------------------------*
 
 This is a stop-hook and thus gets control whenever gdb stops executing the target program.
 Here we make sure the pc area and side bar are suitably updated to reflect the stopped
 execution.
*/

static void stop_hook(char *arg, int from_tty) 
{
    gdb_execute_hook(hook_stop);
    Update_PC_and_SideBare_Areas();
}


#if 0
static int line_prefix_cp = 0;
static int xseen = 0;
/*-------------------------------------------------------------------------------------*
 | filter_keyboard_characters - check each character as it's entered from the keyboard |
 *-------------------------------------------------------------------------------------*/

static int filter_keyboard_characters(int c)
{
    char ch = c;
    static char line_prefix[5];
    static char esc_up_arrow[5]   = {ESC, ESC, '[', 'A', '\0'};
    static char esc_down_arrow[5] = {ESC, ESC, '[', 'B', '\0'};
    
    #if 0
    if (c == '\n' || c == '\r')
    	line_prefix_cp = 0;
    else if (line_prefix_cp < 4) {
    	line_prefix[line_prefix_cp++] = c;
	if (line_prefix_cp == 4) {
	    if (strcmp(line_prefix, esc_up_arrow) == 0) {
	    	gdb_fprintf(gdb_current_stderr, "еее esc up еее\n");
		c = 0;
	    } else if (strcmp(line_prefix, esc_down_arrow) == 0) {
	    	gdb_fprintf(gdb_current_stderr, "еее esc down еее\n");
		c = 0;
	    } else if (c == ESC)
	    	c = 0;
	}
    }
    #endif
    xseen = 1;
    
    return (c);
}
#endif


/*-----------------------------------------------------------*
 | my_prompt_position_function - prompt positioning function |
 *-----------------------------------------------------------*
 
 While the macsbug screen is active all prompts that gdb does come here first to allow
 us to control the position of the prompt on the screen.  We already have the positioning
 defined in the normal gdb prompt (done by macsbug_on when the screen is turned on).  If
 we don't do that then gdb's history display appears in strange places as it's scrolled.
 Thus the only prompt positioning we're interested in here is the one gdb uses for raw
 line input.
 
 Raw line input is initiated when the user types a COMMANDS, DEFINE, DOCUMENT, WHILE,
 or IF at the "outer" level.  Nested WHILE's and IF's are themselves read as raw lines.
 It is the prompts on all these lines we need to control.
 
 When the outer-most COMMANDS, DEFINE, DOCUMENT, WHILE, or IF are entered they are
 treated as normal commands which we intercept to echo that command to the history area
 (gdb doesn't echo since it things you're entering the stuff at a scrolling terminal).
 It's also sets control_level to the initial indenting level and reading_raw to 1 to
 tell us here to position the prompt.
 
 Note, the continued parameter is the count of the number of continued lines preceding
 the upcoming line that's about to be prompted for.  We use it here to use the lines
 we allocated for the command line area to scroll the continued lines up.  This code 
 uses the line starts array built by filter_commands() which receives each line
 segment after it is read following the prompt which is about to appear.
*/

void my_prompt_position_function(int continued)
{
    int  len, i, j;
    char line[1024];
    
    static int used_cmd_lines = 0;
    static int starts_top     = 0;
    
    if (gdb_interactive() && macsbug_screen)
    	if (continued == 0) {
	    immediate_flush = NORMAL_REFRESH;
	    gdb_fflush(gdb_current_stdout);
	    gdb_fflush(gdb_current_stderr);
	    screen_refresh(0);
	    Update_PC_and_SideBare_Areas();
	    while (used_cmd_lines)
	    	screen_fprintf(stderr, CMD_AREA, GOTO CLEAR_LINE, cmd_top+used_cmd_lines--, cmd_left);
	    starts_top = 0;
    	    screen_fprintf(stderr, CMD_AREA, GOTO CLEAR_LINE, cmd_top, cmd_left);
	} else if (continued < cmd_lines) {
    	    screen_fprintf(stderr, CMD_AREA, GOTO, cmd_top + continued, cmd_left);
	    used_cmd_lines = continued;
	} else {
	    for (i = ++starts_top, j = 0; i < continued_count; ++i, ++j) {
	    	len = continued_segment_len[i];
		memmove(line, continued_line_starts[i], len);
		line[len++] = '\\';
		line[len]   = '\0';
		screen_fprintf(stderr, CMD_AREA, GOTO CLEAR_LINE "%s", cmd_top+j, cmd_left, line);
	    }
	    screen_fprintf(stderr, CMD_AREA, GOTO CLEAR_LINE, cmd_top + cmd_lines - 1, cmd_left);
	}
    
    gdb_define_raw_input_handler(my_raw_input_handler);
}


/*---------------------------------------------------------------------------------*
 | my_raw_input_prompt_setter - modify the raw input prompt in the Macsbug screen  |
 *---------------------------------------------------------------------------------*
 
 While the macsbug screen is active all raw prompts for COMMANDS, DEFINE, DOCUMENT,
 WHILE, or IF that gdb does come here before going to our my_prompt_position_function()
 above.  This allows us to modify the raw input prompt used for those commands as
 the body of commands for them is being input.  Initially the prompt would be in the
 correct place due to what my_prompt_position_function() does.  But if the user starts
 using the arrows to scroll through the gdb command history then newlines in the
 commands would cause the prompt to shift left into the side bar.  So we "sneak" into
 the command line handling (this is a hook routine) and modify gdb's prompt "behind
 it's back" (i.e., we are actually changing the prompt buffer which is owned by gdb).
 It's dirty, but there doesn't seem to be any alternative.
*/

void my_raw_input_prompt_setter(char *prompt)
{
    char orig_prompt[256];
    
    if (strstr(prompt, GOTO CLEAR_LINE) == NULL) {
	strcpy(orig_prompt, prompt);
	//sprintf(prompt, GOTO CLEAR_LINE "%s", cmd_top, cmd_left, orig_prompt);
	sprintf(prompt, GOTO "%s", cmd_top, cmd_left, orig_prompt);
    }
}


/*-----------------------------------------------------------------*
 | my_raw_input_handler - echo raw input lines to the history area |
 *-----------------------------------------------------------------*
 
 This is called after a raw input line has been read (i.e., COMMANDS, DEFINE, DOCUMENT,
 WHILE, or IF).  From here we echo the line to the history area (indented as necessary
 to show nesting level).  We also need to check for nested IF and WHILE since those
 are also read as raw lines.  Similarly END indicates that the nesting level is to be
 decremented.

 The control_level indicates the nesting depth.  If it's -1 we have DEFINE or DOCUMENT
 indicating no indenting is wanted.  The thing doubles as a switch for 
 my_prompt_position_function() above.
*/

void my_raw_input_handler(char *theRawLine)
{
    char *p1, *p2, c;
    int  len;    
    
    if (theRawLine) {
    	p1 = theRawLine;
	while (*p1 == ' ' || *p1 == '\t')
	    ++p1;
	
	for (p2 = p1; *p2 && (*p2 != ' ' && *p2 != '\t'); p2++) ;
	len = p2 - p1;
	c = *p2;
	*p2 = '\0';
	if (gdb_strcmpl(p1, "end")) {
	    if (control_level > 0)
	    	--control_level;
	    if (control_level <= 0) {
		gdb_define_raw_input_handler(NULL);
		gdb_control_prompt_position(my_prompt_position_function);
		gdb_set_raw_input_prompt_handler(NULL);
		reading_raw = 0;
	    }
	}
	*p2 = c;
	
	if (control_level <= 0)
    	    gdb_fprintf(gdb_current_stdout, "%s\n", theRawLine);
	else
    	    gdb_fprintf(gdb_current_stdout, "%*c%s\n", control_level, ' ', theRawLine);
    	gdb_fflush(gdb_current_stdout);
	
	*p2 = '\0';
	if (gdb_strcmpl(p1, "if") || gdb_strcmpl(p1, "while")) {
    	    gdb_define_raw_input_handler(my_raw_input_handler);
    	    gdb_control_prompt_position(my_prompt_position_function);
	    ++control_level;
	}
	*p2 = c;
    }
}


/*-----------------------------------------------------*
 | doing_query - signal that we're about to do a query |
 *-----------------------------------------------------*
 
 This is a Gdb_Before_Query special event which is called when a query is about to be
 performed.  The query will be written to the history area and a read will be done by gdb
 for the "y"/"n" answer.  We need to make sure that the prompt is displayed before the
 read comes up.  To that end we set the immediate_flush switch to QUERY_REFRESH1 to tell
 write_to_history_area() to flush what it has just written so it can be seen.  It will
 continue to do so until the switch is reset after the query by end_of_query(), a
 Gdb_After_Query special event.
*/

static int doing_query(const char *prompt, int *result)
{
    if (immediate_flush == NORMAL_REFRESH)
    	immediate_flush = QUERY_REFRESH1;
    return (1);
}


/*-----------------------------------------------------------------------*
 | end_of_query - end of a query event, reset to normal history flushing |
 *-----------------------------------------------------------------------*
 
 This is a Gdb_After_Query special event which is called after a query has been responded
 to.  The query result is passed and we return it unchanged.  The flush mode switch, set
 by doing_query() above, is reset to NORMAL_REFRESH.  The flush mode set by doing_query()
 was such so that the history area was updated for each query prompt line to ensure that
 it was all seen before the read came up.  Now we can go back to the normal flush state.
*/

static int end_of_query(int result)
{
    screen_refresh(0);
    immediate_flush = NORMAL_REFRESH;
    return (result);
}


/*------------------------------------------*
 | macsbug_on - turn on the MacsBug display |
 *------------------------------------------*/

void macsbug_on(int resume)
{
    get_screen_size(&max_rows, &max_cols);
    
    if (!isatty(STDOUT_FILENO))
    	gdb_error("MacsBug screen cannot be used in this environment.");

    if (max_rows < MIN_SCREEN_ROWS || max_cols < MIN_SCREEN_COLS)
    	gdb_error(COLOR_RED "\nTerminal window too small (must be at least %ld rows and %ld columns)." COLOR_OFF "\n",
			MIN_SCREEN_ROWS, MIN_SCREEN_COLS);

    macsbug_screen_stdout = gdb_open_output(stdout, write_to_history_area, NULL);
    macsbug_screen_stderr = gdb_open_output(stderr, write_to_history_area, NULL);
    gdb_special_events(Gdb_Word_Completion_Cursor, (Gdb_Callback)word_completion_cursor_control);
    gdb_special_events(Gdb_Word_Completion_Query,  (Gdb_Callback)word_completion_query);
    gdb_special_events(Gdb_Before_Query,           (Gdb_Callback)doing_query);
    gdb_special_events(Gdb_After_Query,            (Gdb_Callback)end_of_query);
    gdb_special_events(Gdb_History_Prompt,         (Gdb_Callback)position_history_prompt);
    
    gdb_redirect_output(macsbug_screen_stderr);
    gdb_redirect_output(macsbug_screen_stdout);
    gdb_define_raw_input_handler(NULL);
    gdb_control_prompt_position(my_prompt_position_function);
    gdb_set_int("$macsbug_screen", macsbug_screen = 1);
    
    reading_raw = 0;
    
    if (!resume) {
    	history_cnt = 0;
    	history = history_tail = NULL;
    	screen_top = screen_bottom = NULL;
    	wrap_lines = 1;
    }
    
    prompt_start = 0;
    refresh(NULL, 0);
    
    //hook_stop = gdb_replace_command_hook("stop", stop_hook, "For internal use only -- do not use.");
        
    screen_fprintf(stderr, CMD_AREA, GOTO CLEAR_LINE, cmd_top, cmd_left);
}


/*--------------------------------------------*
 | macsbug_off - turn off the MacsBug display |
 *--------------------------------------------*/

void macsbug_off(int suspend)
{
    History *h;
    char    set_prompt_cmd[1024];
    
    if (!macsbug_screen)
    	return;
	
    gdb_close_output(macsbug_screen_stderr);
    gdb_close_output(macsbug_screen_stdout);
    gdb_special_events(Gdb_Word_Completion_Cursor, NULL);
    gdb_special_events(Gdb_Word_Completion_Query,  NULL);
    gdb_special_events(Gdb_Before_Query,           NULL);
    gdb_special_events(Gdb_After_Query,            NULL);
    gdb_special_events(Gdb_History_Prompt,         NULL);

    gdb_define_raw_input_handler(NULL);
    gdb_control_prompt_position(NULL);
    macsbug_screen_stdout = macsbug_screen_stderr = NULL;
        
    get_screen_size(&max_rows, &max_cols);
        
    if (!suspend) {
    	while (history) {
            h = history->next;
    	    gdb_free(history);
    	    history = h;
        }
        history_cnt = 0;
        history = history_tail = NULL;
        screen_top = screen_bottom = NULL;
	
    	screen_close();
    }
    
    position_cursor_for_shell_input();
    
    gdb_set_int("$macsbug_screen", macsbug_screen = 0);
    
    //gdb_remove_command_hook(hook_stop);
    
    gdb_get_prompt(prompt);
    sprintf(set_prompt_cmd, "set prompt %s", prompt + prompt_start);
    doing_set_prompt = 1;
    gdb_execute_command(set_prompt_cmd);
    doing_set_prompt = 0;
    prompt_start = 0;
}


/*-------------------------------------------------------------*
 | mb [on | off] - turn MacsBug display on or off or toggle it |
 *-------------------------------------------------------------*/

static void mb(char *arg, int from_tty)
{
    if (arg && *arg) {
	if (gdb_strcmpl(arg, "on")) {
	    if (!macsbug_screen_stdout)
	    	macsbug_on(0);
	} else if (gdb_strcmpl(arg, "off")) {
	    if (macsbug_screen_stdout)
	    	macsbug_off(0);
	} else
	    gdb_error("macsbug [on | off] expected, got \"%s\"", arg);
    } else if (macsbug_screen_stdout)
    	macsbug_off(0);
    else
    	macsbug_on(0);
}

#define MB_HELP \
"MB [on | off] -- Turn MacsBug display screen on or off or toggle it.\n"		\
"Specifying \"on\" turns on the display if not already on.\n" 				\
"Specifying \"off\" turns off the display if not already off.\n" 			\
"The default is to toggle the display.\n"						\
"\n"											\
"Type \"help screen\" command to get a list of all MacsBug screen\n"			\
"commands."

/*--------------------------------------------------------------------------------------*/

/*----------------------------------------------------*
 | init_macsbug_display - initialization for displays |
 *----------------------------------------------------*/

void init_macsbug_display(void)
{
    MACSBUG_SCREEN_COMMAND(mb, 	    MB_HELP);
    MACSBUG_SCREEN_COMMAND(refresh, REFRESH_HELP);
    MACSBUG_SCREEN_COMMAND(scroll,  SCROLL_HELP);
    MACSBUG_SCREEN_COMMAND(su,      SU_HELP);
    MACSBUG_SCREEN_COMMAND(sd,      SD_HELP);
    MACSBUG_SCREEN_COMMAND(page,    PAGE_HELP);
    MACSBUG_SCREEN_COMMAND(pgu,     PGU_HELP);
    MACSBUG_SCREEN_COMMAND(pgd,     PGD_HELP);
    MACSBUG_SCREEN_COMMAND(log_,    LOG_HELP);
    
    MACSBUG_USEFUL_COMMAND(__display_side_bar, __DISPLAY_SIDE_BAR_HELP);
    gdb_define_cmd("log?", which_log, macsbug_internal_class, "");
    
    COMMAND_ALIAS(page, pg);
    COMMAND_ALIAS(su, scu);
    COMMAND_ALIAS(sd, scd);
        
    define_macsbug_screen_positions(pc_area_lines, cmd_area_lines);

    //gdb_execute_command("set use-cached-symfiles 0");/* remove someday if gdb is fixed*/
}
