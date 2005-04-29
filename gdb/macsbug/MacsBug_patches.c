/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                  MacsBug_patches.c                                   |
 |                                                                                      |
 |               Patches to GDB Commands And Other GDB-Related Overrides                |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2001                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*

 This file contains "patches" to existing gdb commands so that we may do additional
 things when those commands are executed.  Also here is other stuff needed to
 intercept or override "normal" gdb behavior.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "MacsBug.h"

/*--------------------------------------------------------------------------------------*/

Gdb_Plugin gdb_printf_command = NULL;

int control_level = 0;				/* if, while, etc. nesting level	*/
int reading_raw   = 0;				/* reading raw data for if, while, etc.	*/
    
static unsigned long *bkpt_tbl      = NULL;	/* table of sorted breakpoint addresses	*/
static int           bkpt_tbl_sz    = 0;        /* current max size of the bkpt_tbl	*/
static int           bkpt_tbl_index = -1;	/* index of last entry in the bkpt_tbl	*/

#define BKPT_DELTA  50				/* bkpt_tbl grows in these increments	*/

static Gdb_Plugin gdb_set_command   	= NULL;	/* gdb's address's for these commands	*/
static Gdb_Plugin gdb_help_command  	= NULL;
static Gdb_Plugin gdb_run_command   	= NULL;
static Gdb_Plugin gdb_shell_command 	= NULL;
static Gdb_Plugin gdb_make_command  	= NULL;
static Gdb_Plugin gdb_list_command	= NULL;
static Gdb_Plugin gdb_next_command	= NULL;
static Gdb_Plugin gdb_step_command	= NULL;
static Gdb_Plugin gdb_nexti_command	= NULL;
static Gdb_Plugin gdb_stepi_command	= NULL;

static Gdb_Plugin gdb_define_command  	= NULL;
static Gdb_Plugin gdb_document_command  = NULL;
static Gdb_Plugin gdb_if_command  	= NULL;
static Gdb_Plugin gdb_while_command  	= NULL;

static Gdb_Plugin gdb_file_command	= NULL;
static Gdb_Plugin gdb_attach_command	= NULL;
static Gdb_Plugin gdb_symbol_file_command=NULL;
static Gdb_Plugin gdb_sharedlibrary_command=NULL;
static Gdb_Plugin gdb_load_command	= NULL;

typedef void (*SigHandler)(int);

static SigHandler prev_SIGWINCH_handler = NULL;
static SigHandler prev_SIGCONT_handler  = NULL;
static SigHandler prev_SIGINT_handler   = NULL;
static SigHandler prev_SIGTSTP_handler  = NULL;

/*--------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------*
 | my_signal_handler - private signal handling behind gdb's back |
 *---------------------------------------------------------------*
 
 We detect SIGCONT (program continued after stopping, CTL-Z then fg command) and
 SIGWINCH (screen size changed) signals here.  If the macsbug screen is currently active
 we refresh it for these events.
*/

