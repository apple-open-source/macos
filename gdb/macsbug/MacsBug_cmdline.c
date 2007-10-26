/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                  MacsBug_cmdline.c                                   |
 |                                                                                      |
 |                            MacsBug Command Line Processing                           |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2005                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*

 This file contains the command line preprocessing and postprocessing routines.  All
 terminal commands come through here.  It is preprocess_commands() which is the key
 routine.  It handles line continuations, whether a command repeats the way it does
 in MacsBug, and so on.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "MacsBug.h"

/*--------------------------------------------------------------------------------------*/

#define MAXARGS 40				/* max nbr of $__argN's allowed		*/
#define ARGC   "$__argc"			/* name we use for our argc		*/
#define ARG    "$__arg"				/* $__argN (where N is an integer)	*/

static int current_argc = 0;			/* most recent $__argc			*/

/*--------------------------------------------------------------------------------------*/

#if 0
/*----------------------------------------------------------------*
 | colon_filter - filter to handle a 'WH' output to define $colon |
 *----------------------------------------------------------------*
 
 This is the redirected stream output filter function for a "WH $pc" command issued by 
 define_colon().  We intercept the output to define $colon with the address of the
 function containing the PC.
*/

static char *colon_filter(FILE *f, char *line, void *data)
{
    int	 n, n_addr, n_offset;
    char *p1, *p2, address[20], function[1024], offset[8];

    /* A gdb_print_address line is formatted as follows:				*/
    
    /* 0xaaaa <function+dddd ...>							*/
    
    /* where 0xaaaa is the address and dddd is the offset in the specified function. 	*/
    /* There offset is suppressed if +dddd is zero.					*/
   
    if (line) {
        
        /* Extract the address...							*/
        
        n_addr = 0;
        while (*line && !isspace(*line) && *line != ':' && n_addr < 18)
    	    address[n_addr++] = *line++;
        address[n_addr] = '\0';
    
	/* To extract the offset (dddd) from the "function+dddd" information we make no	*/
    	/* assumptions about what characters are in the function name other than the	*/
    	/* sequence ">:". We look for the ">:" first and work backwords towards the '+'	*/
    	/* sign.  Everything before is taken as the function name and everything after	*/
    	/* is the offset.  We only extract the offset here.				*/
    
    	while (*line && isspace(*line))		/* skip white space up to '<'		*/
    	    ++line;
    
    	offset[n_offset = 0] = '\0';
    
    	if (*line && *line == '<') {		/* if '<' extract function and offset...*/
    	    p1 = ++line;			/* ...search for ">" delimiter		*/
	    while (*line && *line != '>')
	    	++line;
	
	    if (*line == '>') {			/* ...if ">" found, work backwards...	*/
	    	p2 = line - 1;			/* ...search left for '+'		*/
	    	while (p2 > p1 && *p2 != '+')
		    --p2;
	    	n = p2 - p1;			/* ...length of offset (if any)		*/
	    	    
	    	if (n > 0) {			/* ...extract offset...			*/
 	            n_offset = line - p2;
 	            if (n_offset > 7) n_offset = 7;
 	            memcpy(offset, p2, n_offset);
 	            offset[n_offset] = '\0';
	    	} else {			/* ...if there wasn't any offset...	*/
 	            strcpy(offset, "+0");	/* ...fake the offset			*/
 	            n_offset = 2;
	    	}
	    	
	    	gdb_set_address("$colon", strtoull(address, NULL, 0) - strtol(offset+1, NULL, 0));
	    }
    	}
    }
    
    return (NULL);
}
#endif

/*--------------------------------------------*
 | define_colon - define the value for $colon |
 *--------------------------------------------*
 
 This is called to set the current value for $colon.  Essentially what we do is a WH $pc
 and use it's output to set $colon.  A "where" $pc prints the $pc address, the function
 and offset in that function for that address.  We use the address-offset to define 
 $colon as the start of the function containing the PC.
 
 NOTE: While this works I'm going to suppress the support of $colon since supporting
       it is too expensive to be doing on every command.
*/

