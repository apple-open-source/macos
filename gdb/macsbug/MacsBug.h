/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                      MacsBug.h                                       |
 |                                                                                      |
 |                          MacsBug Plugins Private Interfaces                          |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2006                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*
 
 Guess what this file does! :-)
*/

#ifndef __MACSBUG_H__
#define __MACSBUG_H__

#include "gdb.h"

/*--------------------------------------------------------------------------------------*/

/*-------------------*
 | MacsBug_plugins.c |
 *-------------------*/

extern Gdb_Cmd_Class macsbug_class;		/* help class for MacsBug commands	*/
extern Gdb_Cmd_Class macsbug_internal_class;	/* help class for internal commands	*/
extern Gdb_Cmd_Class macsbug_screen_class;	/* help class for screen UI commands	*/
extern Gdb_Cmd_Class macsbug_testing_class;	/* help class for testing commands	*/
extern Gdb_Cmd_Class macsbug_useful_class;	/* help class for useful commands	*/

#define MACSBUG_COMMAND(command, help) \
    gdb_define_cmd(#command, command, macsbug_class, help)
    
#define MACSBUG_INTERNAL_COMMAND(command, help) \
    gdb_define_cmd(#command, command, macsbug_internal_class, help)

#define MACSBUG_SCREEN_COMMAND(command, help) \
    gdb_define_cmd(#command, command, macsbug_screen_class, help)

#define MACSBUG_TESTING_COMMAND(command, help) \
    gdb_define_cmd(#command, command, macsbug_testing_class, help)

#define MACSBUG_USEFUL_COMMAND(command, help) \
    gdb_define_cmd(#command, command, macsbug_useful_class, help)

#define COMMAND_ALIAS(command, alias) gdb_define_cmd_alias(#command, #alias)

#define CHANGE_TO_MACSBUG_COMMAND(command) gdb_change_class(#command, macsbug_class)

#define safe_strcpy(dst, src) (src ? strcpy(dst, src) : NULL)

/*--------------------------------------------------------------------------------------*/

/*-----------------*
 | MacsBug_utils.c |
 *-----------------*/

#define DEFAULT_HEXDUMP_WIDTH	16		/* default hexdump bytes per line	*/
#define DEFAULT_HEXDUMP_GROUP	4		/* default hexdump bytes per group	*/

extern void init_macsbug_utils(void);
extern char *format_disasm_line(FILE *f, char *src, void *data);
extern char *filter_char(int c, int isString, char *buffer);
extern void __asciidump(char *arg, int from_tty);
extern void __binary(char *arg, int from_tty);
extern void __disasm(char *arg, int from_tty);
extern void __hexdump(char *arg, int from_tty);
extern void __is_running(char *arg, int from_tty);
extern void __print_1(char *arg, int from_tty);
extern void __print_2(char *arg, int from_tty);
extern void __print_4(char *arg, int from_tty);
extern void __reset_current_function(char *arg, int from_tty);
extern void __window_size(char *arg, int from_tty);

extern char *default_help;			/* use for default help on commands	*/

typedef struct {				/* format_disasm_line() data layout:	*/
     GDB_ADDRESS    addr;			/*   current addr being disassembled	*/
     GDB_ADDRESS    pc;				/*   current $pc value			*/
     short	    max_width;			/*   truncate line to this width (if >0)*/
     short          comm_max;			/*   truncate comments to this width(>0)*/
     GDB_FILE       *stream;			/*   output to this stream		*/
     unsigned short flags;			/*   control flags			*/
 	  #define FLAG_PC   	    0x0001	/*	flag pc line with '*' 		*/
	  #define ALWAYS_SHOW_NAME  0x0002	/*	always show the function name	*/
	  #define NO_NEWLINE	    0x0004	/*	don't append '\n' to line	*/
	  #define WRAP_TO_SIDEBAR   0x0008	/*	wrap lines to right sidebar	*/
	  #define DISASM_PC_AREA    0x0010	/*      displaying pc-area		*/
	  #define BRANCH_TAKEN 	    0x4000	/*	cond br at pc will be taken	*/
	  #define BRANCH_NOT_TAKEN  0x8000	/*	cond br at pc will not be taken	*/
} DisasmData;

extern int branchTaken;				/* !=0 if branch [not] taken in disasm	*/

extern unsigned long update_env_values;		/* environment variable update state	*/

/*--------------------------------------------------------------------------------------*/

/*-------------------*
 | MacsBug_cmdline.c |
 *-------------------*/

extern void init_macsbug_cmdline(void);

/*--------------------------------------------------------------------------------------*/

/*-------------------*
 | MacsBug_display.c |
 *-------------------*/

extern void init_macsbug_display(void);
extern void position_cursor_for_shell_input(void);
extern void refresh(char *arg, int from_tty);
extern void init_sidebar_and_pc_areas(void);
extern void macsbug_on(int resume);
extern void macsbug_off(int suspend);
extern void display_pc_area(void);
extern void restore_current_prompt(void);
extern void force_pc_area_update(void);
extern void fix_pc_area_if_necessary(GDB_ADDRESS address);
extern void rewrite_bottom_line(char *line, int err);
extern void __display_side_bar(char *arg, int from_tty);
extern void get_screen_size(int *max_rows, int *max_cols);
extern void save_stack(int max_rows);
extern void my_prompt_position_function(int continued);
extern void my_raw_input_handler(char *theRawLine);
extern void my_raw_input_prompt_setter(char *prompt);
extern void update_macsbug_prompt(void);
extern void forget_some_history(int n);
extern void define_macsbug_screen_positions(short pc_area_lines, short cmd_area_lines);

#define DEFAULT_PC_LINES     	4		/* default pc area max nbr of lines	*/
#define DEFAULT_CMD_LINES	2		/* default cmd area max nbr of lines	*/
#define DEFAULT_TAB_VALUE    	8		/* default history area tab setting	*/
#define DEFAULT_HISTORY_SIZE 	4000		/* default max number of history lines	*/

#define MIN_SIDEBAR (12+32)	 		/* "SP"/sp/empty/"CurApName"/appname/	*/
    						/* empty/LR/CR/empty/CTR/XER/empty/	*/
						/* R0...R31				*/

#define MIN_PC_LINES		2		/* min nbr of pc area lines allowed	*/
#define MAX_PC_LINES		15		/* max nbr of pc area lines allowed	*/
#define MIN_CMD_LINES		2		/* min nbr of cmd area lines allowed	*/
#define MAX_CMD_LINES		15		/* max nbr of cmd area lines allowed	*/
#define MIN_SCREEN_COLS		80		/* min width of screen			*/
#define MIN_SCREEN_ROWS		MIN_SIDEBAR	/* min length of screen		*/

#define CLEAR_LINE  		"\033[0K"	/* xterm controls that we use		*/
#define GOTO	    		"\033[%d;%dH"
#define COLOR			"\033[%dm"
#define COLOR_BLUE   		"\033[34m"
#define COLOR_RED   		"\033[31m"
#define COLOR_BOLD		"\033[1m"
#define COLOR_OFF   		"\033[0m"
#define SAVE_CURSOR		"\0337"
#define RESTORE_CURSOR		"\0338"
#define ERASE_BELOW		"\033[0J"
#define RESET 	    		"\033c\033\014\33[?7h"
#define CURSOR_UP		"\033[%dA\n"

#define ESC			'\033'

#define COLOR_CHANGE(x) ((x) ? COLOR_RED : COLOR_OFF) /* generates reg color changes	*/

#define Update_PC_and_SideBare_Areas() if (macsbug_screen) {				\
				           __display_side_bar(NULL, 0);			\
					   display_pc_area();				\
			               }

extern int target_arch;				/* target architecture (4/8 for 32/64)	*/
#define DEFAULT_TARGET_ARCH "inferior's"	/* default is to use inferior's arch	*/

extern int macsbug_screen;			/* !=0 ==> MacsBug screen is active	*/

extern GDB_FILE *macsbug_screen_stdout;		/* macsbug screen's stdout		*/
extern GDB_FILE *macsbug_screen_stderr;		/* macsbug screen's stderr		*/

extern int max_rows, max_cols;			/* current screen dimensions		*/

extern int  continued_len;			/* continued cmd line, length so far	*/
extern int  continued_count;			/* number of continued line segments	*/
extern char *continued_line_starts[];		/* array of continued_line lines starts */
extern int  continued_segment_len[];		/* length of each continued line segment*/
extern char continued_line[1024];		/* current continued command line	*/

extern int doing_set_prompt;			/* currently doing our own set prompt	*/
extern int need_CurApName;			/* run issued, need new app name	*/

extern int  scroll_mode;			/* 1 ==> fast scroll, 0 ==> slow	*/

extern FILE *log_stream;			/* log file stream variable		*/
extern char *log_filename;			/* log filename (NULL if file closed)	*/

/* The Special_Refresh_States tell write_to_history_area() whether it needs to call	*/
/* screen_refresh().  Generally it doesn't, leaving it usually to prompts to call	*/
/* it. But for queries and progress displays for commands like FILE it needs to refresh	*/
/* as soon as it has something to display.  These states control that.			*/

typedef enum {					/* special screen_refresh() states:	*/
    NORMAL_REFRESH,				/*   normal refresh (generally prompts)	*/
    QUERY_REFRESH1,				/*   query refresh no message yet	*/
    QUERY_REFRESH2,				/*   query refresh expecting a prompt	*/
    PROGRESS_REFRESH				/*   progress refresh			*/
} Special_Refresh_States;

extern Special_Refresh_States immediate_flush;	/* Special_Refresh_States state switch	*/

/*--------------------------------------------------------------------------------------*/

/*-------------------*
 | MacsBug_patches.c |
 *-------------------*/

extern void run_command(char *arg, int from_tty);
extern void init_macsbug_patches(void);
extern int find_breakpt(GDB_ADDRESS address);	/* -1 means not found			*/

extern int control_level;			/* if, while, etc. nesting level	*/
extern int reading_raw;				/* reading raw data for if, while, etc.	*/

extern Gdb_Plugin gdb_printf_command;		/* gdb's own printf command		*/

extern int macsbug_generation;			/* "time" of most recent run,attach,etc.*/

/*--------------------------------------------------------------------------------------*/

/*---------------*
 | MacsBug_set.c |
 *---------------*/

extern void init_macsbug_set(void);

extern int ditto;				/* ditto dup memory displays		*/
extern int unmangle;				/* unmanged names in disassembly	*/
extern int echo_commands;			/* echo command lines to the history	*/
extern int wrap_lines;				/* wrap history lines			*/
extern int tab_value;				/* history display tab value		*/
extern int pc_area_lines;			/* user controlled pc area max lines	*/
extern int cmd_area_lines;			/* user controlled cmd area max lines	*/
extern int max_history;				/* max nbr of lines of history recorded	*/
extern int show_so_si_src;			/* show source with so/si commands	*/
extern int dx_state;				/* breakpoints enabled state		*/
extern int sidebar_state;			/* display reg sidebar in scroll mode	*/
extern int show_selectors;			/* display objc selectors at pc		*/
extern int comment_insns;			/* comment selected instructions	*/
extern int hexdump_width;			/* hexdump line bytes per line		*/
extern int hexdump_group;			/* hexdump bytes per group		*/
extern int force_arch;				/* target_arch set to this or inferior	*/
extern int mb_testing;				/* set by SET mb_testing for debugging	*/

/*--------------------------------------------------------------------------------------*/

/*------------------*
 | MacsBug_screen.c |
 *------------------*/

typedef enum {					/* area we are writing to...		*/
    HISTORY_AREA,
    PC_AREA,
    SIDE_BAR,
    CMD_AREA,
    NO_AREA
 } Screen_Area;

extern void screen_init(int top, int left, int bottom, int right);
extern void screen_close(void);
extern void screen_refresh(int full_refresh);
extern void screen_fflush(FILE *stream, Screen_Area area);
extern void screen_fputs(char *line, FILE *stream, Screen_Area area);
extern void screen_fprintf(FILE *stream, Screen_Area area, char *format, ...);

#define BUFSIZE 	10000			/* output buffer size			*/

/* REMINDER: ANY WRITES TO THE HISTORY_AREA THAT REQUIRE CLEAR_LINE MUST PLACE THE 	*/
/*           CLEAR_LINE AT THE END OF THE LINE AND NOT AT THE BEGINNING.  THIS IS DUE	*/
/*           TO THE WAY screen_fprintf() and screen_fputs() PROCESS CURSOR GOTO'S FOR	*/
/*           POSITIONING THE CURSOR.							*/

/*--------------------------------------------------------------------------------------*/

#endif
