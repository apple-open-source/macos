/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                  MacsBug_plugins.c                                   |
 |                                                                                      |
 |                                MacsBug Plugins Command                               |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2002                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*

 This file contains MacsBug command plugins that are too hard to write, too inefficient,
 or cannot be genralized enough if written in gdb command language.
 
 Also in this file is oid init_from_gdb() which is the main entry point to the MacsBug
 plugins, i.e., it is that routine that, by name convention, is called from gdb when
 a load-plugin command is done on the linked collection of MacsBug modules.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

#include "MacsBug.h"

/*--------------------------------------------------------------------------------------*/

Gdb_Cmd_Class macsbug_class;			/* help class for MacsBug commands	*/
Gdb_Cmd_Class macsbug_internal_class;		/* help class for internal commands	*/
Gdb_Cmd_Class macsbug_screen_class;		/* help class for screen UI commands	*/
Gdb_Cmd_Class macsbug_testing_class;		/* help class for testing commands	*/
Gdb_Cmd_Class macsbug_useful_class;		/* help class for useful commands	*/

static int  find_size = 0;			/* FB/FW/FL parameter to FIND		*/
static char *findName;				/* "FB", "FW", or "FL"			*/

static Gdb_Plugin gdb_continue_command = NULL;

static void ra(char *arg, int from_tty);	/* needed by G and GT when not running	*/

/*--------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------*
 | BRC [addr] - clear all breakpoints or one breakpoint at addr |
 *--------------------------------------------------------------*/

static void brc(char *arg, int from_tty)
{
    if (arg && *arg)
	gdb_execute_command("clear %s", arg);
    else
    	gdb_execute_command("delete");
    
    gdb_set_int("$__lastcmd__", 1);
}

#define BRC_HELP \
"BRC [addr] -- Clear all breakpoints or one breakpoint at addr.\n"			\
"See also gdb's DELETE command (clears breakpoints by number), and DISABLE."


/*--------------------------------------------------*
 | BRD [n] - show all breakpoints (or breakpoint n) |
 *--------------------------------------------------*/

static void brd(char *arg, int from_tty)
{
    if (arg && *arg)
	gdb_execute_command("info breakpoints %s", arg);
    else
    	gdb_execute_command("info breakpoints");
    
    gdb_set_int("$__lastcmd__", 2);
}

#define BRD_HELP "BRD -- Show all breakpoints (or breakpoint n)."


/*------------------------------------------*
 | BRM regex - set breakpoints (gdb RBREAK) |
 *------------------------------------------*/

static void brm(char *arg, int from_tty)
{
    if (!arg || !*arg)
    	gdb_error("usage: BRM regex (regex expected)");
    
    gdb_execute_command("rbreak %s", arg);	/* rbreak regex				*/
    
    gdb_set_int("$__lastcmd__", 38);
}

#define BRM_HELP \
"BRM regex -- Set breakpoints at all functions matching regular expression.\n"		\
"This sets unconditional breakpoints on all matches, printing a list of all\n"		\
"breakpoints set.  This command is equivalent to a gdb RBREAK command."


/*-----------------------------------------------------------------------------------*
 | BRP [gdb-spec [expr]] - Break at gdb-spec (under specified condition) (gdb BREAK) |
 *-----------------------------------------------------------------------------------*/

static void brp(char *arg, int from_tty)
{
    if (!arg || !*arg)
    	gdb_error("usage: BRP [gdb-spec [expr]]");
    
    gdb_execute_command("br %s", arg);		/* br [gdb-spec [expr]]			*/
    
    gdb_set_int("$__lastcmd__", 33);
}

#define BRP_HELP \
"BRP [gdb-spec [expr]] -- Break at gdb-spec (under specified condition).\n"		\
"This implements the gdb \"BREAK gdb-spec [if condition]\".  See gdb BREAK\n"		\
"documentation for further details.\n"							\
"\n"											\
"Macsbug features not supported: Interation count.\n"					\
"                                No semicolon before the command.\n"			\
"                                Command is not enclosed in quotes.\n"			\
"                                Only a single command is allowed."


/*-------------------------------------------------------------------*
 | back_up_over_prompt - position cursor over the current gdb prompt |
 *-------------------------------------------------------------------*
 
 If we are NOT using the MacsBug screen and we are processing a MacsBug command which
 is intended to have contiguous output then this routine back up the cursor over the
 prompt.  Normally this would be 2 lines (one for the prompt + one for the return to
 enter the command).
 
 An example of this is multiple IL's.  We want to display the disassembly without showing
 the intervening gdb prompts.  So this routine will back the cursor up so that the
 disassembly will overwrite the prompt.
 
 For commands that do disassemblies there is the possibility of them also displaying
 the "branch [not] taken" message at the end.  So for those we want to back up 3 lines.
 Thus the switch, branchTaken, is passed here for such commands to tell us whether to
 back up 2 lines (branchTaken == 0) or 3 (branchTaken != 0).
 
 Note that not of this is ever a problem when the MacsBug screen is up since the prompts
 never show in the history display area and always appear in their separate command area.
 
 Also, since this routine is called at the beginning of each command that wants a
 contiguous display when it decides it needs to do it, the starting address for the
 display is also passed.  We use that to access *(char*)addr just in case the starting
 address is not accessible.  That will cause an appropriate error message with the
 command aborted BEFORE we get a chance to back up the cursor.
*/

static void back_up_over_prompt(unsigned long addr, int branchTaken, int from_tty)
{
    if (!macsbug_screen && isatty(STDOUT_FILENO)) {	/* skip if macsbug screen is up	*/
	gdb_eval("$__accessible__=*(char*)0x%lX", addr);
	if (from_tty)
	    gdb_printf(CURSOR_UP CLEAR_LINE, 2 + (branchTaken != 0));
    }
}


/*---------------------------------------------------------*
 | db_dw_and_dl [addr] - common routine for DB, DW, and DL |
 *---------------------------------------------------------*
 
 The arg is the DB, DW, and DL arg.  The size is 1 (DB), 2(DW), or 4 (DL).  The cmdNbr
 is the command's $__lastcmd__ number.
*/

static void db_dw_and_dl(char *arg, int from_tty, int size, int cmdNbr)
{
    unsigned char  b;
    unsigned short w;
    unsigned long  addr, l;
    char 	   line[20], addrStr[20];
    
    __is_running("The program is not running.", 0);
    
    if (arg && *arg) {
    	addr = gdb_get_int(arg);
	gdb_set_int("$dot", addr);
    } else if (gdb_get_int("$__lastcmd__") == cmdNbr) {
    	addr = gdb_get_int("$dot") + size;
	back_up_over_prompt(addr, 0, from_tty);
 	gdb_set_int("$dot", addr);
   } else
    	addr = gdb_get_int("$dot");
    
    sprintf(addrStr, "0x%lX", addr);
    
    switch (size) {
    	case 1:
	    gdb_read_memory(&b, addrStr, 1);
	    gdb_printf("Byte at %.8lX = 0x%.2X   %u    %d    ", addr, b, b, b);
	    sprintf(line, "0x%X", b);
	    __print_1(line, from_tty);
	    gdb_puts("\n");
	    break;
	
	case 2:
	    gdb_read_memory(&w, addrStr, 2);
	    gdb_printf("Two bytes at %.8lX = 0x%.4X   %u    %d    ", addr, w, w, w);
	    sprintf(line, "0x%X", w);
	    __print_2(line, from_tty);
	    gdb_puts("\n");
	    break;
	
	case 4:
	    gdb_read_memory(&l, addrStr, 4);
	    gdb_printf("Four bytes at %.8X = 0x%.8lX   %u    %d    ", addr, l, l, l);
	    sprintf(line, "0x%lX", l);
	    __print_4(line, from_tty);
	    gdb_puts("\n");
	    break;
    }
    
    gdb_set_int("$__lastcmd__", cmdNbr);
}


/*-------------------------------------------------------*
 | DB addr -- Display in hex the byte at addr (or $dot)  |
 *-------------------------------------------------------*/

static void db(char *arg, int from_tty)
{
    db_dw_and_dl(arg, from_tty, 1, 3);
}

#define DB_HELP "DB addr -- Display in hex the byte at addr (or $dot)."


/*-----------------------------------------------------------*
 | DL [addr] -- Display in hex the 4 bytes at addr (or $dot) |
 *-----------------------------------------------------------*/

static void dl(char *arg, int from_tty)
{
    db_dw_and_dl(arg, from_tty, 4, 4);
}

#define DL_HELP "DL [addr] -- Display in hex the 4 bytes at addr (or $dot)."


/*-------------------------------------------------------------------------------------*
 | DM [addr [n | basicType]] - display memory from addr for n bytes or as a basic type |
 *-------------------------------------------------------------------------------------*/

