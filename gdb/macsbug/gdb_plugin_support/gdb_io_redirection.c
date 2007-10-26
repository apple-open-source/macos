/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                 gdb_io_redirection.c                                 |
 |                                                                                      |
 |                               I/O Redirection Routines                               |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2006                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*

 All input/output redirection routines are in this file.  Input redirection allows a
 plugin to intercept all command line command input.  Output redirection allows 
 intercepting of all stdout and stderr output.  Because a plugin does not have any
 control over gdb's displays this output redirection "tricks" gdb into passing all
 the displays through user specified routines.
*/

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>

#include "gdb_private_interfaces.h"

#include "top.h" 	// line, linesize
#include "ui-file.h"
#include "ui-out.h"
#include "cli-out.h"
#include "event-top.h"	// input_handler
#include "gdbarch.h"
#include "interps.h"

char *current_cmd_line = NULL;

#define DEBUG 0
#define BUFSIZE (30*132)

GDB_FILE *gdb_default_stdout = (GDB_FILE*)1; /* default (gdb's original) gdb_stdout	*/
GDB_FILE *gdb_default_stderr = (GDB_FILE*)2; /* default (gdb's original) gdb_stderr	*/
GDB_FILE *gdb_current_stdout = (GDB_FILE*)1; /* current gdb stdout stream		*/
GDB_FILE *gdb_current_stderr = (GDB_FILE*)2; /* current gdb stderr stream		*/ 

static int continued_line = 0;		  /* continued line count for prompts		*/
static struct interp *console_interp;	  /* ptr to gdb's console interpreter		*/

static GDB_FILE *prev_default_stdout_stream = (GDB_FILE*)1;
static GDB_FILE *prev_default_stderr_stream = (GDB_FILE*)2;

int (*__default_gdb_query_hook)(char *, va_list) = NULL; /* can be set for special event*/
static void (*default_rl_startup_hook)() = NULL;

typedef char *(*Command_line_input_hook)(char *, int, char *);

static Command_line_input_hook default_command_line_input_hook  = NULL;
static Gdb_Raw_Input_Handler   users_raw_input_handler 	 	= NULL;
static Gdb_Raw_Input_Set_Prompt users_raw_input_prompt_handler  = NULL;
static Gdb_Prompt_Positioning  user_prompt_positioning_function = NULL;

/* All data associated with the current redirected output is defined by the following	*/
/* layout:										*/

typedef struct gdb_file {		  /* Redirected output data:			*/
    int  	    *magic_nbr;		  /*	magic nbr to verify data		*/
    
    GDB_FILE 	    *prev_stream;	  /* 	old stream prior to redirecting to this	*/
    
    struct ui_file  *uifile;		  /* 	output controller			*/
    struct ui_out   *uiout;		  /* 	backpointer to the stream variable	*/
    
    FILE 	    	    *f;		  /*	stdout or stderr			*/
    gdb_output_filter_ftype filter;	  /*	output filter hook			*/
    void 	            *data;	  /*	user data passed to the filter hook	*/
    
    struct gdb_file *next, *prev;	  /*    open_recovery_chain link		*/ 
    unsigned long   refCount;		  /*    use links iff refCount >= open_refCount	*/
    
    int 	    writing_line;	  /*	true ==> currently printing a line	*/
    char            *bp;		  /* 	ptr to next free byte in buffer		*/
    char 	    buffer[BUFSIZE+1];	  /*	buffer to save output line		*/
} gdb_file;

static gdb_file *open_recovery_chain_head = NULL; /* used to reclaim opens iff errors	*/
static gdb_file *open_recovery_chain_tail = NULL; /* used to reclaim opens iff errors	*/
static unsigned long open_refCount        = 0;	  /* "clock" to not reclaim old opens	*/

//static int magic;			  /* simple check on this address 		*/
extern int ui_file_magic;
#define magic ui_file_magic		  /* all plugin support libs use this 1 instance*/
#define CHECK_MAGIC(p, func)  if ((p)->magic_nbr != &magic) \
    			          internal_error(__FILE__, __LINE__, #func ": bad magic number for %s", \
				  	          p->f == stderr ? "stderr" : "stdout")

/* List entries of unknown stream redirection values, i.e., corresponding to streams	*/
/* that gdb_open_output() has not created.  An entry one this list contains all the 	*/
/* critical settings needed to redirect gdb output.					*/

typedef struct UnknownRedirection {	  /* Redirection data for unknown streams:	*/
    struct UnknownRedirection *prev;	  /*	list entries are doubly linked		*/
    struct UnknownRedirection *next;
    
    struct ui_file *stream;		  /*	redirection data is for this stream	*/
    FILE	   *f;			  /* 	corresponds to stdout or stderr		*/
    
    struct ui_file *uiout;		  /*	associated redirection data...		*/
    void (*completion_hook)(char **, int, int);
    int (*query_hook)(char *, va_list);
    void (*rl_startup_hook)();
    Command_line_input_hook command_line_input_hook;
} UnknownRedirection;

static UnknownRedirection *unknown_redirections      = NULL; /* head of unknowns list	*/
static UnknownRedirection *unknown_redirections_tail = NULL; /* tail of unknowns list	*/
	      
static int my_query_hook(char *format, va_list ap);
static void my_rl_startup_hook(void);
static char *my_command_line_input_hook(char *, int , char *);

