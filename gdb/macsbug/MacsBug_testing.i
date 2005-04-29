/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                  MacsBug_testing.i                                   |
 |                                                                                      |
 |            Separate place to "bread board", test, and expirement with gdb            |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2001                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*

 This file is just for "fooling around".  It is included as part of MacsBug_display.c
 and uses its #include setup.  For testing possible gdb.c routines use it's testing.c
 file.
*/

/*--------------------------------------------------------------------------------------*/

static void prehook(char *arg, int from_tty)
{
    gdb_printf("pre-hook plugin\n");
}

static void posthook(char *arg, int from_tty)
{
    gdb_printf("post-hook plugin\n");
}

static void testa(char *arg, int from_tty)
{
    GDB_HOOK *pre, *post;
    
    pre  = gdb_replace_command_hook("testpre",  prehook,  "prehook plugin help", 0);
    post = gdb_replace_command_hook("testpost", posthook, "posthook plugin help", 0);
    
    gdb_execute_hook(pre);
    gdb_execute_hook(post);
    
    gdb_remove_command_hook(pre);
    gdb_remove_command_hook(pre);
}

static void testb(char *arg, int from_tty)
{
    char *p, *s = arg;
    
    union {
    	unsigned long r;
    	float         f;
    	union {
    	    char          c[16];
    	    short         s[8];
    	    unsigned long l[4];
    	    float         f[4];
    	    double	  d[2];
    	} v;
    	int           other;
    } value;
    
    p = gdb_get_register(arg, &value);
    
    if (!p) {
    	gdb_printf("%d\n", value.other);
    	return;
    }
    
    if (*s == '$')
	++s;
    
    switch (*s) {
	case 'r': /* r0-r31 */
	    gdb_printf("%s = %.8lX\n", arg, value.r);
	    break;
	    
	case 'f': /* f0-f31, fpscr */
	    if (strcmp(arg, "fpscr") == 0)
	    	gdb_printf("fpscr\n");
	    else
	    	gdb_printf("%s = %lg\n", arg, value.v.d[0]);
	    break;
	    
	case 'v': /* v0-v31, vscr, vrsave */
	    if (strcmp(arg, "vrsave") == 0)
	    	gdb_printf("vrsave\n");
	    else if (strcmp(arg, "vscr") == 0)
	    	gdb_printf("vscr\n");
	    else
	    	gdb_printf("%s = (%.8lX,%.8lX,%.8lX,%.8lX)\n", arg,
	    		    value.v.l[0],value.v.l[1],value.v.l[2], value.v.l[3]);
	    break;
	    
	case 'p': /* pc, ps */
	    if (strcmp(arg, "pc") == 0)
	    	gdb_printf("pc = %.8lX\n", value.r);
	    else
	    	gdb_printf("ps\n");
	    break;
	    
	case 'c': /* cr, ctr */
	    if (strcmp(arg, "cr") == 0)
	    	gdb_printf("cr = %.8lX\n", value.r);
	    else
	    	gdb_printf("ctr = %.8lX\n", value.r);
	    break;
	    
	case 'l': /* lr */
	    gdb_printf("lr = %.8lX\n", value.r);
	    break;
	    
	case 'x': /* xer */
	    gdb_printf("xer = %.8lX\n", value.r);
	    break;
	    
	case 'm': /* mq */
	    gdb_printf("mq = %.8lX\n", value.r);
	    break;
	    
	default:
	    gdb_printf("other = %d\n", value.other);
    } 
}

static void testc(char *arg, int from_tty)
{
    char *p;
    int value = -2;
    float v[4] = {1,2,3,4};
    
    if (*arg && arg[1] == 'v')
    	p = gdb_set_register(arg, &v, 16);
    else
    	p = gdb_set_register(arg, &value, 4);
    
    if (p)
    	gdb_printf("%s\n", p);
}

static void testd(char *arg, int from_tty)
{
    gdb_set_int(arg, 1);
}

static void testw(char *arg, int from_tty)
{
    gdb_testw(arg, from_tty);
}

static void testx(char *arg, int from_tty)
{
    gdb_testx(arg, from_tty);
}

static void testy(char *arg, int from_tty)
{
    gdb_testy(arg, from_tty);
}
#define testy_help 	\
"line 1.\n" 		\
"\n" 			\
"   line 3\n" 		\
"\n" 			\
"line 5."

static void testz(char *arg, int from_tty)
{
    gdb_testz(arg, from_tty);
}

extern void save_breakpoints_command(char *, int);

#if 0
static void savebp(char *arg, int from_tty)
{
    save_breakpoints_commandX(arg, from_tty);
}
#endif

/*--------------------------------------------------------------------------------------*/

static void add_testing_commands(void)
{
    #if 0
    MACSBUG_TESTING_COMMAND(testa, "");
    MACSBUG_TESTING_COMMAND(testb, "");
    MACSBUG_TESTING_COMMAND(testc, "");
    MACSBUG_TESTING_COMMAND(testd, "");
    MACSBUG_TESTING_COMMAND(testw, "");
    MACSBUG_TESTING_COMMAND(testx, "");
    MACSBUG_TESTING_COMMAND(testy, "");
    MACSBUG_TESTING_COMMAND(testz, "");
    #endif
    //MACSBUG_TESTING_COMMAND(testy, testy_help);
    //MACSBUG_TESTING_COMMAND(savebp, "");
    //gdb_enable_filename_completion("savebp");
}