static void dm(char *arg, int from_tty)
{
    int  	   argc, n, i;
    unsigned char  b;
    unsigned short w;
    unsigned long  l, addr;
    char 	   *basicType, *argv[5], addrStr[20], buf[20], line[1024], tmpCmdLine[1024];
    
    struct {
        short top;
	short left;
	short bottom;
	short right;
    } rect;
    
    static char *basic_types[] = {
    	"Byte", "Word", "Long",
	"SignedByte", "SignedWord", "SignedLong",
	"UnsignedByte", "UnsignedWord", "UnsignedLong",
	"PString", "CString",
	"Boolean", 
	"Binary8", "Binary16", "Binary32",
	"OSType", "Rect", 
	"Text", "Pointer", "Handle",
	"IORefNum", "VRefNum", "Seconds", "ATrapWord",
	"AbsTicks", "TickInterval", "RgnHandle", "IOTrapWord",
	"Version", "RGBColor", "Fixed", "ShortFixed", "UnsignedFixed",
	"Fract", "Region",
	NULL
    };
    
    __is_running("The program is not running.", 0);
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "DM", &argc, argv, 4);
    
    /* Handle the default case...							*/
    
    if (argc == 1) {
    	if (gdb_get_int("$__lastcmd__") == 5) {
	    gdb_eval("$dot=(unsigned char *)$dot+$__prev_dm_n__");
	    back_up_over_prompt(gdb_get_int("$dot"), 0, from_tty);
	} else {
	    gdb_printf("Displaying memory from %.8lX\n", gdb_get_int("$dot"));
	    gdb_set_int("$__prev_dm_n__", hexdump_width);
	}
	
	__hexdump("$dot $__prev_dm_n__", from_tty);
	
	gdb_set_int("$__lastcmd__", 5);
	return;
    }
    
    /* Handle the DM addr case...							*/
    
    addr = gdb_get_int(argv[1]);
   
    if (argc == 2) {
	gdb_set_int("$dot", addr);		/* $dot=addr				*/
	gdb_set_int("$__prev_dm_n__", hexdump_width);
	
	gdb_printf("Displaying memory from %.8lX\n", addr);
	__hexdump("$dot", from_tty);
	
	gdb_set_int("$__lastcmd__", 5);
	return;
    }
    
    /* Handle the DM addr n|basicType case...						*/
    
    if (argc > 3)
    	gdb_error("usage: DM [addr [n | basicType]] (wrong number of arguments)");
    
    if (isdigit(argv[2][0]) || argv[2][0] == '(' || argv[2][0] =='\'') { /* DM addr n	*/
    	n = gdb_get_int(argv[2]);
	gdb_set_int("$dot", addr);
	gdb_set_int("$__prev_dm_n__", n);
	
    	gdb_printf("Displaying memory from %.8lX\n", addr);
	__hexdump("$dot $__prev_dm_n__", from_tty);
	
	gdb_set_int("$__lastcmd__", 5);
	return;
    }
     
    /* From here on we want to display the addr as a function of a basicType keyword	*/
    
    gdb_set_int("$__lastcmd__", -5);
    sprintf(addrStr, "0x%lX", addr);
    basicType = (argv[2][0] != '"') ? argv[2] : gdb_get_string(argv[2], line, 30);
    
    switch (n = gdb_keyword(basicType, basic_types)) {
    	case  0: /* Byte */
	    gdb_read_memory(&b, addrStr, 1);
	    gdb_printf("Displaying Byte\n"
	               " %.8lX: %.2X\n", addr, (unsigned char)b);
    	    break;
	    
    	case  1: /* Word */
	    gdb_read_memory(&w, addrStr, 2);
	    gdb_printf("Displaying Word\n"
	               " %.8lX: %.4X\n", addr, (unsigned short)w);
    	    break;
	    
    	case  2: /* Long */
	    gdb_read_memory(&l, addrStr, 4);
	    gdb_printf("Displaying Long\n"
	               " %.8lX: %.8lX\n", addr, (unsigned long)l);
    	    break;
	    
    	case  3: /* SignedByte */
 	    gdb_read_memory(&b, addrStr, 1);
	    gdb_printf("Displaying SignedByte\n"
	               " %.8lX: %d\n", addr, (char)b);
   	    break;
	    
    	case  4: /* SignedWord */
 	    gdb_read_memory(&w, addrStr, 2);
	    gdb_printf("Displaying SignedWord\n"
	               " %.8lX: %d\n", addr, (short)w);
    	    break;
	    
    	case  5: /* SignedLong */
 	    gdb_read_memory(&l, addrStr, 4);
	    gdb_printf("Displaying SignedLong\n"
	               " %.8lX: %ld\n", addr, (long)l);
    	    break;
	    
    	case  6: /* UnsignedByte */
	    gdb_read_memory(&b, addrStr, 1);
	    gdb_printf("Displaying UnsignedByte\n"
	               " %.8lX: %u\n", addr, (unsigned char)b);
    	    break;
	    
    	case  7: /* UnsignedWord */
	    gdb_read_memory(&w, addrStr, 2);
	    gdb_printf("Displaying UnsignedWord\n"
	               " %.8lX: %u\n", addr, (unsigned short)w);
     	    break;
	    
   	case  8: /* UnsignedLong */
	    gdb_read_memory(&l, addrStr, 4);
	    gdb_printf("Displaying UnsignedLong\n"
	               " %.8lX: %lu\n", addr, (unsigned long)l);
    	    break;
	    
    	case  9: /* PString */
	    gdb_read_memory(&b, addrStr, 1);
	    gdb_read_memory(line, addrStr, b + 1);
	    gdb_printf("Displaying PString\n"
	               " %.8lX: (%d) \"", addr, b);
	    for (i = 1; i <= b; ++i)
	    	gdb_printf("%s", filter_char(line[i], 1, buf));
	    gdb_puts("\"\n");
     	    break;
	    
   	case 10: /* CString */
	    gdb_printf("Displaying CString\n"
	               " %.8lX: \"", addr);
            gdb_read_memory(&b, addrStr, 1);
	    while (b) {
	    	gdb_printf("%s", filter_char(b, 1, buf));
    		sprintf(addrStr, "0x%lX", ++addr);
		gdb_read_memory(&b, addrStr, 1);
	    }
	    gdb_puts("\"\n");
     	    break;
	    
   	case 11: /* Boolean */
	    gdb_read_memory(&b, addrStr, 1);
	    gdb_printf("Displaying Boolean\n"
	               " %.8lX: %s\n", addr, b ? "true" : "false");
    	    break;
	    
    	case 12: /* Binary8 */
	    gdb_read_memory(&b, addrStr, 1);
	    gdb_printf("Displaying Binary8\n"
	               " %.8lX: %.2X = ", addr, (unsigned char)b);
	    sprintf(line, "0x%X 4", (b >> 4) & 0x0F);
	    __binary(line, from_tty);
	    gdb_puts(" ");
	    sprintf(line, "0x%X 4", b & 0x0F);
	    __binary(line, from_tty);
	    gdb_puts("\n");
    	    break;
	    
    	case 13: /* Binary16 */
	    gdb_read_memory(&w, addrStr, 2);
	    gdb_printf("Displaying Binary16\n"
	               " %.8lX: %.4X = ", addr, (unsigned short)w);
	    sprintf(line, "0x%X 4", (w >> 12) & 0x0F);
	    __binary(line, from_tty);
	    gdb_puts(" ");
	    sprintf(line, "0x%X 4", (w >> 8) & 0x0F);
	    __binary(line, from_tty);
	    gdb_puts(" ");
	    sprintf(line, "0x%X 4", (w >> 4) & 0x0F);
	    __binary(line, from_tty);
	    gdb_puts(" ");
	    sprintf(line, "0x%X 4", w & 0x0F);
	    __binary(line, from_tty);
	    gdb_puts("\n");
    	    break;
	    
    	case 14: /* Binary32 */
	    gdb_read_memory(&l, addrStr, 4);
	    gdb_printf("Displaying Binary32\n"
	               " %.8lX: %.8lX = ", addr, (unsigned long)l);
	    sprintf(line, "0x%X 4", (l >> 28) & 0x0F);
	    __binary(line, from_tty);
	    gdb_puts(" ");
	    sprintf(line, "0x%X 4", (l >> 24) & 0x0F);
	    __binary(line, from_tty);
	    gdb_puts(" ");
	    sprintf(line, "0x%X 4", (l >> 20) & 0x0F);
	    __binary(line, from_tty);
	    gdb_puts(" ");
	    sprintf(line, "0x%X 4", (l >> 16) & 0x0F);
	    __binary(line, from_tty);
	    gdb_puts(" ");
	    sprintf(line, "0x%X 4", (l >> 12) & 0x0F);
	    __binary(line, from_tty);
	    gdb_puts(" ");
	    sprintf(line, "0x%X 4", (l >> 8) & 0x0F);
	    __binary(line, from_tty);
	    gdb_puts(" ");
	    sprintf(line, "0x%X 4", (l >> 4) & 0x0F);
	    __binary(line, from_tty);
	    gdb_puts(" ");
	    sprintf(line, "0x%X 4", l & 0x0F);
	    __binary(line, from_tty);
	    gdb_puts("\n");
    	    break;
	    
    	case 15: /* OSType */
	    gdb_read_memory(&l, addrStr, 4);
	    gdb_printf("Displaying OSType\n"
	               " %.8lX: %.8X = ", addr, l);
	    sprintf(line, "0x%.lX", l);
	    __print_4(line, from_tty);
	    gdb_puts("\n");
    	    break;
	    
    	case 16: /* Rect */
	    gdb_read_memory(&rect, addrStr, sizeof(rect));
	    gdb_printf("Displaying Rect\n"
	               " %.8lX: %d %d %d %d (t,l,b,r) %d %d (w,h)\n",
		       addr, rect.top, rect.left, rect.bottom, rect.right,
		       rect.right - rect.left, rect.bottom - rect.top);
    	    break;
	    
	default: 
	    if (n < 0)
		gdb_error("Unrecognized type.");
	    else
		gdb_fprintf(gdb_current_stderr,
			"%s is not supported in the gdb MacsBug commands.\n"
		        "Use the gdb PRINT command.  With appropriate type\n"
		        "casts you'll probably get better results anyhow.\n", argv[2]);
    	    break;
    }
}

