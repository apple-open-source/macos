/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                        gdb.c                                         |
 |                                                                                      |
 |                           Gdb Support Routines for Plugins                           |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2006                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*
 
 This file contains all the plugin interface routines (except for I/O redirection and 
 SET handling).  The routines in here are basically "in bed" with gdb and take advantage
 of the knowledge of how gdb internally does things for the routines to hid the details
 from plugin callers.
*/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <termios.h>

#include "gdb_private_interfaces.h"

#include "target.h"
#include "value.h"
#include "command.h"
#include "top.h"        // execute_command, get_prompt, instream, line, word brk completer
#include "gdbtypes.h"   // enum type_code, builtin_type_void_data_ptr
#include "gdbcmd.h"     // cmdlist, execute_user_command, filename_completer
#include "completer.h"  // cmdlist, execute_user_command, filename_completer
/* FIXME: We should not have to rely on these details, but there aren't
   interfaces for all the cmd_list_element bits we need here yet. */
#include "cli/cli-decode.h"
#include "expression.h" // parse_expression
#include "inferior.h"   // stop_bpstat, read_sp
#include "symtab.h"     // struct symtab_and_line, find_pc_line, find_pc_sect_function
#include "frame.h"      // selected_frame
#include "gdbarch.h"    // gdbarch_tdep
#include "regcache.h"   // register_size
#include "ppc-tdep.h"   // struct gdbarch_tdep (wordsize)
#include "gdbcore.h"    // memory_error
#include "objfiles.h"   // find_pc_section, struct objfile, struct obj_section
#include "bfd.h"        // bfd_get_filename, bfd_section_name
#include "symfile.h"    // overlay_debugging and related functions
#include "block.h"      // BLOCK_START
#include "exceptions.h" // catch_errors
#include "demangle.h"	// for cplus_demangle
#include "target.h"
#include "defs.h"

#define CLASS_BASE 100

static int unique_class = 0;
static Gdb_Plugin gdb_help_command = NULL;

typedef struct Class_user {		      /* reclassed class_user cmd list entry:	*/	
    struct Class_user 	    *next;	      /*    next list entry			*/
    struct cmd_list_element *c;		      /*    command's gdb info			*/
    Gdb_Cmd_Class 	    class;	      /*    new class for the cmd's HELP	*/
} Class_user;

static Class_user *class_user_classes = NULL; /* list of reclassed class_user cmds 	*/

/*--------------------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------*
 | map_class_to_gdb - map our command class values to gdb command class values |
 *-----------------------------------------------------------------------------*
 
 This is an internal routine to convert our class enum values back to gdb's enum
 command_class values.  If it doesn't map to one of those, it's one of ours defined
 by gdb_define_class() and so we use it as is.  If it's not one of ours we simply map
 it to no_class.
*/

static enum command_class map_class_to_gdb(Gdb_Cmd_Class theClass)
{
    switch (theClass) {
    	case Gdb_Breakpoints: return (class_breakpoint); 	/* breakpoints		*/
    	case Gdb_Data:	      return (class_vars);		/* data			*/
    	case Gdb_Files:	      return (class_files);		/* files		*/
    	case Gdb_Internals:   return (class_maintenance);	/* internals		*/
    	case Gdb_Obscure:     return (class_obscure);		/* obscure		*/
    	case Gdb_Running:     return (class_run);	    	/* running		*/
    	case Gdb_Stack:	      return (class_stack);		/* stack		*/
    	case Gdb_Support:     return (class_support);		/* support		*/
    	case Gdb_Private:     return (no_class);		/* internal commands	*/
	default: 
	    if ((int)theClass >= CLASS_BASE && (int)theClass < CLASS_BASE + unique_class)
	    	return ((enum command_class)theClass);
	    return (no_class);
    }
}


/*-----------------------------------------------------------------------*
 | map_gdb_to_class - mape gdb command class to our command class values |
 *-----------------------------------------------------------------------*
 
 This is the reverse of map_class_to_gdb() above.  A gdb class is mapped into one of our
 class value.  If it's not one of the supported classes Gdb_Private is returned.
*/

static Gdb_Cmd_Class map_gdb_to_class(enum command_class theClass)
{
    switch (theClass) {
	case class_breakpoint:	return (Gdb_Breakpoints);	/* breakpoints		*/
	case class_vars:	return (Gdb_Data);		/* data			*/
	case class_files:	return (Gdb_Files);		/* files		*/
	case class_maintenance:	return (Gdb_Internals);		/* internals		*/
	case class_obscure:	return (Gdb_Obscure);		/* obscure		*/
	case class_run:		return (Gdb_Running);		/* running		*/
	case class_stack:	return (Gdb_Stack);		/* stack		*/
	case class_support:	return (Gdb_Support);		/* support		*/
	case no_class:		return (Gdb_Private);		/* internal commands	*/
	default:		return ((Gdb_Cmd_Class)theClass);
	    if ((int)theClass >= CLASS_BASE && (int)theClass < CLASS_BASE + unique_class)
	    	return ((Gdb_Cmd_Class)theClass);
	    return (Gdb_Too_Many_Classes);
    }
}


/*----------------------------------------------------------------------------------*
 | restore_user_classes - restore user classes changed by intercept_help_commands() |
 *----------------------------------------------------------------------------------*

 This is a cleanup hook established by intercept_help_commands() to restore DEFINE
 commands that are classed as something other than class_user while displaying HELP.
 See intercept_help_commands() for further details.
 
 Note we do this as a cleanup routine since intercept_help_commands() intercepts ALL
 HELP commands.  If a HELP command reports an error we need to ensure we restore the
 class_user commands we reclassed for the help.
*/

static void restore_user_classes(void *unused)
{
    Class_user *p;
    
    for (p = class_user_classes; p; p = p->next)  /* ...restore the original class	*/
	p->c->class = class_user;
}


/*-------------------------------------------------------*
 | intercept_help_commands - intercept all HELP commands |
 *-------------------------------------------------------*
 
 Commands created with DEFINE are interpreted as opposed to plugins/built-ins being 
 called.  They are classed by gdb as class_user and it uses that class to determine if
 they are to be interpreted.  It's also used by SHOW USER to display these commands.
 Thus gdb_change_class() which can immediately change the class of plugins cannot do that
 for DEFINE commands.  So it builds the class_user_classes list to record which DEFINE
 commands need to be reclassed.  Since the purpose of the class is to affect the HELP
 display (other than additional items for class_user as just mentioned) we use the list
 to temporarily change the DEFINE class across the HELP command.  That's the reason
 for this intercept routine.
*/

static void intercept_help_commands(char *arg, int from_tty)
{
    Class_user *p;
    struct cleanup *old_chain = make_cleanup(restore_user_classes, NULL);
    
    for (p = class_user_classes; p; p = p->next)/* ...change the recorded cmds		*/
	p->c->class = map_class_to_gdb(p->class);			
    
    gdb_help_command(arg, from_tty);		/* ...HELP is none the wiser!		*/
    do_cleanups(old_chain);
}

/*--------------------------------------------------------------------------------------*/
			      /*--------------------------*
			       | Initialization and Setup |
			       *--------------------------*/

/*-----------------------------------------------------------------*
 | gdb_initialize - required forst call to the gdb support package |
 *-----------------------------------------------------------------*
 
 This must be called before any other of the interface routines to do some required
 initialization.  Returns 1 if the initialization succeeds, 0 if it fails.
 
 Note, hopefully someday in the future MacsBug will be extended to support Intel.
 Until then we need the return code.
*/

int gdb_initialize(void)
{
    static initialized = 0;
    
    if (!initialized) {
	if (gdbarch_bfd_arch_info(current_gdbarch)->arch != bfd_arch_powerpc) {
	    warning("The MacsBug plugin is currently only supported for PowerPC targets.\n");
            return (0);
        }
        
    	initialized = 1;
	
	/* If this is the first instance of the plugin support library then set 	*/
	/* gdb_global_data_p to point to an 0-initialized malloc'ed chunk of data used	*/
	/* to hold some key valued defined by gdb.  See comments about this in 		*/
	/* gdb_private_interfaces.h.							*/
	
	if (gdb_global_data_p == NULL) {
	    gdb_global_data_p = (Gdb_Global_Data *)gdb_malloc(sizeof(Gdb_Global_Data));
	    memset(gdb_global_data_p, 0, sizeof(Gdb_Global_Data));
	}

	__initialize_io();
	__initialize_set();
	
	//gdb_help_command = gdb_replace_command("help", intercept_help_commands);
	gdb_help_command = INITIAL_GDB_VALUE(help_command, 
				     	     gdb_replace_command("help", intercept_help_commands));
    }
    
    return (1);
}


/*----------------------------------------------------------------------------*
 | gdb_define_class - create a new class or change a plugincommand to a class |
 *----------------------------------------------------------------------------*
 
 Defines a new plugin command class that is NOT one of the predefined classes OR converts
 an existing plugin command into a class.  The function returns the new class value if
 the command did not previously exist or an existing plugin's class, now converted into
 a class.
 
 The classTitle should be a short description to display with the category.  Follow the
 style that gdb uses.  For example, when HELP shows the category for "data", it's help is
 simply "Examining data".
 
 Internally even classes are represented as commands (just ones that cannot be executed).
 That is why there is a relationship between plugin commands and classes.  The only real
 reason you might want to redefine an exiting (plugin) command as a class is when you
 want to use the command's name as a class category for the help display.  Then after
 the display you would convert it back to a plugin command again the way it was created
 in the first place, i.e., by calling gdb_define_cmd().  Of course you need to intercept
 the HELP command to do this.  The gdb_replace_command() call is used to define command
 intercepts so that is relatively easy.
 
 Note, there is a limit of 50 user defined classes (why would you ever define so many?).
 After that the function returns Gdb_Too_Many_Classes.
 
 Also note that the new class values returned are still typed as Gdb_Cmd_Class even
 though they are outside the enum range.
*/
 
Gdb_Cmd_Class gdb_define_class(char *className, char *classTitle)
{
    int    new_class;
    struct cmd_list_element *c;
    char   orig_cmd[1024], *s = orig_cmd;
    
    if (unique_class >= 50)			/* see if too many classes		*/
	return (Gdb_Too_Many_Classes);
    
    strcpy(orig_cmd, className);
    c = lookup_cmd(&s, cmdlist, "", -1, 0);	/* see if class already exists		*/
    
    /* New commands get their own uniqe class number.  Existing commands are redefined	*/
    /* as a class.  What we're doing here is faking out gdb's class machinery.  It only	*/
    /* views the classes as numbers anyhow for grouping and selecting purposes (well, 	*/
    /* except for class_user which indicates DEFINE, i.e., interpretive commands) so it	*/
    /* doesn't mind that we're using "illegal" (from it's internal enum point of view)	*/
    /* class values outside it's enum range (and for that matter, ours as well).	*/
    
    //new_class = c ? (int)c->class : (CLASS_BASE + unique_class++);
    new_class = (CLASS_BASE + unique_class++);
    //fprintf(stderr, "gdb_define_class(%s) --> %d\n", className, new_class);
    
    /* Classes are defined as commands without functions...		*/
    
    add_cmd(strcpy((char *)gdb_malloc(strlen(className) + 1), className), new_class,
    		   NULL, classTitle, &cmdlist);
	    
    return ((Gdb_Cmd_Class)(new_class));
}


/*-----------------------------------------------------------*
 | gdb_change_class - change the class of an exiting command |
 *-----------------------------------------------------------*
 
 Called to change to class of an exiting command.  Nothing happens if the command does
 not exist.
 
 Generally you never need this if you define your own plugins since you define the class
 when you define the plugin.  But if you have commands written in gdb command language
 with DEFINE then gdb classes those as "user-defined" (Gdb_Public).  For those you need
 to call this function to change their class to your desired class. Further you obviously
 must make sure that the plugins are loaded (via LOAD-PLUGIN) after the DEFINE commands,
 in the .gdbinit script (or one sourced during that time) so that the commands exist
 by the time this routine is called.
*/

void gdb_change_class(char *theCommand, Gdb_Cmd_Class newClass)
{
    struct cmd_list_element *c;
    Class_user 	    	    *p;
    char   		    orig_cmd[1024], *s = orig_cmd;
    
    strcpy(orig_cmd, theCommand);
    c = lookup_cmd(&s, cmdlist, "", -1, 0);
    if (!c)
    	return;
    
    /* Found command, change its class.  Commands created from DEFINE are classed as	*/
    /* class_user and treated specially since these are interpretive rather than calling*/
    /* a plugin or built-in command.  Gdb actually looks for the class class_user to	*/
    /* decide whether to interpret the command when it is class_user or to call it.  It	*/
    /* also uses class_user to allow the user to display a command's definition with 	*/
    /* SHOW USER.  Thus we cannot change the class now.  Rather we keep a list of all	*/
    /* class_user classes which are to be given a different class.  Since the purpose 	*/
    /* of the class is to place the command in a proper HELP class display then we use	*/
    /* the list to temporarily replace the class with the desired class only when doing	*/
    /* HELP.  To that end initialization has replaced the gdb HELP command with our own */
    /* (intercept_help_commands()) where we do the replacement before calling the real	*/
    /* gdb help.									*/
	
    if (c->class != class_user)	{		/* if not class_user...			*/
	c->class = map_class_to_gdb(newClass);	/* ...simply set the new class		*/
	return;
    }
	
    for (p = class_user_classes; p; p = p->next)/* see if cmd is already on our list	*/
	if (p->c == c) {			
	    p->class = map_class_to_gdb(newClass);/* if it is, just record latest class	*/
	    return;
	}
	
    p = gdb_malloc(sizeof(Class_user));		/* record a new class_user entry...	*/
    p->c     = c;
    p->class = newClass;
    p->next  = class_user_classes;
    
    class_user_classes = p;			/* intercept_all_commands() uses list	*/
}