static void define_colon(void)
{
    GDB_FILE *redirect_stdout;
    
    #if 1
    
    GDB_ADDRESS start = (GDB_ADDRESS)-1, pc;
    
    if (gdb_get_register("$pc", &pc)) {
    	start = gdb_get_function_start(pc);
        if (!start)
            start = -1;
    }
    
    gdb_set_address("$colon", start);
    
    #else
    
    /* If running then redirect the "WH" output through colon_filter() so it can	*/
    /* extract the info necessary to set $colon.  That filter will set it too.		*/
    
    if (gdb_target_running()) {
        redirect_stdout = gdb_open_output(stdout, colon_filter, NULL);
        gdb_redirect_output(redirect_stdout);
        gdb_print_address("*$pc", 0);
        gdb_close_output(redirect_stdout);
    } else
    	gdb_set_address("$colon", -1);
    
    #endif
}


/*---------------------------------------------------------------------*
 | bsearch_compar_cmd - bsearch compare routine for the command filter |
 *---------------------------------------------------------------------*/
 
static int bsearch_compar_cmd(const void *s1, const void *s2)
{
    return (strcmp((char *)s1, *(char **)s2));
}


/*----------------------------------------------*
 | change_cmd - change a command's command name |
 *----------------------------------------------*
 
 Change the command on a command line from old_cmd to new_cmd.  The change is done
 directly to the commandLine.  That line was allocated with gdb_malloc() so we're free
 to change it's size with gdb_realloc().  If keep_args is non-zero then the original
 args from the commandLine are appended to the new command.  Otherwise the new command
 line consists of only the new command name.
*/

static void change_cmd(char *commandLine, char *old_cmd, char *new_cmd, int keep_args)
{
    char c, new_cmdLine[1024];
    
    int old_cmd_len = strlen(old_cmd);
    int new_cmd_len = strlen(new_cmd);
    
    if (old_cmd_len == 0) {
    	gdb_realloc(commandLine, new_cmd_len+1);
	strcpy(commandLine, new_cmd);
    } else if (new_cmd_len < old_cmd_len) {
    	strcpy(commandLine, new_cmd);
	if (keep_args)
	    strcpy(commandLine + new_cmd_len, commandLine + old_cmd_len);
	gdb_realloc(commandLine, strlen(commandLine)+1);
    } else if (new_cmd_len > old_cmd_len) {
    	strcpy(new_cmdLine, new_cmd);
	if (keep_args)
	    strcpy(new_cmdLine + new_cmd_len, commandLine + old_cmd_len);
	gdb_realloc(commandLine, strlen(new_cmdLine)+1);
	strcpy(commandLine, new_cmdLine);
    } else { /* (new_cmd_len == old_cmd_len) */
    	c = commandLine[old_cmd_len];
    	strcpy(commandLine, new_cmd);
	if (keep_args)
	    commandLine[old_cmd_len] = c;
    }
}


/*-------------------------------------------------*
 | __arg N - return the value of $__argN in $__arg |
 *-------------------------------------------------*
 
 This allows a rudimentary way of indexing into the $__argN arguments built by
 build_argv().  Thus calling __arg N and using $__arg is the same as using $__argN,
 where N is 0 to $__argc-1.
 
 If N >= $__argc, $__arg is set to "", i.e., a null string.
*/

static void __arg(char *arg, int from_tty)
{
    int i;
    
    if (!arg && *arg)
	gdb_error("Invalid __arg index");
     
    i = gdb_get_int(arg);
    
    if (i >= current_argc)
	gdb_eval(ARG "=\"\"" );				/* __arg=""			*/
    else
    	gdb_eval(ARG "=" ARG "%ld", i);			/* __arg=$__argN		*/
}

#define ARG_HELP \
"__arg N -- Set $__arg to the value of $__argN. Calling __arg N and using\n" 		\
"$__arg is the same as using $__argN, where N is 0 to $__argc-1. If N >= argc\n"	\
"then $__arg is set to a null string."