#define DM_HELP \
"DM [addr [n | basicType]] -- Display memory from addr for n bytes or as a basic type.\n" \
"The basic types are Byte, Word, Long, SignedByte, SignedWord, SignedLong,\n"		\
"UnsignedByte, UnsignedWord, UnsignedLong, PString, CString, Boolean,\n"		\
"Binary8, Binary16, Binary32, OSType, and Rect.\n"					\
"\n"											\
"Also see SET for [mb-]ditto mode (HELP set mb-ditto).\n"							\
"\n"											\
"Macsbug features not supported: templates and the following basic types.\n"		\
"                                Pointer, Handle, RGBColor, Text, IORefNum,\n"		\
"                                VRefNum, Seconds, ATrapWord, AbsTicks,\n"		\
"                                TickInterval, Region, RgnHandle, IOTrapWord,\n"	\
"                                Version, Fixed, ShortFixed, UnsignedFixed,\n"		\
"                                and Fract."


/*-------------------------------------------------------------------------------*
 | DMA [addr [n]] -- Display memory as ASCII from addr for n (default 512) bytes |
 *-------------------------------------------------------------------------------*/

static void dma(char *arg, int from_tty)
{
    int  	  argc, n;
    unsigned long addr;
    char 	  *argv[5], tmpCmdLine[1024];
    
    __is_running("The program is not running.", 0);
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "DDMAM", &argc, argv, 4);
    
    /* Handle the default case...							*/
    
    if (argc == 1) {
    	if (gdb_get_int("$__lastcmd__") == 37) {
	    gdb_eval("$dot=(unsigned char *)$dot+$__prev_dma_n__");
	    back_up_over_prompt(gdb_get_int("$dot"), 0, from_tty);
	} else {
	    gdb_printf("Displaying memory from %.8lX\n", gdb_get_int("$dot"));
	    gdb_set_int("$__prev_dma_n__", 512);
	}
 	
	__asciidump("$dot $__prev_dma_n__", from_tty);
	
	gdb_set_int("$__lastcmd__", 37);
	return;
   }
    
    /* Handle the DMA addr case...							*/
    
    addr = gdb_get_int(argv[1]);
   
    if (argc == 2) {
	gdb_set_int("$dot", addr);		/* $dot=addr				*/
	gdb_set_int("$__prev_dma_n__", 512);
	
	gdb_printf("Displaying memory from %.8lX\n", addr);
	__asciidump("$dot 512", from_tty);
	
	gdb_set_int("$__lastcmd__", 37);
	return;
    }
    
    /* Handle the DMA addr n case...							*/
    
    if (argc > 3)
    	gdb_error("usage: DMA [addr [n]] (wrong number of arguments)");
    
    n = gdb_get_int(argv[2]);
    gdb_set_int("$dot", addr);
    gdb_set_int("$__prev_dma_n__", n);
    
    gdb_printf("Displaying memory from %.8lX\n", addr);
    __asciidump("$dot $__prev_dma_n__", from_tty);
    
    gdb_set_int("$__lastcmd__", 37);
}

#define DMA_HELP \
"DMA [addr [n]] -- Display memory as ASCII from addr for n (default 512) bytes.\n"	\
"ASCII characters outside the range 0x20 to 0x7F are shown as '.'s.\n"


/*--------------------------------------------------*
 | DP [addr] -- Display 128 bytes at addr (or $dot) |
 *--------------------------------------------------*/

static void dp(char *arg, int from_tty)
{
    unsigned long addr;
    
    __is_running("The program is not running.", 0);
    
    if (arg && *arg) {
    	addr = gdb_get_int(arg);
	gdb_set_int("$dot", addr);
	gdb_printf("Displaying memory from %.8X\n", addr);
    } else if (gdb_get_int("$__lastcmd__") == 6) {
    	addr = gdb_get_int("$dot") + 128;
	back_up_over_prompt(addr, 0, from_tty);
 	gdb_set_int("$dot", addr);
    } else {
    	addr = gdb_get_int("$dot");
	gdb_printf("Displaying memory from %.8X\n", addr);
    }
    
    __hexdump("$dot 128", from_tty);
    
    gdb_set_int("$__lastcmd__", 6);
}

#define DP_HELP "DP [addr] -- Display 128 bytes at addr (or $dot)."


/*-------------------------------*
 | dv [v] - display version info |
 *-------------------------------*/

static void dv(char *arg, int from_tty)
{
    time_t    t;
    struct tm *ts;
    long      year;
    
    gdb_printf("\nGdb/Macsbug " VERSION ", Copyright Apple Computer, Inc. 2000-2002");
    
    time(&t);
    ts = localtime(&t);

    year = 1900 + ts->tm_year;
    
    if (year > 2000)
    	gdb_printf("-%d", year);
    
    gdb_printf("\n\n");
   
    gdb_set_int("$__lastcmd__", 12);
    
    if (arg && *arg && !gdb_strcmpl(arg, "V"))
      	gdb_error("Invalid option (V exprected)");
    else if (!arg)
     	gdb_printf("  Written by Ira L. Ruben.\n\n");
}

#define DV_HELP \
"DV [V] -- Display MacsBug version.\n" \
"V: Display version only, do not show credits."


/*-------------------------------------------------------------*
 | DW [addr] -- Display in hex the two bytes at addr (or $dot) |
 *-------------------------------------------------------------*/

static void dw(char *arg, int from_tty)
{
    db_dw_and_dl(arg, from_tty, 2, 7);
}

#define DW_HELP "DW [addr] -- Display in hex the two bytes at addr (or $dot)."


/*---------------------------------------------------------------------*
 | FB addr n expr|"string" - search from addr to addr+n-1 for the byte |
 *---------------------------------------------------------------------*/

static void fb(char *arg, int from_tty)
{
    static void find(char *arg, int from_tty);
    
    find_size = 1;
    findName  = "FB";
    find(arg, from_tty);
}

#define FB_HELP  \
"FB addr n expr|\"string\" -- Search from addr to addr+n-1 for the byte.\n"		\
"\n"											\
"Note, FB is also an alias for gdb's FUTURE-BREAK command.  The syntax of\n"		\
"the FB parameters determines whether FB is treated as a MacsBug FB or a\n"		\
"gdb FUTURE-BREAK alias."


/*--------------------------------------------------------------------------*
 | FILL addr n expr|"string" - fill from addr to addr+n with expr or string |
 *--------------------------------------------------------------------------*/

static void fill(char *arg, int from_tty)
{
   int 		  argc, n, len, i;
   unsigned short w;
   unsigned long  addr, addr0, value;
   char 	  b, *argv[6], addrStr[20], str[1025], tmpCmdLine[1024];
      
    __is_running("The program is not running.", 0);
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "FILL", &argc, argv, 5);
    
    if (argc != 4)
    	gdb_error("usage: FILL addr n expr|\"string\" (wrong number of arguments)");
    
    addr  = addr0 = gdb_get_int(argv[1]);
    n     = gdb_get_int(argv[2]);
    
    gdb_set_int("$dot", addr);
    
    if (gdb_is_string(argv[3])) {			/* FILL addr n "string"		*/
    	gdb_get_string(argv[3], str, 1024);
    	len = strlen(str);
    	i = 0;
	
	while (n--) {
	    sprintf(addrStr, "0x%lX", addr++);
	    gdb_write_memory(addrStr, &str[i], 1);
	    if (++i >= len)
	    	i = 0;
	}
    } else {						/* FILL addr n value		*/
    	value = gdb_get_int(argv[3]);
	if ((long)value >= -128 && (long)value <= 255) {
	    b = value;
	    while (n--) {
	    	sprintf(addrStr, "0x%lX", addr++);
	    	gdb_write_memory(addrStr, &b, 1);
	    }
	} else if ((long)value >= -32768 && (long)value <= 65535) {
	    w = value;
	    i = 0;
	    while (n--) {
	    	sprintf(addrStr, "0x%lX", addr++);
		b = (value >> (8 - (i++%2)*8)) & 0xFF;
		gdb_write_memory(addrStr, &b, 1);
	    }
	} else {
	    i = 0;
	    while (n--) {
	    	sprintf(addrStr, "0x%lX", addr++);
		b = (value >> (24 - (i++%4)*8)) & 0xFF;
		gdb_write_memory(addrStr, &b, 1);
	    }
	}
    }
    
    sprintf(addrStr, "0x%lX", addr0);
    gdb_printf("Memory set starting at %s\n", addrStr);
    __hexdump(addrStr, from_tty);
    
    gdb_set_int("$__lastcmd__", 34);
}

#define FILL_HELP \
"FILL addr n expr|\"string\" -- Fill from addr to addr+n-1 with expr or string.\n"	\
"The fill is repeatedly used as a byte, word, or long (determined by the size\n"	\
"of the expr value) or the string."