/*-----------------------------------------------------------------------------------*
 | gdb_fixup_document_help - replace option-spaces in DOCUMENT help with real spaces |
 *-----------------------------------------------------------------------------------*
 
 Help info for gdb script commands is defined between a DOCUMENT and its matching END. It
 is a "feature" of gdb's handling of DOCUMENT help to strip all leading spaces from the
 help strings as they are read in by gdb.  It also removes all blank lines or lines which
 consist only of a newline.  This makes formatting such help information somewhat of a
 pain!  The work around it is to use a leading "significant space", i.e., option-space,
 which is 0xCA in Mac ASCII.  Then the help strings are displayed as desired.
 
 The problem with using option-spaces for a gdb script command's help is that if the help
 is displayed in any other context other than a xterm terminal (e.g., Project Builder) the
 display might (and for Project Builder, does) explicitly show the "non-printable"
 characters as something unique.  Thus gdb_fixup_document_help() is provided to replace
 all the option-spaces with actual spaces after the script commands are defined and their
 help read by gdb.
 
 Call gdb_fixup_document_help() for each script command whose DOCUMENT help was formatted
 using option-spaces.  If the command is defined and has DOCUMENT help it's options-spaces
 will be replaced with real spaces.  Undefined commands or commands with no help info are
 ignored.
*/
 
void gdb_fixup_document_help(char *theCommand)
{
    struct cmd_list_element *c;
    unsigned char 	    *p;
    
    if (!theCommand || !*theCommand)
    	return;
    
    c = lookup_cmd(&theCommand, cmdlist, "", 1, 0);
    if (!c || !c->doc)
	return;
		
    p = (unsigned char *)c->doc;
    while (*p)
    	if (*p == 0xCA)
    	    *p++ = ' ';
    	else
    	    ++p;
}


/*-------------------------------------------*
 | gdb_define_cmd - define a new gdb command |
 *-------------------------------------------*
 
 The command name (theCommand), its class category (itsClass), and it's help info
 (helpInfo) are used to (re)define a plugin command.  A pointer to the plugin's
 implementation is passed in the plugin function pointer.
 
 The command's class defines it's help class.  It any one of the predefined values from
 the Gdb_Cmd_Class enum or one generated by gdb_define_class().  Plugin commands can be
 redefined or they can be used to change a class into a command (see gdb_define_class()
 for more details).
 
 Note, the help info's first line is brief documentation; remaining lines form, with it,
 the full documentation.  The first line should end with a period.  The entire string
 should also end with a period, not a newline.
*/

void gdb_define_cmd(char *theCommand, Gdb_Plugin plugin, Gdb_Cmd_Class itsClass,
		    char *helpInfo)
{
    add_com(strcpy((char *)gdb_malloc(strlen(theCommand) + 1), theCommand),
    	    map_class_to_gdb(itsClass), plugin, helpInfo);
}


/*-----------------------------------------------------------------------------*
 | gdb_define_cmd_alias - define an alias (special abbreviation) for a command |
 *-----------------------------------------------------------------------------*
 
 Gdb allows special abbreviations for commands (e.g., gdb's "ni" is a alias for "nexti").
 By calling gdb_define_cmd_alias() you can define aliases for a command as well. It's
 intended for defining aliases for commands defined by gdb_define_cmd() but there's
 nothing to prevent defining aliases for existing gdb commands as well.  As the grammar
 here implies ("aliases") you can defined more than one alias for the same command.
 
 Note the command class for the alias is always made the same as the class of the command
 to which it's aliased.
*/

void gdb_define_cmd_alias(char *theCommand, char *itsAlias)
{
    struct cmd_list_element *c;
    char   orig_cmd[1024], *s = orig_cmd, *a;
    
    strcpy(orig_cmd, theCommand);
    c = lookup_cmd(&s, cmdlist, "", 1, 1);
    
    if (c)
    	add_com_alias(strcpy((char *)gdb_malloc(strlen(itsAlias) + 1), itsAlias),
			theCommand, class_alias, 0);
}


/*----------------------------------------------------------------------------*
 |  gdb_enable_filename_completion - enable filename completion for a command |
 *----------------------------------------------------------------------------*
 
 Allow filename completion for the specified command.  Generally the default is symbol
 completion for most commands (there are a few that have their own unique completion,
 e.g., ATTACH).  Defining a command with gdb_define_cmd() defaults to symbol completion.
 By calling gdb_enable_filename_completion() you are saying that the specified command
 has a filename argument and that filename completion should be used.
 
 Note, completion refers to the standard shell behavior of attempting to finish a
 word when a tab is entered or show all possible alternatives for that word.
*/

void gdb_enable_filename_completion(char *theCommand)
{
    struct cmd_list_element *c;
    char   orig_cmd[1024], *s = orig_cmd, *a;
    
    strcpy(orig_cmd, theCommand);
    c = lookup_cmd(&s, cmdlist, "", 1, 1);
    
    if (c)
  	set_cmd_completer(c, filename_completer);
}


/*----------------------------------------------------------------------*
 | gdb_replace_command - replace an exiting command/plugin with another |
 *----------------------------------------------------------------------*

 Replace an existing command specified by theCommand (string) with a new replacement
 command (new_command) if it is not NULL.  The pointer to the "old" command is returned
 as the function result (thus passing NULL for the new_command is a way to find the
 address of the specified command plugin).  NULL is returned and an error reported if
 theCommand does not exist.  This allows for the enhancement of existing commands with
 additional functionality or simply to get the address of an existing command.
*/

Gdb_Plugin gdb_replace_command(char *theCommand, Gdb_Plugin new_command)
{
    struct cmd_list_element *c;
    Gdb_Plugin		    cmd;
    char		    orig_cmd[1024], *s = orig_cmd;
    
    strcpy(orig_cmd, theCommand);
    c = lookup_cmd(&s, cmdlist, "", 1, 1);
    
    if (c) {
    	cmd = c->function.cfunc;
	if (new_command && new_command != c->function.cfunc)
    	    c->function.cfunc = new_command;
    } else
    	cmd = NULL;
	
    return (cmd);
}


/*-----------------------------------------------------*
 | gdb_replace_helpinfo - replace a commands help info |
 *-----------------------------------------------------*
 
 The newHelp string replaces theCommand's current help string.  The pointer to the now
 previous help string is returned.  If the newHelp is specified as NULL then only a
 pointer the command's help string is returned.  If NULL is returned then either the
 command didn't exist or didn't have any helpInfo to begin with.
       
 Note, you cannot assume the returned pointer is to malloc'ed space for prexisting gdb
 commands.  For commands you create with gdb_define_cmd() you know how the helpInfo is
 allocated and can take appropriate actions.
*/

char *gdb_replace_helpinfo(char *theCommand, char *newHelp)
{
    struct cmd_list_element *c;
    char                    *oldHelp, orig_cmd[1024], *s = orig_cmd;
    
    c = lookup_cmd(&s, cmdlist, "", 1, 1);
    if (c) {
    	oldHelp = c->doc;
    	c->doc  = newHelp;
    } else
    	oldHelp = NULL;
	
    return (oldHelp);
}


/*--------------------------------------------*
 | gdb_private_data - plugin private data use |
 *--------------------------------------------*
 
 It is possible that there could be multiple instances of the plugin support library and
 that cooperating instances might want to "talk" to each other.  Instance data is
 distinct so they have no direct way to pass data between them unless they can store it
 in gdb itself which is common to all the instances.  Gdb_private_data() is provided
 for this purpose.
 
 A plugin is identified by an (arbitrary) name and associates a block of global data
 with that name.  The pointer to the data is established in gdb by calling
 gdb_private_data() with the name.
 
 Each call to gdb_private_data() with the same name from any library instance returns
 the same pointer to a pointer uniquely associated with the name.  Initially the pointer
 value is NULL so the first instance to establish it should probably allocate (malloc'ed
 space only) the private data to be used among the other instances.
 
 Once the pointer to the data is known each instance can save it privately instead of
 recalling gdb_private_data() although that of course would also work.
 
 In addition to "private" data pointers there is one global data pointer that can be
 used among the library instances.  The pointer to this pointer is returned by 
 gdb_private_data() when a null name (string or pointer) is passed.
 
 It should be pointed out that multiple instances of the plugin support library are
 not totally independent of one another.   See Implementation Considerations at the end
 of gdb.h for further details.  
*/

void **gdb_private_data(char *plugin_name)
{
    Plugin_Pvt_Data *p;
    
    if (!plugin_name || !*plugin_name)
	return (&gdb_global_data_p->plugin_global_data);
	
    for (p = gdb_global_data_p->pvt_data_list; p; p = p->next)
    	if (strcmp(plugin_name, p->plugin_name))
	    return (&p->plugin_data);
    
    p = gdb_malloc(sizeof(Plugin_Pvt_Data) + strlen(plugin_name));
    p->plugin_data = NULL;
    strcpy(p->plugin_name, plugin_name);
    p->next = gdb_global_data_p->pvt_data_list;
    gdb_global_data_p->pvt_data_list = p;
    
    return (&p->plugin_data);
}

/*--------------------------------------------------------------------------------------*/
                      /*-----------------------------------------*
                       | Command Hook Handling (e.g., hook-stop) |
                       *-----------------------------------------*/

/*-----------------------------------------------------------------*
 | gdb_replace_command_hook - replace a command hook with a plugin |
 *-----------------------------------------------------------------*
 
 A command hook in the gdb command language is defined as follows:
 
    DEFINE hook-foo
    ...gdb commands that define the hook...
    END
    
 where foo is some other DEFINE command.
 
 When a hook is defined gdb will call the hook before the command itself.  In the above
 example hook-foo is called before foo itself.
 
 The gdb_replace_command_hook() function allows you to define a plugin in place of, using
 the above example, hook-foo.  Pass the original command name (e.g., "foo") for which the
 hook will be established.  Also pass any help info (hooks are like any other commands)
 and a pointer to the command's plugin implementation which has the same prototype as any
 other plugin.  The hookpost parameter should be passed as 0 (see below).
 
 Starting with GDB 5.x post-command hooks are also allowed, i.e., hooks which are called
 immediately after a command.  Using the above example,
 
    DEFINE hookpost-foo
    ...gdb commands that define the hook...
    END
    
 Defining one of these is the same as defining a pre-command hook except that hookpost 
 should be passed as non-zero.
  
 The function returns an anonymous pointer representing the hook data.  This pointer
 should be passed to gdb_execute_hook() to execute any preexisting hook and to 
 gdb_remove_command_hook() to remove the plugin as a hook.  See those routines for
 further details.
 
 NULL is returned if theCommand does not exist or hookpost was non-zero when using GDB
 4.x which doesn't support post-execute hooks.
 
 The anonymous pointer returned to the user is actually a pointer to the following data:
 											*/
 typedef struct {				/* Info saved to restore a previous hook*/
     int		     hookpost;		/*	1 ==> this is for a post hook	*/
     int		     replaced_hook;	/*	1 ==> cmd has a previous hook	*/
     struct cmd_list_element *c;		/* 	cmd that had a previous hook	*/
     struct cmd_list_element *hook;		/*	cmd had this as its hook	*/
     struct command_line     *user_commands;	/*	definition of previous hook	*/
     char 		     *doc;		/*	help strings of previous hook 	*/
     struct cmd_list_element *new_hookc;	/* 	replacement hook		*/
     struct cmd_list_element *__cmd_hook_N;	/*	cmd corresponding to user funct	*/
 } Old_hook_info;
 											/*
 This is used to remember key info about the new hook and the old hook (if one exists at
 the time of this call).  This allows gdb_remove_command_hook() to restore the old hook
 and for gdb_execute_hook() to execute it.
*/

