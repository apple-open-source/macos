/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                   MacsBug_utils.c                                    |
 |                                                                                      |
 |                            MacsBug Utility Plugin Commands                           |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2001                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*

 This file contains all of "low level" plugins that may be needed by the commands written
 in gdb command language or other higher level plugins. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "MacsBug.h"

/*--------------------------------------------------------------------------------------*/

int branchTaken = 0;				/* !=0 if branch [not] taken in disasm	*/
char curr_function[1025] = {0};			/* current function being disassembled	*/

char *default_help = "For internal use only -- do not use.";

/*--------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------*
 | __asciidump addr n - dump n bytes starting at addr in ASCII |
 *-------------------------------------------------------------*/
 
void __asciidump(char *arg, int from_tty)
{
    int  	  argc, i, k, n, offset, repeated, repcnt;
    unsigned long addr;
    char 	  *argv[5], tmpCmdLine[1024];
    unsigned char *c, x, data[65], prev[64], addrexp[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__asciidump", &argc, argv, 4);
    if (argc != 3)
    	gdb_error("__asciidump called with wrong number of arguments");
    	
    n = gdb_get_int(argv[2]);
    
    for (repcnt = offset = 0; offset < n; offset += 64) {
    	sprintf(addrexp, "(char *)(%s)+%ld", argv[1], offset);
	
    	if (offset + 64 < n) {
    	    addr     = gdb_read_memory(data, addrexp, 64);
    	    repeated = ditto && (memcmp(data, prev, 64) == 0);
    	} else {
    	    addr     = gdb_read_memory(data, addrexp, n - offset);
    	    repeated = 0;
    	}
    	
    	memcpy(prev, data, 64);

    	if (repeated) {
    	    if (repcnt++ == 0)
    	    	gdb_printf(" %.8X: ''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''\n", addr);
    	} else {
    	    repcnt = 0;
    	    gdb_printf(" %.8lX: ", addr);
    	                
            for (k = i = 0; i < 64; ++i) {
            	if (offset + k < n) {
            	    x = data[k];
            	    data[k++] = isprint((int)x) ? (unsigned char)x : (unsigned char)'.';
            	}
            }
            
	    data[k] = '\0';
	    gdb_printf("%s\n", data);
    	}
    }
}

#define __ASCIIDUMP_HELP \
"__ASCIIDUMP addr n -- Dump n bytes starting at addr in ASCII."


/*-----------------------------------------------------------------*
 | __binary value n - display right-most n bits of value in binary |
 *-----------------------------------------------------------------*/ 
 
void __binary(char *arg, int from_tty)
{
    int  	  argc, n, i;
    unsigned long value;
    char 	  *argv[5], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__binary", &argc, argv, 4);
    
    if (argc != 3)
    	gdb_error("__binary called with wrong number of arguments");
    	
    value = gdb_get_int(argv[1]);
    n     = gdb_get_int(argv[2]);
    
    i = 0;
    while (i++ < n)
    	gdb_printf("%1d", (value >> (n-i)) & 1);
}

#define __BINARY_HELP \
"__BINARY value n -- Display right-most n bits of value in binary.\n" \
"No newline is output after the display."


/*-----------------------------------------------------------------------------*
 | __command_exists cmd - return 1 if the specified cmd exists and 0 otherwise |
 *-----------------------------------------------------------------------------*/
 
static void __command_exists(char *arg, int from_tty)
{
    gdb_set_int("$exists", gdb_is_command_defined(arg));
}

#define __COMMAND_EXISTS_HELP \
"__COMMAND_EXISTS cmd -- See if specified cmd exists.\n"				\
"Set $exists to 1 if the command exists and 0 otherwise."


/*-----------------------------------------------------------------------------------*
 | branch_taken - determine if instr. at PC is conditional branch whether it's taken |
 *-----------------------------------------------------------------------------------*
 
 If the specified instruction is a conditional branch the function returns 1 if the
 branch will be taken and -1 if it won't.  0 is returned if the instruction is not
 a conditional branch.
 
 The algorithm here is based on what is described in the IBM PPC Architecture book.
 To paraphrase what's there in determining whether the branch is taken or not it
 basically says:

   bc[l][a]  (primary = 16)
   bclr[l]   (primary = 19, extend = 16)
     cond_ok = BO[0] || (CR[BI] == BO[1])
     if !BO[2] then CTR = CTR-1
     ctr_ok = BO[2] || (CTR != 0) ^ BO[3]
     if ctr_ok && cond_ok then "will branch"
     else "will not branch"

   bcctr[l]  (primary = 19, extend = 528)
     cond_ok = BO[0] || (CR[BI] == BO[1])
     if cond_ok then "will branch"
     else "will not branch"

 where the notation X[i] is bit i in field X.

 The implementation of this below is "optimized" to read as:
     cond_ok = BO[0] || (CR[BI] == BO[1])
     if (cond_ok && !"bcctr[l]") then cond_ok = BO[2] || (CTR-1 != 0) ^ BO[3]
     if cond_ok then "will branch"
     else "will not branch"
     
 Note, for all these brances, of BO == 0x14 we have a "branch always" which are not
 classed as conditional branches for our purposes.
*/

static int branch_taken(unsigned long instruction)
{
    int pri, ext, cond_ok;
        
    pri = (instruction >> 26);			/* get primary opcode			*/
    if (pri == 19) {				/* bclr[l] or bcctr[l] ?		*/
    	ext = (instruction >> 1) & 0x3FFUL;	/* ...get extension			*/
    	if (ext != 16 && ext != 528)		/* ...not bclr[l] or bcctr[l]		*/
    	    return (0);
    } else if (pri == 16)			/* bc[l][a]				*/
    	ext = 0;
    else					/* not bc[l][a], bclr[l], or bcctr[l]	*/
    	return (0);
    
    if (((instruction >> 21) & 0x14) == 0x14)	/* ignore "branch always" branches	*/
    	return (0);
    	
    /* At this point we know we have a bc[l][a], bclr[l], or bcctr[l].   All three of	*/
    /* these do one common test: cond_ok = BO[0] || (CR[BI] == BO[1])			*/

    cond_ok = (instruction & 0x02000000); 	/* BO[0]				*/
    if (!cond_ok) {				/* !BO[0], try CR[BI] == BO[1]		*/
    	unsigned long cr = gdb_get_int("$cr");
	cond_ok = (((cr >> (31-((instruction>>16) & 0x1F))) & 1) == (((instruction & 0x01000000) >> 24) & 1));
    }
    
    /* bc[l][a] and bclr[l] also need to check to ctr...				*/
     
    if (cond_ok && ext != 528) {		/* cond_ok = BO[2] || (CTR-1!=0)^BO[3]	*/
    	cond_ok = (instruction & 0x00800000);	/* BO[2]				*/
    	if (!cond_ok) {				/* !BO[2], try (CTR-1 != 0) ^ BO[3]	*/
    	    unsigned long ctr = gdb_get_int("$ctr");
    	    cond_ok = (ctr-1 != 0) ^ ((instruction & 0x00400000) >> 22);
    	}
    }
    
    return (cond_ok ? 1 : -1);			/* return +1 if branch taken		*/
}


/*------------------------------------------------------*
 | format_disasm_line - format the gdb disassembly line |
 *------------------------------------------------------*
 
 This is the redirected stream output filter function for disassembly lines.  The src
 line is reformatted to the way we want it (see code) and the function returns the
 reformatted line as its result.  The line filtering machinery handles the actual
 printing.
 
 The data passed to this function is a communication link between here and __disasm()
 which specified this function as the output filter.  The data is actually actually 
 a DisasmData struct as defined in MacsBug.h.
*/

char *format_disasm_line(FILE *f, char *src, void *data)
{
    DisasmData    *disasm_info = (DisasmData *)data;
    char 	  c, c2, *p1, *p2, *src0 = src;
    int	 	  n, n_addr, n_offset, n_opcode, n_operand, brack, paren, angle, len, wrap;
    unsigned long instruction;
    char 	  address[11], function[1024], offset[8], opcode[12], operand[1025],
    		  verify[20];
    
    static char   formatted[3000];
  
    if (!src)					/* null means flush, no meaning here	*/
    	return (NULL);

    /* A disassembly line from gdb is formatted as one of the following:		*/
    
    /* 0xaaaa <function[+dddd][ at file:line]>:    opcode   operand			*/
    /* 0xaaaa <function[+dddd][ in file]>:         opcode   operand			*/
    
    /* Where 0xaaaa is the address and dddd is the offset in the specified function. 	*/
    /* There offset is suppressed if +dddd is zero.					*/
    /* If SET print symbol-filename is on the the "in" or "at" info is present showing	*/
    /* file and line.									*/
    /* The function name could be a Objective C message and thus enclosed in brackets 	*/
    /* which may contain embedded spaces (e.g., "[msg p1]").				*/
    /* The function may also be an unmangled C++ name, possibly a template instance 	*/
    /* which means we have to be careful about nested parens and angle brackets.	*/
    
    /* Extract the address...								*/
    
    n_addr = 0;
    while (*src && !isspace(*src) && *src != ':' && n_addr < 10)
	address[n_addr++] = *src++;
    address[n_addr] = '\0';
    
    /* As an additional check verify that the address is the one we are expecting...	*/
    
    sprintf(verify, "0x%lx", disasm_info->addr);
    if (strcmp(verify, address) != 0) {
    	gdb_fprintf(disasm_info->stream, COLOR_RED "### Reformat error extracting address (raw source line follows)...\n"
    	                                "%s" COLOR_OFF "\n", src0);
	return (NULL);
    }
    
    /* To extract the "function+dddd" information we make no assumptions about what	*/
    /* characters are in the function name other than the sequence ">:".  We look for	*/
    /* the ">:" first and work backwords towards the '+' sign.  Everything before is 	*/
    /* taken as the stuff containing the function name and everything after is the 	*/
    /* stuff that contains the offset.  Both then need additional processing to 	*/
    /* extract the proper information out of the additional "noise".			*/
    
    while (*src && isspace(*src))		/* skip white space up to '<'		*/
    	++src;
    
    function[0]          = '\0';
    offset[n_offset = 0] = '\0';
    
    if (*src && *src == '<') {			/* if '<' extract function and offset...*/
    	p1 = ++src;				/* ...search for ">:" delimiter		*/
	while (*src && !(*src == '>' && *(src+1) == ':'))
	    ++src;
	
	if (*src == '>') {			/* ...if ">:" found, work backwards...	*/
	    p2 = src - 1;			/* ...search left for '+'		*/
	    while (p2 > p1 && *p2 != '+')
		--p2;
	    n = p2 - p1;			/* ...length of offset (if any)		*/
	    	    
	    if (n > 0) {			/* ...extract function and offset...	*/
 	        if (n > 1023) n = 1023;
 	        memcpy(function, p1, n);	/* ...save function name		*/
 	        function[n] = '\0';
 	    	
		p1 = p2;			/* ...find out where offset ends	*/
 	        while (*p1 && *p1 != '>' && *p1 != ' ')
		    ++p1;			/*    "+dddd[ in/out...]>"		*/
 	        n_offset = p1 - p2;		/* ...save offset			*/
 	        if (n_offset > 7) n_offset = 7;
 	        memcpy(offset, p2, n_offset);
 	        offset[n_offset] = '\0';
	    } else {				/* ...if there wasn't any offset...	*/
		brack = paren = angle = 0;	/* ...find out where funct name ends	*/
 	        p1 = p2;
 	        while ((c = *p1++) != '\0')
 	            if (c == ' ') {
 	            	if (paren == 0 && brack == 0 && angle == 0 &&
 	            	    (strncmp(p1, "in ", 3) == 0 || strncmp(p1, "at ", 3) == 0)) {
 	            	    --p1;
 	            	    break;
 	            	}
 	            } else if (c == '<') {
 	            	if (paren == 0 && brack == 0)
 	            	    ++angle;
 	            } else if (c == '>') {
 	            	if (paren == 0 && brack == 0 && --angle <= 0 && *p1 == ':') {
 	            	    --p1;
 	            	    break;
 	            	}
 	            } else if (c == '[') {
 	            	if (paren == 0)
 	            	    ++brack;
 	            } else if (c == ']') {
 	            	if (paren == 0 && --brack <= 0 && *p1 == '>')
 	            	    break;
 	            } else if (c == '(') {
 	            	if (brack == 0)
 	            	    ++paren;
 	            } else if (c == ')') {
 	            	if (brack == 0 && --paren <= 0 && *p1 == '>')
 	            	    break;
 	            }
		n = p1 - p2;
 	        if (n > 1023) n = 1023;
 	        memcpy(function, p2, n);	/* ...save function name		*/
 	        function[n] = '\0';
 	        
 	        strcpy(offset, "+0");		/* ...fake the offset			*/
 	        n_offset = 2;
	    }
	    	    
	    ++src;				/* point at the required ':'		*/
	}
    }
    
    /* At this point we should be looking at the expected ':'.  If there we know the 	*/
    /* opcode and operand follow...							*/
    
    if (*src++ != ':') {
	gdb_internal_error("reformat error searching for ':'");
    	gdb_fprintf(disasm_info->stream, COLOR_RED "### Reformat error searching for ':' (raw source line follows)...\n"
    	                                "%s" COLOR_OFF "\n", src0);
	return (NULL);
    }
    
    /* Extract the opcode...								*/
    
    while (*src && isspace(*src))		/* skip white space after ':'		*/
    	++src;
    
    n_opcode = 0;
    while (*src && !isspace(*src))
    	if (n_opcode < 11)
    	    opcode[n_opcode++] = *src++;
    opcode[n_opcode] = '\0';
    
    /* Extract the operand...								*/
    
    while (*src && isspace(*src))		/* skip white space after opcode	*/
    	++src;
    
    if (*src) {					/* everything till end of line is opnd	*/
        p1 = src;					
        while (*++src) ;
        for (--src; src >= p1 && isspace(*src); --src); /* remove trailing white space	*/
        n_operand = ++src - p1;
        if (n_operand > 1023) n_operand = 1023;
        memcpy(operand, p1, n_operand);
    } else					/* no operand				*/
    	n_operand = 0;
    
    if (!(disasm_info->flags & NO_NEWLINE))	/* append new line?			*/
    	operand[n_operand++] = '\n';
    operand[n_operand]   = '\0';
    
    #if 0
    gdb_printf("address  = \"%s\"\n", address);
    gdb_printf("function = \"%s\"\n", function);
    gdb_printf("offset   = \"%s\"\n", offset);
    gdb_printf("opcode   = \"%s\"\n", opcode);
    gdb_printf("operand  = \"%s\"\n", operand);
    #endif
    
    /* Get instruction at the address.  We already have the address as a hex string.	*/
    
    gdb_read_memory(&instruction, address, 4);
    
    /* Format the disassembly line the way we want...					*/
    
    /*            1         2         3         4         5				*/
    /*  012345678901234567890123456789012345678901234567890				*/
    /*    *       *         *         **          *					*/
    /*   function									*/
    /*    +dddddd aaaaaaaa  xxxxxxxx   opcode     operand...				*/
    
    /* where, +dddddd is offset, aaaaaaaa is adddress, xxxxxxxx is instruction.		*/
    
    memset(formatted, ' ', 50);
        	  
    if (strcmp(function, curr_function) != 0 || (disasm_info->flags & ALWAYS_SHOW_NAME)) {
    	strcpy(curr_function, function);
    	if (disasm_info->flags & WRAP_TO_SIDEBAR)
    	    function[max_cols-12] = '\0';
    	if (0 && !macsbug_screen && isatty(STDOUT_FILENO))
    	    gdb_fprintf(disasm_info->stream, " " COLOR_BLUE "%s" COLOR_OFF "\n", function);
    	else
    	    gdb_fprintf(disasm_info->stream, " %s\n", function);
    }
    
    memcpy(formatted + 2 + (7-n_offset), offset, n_offset);	/* +dddddd		*/
    
    p1 = formatted + 10;					/* aaaaaaaa		*/
    memset(p1, '0', 8);
    memcpy(p1 + 10 - n_addr, address+2, n_addr - 2);
    
    p1 = formatted + 20;					/* xxxxxxxx		*/
    n = sprintf(p1, "%.8lx", instruction);
    *(p1 + n) = ' ';
    
    p1 = formatted + 31;					/* opcode		*/
    memcpy(p1, opcode, n_opcode);

    if (n_operand) {						/* operand		*/
    	strcpy(formatted + 42, operand);
    	if (disasm_info->max_width > 0 && 42+n_operand > disasm_info->max_width) {
	    if (disasm_info->flags & NO_NEWLINE)
		formatted[disasm_info->max_width] = '\0';
	    else {
		formatted[disasm_info->max_width]   = '\n';
		formatted[disasm_info->max_width+1] = '\0';
	    }
	}
    } else
    	*(p1 + n_opcode) = '\0';
    
    if (disasm_info->addr == disasm_info->pc) {			/* flag the pc...	*/
	if (disasm_info->flags & FLAG_PC)		 	/* ...if requested	*/
	    formatted[30] = '*';
	n = branch_taken(instruction);				/* cvt branch info	*/
	if (n == 1)
	    disasm_info->flags |= BRANCH_TAKEN;
	else if (n == -1)
	    disasm_info->flags |= BRANCH_NOT_TAKEN;
    }
    
    if (find_breakpoint(disasm_info->addr) >= 0)		/* flag breakpoints	*/
	formatted[29] = '.';
    
    /* Writing to the macsbug screen takes care of line wrapping for us.  But if we are	*/
    /* not writing to the macsbug screen and we are going to display the sidebar we 	*/
    /* need to handle the wrapping here ourselves.  Otherwise the sidebar would cut off	*/
    /* the right side of the line.							*/
    
    if (disasm_info->flags & WRAP_TO_SIDEBAR) {
    	get_screen_size(&max_rows, &max_cols);
    	if ((len = strlen(formatted) - 1) < max_cols-12)
    	    gdb_fprintf(disasm_info->stream, "%s", formatted);
    	else {
    	    wrap = max_cols - 13;
    	    p1 = formatted;
    	    while (len >= wrap) {
    	    	c  = *(p1+wrap);
    	    	c2 = *(p1+wrap+1);
    	    	*(p1+wrap)   = '\n';
    	    	*(p1+wrap+1) = '\0';
    	    	gdb_fputs(p1, disasm_info->stream);
    	    	len -= wrap;
    	    	p1 += wrap;
    	    	*p1 = c;
    	    	*(p1+1) = c2;
    	    }
    	    if (len > 0)
    	        gdb_fputs(p1, disasm_info->stream);
    	}
    } else
    	gdb_fprintf(disasm_info->stream, "%s", formatted);
    
    return (NULL);
}


/*--------------------------------------------------------------------------------------*
 | __disasm addr n [noPC] - disassemble n lines from addr (optionally suppress pc flag) |
 *--------------------------------------------------------------------------------------*
 
 If noPC is specified then suppress flagging the PC line.
*/
 
void __disasm(char *arg, int from_tty)
{
    int  	  argc, n, nopc = 0, show_sidebar;
    unsigned long addr, limit;
    DisasmData	  disasm_info;
    GDB_FILE	  *redirect_stdout, *prev_stdout;
    char 	  *argv[6], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__disasm", &argc, argv, 5);
    
    nopc = (argc == 4) ? (gdb_get_int(argv[3]) != 0) : 0;
    
    if (argc < 3)
    	gdb_error("usage: __disasm addr n [noPC] (wrong number of arguments)");
    	
    addr  = gdb_get_int(argv[1]);
    limit = addr + 4 * gdb_get_int(argv[2]);
   
    /* All disassembly output is filtered through format_disasm_line() so that we may	*/
    /* reformat the lines the way we want them...					*/
    
    redirect_stdout = gdb_open_output(stdout, format_disasm_line, &disasm_info);
    prev_stdout     = gdb_redirect_output(redirect_stdout);
    
    show_sidebar = (gdb_target_running() && !macsbug_screen && sidebar_state && isatty(STDOUT_FILENO));
    
    disasm_info.pc        = gdb_get_int("$pc");
    disasm_info.max_width = -1;
    disasm_info.flags	  = (nopc ? 0 : FLAG_PC) | (show_sidebar ? WRAP_TO_SIDEBAR : 0);
    disasm_info.stream    = prev_stdout;
    
    while (addr < limit) {
    	disasm_info.addr = addr;
    	gdb_execute_command("x/i 0x%lX", addr);
    	addr += 4;
    }
    
    gdb_close_output(redirect_stdout);
    
    /* The "branch taken" info is handled by display_pc_area() when the MacsBug screen	*/
    /* is on...										*/
    
    if (disasm_info.flags & BRANCH_TAKEN) {
    	gdb_set_int("$branch_taken", branchTaken = 1);
    	if (!macsbug_screen)
    	    gdb_printf("Will branch\n");
    } else if (disasm_info.flags & BRANCH_NOT_TAKEN) {
    	gdb_set_int("$branch_taken", branchTaken = -1);
    	if (!macsbug_screen)
    	    gdb_printf("Will not branch\n");
    } else
    	gdb_set_int("$branch_taken", branchTaken = 0);
    
    if (show_sidebar)
    	__display_side_bar("right", 0);
}

#define __DISASM_HELP \
"__DISASM addr n [noPC] -- Disassemble n lines from addr (suppress pc flag).\n"	\
"The default is to always flag the PC line with a '*' unless noPC is specified\n"	\
"and is non-zero.  If the PC line is a conditional branch then an additional\n"		\
"line is output indicating whether the branch will be taken and $branch_taken\n"	\
"is set to 1 if the branch will br taken, -1 if it won't be taken.  In all\n"		\
"other cases $branch_taken is set to 0.\n"						\
"\n"											\
"As a byproduct of calling __DISASM the registers are displayed in a side bar\n"	\
"on the right side of the screen IF the MacsBug screen is NOT being used.\n"		\
"\n"											\
"Enabled breakpoints are always flagged with a '.'.\n"					\
"\n"											\
"Note that this command cannot be used unless the target program is currently\n"	\
"running.  Use __is_running to determine if the program is running."


/*----------------------------------------*
 | __error msg - display an error message |
 *----------------------------------------*
 
 The message is reported as an error and the command terminated.
*/

static void __error(char *arg, int from_tty)
{
    gdb_error("%s", arg);
}

#define __ERROR_HELP "__ERROR string -- Abort command with an error message."


/*-------------------------------------------------------*
 | __getenv name - access the named environment variable |
 *-------------------------------------------------------*
 
 Sets convenience variable "$name" with the string represening the environment variable
 or a null string if undefined.
*/

static void __getenv(char *arg, int from_tty)
{
    int  numeric;
    long lvalue;
    char *p, *value, name[1024];
    
    if (!arg || !*arg)
    	gdb_error("__getenv expects a environemnt variable name");
    
    name[0] = '$';
    strcpy(&name[1], arg);
	
    value = getenv(arg);
    
    numeric = (value && *value);
    if (numeric) {
    	lvalue = strtol(value, &p, 0);
	numeric = (p > value);
	if (numeric) {
	    while (*p && *p == ' ' || *p == '\t')
	    	++p;
	    numeric = (*p == '\0');
	}
    }
    
    if (numeric)
    	gdb_set_int(name, lvalue);
    else if (!gdb_target_running())
    	gdb_set_int(name, (value != NULL));
    else
	gdb_set_string(name, value ? value : "");
    
    gdb_set_int("$__undefenv__", value == NULL);
}

#define __GETENV_HELP \
"__GETENV name -- Get the value of an environment variable.\n"				\
"Sets $name with value of environment variable \"name\".  If the\n"			\
"environment variable does not exist $name is set to a null string\n"			\
"and $__undefenv__ set to 1.\n"								\
"\n"											\
"If the environment variable is numeric then the type of $name is\n"			\
"an integer that contains the environment variable's value.  If it\n"			\
"isn't numeric, $name is the environment variable's string value.\n"			\
"\n"											\
"Note, due to the way gdb stores string convenience variables, $name\n"			\
"CANNOT be a string unless the target program is running!  Thus when\n"			\
"the target is NOT running, and the environment variable value is\n"			\
"not numeric, $name is set to 1 if the variable exists and 0 if it\n"			\
"doesn't exist.  $__undefenv__ is still set accordingly."


/*------------------------------------------------------------------------------*
 | __hexdump [addr [n]] dump n (default 16) bytes starting at addr (default pc) |
 *------------------------------------------------------------------------------*/
 
void __hexdump(char *arg, int from_tty)
{
    int  	  argc, i, j, k, n, offset, repeated, repcnt;
    unsigned long addr;
    char 	  *start, *argv[5], tmpCmdLine[1024];
    unsigned char *c, x, data[16], prev[16], charline[17], addrexp[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__hexdump", &argc, argv, 4);
    
    if (argc == 1) {
    	start = "$pc";
    	n     = 16;
    } else if (argc == 2) {
    	start = argv[1];
    	n     = 16;
    } else if (argc == 3) {
    	start = argv[1];
    	n     = gdb_get_int(argv[2]);
    } else
    	gdb_error("__hexdump called with wrong number of arguments.");
    
    for (repcnt = offset = 0; offset < n; offset += 16) {
    	sprintf(addrexp, "(char *)(%s)+%ld", start, offset);
	
    	if (offset + 16 < n) {
    	    addr     = gdb_read_memory(data, addrexp, 16);
    	    repeated = ditto && (memcmp(data, prev, 16) == 0);
    	} else {
    	    addr     = gdb_read_memory(data, addrexp, n - offset);
    	    repeated = 0;
    	}
    	
    	memcpy(prev, data, 16);

    	if (repeated) {
    	    if (repcnt++ == 0)
    	    	gdb_printf(" %.8lX: '''' '''' '''' ''''  '''' '''' '''' ''''  ''''''''''''''''\n", addr);
    	} else {
    	    repcnt = 0;
    	    gdb_printf(" %.8lX: ", addr);
    	                
            for (k = i = 0, c = charline; i < 8; ++i) {
            	for (j = 0; j < 2; ++j) {
            	    if (offset + k < n) {
            	    	x = data[k++];
            	    	gdb_printf("%1X%1X", (x>>4) & 0x0F, x & 0x0F);
            	    	*c++ = isprint((int)x) ? (unsigned char)x : (unsigned char)'.';
            	    } else
            	    	gdb_printf("  ");
            	}
            	gdb_printf(" ");
            	if (i == 3)
            	    gdb_printf(" ");
            }
            
	    *c = '\0';
	    gdb_printf(" %s\n", charline);
    	}
    }
}

#define __HEXDUMP_HELP \
"__HEXDUMP [addr [n]] -- Dump n (default 16) bytes starting at addr (default pc)."


/*-------------------------------------------------------------------*
 | __is_running [msg] - test to see if target is currently being run |
 *-------------------------------------------------------------------*
 
 If the target is being run set $__running__ to 1.  Otherwise set $__running__ to 0.
 If the message is omitted just return so that the caller can test $__running__.  If a
 message is specified report it as an error and abort the command.
*/
  
void __is_running(char *arg, int from_tty)
{
    int running = gdb_target_running();
    
    gdb_set_int("$__running__", running);
    
    if (!running && arg && *arg)
    	 gdb_error("%s", arg);
}

#define __IS_RUNNING_HELP \
"__IS_RUNNING [msg] -- Test to see if target is currently being run.\n"			\
"Sets $__running__ to 1 if the target is running and 0 otherwise.\n"			\
"\n"											\
"If a msg is supplied and the target is not running then the command\n"			\
"is aborted with the error message just as if __error msg was issued."


/*--------------------------------------------------*
 | __is_string arg - determine if arg is a "string" |
 *--------------------------------------------------*
 
 Returns $string set to the a pointer to the string if arg is a string and $string
 is set to 0 if the arg is not a string.
*/

static void __is_string(char *arg, int from_tty)
{
    char *p, str[1024];
    
    if (gdb_is_string(arg)) {
    	gdb_get_string(arg, str, 1023);
    	p = (char *)gdb_malloc(strlen(str) + 1);
    	strcpy(p, str);
    	gdb_set_string("$string", p);
    } else
    	gdb_set_int("$string", 0);
}

#define __IS_STRING_HELP \
"__IS_STRING arg -- Tries to determine if arg is a double quoted \"string\".\n"		\
"Sets $string to 1 if the arg is a string and 0 otherwise."


/*--------------------------------------------------*
 | __lower_case word - convert a word to lower case |
 *--------------------------------------------------*

 Returns $lower_case as a string containing the lower cased copy of the key word. Also
 assumes we're currently executing or string operations cause errors.
*/

static void __lower_case(char *arg, int from_tty)
{
    char c, *p, *w, str[1024];
    
    if (*arg != '"') {
    	sprintf(str, "$lower_case = (char *)\"%s\"", arg);
    	gdb_eval(str);
    	p = arg = gdb_get_string("$lower_case", str, 1023);
    } else
    	gdb_set_string("$lower_case", arg);
	
    p = arg = gdb_get_string("$lower_case", str, 1023);
    
    while ((c = *p) != 0)
	*p++ = tolower(c);
     
    w = (char *)gdb_malloc(strlen(arg) + 1);
    strcpy(w, arg);
    gdb_set_string("$lower_case", w);
}

#define __LOWER_CASE_HELP \
"__LOWER_CASE word -- Convert a word or string to lower case.\n"			\
"Sets $lower_case to the lower case copy of the word."


/*-----------------------------------------------------*
 | __memcpy addr src n - copy n bytes from src to addr |
 *-----------------------------------------------------*
 
 The addr is always an address but the src can either be a double quoted "string" or
 a expression representing an integer value.  For moving integer values n must be 1,
 2, or 4.  For strings n can be any length.
*/

static void __memcpy(char *arg, int from_tty)
{
    int  argc, n, v;
    char *argv[6], *src, str[1024], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__memcpy", &argc, argv, 5);
    
    if (argc != 4)
    	gdb_error("__memcpy called with wrong number of arguments");
    
    n = gdb_get_int(argv[3]);
    
    if (gdb_is_string(argv[2]))
    	src = gdb_get_string(argv[2], str, 1023);
    else {
    	if (n != 1 && n != 2 && n != 4)
    	    gdb_error("__memcpy of a int value with size not 1, 2, or 4.");
    	v = gdb_get_int(argv[2]);
    	src = (char *)&v + (4 - n);
    }
        
    gdb_write_memory(argv[1], src, gdb_get_int(argv[3]));
}

#define __MEMCPY_HELP \
"__MEMCPY addr src n -- Copy n bytes from src to addr."


/*-----------------------------------------------------------------*
 | printf_stdout_filter - stdout output filter for __printf_stderr |
 *-----------------------------------------------------------------*
 
 The __printf_stderr command uses this filter to redirect stdout output from a printf
 command to stderr.
*/

static char *printf_stdout_filter(FILE *f, char *line, void *data)
{
    if (line)
    	gdb_fprintf(*(GDB_FILE **)data, "%s", line);
	
    return (NULL);
}


/*--------------------------------------------------------*
 | __printf_stderr fmt,args... - printf command to stderr |
 *--------------------------------------------------------*
 
 The standard printf gdb command always goes to stdout.  The __printf_stderr is identical
 except that the output goes to stderr.
*/

static void __printf_stderr(char *arg, int from_tty)
{
    GDB_FILE *prev_stderr;
    GDB_FILE *redirect_stdout = gdb_open_output(stdout, printf_stdout_filter, &prev_stderr);
    GDB_FILE *redirect_stderr = gdb_open_output(stderr, printf_stdout_filter, &prev_stderr);
    
    prev_stderr = gdb_redirect_output(redirect_stderr);
    gdb_redirect_output(redirect_stdout);
    
    gdb_printf_command(arg, from_tty);
    
    gdb_close_output(redirect_stdout);
    gdb_close_output(redirect_stderr);
}

#define __PRINTF_STDERR_HELP \
"__PRINTF_STDERR \"gdb printf args\" -- printf to stderr.\n" \
"Gdb's PRINTF always goes to stdout.  __PRINTF_STDERR is identical except that the\n"	   \
"output goes to stderr."


/*-----------------------------------------------*
 | filter_char - convert a character to a string |
 *-----------------------------------------------*
 
 Internal routine to convert a character, c, to a string.  If the character needs to be
 escaped it is returned as the string "\c".  If the character is outside the range 0x20
 to 0x7F then it is returned as a pointer to a static string formatted as an escaped
 octal ("\ooo") with xterm controls surrounding it to display it in bold.
 
 Note that if isString is non-zero then the character is in the context of a double-
 quoted "string".  In that case a double quote is escaped.  Conversely, if isString is
 0 then we assume we're in the context of a single quoted 'string'.  Then single quotes
 are escaped.
*/

char *filter_char(int c, int isString, char *buffer)
{
    c &= 0xFF;
    
    switch (c) {
    	case '\n': return ("\\n");
	case '\r': return ("\\r");
	case '\t': return ("\\t");
	case '\a': return ("\\a");
	case '\f': return ("\\f");
	case '\b': return ("\\b");
	case '\v': return ("\\v");
	case '\'': return (isString ? "'" : "\\'");
	case '"' : return (isString ? "\\\"" : "\"");
	default  : if (c < 0x20 || c >= 0x7F)
			sprintf(buffer, COLOR_BOLD "\\%03o" COLOR_OFF, c);
		   else {
		   	buffer[0] = c;
		   	buffer[1] = 0;
		   }
		   return (buffer);
    }
}


/*-----------------------------------------------------------*
 | __print_char c [s] - print a (possibly escaped) character |
 *-----------------------------------------------------------*

 If s (actually any 2nd argument) is specified then this character is in the context
 of a "string" as opposed to a 'string' (i.e., the surrounding quotes on the string)
 so that a single quote does not need to be escaped.  On the other hand if this is the
 context of a double-quoted string then double quotes need to be escaped.
*/

static void __print_char(char *arg, int from_tty)
{
    int  argc;
    char *argv[5], buf[20], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__print_char", &argc, argv, 4);
    if (argc != 2 && argc != 3)
    	gdb_error("__print_charx called with wrong number of arguments");
        
    gdb_printf("%s", filter_char(gdb_get_int(argv[1]), argc == 3, buf));
}

#define __PRINT_CHAR_HELP \
"__print_char c [s] -- Print a (possibly escaped) character.\n"				\
"Just echos the c to stdout unless it's to be represented as a escaped\n"		\
"character (e.g., \\n) or octal (bolded) if not one of the standard escapes,\n"		\
"or quotes.\n"										\
"\n"											\
"If s (actually any 2nd argument) is specified then the character is in\n"		\
"the context of a \"string\" as opposed to a 'string' (i.e., the surrounding\n"		\
"quotes on the string) so that a single quote does not need to be escaped.\n"		\
"On the other hand if this is the  context of a double-quoted string then\n"		\
"double quotes need to be escaped."


/*-----------------------------------------*
 | __print_1 x - print one character ('x') |
 *-----------------------------------------*/
 
void __print_1(char *arg, int from_tty)
{
    char buf[20];
    
    gdb_printf("'%s'", filter_char(gdb_get_int(arg), 0, buf));
}

#define __PRINT_1_HELP \
"__PRINT_1 x -- Print 1 character surrounded with single quotes ('x').\n"		\
"Special characters are shown escaped or bold octal."


/*-----------------------------------------*
 | __print_2 - print two characters ('xy') |
 *-----------------------------------------*/

void __print_2(char *arg, int from_tty)
{   
    char buf1[20], buf2[20];
    unsigned long x2 = gdb_get_int(arg);

    gdb_printf("'%s%s'", filter_char(x2 >> 8, 0, buf1),
    			 filter_char(x2     , 0, buf2));
}

#define __PRINT_2_HELP \
"__PRINT_2 xy -- Print 1 characters surrounded with single quotes ('xy').\n"		\
"Special characters are shown escaped or bold octal."


/*--------------------------------------------*
 | __print_4 - print four characters ('wxyz') |
 *--------------------------------------------*/

void __print_4(char *arg, int from_tty)
{
   char buf1[20], buf2[20], buf3[20], buf4[20];
   unsigned long x4 = gdb_get_int(arg);
   
   gdb_printf("'%s%s%s%s'", filter_char(x4 >> 24, 0, buf1),
 			    filter_char(x4 >> 16, 0, buf2),
 			    filter_char(x4 >>  8, 0, buf3),
 			    filter_char(x4      , 0, buf4));
}

#define __PRINT_4_HELP \
"__PRINT_4 wxyz -- Print 4 characters surrounded with single quotes ('wxyz').\n"		\
"Special characters are shown escaped or bold octal."


/*---------------------------------------------------------------------------*
 | __reset_current_function - reset the currently function for disassemblies |
 *---------------------------------------------------------------------------*/
 
void __reset_current_function(char *arg, int from_tty)
{
    curr_function[0] = '\0';
}

#define __RESET_CURRENT_FUNCTION_HELP \
"__RESET_CURRENT_FUNCTION -- Make __DISASM forget which function it's in\n"		\
"The __DISASM will not display the function name unless it changes since the\n"		\
"since last time it disassembled a line.  The __reset_current_function forces it\n"	\
"to forget so that the next __DISASM disassembly line will be preceded by\n"		\
"the function name to which it belongs."


/*-------------------------------------------------------------------------------*
 | __setenv name [value] - set environment variable name to value (or delete it) |
 *-------------------------------------------------------------------------------*/

static void __setenv(char *arg, int from_tty)
{
    int  argc;
    char *argv[5], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__setenv", &argc, argv, 4);
    
    if (argc == 3)
    	(void)setenv(argv[1], argv[2], 1);
    else if (argc == 2)
    	unsetenv(argv[1]);
    else
    	gdb_error("__setenv called with wrong number of arguments");
}

#define __SETENV_HELP \
"__SETENV name [value] -- Set environment variable name to value (or delete it).\n"	\
"Note, setting environment variables from gdb has NO EFFECT on your shell's\n"		\
"environment variables other than locally changing their values while gdb is\n"		\
"loaded."


/*----------------------------------------------------------*
 | __strcmp str1 str2 - compare string compare for equality |
 *----------------------------------------------------------*
 
 Sets $strcmp to 1 if str1 == str2 and 0 if str1 != str2.
*/

static void __strcmp(char *arg, int from_tty)
{
    int  argc;
    char *argv[5], str1[1024], str2[1024], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__strcmp", &argc, argv, 4);
    
    if (argc != 3)
    	gdb_error("__strcmp called with wrong number of arguments");
    
    gdb_get_string(argv[1], str1, 1023);
    gdb_get_string(argv[2], str2, 1023);
    	
    gdb_set_int("$strcmp", strcmp(str1, str2) == 0);
}

#define __STRCMP_HELP \
"__STRCMP str1 str2 -- Compare string compare for equality.\n"				\
"Sets $strcmp to 1 if they are equal and 0 if not equal."				\


/*--------------------------------------------------------------------*
 | __strcmpl str1 str2 - case-independent string compare for equality |
 *--------------------------------------------------------------------*
 
 Sets $strcmpl to 1 if str1 == str2 and 0 if str1 != str2.
*/

static void __strcmpl(char *arg, int from_tty)
{
    int  argc, i;
    char *argv[5], str1[1024], str2[1024], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__strcmpl", &argc, argv, 4);
        
    if (argc != 3)
    	gdb_error("__strcmpl called with wrong number of arguments");
    	
    gdb_get_string(argv[1], str1, 1023);
    gdb_get_string(argv[2], str2, 1023);
        
    gdb_set_int("$strcmpl", gdb_strcmpl(str1, str2) == 1);
}

#define __STRCMPL_HELP \
"__STRCMPL str1 str2 -- Compare string compare for equality (ignoring case).\n"		\
"Sets $strcmpl to 1 if they are equal and 0 if not equal."


/*----------------------------------------------------------------------------*
 | __strncmp str1 str2 n - compare string compare for equality (n characters) |
 *----------------------------------------------------------------------------*
 
 Sets $strncmp to 1 if str1 == str2 and 0 if str1 != str2.
*/

static void __strncmp(char *arg, int from_tty)
{
    int  argc, n;
    char *argv[6], str1[1024], str2[1024], tmpCmdLine[1024];
    
    gdb_setup_argv(safe_strcpy(tmpCmdLine, arg), "__strncmp", &argc, argv, 5);
    
    if (argc != 4)
    	gdb_error("__strncmp called with wrong number of arguments");
    
    gdb_get_string(argv[1], str1, 1023);
    gdb_get_string(argv[2], str2, 1023);
    n = gdb_get_int(argv[3]);
    
    gdb_set_int("$strncmp", strncmp(str1, str2, n) == 0);
}

#define __STRNCMP_HELP \
"__STRNCMP str1 str2 n -- Compare string compare for equality (n characters).\n"	\
"Sets $strncmp to 1 if they are equal and 0 if not equal."


/*------------------------------------------------*
 | __strlen str - return length of str in $strlen |
 *------------------------------------------------*/

static void __strlen(char *arg, int from_tty)
{
    char *p, str[1024];
    
    p = gdb_get_string(arg, str, 1023);
    gdb_set_int("$strlen", strlen(p));
}

#define __STRLEN_HELP \
"__STRLEN string -- Get the length of the string.\n" \
"Sets $strlen with the length of the string."


/*-------------------------------------------------------------------------------------*
 | __window_size - set $window_rows and $window_cols to the current window screen size |
 *-------------------------------------------------------------------------------------*/
 
void __window_size(char *arg, int from_tty)
{
    get_screen_size(&max_rows, &max_cols);
    
    gdb_set_int("$window_rows", max_rows);
    gdb_set_int("$window_cols", max_cols);
}

#define __WINDOW_SIZE_HELP \
"__WINDOW_SIZE -- Get the current window screen size.\n" \
"Sets $window_rows and $window_cols to current window size."

/*--------------------------------------------------------------------------------------*/

#include "MacsBug_testing.i"

/*--------------------------------------------------------------------------------------*/

/*-----------------------------------------------------*
 | init_macsbug_utils - initialize the utility plugins |
 *-----------------------------------------------------*/

void init_macsbug_utils(void)
{
    MACSBUG_USEFUL_COMMAND(__asciidump,      	     __ASCIIDUMP_HELP);
    MACSBUG_USEFUL_COMMAND(__binary,		     __BINARY_HELP);
    MACSBUG_USEFUL_COMMAND(__command_exists, 	     __COMMAND_EXISTS_HELP);
    MACSBUG_USEFUL_COMMAND(__disasm,		     __DISASM_HELP);
    MACSBUG_USEFUL_COMMAND(__error,		     __ERROR_HELP);
    MACSBUG_USEFUL_COMMAND(__getenv,	     	     __GETENV_HELP);
    MACSBUG_USEFUL_COMMAND(__hexdump,		     __HEXDUMP_HELP);
    MACSBUG_USEFUL_COMMAND(__is_running,     	     __IS_RUNNING_HELP);
    MACSBUG_USEFUL_COMMAND(__is_string,      	     __IS_STRING_HELP);
    MACSBUG_USEFUL_COMMAND(__lower_case,     	     __LOWER_CASE_HELP);
    MACSBUG_USEFUL_COMMAND(__memcpy,		     __MEMCPY_HELP);
    MACSBUG_USEFUL_COMMAND(__printf_stderr,  	     __PRINTF_STDERR_HELP);
    MACSBUG_USEFUL_COMMAND(__print_char,     	     __PRINT_CHAR_HELP);
    MACSBUG_USEFUL_COMMAND(__print_1,		     __PRINT_1_HELP);
    MACSBUG_USEFUL_COMMAND(__print_2,		     __PRINT_2_HELP);
    MACSBUG_USEFUL_COMMAND(__print_4,		     __PRINT_4_HELP);
    MACSBUG_USEFUL_COMMAND(__reset_current_function, __RESET_CURRENT_FUNCTION_HELP);
    MACSBUG_USEFUL_COMMAND(__setenv, 	     	     __SETENV_HELP);
    MACSBUG_USEFUL_COMMAND(__strcmp, 	     	     __STRCMP_HELP);
    MACSBUG_USEFUL_COMMAND(__strcmpl,		     __STRCMPL_HELP);
    MACSBUG_USEFUL_COMMAND(__strncmp,		     __STRNCMP_HELP);
    MACSBUG_USEFUL_COMMAND(__strlen,		     __STRLEN_HELP);
    MACSBUG_USEFUL_COMMAND(__window_size,    	     __WINDOW_SIZE_HELP);
   
    add_testing_commands(); 		/* for trying out new things in testing.c	*/
}