/*------------------------------------------------------------------*
 | FIND addr n expr - search from addr to addr+n-1 for the pattern. |
 *------------------------------------------------------------------*/

static void find(char *arg, int from_tty)
{
   int 		  argc, size, found, len, i;
   unsigned short w;
   unsigned long  addr, limit, value, l;
   char 	  b, *buf, s[20], *argv[7], str[1025], addrStr[20], tmpCmdLine[1024];
    
    /* Since FIND is itself a command, but also used for FB, FW, and FL, we need to 	*/
    /* pass their information through globals.  Specifically find_size is 1, 2, or 4	*/
    /* for FB, FW, and FL respectively and findName is the command name.  		*/
    
    size = find_size;				/* 0, 1, 2, or 4			*/
    find_size = 0;				/* reset to FIND unless FB/FW/FL used	*/
    if (size == 0)
    	findName = "FIND";
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), findName, &argc, argv, 6);
    
    /* FB is also be an alias for the gdb FUTURE_BREAK command.  Most breadkpoint	*/
    /* commands take a single argument unless qualified with "if" or "thread".  Based	*/
    /* on that, if we have FB (size == 1) we assume the command is FUTURE_BREAK and	*/
    /* let it be parsed as that.  But we use FU, not FB for the command name otherwise	*/
    /* we get ourselves into a recursive loop!						*/
    
    if (argc != 4 || 
        (argc >= 4 && (strcmp(argv[2], "if") == 0 || strcmp(argv[2], "thread") == 0))) {
    	if (size == 1) {
	    gdb_execute_command("fu %s", arg);
	    return;
	}
    	gdb_error("usage: %s addr n expr|\"string\" (wrong number or invalid of arguments)", findName);
    }
      
    __is_running("The program is not running.", 0);
    
    /* Set up to search from addr to add+n-1.  The limit will actually be limit+n minus	*/
    /* the size of the pattern we're looking for (string lenght, 1, 2, or 4).		*/
    
    addr  = gdb_get_int(argv[1]);
    limit = addr + gdb_get_int(argv[2]);
    found = 0;
    
    if (gdb_is_string(argv[3])) {		/* FIND/FB/FW/FL addr n "string"	*/
    	gdb_get_string(argv[3], str, 1024);
    	len = strlen(str);
    	
	gdb_printf("Searching for \"");
	for (i = 0; i < len; ++i)
	    gdb_printf("%s", filter_char(str[i], 1, s));
	gdb_printf("\" from %.8lX to %.8lX\n", addr, limit-1);
	
    	buf = gdb_malloc(len);
	limit -= len;
	while (addr < limit) {
	    sprintf(addrStr, "0x%lX", addr++);
	    gdb_read_memory(buf, addrStr, len);
	    if (memcmp(buf, str, len) == 0) {
	    	found = 1;
		break;
	    }
	}
	gdb_free(buf);
    } else {					/* FIND/FB/FW/FL addr n value		*/
    	value = gdb_get_int(argv[3]);
	switch (size) {
	    case 0:				/* FIND					*/
	       if ((long)value >= -128 && (long)value <= 255)
		   goto FB;
	       else if ((long)value >= -32768 && (long)value <= 65535)
		   goto FW;
	       else
		   goto FL;
	   
	    case 1:				/* FB					*/
	    FB: value = value & 0xFF;
	    	gdb_printf("Searching for 0x%.2X from 0x%.8lX to 0x%.8lX\n", value, addr, limit-1);
	   	while (addr < limit) {
		    sprintf(addrStr, "0x%lX", addr++);
		    gdb_read_memory(&b, addrStr, 1);
		    if (b == value) {
		    	found = 1;
			break;
		    }
		}
	        break;
		
	    case 2:				/* FW					*/
	    FW: value = value & 0xFFFF;
	    	gdb_printf("Searching for 0x%.4X from 0x%.8lX to 0x%.8lX\n", value, addr, limit-1);
	    	limit -= 2;
	   	while (addr < limit) {
		    sprintf(addrStr, "0x%lX", addr++);
		    gdb_read_memory(&w, addrStr, 2);
		    if (w == value) {
		    	found = 1;
			break;
		    }
		}
	        break;
	   
	    case 4:				/* FL					*/
	    FL: gdb_printf("Searching for 0x%.8lX from 0x%.8lX to 0x%.8lX\n", value, addr, limit-1);
	    	limit -= 4;
	        while (addr < limit) {
		    sprintf(addrStr, "0x%lX", addr++);
		    gdb_read_memory(&l, addrStr, 4);
		    if (l == value) {
		    	found = 1;
			break;
		    }
		}
	        break;
	   
	   default:
		gdb_error("Internal error -- invalid size passed to %s", findName);
	}
    }
    
    if (found) {
    	gdb_set_int("$dot", addr);
	__hexdump(addrStr, from_tty);
    } else if (macsbug_screen)
	gdb_fprintf(gdb_current_stderr, " Not found\n");
    else
	gdb_printf(COLOR_RED " Not found" COLOR_OFF "\n");
    
    gdb_set_int("$__lastcmd__", 35);
}

#define FIND_HELP \
"FIND addr n expr -- Search from addr to addr+n-1 for the pattern.\n"			\
"If pattern is an expr then the width of the pattern is the smallest\n"			\
"unit (byte, word or long) that contains its value.\n"					\
"\n"											\
"Restriction: The expr value may not have any embedded blanks.  For example\n"		\
"             a value like (unsigned char *)&a is invalid.\n"				\
"\n"											\
"Macsbug features not supported: MacsBug F must be FIND here since F conflicts\n"	\
"                                with the gdb FRAME command abbreviation.\n"		\
"\n"											\
"                                Double quoted \"string\" instead of single quoted\n"	\
"                                'string'."


/*----------------------------------------------------------------------------*
 | FL addr n expr|"string" - search from addr to addr+n-1 for the 4-byte long |
 *----------------------------------------------------------------------------*/

static void fl(char *arg, int from_tty)
{
    find_size = 4;
    findName  = "FL";
    find(arg, from_tty);
}

#define FL_HELP  \
"FL addr n expr|\"string\" -- Search from addr to addr+n-1 for the 4-byte long."


/*-----------------------------------------*
 | G [addr] - continue execution (at addr) |
 *-----------------------------------------*
 
 The G command is written as a plugin for the following reasons:
 
 1. G is executed as RA if the target is not currently running.
    
    Once upon a time this was done by changing the command line from G to RA (and before
    that, R) in preprocess_commands() because G was a DEFINE command and the implications
    of G at the time were not observed where typing G to start the target and typing just
    a return to repeat the G at the next breakpoint wouldn't do another G but a RA (or R)
    instead.  By doing it here the command line remains a G so we get the desired effect.
    
 2. As a plugin a bug in gdb doing it as a DEFINE does not occur.
 
    If a breakpoint is defined with associated commands, then continuing or starting
    executing with a CONTINUE or RUN in a define command will NOT execute the associated
    commands when the breakpoint is hit.  This is a bug in gdb.  It does not occur when
    we do this from a plugin.
*/

static void g(char *arg, int from_tty)
{
    if (!gdb_target_running())			/* if not running...			*/
    	ra(arg, from_tty);			/* ...treat G as a RA command		*/
    else {					/* if running...			*/
	if (arg && *arg)			/* ...set $pc to specified addr		*/
	    gdb_eval("$pc=%s", arg);
	
	gdb_set_int("$__lastcmd__", 8);
	
	gdb_continue_command(NULL, from_tty);	/* ...continue execution		*/
    }
}

#define G_HELP \
"G [addr] -- Continue execution (at addr if supplied).\n"				\
"\n"											\
"Note, G is treated as a RA (\"run again\") command if the target\n"			\
"program is not currently running."


/*----------------------------------------------------------------------------*
 | FW addr n expr|"string" - search from addr to addr+n-1 for the 2-byte word |
 *----------------------------------------------------------------------------*/

static void fw(char *arg, int from_tty)
{
    find_size = 2;
    findName  = "FW";
    find(arg, from_tty);
}

#define FW_HELP  \
"FW addr n expr|\"string\" -- Search from addr to addr+n-1 for the 2-byte word."


/*-----------------------------------------------------------*
 | GT gdb-spec - go (continue) until the gdb-spec is reached |
 *-----------------------------------------------------------*/

static void gt(char *arg, int from_tty)
{
    if (!arg || !*arg)
    	gdb_error("usage: GT gdb-spec (gdb-spec expected)");
    
    gdb_execute_command("tbreak %s", arg);	/* tbreak gdb-spec			*/
    
    gdb_set_int("$__lastcmd__", 36);
    
    if (gdb_target_running()) 
    	gdb_continue_command(NULL, from_tty);
    else
    	ra(NULL, from_tty);
}

#define GT_HELP \
"GT gdb-spec -- Go (continue) until the gdb-spec is reached.\n" 			\
"This implements the gdb \"TBREAK gdb-spec\" followed by a CONTINUE\n"			\
"command.  See gdb TBREAK documentation for further details.\n" 			\
"\n"											\
"Note, GT is treated as a TBREAK followed by RA (\"run again\")\n"			\
"command if the target program is not currently running.\n"				\
"\n" 											\
"Macsbug features not supported: Command list not allowed."				\