GDB_HOOK *gdb_replace_command_hook(char *theCommand, Gdb_Plugin new_hook, char *helpInfo,
				   int hookpost)
{
    int			    i;
    struct cmd_list_element *c, *old_hookc, *new_hookc;
    Gdb_Plugin		    hook;
    struct command_line	    *cmd;
    Old_hook_info	    *old_hook_data;
    char		    *s, tmpstr1[1024], tmpstr2[1024];
    static int		    unique_name_cnt = 0;
    
    #if GDB4
    if (hookpost)				/* not supported in gdb 4.x		*/
    	return (NULL);
    #endif
    
    /* See if theCommand exists.  We can't create a hook to a nonexistant command.	*/
    
    s = strcpy(tmpstr1, theCommand);
    c = lookup_cmd(&s, cmdlist, "", -1, 0);
    	
    if (c && !gdb_strcmpl(theCommand, c->name))
	return (NULL);
    
    /* We will be defining a command hook which is the equivalent to the following user	*/
    /* definition:									*/
    
    /*	define hook[post]-<theCommand>							*/
    /*	   __<theCommand>-hook-N							*/
    /*  end										*/
    
    /* where <theCommand> is the original command name and N is a unique integer.  The	*/
    /* reason we cannot simply add a hook command into the command list like we do 	*/
    /* with a normal command is that hooks are treated specially by gdb.  They always	*/
    /* are interpreted, never called.  So we need the definition to act as glue to a	*/
    /* normal command which is what the user specified.					*/
    
    /* If there already is a user defined hook-<theCommand> then save enough info to	*/
    /* execute the old hook from our __<theCommand>-hook-N hook and to restore the old	*/
    /* hook when our hook is deleted by pdb_pop_hook().					*/
    
    #if GDB4
    s = strcpy(tmpstr1, strcat(strcpy(tmpstr2, "hook-"), theCommand));
    #else
    if (hookpost)
    	s = strcpy(tmpstr1, strcat(strcpy(tmpstr2, "hookpost-"), theCommand));
    else
    	s = strcpy(tmpstr1, strcat(strcpy(tmpstr2, "hook-"), theCommand));
    #endif
    old_hookc = lookup_cmd(&s, cmdlist, "", 1, 1);
    
    old_hook_data = (Old_hook_info *)xmalloc(sizeof(Old_hook_info));
    if (old_hookc) {
    	old_hook_data->replaced_hook = 1;
    	old_hook_data->c	     = c;		
    	old_hook_data->hook	     = old_hookc;
    	old_hook_data->user_commands = old_hookc->user_commands;
    	old_hook_data->doc	     = old_hookc->doc;
    	
    	old_hookc->doc = helpInfo;
    	new_hookc = old_hookc;
    } else {
    	old_hook_data->replaced_hook = 0;
    	old_hook_data->c	     = c;		
    	old_hook_data->hook	     = NULL;
    	old_hook_data->user_commands = NULL;
    	old_hook_data->doc	     = NULL;
    	
    	s = strcpy((char *)xmalloc(strlen(tmpstr2) + 1), tmpstr2);
    	new_hookc = add_cmd(s, class_user, new_hook, helpInfo, &cmdlist);
    }
    
    old_hook_data->new_hookc = new_hookc;
    old_hook_data->hookpost  = hookpost;
   
    #if GDB4
    c->hook           = new_hookc;		/* target gets hooked  			*/
    new_hookc->hookee = c;			/* we are marked as hooking target cmd	*/
    #else
    if (hookpost) {
    	c->hook_post           = new_hookc;	/* target gets hooked  			*/
    	new_hookc->hookee_post = c;		/* we are marked as hooking target cmd	*/
    } else {
    	c->hook_pre            = new_hookc;	/* target gets hooked  			*/
    	new_hookc->hookee_pre  = c;		/* we are marked as hooking target cmd	*/
    }
    #endif
    
    /* Create the hook-<theCommand> command...						*/
	
    new_hookc->user_commands = cmd = (struct command_line *)xmalloc(sizeof(struct command_line));
    cmd->next         = NULL;
    cmd->control_type = simple_control;
    cmd->body_count   = 0;
    cmd->body_list    = (struct command_line **)xmalloc(sizeof(struct command_line *));
    memset(cmd->body_list, 0, sizeof(struct command_line *));
	
    i = sprintf(tmpstr1, "__%s-hook-%d", theCommand, unique_name_cnt++) + 1;
    cmd->line = (char *)xmalloc(i);
    strcpy(cmd->line, tmpstr1);
    
    /* Define the command that hook-<theCommand> will call.  This is the user's routine	*/
    /* to be called.									*/
	
    sprintf(tmpstr2, "%s - internal hook.", tmpstr1);
    old_hook_data->__cmd_hook_N = add_cmd(strcpy((char *)xmalloc(i), cmd->line), no_class,
     					   new_hook, tmpstr2, &cmdlist);
    
    return ((GDB_HOOK *)old_hook_data);
}


/*--------------------------------------------------------------------------*
 | restore_hook - restore our hook if an error occurrred in the user's hook |
 *--------------------------------------------------------------------------*
 
 This is a cleanup hook established by gdb_execute_hook() to recover our hook when an
 error is reported from something in the user's hook.
 
 gdb_execute_hook() passes the data we need to do the recovery in the _data parameter
 which is actually a pointer to the following:
 											*/ 
 typedef struct {
     Old_hook_info	 *old_hook_data;	/* all the info about the user's hook	*/ 
     struct command_line *user_commands;	/* place to save our hook definition	*/
 } Hook_data;										/*
*/

static void restore_hook(void *_data)
{
    Hook_data *data = (Hook_data *)_data;
    
    data->old_hook_data->new_hookc->user_commands = data->user_commands;
}


/*----------------------------------------------------------*
 | gdb_execute_hook - execute a hook overridden by a plugin |
 *----------------------------------------------------------*
 
 The anonymous pointer returned from a gdb_replace_command_hook() may be passed here to
 execute the hook (if any) that was defined previously to the gdb_replace_command_hook()
 call.  Thus both a .gdbinit user defined hook and a plugin hook can be executed if the
 plugin calls this routine.  There is no way for the caller to know whether there is
 a preexisting hook.  This routine checks that and of course simply returns if there
 isn't one.
*/

void gdb_execute_hook(GDB_HOOK *hook)
{
    struct cleanup *old_chain;
    Hook_data	   data;
    
    if (hook && ((Old_hook_info *)hook)->replaced_hook) {
    	data.old_hook_data = (Old_hook_info *)hook;
    	
    	data.user_commands = data.old_hook_data->new_hookc->user_commands;
    	data.old_hook_data->hook->user_commands = data.old_hook_data->user_commands;
    	
    	old_chain = make_cleanup(restore_hook, &data);
    	execute_user_command(data.old_hook_data->hook, NULL);
    	do_cleanups(old_chain);
    	
    	data.old_hook_data->new_hookc->user_commands = data.user_commands;
    }
}


/*--------------------------------------------------------*
 | gdb_remove_command_hook - remove a command hook plugin |
 *--------------------------------------------------------*
 
 The anonymous pointer returned from a gdb_replace_command_hook() may be passed here to
 remove the hook previously created by gdb_replace_command_hook().  Any user defined
 hook that was in effect prior to the gdb_replace_command_hook() call is reestablished
 as the primary hook for its associated command.
*/
 
void gdb_remove_command_hook(GDB_HOOK *hook)
{
    Old_hook_info 	    *old_hook_data;
    char	   	    *s;
    struct cmd_list_element *c, *old_hookc;
      
    if (hook) {
    	old_hook_data = (Old_hook_info *)hook;
    	
    	if (!old_hook_data->replaced_hook) {
    	    s = old_hook_data->new_hookc->name;
    	    delete_cmd(s, &cmdlist);
    	    xfree(s);
    	}
    	
    	if (old_hook_data->new_hookc->user_commands)
    	    xfree(old_hook_data->new_hookc->user_commands);
    	
    	if (old_hook_data->new_hookc->user_commands->body_list)
    	    xfree(old_hook_data->new_hookc->user_commands->body_list);
    	
    	s = old_hook_data->__cmd_hook_N->name;
    	delete_cmd(s, &cmdlist);
    	xfree(s);
    	
    	if (old_hook_data->replaced_hook) {
    	    c         = old_hook_data->c;
    	    old_hookc = old_hook_data->hook;
    	    
	    #if GDB4
    	    c->hook           = old_hookc;
    	    old_hookc->hookee = c;
	    #else
	    if (old_hook_data->hookpost) {
    	    	c->hook_post           = old_hookc;
    	    	old_hookc->hookee_post = c;
	    } else {
    	    	c->hook_pre            = old_hookc;
    	    	old_hookc->hookee_pre  = c;
    	    }
	    #endif
	    
    	    old_hookc->user_commands = old_hook_data->user_commands;
    	    old_hookc->doc	     = old_hook_data->doc;
    	}
    	
    	xfree(old_hook_data);
    }
}

/*--------------------------------------------------------------------------------------*/
                             /*--------------------------*
                              | Gdb Exit (Quit) Handling |
                              *--------------------------*/

static Gdb_Exit_Handler users_exit_handler = NULL;
static void (*saved_state_change_hook)(Debugger_state new_state) = NULL;

/*------------------------------------------------------------*
 | my_state_change_hook - intercept the gdb state change hook |
 *------------------------------------------------------------*
 
 If a exit handler is installed with gdb_define_exit_handler() then my_state_change_hook()
 is called each time gdb uses its state_change_hook to signal a state change.  We're
 only interested in the STATE_NOT_ACTIVE state which is only issued when gdb is just about
 to exit back to the OS.  We let gdb process its original hook (if any) and then we
 call the user's exit routine that was specified to gdb_define_exit_handler().
*/

static void my_state_change_hook(Debugger_state new_state)
{
    if (new_state == STATE_NOT_ACTIVE && users_exit_handler)
    	users_exit_handler();
	
    if (saved_state_change_hook)
    	saved_state_change_hook(new_state);
}


/*------------------------------------------------------------------------*
 | gdb_define_exit_handler - define a user routine to call when gdb exits |
 *------------------------------------------------------------------------*
 
 This always you to specify a handling routine to get control just before gdb is about
 to exit.  The handler should have the following prototype:
 
   void handler(void);
 
 Note, that you can still gdb_replace_command() to replace the gdb QUIT command.  But
 that intercepts quit BEFORE that command executes allowing you to do anything special
 in place of the quit command.   On the other hand, specifying a exit handler here 
 will cause that exit handler to be called when gdb is truly about to quit.
*/
 
void gdb_define_exit_handler(Gdb_Exit_Handler theHandler)
{
    saved_state_change_hook = state_change_hook;
    state_change_hook       = my_state_change_hook;
    users_exit_handler      = theHandler;
}

/*--------------------------------------------------------------------------------------*/
			      /*---------------------------*
			       | Gdb Command Line Routines |
			       *---------------------------*/

/*-----------------------------------------------------------------------*
 | my_target_can_async_p - make gdb it's running a command synchronously |
 *-----------------------------------------------------------------------*
 
 Gdb has the ability to run commands either asynchronously or synchronously.  I think
 it depends on how its built.  Gdb has the following macro defined (in its target.h):
 
   #define target_can_async_p() (current_target.to_can_async_p ())
 
 It uses target_can_async_p() all over the place to determine what it has to do if the
 target program can be run asynchronously.  The current_target.to_can_async_p is a
 pointer to a function of the kind we have here.  It returns 0 for synchronous and 1
 for asynchronous mode.
 
 We always return 0 to make gdb think the commands we execute with gdb_execute_command()
 (who sets current_target.to_can_async_p to point here) are synchronous.  Why always
 synchronous?  Because commands that cause the target to execute (e.g., like NEXTI) 
 queue their output until just before the next command line command!  That breaks the
 paradigm we have established for redirected output and their associated filters.
 
 Consider a plugin that executes does gdb_execute_command("nexti") and also wants to
 redirect the NEXTI output through a filter.  If the NEXTI output is queued up until
 the plugin command is finished then the filter can never get a hold of the output.
 It's too late.  The plugin has long since closed its redirected stream.
 
 By executing the commands synchronously gdb doesn't queue the output.  It will be
 done by the time gdb_execute_command() returns.  And during that time the redirection
 filter got what it was after.
 
 This is a big comment for a function that only returns a 0!  But it is worth the
 explanation.  Because I won't remember why I did it in the future :-)
*/
 
static int my_target_can_async_p(void)
{
    return(0);
}


/* The latest version of gdb has a new routine, gdb_set_async_override(int), that	*/
/* enables (0) or disables (1) its async operation.  This supercedes the expliclt	*/
/* manipulation of current_target.to_can_async_p.  But for compatibility with older	*/
/* versions of gdb we control which scheme we use with the NEW_ASYNC_SCHEME macro.	*/

#ifndef NEW_ASYNC_SCHEME
#define NEW_ASYNC_SCHEME 1			/* 1 ==> use gdb_set_async_override()	*/
#endif


#if !NEW_ASYNC_SCHEME
/*------------------------------------------------------*
 | restore_async_state- restore gdb current async state |
 *------------------------------------------------------*
 
 This is used to restore current_target.to_can_async_p to what it was prior to executing
 gdb_eva and gdb_execute_command.  It is called via do_cleanups() either explicitly in
 those routines or through error recovery due so some error that might occur during those
 routines.
*/

static void restore_async_state(void *async_p)
{
    current_target.to_can_async_p = (int (*)(void))async_p;
}
#endif


/*------------------------------------*
 | gdb_eval - evaluate the expression |
 *------------------------------------*
 
 The string specified by the expression is parsed and evaluated by gdb.  This can be
 used for assignment expressions instead of using the full command parser with
 gdb_execute_command().  For example, gdb_eval("$a=2") as opposed to 
 gdb_execute_command("set $a=2").
 
 Note that the arguments to gdb_eval() are the same as for a sprintf() allowing you to
 format the expression with the call.
*/

