/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                  MacsBug_testing.i                                   |
 |                                                                                      |
 |            Separate place to "bread board", test, and expirement with gdb            |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2006                       |
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

extern int string_for_ptr(GDB_ADDRESS addr, char *theString, int maxLen,
			  int not_guessable_obj);

static void teste(char *arg, int from_tty)
{
    char *p, env, result[1025], tmpString[1025];
    GDB_ADDRESS addr;
    
    addr = gdb_get_address(arg);
    p = (char *)gdb_teste(addr);
    
    #if 0 && defined(not_ready_for_prime_time_but_keep_enabled_for_private_testing)
    if (string_for_ptr(addr, tmpString, 500, 0)) {
	if (*p)
	    strcat(p, " ");
	strcpy(p + strlen(p), tmpString);
    }
    
    if (*p)
    	gdb_printf("%s\n", p);
    #endif
}

static void testw(char *arg, int from_tty)
{
    gdb_testw(arg);
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

#define TEST_SET 0
#if TEST_SET
static int int_value = 0;
static int *bool_value = 0;
static char *str_value;
static char *str_noesc_value;
static char *fname;
static char *enum_value;
static char *enum_settings[] = {"red", "green", "blue", NULL};

static init_str(char **p, char *value)
{
    *p = strcpy((char *)gdb_malloc(strlen(value)+1), value);
}

static void set_test_int(char *theSetting, Gdb_Set_Type type, void *value, int show, int confirm)
{
    gdb_printf("My int set/show function: %s = %d (show = %d, confirm = %d)\n",
    		theSetting, *(int*)value, show, confirm);
}

static void set_test_str(char *theSetting, Gdb_Set_Type type, void *value, int show, int confirm)
{
    gdb_printf("My string set/show function: %s = \"%s\" (show = %d, confirm = %d)\n",
    		theSetting, *(char**)value, show, confirm);
}

/* ------------- */

static void test_bool(char *arg, int from_tty)
{
    gdb_define_set("test-bool", set_test_int, Set_Boolean, &bool_value, 1, "Set test-bool");
}

static void test_int(char *arg, int from_tty)
{
    gdb_define_set("test-int", set_test_int, Set_Int, &int_value, 1, "Set test-int");
}

static void test_str(char *arg, int from_tty)
{
    gdb_define_set("test-str", set_test_str, Set_String, &str_value, 1, "Set test-str");
}

static void test_str_noesc(char *arg, int from_tty)
{
    gdb_define_set("test-str-noesc", set_test_str, Set_String_NoEsc, &str_noesc_value, 1, "Set test-str-noesc");
}

static void test_fname(char *arg, int from_tty)
{
    gdb_define_set("test-fname", set_test_str, Set_Filename, &fname, 1, "Set test-fname");
}

static void test_enum(char *arg, int from_tty)
{
    gdb_define_set_enum("test-enum", set_test_str, enum_settings, &enum_value, 1, "Set test-enum {red | greeen | blue}");
}

static void test_only_set(char *arg, int from_tty)
{
    gdb_define_set("test-only-set", set_test_int, Set_Int, &int_value, 0, "Set test-int (only)");
}                                                                /*    ^     */

#endif /* TEST_SET */

/*--------------------------------------------------------------------------------------*/

static void add_testing_commands(void)
{
    #if 0
    MACSBUG_TESTING_COMMAND(testa, "");
    MACSBUG_TESTING_COMMAND(testb, "");
    MACSBUG_TESTING_COMMAND(testc, "");
    MACSBUG_TESTING_COMMAND(testd, "");
    MACSBUG_TESTING_COMMAND(teste, "");
    MACSBUG_TESTING_COMMAND(testw, "");
    MACSBUG_TESTING_COMMAND(testx, "");
    MACSBUG_TESTING_COMMAND(testy, "");
    MACSBUG_TESTING_COMMAND(testz, "");
    #endif
    //MACSBUG_TESTING_COMMAND(testy, testy_help);
    //MACSBUG_TESTING_COMMAND(savebp, "");
    //gdb_enable_filename_completion("savebp");
    
    #if TEST_SET
    MACSBUG_TESTING_COMMAND(test_bool, "");
    MACSBUG_TESTING_COMMAND(test_int, "");
    MACSBUG_TESTING_COMMAND(test_str, "");
    MACSBUG_TESTING_COMMAND(test_str_noesc, "");
    MACSBUG_TESTING_COMMAND(test_fname, "");
    MACSBUG_TESTING_COMMAND(test_enum, "");
    MACSBUG_TESTING_COMMAND(test_only_set, "");
    
    init_str(&str_value, "init str_value");
    init_str(&str_noesc_value, "init str__noesc_value");
    init_str(&fname, "");
    init_str(&enum_value, "red");
    
    gdb_execute_command("test_bool");
    gdb_execute_command("test_int");
    gdb_execute_command("test_str");
    gdb_execute_command("test_str_noesc");
    gdb_execute_command("test_fname");
    gdb_execute_command("test_enum");
    gdb_execute_command("test_only_set");
    #endif /* TEST_SET */
    #undef TEST_SET
}