/*---------------------------------------------------------------------------------------*
 | build_argv - create "$__argc" and "$__argN"s (N an integer) representing the cmd line |
 *---------------------------------------------------------------------------------------*
 
 This is a more general version of what gdb does when it sets up $argc and $argN's.  Ours
 use similar naming conventions; $__argc and $__argN's.  They differ from gdb's in the
 following ways:
 
 1. Gdb generally limits the mac number of $argN's to 10 ($arg0 to $arg9).  We limit it
    to what we have MAXARGS set to which is larger than 10.
 
 2. Ours are convenience variables.  Gdb treats theirs as macros to be replaced on the
    command line it executes.  While source looks like it has the variables, the actual
    lines that are executed do not.  Theirs is more efficient doing it that way.  But 
    we cannot do that since the hook provided by gdb for command interception comes too
    late, i.e. after the substitutions are done.  Hence no gdb API support for this.
    Note there are subtle implications of using variables as opposed to preexpanding
    the values.  See below for details.

 3. We provide an special command __arg N to allow access to the $__argN.  The __arg
    command sets the convenience variable $__arg representing the value.  Thus calling
    __arg N and using $__arg is the same as using $__argN, where N is 0 to $__argc-1.
    This provides a rudimentary way of indexing into the arguments which gdb doesn't
    provide.
    
 4. We extend the concept of a "word" which defines what makes up an argument.  Gdb
    defines a word as any sequence of non-blanks and singly and doubly quoted strings
    can contain blanks.  We define an argument as that PLUS blanks are accepted between
    matching parentheses and square brackets.  Thus (char *)a[i + 1] is one valid
    argument (in gdb it would be 4).  We also accept octal and hex escapes in strings.
    All of this is done for by gdb_setup_argv().
    
    Point 4 is the primary reason for this implementation.  Doing MacsBug commands the
    can accept expressions is a real pain if we have to go against some of our basic
    coding instincts and squeeze all the spaces out (e.g., particularly in casts which
    gdb usually requires).
 
 5. We evaluate the arguments here as we collect them.  Because of gdb's blind macro
    substitution we don't find out errors from theirs until we actually use them.  That's
    good and bad.  But we must evaluate them now to get gdb to properly type the
    $__argN's so that they can be used in the MacsBug gdb commands.  Basically we do a
    "set $__argN=expr" for each argv expression collected according to point 4.  If expr
    causes errors they will be reported by gdb and the command terminated.  This means
    keywords are not supported with this convention since, in general, they are undefined
    in the target program.  Thus and commands requiring keywords must be written
    as plugins.  Such commands are flagged in our command table so that we don't
    build the argv here.
  
 Using convenience variables instead of letting gdb preexpand the "macros" has subtle
 implications in the way gdb commands use the arguments.  One trick that can be done in
 gdb is to cast an argument into a string, e.g.,
 
    set $x = (char *)"$arg0"
    
 Since gdb preexpands the $argN's before the command line is executed whatever $arg0 in
 this example is will be effectively there.  So if it were a number, it might end up
 looking like (char *)"3".  If $arg0 is a keyword then it would look like
 (char *)"keyword".  Both are strings and thus can be indexed, e.g., $x[0] would yield
 the character '3' in the first case and 'k' in the second.  You thus could tell the
 difference between a number argument and keyword argument.
 
 On the other hand, using convenience variables won't allow this trick to be used.  If
 you write (char *)"$__arg0", you simply get the string "$__arg0"!  There's no good
 solution for this problem using the convenience variable method.  And indeed, they
 will cause an error to be reported and the command not executed.  This is what leads to
 point 5 above requiring that commands that accept keywords must be written as plugins
 and parse the arguments themselves.
  
 Note this routine called by preprocess_commands() to build the argv vector for the
 interactive macsbug commands only.  Obviously we can't get control on each command's
 line; well we could, but as noted in point 2 above, it's too late) so that $__argc
 $__argN's are available for use by those commands (which is again why their convenience
 variables).  Also, because we run the risk that some arguments could be strings we
 don't call this routine unless the target is running.  That's because strings cannot
 be used unless the target is running.
*/

static void build_argv(char *commandLine)
{
    int  i, argc;
    char *argv[MAXARGS], name[30], tmpCmdLine[1025];
    
    if (!commandLine)
    	argc = 0;
    else
    	gdb_setup_argv(safe_strcpy(tmpCmdLine, commandLine), NULL, &argc, argv, MAXARGS-1);
    
    gdb_set_int(ARGC, current_argc = (argc > 0 ? argc - 1 : 0));
        
    for (i = 1; i < argc; ++i)
	gdb_eval(ARG "%ld=%s", i - 1, argv[i]); 	/* could cause an error		*/
}