void gdb_eval(char *expression, ...)
{
    struct cleanup *old_chain;
    va_list 	   ap;
    char    	   line[1024];
    
    if (expression) {
    	#if !NEW_ASYNC_SCHEME
    	old_chain = make_cleanup(restore_async_state, current_target.to_can_async_p);
	current_target.to_can_async_p = my_target_can_async_p;
	#else
	gdb_set_async_override(1);
	old_chain = make_cleanup(gdb_set_async_override, 0);
	#endif
	
	va_start(ap, expression);
	vsprintf(line, expression, ap);
	va_end(ap);
	
	parse_and_eval(line);
	
	do_cleanups(old_chain);
    }
}


/*-------------------------------------------------------------------------*
 | gdb_execute_command - execute a complete gdb command line synchronously |
 *-------------------------------------------------------------------------*
  
 A complete gdb command line is passed as if it were entered into gdb itself.  However
 the command is always executed synchronously even if gdb could run it asynchronously.
 See comments for my_target_can_async_p() above for further details.
 
 Note that the arguments to gdb_execute_command() are the same as for a sprintf()
 allowing you to format the command with the call.  Note, be careful how you specify
 printf formats if you need to execute a PRINTF command since the command line acts as a
 format string to gdb_execute_command().  In other words if commandLine has any
 %-formatting for the printf you'll need to double the  %'s to keep gdb_execute_command()
 from using them.
*/
 
void gdb_execute_command(char *commandLine, ...)
{
    struct cleanup *old_chain;
    va_list 	   ap;
    char    	   line[1024];
    
    if (commandLine) {
    	#if !NEW_ASYNC_SCHEME
    	old_chain = make_cleanup(restore_async_state, current_target.to_can_async_p);
	current_target.to_can_async_p = my_target_can_async_p;
	#else
	gdb_set_async_override(1);
	old_chain = make_cleanup(gdb_set_async_override, 0);
	#endif
	
	va_start(ap, commandLine);
	vsprintf(line, commandLine, ap);
	va_end(ap);
	
	execute_command(line, 0);
    
	do_cleanups(old_chain);
    }
}


/*----------------------------------------------------*
 | suppress_errors - detect that an error has occured |
 *----------------------------------------------------*
 
 This is a redirected stdout/stderr output filter set up by both gdb_eval_silent()
 and gdb_execute_command_silent() to detect that an error has occurred.  Those
 routines want to be able to continue after an error occurs and also return to their
 callers that an error did indeed occur (or not).  So error messages will come
 through here where we promptly drop them on the floor.  We do however send back a
 signal that we were called.
*/

static char *suppress_errors(FILE *f, char *line, void *data)
{
    if (line)
    	*(int *)data = 1;
	
    return (NULL);
}


/*-----------------------------------------------------------*
 | wrap_parse_and_eval - catch_errors parse_and_eval wrapper |
 *-----------------------------------------------------------*
 
 This is called by gdb's catch_errors() to execute a parse_and_eval().  The call is
 "wrapped" by catch_errors() which gdb_eval_silent() calls so that gdb_eval_silent()
 may get control back after the error.  Normally this wouldn't happen.  But the
 intent is to be able to continue even if an error is detected.
*/

static int wrap_parse_and_eval(char *expression)
{
    parse_and_eval(expression);
    bpstat_do_actions(&stop_bpstat);
    return (0);
}


/*---------------------------------------------------------------------------------*
 | gdb_eval_silent - evaluate the expression silently (i.e., do not report errors) |
 *---------------------------------------------------------------------------------*
 
 This is identical to gdb_eval() except that any errors due to evaluation are suppressed.
 The function returns 1 if there were suppressed errors and 0 if the evaluation
 succeeded.
*/

int gdb_eval_silent(char *expression, ...)
{
    va_list  ap;
    GDB_FILE *redirect_stderr, *redirect_stdout;
    int      had_error = 0;
    char     line[1024];
    #if !NEW_ASYNC_SCHEME
    int      (*saved_target_can_async_p)(void);
    #endif
    
    if (expression) {
    	#if !NEW_ASYNC_SCHEME
	saved_target_can_async_p      = current_target.to_can_async_p;
	current_target.to_can_async_p = my_target_can_async_p;
	#else
	gdb_set_async_override(1);
	#endif
	
	redirect_stdout = gdb_open_output(stdout, suppress_errors, &had_error);
	redirect_stderr = gdb_open_output(stderr, suppress_errors, &had_error);
	gdb_redirect_output(redirect_stderr);
       
	va_start(ap, expression);
	vsprintf(line, expression, ap);
	va_end(ap);
	
	catch_errors(wrap_parse_and_eval, line, NULL, RETURN_MASK_ALL);
	
	gdb_close_output(redirect_stderr);
	gdb_close_output(redirect_stdout);
	
    	#if !NEW_ASYNC_SCHEME
	current_target.to_can_async_p = saved_target_can_async_p;
	#else
	gdb_set_async_override(0);
	#endif
    }
    
    return (had_error);
}


/*---------------------------------------------------------------*
 | wrap_execute_command - catch_errors execute_command() wrapper |
 *---------------------------------------------------------------*
 
 This is called by gdb's catch_errors() to execute a execute_command().  The call is
 "wrapped" by catch_errors() which gdb_execute_command_silent() calls so that
 gdb_execute_command_silent() may get control back after the error.  Normally this
 wouldn't happen.  But the intent is to be able to continue even if an error is detected.
*/

static int wrap_execute_command(char *commandLine)
{
    execute_command(commandLine, 0);
    bpstat_do_actions(&stop_bpstat);
    return (0);
}


/*---------------------------------------------------------------------------------------*
 | gdb_execute_command_silent - execute a complete gdb command line silently (no errors) |
 *---------------------------------------------------------------------------------------*
 
 This is identical to gdb_execute_command() except that any errors due to execution of 
 the command are suppressed.   The function returns 1 if there were suppressed errors and
 0 if the execution succeeded.
*/

int gdb_execute_command_silent(char *commandLine, ...)
{
    va_list  ap;
    GDB_FILE *redirect_stderr, *redirect_stdout;
    int      had_error = 0;
    char     line[1024];
    #if !NEW_ASYNC_SCHEME
    int      (*saved_target_can_async_p)(void);
    #endif
    
    if (commandLine) {
    	#if !NEW_ASYNC_SCHEME
	saved_target_can_async_p      = current_target.to_can_async_p;
	current_target.to_can_async_p = my_target_can_async_p;
	#else
	gdb_set_async_override(1);
	#endif

	redirect_stdout = gdb_open_output(stdout, suppress_errors, &had_error);
	redirect_stderr = gdb_open_output(stderr, suppress_errors, &had_error);
	gdb_redirect_output(redirect_stderr);
	
	va_start(ap, commandLine);
	vsprintf(line, commandLine, ap);
	va_end(ap);
	
	catch_errors((catch_errors_ftype *)wrap_execute_command, line, NULL, RETURN_MASK_ALL);
	
	gdb_close_output(redirect_stderr);
	gdb_close_output(redirect_stdout);
	
    	#if !NEW_ASYNC_SCHEME
	current_target.to_can_async_p = saved_target_can_async_p;
	#else
	gdb_set_async_override(0);
	#endif
    }
    
    return (had_error);
}

/*--------------------------------------------------------------------------------------*/
			        /*---------------------*
				 | GDB Status Routines |
				 *---------------------*/

/*------------------------------------------------------------------------------*
 | gdb_target_running - determine if target program (inferior) is still running |
 *------------------------------------------------------------------------------*
 
 If a RUN command has been issued and the target program has not yet completed 1 is
 returned.  Otherwise 0 is returned.
 
 This allows you to control initialization and termination sequences and to permit 
 operations which only make sense while the target is being run (e.g., accessing the
 target's registers).
*/

int gdb_target_running(void)
{
    return (target_has_execution != 0);
}


/*---------------------------------------------------------------*
 | gdb_target_pid - return the target program's (inferior's) pid |
 *---------------------------------------------------------------*
 
 If the inferior is running it's pid is returned otherwise -1 is returned.
*/

int gdb_target_pid(void)
{
    return target_has_execution ? PIDGET(inferior_ptid) : -1;
}


/*-----------------------------------------------------------------------------------*
 | gdb_have_registers - determine if target (inferior) register values are available |
 *-----------------------------------------------------------------------------------*
 
 Returns 1 if the target register values are available and 0 otherwise.
*/

int gdb_have_registers(void)
{
    return (target_has_registers != 0);
}


/*------------------------------------------------------------------*
 | gdb_is_command_defined - see if the specified command is defined |
 *------------------------------------------------------------------*
 
 Returns 1 if the specified command is defined and 0 if it isn't.
*/

int gdb_is_command_defined(char *theCommand)
{
    return (lookup_cmd(&theCommand, cmdlist, "", 1, 1) != NULL);
}


/*-------------------------------------------------------*
 | gdb_get_prompt - return the current gdb prompt string |
 *-------------------------------------------------------*
 
 The buffer is assumed large enough to hold the prompt string and it is also returned
 as the function result.
*/
 
char *gdb_get_prompt(char *prompt_buffer)
{
    char *prompt = get_prompt();
    
    if (prompt)
    	strcpy(prompt_buffer, prompt);
    else
    	*prompt_buffer = '\0';
    
    return (prompt_buffer);
}


/*----------------------------------------------------------------------*
 | gdb_interactive - return 1 if the current input is coming from stdin |
 *----------------------------------------------------------------------*
 
 Note this is generally not needed since the second argument of plugins indicates that
 same information.
*/

int gdb_interactive(void)
{
    return (instream == stdin);
}


/*-------------------------------------------------------------*
 | gdb_get_function_start - return start address of a function |
 *-------------------------------------------------------------*
 
 Returns the address of the start of the function containing the specified address or
 NULL if the start address cannot be determined.
 
 This code is basically ripped off from printcmd.c:build_address_symbolic().  We're
 only interested in the start address here.
*/

GDB_ADDRESS gdb_get_function_start(GDB_ADDRESS addr)
{
    struct symbol         *symbol;
    struct minimal_symbol *msymbol;
    asection              *section = NULL;
    CORE_ADDR		  starting_addr;
    
    FIX_TARGET_ADDR(addr);
    
    if (overlay_debugging) {
	section = find_pc_overlay(addr);
      	if (pc_in_unmapped_range(addr, section))
	    addr = overlay_mapped_address(addr, section);
    }

    msymbol = lookup_minimal_symbol_by_pc_section(addr, section);
    symbol  = find_pc_sect_function(addr, section);
    
    /* APPLE LOCAL begin address ranges   */
    if (symbol)
    	starting_addr = BLOCK_LOWEST_PC(SYMBOL_BLOCK_VALUE(symbol));
    /* APPLE LOCAL end address ranges  */
    	
    if (msymbol && (SYMBOL_VALUE_ADDRESS(msymbol) > starting_addr || symbol == NULL))
    	starting_addr = SYMBOL_VALUE_ADDRESS(msymbol);
  
    return ((symbol || msymbol) ? starting_addr : NULL);
}


/*---------------------------------------------------------------------------------*
 | gdb_address_symbol - convert an address to a string (possibly with symbol info) |
 *---------------------------------------------------------------------------------*
 
 Called to convert an address to s symbol.  The function returns the pointer to the
 symbol string possibly truncated to maxLen characters.  If onlyAddr is non-zero
 then addr is simply converted to a hex value ("0xXXXX....").  If onlyAddr is 0 then
 the symbol information associated with the addr is appended to the string.
 
 NULL is never returned from this function.  At a minimum the hex address is returned.
 The symbol information is not appended if it cannot be determined.
 
 The full output formats are as follows:
 
   0xXXXX...: <name[+offset] in filename>
   0xXXXX...: <name[+offset] at filename:line>
   
 Internal note: This is basically a blatant rip off of a combination of gdb's
 print_address_symbolic() and deprecated_print_address_numeric().  Also factored
 in is the guts of print_longest() for hex conversion.  We need to "reinvent the
 wheel" here since all those gdb routines want to output their stuff.  We only
 want the string, and for efficiency purposes, we want to get it without having
 to play output redirection games.
*/

char *gdb_address_symbol(GDB_ADDRESS addr, int onlyAddr, char *symbol, int maxLen)
{
    char *name, *filename, *p, *symbol0 = symbol;
    int  addr_bit, unmapped, offset, line, len, maxLen1, n;
    char tmpstr[40];
    
    FIX_TARGET_ADDR(addr);
    
    /* Convert the address (0xXXXXX...).  If that's all we need to do then we're done...*/
    
    len = sprintf(symbol, "%s", int_string((ULONGEST)addr, 16, 1, 0, 1));
   
    if (onlyAddr)
    	return (symbol0);
    
    /* Append the symbol info to the address...						*/
   
    unmapped = offset = line = 0;
    name = filename = NULL;
    
    if (build_address_symbolic(addr, (demangle || asm_demangle), &name, &offset,
    			       &filename, &line, &unmapped))
    	return (symbol0);
    
    symbol += len;
    *symbol++ = ':';
    *symbol++ = ' ';
    len += 2;
    
    *symbol++ = '<';
    if (unmapped) {				/* leave room for right delimiters	*/
    	*symbol++ = '*';
    	len += 2;
    	maxLen1 = maxLen - 2;
    } else {
    	++len;
    	maxLen1 = maxLen - 1;
    }
    
    p = name;					/* append the name symbol...		*/
    for (p = name; *p && len < maxLen1; ++len)
    	*symbol++ = *p++;
    gdb_free(name);
    
    if (offset && len < maxLen1) {		/* append offset if we have it...	*/
    	sprintf(tmpstr, "+%u", (unsigned int)offset);
	for (p = tmpstr; *p; ++len)
	    *symbol++ = *p++;
    }
    
    if (filename) {				/* append filename if we have it...	*/
    	if (len < maxLen1) {
	    if (len == -1) {
		if (len + 4 + strlen(filename) < maxLen1) {
		    len += sprintf(symbol, "in %s", filename);
		    symbol += len;
		}
	    } else if (len + 4 + strlen(filename) + sprintf(tmpstr, ":%d", line) < maxLen1) {
		len += sprintf(symbol, "at %s%s", filename, tmpstr);
		symbol += len;
	    }
    	}
    	gdb_free(filename);
    }
    
    if (len >= maxLen1) {			/* if too long truncate the output...	*/
    	symbol -= 3;
    	*symbol++ = '.'; *symbol++ = '.'; *symbol++ = '.';
    }
    	
    if (unmapped)				/* we're done				*/
    	*symbol++ = '*';
    *symbol++ = '>';
    
    *symbol = '\0';
    
    return (symbol0);
}