static void my_signal_handler(int signo)
{
    char prompt[1024];
    
    switch (signo) {
    	case SIGTSTP:				/* program stopped (e.g., ^Z)		*/
	    if (macsbug_screen)
	    	position_cursor_for_shell_input();
    	    if (log_stream) 			/* close log in case we don't come back	*/
	    	fclose(log_stream);		/* ...are we paranoid or what?		*/
	    signal(SIGTSTP, SIG_DFL);		/* we need to do what gdb does or we	*/
    	    sigsetmask(0);			/* don't get control back here because	*/
    	    kill(getpid(), SIGTSTP);		/* of this kill() call			*/
    	    signal(SIGTSTP, my_signal_handler);
    	    break;
	    
	    /* Note: SIGTSTP is currently not used since gdb reestablishes its own	*/
	    /*       every time a command is processed.  Sigh :-(			*/
	    
    	case SIGCONT:				/* continue execution (e.g., after ^Z)	*/
    	    if (prev_SIGCONT_handler)
    	    	prev_SIGCONT_handler(signo);
	    signal(SIGCONT, my_signal_handler);
	    if (macsbug_screen)
	    	refresh(NULL, 0);
	    #if 0
	    if (log_stream) {			/* reopen log now that we're back :-)	*/
	    	log_stream  = fopen(log_filename, "a");
		if (!log_stream)
		    gdb_fprintf(gdb_current_stderr, "Cannot reopen log file: %s", strerror(errno));
	    }
	    #endif
	    fprintf(stderr, "%s", gdb_get_prompt(prompt));
    	    break;
    	
    	case SIGWINCH:				/* terminal screen changed		*/
    	    if (prev_SIGWINCH_handler)
    	    	prev_SIGWINCH_handler(signo);
	    signal(SIGWINCH, my_signal_handler);
    	    __window_size(NULL, 0);
	    get_screen_size(&max_rows, &max_cols);
	    save_stack(max_rows);
	    if (macsbug_screen) {
		if (max_rows < MIN_SCREEN_ROWS || max_cols < MIN_SCREEN_COLS) {
		    macsbug_off(0);
		    gdb_error(COLOR_RED "Terminal window too small (must be at least %ld rows and %ld columns)." COLOR_OFF "\n",
				    MIN_SCREEN_ROWS, MIN_SCREEN_COLS);
		    return;
		}
	    	refresh(NULL, 0);
	    	fprintf(stderr, "%s", gdb_get_prompt(prompt));
	    }
    	    break;
	
	/* It looks like this one is reset by just like SIGTSTP is...damit :-(		*/
	case SIGINT:				/* terminal interrupt (^C)		*/
	    printf("My SIGINT\n");
	    control_level = reading_raw = 0;
    	    if (prev_SIGINT_handler)
    	    	prev_SIGINT_handler(signo);
	    signal(SIGINT, my_signal_handler);
	    break;
    	
    	default:
	   gdb_internal_error("Unexpected signal detected in MacsBug signal handler");
    	   break;
    }
}

/*--------------------------------------------------------------------------------------*/

/*-------------------------------------*
 | run_command - RUN command intercept |
 *-------------------------------------*
 
 This command adds some processing to the run command by initializing our state and
 preference variables.
 
 Caution: Be careful here.  With gdb 5.x asynchronous processing there is a limit of
          what can be done here at this time.
*/

void run_command(char *arg, int from_tty)
{
    gdb_run_command(arg, from_tty);
    
    __window_size(NULL, 0);
    gdb_set_int("$dot", 0);
    gdb_set_int("$__running__",    1);
    gdb_set_int("$__lastcmd__",   -1);
    gdb_set_int("$__next_addr__", -1);
    gdb_set_int("$__prev_dm_n__",  0);
    gdb_set_int("$__prev_dma_n__", 0);
    gdb_set_int("$macsbug_screen", macsbug_screen);
    need_CurApName = 1;
    init_sidebar_and_pc_areas();
}


/*------------------------------------------*
 | set_command expr - intercept SET command |
 *------------------------------------------*
 
 This intercepts the SET expr command (ass opposed to the SET settings command).  Here
 we make sure the macsbug screen is updated just in case the user changed a register
 or the pc.
*/
 
static void set_command(char *arg, int from_tty)
{
     gdb_set_command(arg, from_tty);		/* let the normal set do its thing...	*/
     Update_PC_and_SideBare_Areas();		/* ...then update display accordingly	*/
}


/*-------------------------------------------*
 | help_command - intercept the HELP command |
 *-------------------------------------------*/
 
static void help_command(char *arg, int from_tty)
{
    gdb_help_command(arg, from_tty);
    if (!arg)
    	gdb_printf("\nType \"help mb-notes\" or just \"mb-notes\" to get additional info about MacsBug.\n");
}


/*--------------------------------------------------------------------*
 | shell_command - suspend MacsBug display across a gdb SHELL command |
 *--------------------------------------------------------------------*/
 