/*---------------------------------------------------------*
 | ID [addr] - disassemble 1 line starting ar addr (or pc) |
 *---------------------------------------------------------*/

static void id(char *arg, int from_tty)
{
    unsigned long addr;
    char 	  line[1024];
    
    __is_running("The program is not running.", 0);
    
    if (arg && *arg) {
    	__reset_current_function(NULL, 0);
    	addr = gdb_get_int(arg);
    } else if (gdb_get_int("$__lastcmd__") == 9) {
	addr = gdb_get_int("$dot") + 4;
	back_up_over_prompt(addr, branchTaken, from_tty);
    } else {
    	__reset_current_function(NULL, 0);
	addr = gdb_get_int("$pc");
    }

    gdb_set_int("$dot", addr);
    sprintf(line, "0x%lX 1 %d", addr, macsbug_screen);
    __disasm(line, from_tty);
    
    gdb_set_int("$__lastcmd__", 9);
}

#define ID_HELP \
"ID [addr] -- Disassemble 1 line starting at addr (or pc).\n"				\
"\n"											\
"Macsbug features not supported: -c option."


/*-----------------------------------------------------------------------*
 | IL [addr [n]] -- disassemble n (default 20) lines from the addr or pc |
 *-----------------------------------------------------------------------*/

static void il(char *arg, int from_tty)
{
    int  	  argc, n;
    unsigned long addr;
    char 	  *argv[5], tmpCmdLine[1024];
    
    __is_running("The program is not running.", 0);
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "IL", &argc, argv, 4);
    
    if (argc == 1) {
	if (gdb_get_int("$__lastcmd__") == 11) {
	    addr = gdb_get_int("$dot") + 4*20;
	    back_up_over_prompt(addr, branchTaken, from_tty);
	} else {
	    __reset_current_function(NULL, 0);
	    addr = gdb_get_int("$pc");
	}
	n = 20;
    } else {
	__reset_current_function(NULL, 0);
	if (argc == 2) {
	    addr = gdb_get_int(argv[1]);
	    n    = 20;
	} else if (argc == 3) {
	    addr = gdb_get_int(argv[1]);
	    n    = gdb_get_int(argv[2]);
	} else
	    gdb_error("usage: IL [addr [n]] (wrong number of arguments)");
    }
    
    gdb_set_int("$dot", addr);
    
    sprintf(tmpCmdLine, "0x%lX %ld", addr, n);
    __disasm(tmpCmdLine, from_tty);
    
    gdb_set_int("$__lastcmd__", 11);
}

#define IL_HELP \
"IL [addr [n]] -- Disassemble n (default 20) lines from the addr or pc.\n"		\
"\n"											\
"Macsbug features not supported: -c option."


/*-------------------------------------------------------------------*
 | IP [addr] - Disassemble 20 lines centered around the addr (or pc) |
 *-------------------------------------------------------------------*/

static void ip(char *arg, int from_tty)
{
    int  	  argc;
    unsigned long addr;
    char 	  *argv[4], tmpCmdLine[1024];
    
    __is_running("The program is not running.", 0);
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "IL", &argc, argv, 3);
    
    if (argc == 1) {
	if (gdb_get_int("$__lastcmd__") == 13) {
	    addr = gdb_get_int("$dot") + 4*21;
	    back_up_over_prompt(addr - 40, branchTaken, from_tty);
	} else {
	    __reset_current_function(NULL, 0);
	    addr = gdb_get_int("$pc");
	}
    } else {
	__reset_current_function(NULL, 0);
	if (argc != 2)
	    gdb_error("usage: IP [addr] (wrong number of arguments)");
	addr = gdb_get_int(argv[1]);
    }
    
    gdb_set_int("$dot", addr);
    
    sprintf(tmpCmdLine, "0x%lX 21", addr-40);
    __disasm(tmpCmdLine, from_tty);
    
    gdb_set_int("$__lastcmd__", 13);
}

#define IP_HELP \
"IP [addr] -- Disassemble 20 lines centered around the addr (or pc).\n"			\
"\n"											\
"Macsbug features not supported: -c option."


/*---------------------------------------------*
 | MR - return from current frame (gdb FINISH) |
 *---------------------------------------------*/

static void mr(char *arg, int from_tty)
{
    if (arg && *arg)
    	gdb_error("Only the parameterless form of MR is supported.");
    else
    	gdb_execute_command("finish");
    
    gdb_set_int("$__lastcmd__", 15);
}

#define MR_HELP \
"MR -- Return from current frame.\n"							\
"\n"											\
"Macsbug features not supported: offset and addr arguments."


/*------------------------------*
 | PC - display the value of PC |
 *------------------------------*/

static void pc(char *arg, int from_tty)
{
    static void _rn(int r, int from_tty);
    
    _rn(-1, from_tty);				/* -1 means use $pc in rn()		*/
}

#define HELP_PC "PC -- Display the value of PC"


/*--------------------------------------------------------------------------*
 | check_confirm_status - stdout filter for SHOW confirm command done by RA |
 *--------------------------------------------------------------------------*
 
 Checks the SHOW confirm display line to see if confirmation is currently enabled. Sets
 *(int *)data to 1 if it is enabled and 0 if it isn't.
*/

static char *check_confirm_status(FILE *f, char *line, void *data)
{
    if (line)
    	*(int *)data = (strstr(line, "off.") != NULL) ? 0 : 1;
    
    return (NULL);
}


/*-------------------------------------------------------------------*
 | RA [args] - unconditionaly restart debugged program ("run again") |
 *-------------------------------------------------------------------*
 
 This is NOT a MacsBug command but it is useful in a gdb environment.  The HELP info is
 placed in the "run" class so it appears with the HELP running command list instead of
 the MacsBug command list.
*/
 
static void ra(char *arg, int from_tty)
{
    GDB_FILE *redirect_stdout, *redirect_stderr;
    int      confirm_status;
    char     *cmdLine = gdb_get_previous_command();
        
    /* Determine if confirmation is on or off...					*/
    
    redirect_stdout = gdb_open_output(stdout, check_confirm_status, &confirm_status);
    
    gdb_redirect_output(redirect_stdout);
    gdb_execute_command("show confirm");
    gdb_close_output(redirect_stdout);
    
    /* If confirmation is currently on turn it off across the RUN command.  Otherwise	*/
    /* there is no need to turn it off...						*/
    
    if (confirm_status) {
    	gdb_execute_command("set confirm off");
	run_command(arg, from_tty);
    	gdb_execute_command("set confirm on");
    } else
	run_command(arg, from_tty);
    
    /* We saved the RA command line to restore here.  We need to do this because a	*/
    /* byproduct of the RUN command is to clear the previous command.  This prevents	*/
    /* it from executing again if the user just types an empty command line (i.e., just	*/
    /* a return).  It less confusing when RUN does this because of it's usual prompt	*/
    /* when RUN is executed (although answering 'n' to the prompt is equally confusing).*/
    /* But we elect to always rerun the command if RETURN is entered.  So we restore	*/
    /* the "previous" command behind gdb's back from here.				*/

    gdb_set_previous_command(cmdLine);
    gdb_free(cmdLine);
}

#define RA_HELP \
"Unconditionaly restart debugged program (\"run again\").\n"				\
"This is identical to RUN except no prompt is displayed if the program\n"		\
"is currently running.  The debugged program is unconditionally restarted.\n"		\
"See gdb RUN documentation for further details."


/*-----------------------------------------*
 | rn - display the value of Rn, SP, or PC |
 *-----------------------------------------*
 
 Common routine called by the individual commands R0-R31, PC, and SP.  The register
 number is passed.  For SP, 1 is passed since that is R1.  For PC -1 is passed.  -2
 for SP.  SP is really R1 but we want to show it as SP.
*/

static void _rn(int r, int from_tty)
{
    int		  i;
    unsigned long value;
    char          regName[10], uc_regName[10];
    
    /* Following causes an appropriate error message if regs are unavailable...		*/
    
    (void)gdb_get_int("$pc");
    
    if (r >= 0) {
    	sprintf(regName, "$r%d", r);
    	value = gdb_get_int(regName);
    } else if (r == -1)
    	value = gdb_get_int(strcpy(regName, "$pc"));
    else
    	value = gdb_get_int(strcpy(regName, "$sp"));
    
    i = strlen(strcpy(uc_regName, regName));
    while (--i >= 1)
    	uc_regName[i] = toupper(uc_regName[i]);
    
    gdb_printf("%s = 0x%.8lX   %lu    %ld    ", &uc_regName[1], value, value, value);
    __print_4(regName, from_tty);
    gdb_printf("\n");
    
    gdb_set_int("$__lastcmd__", 41);
}


/*--------------------------------------*
 | R0-R31 - display the value of R0-R31 |
 *--------------------------------------*
 
 All of these are similarly defined so we define them by macros.
*/

#define Rn(n) static void r##n(char *arg, int from_tty) {_rn(n, from_tty);}
#if 0
#define Rstr(x) #x
#define R_COMMAND(n) MACSBUG_COMMAND(r##n, Rstr(R##n) " -- display the value of " Rstr(R##n))
#else
#define R_COMMAND(n) MACSBUG_INTERNAL_COMMAND(r##n, NULL)
static void rn(char *arg, int from_tty)
{
    //gdb_execute_command("help rn");
    gdb_error("Type R<n>, where \"n\" is 0 to 31 to display the register value");
}
#define RN_HELP "R<n> [n = 0 to 31] -- Display value of register" 
#endif