/*--------------------------------------------------------------------------------*
 | gdb_target_arch - Return 8 for a 64-bit target architecture otherwise return 4 |
 *--------------------------------------------------------------------------------*
 
 This is used to know whether the inferior was compiled for a 64 ot 32 bit architecture.
*/

int gdb_target_arch(void)
{
    return (gdbarch_tdep(current_gdbarch)->wordsize);
}

/*--------------------------------------------------------------------------------------*/
			   /*-------------------------------*
			    | Convenience Variable Routines |
			    *-------------------------------*/

/*----------------------------------------------------------*
 | gdb_set_int - perform "set $theVariable = (int)theValue" |
 *----------------------------------------------------------*/

void gdb_set_int(char *theVariable, int theValue)
{
    set_internalvar(lookup_internalvar(theVariable+1),
                    value_from_longest(builtin_type_int, (LONGEST)theValue));
}


/*------------------------------------------------------------*
 | gdb_set_long - perform "set $theVariable = (long)theValue" |
 *------------------------------------------------------------*/

void gdb_set_long(char *theVariable, long theValue)
{
    set_internalvar(lookup_internalvar(theVariable+1),
                    value_from_longest(builtin_type_long, (LONGEST)theValue));
}


/*----------------------------------------------------------------------*
 | gdb_set_long_long - perform "set $theVariable = (long long)theValue" |
 *----------------------------------------------------------------------*/

void gdb_set_long_long(char *theVariable, long long theValue)
{
    set_internalvar(lookup_internalvar(theVariable+1),
                    value_from_longest(builtin_type_long_long, (LONGEST)theValue));
}


/*----------------------------------------------------------------*
 | gdb_set_double - perform "set $theVariable = (double)theValue" |
 *----------------------------------------------------------------*/

void gdb_set_double(char *theVariable, double theValue)
{
    set_internalvar(lookup_internalvar(theVariable+1),
                    value_from_double(builtin_type_double, (DOUBLEST)theValue));
}


/*-----------------------------------------------------------------*
 | gdb_set_string - perform "set $theVariable = (char *)theString" |
 *-----------------------------------------------------------------*
 
 Note, unlike gdb_set_int() and gdb_set_double() strings are stored by gdb in the
 target's memory.  That means the target has to be running when this call is done.
 Sorry, but that's also the rule for using strings in the gdb command language as well.
*/

void gdb_set_string(char *theVariable, char *theString)
{
    #if 0
    char line[2048];
    sprintf(line, "%s=\"%s\"", theVariable, theString);
    parse_and_eval(line);
    #else
    set_internalvar(lookup_internalvar(theVariable+1),
    		    value_coerce_array(value_string(theString, strlen(theString) + 1)));
    #endif
}


/*----------------------------------------------------------------------*
 | gdb_set_address - perform "set $theVariable = (GDB_ADDRESS)theValue" |
 *----------------------------------------------------------------------*
 
 This is the same as gdb_set_long_long but we supply it for type checking purposes
 and for self-documentation in the caller's code.
*/

void gdb_set_address(char *theVariable, GDB_ADDRESS theValue)
{
    set_internalvar(lookup_internalvar(theVariable+1),
                    value_from_pointer(builtin_type_void_data_ptr, (CORE_ADDR)theValue));
}


/*-----------------------------------------------------------------------------*
 | expression_to_value_ptr - convert a expressions into a gdb value_ptr result |
 *-----------------------------------------------------------------------------*
 
 Returns a gdb value pointer representing the evaluated expression result.
*/

static struct value *expression_to_value_ptr(char *expression)
{
    struct expression *expr;
    struct cleanup    *old_chain;
    struct value      *vp;
             
    expr = parse_expression(expression);
    old_chain = make_cleanup(free_current_contents, &expr);
    vp = evaluate_expression(expr);
    do_cleanups(old_chain);
    
    return (vp);
}


/*-----------------------------------------------------*
 | gdb_get_int - return the int value of an expression |
 *-----------------------------------------------------*
 
 Returns the int value of the specified expression.
*/

int gdb_get_int(char *expression)
{
    #if 0
    return ((int)value_as_long(lookup_internalvar(expression+1)));
    #else
    LONGEST value = value_as_long(expression_to_value_ptr(expression));
    //fprintf(stderr, "gdb_get_int(%s) <-- %ld\n", theVariable, (int)value);
    return ((int)value);
    #endif
}


/*----------------------------------------------------------*
 | gdb_get_long - return the integer value of an expression |
 *----------------------------------------------------------*
 
 Returns the long value of the specified expression.
*/

long gdb_get_long(char *expression)
{
    LONGEST value = value_as_long(expression_to_value_ptr(expression));
    return ((long)value);
}


/*-----------------------------------------------------------------*
 | gdb_get_long_long - return the long long value of an expression |
 *-----------------------------------------------------------------*
 
 Returns the long long value of the specified expression.
*/

long long gdb_get_long_long(char *expression)
{
    LONGEST value = value_as_long(expression_to_value_ptr(expression));
    return ((long long)value);
}


/*-----------------------------------------------------------*
 | gdb_get_double - return the double value of an expression |
 *-----------------------------------------------------------*
 
 Returns the double value of the specified floating point expression.
*/

double gdb_get_double(char *expression)
{
    #if 0
    return ((int)value_as_double(lookup_internalvar(expression)));
    #else
    DOUBLEST value = value_as_double(expression_to_value_ptr(expression));
    //fprintf(stderr, "gdb_get_double(%s) <-- %g\n", theVariable, (double)value);
    return ((double)value);
    #endif
}


/*--------------------------------------------------------*
 | gdb_get_string - return the string value of a variable |
 *--------------------------------------------------------*
 
 This is used to get the value of the specified convenience variable as a string.  If the
 variable is undefined "" is returned.  Up to maxlen characters are copied to the
 specified string buffer (str).
*/

char *gdb_get_string(char *theVariable, char *str, int maxlen)
{
    struct value *vp;
    CORE_ADDR temp;
    int	      i;
    
    vp = expression_to_value_ptr(theVariable);
    if (!vp)
    	return (strcpy(str, ""));
        
    temp = value_as_address(vp);
    
    for (i = 0; i < maxlen; i++) {
  	QUIT;
  	target_read_memory(temp + i, str + i, 1);
  	if (str[i] == '\0')
    	    break;
    }
    
    str[i] = '\0';
    
    //fprintf(stderr, "gdb_get_string(%s) <-- %s\n", theVariable, str);
    
    return (str);
}


/*-----------------------------------------------------------------*
 | gdb_get_address - return the GDB_ADDRESS value of an expression |
 *-----------------------------------------------------------------*
 
 Returns the value of the specified expression as a GDB_ADDRESS.
*/

GDB_ADDRESS gdb_get_address(char *expression)
{
    CORE_ADDR value = value_as_address(expression_to_value_ptr(expression));
    return ((GDB_ADDRESS)value);
}


/*-----------------------------------------------------------*
 | is_var_defined - see if a convenience variable is defined |
 *-----------------------------------------------------------*
 
 This is an internal routine that takes a convenience variable name and returns 1 if
 it's defined or 0 if it is not defined.
*/

static int is_var_defined(char *theVariable)
{
    struct value *vp = value_of_internalvar(lookup_internalvar(theVariable+1));
    
    return ((vp) && value_type(vp) && TYPE_CODE(value_type(vp)) != TYPE_CODE_VOID);
}


/*---------------------------------------------------------------*
 | gdb_is_var_defined - see if a convenience variable is defined |
 *---------------------------------------------------------------*
 
 Returns 1 if the specified convenience variable is defined and 0 if it is not.
*/
 
 int gdb_is_var_defined(char *theVariable)
 {
     return (is_var_defined(theVariable));
 }

/*--------------------------------------------------------------------------------------*/
		       /*---------------------------------------*
		        | Direct Register Manipulation Routines |
			*---------------------------------------*/

/*----------------------------------------------------------*
 | gdb_set_register - set the value of a specified register |
 *----------------------------------------------------------*
 
 Set the value of theRegister (e.g., "$r0") from the size bytes in the specified value
 buffer.  The size must match the size of the register.  NULL is returned if the
 assignment is successful.  Otherwise the function returns a pointer to an appropriate
 error string.
 
 The following errors are possible:
    
    no registers available at this time
    no frame selected
    bad register
    invalid register length
    left operand of assignment is not an lvalue
    could not convert register to internal representation
    
 Note, that it is recommended that the more general gdb_set_int() be used for 32-bit
 registers.  The gdb_set_register() routine is intended mainly for setting larger
 register data types like the AltiVec 16-byte registers.
*/
 
char *gdb_set_register(char *theRegister, void *value, int size)
{
    int               regnum;
    char              *start = theRegister, *end;
    struct value      *vp;
    struct frame_info *frame;
    
    if (!target_has_registers)
    	return ("no registers available at this time");
    
    if ((frame = get_selected_frame(NULL)) == NULL)
      return ("no frame selected");
    
    if (*start == '$')
	++start;
    end = start + strlen(start);
    
    regnum = frame_map_name_to_regnum(frame, start, end - start);
    if (regnum < 0)
    	return ("bad register");
    
    if (register_size(current_gdbarch, regnum) != size)
    	return ("invalid register length");
    
    vp = expression_to_value_ptr(theRegister);
    if (!vp)
    	return ("could not convert register to internal representation");
    
    if (VALUE_LVAL(vp) != lval_register)
    	return ("left operand of assignment is not an lvalue");
    
    deprecated_write_register_bytes(VALUE_ADDRESS(vp) + value_offset(vp), (char *)value, size);
    
    return (NULL);
}


/*-------------------------------------------------------------*
 | gdb_get_register - return the value of a specified register |
 *-------------------------------------------------------------*

 Returns the value of theRegister (e.g., "$r0") in the provided value buffer.  The
 value pointer is returned as the function result and the value is copied to the
 specified buffer (assumed large enough to hold the value and at least a long long).
 If the register is invalid, or its value cannot be obtained, NULL is returned and
 the value buffer (treated as a long* pointer) is set with one of the following
 error codes:

    Gdb_GetReg_NoRegs     no registers available at this time
    Gdb_GetReg_NoFrame    no frame selected
    Gdb_GetReg_BadReg     bad register (gdb doesn't know this register)
    Gdb_GetReg_NoValue    value not available

 Always use this function instead of, say, gdb_get_int(), to get register values
 because (a) it is more efficient (not expression evaluation is done) and (b) it
 is more accurate in that GDB might not be in the proper context to get the
 current register frame value.
*/

void *gdb_get_register(char *theRegister, void *value)
{
    int               regnum;
    char              *end;
    struct frame_info *frame;
    
    if (!target_has_registers) {
	//strcpy((char *)value, "no registers available at this time");
	*(long long *)value = Gdb_GetReg_NoRegs;
	return (NULL);
    }
    
    if ((frame = get_selected_frame(NULL)) == NULL) {
	//strcpy((char *) value, "no frame selected");
	*(long long *)value = Gdb_GetReg_NoFrame;
	return (NULL);
    }
    
    if (*theRegister == '$')
	++theRegister;
    end = theRegister + strlen(theRegister);
    
    regnum = frame_map_name_to_regnum(frame, theRegister, end - theRegister);
    if (regnum < 0) {
	//strcpy((char *)value, "bad register");
	*(long long *)value = Gdb_GetReg_BadReg;
	return (NULL);
    }

    if (!frame_register_read(frame, regnum, (char *)value)) {
    	//strcpy((char *)value, "value not available");
	*(long long *)value = Gdb_GetReg_NoValue;
	return (NULL);
    }
    
    //if (size)
    //	*size = register_size (current_gdbarch, regnum);
    //gdb_printf("type = %d\n", TYPE_CODE(gdbarch_register_virtual_type(current_gdbarch, regnum)));
    
    return (value);
}


/*-------------------------------------------------*
 | gdb_get_sp - get the value of the stack pointer |
 *-------------------------------------------------*
 
 Returns the value of the stack pointer.
*/

