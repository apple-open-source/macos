/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                    gdb_testing.i                                     |
 |                                                                                      |
 |            Separate place to "bread board", test, and expirement with gdb            |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2001                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*

 This file is just for "fooling around".  It is included as part of gdb.c and uses its
 #include setup.  As such it can talk directly to gdb since the includes and -I's for
 gdb.c allow using its frameworks. 
*/

/*--------------------------------------------------------------------------------------*/

static char *(*saved_cmd_line_input_hook)(char *, int, char *) = NULL;
static void (*saved_window_hook)(FILE *, char *) = NULL;
extern void (*window_hook) PARAMS ((FILE *, char *));

/*--------------------------------------------------------------------------------------*/

static char rawline[1024];
static int cp = 0;

static void (*saved_rl_getc_function)();
static int first_testx = 1;

static int my_getc(FILE *stream)
{
    int i, c = ((int (*)(FILE*))saved_rl_getc_function)(stream);
    rawline[cp++] = c;
    
    //fprintf(stderr, "%.2x\n", c);
    
    if (c == '\n' || c == '\r') {
    	rawline[cp] = 0;
	fprintf(stderr, "\n");
    	for (i = 0; i < cp; ++i)
	    fprintf(stderr, "%.2x ", rawline[i] & 0xFF);
	fprintf(stderr, " ==> ");
    	for (i = 0; i < cp; ++i) {
	    c = rawline[i] & 0xFF;
	    fprintf(stderr, "%c", (c < ' ') ? '¥' : c);
	}
	fprintf(stderr, "\n");
	cp = 0;
    }
    
    return (c);
}

void gdb_testx(char *arg, int from_tty)
{
    if (first_testx) {
    	first_testx  = 0;
    	saved_rl_getc_function = rl_getc_function;
    	rl_getc_function = (void (*)())my_getc;
    }
    cp = 0;
}

/*--------------------------------------------------------------------------------------*/

void gdb_testy(char *arg, int from_tty)
{
}

/*--------------------------------------------------------------------------------------*/

static Gdb_Cmd_Class myClass = Gdb_Private;

void a_cmd(char *arg, int from_tty) 
{
    gdb_printf("a_cmd(%s)\n", a_cmd ? a_cmd : "<null>");
}

void gdb_testa(char *arg, int from_tty)
{
    struct cmd_list_element *c;
    char   alias[1024];
    
    if (arg && *arg) {
    	gdb_define_plugin(arg, a_cmd, myClass, "cmd help.");
	
	if (strlen(arg) > 1) {
	    strcpy(alias, arg);
	    alias[0]= 'X';
	    gdb_define_plugin_alias(arg, alias);
	}
    }
}

/*--------------------------------------------------------------------------------------*/

void gdb_testw(char *arg, int from_tty)
{
    if (arg && *arg)
    	myClass = gdb_define_class(arg, "category title");
    
    //Xgdb_define_plugin("Ê", a_cmd, myClass, "");
}

/*--------------------------------------------------------------------------------------*/

#include "gdbarch.h"
#include "inferior.h"

void gdb_testz(char *arg, int from_tty)
{
    #if 0
    int regnum;
    //int numregs = ARCH_NUM_REGS;
    char virtual_buffer[MAX_REGISTER_VIRTUAL_SIZE];
    char raw_buffer[MAX_REGISTER_RAW_SIZE];
    
    if (!arg || !*arg)
    	return;
	
    regnum = target_map_name_to_register (arg, 2);
    if (regnum < 0)
	gdb_error("undefined");
	
    if (REGISTER_NAME(regnum) == NULL || *(REGISTER_NAME(regnum)) == '\0')
	gdb_error("undefined for machine");
    
    if (read_relative_register_raw_bytes(regnum, raw_buffer))
	gdb_error("value not available");

    if (REGISTER_CONVERTIBLE(regnum)) {
	REGISTER_CONVERT_TO_VIRTUAL(regnum, REGISTER_VIRTUAL_TYPE(regnum),
					raw_buffer, virtual_buffer);
    } else
	memcpy(virtual_buffer, raw_buffer, REGISTER_VIRTUAL_SIZE(regnum));
    #endif
}

/*--------------------------------------------------------------------------------------*/