Rn(0);  Rn(1);  Rn(2);  Rn(3);  Rn(4);  Rn(5);  Rn(6);  Rn(7);  Rn(8);  Rn(9);
Rn(10); Rn(11); Rn(12); Rn(13); Rn(14); Rn(15); Rn(16); Rn(17); Rn(18); Rn(19);
Rn(20); Rn(21); Rn(22); Rn(23); Rn(24); Rn(25); Rn(26); Rn(27); Rn(28); Rn(29);
Rn(30); Rn(31);


/*----------------------------------------------------------------------------------*
 | sb_sw_sl_and_sm addr value1 [... valueN] - common routine for SB, SW, SL, and SM |
 *----------------------------------------------------------------------------------*
 
 The arg is the SB, SW, SL, and SM arg.  The size is 1 (SB), 2(SW), 4 (SL), or 0 (SM).
 The cmdName is "SB", "SW", "SL", or "SM" for error messages.
*/

static void sb_sw_sl_and_sm(char *arg, int from_tty, int size, int cmdNbr, char *cmdName)
{
    int            i, argc, isstr, len;
    unsigned char  b;
    unsigned short w;
    unsigned long  addr, start, value;
    char 	   *argv[41], addrStr[20], str[1024], tmpCmdLine[1024];
    
    __is_running("The program is not running.", 0);
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), cmdName, &argc, argv, 40);
    
    if (argc < 3)
    	gdb_error("usage: %s addr value1 [... valueN]", cmdName);
    
    addr = start = gdb_get_int(argv[1]);	/* start with this address		*/
    
    /* For each value write the target's memory according to type...			*/
    
    for (i = 2; i < argc; ++i) {		/* process all the args...		*/
    	isstr = gdb_is_string(argv[i]);
	
	if (i == 2)				/* 1st time around display the title	*/
	    gdb_set_int("$dot", addr);		/* set $dot to initial address		*/
	
	if (isstr) {				/* always write entire strings		*/
	    gdb_get_string(argv[i], str, 1023);
	    len = strlen(str);
	    sprintf(addrStr, "0x%lX", addr);
	    gdb_write_memory(addrStr, str, len);
	    addr += len;
	} else {				/* values written according to type	*/
	    sprintf(addrStr,  "0x%lX", addr);	/* need address as a string		*/
	    b = w = value = gdb_get_int(argv[i]);/* use 1, 2, or all 4 bytes of value	*/
	    
	    switch (size) {
	    	case 0:				/* SM					*/
		    if ((long)value >= -128 && (long)value <= 255)
		    	goto SB;
		    else if ((long)value >= -32768 && (long)value <= 65535)
		    	goto SW;
		    else
		    	goto SL;
		    
		case 1:				/* SB					*/
		SB: sprintf(addrStr,  "0x%lX", addr);
		    gdb_write_memory(addrStr, &b, 1);
		    ++addr;
		    break;
		    
		case 2:				/* SW					*/
	    	SW: sprintf(addrStr,  "0x%lX", addr);
		    gdb_write_memory(addrStr, &w, 2);
		    addr += 2;
		    break;
		    
		case 4:				/* SL					*/
	    	SL: gdb_write_memory(addrStr, &value, 4);
		    addr += 4;
		    break;
		    
		default:
		    gdb_error("Internal error -- invalid size passed to %s", cmdName);
	    }
	}
    }
    
    /* Hexdump the results...								*/
    
    gdb_printf("Memory set starting at %.8lX\n", start);
    sprintf(str, "0x%lX %ld", start, 
    	      ((addr - start + hexdump_width - 1)/hexdump_width)*hexdump_width);
    __hexdump(str, from_tty);			/* __hexdump start N			*/
    
    gdb_set_int("$__lastcmd__", cmdNbr);
}


/*---------------------------------------------------------*
 | SB addr value - assign values to bytes starting at addr |
 *---------------------------------------------------------*/

static void sb(char *arg, int from_tty)
{
    sb_sw_sl_and_sm(arg, from_tty, 1, 17, "SB");
}

#define SB_HELP \
"SB addr values -- Assign values to bytes starting at addr.\n"				\
"String values are fully assigned at the next assignable byte.\n"			\
"\n"											\
"Macsbug features not supported: Double quoted \"string\" instead of single quoted\n"	\
"                                'string'."


/*-----------------------------------------------------------*
 | SC [n] - stack crawl (only the n most inner/outer frames) |
 *-----------------------------------------------------------*/

static void sc(char *arg, int from_tty)
{
    if (arg && *arg)
	gdb_execute_command("bt %s", arg);
    else
    	gdb_execute_command("bt");
    
    gdb_set_int("$__lastcmd__", 18);
}

#define SC_HELP \
"SC [n] -- Display back trace (stack crawl).\n"						\
"\n"											\
"MacsBug extensions: Optional \"n\" is a positive or negative value.  A positive\n"	\
"                    value indicates to display only the innermost n frames.\n"		\
"                    Negative means display only the outermost n frames."


/*-------------------------------------------------------*
 | so_si_filter - stdout output filter for __so and __si |
 *-------------------------------------------------------*
 
 This is used around a stepi and nexti so that we may conditionally suppress the source
 line display.
 
 It is never suppressed when rhe macsbug screen is off.  When on we display the source
 lines (not repeats of the line where gdb shows the address) but that too can be
 suppressed with the show_so_si_src flag.
*/

static char *so_si_filter(FILE *f, char *line, void *data)
{
    if (line) {
	if (!macsbug_screen)
	    gdb_fprintf(*(GDB_FILE **)data, "%s", line);
	else if (show_so_si_src && (line[0] != '0' || line[1] != 'x'))
	    gdb_fprintf(*(GDB_FILE **)data, "%s", line);
    }
    
    return (NULL);
}


/*------------------------------------------*
 | so_and_si - common routine for SO and SI |
 *------------------------------------------*
 
 Note that SO and SI are written as plugins because we need to play silly games with
 the stepi/nexti source display when drawing to the macsbug screen.
*/

static void so_and_si(char *arg, int from_tty, int cmdNbr, char *cmdName, char *gdbCmd)
{
    GDB_FILE 	  *redirect_stdout, *prev_stdout;
    int      	  argc, step, n;
    unsigned long pc;
    char     	  *argv[5], line[1024], tmpCmdLine[1024];
    
    if (!gdb_target_running())
    	gdb_error("The program is not running.");
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), cmdName, &argc, argv, 4);
   
    if (argc > 3)
    	gdb_error("%s called with wrong number of arguments.", cmdName);
	
    if (argc == 1) {
    	step = 1;
	n    = macsbug_screen ? 1 : pc_area_lines;
    } else if (argc == 2) {
    	step = gdb_get_int(argv[1]);
	n    = macsbug_screen ? 1 : pc_area_lines;
    } else {
    	step = gdb_get_int(argv[1]);
    	n    = gdb_get_int(argv[2]);
    }
   
    if (!macsbug_screen)
    	__reset_current_function(NULL, 0);
	
    if (macsbug_screen) {
    	sprintf(line, "$pc %ld 1", n);
    	__disasm(line, from_tty);
    }
    
    redirect_stdout = gdb_open_output(stdout, so_si_filter, &prev_stdout);
    prev_stdout = gdb_redirect_output(redirect_stdout);
    
    sprintf(line, "%s %ld\n", gdbCmd, step);
    gdb_execute_command(line);
    
    gdb_close_output(redirect_stdout);
    
    gdb_set_int("$dot", gdb_get_int("$pc"));
    
    if (!macsbug_screen) {
    	sprintf(line, "$pc %ld", n);
    	__disasm(line, from_tty);
    }
    
    gdb_set_int("$__lastcmd__", cmdNbr);
}


/*-------------------------------------------------------------------------*
 | si [n [m]] - Step in n instructions and disassemble m lines from new pc |
 *-------------------------------------------------------------------------*/

static void si(char *arg, int from_tty)
{
    so_and_si(arg, from_tty, 16, "SI", "stepi");
}

#define SI_HELP \
"SI [n] [m] -- Step n (or 1) instruction(s) and disassemble m lines from new pc.\n" \
"See also gdb's next and step instructions, which step by source lines,\n" 		   \
"not instructions.\n" 									   \
"\n" 											   \
"The second argument is a extension to the MacsBug syntax to allow a disassembly\n" 	   \
"of m lines to show the next instructions to be executed.  This approximates the\n" 	   \
"disassembly window that MacsBug always shows.\n" 					   \
"\n"											   \
"Macsbug features not supported: S expr\n" 						   \
"\n" 											   \
"                                The MacsBug S is SI here.  This was done to preserve\n"   \
"                                gdb's definition of S[tep] to single statement step.\n"   \
"                                There is (was) a gdb SI to which does exactly what\n" 	   \
"                                this MacsBug SI does except that now the instruction\n"   \
"                                is also displayed at each instruction step.\n"		   \
"\n"											   \
"Note, the gdb SI abbreviation for STEPI is overridden by this MacsBug command."


/*---------------------------------------------------------*
 | SL addr value - assign values to longs starting at addr |
 *---------------------------------------------------------*/