GDB_ADDRESS gdb_get_sp(void)
{
    /* This is a hack.  Sometimes gdb leaves the deprecated_selected_frame null, but	*/
    /* still uses it.  get_selected_frame will force it to get set. 			*/
    
    get_selected_frame(NULL);
   
    /* Note, the above comment and call were done in a different context getting the	*/
    /* sp, not the pc.  That was made unnecessary when the code to get the sp was	*/
    /* replaced by calling gdb_get_register() above.  It does it's own call to 		*/
    /* get_selected_frame().  The repeatable test case that illustrated the bug could 	*/
    /* no longer be reproduced due to calling get_selected_frame() in that context.  	*/

    /* Since fixing that problem, the identical problem has been seen at least once	*/
    /* again.  At the time of the original fix this gdb_get_sp() did not exist.  So	*/
    /* it's only a guess, but I am assuming the problem can occur calling read_sp(), 	*/
    /* albeit more rarely.  If it isn't this, then I don't know what else it could be	*/
    /* without a reproducible test case.						*/
   
    return ((CORE_ADDR)read_sp());
}


/*-----------------------------------------------*
 | gdb_get_reg_size - get the size of a register |
 *-----------------------------------------------*
 
 For 64-bit register value support; return the size of register regnum.  For generality
 we don't assume all the sizes are the same.  Hence the regnum argument.
*/

int gdb_get_reg_size(int regnum)
{
    return (register_size(current_gdbarch, regnum));
}


/*--------------------------------------------------------------------------------------*/
			     /*----------------------------*
			      | Target Memory Manipulation |
			      *----------------------------*/

/*---------------------------------------------------------------------------------*
 | gdb_read_memory - copy a value from an target address (string) to plugin memory |
 *---------------------------------------------------------------------------------*
 
 The n bytes in the target's memory represented by the src expression *string*
 are copied to the plugin memory specified by dst.  The target actual address is
 returned as the function result.
*/

GDB_ADDRESS gdb_read_memory(void *dst, char *src, int n)
{
    CORE_ADDR memaddr;
    int       status;
    
    memaddr = parse_and_eval_address(src);
    status  = target_read_memory(memaddr, dst, n);
    
    if (status != 0)
	memory_error(status, memaddr);
    
    return ((GDB_ADDRESS)memaddr);
}


/*----------------------------------------------------------------------------------*
 | gdb_read_memory_from_addr - copy a value from an target address to plugin memory |
 *----------------------------------------------------------------------------------*
 
 The n bytes in the target's memory at addr are copied to the plugin memory specified
 by dst.  The target address is returned as the function result.  If an error is
 detected while reading NULL is returned if report_error is 0.  Otherwise an error
 message is displayed. 
*/

GDB_ADDRESS gdb_read_memory_from_addr(void *dst, GDB_ADDRESS src, int n, int report_error)
{
    int status;
    
    FIX_TARGET_ADDR(src);
    
    status = target_read_memory((CORE_ADDR)src, dst, n);
    
    if (status != 0) {
    	if (report_error)
	    memory_error(status, (CORE_ADDR)src);
	return (NULL);
    }
    
    return (src);
}


/*----------------------------------------------------------------------------------*
 | gdb_write_memory - write a value from plugin memory to a target address (string) |
 *----------------------------------------------------------------------------------*
 
 The n bytes from the (plugin) src are written to the target memory address 
 represented by the dst expression *string*.
*/

void gdb_write_memory(char *dst, void *src, int n)
{
    CORE_ADDR memaddr;
    int       status;
    
    memaddr = parse_and_eval_address(dst);
    status  = target_write_memory(memaddr, src, n);
    
    if (status != 0)
	memory_error(status, memaddr);
}


/*---------------------------------------------------------------------------------*
 | gdb_write_memory_to_addr - write a value from plugin memory to a target address |
 *---------------------------------------------------------------------------------*
 
 The n bytes from the (plugin) src are written to the target memory address 
 represented by the dst expression value.
*/

void gdb_write_memory_to_addr(GDB_ADDRESS dst, void *src, int n)
{
    int status;
    
    FIX_TARGET_ADDR(dst);
    
    status = target_write_memory((CORE_ADDR)dst, src, -n);

    if (status != 0)
	memory_error(status, (CORE_ADDR)dst);
}

/*--------------------------------------------------------------------------------------*/
				   /*----------------*
				    | Print Routines |
				    *----------------*/

/*---------------------------------------------------------*
 | gdb_printf - print a line allowing gdb to do the output |
 *---------------------------------------------------------*
 
 Generate a printf to the current (redirected) stdout output.
*/
 
void gdb_printf(char *fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    gdb_vfprintf(gdb_stdout, fmt, ap);
    va_end(ap);
}


/*------------------------------------------------------------------------*
 | gdb_vprintf - print a line allowing gdb to do the output (stdarg form) |
 *------------------------------------------------------------------------*/
 
void gdb_vprintf(char *fmt, va_list ap)
{
    gdb_vfprintf(gdb_stdout, fmt, ap);
}


/*--------------------------*
 | gdb_puts - puts a string |
 *--------------------------*/

void gdb_puts(char *s)
{
    if (s)
	gdb_printf("%s", s);
}


/*----------------------------------------------------------*
 | gdb_fprintf - Generate a fprintf to the specified stream |
 *----------------------------------------------------------*
 
 The current (redirected) stream is not used for this call and should be used when 
 the caller does have control over the output stream (as opposed to, say using
 gdb_execute_command() where gdb may do the output.
 
 Note that even if output is currently redirected with gdb_redirect_output() for gdb
 command output you can still use gdb_fprintf() for your own GDB_FILE streams that
 are not currently redirected and use their filters.  This allows, for instance, to
 chain output filters (this discussions assumes you read the comments on redirected
 output first).  For example, say you redirect output as follows (assume GDB_FILE
 "stream1" is already open and redirected to "filter1"):
 
 	GDB_FILE *previous_stream;
	
 	stream2 = gdb_open_output(stdout, filter2, &previous_stream);
	previous_stream = gdb_redirect_output(stream2);
	
	...stuff that causes gdb to do prints...
	
	gdb_close_output(stream2);
	
 In filter2 (a prototype of the form gdb_output_filter_ftype) you use it's data 
 parameter as a GDB_FILE stream then doing gdb_fprintf()'s to that stream will cause
 that output to go through filter1.  Further, when you gdb_close_output(stream2) the
 output redirection will again revert back to using filter1 assuming it was the 
 redirected stream in effect when you did the gdb_open_output().
 
 There's no limit on the cascading of the filters using this technique if you define
 the data to hold multiple GDB_FILE's that are passed among a set of cooperating output
 filters all expecting that same data.
*/

void gdb_fprintf(GDB_FILE *stream, char *fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    gdb_vfprintf(stream, fmt, ap);
    va_end(ap);
}


/*-----------------------------------------------------------*
 | gdb_vprintf - Generate a vfprintf to the specified stream |
 *-----------------------------------------------------------*/

void gdb_vfprintf(GDB_FILE *stream, char *fmt, va_list ap)
{    
    if (stream == (GDB_FILE *)1)
    	stream = gdb_stdout;
    else if (stream == (GDB_FILE *)2)
    	stream = gdb_stderr;
    
    vfprintf_filtered(stream, fmt, ap);
}


/*----------------------------------------*
 | gdb_fputs - fputs a string to a string |
 *----------------------------------------*/

void gdb_fputs(char *s, GDB_FILE *stream)
{
    if (s)
   	fprintf_filtered(stream, "%s", s);
}


/*----------------------------------------------------------------------------------*
 | gdb_print_address - display the address and function corresponding to an address |
 *----------------------------------------------------------------------------------*
 
 Display the address and function corresponding to an address (expression or file/line
 specification).  If the address corresponds to a source line, the file/line
 information along with the source line are also displayed.  Otherwise the load segment
 information, which includes the segments type, encompassing address range, and,
 depending on section type, either the load address or pathname, are displayed.
 
 Note, if source lines are displayed they are shown exactly as if a LIST was done of it.
 Thus it is listed in a context of N lines, where N is determined by the gdb SET listsize
 command.
 
 The SET listsize command set the gdb global lines_to_list which we access here.
*/

void gdb_print_address(char *address)
{
    int 	  	     i, curr_print_symbol_filename, len;
    long		     start_line, end_line;
    char		     *s, *p, filename[1024];
    GDB_ADDRESS		     addr;
    struct symtabs_and_lines sals;
    struct symtab_and_line   sal;
    CORE_ADDR		     start_pc, end_pc;
    struct objfile	     *objfile;
    struct obj_section	     *objsect;
    
    extern int lines_to_list;		  /* # of lines "list" command shows by default */
    
    static struct cmd_list_element *show_print_symbol_filename = NULL;
    static int 			   *print_symbol_filename_p    = NULL;
    
    /* If this is the first time this routine got called then find the address of gdb's	*/
    /* private switch that contains the current state for "SET print symbol-filename" 	*/
    /* (FWIW, it happens to be called print_symbol_filename in gdb's printcmd.c).  We	*/
    /* need to have this switch set 'on' across the print_address() call to get gdb to	*/
    /* include file/line information as part of the address display.			*/
    
    if (show_print_symbol_filename == NULL || print_symbol_filename_p == NULL) {
	char *p = "show print symbol-filename";
	show_print_symbol_filename = lookup_cmd(&p, cmdlist, "", -1, 0);
	
	if (show_print_symbol_filename != NULL)
	    if (show_print_symbol_filename->var_type == var_boolean)
		print_symbol_filename_p = (int *)show_print_symbol_filename->var;
    }
    
    /* Try to print the source line associated with the addr (if there is one)...	*/
    
    sals = decode_line_spec_1(address, 0);
    
    if (print_symbol_filename_p) {
	curr_print_symbol_filename = *print_symbol_filename_p;
	*print_symbol_filename_p = 1;
	
	/* C++  More than one line may have been specified, as when the user		*/
	/* specifies an overloaded function name. Print info on them all.		*/
	/* If we can't print any then display the section and containing file.		*/
	
	for (i = 0; i < sals.nelts; i++) {
	    sal = sals.sals[i];
	    if (sal.symtab == NULL) {
		*print_symbol_filename_p = curr_print_symbol_filename;
		gdb_free(sals.sals);
  		
  		objsect = find_pc_section(sal.pc);
  		if (!objsect)
		    gdb_error("No file for address 0x%.8llX", (long long)sal.pc);
		    /* gdb_error() does not return */
		
		s = bfd_get_target(objsect->objfile->obfd);
      		gdb_printf("0x%.8llX is in %s %s section (0x%.8llX-0x%.8llX) in\n ", 
      			   (long long)sal.pc,
      			   strncmp(s, "mach-o", 6) == 0 ? "mach-o" : s,
      			   bfd_section_name(unused, objsect->the_bfd_section),
      			   (long long)objsect->addr, (long long)objsect->endaddr);
      		
      		s = bfd_get_filename(objsect->objfile->obfd);
      		if (strncmp(s, "[memory object", 14) == 0) {
      		    p = strstr(s, " for 0x");
      		    if (p) {
      		    	strncpy(filename, s, len = p - s);
      		    	strcpy(filename + len, "]");
      		    	s = filename;
      		    }
      		}
      		    
      		gdb_printf("%s\n", s);

      		return;
	    }
	    
	    if (sal.line > 0 && find_line_pc_range(sal, &start_pc, &end_pc)) {
		if (i > 0)
		    gdb_printf("\n");
		print_address(start_pc, gdb_stdout);
		gdb_fputs("\n", gdb_stdout);
		start_line = sal.line - lines_to_list/2;
		if (start_line < 1)
		    start_line = 1;
		end_line = start_line + lines_to_list - 1;
		if (end_line > sal.symtab->nlines)
		    end_line = sal.symtab->nlines;
		//gdb_printf("start_line = %d, end_line = %d, nlines = %d\n",
		// 	start_line, end_line, end_line - start_line + 1);
		print_source_lines(sal.symtab, start_line, end_line - start_line + 1, 0);
	    }
	}
      
	*print_symbol_filename_p = curr_print_symbol_filename;
    	gdb_free(sals.sals);
    }
}


/*----------------------------------------------*
 | gdb_query - display a query like gdb does it |
 *----------------------------------------------*
 
 The arguments are exactly the same as for printf().  The function returns 1 if the
 answer is "yes".  The formatted message should end with a "?".  It should not say how
 to answer, because gdb does that.
*/

int gdb_query(char *fmt, ...)
{
    int     result = 0;
    va_list ap;
    char    msg[1024];
    
    va_start(ap, fmt);
    vsprintf(msg, fmt, ap);
    va_end(ap);
        
    return (query("%s", msg));
}

/*--------------------------------------------------------------------------------------*/
				  /*-----------------*
				   | Error Reporting |
				   *-----------------*/

/*------------------------------------*
 | gdb_error - report a command error |
 *------------------------------------*
 
 Report an error from a command.  Gdb reports the error and does not return to the caller.
*/

void gdb_error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    verror(fmt, ap);
    va_end(ap);
}


/*-------------------------------------*
 | gdb_verror - report a command error |
 *-------------------------------------*
 
 Same as gdb_error() above except that this function accepts a va_list.
*/

void gdb_verror(const char *fmt, va_list ap)
{
    verror(fmt, ap);
}


/*-------------------------------------------------------*
 | gdb_internal_error - report an internal command error |
 *-------------------------------------------------------*
 
 Report an internal error the same way gdb does.  This function does not return.  Note
 only a single string may be passed, not a format with values like gdb_error().
*/

