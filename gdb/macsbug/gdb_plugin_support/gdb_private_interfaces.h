/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                               gdb_private_interfaces.h                               |
 |                                                                                      |
 |                          MacsBug Plugins Private Interfaces                          |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2006                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*/

#ifndef __GDB_PRIVATE_INTERFACES_H__
#define __GDB_PRIVATE_INTERFACES_H__

#include "gdb.h"
#include "defs.h"

#include "dis-asm.h"

/*--------------------------------------------------------------------------------------*/

/* The development of this API spans versions of GDB 4.x and beyond.  There are, of  	*/
/* course some differences between GDB 4.x and what followed which affect our code.	*/
/* We must make sure we don't do anything that is incompatible with the gdb we are	*/
/* tied to.  So the following macro is defined for that purpose.			*/

/* The GDB_MULTI_ARCH_PARTIAL is not defined in GDB 6.x in defs.h so it's as good as	*/
/* any other such define to distinguish 6.x and beyond.  All the API files use this	*/
/* header and all of them also require defs.h included.  So we convert the sort of 	*/
/* "random" define grepped from defs.h into something more appropriate.  It's a shame	*/
/* gdb doesn't have some version macro in some header to make this cleaner.		*/

#ifdef GDB_MULTI_ARCH_PARTIAL
#define GDB_VERSION 5
#else
#define GDB_VERSION 6
#endif

#define GDB4 0				/* kept for historical reasons for gdb 4.x	*/
					/* we no longer support that older version	*/

/*--------------------------------------------------------------------------------------*/
/*-------*
 | gdb.c |
 *-------*/

/* Certain hooks and values are saved with the assumption that they are the initial	*/
/* values defined by gdb itself.  We then change these hooks and values for the plugin	*/
/* library support as required.  If the plugin library is used by independent plugins	*/
/* then after the first instance of the plugin is loaded there is the chance that the	*/
/* values we pick up thinking they are gdb's are actually values defined by the earlier	*/
/* loaded plugin(s).  									*/

/* In order to get independent plugin library instances to use the original gdb settings*/
/* the first instance needs to squirrel away gdb's values in a safe place so that 	*/
/* following instances know they are following and can pick up gdb's value from that 	*/
/* same safe place.									*/

/* The only "safe" place is in gdb somewhere since it remains loaded across instances of*/
/* our library.  And the only "safe" places in gdb is some unused piece of data (or 	*/
/* used but not for our environment).  On doing some searching through the gdb sources	*/
/* the following three candidates were found:						*/

extern int ui_file_magic;
//extern int epoch_interface; /* in top.h */
//extern char *rl_library_version;

/* Only the address if ui_file_magic is used by gdb, not what's in it.  It's not static	*/
/* so we can get at it.  But there's no reason it shouldn't be static.  If someone ever */
/* figures this out we could get screwed.  So the other two candidates are available.	*/

/* The "epoch_interface" is set by an undocumented command line option and only used by */
/* the gdb INSPECT command.  I think we can sacrifice that since it basically PRINT.	*/

/* The "rl_library_version" appears to be a constant string but otherwise unused.	*/

/* So for now we're using "ui_file_magic" until we get screwed then switch to one of the*/
/* others.  We set ui_file_magic on the first instance and check it on all additional 	*/
/* instances.  Being a global it should be 0 that first time.				*/

/* The following typedef struct defines all the key gdb data that we want to guarantee 	*/
/* are the values preset by gdb and not our library:					*/

typedef struct {				/* initialized by or initial value...	*/
    void (*input_handler)(char *);		/* event_top.c: gdb_setup_readline()	*/
    struct ui_file *gdb_stdout;			/* event_top.c: gdb_setup_readline()	*/
    struct ui_file *gdb_stderr;			/* event_top.c: gdb_setup_readline()	*/
    struct ui_out *uiout;			/* cli_out.c: _initialize_cli_out()	*/
    void (*rl_completion_display_matches_hook)(char **, int, int); /* NULL		*/
    //int query_hook(char *format, va_list ap);	/* NULL					*/
    void (*rl_startup_hook)(void);		/* NULL					*/
    char *(*command_line_input_hook)(char *, int, char *); /* NULL 			*/
    void (*help_command)(char *, int);		/* gdb command definition		*/
    void (*deprecated_set_hook)(struct cmd_list_element *);/* NULL			*/
    
    char input_handler_defined;			/* these tell us when above are defined	*/
    char gdb_stdout_defined;
    char gdb_stderr_defined;
    char insn_stream_defined;
    char insn_printf_defined;
    char uiout_defined;
    char rl_completion_display_matches_hook_defined;
    //char query_hook_defined;
    char rl_startup_hook_defined;
    char command_line_input_hook_defined;
    char help_command_defined;
    char deprecated_set_hook_defined;
    
    /* The following fields are carried in this global struct to support multiple	*/
    /* instances of the plugin library to communicate with one another.			*/
    
    void *plugin_global_data;			/* global data for all plugin instances	*/
    struct Plugin_Pvt_Data *pvt_data_list;	/* list of private channels		*/
} Gdb_Global_Data;

/* As discussed above we are using ui_file_magic as our "safe" place in gdb and setting	*/
/* it as a pointer to the above struct.  In our code we use a more appropriate name for	*/
/* the pointer:										*/

