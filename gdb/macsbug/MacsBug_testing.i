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
}

static void testc(char *arg, int from_tty)
{
}

static void testw(char *arg, int from_tty)
{
}

static void testx(char *arg, int from_tty)
{
    gdb_testx(arg, from_tty);
}

static void testy(char *arg, int from_tty)
{
    gdb_testy(arg, from_tty);
}

static void testz(char *arg, int from_tty)
{
    gdb_testz(arg, from_tty);
}

/*--------------------------------------------------------------------------------------*/

static void add_testing_commands(void)
{
    #if 0
    MACSBUG_TESTING_COMMAND(testa, "");
    MACSBUG_TESTING_COMMAND(testb, "");
    MACSBUG_TESTING_COMMAND(testc, "");
    MACSBUG_TESTING_COMMAND(testw, "");
    MACSBUG_TESTING_COMMAND(testx, "");
    MACSBUG_TESTING_COMMAND(testy, "");
    MACSBUG_TESTING_COMMAND(testz, "");
    #endif
}