#if DEBUG
#define DEBUG1(func) fprintf(stdout, "\n--- " #func " ---\n");
#define DEBUG2(func, s, len)						\
    {									\
    	int  _i, _l = len;						\
    	fprintf(stdout, "\n--- " #func " --- (");			\
    	for (_i = 0; _i < _l; ++_i) 					\
    	    if (s[_i] == '\t')						\
    	    	fprintf(stdout, "\\t");					\
    	    else if (s[_i] == '\n')					\
    	    	fprintf(stdout, "\\n");					\
    	    else							\
    	    	fprintf(stdout, "%c", s[_i]);				\
    	fprintf(stdout, ") (");						\
    	for (_i = 0; _i < _l; ++_i) fprintf(stdout, "%.2X", s[_i]);	\
    	fprintf(stdout, ")\n");						\
    }
#else
#define DEBUG1(func)
#define DEBUG2(func, s, len)
#endif

/*--------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------*
 | save_line_segment - save all stdout/stderr line segments and filter full lines |
 *--------------------------------------------------------------------------------*
 
 Each line segment sent to stdout and stderr is passed through here.  Line segments ending
 with newlines are passed to the line filter which can either do nothing with the line,
 change it, or indicate that the line is not to be output.
*/

static void save_line_segment(struct ui_file *file, char *segment, int length)
{
    char 	    *p, *line;
    int  	    i;
    struct gdb_file *output = ui_file_data(file);
        
    for (p = segment, i = 0; i < length; ++p, ++i) {
    	*output->bp++ = *p;
    	if (*p == '\n') {
    	    *output->bp = '\0';
    	    line = output->filter ? output->filter(output->f, output->buffer, output->data) 
    	    			  : output->buffer;
    	    if (line) {
    	    	 output->writing_line = 1;
    	    	 fprintf_filtered(file, "%s", line);
    	    	 output->writing_line = 0;
		 if (output->f == stderr) {
		     gdb_fflush(gdb_stdout);
		     gdb_fflush(gdb_stderr);
		 }
    	    }
    	    output->bp = output->buffer;
    	}
    }
}

/*--------------------------------------------------------------------------------------*/

/*----------------------------*
 | file_flush - fflush output |
 *----------------------------*/

static void file_flush(struct ui_file *file)
{
    struct gdb_file *output = ui_file_data(file);
    char            *line;
 
    DEBUG1(file_flush);
    
    CHECK_MAGIC(output, file_flush);
    
    if (output->bp > output->buffer) {
  	*output->bp = '\0';
  	line = output->filter ? output->filter(output->f, output->buffer, output->data) 
  			      : output->buffer;
    	if (line)
    	    fputs(line, output->f); //fprintf(output->f, "%s", line);
    	output->bp = output->buffer;
    }
    
    if (output->filter)
    	output->filter(output->f, NULL, output->data);
    fflush(output->f);
}


/*-------------------------------------*
 | file_write - write buffer to output |
 *-------------------------------------*/

static void file_write(struct ui_file *file, const char *buf, long length_buf)
{
    struct gdb_file *output = ui_file_data(file);
    
    DEBUG2(file_write, buf, length_buf);
        
    CHECK_MAGIC(output, file_write);
    
    if (output->writing_line)
    	fwrite(buf, length_buf, 1, output->f);
    else
    	save_line_segment(file, buf, length_buf);
}


/*-----------------------------------------------------*
 | file_fputs - write a line (string buffer) to output |
 *-----------------------------------------------------*/

static void file_fputs(const char *linebuffer, struct ui_file *file)
{
    struct gdb_file *output = ui_file_data(file);
  
    DEBUG2(file_fputs, linebuffer, strlen(linebuffer));
        
    CHECK_MAGIC(output, file_fputs);
  
    if (output->writing_line)
    	fputs(linebuffer, output->f);
    else
    	save_line_segment(file, linebuffer, strlen(linebuffer));
}


/*--------------------------------------------*
 | file_isatty - determine of stdout is a tty |
 *--------------------------------------------*/

static int file_isatty(struct ui_file *file)
{
    struct gdb_file    *output = ui_file_data(file);
    struct stderr_file *output2;
    FILE	       *f;
  
    DEBUG1(file_isatty);
    
    CHECK_MAGIC(output, file_isatty);
  
    return (isatty(fileno(output->f)));
}


/*---------------------------------------------------------------------------------*
 | file_delete - delete the temporary stdout gdb file created by output_file_new() |
 *---------------------------------------------------------------------------------*/

static void file_delete(struct ui_file *file)
{
    struct gdb_file *output = ui_file_data(file);
    
    DEBUG1(file_delete);
        
    CHECK_MAGIC(output, file_delete);
    
    gdb_fflush(file);
    
    /* Delete file from doubly-linked list if and only if it's refCount is greater than	*/
    /* or equal to open_refCount.  Only those are "fresh", i.e., created since the last	*/
    /* error recovery.  Older ones are "stale" are presumably the user knows they are	*/
    /* valid.										*/
    
    if (output->refCount >= open_refCount) {
	if (output->next)
	    output->next->prev = output->prev;
	if (output->prev)
	    output->prev->next = output->next;
	if (output == open_recovery_chain_head)
	    open_recovery_chain_head = open_recovery_chain_head->next;
	if (output == open_recovery_chain_tail)
	    open_recovery_chain_tail = open_recovery_chain_tail->prev;
    }
    
    xfree(output);
}

/*--------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------*
 | is_unknown_redirection - determine if a stream is one we created |
 *------------------------------------------------------------------*

 This routine is called by only by gdb_redirect_output() to determine whether the
 stream to be redirected to is a known redirection stream (i.e., one that
 gdb_open_output() defined) or is one that someone else outside of our control created.
 This function returns NULL if it IS one of our streams and non-NULL (a pointer to a 
 struct UnknownRedirection list entry) if it is not.
 
 A list is of streams not defined by gdb_open_output() is built by gdb_open_output().
 These are the streams that are in effect at the time gdb_open_output() is called.  It
 saves that stream to allow gdb_close_output() to restore that stream.  It does so by
 calling gdb_redirect_output() which is why this routine is called from there.
 
 The UnknownRedirection list entries contain the redirection data that was in effect
 at the time gdb_open_output() was called.  We "blindly" use it in gdb_redirect_output()
 when we know we are not dealing with one of our own streams.  This appears to be safe
 since all the redirection data we're setting is what we saved when we recorded the
 list entry.
*/
 
static UnknownRedirection *is_unknown_redirection(struct ui_file *stream)
{
    UnknownRedirection *u = unknown_redirections_tail;
    
    while (u) {
    	if (u->stream == stream)
	    return (u);
    	u = u->prev;
    }
    
    return (NULL);
}


/*---------------------------------------------------------------------*
 | is_known_redirection - determine if a stream is a known redirection |
 *---------------------------------------------------------------------*
 
 Streams created by gdb_open_output() are of course our own.  We record these streams on
 a list for error recovery.  That list is also convenient to scan to see if it is one of
 our own streams.  If it is the function returns the stream data (which has a back
 pointer to the stream).  If it isn't, NULL is returned.
 
 This function is used gdb_open_output() to see if the stream in effect at the time
 gdb_open_output() is called is a stream previously created by gdb_open_output() or
 someone else.  We need to save that stream to allow gdb_close_output() to restore it
 when it is called.  But gdb_close_output() will restore it by calling 
 gdb_redirect_output().  It, in turn, will need to know whether the stream is one of our
 own or someone else's (to set the proper redirection values).  So gdb_redirect_output()
 will call is_unknown_redirection() above to check that.  The list that
 is_unknown_redirection() checks is built by gdb_open_output() when it knows that the
 stream being saved is not one it previously created.  And it determines that by calling
 is_known_redirection().
 
 Note that is_known_redirection() is also called by gdb_redirect_output() when 
 is_unknown_redirection() implies it is a known redirection.  It is done merely as an
 assert/safety check.
*/

static gdb_file *is_known_redirection(struct ui_file *stream)
{
    gdb_file *output = open_recovery_chain_tail;
    
    while (output) {
    	if (output->uifile == stream)
    	    return (output);
    	output = output->prev;
    }
    
    return (NULL);
}


/*---------------------------------------------------------------*
 | gdb_open_output - create a new stdout or stderr output stream |
 *---------------------------------------------------------------*
 
 Creates a new output stream pointer for stdout or stderr (specified by f).  When the
 output is redirected by calling gdb_redirect_output() all the stdout or stderr writes
 will be filtered by the specified filter with the following prototype:
 
    char *filter(FILE *f, char *line, void *data);
 
 The filter can return NULL or the filtered line.  NULL implies that the line is not to
 be output or that the filter did the output.
 
 Note that all lines except possibly the last include the terminating '\n'.  The last
 may not have one if there is a partially built line at the time the stream is flushed.
 
 If the line pointer is NULL, then that indicates to the filter that it's output is being
 flushed.  All filters MUST check for line == NULL.
 
 The data parameter passed to gdb_open_output() is passed to the filter as is.  This
 allows the caller to have a communication channel between the gdb_open_output() caller
 and it's filter.

 The function returns a output stream (GDB_FILE) pointer for passing to
 gdb_redirect_output() and gdb_close_output().  Note that this is NOT a stdio FILE* 
 stream pointer so it cannot be used in stdio.h routines.  Indeed it can ONLY be used as
 a parameter to the redirect and close routines just mentioned (see their comments for
 details).
 
 Two GDB_FILE are initially provided; gdb_default_stdout and gdb_default_stderr which
 can be passed to gdb_redirect_output() and gdb_close_output().  These represent the
 stdout and stderr streams the gdb normally uses.  YOU SHOULD NEVER STORE INTO THESE
 POINTERS.
 
 If no filter is passed (i.e., NULL is passed) to gdb_open_output() then
 gdb_default_stdout or gdb_default_stderr (depending on f) is returned as the function
 result.  In other words, while these streams are already open, no harm is done by
 attempting to explicitly reopen them.
 
 Caution - you must be cognizant of which output to redirect if you intend to use other
 gdb commands (e.g., x/i) whose output you are redirecting.  As a general rule stdout
 is usually the stream to be redirected.  Further, since the filter is called for entire
 lines (except possibly for the last) you must ensure all output always ends with a new
 line (\n).
*/

GDB_FILE *gdb_open_output(FILE *f, gdb_output_filter_ftype filter, void *data)
{
    struct gdb_file    *output;
    struct ui_file     *ui_file;
    UnknownRedirection *u;
     
    DEBUG1(output_file_new);
    
    if (f != stdout && f != stderr)
    	internal_error(__FILE__, __LINE__, "attempt to call gdb_open_output for something other than stdout or stderr");
    
    if (!filter) {
    	if (f == stdout) {
	    prev_default_stdout_stream = gdb_stdout;
	    return (gdb_default_stdout);
	} else {
	    prev_default_stderr_stream = gdb_stderr;
	    return (gdb_default_stderr);
	}
    }
    
    output = (struct gdb_file *)xmalloc(sizeof(struct gdb_file));
    
    ui_file = ui_file_new();
          
    set_ui_file_data(ui_file, output, file_delete);
    set_ui_file_flush(ui_file, file_flush);
    set_ui_file_write(ui_file, file_write);
    set_ui_file_fputs(ui_file, file_fputs);
    set_ui_file_isatty(ui_file, file_isatty);
    
    output->magic_nbr	   = &magic;
    
    output->prev_stream    = (f == stdout) ? gdb_stdout : gdb_stderr;
    
    output->uifile	   = ui_file;
    output->uiout 	   = cli_out_new(ui_file);
    
    output->f		   = f;
    output->filter	   = filter;
    output->data	   = data;
    
    output->writing_line   = 0;
    output->bp		   = output->buffer;
    
    output->next     	   = NULL;
    output->prev    	   = open_recovery_chain_tail;
    output->refCount	   = open_refCount;
    
    /* See cleanup_output() for some info on why this open_recovery_chain is being	*/
    /* maintained...									*/
    
    if (open_recovery_chain_tail == NULL)
    	open_recovery_chain_head = open_recovery_chain_tail = output;
    else
    	open_recovery_chain_tail->next = output;
    open_recovery_chain_tail = output;
    
    /* If the output->prev_stream is a stream we previously created then we are		*/
    /* basically done.  If it's someone else's stream then we need to save all the 	*/
    /* current redirection data so that gdb_redirect_output() can use it at the time	*/
    /* gdb_close_output() asks gdb_redirect_output() to redirect back to it.		*/
    
    if (!is_known_redirection(output->prev_stream)) {	/* not known stream...		*/
	u = xmalloc(sizeof(UnknownRedirection));	/* ...add to list of unknowns	*/
	u->next   = NULL;
	u->prev   = unknown_redirections_tail;
	u->stream = output->prev_stream;
	u->f	  = f;
	
	if (unknown_redirections_tail == NULL)
	    unknown_redirections = unknown_redirections_tail = u;
	else
	    unknown_redirections_tail->next = u;
	unknown_redirections_tail = u;
	
	u->uiout		   = uiout;
	u->completion_hook	   = rl_completion_display_matches_hook;
	u->query_hook	           = deprecated_query_hook;
	u->rl_startup_hook	   = rl_startup_hook;
	u->command_line_input_hook = command_line_input_hook;
    }
    
    //fprintf(stderr, "  gdb_open_output: ui_file = %X, data = %X, magic_nbr = %X, &magic = %X\n",
    //				ui_file, output, output->magic_nbr, &magic);
    
    return ((GDB_FILE *)ui_file);
}


/*------------------------------------------------------------------*
 | gdb_redirect_output - redirect gdb output to a new output stream |
 *------------------------------------------------------------------*
 
 Causes all future output from gdb to be filtered with the filter routine associated
 with the specified file (stream pointer) and sent to stdout or stderr also associated 
 with the stream.  These both were specified when the stream pointer was created by
 gdb_open_output().  Redirection continues until another redirection is specified.
 
 The function returns the redirection stream that was in effect at the time of the
 call (i.e., the "old" or previous stream).  
  
 Conceptually, the initial "redirection" state is for gdb_default_stdout for stdout and 
 gdb_default_stderr for stderr.  Passing these to gdb_redirect_output() causes gdb to
 use its standard default output machinery (i.e., as if no redirection was ever done).
 Of course no filtering is done either.
 
 There is no explicit stream specification in gdb output.  The gdb echo and printf
 commands all output to stdout.  Indeed so do all other gdb output commands (e.g., x).
 Thus redirection done here affects the behavior of all gdb output commands.  That is
 what dictated this form of design.
 
 If output is being redirected and thus filtered, there is nothing prohibiting that
 filter from temporarily redirecting its own output to another (possibly already
 opened) stream and thus using another filter to further filter the output.
 
 For example, output could be handled by a stream that does its own screen drawing.  A
 stream could also be set up to filter gdb disassembly output to reformat it.  The
 disassembly filter could then take each reformatted line and temporarily set up
 redirection to print to the screen stream for placement on the screen.
 
 Although the gdb command set does not provide a way to specify stdout or stderr the
 plugin ABI does.  The gdb_printf() and gdb_error() functions are provided to output
 to the current stdout and stderr redirections respectively.  Also provided is
 gdb_fflush() to force flushing of a specified stream.
 
 Note: Due to the way gdb handles output internally, if both stdout and stderr are
 redirected then it is recommended that stderr be redirected before stdout.  Almost all
 gdb output is to stdout streams.  Internally gdb has its own stream variables which are
 modified by this call.  There's only one instance of these streams.  Thus the preference
 should always be to associate the streams with stdout.  Hence it should be the most
 recent steam that is redirected.
 
 See also comments for gdb_special_events() for the Gdb_Word_Completion_Cursor GdbEvent.
 That event is for saving and restoring the word completion (i.e., displaying list of
 alternative names for commands, filenames, etc. or displaying all alternatives) input
 cursor when output is redirected and needs some specialized input cursor handling.
       
 An example illustrating a use for redirection is discussed in the comments for
 gdb_fprintf().
*/

GDB_FILE *gdb_redirect_output(GDB_FILE *stream)
{
    struct gdb_file    *output;
    GDB_FILE           *prev_file;
    UnknownRedirection *u;
    static int         firsttime = 1;
    
    static struct ui_out  *default_gdb_uiout;
    static void (*default_gdb_completion_hook)(char **matches, int len, int max);
            
    if (firsttime) {
	gdb_default_stdout 	    = (GDB_FILE *)INITIAL_GDB_VALUE(gdb_stdout, gdb_stdout);
	gdb_default_stderr 	    = (GDB_FILE *)INITIAL_GDB_VALUE(gdb_stderr, gdb_stderr);
	default_gdb_uiout           = INITIAL_GDB_VALUE(uiout, uiout);
	default_gdb_completion_hook = INITIAL_GDB_VALUE(rl_completion_display_matches_hook, rl_completion_display_matches_hook);
	default_rl_startup_hook	    = INITIAL_GDB_VALUE(rl_startup_hook, rl_startup_hook);
	__default_gdb_query_hook    = deprecated_query_hook;
	
	interp_set_ui_out(console_interp, uiout);
	
	firsttime = 0;
	
	/* Note that the __default_gdb_query_hook can be changed by a Gdb_Before_Query	*/
	/* special event handler.							*/
    }
    
    if (stream == (GDB_FILE *)1)
    	stream = gdb_default_stdout;
    else if (stream == (GDB_FILE *)2)
    	stream = gdb_default_stderr;
	
    if (stream == gdb_default_stdout || stream == gdb_default_stderr) {
    	if (stream == gdb_default_stdout) {
	    prev_file  = prev_default_stdout_stream = (GDB_FILE *)gdb_stdout;
	    gdb_stdout = (struct ui_file *)gdb_default_stdout;
	} else {
	    prev_file  = prev_default_stderr_stream = (GDB_FILE *)gdb_stderr;
	    gdb_stderr = (struct ui_file *)gdb_default_stderr;
	}
	
	uiout                                = default_gdb_uiout;
	rl_completion_display_matches_hook   = default_gdb_completion_hook;
	deprecated_query_hook		     = __default_gdb_query_hook;
	rl_startup_hook			     = default_rl_startup_hook;
	command_line_input_hook		     = default_command_line_input_hook;
	
	interp_set_ui_out(console_interp, uiout);

	//fprintf(stderr, "  gdb_redirect_output0: stream == gdb_default_stdout\n");
    } else {
	if (!stream)
    	    internal_error(__FILE__, __LINE__, "attempt to redirect output to a undefined (NULL) stream");
	
	/* If the stream being redirected to is one of ours then we can use the data	*/
	/* that gdb_open_output() set up.  If it isn't one of ours we use the data we	*/
	/* blindly saved at gdb_open_output() time on the "unknown" stream list.	*/
	
	if ((u = is_unknown_redirection(stream)) != NULL) {
	    if (u->f == stdout) {
		prev_file  = (GDB_FILE *)gdb_stdout;
		gdb_stdout = (struct ui_file *)stream;
	    } else {
		prev_file  = (GDB_FILE *)gdb_stderr;
		gdb_stderr = (struct ui_file *)stream;
	    }
	    
	    uiout 				 = u->uiout;
	    rl_completion_display_matches_hook   = u->completion_hook;
	    deprecated_query_hook		 = u->query_hook;
	    rl_startup_hook			 = u->rl_startup_hook;
	    command_line_input_hook		 = u->command_line_input_hook;
	    
	    interp_set_ui_out(console_interp, uiout);
	    
	    /* Once we use the data we never use it again...				*/
	    
	    if (u->next)
		u->next->prev = u->prev;
	    if (u->prev)
		u->prev->next = u->next;
	    if (u == unknown_redirections)
		unknown_redirections = unknown_redirections->next;
	    if (u == unknown_redirections_tail)
		unknown_redirections_tail = unknown_redirections_tail->prev;
	    xfree(u);
	    //fprintf(stderr, "  gdb_redirect_output2: ui_file = %X, data = %X, magic_nbr = %X, &magic = %X\n",
	    //			    stream, output, output->magic_nbr, &magic);
	} else {
	    if (!is_known_redirection(stream))
	    	internal_error(__FILE__, __LINE__, "attempt to redirect to a undefined stream");
	    output = ui_file_data((struct ui_file *)stream);
	    
	    #if 0
	    if (stream == gdb_default_stdout)
		fprintf(stderr, "    gdb_default_stdout\n");
	    else if (stream == gdb_default_stderr)
		fprintf(stderr, "    gdb_default_stderr\n");
	    fprintf(stderr, "  gdb_redirect_output1: ui_file = %X, data = %X, magic_nbr = %X, &magic = %X\n",
				    stream, output, output->magic_nbr, &magic);
	    #endif
	    
	    if (output->magic_nbr != &magic)
		internal_error(__FILE__, __LINE__, "attempt to redirect to a stream with a bad magic number");
	    
	    if (output->f == stdout) {
		prev_file  = (GDB_FILE *)gdb_stdout;
		gdb_stdout = (struct ui_file *)stream;
	    } else {
		prev_file  = (GDB_FILE *)gdb_stderr;
		gdb_stderr = (struct ui_file *)stream;
	    }
	    
	    uiout 				 = output->uiout;
	    rl_completion_display_matches_hook   = __cmd_completion_display_hook;
	    deprecated_query_hook		 = my_query_hook;
	    rl_startup_hook			 = my_rl_startup_hook;
	    
	    interp_set_ui_out(console_interp, uiout);
	    
	    //fprintf(stderr, "  gdb_redirect_output3: ui_file = %X, data = %X, magic_nbr = %X, &magic = %X\n",
	    //			    stream, output, output->magic_nbr, &magic);
    	}
    }
    
    gdb_current_stdout = gdb_stdout;
    gdb_current_stderr = gdb_stderr;
    
    #if 0
    output = ui_file_data((struct ui_file *)prev_file);
    fprintf(stderr, "  gdb_redirect_output4: ui_file = %X, data = %X, magic_nbr = %X, &magic = %X\n",
    				prev_file, output, output->magic_nbr, &magic);
    if (prev_file == gdb_default_stdout)
    	fprintf(stderr, "    gdb_default_stdout\n");
    else if (prev_file == gdb_default_stderr)
    	fprintf(stderr, "    gdb_default_stderr\n");
    #endif
    
    return (prev_file);
}


/*-----------------------------------------------------*
 | gdb_close_output - close a redirected output stream |
 *-----------------------------------------------------*
 
 This is used to close a stream previously opened by gdb_open_output().  Once closed
 the stream pointer must no longer be passed to gdb_redirect_output().  The current
 redirection after closing is the redirection that was in effect when this stream was
 originally opened (note, opened, NOT redirected).
 
 Since gdb_redirect_output() returns the stream that was in effect at the time of its
 call, then if there are no other gdb_redirect_output() calls between gdb_open_output()
 and that gdb_redirect_output() call, then the stream redirected to after the close
 is the same stream returned by gdb_redirect_output().  For example,
 
     new_stream = gdb_open_output(stdout, my_filter, NULL);
     old_stream = gdb_redirect_output(new_stream);
     - - -
     gdb_close_output(new_stream);
     
 The redirection at this point is to the old_stream since that was also in effect at the
 time the gdb_open_output() was done.  If there are any other redirections between opening
 new_stream and redirecting it then old_stream will not be the same as what's redirected
 to when the gdb_close_output(new_stream) is done.
*/

void gdb_close_output(GDB_FILE *stream)
{
    struct gdb_file *output;
    GDB_FILE	    *prev_stream;
    
    if (stream == (GDB_FILE *)1)
    	stream = gdb_default_stdout;
    else if (stream == (GDB_FILE *)2)
    	stream = gdb_default_stderr;
    
    if (stream == gdb_default_stdout) {
    	fflush(stdout);
	(void)gdb_redirect_output(prev_default_stdout_stream);
	return;
    }
    
    if (stream == gdb_default_stderr) {
    	fflush(stderr);
	(void)gdb_redirect_output(prev_default_stderr_stream);
    	return;
    }
    
    output = ui_file_data((struct ui_file *)stream);
    if (output->magic_nbr != &magic)
	internal_error(__FILE__, __LINE__, "attempt to close to a stream with a bad magic number");

    //fprintf(stderr, "  gdb_close_output1: ui_file = %X, data = %X, magic_nbr = %X, &magic = %X\n",
    //				stream, output, output->magic_nbr, &magic);
    
    gdb_fflush((struct ui_file *)stream);
    xfree(output->uiout);
    prev_stream = output->prev_stream;
    file_delete((struct ui_file *)stream);
    
    #if 0
    output = ui_file_data((struct ui_file *)prev_stream);
    fprintf(stderr, "  gdb_close_output2: ui_file = %X, data = %X, magic_nbr = %X, &magic = %X\n",
    				prev_stream, output, output->magic_nbr, &magic);
    if (prev_stream == gdb_default_stdout)
    	fprintf(stderr, "    gdb_default_stdout\n");
    else if (prev_stream == gdb_default_stderr)
    	fprintf(stderr, "    gdb_default_stderr\n");
    #endif
    
    (void)gdb_redirect_output(prev_stream);
}


/*---------------------------*
 | gdb_fflush - flush stream |
 *---------------------------*
 
 Makes sure all stream output is written.  The stream pointer should have been previously
 created by gdb_open_output().
*/

void gdb_fflush(GDB_FILE *stream)
{
    if (stream == (GDB_FILE *)1)
    	stream = gdb_stdout;
    else if (stream == (GDB_FILE *)2)
    	stream = gdb_stderr;
	
    gdb_flush((struct ui_file *)stream);
}


/*---------------------------------------------------------------------------*
 | gdb_define_raw_input_handler - define a routine to handle raw input lines |
 *---------------------------------------------------------------------------*
 
 Gdb has a mode where it basically reads raw lines from the terminal (as opposed to
 command lines).  This occurs when a COMMANDS, DEFINE, DOCUMENT, IF, or WHILE command
 is entered from the terminal (as opposed to a script), i.e., interactively.  It also
 reads control structures (i.e.., WHILE and IF) this way.  
 
 gdb_define_raw_input_handler() allows you to specify an handler to filter the raw
 data lines before gdb saves them as the lines making up the body of the control
 command.  The handler has the following prototype:
 
   void raw_input_handler(char *theRawLine);
   
 This allows for handler to look at the lines before gdb sees them and also to possibly
 echo the lines elsewhere in the display.  Whatever...
 
 Only one handler may exist at any point in time.  Specifying NULL as theInputHandler 
 reverts back to gdb's original behavior.
 
 Note, that the prompt gdb uses is not the standard prompt.  By default it is a '>'
 appropriately indented to show control structure nesting depth.  This however may be
 changed by calling gdb_set_raw_input_prompt_handler() to define a handler which
 can return the desired prompt.
 
 If the specified stream is associated with a filter that is controlling the display
 wants the prompts in a position other than the normal gdb default then the SET prompt
 can be used to define the standard prompt with xterm terminal positioning controls
 (or whatever is appropriate for the terminal).  But since that cannot be done with the
 raw data line prompt gdb_define_raw_input_handler() is provided to let you get control
 just before ANY prompt is displayed by gdb during the raw input.
       
 Caution: It appears that gdb has a tendency to reset itself to its own handler under
          some conditions (one known is using CTL-C).  So you may need to recall
	  gdb_define_raw_input_handler() to reestablish your raw input handler.
*/
 
void gdb_define_raw_input_handler(Gdb_Raw_Input_Handler theInputHandler)
{
    if (theInputHandler)
    	command_line_input_hook = my_command_line_input_hook;
    else
    	command_line_input_hook = default_command_line_input_hook;
    	
    users_raw_input_handler = theInputHandler;
}


/*-------------------------------------------------------------*
 | gdb_set_raw_input_prompt_handler - set prompt for raw input |
 *-------------------------------------------------------------*
 
 This defines a handler that can be used to set the prompt used during raw input. The
 handler has the following prototype:
 
   void thePromptHandler(char *prompt);
 
 Specifying NULL for the thePromptHandler removes the handler and gdb reverts to it's
 standard '>' prompt.  The handler is expected to modify the 256-character prompt buffer
 with the desired prompt.
 
 Caution/Warning: The prompt gdb is using is for raw input is in a 256-character 
 local buffer in cli-script.c:read_next_line,  This is on the call chain so it can
 be legally (!) accessed.  But remember it has the 256 limit.  In order to get gdb
 to use the desired prompt that specific buffer must be modified.
*/

void gdb_set_raw_input_prompt_handler(Gdb_Raw_Input_Set_Prompt thePromptHandler)
{
    users_raw_input_prompt_handler = thePromptHandler;
}


/*----------------------------------------------------------------------*
 | gdb_control_prompt_position - allow user to control where prompts go |
 *----------------------------------------------------------------------*
 
 This routine allows you to specify a routine which will get control just before any
 prompt the gdb displays prior to reading from the stdin command line (excluding
 queries).  While you cannot change the prompt (other than gdb's standard prompt with
 a set prompt command) you can use the positioning function to do as that name implies,
 namely control the position of where the prompt will be displayed.  This is useful
 when using a redirected output where the associated filter want's to the prompt in
 some specific position.
 
 The positioningFunction should have the following prototype:
 
   void positioningFunction(int continued);
   
 The continued parameter is 0 unless the prompt is going to be for a continued line,
 i.e., the input line had a '\' at the end.  In other words, continued is the number
 of lines entered before the upcoming line that have been continued.  The
 positioningFunction() might want to use this to adjust the position of the prompt.
 
 Specifying NULL for the positioningFunction removes the positioning control.
 
 Note, also see gdb_define_raw_input_handler().  It discusses why you would want to
 use gdb_control_prompt_position() to control the prompt positioning.
 
 It should be pointed out that while it is tempting to use this routine to always
 control the prompt position, it is not recommended for the standard gdb prompt which
 is controlled by a SET prompt command.  That's because apparently gdb uses the current
 standard prompt for its history display and scrolling through that display will not
 appear on the screen where your prompt is positioned if it's positioned separately.
*/

void gdb_control_prompt_position(Gdb_Prompt_Positioning positioningFunction)
{
    user_prompt_positioning_function = positioningFunction;
}


#if 0 // obsolete since binutils asm now uses gdb output conventions
/*--------------------------------------------------*
 | my_disasm_fprintf - intercept disassembly output |
 *--------------------------------------------------*
 
 This routine is defined as the disassembly printf() function for disassmblies generated
 from binutils routines.  It was set up by gdb_redirect_output() above so that we can
 filter disassembly output just like all other output generated to stdout/stderr by gdb.
*/

static int my_disasm_fprintf(struct ui_file *stream, const char *fmt, ...)
{
    va_list ap;
    int     i;
    char    line[BUFSIZE+1];
    
    va_start(ap, fmt);
    i = vsprintf(line, fmt, ap);			/* length might be usefull (?)	*/
    va_end(ap);
    
    DEBUG2(my_disasm_fprintf, line, i);
    
    save_line_segment(stream, line, strlen(line));	/* give opportunity to filter	*/
    
    return (i);
}
#endif


/*----------------------------------------------------------------*
 | call_default_query_hook - call a previously defined query hook |
 *----------------------------------------------------------------*
 
 This is used only by my_query_hook() to convert a parameter list into a va_list which
 is the form query hooks expect.
*/

static int call_default_query_hook(char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    __default_gdb_query_hook(format, ap);
    va_end(ap);
}


/*---------------------------------------*
 | my_query_hook - intercept gdb queries |
 *---------------------------------------*
 
 When output is redirected to someplace other than gdb_default_stdout and
 gdb_default_stderr we have this hook installed by gdb_redirect_output() to allow
 us to intercept all queries done by gdb's query() routine.  This gives us a chance
 to flush our current streams just in case those streams are using filters which buffer
 their output.
*/

static int my_query_hook(char *format, va_list ap)
{
    int  result;
    char msg[1024];
    
    gdb_fflush(gdb_current_stdout);
    gdb_fflush(gdb_current_stderr);
    
    vsprintf(msg, format, ap);
    deprecated_query_hook = NULL;		/* avoid recursion			*/
    
    if (__default_gdb_query_hook)
    	result = call_default_query_hook("%s", msg);
    else
    	result = query("%s", msg);
    
    gdb_fprintf(gdb_current_stdout, "\n");	/* query() writes to stdout		*/
    gdb_fflush(gdb_current_stdout);
    gdb_fflush(gdb_current_stderr);
    
    deprecated_query_hook = my_query_hook;
        
    return (result);
}


/*--------------------------------------------------------------*
 | my_rl_startup_hook - intercept all prompts from the terminal |
 *--------------------------------------------------------------*
 
 This hook called just before *all* prompts to the terminal and only if the output is
 redirected to someplace other than gdb_default_stdout and gdb_default_stderr. This gives
 us a chance to flush the current redirected streams just in case those streams are using
 filters which buffer their output.
 
 Because the user might want to control the prompt position, an hook is provided,
 user_prompt_positioning_function, which is set by gdb_control_prompt_position().
*/

static void my_rl_startup_hook(void)
{
    if (default_rl_startup_hook)
    	(*default_rl_startup_hook)();
    
    if (user_prompt_positioning_function)
    	user_prompt_positioning_function(continued_line);
    
    gdb_fflush(gdb_current_stdout);
    gdb_fflush(gdb_current_stderr);
}


/*---------------------------------------------------------------------*
 | my_command_line_input_hook - intercept all raw command line prompts |
 *---------------------------------------------------------------------*
 
 This is a command_line_input_hook to allow the user to get control before each raw
 command line input prompt (see gdb_define_raw_input_prompt_handler() for details).
*/

static char *my_command_line_input_hook(char *prompt, int repeat, char *annotation_suffix)
{
    char *result;

    command_line_input_hook = NULL;		/* prevent recursion			*/
    
    if (prompt && users_raw_input_prompt_handler)
    	users_raw_input_prompt_handler(prompt);
    
    if (default_command_line_input_hook)
    	result = default_command_line_input_hook(prompt, repeat, annotation_suffix);
    else
    	result = command_line_input(prompt, repeat, annotation_suffix);
    
    command_line_input_hook = my_command_line_input_hook;
    
    if (users_raw_input_handler)
    	users_raw_input_handler(result);
        
    return (result);
}

/*--------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------*
 | cleanup_output - error recovery to close new redirection streams  |
 *-------------------------------------------------------------------*
 
 This is a gdb cleanup function to close all output streams opened since the last command
 was executed.  This is called only when an error was reported.  Command errors do not
 return to the caller that caused them.  Rather they have their own error recovery before
 putting up another prompt.  That error recovery includes calling all registered cleanup
 functions, of which this is one.  This is the only way to clean up opened output streams
 that the command might have created.
 
 With few exceptions most routines only temporarily open redirection streams and then
 close them as soon as they are done (a screen display might be one of those exceptions).
 But if an error occurs in gdb before the routine has a chance to close the stream it
 never will since, as just mentioned, error recovery does not return.  Thus this routine
 will be used to close the stream.
 
 Only streams created as a result of the last command executed from the stdin command
 line are closed.  All others are left alone.  This is controlled by maintaining a
 doubly linked list of all opened streams where the list is initially empty before 
 executing each stdin command and using a usage (reference) counter as a "clock". Only
 those list entries with a "time" greater than the time of the command are closed.
 
 Closing is in the reverse order of opening so that the redirection should be as it was
 prior to executing the command.
*/ 

static void cleanup_output(void *data) 
{
    gdb_file *prev;
    
    while (open_recovery_chain_tail) {
    	prev = open_recovery_chain_tail->prev;
	gdb_close_output(open_recovery_chain_tail->uifile);
	open_recovery_chain_tail = prev;
    }
}

static void (*gdb_input_handler)(char *) = NULL;

/*-------------------------------------------------------------------------------------*
 | output_recovery_stdin_hook - intercept all stdin commands to handle output recovery |
 *-------------------------------------------------------------------------------------*
 
 __initialize_output() defines this as gdb's input_handler.  Thus it will get control
 prior to executing any gdb commands read from the stdin command line.  It's sole goal
 in life is to initialize the head/tail pointers of the double linked open stream list
 and to bump the "clock" used to identify all "freshly" open streams created as a result
 of executing the command.  It then sets up the gdb cleanup machinery so that 
 cleanup_output() above gets control if there's an error reported for the command.  This
 is how we attempt to clean up open streams created since the last command.
*/

static void output_recovery_stdin_hook(char *commandLine)
{
    struct cleanup *old_chain = make_cleanup(cleanup_output, NULL);
    
    open_recovery_chain_head = open_recovery_chain_tail = NULL;
    ++open_refCount;
   
    if (gdb_input_handler)
    	gdb_input_handler(commandLine);
    
    discard_cleanups(old_chain);
}

/*--------------------------------------------------------------------------------------*/

typedef struct stdin_file {			/* gdb_redirect_stdin() stack entries:	*/
   struct stdin_file 	       *prev;		/*	previous stdin handler		*/
   
   gdb_stdin_preprocess_ftype  preprocess;	/*	user's preprocessing routine	*/
   gdb_stdin_postprocess_ftype postprocess;	/*	paired postprocessing routine	*/
   void 	               *data;		/*   	user data passed to filter	*/
   
   void (*prev_input_handler)(char *);		/* 	previous input handler		*/
} stdin_file;

static stdin_file *current_stdin = NULL; 	/* currently redirected stdin		*/

static void recover_stdin_error(void *data)
{
   /* not currently used */
}


/*--------------------------------------------------------*
 | my_stdin_hook - gdb_redirect_stdin() command line hook |
 *--------------------------------------------------------*
 
 If gdb_redirect_stdin() is used to process stdin, then gdb's input_handler stdin command
 line hook is changed to point here.  This allows us to pre and post process the command.
 
 Note that once this hook is active the input_handler call chain looks like the following:
 
     my_stdin_hook() --> output_recovery_stdin_hook() --> gdb (original input_handler)
*/

static void my_stdin_hook(char *commandLine)
{
    int len;
    //struct cleanup *old_chain;
    
    if (!commandLine) {
  	if (current_stdin->prev_input_handler)
	    current_stdin->prev_input_handler(commandLine);
	return;
    }
    	
    //old_chain = make_cleanup(recover_stdin_error, NULL);
    
    /* If the input line is has a '\' at the end then it's continued.  So we bump the	*/
    /* continued which my_rl_startup_hook() passes to the user if the uer prompt hook	*/
    /* is installed.  It could be useful to move the prompt for continued lines.	*/
    
    len = strlen(commandLine);
    if (len > 0 && commandLine[len - 1] == '\\')
    	++continued_line;
    else
    	continued_line = 0;
	
    if (current_stdin->preprocess) {
	char *p = commandLine;
	while (*p == ' ' || *p == '\t')		/* remove leading blanks		*/
	    ++p;
	if (p > commandLine)
	    strcpy(commandLine, p);
	current_stdin->preprocess(commandLine, current_stdin->data);
    }
    
    current_cmd_line = commandLine;

    if (current_stdin->prev_input_handler) {
	//input_handler = gdb_input_handler;
	current_stdin->prev_input_handler(commandLine);
	//input_handler = my_stdin_hook;
    }
    
    if (current_stdin->postprocess)
	current_stdin->postprocess(current_stdin->data);
}


/*-------------------------------------------------------------*
 | gdb_redirect_stdin - create a new command line stdin stream |
 *-------------------------------------------------------------*
 
 Causes all future stdin command line stream input be pre and post processed by the
 specified routines (either can be specified as NULL if one or the other is not needed).
 
 The preprocessing routine has the following prototype:
 
     void preprocess(char *commandLine, void *data);
     
 The preprocess function may look at or change the contents of the command line (you
 should assume that the command line was malloc'ed so you can change it's size and
 contents if you wish).  By the time the preprocessor sees the line all leading blanks
 and tabs have been removed. 
 
 The postprocessing routine has the following prototype:
 
     void postprocess(void *data);
     
 The data parameter is passed to these routines is the data parameter passed to 
 gdb_open_stdin().  It permits the caller to pass additional information among the
 pre and postprocessors and to communicate back to the gdb_open_stdin() caller.
 
 Only one redirection is active at any one time but nesting of redirections is allowed.
 A redirection stays in effect until gdb_close_stdin() is called.
*/

void gdb_redirect_stdin(gdb_stdin_preprocess_ftype  stdin_preprocess, 
		        gdb_stdin_postprocess_ftype stdin_postprocess, void *data)
{
    stdin_file *new_stdin = (stdin_file *)xmalloc(sizeof(stdin_file));
    
    new_stdin->prev  	   = current_stdin;
    current_stdin          = new_stdin;
    
    new_stdin->preprocess  = stdin_preprocess;
    new_stdin->postprocess = stdin_postprocess;
    new_stdin->data        = data;
    
    new_stdin->prev_input_handler = input_handler;
    input_handler 	   = my_stdin_hook;
}


/*----------------------------------------------------------------*
 | gdb_close_stdin - close a stdin command line processing stream |
 *----------------------------------------------------------------*
 
 Causes stdin command line redirection to revert to the state it had prior to the
 most recent gdb_redirect_stdin() call.
 */

void gdb_close_stdin(void)
{
    if (current_stdin) {
    	stdin_file *stream = current_stdin;
	input_handler = current_stdin->prev_input_handler;
    	current_stdin = current_stdin->prev;
    	xfree(stream);
    }
}


/*----------------------------------------------------------------------------------*
 | gdb_set_previous_command - replace gdb's previous command line with a replacment |
 *----------------------------------------------------------------------------------*
 
 A null line is entered from a terminal command line tells gdb to repeat the previous
 command.  Calling gdb_set_previous_command() and specifying a command line of your own
 gives you control over what that command is to be.
 
 You generally would use this routine in a gdb_redirect_stdin() preprocess routine to
 control gdb's behavior of your own repeated plugin commands if the desired behavior is
 not the gdb default.  There's nothing stopping you from doing this from the preprocess
 routine directly on the (malloc'ed) command line passed to it.  Gdb will obediently
 execute the command AND record it in its command history list.  You might not want that
 to happen, i.e., recording your replacement every time the user types a return on the
 command line.
 
 What gdb does for a empty command line however is NOT record it in its command history.
 Instead it simply uses the previous command line.  By calling gdb_set_previous_command()
 you can change what that previous line is.
*/

void gdb_set_previous_command(char *replacement_line)
{
    int linelength = strlen(replacement_line);
    
    if (strlen(replacement_line) > linesize) {	/* line and linesize are gdb's prev line*/
    	line = gdb_realloc(line, linelength);	/* its malloc'ed space			*/
	linesize = linelength;
    }
    
    strcpy(line, replacement_line);		/* set gdb's previous command line	*/
}


/*-------------------------------------------------------------------------*
 | gdb_get_previous_command - return a copy of gdb's previous command line |
 *-------------------------------------------------------------------------*
 
 This returns a copy of gdb's previous command line.  This is the line gdb would
 execute when a null line is entered as a command form the keyboard.  The function
 returns a (malloc'ed) pointer to a copy of the line or NULL if it doesn't exist.
*/

char *gdb_get_previous_command(void)
{
    return (line ? strcpy((char *)gdb_malloc(strlen(line) + 1), line) : NULL);
}

/*--------------------------------------------------------------------------------------*/

/*--------------------------------------------------*
 | __initialize_io - I/O redirection initialization |
 *--------------------------------------------------*
 
 This is called as part of overall gdb support initialization to initialize the stuff
 that needs initializing in this file.
*/

void __initialize_io(void)
{
    static initialized = 0;
    
    if (!initialized) {
    	initialized = 1;
	
	console_interp = interp_lookup(INTERP_CONSOLE);
	
    	gdb_redirect_output(gdb_default_stdout = gdb_open_output(stdout, NULL, NULL));
    	gdb_redirect_output(gdb_default_stderr = gdb_open_output(stderr, NULL, NULL));
	
	#if 0
	if (ui_file_magic)
	     gdb_input_handler = (void (*)(char*))ui_file_magic;
	else
	    gdb_input_handler = (void (*)(char*))ui_file_magic = input_handler;
	input_handler = output_recovery_stdin_hook;
	default_command_line_input_hook = command_line_input_hook;
	#else
    	gdb_input_handler		= INITIAL_GDB_VALUE(input_handler, input_handler);
    	default_command_line_input_hook = INITIAL_GDB_VALUE(command_line_input_hook, command_line_input_hook);
	input_handler = output_recovery_stdin_hook;
	#endif
	
	#if 0
	fprintf(stdout, "#### gdb_input_handler = ");
	print_address((CORE_ADDR)gdb_input_handler, gdb_stdout);
	fprintf(stdout, " ####\n");
	fflush(stdout);
	#endif
    }
}