#define gdb_global_data_p ((Gdb_Global_Data *)ui_file_magic)

/* We still allow setting local statics from the gdb values, but we do it with the 	*/
/* following macro.  It guarantees that we set the value on the first instance and then	*/
/* use that first set value for additional instances:					*/

#define INITIAL_GDB_VALUE(x, y) \
    (gdb_global_data_p->x##_defined) ? gdb_global_data_p->x : (gdb_global_data_p->x##_defined = 1, \
    				              	     	       gdb_global_data_p->x = (y))
//#define INITIAL_GDB_VALUE(x, y) (y)

/* Because the Gdb_Global_Data struct is common to all plugin library instances it	*/
/* provides a convenient place to hang some data on to to allow multiple instances to	*/
/* talk to one another.  Gdb_Global_Data contains plugin_global_data for a global 	*/
/* communication channel.  It also has a pointer to a list of individual channels, 	*/
/* pvt_data_list, who's list elements are defined below.				*/

typedef struct Plugin_Pvt_Data {
    struct Plugin_Pvt_Data *next;		/* next plugin instance			*/
    void 		   *plugin_data;	/* plugin's private data		*/
    char   		   plugin_name[1];	/* start of plugin's name		*/
} Plugin_Pvt_Data;

/* Addresses computed in a data type larger than the target architecture's address	*/
/* (e.g., 64-bit computation on a 32-bit machine) need to be corrected for the target.  */
/* The FIX_TARGET_ADDR macro checks this and zeroes out the high-order bits if 		*/
/* necessary (e.g., high order 32-bits of a 64-bit value if we're on a 32-bit target).	*/
/* Use this macro in all places where an GDB_ADDRESS is supplied as an argument to	*/
/* gdb.h functions when there is doubt where the specified address is coming from.	*/

#define FIX_TARGET_ADDR(addr) if (TARGET_ADDR_BIT < (sizeof(CORE_ADDR) * HOST_CHAR_BIT)) \
    				  addr &= ((CORE_ADDR)1 << TARGET_ADDR_BIT) - 1

extern char *tilde_expand(char *pathname);	/* defined in readline.c		*/


/*----------------------*
 | gdb_io_redirection.c |
 *----------------------*/

extern void __initialize_io(void);			 /* initialize I/O redirection	*/
extern int (*__default_gdb_query_hook)(char *, va_list); /* set for special events	*/


/*----------------*
 | gdb_complete.c |
 *----------------*/

extern void __cmd_completion_display_hook(char **matches, int num_matches, int max_length);

extern void (*__word__completion_hook)(int save_input_cursor); /* hooks for the above	*/
extern void (*__word__completion_query_hook)(GDB_FILE *stream, char *format, ...);
extern int  (*__word__completion_read_hook)(void);


/*-----------*
 | gdb_set.c |
 *-----------*/

extern void __initialize_set(void);			/* initialize set		*/
extern void __my_set_hook_guts(struct cmd_list_element *, Gdb_Set_Type *, void **);


/*----------------------*
 | gdb_special_events.c |
 *----------------------*/

/* none...yet */


/*---------------------------------------------------*
 | Possibly useful gdb stuff kept here as a reminder |
 *---------------------------------------------------*/

extern int screenwidth, screenheight;


/*----------------------------------------------------------------------*
 | readline hooks and data - defined here because header is unavaliable |
 *----------------------------------------------------------------------*/

extern int rl_read_key(void);			/* read 1 char from terminal and return	*/

extern void (*rl_startup_hook)();		/* called just before prompt 		*/
extern void (*rl_completion_display_matches_hook)();/* completion display matches hook	*/
extern void (*rl_getc_function)();		/* raw terminal char input  		*/

extern void (*rl_redisplay_function)();	    	/* called to echo readline chrs & prompt*/
extern char *rl_display_prompt;			/* readline display prompt if not NULL	*/
extern char *rl_prompt;				/* primary readline prompt		*/ 

extern FILE *rl_instream;			/* the input stream			*/
extern FILE *rl_outstream;			/* the output stream			*/


/*-------------------------------------------------------------------*
 | Hooks left here as reminders - maybe they could be useful someday |
 *-------------------------------------------------------------------*/

extern void _rl_abort_internal();
extern int _rl_qsort_string_compare();
extern char *tilde_expand();

extern int _rl_print_completions_horizontally;	/* If !0, completions are printed	*/
						/* horizontally in alphabetical order, 	*/
						/* like `ls -x'. 			*/

extern int rl_visible_stats;			/* Non-zero means add an additional 	*/
						/* character to each filename displayed */
						/* during listing completion iff 	*/
						/* rl_filename_completion_desired which */
						/* helps to indicate the type of file 	*/
						/* being listed. 			*/

extern int rl_completion_query_items;		/* Up to this many items will be 	*/
						/* displayed in response to a possible-	*/
						/* completions call. After that, we ask */
						/* the user if s/he is sure she wants 	*/
						/* to see them all. 			*/

extern int rl_ignore_completion_duplicates;	/* If non-zero, then disallow duplicates*/
						/* in the matches. 			*/

extern int rl_filename_completion_desired;	/* Non-zero means that the results of 	*/
						/* the matches are to be treated as 	*/
						/* filenames.  This is ALWAYS zero on	*/
						/* entry, and can only be changed within*/
						/* a completion entry finder function.	*/

#endif