void gdb_internal_error(char *msg)
{
    internal_error(__FILE__, __LINE__, msg);
}

/*--------------------------------------------------------------------------------------*/
				 /*-------------------*
				  | Memory Management |
				  *-------------------*/

/*----------------------------------*
 | gdb_malloc - allocate gdb memory |
 *----------------------------------*
 
 This function does not return if the memory cannot be allocated.  The error is reported
 and the command aborted.
*/

void *gdb_malloc(int amount)
{
    return ((void *)xmalloc(amount));
}


/*---------------------------------------*
 | gdb_realloc - (re)allocate gdb memory |
 *---------------------------------------*
 
 This function does not return if the memory cannot be allocated.  The error is reported
 and the command aborted.
*/

void *gdb_realloc(void *p, int amount)
{
    return ((void *)xrealloc(p, amount));
}


/*--------------------------------------------------*
 | gdb_free - free memory allocated by gdb_malloc() |
 *--------------------------------------------------*/

void gdb_free(void *p)
{
    xfree(p);
}

/*--------------------------------------------------------------------------------------*/
			      /*-------------------------*
			       | Miscellaneous Utilities |
			       *-------------------------*/

/*---------------------------------------------------------------------*
 | gdb_setup_argv - build an argv[] vector from a argument list string |
 *---------------------------------------------------------------------*
 
 This builds an argv[] vector from a command line of arguments.  Since each plugin
 command is passed all its arguments as a string, this routine may be convenient to break
 up the line into individual arguments using the standard argv/argc conventions.  It is
 assumed the argv[] array is large enough to hold maxargs+1 entries.  The argv[0] entry
 is set with the argv0 parameter which is assumed to be a command name.  However, if
 argv0 is NULL then argv[0] will contain the first word of s.  The argv[argc] entry is
 always NULL.  The function returns argc as its result.
       
 An argv argument is defined here as any sequence of non-blank characters where blanks
 may be contained in singly or doubly quoted strings or paired parenthesis or paired
 square brackets.  Thus (char *)a[i + 1] represents a single argv argument.  Also escaped
 characters (including hex and octal) are permitted.
 
 The argv[] pointers are pointers into the string s.  String s is MODIFIED to contain
 null characters at the end of each argument.  So the caller must assume s has the same
 lifetime as argv[] and is a modifiable string.
 
 Caution: If you pass the command line arg string passed to a plugin then you run the
 risk of modifying gdb's command line.  This is not a problem unless you intend to all
 the plugin command to be reexecuted when the user enters a null command line (i.e.,
 just a return) to reexecute the previous command.  Since null delimiters are placed
 between the args in the string the reexecuted string will be effectively truncated to
 the first argument.Œ
*/
 
int gdb_setup_argv(char *s, char *argv0, int *argc, char *argv[], int maxargs)
{
    int i, n, paren = 0, brack = 0;
    char c, *p = s, str = '\0';
    
    *argc = 0;
    
    if (argv0)
    	argv[(*argc)++] = argv0;
    
    if (s) {
        while ((c = *s++) != '\0') {
	    if (c != ' ' && c != '\t') {
		if (*argc >= maxargs) {
		    --(*argc);
		    break;
		}
		
		argv[(*argc)++] = p;
		
		do {
		    switch (c) {
		    	case '\\':
			    if (!*s)
			    	c = '\\';
			    else
				switch (c = *s++) {
				    case 'n':
					*p++ = '\n';   break;
				    case 'r':
					*p++ = '\r';   break;
				    case 't':
					*p++ = '\t';   break;
				    case 'a':
					*p++ = '\a';   break;
				    case 'f':
					*p++ = '\f';   break;
				    case 'b':
					*p++ = '\b';   break;
				    case 'v':
					*p++ = '\v';   break;
				    case 'e':
					*p++ = '\033'; break;
				    case '0':
				    case '1':
				    case '2':
				    case '3':
				    case '4':
				    case '5':
				    case '6':
				    case '7':
					i = c - '0';
					n = 0;
					while (++n < 3) {
					    if ((c = *s++) >= '0' && c <= '7')
						i = i*8 + (c - '0');
					    else {
						--s;
						break;
					    }
					}
					*p++ = (char)i;
					break;
				    case 'x':
					if (!isxdigit(*s))
					    *p++ = c;
					else {
					    n = 0;
					    i = 0;
					    while (n++ < 2) {
					    	c = *s++;
						if (c >= '0' && c <= '9')
						     i = i*16 + (c - '0');
						else if (c >= 'a' && c <= 'f')
						     i = i*16 + (10 + c - 'a');
						else if (c >= 'A' && c <= 'F')
						     i = i*16 + (10 + c - 'A');
						else {
						    --s;
						    break;
						}
					    }
					    *p++ = (char)i;
					}
					break;
				    default:
					*p++ = c;
				}
			    break;
			    
			case '"':
			case '\'':
			    if (!str)
				str = c;
			    else if (c == str)
				str = 0;
			    *p++ = c;
			    break;
			    
			case '(':
			    *p++ = c;
			    ++paren;
			    break;
			    
			case ')':
			    *p++ = c;
			    if (--paren < 0)
			    	paren = 0;
			    break;
			    
			case '[':
			    *p++ = c;
			    ++brack;
			    break;
			    
			case ']':
			    *p++ = c;
			    if (--brack < 0)
			    	brack = 0;
			    break;
			    
			default:
			    *p++ = c;
		    }
		} while (*s && ((c = *s++) != ' ' || str || paren || brack));
		
		*p++ = '\0';
	    }
        } /* while */
    }
    
    argv[*argc] = NULL;															/* NULL at the end of the list				*/
    
    return (*argc);
}


/*----------------------------------------------------------------*
 | gdb_strcmpl - compare two strings for equality (ignoring case) |
 *----------------------------------------------------------------*

 Compare two strings for equality (ignoring case).  The function returns 1 if the two
 strings are equal (independent of case) and returns 0 otherwise.
*/

int gdb_strcmpl(char *s1, char *s2)
{
    char c1, c2;

    for (;;) {
	c1 = *s1++;
	if (islower(c1)) c1 = toupper(c1);
	c2 = *s2++;
	if (islower(c2)) c2 = toupper(c2);
	if (c1 != c2) return (0);
	if (!c1) return (1);
    }
}


/*------------------------------------------------------------*
 | gdb_keyword - find keyword in a table and return its index |
 *------------------------------------------------------------*

 Find the keyword in the table and return its index as the function result.  The table
 is a list of pointers to all the keywords with a NULL pointer (0) to mark the end of
 the list.  A value of -1 is returned if the keyword is not found in the table (case
 is ignored).  If the keyword is found, the function will return the array element
 index (relative to 0) associated with the found keyword.
*/

int gdb_keyword(char *keyword, register char **table)
{
    char **p;
	
    if(!table)
	return (-1);

    p = table - 1;
    while (*++p)
    	if (gdb_strcmpl(keyword, *p))
    	    return (p - table);
    
    return (-1);
}


/*------------------------------------------------------------------*
 | gdb_is_string - determine if an expression represents a "string" |
 *------------------------------------------------------------------*
 
 Returns 1 if the expression is a quoted "string" and 0 if it isn't.
 
 The reason we cannot simply check the first character of the expression for a double
 quote is that it could be specified as ("string"), i.e., enclosed in parenthesis.  This
 is due to backward compatibility with the "pure" gdb DEFINE command equivalent of this
 operation.
*/
 
int gdb_is_string(char *expression)
{
    struct value   *vp;
    enum type_code type;
    
    if (!expression)
    	return (0);
    	
    vp = expression_to_value_ptr(expression);
    if (!vp)
    	return (0);
    
    type = TYPE_CODE(value_type(vp));
    
    //fprintf(stderr, "vp->type->code = %d\n", (int)type);
    
    return (type == TYPE_CODE_ARRAY || type == TYPE_CODE_PTR);
}


/*-----------------------------------------------------*
 | gdb_tilde_expand - expand tilde's (~) in a pathname |
 *-----------------------------------------------------*
 
 Returns an malloc'ed string with is the result of expanding tilde's (~) in the
 specified pathname.
*/

char *gdb_tilde_expand(char *pathname)
{
    return (tilde_expand(pathname));
}


/*-------------------------------------------------------------------*
 | gdb_demangled_symbol - demangle a symbol if demangling is enabled |
 *-------------------------------------------------------------------*
 
 If demangling is enabled in gdb (SET print demangle |asm-demangle) then try see if
 the mangled symbol can be demangled (including is parameters if params is non-zero).
 The function sets the demangled name (possibly truncated to maxLen characters).
 
 If demangling is not enabled or cannot be done the original mangled name is copied
 to mangled (again possibly truncated to maxLen unless both demangled and mangled
 are pointers to the same string).
 
 In all cases the function returns the length of the mangled string.
 
 Note, a convention gdb uses for Mac OS X is to prefix stubs (trampolines) with
 the string "dyld_stub_".  If this is present in the mangled symbol it is stripped
 off and the remaining characters used for the mangled symbol.  If that can be
 demangled the demangled symbol is returned WITHOUT the "dyld_stub_" prefix.
*/

int gdb_demangled_symbol(char *mangled, char *demangled, int maxLen, int params)
{
    struct symbol	       *symbol;
    struct minimal_symbol      *msymbol;
    struct general_symbol_info *gsymbol;
    char		       *d = NULL, *m;
    int			       len;
    
    if (demangle || asm_demangle) {
	msymbol = lookup_minimal_symbol(demangled, NULL, NULL);
	if (msymbol)
	    gsymbol = &msymbol->ginfo;
	else {
	    symbol = lookup_symbol(demangled, NULL, FUNCTIONS_DOMAIN, NULL, NULL);
	    if (symbol)
		gsymbol = &symbol->ginfo;
	    else {				/* why is METHODS_DOMAIN separate?	*/
		symbol = lookup_symbol(demangled, NULL, METHODS_DOMAIN, NULL, NULL);
		gsymbol = symbol ? &symbol->ginfo : NULL;
	    }
	}
	
	if (gsymbol) {					/* try to demangle the symbol...*/
	    m = mangled;
	    if (strncmp(demangled, "dyld_stub_", sizeof ("dyld_stub_") - 1) == 0)
	    	m += sizeof ("dyld_stub_") - 1;
	    						/* this stuff is similar to 	*/
	    if (gsymbol->language == language_objc     ||/* symbol_find_demangled_name()*/
		gsymbol->language == language_objcplus ||
		gsymbol->language == language_unknown  ||
		gsymbol->language == language_auto)
		d = objc_demangle(m, 0);
		
	    if (d == NULL) {
		int options = DMGL_ANSI;
		if (params)
		    options |= DMGL_PARAMS;
		
		if (gsymbol->language == language_cplus    || 
		    gsymbol->language == language_objcplus ||
		    gsymbol->language == language_unknown  ||
		    gsymbol->language == language_auto)
		    d = cplus_demangle(m, options);
		    
		if (d == NULL)
		    if (gsymbol->language == language_java)
		    	d = cplus_demangle(m, options | DMGL_JAVA);
		 
		/* what about language_ada? */
	    }
	}
    }
    
    if (d == NULL) {					/* didn't demangle symbol	*/
    	if (mangled == demangled)			/* return original if possible	*/
    	    return (strlen(demangled));
    	d = mangled;					/* else treat same as demangled	*/
    }
    
    len = strlen(d);
    if (len < maxLen) {					/* return un-truncated...	*/
	strcpy(demangled, d);
	return (len);
    }
   
    memcpy(demangled, d, maxLen);			/* truncate...			*/
    demangled[maxLen] = '\0';
    return (maxLen);
}


/*----------------------------------------------------------------------------------*
 | ignore_output - gdb_show_objc_object()'s redirected stdout/stderr output filter  |
 *----------------------------------------------------------------------------------*

 This is the redirected stdout/stderr filter function to handle all output while in
 the critical region of gdb_show_objc_object().  See explaination there why all we
 need to do here is just drop all the output on the floor.
*/

static char *ignore_output(FILE *f, char *line, void *data)
{
    return (NULL);
}


/*----------------------------------------------------------------------------*
 | cleanup_gdb_objc_object - gdb_show_objc_object() cleanup recovery function |
 *----------------------------------------------------------------------------*
 
 gdb_show_objc_object() sets up this cleanup function to catch any abnormal error
 conditions (exceptions) that may arise during its critical region.  Here we longjmp
 back to gdb_show_objc_object() so that it can recover.
*/

static jmp_buf gdb_objc_object_env;

static void cleanup_gdb_objc_object(void *unused)
{
    longjmp(gdb_objc_object_env, 1);
}


/*-----------------------------------------------------*
 | gdb_show_objc_object - ask an object to show itself |
 *-----------------------------------------------------*
 
 Ask an object to display itself into the specified object string (up to maxLen
 characters).  If the string is truncated then '...' is appended to the end of the
 string.  The function returns the number of characters in the string.  0 is returned
 if the string cannot be generated.
 
 Note, this function is basically a gdb PRINT-OBJECT (PO) command except that the 
 output is returned in the string instead of being written to stdout.
*/