static void shell_command(char *arg, int from_tty)
{
    GDB_FILE *curr_stdout, *curr_stderr;
    
    if (!gdb_shell_command)
    	return;
    	
    if (macsbug_screen) {
    	position_cursor_for_shell_input();
    	curr_stdout = gdb_current_stdout;
    	gdb_redirect_output(gdb_default_stdout);
    
    	curr_stderr = gdb_current_stderr;
    	gdb_redirect_output(gdb_default_stderr);
    }
    
    gdb_shell_command(arg, from_tty);
    
    if (macsbug_screen) {
    	if (gdb_query(COLOR_RED "Refresh MacsBug screen now? ")) {
    	    fprintf(stderr, COLOR_OFF);
    	    gdb_redirect_output(curr_stdout);
    	    gdb_redirect_output(curr_stderr);
    	    refresh(NULL, 0);
    	} else {
    	    fprintf(stderr, COLOR_OFF);
    	    macsbug_off(0);
    	}
    }
}


/*------------------------------------------------------------------*
 | make_command - suspend MacsBug display across a gdb MAKE command |
 *------------------------------------------------------------------*/

static void make_command(char *arg, int from_tty)
{
    GDB_FILE *curr_stdout, *curr_stderr;
    
    if (!gdb_shell_command)
    	return;
    	
    if (macsbug_screen) {
    	position_cursor_for_shell_input();
    	curr_stdout = gdb_current_stdout;
    	gdb_redirect_output(gdb_default_stdout);
    
    	curr_stderr = gdb_current_stderr;
    	gdb_redirect_output(gdb_default_stderr);
    }
    
    gdb_make_command(arg, from_tty);
    
    if (macsbug_screen) {
    	if (gdb_query(COLOR_RED "Refresh MacsBug screen now? ")) {
    	    fprintf(stderr, COLOR_OFF);
    	    gdb_redirect_output(curr_stdout);
    	    gdb_redirect_output(curr_stderr);
    	    refresh(NULL, 0);
    	} else {
    	    fprintf(stderr, COLOR_OFF);
    	    macsbug_off(0);
    	}
    }
}

/*----------------------------------------------------------------------*
 | listing_filter - stdout output filter for enhanced_gdb_listing_cmd() |
 *----------------------------------------------------------------------*
 
 This is the redirected stream output filter function for enhanced_gdb_listing_cmd() to
 ensure that the lines listed by the LIST, NEXT, STEP, NEXTI, STEPI commands are
 guaranteed initially blank.
 
 The only reason for this is because of our handling of contiguous output for those
 commands when the MacsBug screen is off requires us to move the cursor up after the
 user types the return.  That causes the potential for leaving stuff from the command
 line (the tail end of the line) appearing to be appended to the next line of output!
 
 Example, user type L to list a bunch of lines.  The types LIST to list the next bunch.
 Because this is the same command the list output is to be contiguous (assuming the
 MacsBug screen is off -- if on we don't have these problems).  If the first line of the
 second bunch is short enough the tail end of the LIST command will appear on the line.
 This assumes that the line being output is to a terminal with a curses-like
 implementation that only outputs what it's asked to (as apparently the OSX terminal
 does).
 
 So all we do here is output the lines with a CLEAR_LINE prefixed to it.
*/
 
static char *listing_filter(FILE *f, char *line, void *data)
{
    if (line)
    	gdb_fprintf(*(GDB_FILE **)data, CLEAR_LINE "%s", line);
	
    return (NULL);
}


/*-------------------------------------------------------------------------------------*
 | enhanced_gdb_listing_cmd - common code for ENHANCE_GDB_LISTING_CMD macro expansions |
 *-------------------------------------------------------------------------------------*
 
 This is only called from the expansion of the ENHANCE_GDB_LISTING_CMD macro as the
 common code to handle the LIST, NEXT, STEP, NEXTI, and STEPI commands.  See the comments
 for that macro below for further details.
*/