static void sl(char *arg, int from_tty)
{
    sb_sw_sl_and_sm(arg, from_tty, 4, 21, "SL");
}

#define SL_HELP \
"SL addr values -- Assign values to (4-byte) longs starting at addr.\n"				\
"String values are fully assigned at the next assignable byte.\n"			\
"\n"											\
"Macsbug features not supported: Double quoted \"string\" instead of single quoted\n"	\
"                                'string'."


/*----------------------------------------------------------*
 | SM addr value - assign values to memory starting at addr |
 *----------------------------------------------------------*/

static void sm(char *arg, int from_tty)
{
    sb_sw_sl_and_sm(arg, from_tty, 0, 22, "SM");
}

#define SM_HELP \
"SM addr value -- Assign values to memory starting at addr.\n"				\
"Each value determines the assignment size (byte, 2-byte word, or 4-byte\n"		\
"long).  Specific sizes can be set using SB, SW, or SL.  String values\n"		\
"are fully assigned at the next assignable byte.\n"					\
"\n"											\
"Macsbug features not supported: Double quoted \"string\" instead of single quoted\n"	\
"                                'string'."


/*---------------------------------------------------------------------------*
 | so [n [m]] - Step over n instructions and disassemble m lines from new pc |
 *---------------------------------------------------------------------------*/
 
static void so(char *arg, int from_tty)
{
    so_and_si(arg, from_tty, 23, "SO", "nexti");
}

#define SO_HELP \
"SO [n] [m] -- Step over n instructions and disassemble m lines from new pc.\n"		\
"See also gdb's next and step instructions, which step by source lines,\n"		\
"not instructions.\n" 									\
"\n" 											\
"The second argument is a extension to the MacsBug syntax to allow a disassembly\n" 	\
"of m lines to show the next instructions to be executed.  This approximates the\n" 	\
"disassembly window that MacsBug always shows."


/*-----------------------------------*
 | SP - display the value of SP (r1) |
 *-----------------------------------*/

static void sp(char *arg, int from_tty)
{
    _rn(-2, from_tty);
}

#define HELP_SP "SP -- Display the value of SP (r1)"


/*---------------------------------------------------------*
 | SW addr value - assign values to words starting at addr |
 *---------------------------------------------------------*/

static void sw(char *arg, int from_tty)
{
    sb_sw_sl_and_sm(arg, from_tty, 2, 24, "SW");
}

#define SW_HELP \
"SW addr values -- Assign values to (2-byte) words starting at addr.\n"				\
"String values are fully assigned at the next assignable byte.\n"			\
"\n"											\
"Macsbug features not supported: Double quoted \"string\" instead of single quoted\n"	\
"                                'string'."


/*------------------------------------*
 | T [n [m]] - same as SO (step over) |
 *------------------------------------*
 
 Note that SO and SI are written as plugins because we need to play silly games with
 the stepi/nexti source display when drawing to the macsbug screen.
*/

static void t(char *arg, int from_tty)
{
    so_and_si(arg, from_tty, 25, "SO", "nexti");
}

#define T_HELP "T [n] [m] -- Same as SO."


/*--------------------------------------------*
 | TD - display integer and machine registers |
 *--------------------------------------------*
 
 Example of output format:
 
 PowerPC Registers
                         CR0  CR1  CR2  CR3  CR4  CR5  CR6  CR7
  PC  = 05AB2950     CR  0100 0010 0000 0000 0000 1000 0100 1000
  LR  = 05AB29A4         <>=O XEVO
  CTR = FFD6A848
  MSR = 00000000         SOC Compare Count
                     XER 000   00     00                     MQ  = 00000000
 
  R0  = 00000000     R8  = 00000000      R16 = 05B0FD25      R24 = 05B0FC90
  SP  = 05BCDF00     R9  = 05B150A0      R17 = 05B0FCAC      R25 = 0032822A
  R2  = 05B0AB4B     R10 = 00000000      R18 = 05B0FC8C      R26 = 00000003
  R3  = 0053866F     R11 = 00538E7A      R19 = 05B10668      R27 = 00000000
  R4  = 00000000     R12 = 0002CEA4      R20 = 05B0FCA9      R28 = 05BCE206
  R5  = 05BCDEF8     R13 = 00000000      R21 = 05B0FCAF      R29 = 05B32AD0
  R6  = 05ABAEC4     R14 = 00000000      R22 = 0000001E      R30 = 00538670
  R7  = 05B32AD0     R15 = 00000000      R23 = 00000000      R31 = 05B0FE6C
*/

static void td(char *arg, int from_tty)
{
    int		  i, j, reg;
    unsigned long cr, xer;
    char	  regName[20], tmpCmdLine[100];
    
    /* Following causes an appropriate error message if regs are unavailable...		*/
    
    (void)gdb_get_int("$pc");
    
    gdb_printf("PowerPC Registers\n");
    gdb_printf("                        CR0  CR1  CR2  CR3  CR4  CR5  CR6  CR7\n");
    gdb_printf(" PC  = %.8lX     CR ", gdb_get_int("$pc"));
    
    cr = gdb_get_int("$cr");
    for (i = 28; i >= 0; i -= 4) {
    	sprintf(tmpCmdLine, "0x%X 4", (cr >> i) & 0x0F);
	gdb_printf(" ");
	__binary(tmpCmdLine, from_tty);
    }
    gdb_printf("\n");
       
    gdb_printf(" LR  = %.8lX         <>=O XEVO\n", gdb_get_int("$lr"));
    gdb_printf(" CTR = %.8lX\n", gdb_get_int("$ctr"));
    gdb_printf(" MSR = %.8lX         SOC Compare Count\n", gdb_get_int("$ps"));
    gdb_printf("                    XER ");
    xer = gdb_get_int("$xer");
    sprintf(tmpCmdLine, "0x%X 3", (xer>> 29) & 7);
    __binary(tmpCmdLine, from_tty);
    gdb_printf("   %.2X    %.2X                     MQ  = %.8lX\n",
              (xer>>8) & 0xFF, (xer & 0x7F), gdb_get_int("$mq"));
   
    gdb_printf("\n");
    
    for (i = 0; i < 8; ++i) {
    	for (j = 0; j < 4; ++j) {
	    reg = i + j*8;
	    sprintf(regName, "$r%ld", reg);
	    if (reg <= 7)
	    	if (reg != 1)
		    gdb_printf(" R%-2d = %.8lX", reg, gdb_get_int(regName));
		else
		    gdb_printf(" SP  = %.8lX", gdb_get_int(regName));
	    else if (reg == i+8)
		gdb_printf("     R%-2d = %.8lX", reg, gdb_get_int(regName));
	    else
		gdb_printf("      R%-2d = %.8lX", reg, gdb_get_int(regName));
    	}
	gdb_puts("\n");
    }
    
    gdb_set_int("$__lastcmd__", 26);
}

#define TD_HELP "TD -- Display integer and machine registers."


/*-------------------------------------------*
 | TF - display the floating point registers |
 *-------------------------------------------*

 Example of output format:

 FPU Registers
                                                   S S
           F           N I I Z I                   O Q C
  FPSCR  F E V O U Z X A S D D M V F F             F R V V O U Z X N
         X X X X X X X N I I Z Z C R I C < > = ?   T T I E E E E E I RN
         1 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 00
  
  FPR0  = FFF 8000082004000     -NAN(000) 
  FPR1  = 408 4700000000000      6.540000000000000e+2
  - - -
  FPR30 = 000 0000000000000      0.000000000000000e+0
  FPR31 = 000 0000000000000      0.000000000000000e+0
*/

static void tf(char *arg, int from_tty)
{
    int		  i, size;
    unsigned long fpscr;
    char	  f[4];
    
    union {
	char          msg[50];
    	double	      d;
	struct {
	    unsigned long hi;
	    unsigned long lo;
	} hilo;
    } value, *v;
		
    /* Following causes an appropriate error message if regs are unavailable...		*/
    
    fpscr = gdb_get_int("$fpscr");
		
    gdb_printf("FPU Registers\n");
    gdb_printf("                                                  S S\n");
    gdb_printf("          F           N I I Z I                   O Q C\n");
    gdb_printf(" FPSCR  F E V O U Z X A S D D M V F F             F R V V O U Z X N\n");
    gdb_printf("        X X X X X X X N I I Z Z C R I C < > = ?   T T I E E E E E I RN\n");
    
    gdb_printf("        ");
    for (i = 31; i >= 2; --i)
    	gdb_printf("%d ", (fpscr >> i) & 1);
    gdb_printf("%d%d\n\n", (fpscr >> 1) & 1, fpscr & 1);
    
    for (i = 0; i < 32; ++i) {
    	sprintf(f, "$f%d", i);
	gdb_printf("FPR%-2d = ", i);
	
	v = gdb_get_register(f, &value, &size);
	
	if (v == NULL)
	    gdb_error(value.msg);
	
	/* Break hex up to 1 bit for sign, 11 bits for exponent, 52 bits for fraction.	*/
	/* That's followed by 5 spaces and the scientific notation to 15 digits.	*/ 
	
	gdb_printf("%.3lX %.5lX%.8lX     %- 15.15e\n", 
	           (value.hilo.hi >> 20) & 0xFFF, value.hilo.hi & 0x000FFFFF, value.hilo.lo,
		   value.d);
    }
    
    gdb_set_int("$__lastcmd__", 27);
}