/*-----------------------------------------------------------------------------------*
 | preprocess_commands - check to see if a command line command is a MacsBug command |
 *-----------------------------------------------------------------------------------*
 
 This is a command line filter proc that checks to each gdb command entered to stdin
 to see if it's one of "our" MacsBug commands.  If it is we do nothing.  Otherwise we
 set $__lastcmd__ to -1.  This way "contiguous" commands like IL, DM, etc. know they
 were broken up because some other gdb command was entered.  We always set $__lastcmd__ 
 to something in the MacsBug commands.  But without this filter we have no other way
 of detecting gdb commands.
*/
 
static void preprocess_commands(char *commandLine, void *data)
{
    int  i, len, lastcmd;
    char *p, cmd[20];
    
    static int firsttime = 1;
    
    typedef struct {
	char 	       *cmd;
	short	       cmdNbr;
	short	       pairedCmdNbr;
	unsigned short flags;
	    #define REPEATABLE 	   0x0001	/* null cmd, repeat prev. with no args	*/	
	    #define NOT_REPEATABLE 0x0002	/* null cmd, don't repeat previous	*/
	    #define REPEATED_ONCE  0x0004	/* cmd has been repeated at least once	*/
	    #define HAD_NO_ARGS	   0x0008	/* first use of cmd had no args		*/
    	    #define GDB_ENHANCED   0x0010	/* enhanced existing gdb cmd		*/
	    #define IS_PLUGIN      0x8000	/* cmd written as a plugin		*/
    	    #define GDB_REP_ENHNCD (REPEATABLE | GDB_ENHANCED)
    } Command_Info;
    
    #define MACSBUG_CMD(cmd, cmdNbr, paired, flags) {#cmd, cmdNbr, paired, flags}
    
    static Command_Info prev_cmd = MACSBUG_CMD(NULL, -1, -1, NOT_REPEATABLE);

    /* If any new commands are defined then this list must be updated.  It's a bsearch	*/
    /* table so the commands must be alphabetical.					*/
    
    static Command_Info macsbug_cmds[] = {
    	MACSBUG_CMD(BRC,   1, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(BRD,   2, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(BRM,  38, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(BRP,  33, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(DB,    3, -1, REPEATABLE    |IS_PLUGIN),
	MACSBUG_CMD(DL,    4, -1, REPEATABLE    |IS_PLUGIN),
	MACSBUG_CMD(DLL,  49, -1, REPEATABLE    |IS_PLUGIN),
	MACSBUG_CMD(DM,    5, -1, REPEATABLE    |IS_PLUGIN),
	MACSBUG_CMD(DMA,  37, -1, REPEATABLE    |IS_PLUGIN),
	MACSBUG_CMD(DP,    6, -1, REPEATABLE    |IS_PLUGIN),
	MACSBUG_CMD(DV,   12, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(DW,    7, -1, REPEATABLE    |IS_PLUGIN),
	MACSBUG_CMD(DX,   39, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(ES,   -1, -1, NOT_REPEATABLE	  ),
	MACSBUG_CMD(FB,   35, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(FILL, 34, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(FIND, 35, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(FL,   35, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(FLL,  35, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(FW,   35, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(G,     8, -1, REPEATABLE	|IS_PLUGIN),
	MACSBUG_CMD(GT,   36, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(ID,    9, -1, REPEATABLE    |IS_PLUGIN),
	MACSBUG_CMD(IDP,   9, -1, REPEATABLE    |IS_PLUGIN),  /* alias */
	MACSBUG_CMD(IL,   11, -1, REPEATABLE    |IS_PLUGIN),
	MACSBUG_CMD(ILP,  11, -1, REPEATABLE   	|IS_PLUGIN),  /* alias */
	MACSBUG_CMD(IP,   13, -1, REPEATABLE    |IS_PLUGIN),  
	MACSBUG_CMD(IPP,  13, -1, REPEATABLE    |IS_PLUGIN),  /* alias */
	MACSBUG_CMD(L,	  44, -1, REPEATABLE    |IS_PLUGIN),  /* special case */
	MACSBUG_CMD(LI,   44, -1, REPEATABLE    |IS_PLUGIN),  /* special case */
	MACSBUG_CMD(LIS,  44, -1, REPEATABLE    |IS_PLUGIN),  /* special case */
	MACSBUG_CMD(LIST, 44, -1, REPEATABLE    |IS_PLUGIN),  /* special case */
	MACSBUG_CMD(MR,   15, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(MSET, 40, -1, REPEATABLE    |IS_PLUGIN),
	MACSBUG_CMD(N,	  45, -1, GDB_REP_ENHNCD|IS_PLUGIN),  /* gdb enhanced */
	MACSBUG_CMD(NEXT, 45, -1, GDB_REP_ENHNCD|IS_PLUGIN),  /* gdb enhanced */
	MACSBUG_CMD(NEXTI,47, -1, GDB_REP_ENHNCD|IS_PLUGIN),  /* gdb enhanced */
	MACSBUG_CMD(PC,   41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R0,   41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R1,   41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R2,   41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R3,   41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R4,   41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R5,   41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R6,   41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R7,   41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R8,   41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R9,   41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R10,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R11,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R12,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R13,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R14,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R15,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R16,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R17,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R18,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R19,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R20,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R21,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R22,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R23,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R24,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R25,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R26,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R27,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R28,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R29,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R30,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(R31,  41, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(S,	  46, -1, GDB_REP_ENHNCD|IS_PLUGIN),  /* gdb enhanced */
	MACSBUG_CMD(SB,   17, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(SC,   18, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(SC6,  18, -1, NOT_REPEATABLE|IS_PLUGIN),  /* alias */
	MACSBUG_CMD(SC7,  18, -1, NOT_REPEATABLE|IS_PLUGIN),  /* alias */
	MACSBUG_CMD(SI,   16, 23, REPEATABLE    |IS_PLUGIN),
	MACSBUG_CMD(SL,   21, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(SLL,  50, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(SM,   22, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(SO,   23, 16, REPEATABLE    |IS_PLUGIN),
	MACSBUG_CMD(SP,   41, 16, REPEATABLE    |IS_PLUGIN),
	MACSBUG_CMD(STEP, 46, -1, GDB_REP_ENHNCD|IS_PLUGIN),  /* gdb enhanced */
	MACSBUG_CMD(STEPI,48, -1, GDB_REP_ENHNCD|IS_PLUGIN),  /* gdb enhanced */
	MACSBUG_CMD(SW,   24, -1, NOT_REPEATABLE|IS_PLUGIN),
	MACSBUG_CMD(T,    25, -1, REPEATABLE    |IS_PLUGIN),
	MACSBUG_CMD(TD,   26, -1, REPEATABLE    |IS_PLUGIN),
	MACSBUG_CMD(TF,   27, -1, REPEATABLE    |IS_PLUGIN),
	MACSBUG_CMD(TV,   28, -1, REPEATABLE    |IS_PLUGIN),
	MACSBUG_CMD(WH,   42, -1, REPEATABLE    |IS_PLUGIN)
    };
    
    /* Next free command number is: 51							*/
    
    /* Note, LIST is in this list so that we may back up over the prompt when the 	*/
    /* MacsBug screen is off just to make the listing contiguous the way we do with	*/
    /* disassemblies and memory dumps when the MacsBug screen is off.			*/
    
    /* Similarly N[EXT], S[TEP], NEXTI and STEPI are processed like LIST to make those	*/
    /* commands display contiguously when the MacsBug screen is off.			*/
    
    Command_Info *b;
    
    if (!commandLine)
    	return;
    
    /* If no command was entered use the previous command if that is allowed...		*/
    
    /* A null command line is treated by gdb as a repeat of the previous command.  If	*/
    /* it's a repeatable MacsBug command (e.g., IL) then the repeated command has no	*/
    /* arguments even if the original had arguments.  If it's an enhanced repeatable	*/
    /* gdb command (.e.g., NEXT), i.e., enhanced to show a contiguous display even when	*/
    /* the MacsBug screen is off, then we only allow the contiguity iff the original	*/
    /* command had no arguments.  This is controlled by $__lastcmd__.			*/
    
    len = strlen(commandLine);
    if (len == 0 && continued_len == 0 && (prev_cmd.flags & REPEATABLE) &&
    	gdb_get_int("$__lastcmd__") == prev_cmd.cmdNbr) {
	if (!(prev_cmd.flags & GDB_ENHANCED))			/* not gdb enhanced...	*/
    	    if (prev_cmd.flags & (REPEATED_ONCE|HAD_NO_ARGS))	/* ...had no args	*/
	    	gdb_set_previous_command(prev_cmd.cmd);		/* ...cmd with no args	*/
	    else {						/* ...had args		*/
    	    	change_cmd(commandLine, "", prev_cmd.cmd, 0);	/* ...cmd with no args	*/
	    	prev_cmd.flags |= REPEATED_ONCE;
	    }
	else if (!(prev_cmd.flags & HAD_NO_ARGS))		/* gdb enhanced...	*/
	    gdb_set_int("$__lastcmd__", -1);			/* ...had args		*/

	/* If this is a repeat of a enhanced gdb command, but the original command had	*/
	/* arguments, then make them think that they are not contiguous...		*/
	
	if ((prev_cmd.flags & GDB_ENHANCED) && !(prev_cmd.flags & HAD_NO_ARGS))
	    gdb_set_int("$__lastcmd__", -1);
	    
	gdb_set_int(ARGC, current_argc = 0);
	return;
    }
    
    /* If line is continued, then it is not yet ready for processing. Accumulate line	*/
    /* lsegments in continued_line[] and their line starts in  continued_line_starts[]. */
    /* This code works in conjunction with my_prompt_position_function() to handle the	*/
    /* prompts for continued lines.							*/
        
    if (len > 0 && commandLine[len - 1] == '\\') {
    	p = strcpy(continued_line + continued_len, commandLine);
    	continued_line_starts[continued_count] = p;
	continued_len += --len;
	continued_segment_len[continued_count++] = len;
	return;
    }
    
    /* Have a non-continued line at this point.  It's either a new whole line or the	*/
    /* last continued line.  If the latter then append it to the line accumulated above	*/
    /* and use the completed line now representing a complete command.			*/
    
    if (continued_len) {
    	strcpy(continued_line + continued_len, commandLine);
    	continued_len = continued_count = 0;
	commandLine = continued_line;
    }
    
    /* Echo comments to the history -- in blue no less !!  Log 'em if needed too...	*/
    
    if (macsbug_screen && (*commandLine == '#' || echo_commands)) {
    	if (len > 0)
    	    gdb_printf(COLOR_BLUE "%s" COLOR_OFF "\n", commandLine);
	else {
	    p = gdb_get_previous_command();
	    if (p) {
    	    	gdb_printf(COLOR_BLUE "%s" COLOR_OFF "\n", p);
	        gdb_free(p);
	    }
	}
    }
    
    if (log_stream)
    	fprintf(log_stream, "%s\n", commandLine);
   
    lastcmd = -1;				/* used to set $__lastcmd__ at end	*/

    /* See if the command is one of our MacsBug commands...				*/
    
    for (p = commandLine; *p && (*p != ' ' && *p != '\t'); p++) ; /* find end of cmd	*/
    
    if (p > commandLine) {			/* find the command			*/
    	len = p - commandLine;
    	if (len < 19) {
    	    for (i = 0; i < len; ++i)
    	    	cmd[i] = toupper(commandLine[i]);
    	    cmd[len] = '\0';
    	    b = bsearch(cmd, macsbug_cmds, sizeof(macsbug_cmds)/sizeof(Command_Info),
	    			sizeof(Command_Info), bsearch_compar_cmd);
    	    
	    /* If the command is one of ours, remember it just in case a null line is	*/
	    /* entered the next time so we can treat "repeatable" commands specially.	*/
	    /* Also remember whether the command has arguments so we can be a little	*/
	    /* "cute" about how we cause gdb's command history to be recorded for that	*/
	    /* null line next time.							*/
	    
	    if (b) {
		if (prev_cmd.cmdNbr == b->cmdNbr && (b->flags & REPEATABLE))
		    lastcmd = b->cmdNbr;	  /* see comments near end of function	*/
	    	prev_cmd = *b;
    	    	while (p && (*p == ' ' || *p == '\t'))
    	    	    ++p;
    	    	if (!*p)
		    prev_cmd.flags |= HAD_NO_ARGS;
		else
		    lastcmd = -1;
		if (gdb_target_running() && !(b->flags & IS_PLUGIN))
		    build_argv(commandLine);	   /* ...set up $__argc and $__argN's	*/
		#if 0
		if (!gdb_target_running()) {
		    if (b->cmdNbr == 8 /*G*/)
			change_cmd(commandLine, "g", "ra", 1);
		    else if (b->cmdNbr == 36 /* GT */) {
			gdb_execute_command("tbreak %s", p);
			change_cmd(commandLine, "g", "ra", 0);
		    }
		} else if (!(b->flags & IS_PLUGIN))/* if not written as a plugin...	*/
		    build_argv(commandLine);	   /* ...set up $__argc and $__argN's	*/
	    	#endif
	    }
    	} else
    	    b = NULL;
    } else
    	b = NULL;
    
    if (!b) {
	prev_cmd.cmdNbr = -1;
	prev_cmd.flags  = NOT_REPEATABLE;
	prev_cmd.flags &= ~IS_PLUGIN;
    }
    
    /* Explicit commands always reset to using a command for the first time *unless* it	*/
    /* is a repeatable command (e.g., IP, i.e., ones that attempt to produce a		*/
    /* contiguous display when the MacsBug screen is off) or an alternate spelling for 	*/
    /* for the same repeatable command (.e.g., L and LIST, N and NEXT).  Such commands 	*/
    /* are flagged as REPEATABLE.  If we have one of these, and the previous command 	*/
    /* was the same command (number) then we set $__lastcmd__ exactly as if a null 	*/
    /* command was entered, i.e., $__lastcmd__ is set to the same command number so 	*/
    /* that the command can take appropriate actions if it handles contiguous output 	*/
    /* (as L and LIST do, or IL when another IL is explicitly retyped instead of just	*/
    /* hitting a return to repeat the previous command).  For non-repeatable command	*/
    /* $__lastcmd__ is set to -1.							*/ 
    
    gdb_set_int("$__lastcmd__", lastcmd);
    
    /* Note that we always record the actual previous command number in $__prevcmd__	*/
    /* and this is only -1 if it is not one of our commands.  Thus this is always 	*/
    /* available to all commands, repeatable or not, if they are interested in what 	*/
    /* command preceded them.  For example, SC uses $__prevcmd__ to decide to insert a	*/
    /* blank line between the output when two SC's are done contiguously to the Macsbug	*/
    /* screen so that the two output lists can be distinguished more easily.		*/
    
    /* Define the current value for $colon (addr of function containing pc)...		*/
    
    define_colon();
}


/*----------------------------------------------*
 | postprocess_commands - postprocess a command |
 *----------------------------------------------*
 
 This is called after a command from stdin has been executed.  Here we copy basically do,
 
   set $__prevcmd__=$__lastcmd__
 
 i.e., we unconditionally remember whatever the command number of the command we just
 executed was.  If it is one of "ours" then it has meaning.  If it isn't then it will
 be -1.
 
 This is done independently of what preprocess_commands() is doing to $__lastcmd__ since
 $__lastcmd__ is used only for repeatable commands, i.e., ones that are repeated without
 arguments when a null command line is entered.  $__prevcmd__ is always available for
 ANY command that is interested in what was last executed.
*/

static void postprocess_commands(void *data)
{
    gdb_set_int("$__prevcmd__", gdb_get_int("$__lastcmd__"));
}

/*--------------------------------------------------------------------------------------*/

/*------------------------------------------------------------*
 | gdb_redirect_stdin - initialize the comand line processing |
 *------------------------------------------------------------*/

void init_macsbug_cmdline(void)
{
    MACSBUG_INTERNAL_COMMAND(__arg, ARG_HELP);
    
    /* These command class changes are here because the macsbug_cmds[] is in this file.	*/
    /* Each command in that isn't defined as a plugin is defined with a gdb DEFINE and 	*/
    /* needs to be converted from a user-defined class which DEFINE creates to our 	*/
    /* MacsBug class so that these commands "join" with the others to make up the 	*/
    /* complete set of commands for the MacsBug help class.				*/
    
    CHANGE_TO_MACSBUG_COMMAND(es);
    
    gdb_redirect_stdin(preprocess_commands, postprocess_commands, NULL);
}