int gdb_show_objc_object(GDB_ADDRESS addr, char *objStr, int maxLen)
{
    int            len, orig_unwind_state, e, restore_terminal;
    char           c, *fn_name;
    unsigned long  test4;
    CORE_ADDR      strptr;
    struct cleanup *old_cleanup_chain;
    struct value   *objptr, *retval;
    GDB_FILE	   *redirect_stdout, *redirect_stderr;
    struct termios tio;
    
    static struct cached_value *function = NULL;
    
    /* Preliminary check to see if the addr is even readable...				*/
    
    if (target_read_memory(addr, &test4, 4) != 0)	/* can we even read from addr?	*/
    	return (0);					/* ...apparently not!		*/
    	
    /* There seems to be a case where _NSPrintForDebugger or _CFPrintForDebugger will	*/
    /* fail so bad that even the stack cannot be recovered.  It has been seen where the	*/
    /* object has a valid isa field (1st 4 bytes) but nothing but zero's following.  So	*/
    /* we program around this by insisting that the next two longs be non-zero.  It's a	*/
    /* hack.  But better than the alternative (note, the same failure can be reproduced	*/
    /* using gdb's own PO).								*/
    
    if (target_read_memory(addr+4, &test4, 4) != 0 || test4 == 0 ||
    	target_read_memory(addr+8, &test4, 4) != 0 || test4 == 0)
    	return (0);
    
    /* See if "_NSPrintForDebugger" or "_CFPrintForDebugger" exists in inferior and	*/
    /* get it's entry point address (struct value).  Otherwise, screw it!		*/
    
    if (function == NULL) {
	fn_name = "_NSPrintForDebugger";
	if (lookup_minimal_symbol(fn_name, NULL, NULL) == NULL) {
	    fn_name = "_CFPrintForDebugger";
	    if (lookup_minimal_symbol(fn_name, NULL, NULL) == NULL)
		return (0);
	}
	
    	function = create_cached_function(fn_name, builtin_type_voidptrfuncptr); 
	if (function == NULL)
	    return (0);
    }
    
    /* Set up argument (addr) as a struct value to pass to "_NSPrintForDebugger" or 	*/
    /* "_CFPrintForDebugger" as its argument...						*/
    
    objptr = value_from_longest(builtin_type_void_data_ptr, (CORE_ADDR)addr);
    if (objptr == NULL)
    	return (0);
    
    /* Prepare to do the call.  This is the critical region where we need to catch all	*/
    /* exceptions to undo what we are about to do here.  cleanup_gdb_objc_object() is	*/
    /* added to gdb's cleanup chain so that gdb's exception processing will call it.	*/
    /* It will longjmp back to here to allow us to undo the stuff we need to undo. 	*/
    /* Here's the stuff we need modify and restore:					*/
    
    /* (1) Save the unwind_on_signal state because we need to set it to restore the	*/
    /*     stack if the function fails in some way.					*/
    
    /* (2) Redirect stdout and stderr to insulate what where doing here from any 	*/
    /*     possible current redirections.  At the very least the call machinery does a	*/
    /*     flush of stdout.  If we don't insulate ourselves here we could potentially	*/
    /*     end up in an infinite loop (untile the stack overflows) if we got here in 	*/
    /*     the first place due to some redirected output filter wanting to use 		*/
    /*     gdb_show_objc_object().  The other reason we need to redirect stdout/stderr	*/
    /*     is to ignore any exception errors that might occur.  We don't care what they	*/
    /*     are, only that they occurred so we can return 0.				*/
    
    /* (3) Save the gdb terminal attributes to restore following the call.  		*/
    
    /*     This takes some explaining! The call machinery restores (sets) the inferior's*/
    /*     terminal attributes in preparation for doing the call (specifically when 	*/
    /*     infrun.c:resume() calls target_terminal_inferior() (a macro which has a call	*/
    /*     chain ending in set-unix.c:set_tty_state()).  One of the attributes set is	*/
    /*     ECHO (why?).  Upon return from the call the terminal is left in ECHO mode 	*/
    /*     which means it will echo every keystroke to the terminal.  			*/
    
    /*     Normally gdb restores it's state so that the ECHO attribute is off because	*/
    /*     the readline	 machinery does the echoing explicitly itself.  But under some 	*/
    /*     (not investigated) conditions, e.g., when the MacsBug screen is being 	*/
    /*     displayed) gdb doesn't reset the attributes (or doesn't get a chance to do 	*/
    /*     it) by the time the first prompt following what we do here occurs.  The	*/
    /*     result is every keystroke on that input line appears doubled -- one from the */
    /*     terminal echo and one from readline.						*/
    
    /*     Given this situation the easiest way to solve the problem is to simply save	*/
    /*     and restore gdb's terminal attributes across the call.  So that's why we are	*/
    /*     doing this.									*/
    
    /*     This explanation is far larger than the code to implement it.  But you can't	*/
    /*     believe how long it took to figure out this problem! :-(			*/
    
    orig_unwind_state = set_unwind_on_signal(1);				 /* (1) */
    
    redirect_stdout = gdb_open_output(stdout, ignore_output, NULL);		 /* (2) */
    redirect_stderr = gdb_open_output(stderr, ignore_output, NULL);
    gdb_redirect_output(redirect_stderr);
    gdb_redirect_output(redirect_stdout);
    
    if ((restore_terminal = (rl_instream == stdin)))				 /* (3) */
    	tcgetattr(fileno(rl_instream), &tio);
    
   /* Add our cleanup function to the cleanup chain so that it can longjmp back to here */
   /* to restore the original state prior to what we just changed above...		*/
   
   old_cleanup_chain = make_cleanup(cleanup_gdb_objc_object, NULL);
    
    if (setjmp(gdb_objc_object_env)) {			/* exceptions should come here	*/
	strptr = 0;					/* do only the restorations	*/
	goto cleanup;					
    }
    
    /* Call _NSPrintForDebugger(addr) or _CFPrintForDebugger(addr)...			*/
    
    /* Note that both target_check_safe_call() and call_function_by_hand() can raise 	*/
    /* exceptions, not to mention those pesky, and generally wrong, "corrupt stack"	*/
    /* errors (I just had to rub that in).						*/
    
    if (target_check_safe_call(OBJC_SUBSYSTEM|MALLOC_SUBSYSTEM, CHECK_ALL_THREADS)) {
    	retval = call_function_by_hand(lookup_cached_function(function), 1, &objptr);
    	strptr = value_as_address(retval);
    }
    
    /* Put everything back the way it was...						*/
    
    cleanup:						/* exceptions end up here too	*/
    
    discard_cleanups(old_cleanup_chain);
    
    if (restore_terminal)				/* cannot use	       *//* (3) */
	tcsetattr(fileno(rl_instream), TCSANOW, &tio);	/* target_terminal_ours()	*/
    
    gdb_close_output(redirect_stderr);						 /* (2) */
    gdb_close_output(redirect_stdout);
    
    set_unwind_on_signal(orig_unwind_state);					 /* (1) */
    
    if (strptr == 0)					/* had an exception I guess	*/
    	return (0);
        
    /* Copy the output from _NSPrintForDebugger or _CFPrintForDebugger to the callers's	*/
    /* string...									*/
    
    len = 0;

    while ((e = target_read_memory(strptr++, &c, 1)) == 0 && c != '\0' && len++ < maxLen)
	*objStr++ = c;
    
    if (e)						
    	return (0);					/* damn, almost made it too!	*/
    	
    *objStr = '\0';
    
    if (c != '\0')					/* if we truncated it...	*/
    	strcpy(objStr - 3, "...");			/* ...indicate that		*/
    
    return (len);
}

/*--------------------------------------------------------------------------------------*/
			     /*--------------------------*
			      | Object Module Operations |
			      *--------------------------*/

/*--------------------------------------------------------------------------------------*
 | gdb_is_addr_in_section - see if an address is within a specified Mach-o load section |
 *--------------------------------------------------------------------------------------*
 
 This is a rather low-level function which is used to determine whether the specified
 addr is located within one of an object file's load sections (segname_sectname).  If it
 is, a (const) pointer to the segname_sectname pointer is returned.  Otherwise NULL is
 returned.
 
 If segname_sectname is passed as NULL then the function will return a pointer to the
 first segname_sectname found in ANY of the loaded sections which contains the addr.
 
 The segname_sectname is a string indicating the Mach-o load segname and sectname
 concatenated with a period (e.g., "__TEXT.__cstring", "__DATA.__cfstring", etc.).  See
 'struct section' definition in /usr/include/mach-o/loader.h for some details.  Also see
 the Mach-o Runtime ABI documentation.
 
 For repeated tests for different sections for the SAME addr this function caches the
 gdb search information to avoid needless repeated object file searching.  To clear
 this cache pass addr with the value 0.  0 is returned for this case too.
*/

const char *gdb_is_addr_in_section(GDB_ADDRESS addr, char *segname_sectname)
{
    struct obj_section *addr_obj_section;
    char               *p;

    static struct obj_section *cached_addr_obj_section = NULL;
    static GDB_ADDRESS        cached_addr = 0;

    if (addr == NULL) {
    	cached_addr_obj_section = NULL;
    	cached_addr             = 0;
    	return (0);
    }

    FIX_TARGET_ADDR(addr);

    if (addr != cached_addr) {
	if ((addr_obj_section = find_pc_section(addr)) == NULL)
	    return (NULL);

	cached_addr             = addr;
	cached_addr_obj_section = addr_obj_section;
    } else
    	addr_obj_section = cached_addr_obj_section;

    p = addr_obj_section->the_bfd_section->name;

    if (strncmp(p, "LC_SEGMENT.", sizeof("LC_SEGMENT.") - 1) == 0)
    	p += (sizeof("LC_SEGMENT.") - 1);

    return ((segname_sectname == NULL || strcmp(segname_sectname, p) == 0) ? p : NULL);
}


/*------------------------------------------------------------------*
 | gdb_find_section - see if a specified Mach-o load section exists |
 *------------------------------------------------------------------*
 
 This function searches all the sections in all the loaded object files for the specified
 segname_sectname.  If found a count is returned as the function result indicating the
 number of instances found with that section name.  If ranges is not NULL it will be
 returned as a pointer to a gdb_malloc'ed list (same number of entries as was found) of
 section address ranges.  Each list entry has the following layout:
 
 typedef struct Section_Range {
     GDB_ADDRESS          addr;			lowest address in section
     GDB_ADDRESS          endaddr;		1+highest address in section
     struct Section_Range *next;		next on list
 } Section_Range;
 
 The segname_sectname is a string indicating the Mach-o load segname and sectname
 concatenated with a period (e.g., "__TEXT.__cstring", "__DATA.__cfstring", etc.).  See
 'struct section' definition in /usr/include/mach-o/loader.h for some details.  Also see
 the Mach-o Runtime ABI documentation.
*/

int gdb_find_section(char *segname_sectname, Section_Range **ranges)
{
    int                count = 0, got_one;
    char 	       *name;
    struct obj_section *s;
    struct objfile     *objfile;
    Section_Range      *last, *p;
    
    if (ranges)
    	*ranges = NULL;
    
    ALL_OBJSECTIONS (objfile, s) {
    	name = s->the_bfd_section->name;
    	
    	if (strncmp(name, "LC_SEGMENT.", sizeof("LC_SEGMENT.") - 1) != 0)
    	    got_one = (strcmp(name, segname_sectname) == 0);
    	else
    	    got_one = (strcmp(name + sizeof("LC_SEGMENT.") - 1, segname_sectname) == 0);
    	
	if (got_one) {
	    if (ranges) {
		p = (Section_Range *)gdb_malloc(sizeof(Section_Range));
		
		p->addr    = s->addr;
		p->endaddr = s->endaddr;
		p->next    = NULL;
		
		if (*ranges == NULL)
		    *ranges = p;
		else
		    last->next = p;
		last = p;
	    }
	    ++count;
	}
    }
    
    return (count);
}

#if 0
/*-----------------------------------------------------------------------------*
 | show_all_section_ranges - debugging routine tto dissplay all section ranges |
 *-----------------------------------------------------------------------------*
 
 This is not a user-available routine and only callable only with gdb.
*/

static void show_all_section_ranges(void)
{
  struct obj_section *s;
  struct objfile     *objfile;
  
  static char curr[2048];
  
  *curr = 0;
  
  ALL_OBJSECTIONS (objfile, s) {
    if (strcmp(objfile->name, curr) != 0) {
    	strcpy(curr, objfile->name);
    	gdb_printf("%s\n", curr);
    }
    gdb_printf("   %s: %.8llx %.8llx\n", s->the_bfd_section->name, s->addr, s->endaddr);
  }
}
#endif

/*--------------------------------------------------------------------------------------*/

#include "gdb_testing.i"

/*--------------------------------------------------------------------------------------*/

#if 0
void init_from_gdb(void) {}

/*
If the above is enabled then the API itself could be loaded as a plugin which means
it potentially could be shared among severy users of it instead of statically binding
it to each user.  The reason we don't enable it is because we export 4 globals from
the API, gdb_default_stdout, gdb_default_stderr, gdb_current_stdout, and
gdb_current_stderr but the LOAD-PLUGIN command's implementation does not cause those
variables to be exported.  It's due to an "incorrect" argument passed to the service
call that does the load.  Until that is fixed we cannot support the API as a sharable
plugin.
*/

#endif