#define TF_HELP "TF -- Display the floating point registers."


/*-----------------------------------*
 | TV - display the vector registers |
 *-----------------------------------*
 
 Vector Registers
                                                                       S
  VRsave = 00000000                    N                               A
                                       J                               T
  VSCR = 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 

  V0  = 00000000 00000000 00000000 00000000   0.0000e+0   0.0000e+0   0.0000e+0   0.0000e+0 
  V1  = 53706F74 20636865 636B206F 66207472   1.0327e+12  1.9262e-19  4.3373e+21  1.8943e+23 
  - - -
  V30 = 7FFFDEAD 7FFFDEAD 7FFFDEAD 7FFFDEAD   NAN(222)    NAN(222)    NAN(222)    NAN(222) 
  V31 = 7FFFDEAD 7FFFDEAD 7FFFDEAD 7FFFDEAD   NAN(222)    NAN(222)    NAN(222)    NAN(222)
*/

static void tv(char *arg, int from_tty)
{
    int 	  i, j, k, size;
    unsigned long vrsave, vscr;
    double	  d;
    char	  vn[4], vf[13];
    
    union {
	char          msg[50];
	unsigned long l[4];
	float	      f[4];
    } value, *v;
    
    /* Following causes an appropriate error message if regs are unavailable...		*/
    
    vrsave = gdb_get_int("$vrsave");
    
    gdb_printf("Vector Registers\n");
    gdb_printf("                                                                      S\n");
    gdb_printf(" VRsave = %.8lX", vrsave);
    gdb_printf("                    N                               A\n");
    gdb_printf("                                      J                               T\n");
    
    vscr = 0;//gdb_get_int("$vscr");	// gdb need to fix the type of vscr
    gdb_printf(" VSCR = ");
    for (i = 31; i >= 0; --i)
	gdb_printf("%d ", (vscr >> i) & 1);
    
    gdb_printf("\n\n");
    
    for (i = 0; i < 32; ++i) {
    	sprintf(vn, "$v%d", i);
	gdb_printf("V%-2d = ", i);
	
	v = gdb_get_register(vn, &value, &size);
	
	if (v == NULL)
	    gdb_error(value.msg);
	
	gdb_printf("%.8lX %.8lX %.8lX %.8lX  ", 
			value.l[0], value.l[1],value.l[2],value.l[3]);
	
	for (k = 0; k < 4; ++k) {
	    strcpy(vf, "            ");
	    //          123456789012
	    d = value.f[k];
	    j = sprintf(vf, "%- 4.4e", d);
	    vf[j] = ' ';
	    gdb_printf("%s ", vf);
	}
	
	gdb_printf("\n");
    }
    
    gdb_set_int("$__lastcmd__", 28);
}

#define TV_HELP "TV -- Display the vector registers."


/*----------------------------------------------------------------------*
 | WH [addr] - display the function corresponding to an address (or pc) |
 *----------------------------------------------------------------------*
 
 This is a plugin simply because you can't do it in the gdb command language.
*/

static void wh(char *arg, int from_tty)
{
    int  argc;
    char *addr, *argv[4], tmpCmdLine[1024];
    
    static char *prev_arg;
    
    __is_running("The program is not running.", 0);
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "WH", &argc, argv, 2);
    
    if (argc == 1)
    	/*
    	if (gdb_get_int("$__lastcmd__") == 42)
    	    addr = prev_arg;
	else
	*/
	    addr = "*$pc";
    else
    	addr = argv[1];
    
    gdb_print_address(addr, 1);
    prev_arg = addr;

    gdb_set_int("$__lastcmd__", 42);
}

#define WH_HELP \
"WH [*addr | linespec] -- Find the name and addr of the parameter.\n" 			\
"If no parameter then assume WH *$pc.  Type HELP INFO LINE or HELP\n"			\
"LIST for more info on linespecs.\n"							\
"\n" 							  				\
"Note that, if possible, the source line is listed exactly like it\n"			\
"was done using the gdb LIST command.  Thus the amount of context\n"			\
"lines is controlled by the SET listsize command.\n"					\
"\n" 							  				\
"Macsbug features not supported: Traps.\n"						\
"                                Numeric addresses and gdb convenience\n"		\
"                                variables must start with a '*'."

/*--------------------------------------------------------------------------------------*/

/*----------------------------------------*
 | init_from_gdb - initialize the plugins |
 *----------------------------------------*
 
 This is called from gdb() when LOAD-PLUGIN command is done on the file containing the
 MacsBug binary.  Thus the name "init_from_gdb" is predetermined by gdb.
 
 From here we call all the various initialization routines in the other files.  The
 plugin commands are defined here.  Other plugin commands associated with those other
 files are defined in those respective file's initialization routine.
*/

void init_from_gdb(void)
{
    gdb_initialize();
    
    macsbug_class 	   = gdb_define_class("macsbug", "MacsBug commands");
    macsbug_internal_class = Gdb_Private;
    macsbug_screen_class   = gdb_define_class("screen", "MacsBug screen commands");
    macsbug_testing_class  = Gdb_Private;
    macsbug_useful_class   = gdb_define_class("useful", "Possibly useful for DEFINE commands (some used by MacsBug)");

    gdb_change_class("mb-notes", Gdb_Private);
    
    MACSBUG_COMMAND(brc,  BRC_HELP);
    MACSBUG_COMMAND(brd,  BRD_HELP);
    MACSBUG_COMMAND(brm,  BRM_HELP);
    MACSBUG_COMMAND(brp,  BRP_HELP);
    MACSBUG_COMMAND(db,   DB_HELP);
    MACSBUG_COMMAND(dl,   DL_HELP);
    MACSBUG_COMMAND(dm,   DM_HELP);
    MACSBUG_COMMAND(dma,  DMA_HELP);
    MACSBUG_COMMAND(dv,   DV_HELP);
    MACSBUG_COMMAND(dp,   DP_HELP);
    MACSBUG_COMMAND(dw,   DW_HELP);
    MACSBUG_COMMAND(fb,   FB_HELP);
    MACSBUG_COMMAND(fill, FILL_HELP);
    MACSBUG_COMMAND(find, FIND_HELP);
    MACSBUG_COMMAND(fl,   FL_HELP);
    MACSBUG_COMMAND(fw,   FW_HELP);
    MACSBUG_COMMAND(g,    G_HELP);
    MACSBUG_COMMAND(gt,   GT_HELP);
    MACSBUG_COMMAND(id,   ID_HELP);
    MACSBUG_COMMAND(il,   IL_HELP);
    MACSBUG_COMMAND(ip,   IP_HELP);
    MACSBUG_COMMAND(mr,   MR_HELP);
    MACSBUG_COMMAND(sb,   SB_HELP);
    MACSBUG_COMMAND(sc,   SC_HELP);
    MACSBUG_COMMAND(si,   SI_HELP);
    MACSBUG_COMMAND(sl,   SL_HELP);
    MACSBUG_COMMAND(sm,   SM_HELP);
    MACSBUG_COMMAND(so,   SO_HELP);
    MACSBUG_COMMAND(sw,   SW_HELP);
    MACSBUG_COMMAND(t,    T_HELP);
    MACSBUG_COMMAND(td,   TD_HELP);
    MACSBUG_COMMAND(tf,   TF_HELP);
    MACSBUG_COMMAND(tv,   TV_HELP);
    MACSBUG_COMMAND(wh,   WH_HELP);
    
    COMMAND_ALIAS(id, idp);
    COMMAND_ALIAS(il, ilp);
    COMMAND_ALIAS(ip, ipp);
    COMMAND_ALIAS(sc, sc6);
    COMMAND_ALIAS(sc, sc7);
    
    R_COMMAND(0);  R_COMMAND(1);  R_COMMAND(2);  R_COMMAND(3);  R_COMMAND(4);
    R_COMMAND(5);  R_COMMAND(6);  R_COMMAND(7);  R_COMMAND(8);  R_COMMAND(9);
    R_COMMAND(10); R_COMMAND(11); R_COMMAND(12); R_COMMAND(13); R_COMMAND(14);
    R_COMMAND(15); R_COMMAND(16); R_COMMAND(17); R_COMMAND(18); R_COMMAND(19);
    R_COMMAND(20); R_COMMAND(21); R_COMMAND(22); R_COMMAND(23); R_COMMAND(24);
    R_COMMAND(25); R_COMMAND(26); R_COMMAND(27); R_COMMAND(28); R_COMMAND(29);
    R_COMMAND(30); R_COMMAND(31);
    MACSBUG_COMMAND(pc, HELP_PC);
    MACSBUG_COMMAND(sp, HELP_SP);
    MACSBUG_COMMAND(rn, RN_HELP);
    
    gdb_define_cmd("ra", ra, Gdb_Running, RA_HELP);
    gdb_enable_filename_completion("ra");
       
    init_macsbug_utils();
    init_macsbug_display();
    init_macsbug_patches();
    init_macsbug_set();
    init_macsbug_cmdline();
    
    gdb_continue_command = gdb_replace_command("continue", NULL);
    if (!gdb_continue_command)
	gdb_internal_error("internal error - continue command not found");
    
    if (!isatty(STDOUT_FILENO)) {
    	gdb_fixup_document_help("mb-notes");
    }
}