static void enhanced_gdb_listing_cmd(char *arg, int from_tty, int cmdNbr, 
				     void (*cmd)(char*, int))
{
    if (from_tty && !macsbug_screen && isatty(STDOUT_FILENO) &&
    	gdb_get_int("$__lastcmd__") == cmdNbr)
    	gdb_printf(CURSOR_UP CLEAR_LINE, 2);
    
    if (macsbug_screen)
    	cmd(arg, from_tty);			/* just do cmd if we have screen	*/
    else {
    	GDB_FILE *redirect_stdout, *prev_stdout;
     	
	redirect_stdout = gdb_open_output(stdout, listing_filter, &prev_stdout);
	prev_stdout = gdb_redirect_output(redirect_stdout);
   	
	cmd(arg, from_tty);			/* filter output with listing_filter()	*/
    	
	gdb_close_output(redirect_stdout);
    }
    
    gdb_set_int("$__lastcmd__", cmdNbr);
}


/*------------------------------------------------------------------------------------*
 | ENHANCE_GDB_LISTING_CMD - macro to enhance LIST, NEXT, STEP, NEXTI, STEPI commands |
 *------------------------------------------------------------------------------------*

 If the MacsBug screen is off and the previous command was also a LIST, NEXT, STEP, NEXTI,
 or STEPI command (i.e., same respective command repeated) then we back up over the 
 prompt to make the display contiguous just like when the MacsBug screen is on.
 
 Note that gdb_<cmd>_command() is gdb's <cmd> command (cmd = list | next | step | nexti |
 stepi) that we replaced but we did NOT replace their help.  So doing HELP on <cmd> still
 works as usual meaning we don't have to dup any of the help info for our replacement.
 
 Also note that these commands are defined as "standard" MacsBug commands in that they
 get their own unique $__lastcmd__ values and are in the macsbug_cmds[] commands table
 defined in macsBug_cmdline.c, preprocess_commands().  That's necessary in order to be
 able to handle the repeat of <cmd> when only a return is entered so that we may know
 to make the listing contiguous.  That's the whole point of this exercise!
*/

#define ENHANCE_GDB_LISTING_CMD(cmd, cmdNbr)						\
static void cmd ## _command(char *arg, int from_tty)					\
{											\
    enhanced_gdb_listing_cmd(arg, from_tty, cmdNbr, gdb_ ## cmd ## _command);		\
}

ENHANCE_GDB_LISTING_CMD(list, 44)			/* list_command			*/
ENHANCE_GDB_LISTING_CMD(next, 45)			/* next_command			*/
ENHANCE_GDB_LISTING_CMD(step, 46)			/* step_command			*/
ENHANCE_GDB_LISTING_CMD(nexti,47)			/* nexti_command		*/
ENHANCE_GDB_LISTING_CMD(stepi,48)			/* stepi_command		*/


/*--------------------------------------------------------------------------------------*
 | CAUSES_PROGRESS_CMD_PLUGIN - macro to define file, attach, symbol-file, load plugins |
 *--------------------------------------------------------------------------------------*

 This macro is used to define five plugins to patch file, attach, symbol-file,
 and load.  We don't really do anything with these commands other than set the
 immediate_flush state to PROGRESS_REFRESH to cause output to the history area to be
 immediately flushed to the screen (when using the curses-like screen display).  These
 commands put out dots for progress (not sure about load though) which we want to see
 as they occur.  Other than that things proceed as they always have.
*/

#define CAUSES_PROGRESS_CMD_PLUGIN(cmd) 				\
static void cmd ## _command(char *arg, int from_tty)			\
{									\
    immediate_flush = PROGRESS_REFRESH;					\
    gdb_ ## cmd ## _command(arg, from_tty);				\
    immediate_flush = NORMAL_REFRESH;					\
}

CAUSES_PROGRESS_CMD_PLUGIN(file)			/* file_command			*/
CAUSES_PROGRESS_CMD_PLUGIN(attach)			/* attach_command		*/
CAUSES_PROGRESS_CMD_PLUGIN(symbol_file)			/* symbol_file_command		*/
CAUSES_PROGRESS_CMD_PLUGIN(load)			/* load_command			*/
//CAUSES_PROGRESS_CMD_PLUGIN(sharedlibrary)		/* sharedlibrary_command	*/


/*------------------------------------------------------------------------------*
 | CONTROL_CMD_PLUGIN - macro to define if, while, define, and document plugins |
 *------------------------------------------------------------------------------*
 
 This macro is used to define four plugins to patch if, while, define, and document.
 Gdb only calls it when these structures are not nested, i.e., at the outer-most level.
 All other nested lines are read as raw data lines which are handled by our raw data
 line handler (my_raw_input_handler() in MacsBug_display.c) used to echo the raw lines
 to the history area.
 
 So all we do here is init the nesting level to 1 and echo the outer command to the
 history area (since that isn't passed to the raw reading since it initiates the reading
 in the first place).  Since we always intercept these commands at level 0 we need to
 be careful not to do the echoing unless we have our macsbug screen up and then only
 if we are reading directly from the terminal (from_tty != 0).
*/

#define CONTROL_CMD_PLUGIN(cmd) 					\
static void cmd ## _command(char *arg, int from_tty)			\
{									\
    if (arg && from_tty && macsbug_screen) {				\
    	gdb_printf(#cmd " %s\n", arg);					\
	gdb_fflush(gdb_current_stdout);					\
    	gdb_define_raw_input_handler(my_raw_input_handler);		\
    	gdb_control_prompt_position(my_prompt_position_function);	\
    }									\
    									\
    control_level = reading_raw = 1;					\
									\
    gdb_ ## cmd ## _command(arg, from_tty);				\
}

CONTROL_CMD_PLUGIN(define)
CONTROL_CMD_PLUGIN(document)
CONTROL_CMD_PLUGIN(if)
CONTROL_CMD_PLUGIN(while)

#if 0
static void show_the_command(char *cmd, char *arg, int from_tty)
{
    if (arg && from_tty && macsbug_screen) {
    	gdb_printf("%s %s\n", cmd, arg);
	gdb_fflush(gdb_current_stdout);
    	gdb_define_raw_input_handler(my_raw_input_handler);
    	gdb_control_prompt_position(my_prompt_position_function);
    }
    
    control_level = reading_raw = 1;
}

static void if_command(char *arg, int from_tty)
{
    show_the_command("if", arg, from_tty);
    gdb_if_command(arg, from_tty);
}
#endif


/*---------------------------------------------------------------------------*
 | exit_handler - gdb termination processing just before it issues an exit() |
 *---------------------------------------------------------------------------*
 
 If the macsbug screen is currently active then move cursor to bottom of the screen to
 cause it to scroll up one line when the shell command line prompt is displayed.
*/

static void exit_handler(void)
{
    if (log_stream) {				/* close log file if it's open...	*/
	gdb_printf("Closing log\n");
	fclose(log_stream);
	log_stream = NULL;
    }
    
    position_cursor_for_shell_input();
}


#if 0
/*----------------------------------------------------*
 | quit_command1 - replacement for gdb's QUIT command |
 *----------------------------------------------------*/

static void quit_command1(char *arg, int from_tty)
{
    if (macsbug_screen) {			/* if macsbug screen is on...		*/
    	if (!gdb_target_running())		/* ...if not currently running...	*/
    	    fprintf(stderr, "\n");		/* ...need extra \n for missing confirm	*/
    	fprintf(stderr, "\n" ERASE_BELOW "\n");	/* ...erase everything below		*/
    }
    
    quit_command(arg, from_tty);		/* let gdb do the quitting		*/
}
#endif


/*------------------------------------------------------------------------*
 | bsearch_compar_bkpt - bsearch compare routine for a finding breakpoint |
 *------------------------------------------------------------------------*/

static int bsearch_compar_bkpt(const void *a1, const void *a2)
{
    if (*(unsigned long *)a1 < *(unsigned long *)a2)
    	return (-1);
    
    if (*(unsigned long *)a1 > *(unsigned long *)a2)
    	return (1);
    
    return (0);
}


/*-------------------------------------*
 | find_breakpoint - find a breakpoint |
 *-------------------------------------*
 
 Searches the bkpt_tbl to see if the specified address is in it.  The index of the
 found breakpoint address is returned or -1 if it is not found.
*/

int find_breakpoint(unsigned long address)
{
    int 	  i = -1;
    unsigned long *p;
   
    if (bkpt_tbl_index >= 0) {
    	p = bsearch((void *)&address, bkpt_tbl, bkpt_tbl_index+1, sizeof(unsigned long),
		    bsearch_compar_bkpt);
	if (p)
	    i = p - bkpt_tbl;
    }
    
    return (i);
}


/*------------------------------------------------------------------------------------*
 | fix_pc_area_if_necessary - update MacsBug screen pc area if breakpoint can be seen |
 *------------------------------------------------------------------------------------*
 
 This checks the address to see if it's in the range of the addresses currently in the
 pc area (if it currently being display) and forces the pc area to be redisplayed if
 it is.  This will show the changing status of a breakpoint at the address.
 
 Note that we don't use the current pc_area_lines setting since it could be one line
 smaller than what's actually displayed there.  This can happen if there is no symbol
 to display.  We use that line normally reserved for the symbol for an extra disassembly
 line instead (why waste it with a blank line?).  The global current_pc_lines reflects
 the actual number of lines displayed (it's pc_area_lines or pc_area_lines+1).
*/

static void fix_pc_area_if_necessary(unsigned long address)
{
    unsigned long pc;
    int           sz;
    
    if (macsbug_screen && gdb_target_running()) {
      /* This is a hack.  Sometimes gdb leaves the deprecated_selected_frame 
         null, but still uses it.  get_selected_frame will force it to get set. */
      get_selected_frame ();
    	/* note, get_selected_frame() done as part of gdb_get_register() */
    	gdb_get_register("$pc", &pc);
	if (address >= pc && address < pc + (4*current_pc_lines))
	    force_pc_area_update();
    }
}
    

/*------------------------------------------------------------------------*
 | qsort_compar_bkpt - qsort compare routine for sorting breakpoint table |
 *------------------------------------------------------------------------*/

static int qsort_compar_bkpt(const void *a1, const void *a2)
{
    if (*(unsigned long *)a1 < *(unsigned long *)a2)
    	return (-1);
    
    if (*(unsigned long *)a1 > *(unsigned long *)a2)
    	return (1);
    
    return (0);
}


/*--------------------------------------------------------------------------------*
 | new_breakpoint - gdb_special_events() callback called when breakpoint is added |
 *--------------------------------------------------------------------------------*
 
 We get control here whenever a breakpoint is added.  Here we keep from adding duplicates
 and build the sorted bkpt_tbl.  We keep the table sorted so that the disassembly can
 use bsearch to look up addresses.  We also do a bsearch in find_breakpoint() to check
 for the presence of breakpoints too.
*/

static void new_breakpoint(unsigned long address, int enabled)
{
    int i;
        
    if (!enabled)				/* can this ever happen?		*/
    	return;
	
    i = find_breakpoint(address);		/* find the breakpoint			*/
    if (i >= 0)					/* if already recorded...		*/
    	return;					/* ...don't record duplicates		*/
	
    if (++bkpt_tbl_index >= bkpt_tbl_sz) {	/* add it to the bkpt_tbl		*/
        bkpt_tbl_sz += BKPT_DELTA;
	bkpt_tbl = gdb_realloc(bkpt_tbl, bkpt_tbl_sz * sizeof(unsigned long));
    }
    bkpt_tbl[bkpt_tbl_index] = address;
    
    qsort(bkpt_tbl, bkpt_tbl_index+1, 		/* always keep table sorted		*/
          sizeof(unsigned long), qsort_compar_bkpt);
   
    fix_pc_area_if_necessary(address);
    
    if (0)
	for (i = 0; i <= bkpt_tbl_index; ++i)
	    gdb_printf("after add: %2d. 0x%.8lX\n", i+1, bkpt_tbl[i]);
}


/*-------------------------------------------------------------------------------------*
 | delete_breakpoint - gdb_special_events() callback called when breakpoint is deleted |
 *-------------------------------------------------------------------------------------*
 
 We get control here whenever a breakpoint is deleted.  Here we keep remove the entry
 from our bkpt_tbl.
*/

static void delete_breakpoint(unsigned long address, int enabled)
{
    int i, j;
     
    i = find_breakpoint(address);		/* find the breakpoint			*/
    if (i >= 0) {				/* if found, delete it...		*/
	 j = i++;				/* ...do it by moving all the items	*/
	 while (i <= bkpt_tbl_index)		/*    one entry lower in the bkpt_tbl	*/
	     bkpt_tbl[j++] = bkpt_tbl[i++];	/*    starting with 1 beyond the one 	*/
	 --bkpt_tbl_index;			/*    that was found			*/
    }
	
    fix_pc_area_if_necessary(address);
    
    if (0)
	for (i = 0; i <= bkpt_tbl_index; ++i)
	    gdb_printf("after delete: %2d. 0x%.8lX\n", i+1, bkpt_tbl[i]);
}


/*---------------------------------------------------------------------------------------*
 | changed_breakpoint - gdb_special_events() callback called when breakpoint is modified |
 *---------------------------------------------------------------------------------------*

 We get control here whenever a breakpoint status is changed.  If the breakpoint is being
 disabled we simply delete it from our bkpt_tbl as if a delete were done.  If it's being
 enabled we have nothing to do (we already have it, we assume).  If its being enabled
 and we don't have it we reverse the delete by adding it back to the table as if it
 were a new breakpoint.
*/

static void changed_breakpoint(unsigned long address, int enabled)
{
    int i;
    
    i = find_breakpoint(address);		/* find the breakpoint			*/
    if (i < 0) {				/* if not found...			*/
    	if (enabled)				/* ...if being enabled...		*/
    	    new_breakpoint(address, 1);		/* ...just recreate it in bkpt_tbl	*/
	return;
    }
    
    if (!enabled)				/* if found and being disabled...	*/
    	delete_breakpoint(address, 0);		/* ...delete it				*/
  
    if (0)
	for (i = 0; i <= bkpt_tbl_index; ++i)
	    gdb_printf("after change: %2d. 0x%.8lX\n", i+1, bkpt_tbl[i]);
}


/*------------------------------------------------------------------*
 | registers_changed - update the side bar if any registers changed |
 *------------------------------------------------------------------*/

static void registers_changed(void)
{
    __display_side_bar(NULL, 0);
}


/*--------------------------------------*
 | state_changed - handle state changes |
 *--------------------------------------*
 
 Currently this is only for debugging.
*/

void state_changed(GdbState newState)
{
    switch (newState) {
	case Gdb_Not_Active:			/* gdb is not active (it's exiting)	*/
	    fprintf(stderr, "еее state not active\n");
	    break;
      	case Gdb_Active:			/* gdb is becomming active		*/
	    fprintf(stderr, "еее state active\n");
	    break;
      	case Gdb_Target_Loaded:			/* gdb just loaded target program	*/
	    fprintf(stderr, "еее state target loaded\n");
	    break;
      	case Gdb_Target_Exited:			/* target program has exited		*/
	    fprintf(stderr, "еее state target exited\n");
	    break;
      	case Gdb_Target_Running:		/* target brogram is going to run	*/
	    fprintf(stderr, "еее state target running\n");
	    break;
      	case Gdb_Target_Stopped:		/* target program has stopped		*/
	    fprintf(stderr, "еее state target stopped\n");
	    break;
    }
}

/*--------------------------------------------------------------------------------------*/

/*-------------------------------------------*
 | init_macsbug_patches - set up the patches |
 *-------------------------------------------*/

void init_macsbug_patches(void)
{
    static int firsttime = 1;

    if (firsttime) {
	firsttime = 0;
	
	gdb_run_command = gdb_replace_command("run", run_command);
	if (!gdb_run_command)
	    gdb_internal_error("internal error - run command not found");
	
	#if 0
	gdb_set_command = gdb_replace_command("set", set_command);
	if (!gdb_set_command)
	    gdb_internal_error("internal error - set command not found");
	#endif
	
	#if 1
	gdb_help_command = gdb_replace_command("help", help_command);
	if (!gdb_help_command)
	    gdb_internal_error("internal error - help command not found");
	#endif
		    
	gdb_shell_command = gdb_replace_command("shell", shell_command);
	if (!gdb_shell_command)
	    gdb_internal_error("internal error - shell command not found");
	
	gdb_make_command = gdb_replace_command("make", make_command);
	if (!gdb_make_command)
	    gdb_internal_error("internal error - make command not found");
 	
	gdb_list_command = gdb_replace_command("list", list_command);
	if (!gdb_list_command)
	    gdb_internal_error("internal error - list command not found");
 	
	gdb_next_command = gdb_replace_command("next", next_command);
	if (!gdb_next_command)
	    gdb_internal_error("internal error - next command not found");
 	
	gdb_step_command = gdb_replace_command("step", step_command);
	if (!gdb_step_command)
	    gdb_internal_error("internal error - step command not found");
 	
	gdb_nexti_command = gdb_replace_command("nexti", nexti_command);
	if (!gdb_nexti_command)
	    gdb_internal_error("internal error - nexti command not found");
 	
	gdb_stepi_command = gdb_replace_command("stepi", stepi_command);
	if (!gdb_stepi_command)
	    gdb_internal_error("internal error - stepi command not found");
	
	gdb_define_command = gdb_replace_command("define", define_command);
	if (!gdb_shell_command)
	    gdb_internal_error("internal error - define command not found");
	
	gdb_document_command = gdb_replace_command("document", document_command);
	if (!gdb_document_command)
	    gdb_internal_error("internal error - document command not found");
	 	    
	gdb_if_command = gdb_replace_command("if", if_command);
	if (!gdb_if_command)
	    gdb_internal_error("internal error - if command not found");
		    
	gdb_while_command = gdb_replace_command("while", while_command);
	if (!gdb_while_command)
	    gdb_internal_error("internal error - while command not found");
		    
	gdb_printf_command = gdb_replace_command("printf", NULL); 
	if (!gdb_printf_command)
	    gdb_internal_error("internal error - printf command not found");
 	
	gdb_file_command = gdb_replace_command("file", file_command); 
	if (!gdb_file_command)
	    gdb_internal_error("internal error - file command not found");
 	
	gdb_attach_command = gdb_replace_command("attach", attach_command); 
	if (!gdb_attach_command)
	    gdb_internal_error("internal error - attach command not found");
 	
	gdb_symbol_file_command = gdb_replace_command("symbol-file", symbol_file_command); 
	if (!gdb_symbol_file_command)
	    gdb_internal_error("internal error - symbol-file command not found");
 	
	#if 0
	gdb_sharedlibrary_command = gdb_replace_command("sharedlibrary", sharedlibrary_command); 
	if (!gdb_sharedlibrary_command)
	    gdb_internal_error("internal error - sharedlibrary command not found");
	#endif
 	
	gdb_load_command = gdb_replace_command("load", load_command); 
	if (!gdb_load_command)
	    gdb_internal_error("internal error - load command not found");
	
	#if 0
	quit_command = gdb_replace_command("quit", quit_command1);
	if (!quit_command)
	    gdb_internal_error("internal error - quit command not found");
	#else
	gdb_define_exit_handler(exit_handler);
	#endif
   
    	prev_SIGCONT_handler  = signal(SIGCONT,  my_signal_handler);
        prev_SIGWINCH_handler = signal(SIGWINCH, my_signal_handler);
	//prev_SIGINT_handler = signal(SIGINT,   my_signal_handler);
	//prev_SIGTSTP_handler= signal(SIGTSTP,  my_signal_handler);
		
	gdb_special_events(Gdb_After_Creating_Breakpoint,  (Gdb_Callback)new_breakpoint);
	gdb_special_events(Gdb_Before_Deleting_Breakpoint, (Gdb_Callback)delete_breakpoint);
	gdb_special_events(Gdb_After_Modified_Breakpoint,  (Gdb_Callback)changed_breakpoint);
	//gdb_special_events(Gdb_After_Register_Changed,     (Gdb_Callback)registers_changed);
	//gdb_special_events(Gdb_State_Changed,              (Gdb_Callback)state_changed);
    }
}
