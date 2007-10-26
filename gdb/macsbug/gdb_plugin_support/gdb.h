#ifndef _GDB_PLUGIN_API_H_
#define _GDB_PLUGIN_API_H_
/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                        gdb.h                                         |
 |                                                                                      |
 |                              Gdb Interfaces For Plugins                              |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2005                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*

All plugins have the following general form:
        
    // This is a non-argv form
    static void plugin1_implementation(char *arg, int from_tty)
    {
	...the plugin's implementation...
    }
    
    // This is a plugin which uses argv (see below)
    static void plugin2_implementation(int argc, char *argv[], int from_tty)
    {
	...the plugin's implementation...
    }
    
    static void plugin2_setup_argv(char *arg, int from_tty)
    {
      int  argc;
      char *argv[5];
      gdb_setup_argv(arg, "my_cmd2", &argc, argv, 4);
      plugin2_implementation(argc, argv, from_tty);
    }
    
    - - -
    
    void init_from_gdb() 
    {
	gdb_define_cmd("cmd1", plugin1_implementation, Gdb_Public, "cmd1 help...");
	gdb_define_cmd("cmd2", plugin2_setup_argv, Gdb_Public "cmd2 help...");
	- - -
    }

The non-static function init_from_gdb() is required and must be that name.  The plugin
implementation(s) are associated with gdb command names where you specify the command
name, its associated implementation function, and its help by calling gdb_define_cmd()
for each plugin defined in this compilation unit.  Command help is grouped into classes,
For these examples it's Gdb_Public defining them just like DEFINE commands (classes are
described in more detail with the "Command Classes" calls). 

Each compilation unit is loaded using the LOAD-PLUGIN gdb command.  They should be 
built using at least the options indicated by the following command line:

    cc -I$gdb/gdb_plugin_support $gdb/gdb_plugin_support/gdb.o \
       -fno-common -bundle -undefined suppress plugin.c -o plugin
       
where in this example, $gdb is the plugin support directory where this header and
library are located, and plugin is what we want to call the plugin module.  In this
case a LOAD-PLUGIN with the pathname to plugin will load the plugin into gdb.  Note,
it's unfortunate, but you must specify a full pathname to the LOAD-PLUGIN command!

Note that a plugin implementation always has a fixed argument list consisting of a 
string and an int.  The string it the command's entire argument list. The from_tty just
indicates whether gdb is running this command from a terminal (!=0) or in batch mode
(0).

To simplify handling these argument lists gdb_setup_argv() is provided to convert the
argument list string into standard argv/argc conventions.  An example if its use is
illustrated in the plugin2 example above.

The other routines provided here are to allow the plugin to interface with gdb in various
ways while still keeping some independence of gdb (i.e., this interface is provided as
a library to hide all the internal details).

NOTE: BEFORE USING ANY OF THE SUPPORT ROUTINES YOU MUST CALL gdb_initialize() FIRST TO
      INITIALIZE THE PACKAGE.
*/
/*--------------------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdarg.h>

typedef unsigned long long GDB_ADDRESS;		/* all addrs are considered as this size*/

typedef void GDB_FILE;				/* anonymous stream variable type	*/
typedef void GDB_HOOK;				/* anonymous hook-function data type	*/

extern GDB_FILE *gdb_default_stdout;		/* default gdb stdout stream		*/
extern GDB_FILE *gdb_default_stderr;		/* default gdb stderr stream		*/ 
extern GDB_FILE *gdb_current_stdout;		/* current gdb stdout stream		*/
extern GDB_FILE *gdb_current_stderr;		/* current gdb stderr stream		*/ 

typedef void (*Gdb_Plugin)(char *, int);	/* all plugins follow this prototype	*/
typedef void (*Gdb_Exit_Handler)(void);		/* exit handler prototype		*/
typedef void (*Gdb_Raw_Input_Handler)(char *);	/* raw input handler prototype		*/
typedef void (*Gdb_Raw_Input_Set_Prompt)(char *);/* raw input prompt definition handler	*/
typedef void (*Gdb_Prompt_Positioning)(int);	/* prompt positioning function prototype*/

/*--------------------------------------------------------------------------------------*/
				/*----------------------*
				 | Command Help Classes |
				 *----------------------*

 When you type HELP with no arguments, gdb displays something like the following:
 
     List of classes of commands:
    
     aliases -- Aliases of other commands
     breakpoints -- Making program stop at certain points
     data -- Examining data
     files -- Specifying and examining files
     internals -- Maintenance commands
     obscure -- Obscure features
     running -- Running the program
     stack -- Examining the stack
     status -- Status inquiries
     support -- Support facilities
     tracepoints -- Tracing of program execution without stopping the program
     user-defined -- User-defined commands
     
     Type "help" followed by a class name for a list of commands in that class.
     Type "help" followed by command name for full documentation.
     Command name abbreviations are allowed if unambiguous.
 
 As you can see commands are grouped into classes.  Similarly plugin commands as well as
 commands defined in the gdb command language (DEFINE) belong to their own classes.  The
 following enum defines the supported classes.						*/
											
 typedef enum {
     Gdb_Breakpoints,			/* breakpoints (e.g., break, delete, disable)	*/
     Gdb_Data,				/* data	(e.g., print, printf, ptype set)	*/
     Gdb_Files,				/* files (e.g., file, pwd, list)		*/
     Gdb_Internals,			/* internals (private commands)			*/
     Gdb_Obscure,			/* obscure (e.g, load-plugin)			*/
     Gdb_Running,			/* running (e.g., nexti, stepi, run, continue)	*/
     Gdb_Stack,				/* stack (e.g., backtrace, up, down)		*/
     Gdb_Support,			/* support (e.g, set, show)			*/
     Gdb_Public,			/* user-defined (e.g., define)			*/
     Gdb_Private,			/* commands that don't show in help		*/
     Gdb_Too_Many_Classes		/* gdb_define_class() error return		*/
 } Gdb_Cmd_Class;									/*

 There are 10 predefined classes representing most of the classes gdb displays.  When
 a command is defined you need to decide into which class it belongs because when a
 user types, for example HELP data all the commands of that class will be listed.
 
 Besides the above fixed classes you can define your own!  A new class, distinct from
 those above is created by calling gdb_define_class().  It would then be listed by gdb
 in its HELP class list just like its own.
 
 Whether it's one of the fixed classes or one of your own, you need to pass it to 
 gdb_define_cmd() which is used to define all plugin commands.  You may also use
 gdb_change_class() to change the class of a command after it's defined.
*/

/*--------------------------------------------------------------------------------------*/
			      /*--------------------------*
			       | Initialization and Setup |
			       *--------------------------*/

int gdb_initialize(void);
    /* You MUST call this function before using any of the other gdb support routines.
       Returns 1 if the initialization succeeds, 0 if it fails. */

Gdb_Cmd_Class gdb_define_class(char *className, char *classTitle);
    /* Defines a new plugin command class that is NOT one of the predefined classes OR
       converts an existing plugin command into a class.  The function returns the new
       class value if the command did not previously exist or an existing plugin's class,
       now converted into a class.
 
      The classTitle should be a short description to display with the category.  Follow
      the style that gdb uses.  For example, when HELP shows the category for "data",
      it's help is simply "Examining data".
 
      Internally even classes are represented as commands (just ones that cannot be
      executed).  That is why there is a relationship between plugin commands and
      classes.  The only real reason you might want to redefine an exiting (plugin)
      command as a class is when you want to use the command's name as a class category
      for the help display.  Then after the display you would convert it back to a plugin
      command again the way it was created in the first place, i.e., by calling
      gdb_define_cmd().  Of course you need to intercept the HELP command to do this. 
      The gdb_replace_command() call is used to define command intercepts so that is
      relatively easy.
 
      Note, there is a limit of 50 user defined classes (why would you ever define so
      many?).  After that the function returns Gdb_Too_Many_Classes.
 
      Also note that the new class values returned are still typed as Gdb_Cmd_Class even
      though they are outside the enum range. */

void gdb_change_class(char *theCommand, Gdb_Cmd_Class newClass);
    /* Called to change to class of an exiting command.  Nothing happens if the command
       does not exist.
 
       Generally you never need this if you define your own plugins since you define the
       class when you define the plugin.  But if you have commands written in gdb command
       language with DEFINE then gdb classes those as "user-defined" (Gdb_Public).  For
       those you need to call this function to change their class to your desired class.
       Further you obviously must make sure that the plugins are loaded (via LOAD-PLUGIN)
       after the DEFINE commands, in the .gdbinit script (or one sourced during that time)
       so that the commands exist by the time this routine is called. */

void gdb_fixup_document_help(char *theCommand);
    /* Help info for gdb script commands is defined between a DOCUMENT and its matching
       END. It is a "feature" of gdb's handling of DOCUMENT help to strip all leading
       spaces from the help strings as they are read in by gdb.  It also removes all
       blank lines or lines which consist only of a newline.  This makes formatting such
       help information somewhat of a pain!  The work around it is to use a leading
       "significant space", i.e., option-space, which is 0xCA in Mac ASCII.  Then the
       help strings are displayed as desired.
 
       The problem with using option-spaces for a gdb script command's help is that if
       the help is displayed in any other context other than a xterm terminal (e.g.,
       Project Builder) the display might (and for Project Builder, does) explicitly
       show the "non-printable" characters as something unique.  Thus
       gdb_fixup_document_help() is provided to replace all the option-spaces with actual
       spaces after the script commands are defined and their help read by gdb.
 
       Call gdb_fixup_document_help() for each script command whose DOCUMENT help was
       formatted using option-spaces.  If the command is defined and has DOCUMENT help
       it's options-spaces will be replaced with real spaces.  Undefined commands or
       commands with no help info are ignored. */

void gdb_define_cmd(char *theCommand, Gdb_Plugin plugin, Gdb_Cmd_Class itsClass,
		    char *helpInfo);
    /* The command name (theCommand), its class category (itsClass), and it's help info
       (helpInfo) are used to (re)define a plugin command.  A pointer to the plugin's
       implementation is passed in the plugin function pointer.
 
       The command's class defines it's help class.  It any one of the predefined values
       from the Gdb_Cmd_Class enum or one generated by gdb_define_class().  Plugin
       commands can be redefined or they can be used to change a class into a command
       (see gdb_define_class() for more details).
 
       Note, the help info's first line is brief documentation; remaining lines form,
       with it, the full documentation.  The first line should end with a period.
       The entire string should also end with a period, not a newline. */

void gdb_define_cmd_alias(char *theCommand, char *itsAlias);
    /* Gdb allows special abbreviations for commands (e.g., gdb's "ni" is a alias for
       "nexti").  By calling gdb_define_cmd_alias() you can define aliases for a
       command as well. It's intended for defining aliases for commands defined by
       gdb_define_cmd() but there's nothing to prevent defining aliases for existing
       gdb commands as well.  As the grammar here implies ("aliases") you can defined
       more than one alias for the same command.
 
       Note the command class for the alias is always made the same as the class of the
       command to which it's aliased. */

void gdb_enable_filename_completion(char *theCommand);
    /* Allow filename completion for the specified command.  Generally the default is
       symbol completion for most commands (there are a few that have their own unique
       completion, e.g., ATTACH).  Defining a command with gdb_define_cmd() defaults to
       symbol completion.  By calling gdb_enable_filename_completion() you are saying
       that the specified command has a filename argument and that filename completion
       should be used.
 
       Note, completion refers to the standard shell behavior of attempting to finish a
       word when a tab is entered or show all possible alternatives for that word. */

Gdb_Plugin gdb_replace_command(char *theCommand, Gdb_Plugin new_command);
    /* Replace an existing command specified by theCommand (string) with a new
       replacement command (new_command) if it is not NULL.  The pointer to the "old"
       command is returned as the function result (thus passing NULL for the new_command
       is a way to find the address of the specified command plugin).  NULL is returned
       and an error reported if theCommand does not exist.  This allows for the
       enhancement of existing commands with additional functionality or simply to get
       the address of an existing command. */

char *gdb_replace_helpinfo(char *theCommand, char *newHelp);
    /* The newHelp string replaces theCommand's current help string.  The pointer to the
       now previous help string is returned.  If the newHelp is specified as NULL then
       only a pointer the command's help string is returned.  If NULL is returned then
       either the command didn't exist or didn't have any helpInfo to begin with.
       
       Note, you cannot assume the returned pointer is to malloc'ed space for prexisting
       gdb commands.  For commands you create with gdb_define_cmd() you know how the
       helpInfo is allocated and can take appropriate actions. */

void **gdb_private_data(char *plugin_name);
    /* It is possible that there could be multiple instances of the plugin support
       library and that cooperating instances might want to "talk" to each other.
       Instance data is distinct so they have no direct way to pass data between them
       unless they can store it in gdb itself which is common to all the instances. 
       Gdb_private_data() is provided for this purpose.
 
       A plugin is identified by an (arbitrary) name and associates a block of global
       data with that name.  The pointer to the data is established in gdb by calling
       gdb_private_data() with the name.
       
       Each call to gdb_private_data() with the same name from any library instance
       returns the same pointer to a pointer uniquely associated with the name.  
       Initially the pointer value is NULL so the first instance to establish it should
       probably allocate (malloc'ed space only) the private data to be used among the
       other instances.
       
       Once the pointer to the data is known each instance can save it privately instead
       of recalling gdb_private_data() although that of course would also work.
       
       In addition to "private" data pointers there is one global data pointer that can
       be used among the library instances.  The pointer to this pointer is returned by
       gdb_private_data() when a null name (string or pointer) is passed.
       
       It should be pointed out that multiple instances of the plugin support library are
       not totally independent of one another.   See Implementation Considerations at the
       end of this header for further details. */
       
/*--------------------------------------------------------------------------------------*/
                      /*-----------------------------------------*
                       | Command Hook Handling (e.g., hook-stop) |
                       *-----------------------------------------*/

GDB_HOOK *gdb_replace_command_hook(char *theCommand, Gdb_Plugin new_hook, char *helpInfo,
				   int hookpost);
    /* A command hook in the gdb command language is defined as follows:
 
    	  DEFINE hook-foo
    	  ...gdb commands that define the hook...
    	  END
    
       where foo is some other DEFINE command.
 
       When a hook is defined gdb will call the hook before the command itself.  In the
       above example hook-foo is called before foo itself.
 
       The gdb_replace_command_hook() function allows you to define a plugin in place of,
       using the above example, hook-foo.  Pass the original command name (e.g., "foo")
       for which the hook will be established.  Also pass any help info (hooks are like
       any other commands) and a pointer to the command's plugin implementation which has
       the same prototype as any other plugin.  The hookpost parameter should be passed
       as 0 (see below).
 
       Starting with GDB 5.x post-command hooks are also allowed, i.e., hooks which are
       called immediately after a command.  Using the above example,
 
    	  DEFINE hookpost-foo
    	  ...gdb commands that define the hook...
    	  END
    
       Defining one of these is the same as defining a pre-command hook except that
       hookpost should be passed as non-zero.
  
       The function returns an anonymous pointer representing the hook data. This pointer
       should be passed to gdb_execute_hook() to execute any preexisting hook and to 
       gdb_remove_command_hook() to remove the plugin as a hook.  See those routines for
       further details. */

void gdb_execute_hook(GDB_HOOK *hook);
    /*  The anonymous pointer returned from a gdb_replace_command_hook() may be passed
        here to execute the hook (if any) that was defined previously to the
        gdb_replace_command_hook() call.  Thus both a .gdbinit user defined hook and a
        plugin hook can be executed if the plugin calls this routine.  There is no way
        for the caller to know whether there is a preexisting hook.  This routine checks
        that and of course simply returns if there isn't one. */

void gdb_remove_command_hook(GDB_HOOK *hook);
    /* The anonymous pointer returned from a gdb_replace_command_hook() may be passed
       here to remove the hook previously created by gdb_replace_command_hook().  Any
       user defined hook that was in effect prior to the gdb_replace_command_hook()
       call is reestablished as the primary hook for its associated command. */

/*--------------------------------------------------------------------------------------*/
		     /*-------------------------------------------*
                      | Extensions to Gdb's SET and SHOW Commands |		     
		      *-------------------------------------------*/

/* The following defines the kinds of values that are allowed for gdb_define_set(),
   gdb_define_set_enum(), and gdb_define_set_generic().  See gdb_define_set() comments
   for further details. */

typedef enum {        /* value                    meaning				*/
    Set_Boolean,      /* &int		          "on", "1", "yes", "off", "0", "no"	*/
    Set_UInt0,        /* &int			  any unsigned int, 0 yields UINT_MAX	*/
    Set_Int0,	      /* &int			  any int, 0 yields INT_MAX		*/
    Set_Int,	      /* &int			  any int, 0 is treated as value 0	*/
    Set_String,	      /* &(char*)malloc'ed space  sequence of chars, escapes removed	*/
    Set_String_NoEsc, /* &(char*)malloc'ed space  sequence of chars, escapes retained	*/
    Set_Filename,     /* &(char*)malloc'ed space  a filename				*/
    Set_Enum	      /* &(char *)		  one of a specified set of strings	*/
} Gdb_Set_Type;

typedef void (*Gdb_Set_Funct)(char *theSetting,	/* SET handler function prototype	*/
                              Gdb_Set_Type type, void *value, int show, int confirm);
    /* A function following this prototype is passed to gdb_define_set(),
       gdb_define_set_enum(), and gdb_define_set_generic().  Such functions allow you
       to specially handle gdb SET and SHOW commands for your OWN settings.
	 
       The SET function should have the following prototype:
	 
	 void sfunct(char *theSetting, Gdb_Set_Type type, void *value, int show,
	             int confirm);
	 
       where theSetting = the SET/SHOW setting being processed.
	     type       = the type of the value described for gdb_define_set().
	     value      = pointer to the value whose form is a function of the type.
	     show       = 0 if called for SET and 1 for SHOW.
	     confirm	= 1 if SET/SHOW is entered from terminal and SET confirm on.
	     
       The value is usually for convenience and of course unnecessary if you associate
       unique sfunct's with unique settings.  If you use an sfunct for more than one
       setting then theSetting and type can be used to interpret the meaning of the
       value. 
       
       Note that you do not have to specify a sfunct at all.  If you pass NULL then
       the data value points to will be set by SET and shown by SHOW.  In many cases
       this may be all that is necessary.
       
       Please read the gdb_define_set() comments below to understand the type/value
       associations. */

void gdb_define_set(char *theSetting, Gdb_Set_Funct sfunct, Gdb_Set_Type type,
		    void *value_ptr, int for_set_and_show, char *helpInfo);
    /* This provides a way to extend the gdb SET to include your own settings and to
       allow SHOW to display your current settings.  A gdb SET command has the general
       form,
       
         SET <theSetting> argument(s...)
       
       and SHOW,
       
         SHOW <theSetting>
	 
       also HELP can display the SET's help info for <theSetting>,
       
         HELP SET <theSetting>
	 
       Thus gdb_define_set() allows you to include your settings in these commands.
       
       The gdb_define_set() parameters are:
       
       	 theSetting	   The keyword associated with the desired setting.
	 
	 sfunct		   The function to handle the SET and/or SHOW operation in some
	                   specialized way not already provided by gdb.  See description
			   for type below.  This function will be described above where
			   the Gdb_Set_Funct typedef for its defined.  Specifying NULL
			   for this parameter means that only the setting is to be 
			   recorded by SET.
			   
	 type		   A Gdb_Set_Type enum indicating what kind of argument to expect
	                   and what the meaning of the value_ptr.  The expected form of
			   the SET arguments as a function Gdb_Set_Type is defined as
			   follows:

			   Set_Boolean       "on", "1", "yes", "off", "0", "no"
			   Set_UInt0         any unsigned int, 0 yields UINT_MAX
			   Set_Int0	     any int, 0 yields INT_MAX
			   Set_Int	     any int, 0 is treated as value 0
			   Set_String	     sequence of chars, escapes removed
			   Set_String_NoEsc  sequence of chars, escapes retained
			   Set_Filename      a filename
			   Set_Enum	     one of a specified set of strings

	 value_ptr	   This is a POINTER to a object that will receive the value of
	                   the SET argument.  The object's type is a function of the
			   Gdb_Set_Type.
			   
			   Gdb_Set_Type's Set_Boolean, Set_UInt0, Set_Int0, and Set_Int
			   all require the value_ptr to be a pointer to an (unsigned)
			   int.  When the SET command is executed the value is set
			   according to the argument and summarized in the comments above
			   for each Gdb_Set_Type and described some more as follows:
			   
			   Set_Boolean	Value is 0 or 1 depending on the setting.
			   
			   Set_UInt0	     Value is an unsigned int but if a value of 0
			                     is entered for the setting, the value is set
					     to UINT_MAX to indicated "unlimited".
			  
			   Set_Int0	     Same as Set_UInt0 except the pointer is to a
			                     signed int and a 0 setting causes the value
					     to be set to INT_MAX.
			   
			   Set_Int	     Value is an int, 0 has no special meaning
			                     and is treated like any other integer
					     setting.
			   
			   Gdb_Set_Type's Set_String, Set_String_NoEsc, and Set_Filename
			   all require value_ptr to be a pointer to a char * (i.e., a
			   char **).  The SET argument is a sequence of characters
			   and that sequence is gdb_malloc()'ed and the pointer to it
			   stored where value_ptr points.  The SET arguments form depends
			   on the Gdb_Set_Type:
			   
			   Set_String	     Any sequence of characters with escaped
			                     characters processed.
			                     
			   Set_String_NoEsc  A sequence of characters stored verbatim,
			   		     i.e., escaped are not processed.
					     
			   Set_Filename      A pathname.
			   
			   Note that the pointer to the string pointer must initially
			   be NULL or to gdb_malloc()'s space since the old space will
			   be gdb_free()'ed when replaced with a new setting.
			   
			   Gdb_Set_Type Set_Enum is special in that it will only appear
			   in the sfunct when the sfunct is called as a result of
			   gdb_define_set_enum().  See it's comments for further details.
			   
	 for_set_and_show  Normally the sfunct is only called when a SET command is
	                   done.  But by setting for_set_and_show to a non-zero value
			   it will also be called for SHOW as well.  The sfunc has
			   a parameter indicating why it's being called (see below).
	 
	 helpInfo          This is a short help info to be used for HELP SHOW and
	                   when SHOW displays the current setting.  THE HELP INFO STRING
			   MUST BEGIN WITH THE SEQUENCE OF CHARACTERS "Set " EXACTLY.  If
			   helpInfo is passed as NULL, then a generic help is created as
			   a function of the type:
			   
			   Set_Boolean                 "Set <theSetting> on | off"
			   Set_UInt0/Set_Int0/Set_Int  "Set <theSetting> n"
			   Set_String/Set_String_NoEsc "Set <theSetting> string of chars..."
			   Set_Filename                "Set <theSetting> filename"
			   Set_Enum                    "Set <theSetting> enum1|enum2..."
			   
			   where <theSetting> is replaced with the passed setting name
			   and enumN is gdb_define_set_enum()'s enum list.
			   
			   Note, keep the help show since it is used by gdb in the SHOW
			   and HELP SET (or HELP SHOW) commands.
	 
	 Finally, one last point -- do not confuse these SET operations with the SET
	 variable (e.g., SET $i = 1") operations.  They are NOT handled here.  If you
	 need to intercept the SET variable command you still need to use 
	 gdb_replace_command(). */

void gdb_define_set_enum(char *theSetting, Gdb_Set_Funct sfunct, char *enumlist[],
			 void *value_ptr, int for_set_and_show, char *helpInfo);
    /* This is almost identical to gdb_define_set() above except that in place of the
       type, a pointer to a NULL terminated list of acceptable string pointers is
       expected.  For example,
       
         gdb_define_set_enum("example", sfunct, valid_settings, 0, "Set UP | DOWN");
       
       where, 
       
         char *valid_settings[] = {"UP", "DOWN", NULL};
	 
       The list should contain all unique items and the matching of the SET argument is
       case sensitive.
      
       When sfunct is called it's type parameter will be set to Set_Enum to indicate that
       the value is a pointer to a pointer to one of the valid_settings strings. */
			 
void gdb_define_set_generic(Gdb_Set_Funct sfunct);
    /* You can specify a "generic" sfunct to catch ALL SET (not SHOW) operations after
       gdb processes it.  The sfunct is as described above but unlike the sfunct's
       associated with a specific setting, the generic function will filter all SET
       setting operations.  As usual though the sfunct's theSetting and type parameters
       can be used to interpret the value.  Since this function is only called for SET
       the sfunct show parameter will always be 0. */

/*--------------------------------------------------------------------------------------*/
                             /*--------------------------*
                              | Gdb Exit (Quit) Handling |
                              *--------------------------*/

void gdb_define_exit_handler(Gdb_Exit_Handler theHandler);
    /* This always you to specify a handling routine to get control just before gdb is
       about to exit.  The handler should have the following prototype:
 
  	  void handler(void);
 
       Note, that you can still gdb_replace_command() to replace the gdb QUIT command.
       But that intercepts quit BEFORE that command executes allowing you to do anything
       special in place of the quit command.   On the other hand, specifying a exit
       handler here will cause that exit handler to be called when gdb is truly about
       to quit. */

/*--------------------------------------------------------------------------------------*/
			      /*---------------------------*
			       | Gdb Command Line Routines |
			       *---------------------------*/

void gdb_eval(char *expression, ...);
    /* The string specified by the expression is parsed and evaluated by gdb.  This
       can be used for assignment expressions instead of using the full command parser
       with gdb_execute_command().  For example, gdb_eval("$a=2") as opposed to
       gdb_execute_command("set $a=2").
 
       Note that the arguments to gdb_eval() are the same as for a sprintf() allowing
       you to format the expression with the call. */

void gdb_execute_command(char *commandLine, ...);
    /* Execute a complete gdb command line.  A complete gdb command line is passed as
       if it were entered into gdb itself.
 
       Note that the arguments to gdb_execute_command() are the same as for a sprintf()
       allowing you to format the command with the call.  Note, be careful how you
       specify printf formats if you need to execute a PRINTF command since the
       command line acts as a format string to gdb_execute_command().  In other words
       if commandLine has any %-formatting for the printf you'll need to double the
       %'s to keep gdb_execute_command() from using them. */

int gdb_eval_silent(char *expression, ...);
    /* This is identical to gdb_eval() except that any errors due to evaluation are
       suppressed. The function returns 1 if there were suppressed errors and 0 if the
       evaluation succeeded. */

int gdb_execute_command_silent(char *commandLine, ...);
    /* This is identical to gdb_execute_command() except that any errors due to execution
       of the command are suppressed.   The function returns 1 if there were suppressed
       errors and 0 if the execution succeeded. */

/*--------------------------------------------------------------------------------------*/
			        /*---------------------*
				 | GDB Status Routines |
				 *---------------------*/

int gdb_target_running(void);     
    /* If a RUN command has been issued and the target program has not yet completed 1 is
       returned.  Otherwise 0 is returned.  This allows you to control initialization and
       termination sequences and to permit operations which only make sense while the
       target is being run (e.g., accessing the target's registers). */

int gdb_target_pid(void);
    /* If the inferior is running it's pid is returned otherwise -1 is returned. */

int gdb_have_registers(void);
    /* Returns 1 if the target register values are available and 0 otherwise. */

int gdb_is_command_defined(char *theCommand);
    /* Returns 1 if the specified command is defined and 0 if it isn't. */

char *gdb_get_prompt(char *prompt_buffer);
    /* The buffer is assumed large enough to hold the prompt string and it is also
       returned as the function result. */
       
int gdb_interactive(void);
    /* Return 1 if the current input is coming from stdin.  Note this is generally not
       needed since the second argument of plugins indicates that same information. */

GDB_ADDRESS gdb_get_function_start(GDB_ADDRESS addr);
    /* Returns the address of the start of the function containing the specified
      address or NULL if the start address cannot be determined. */

char *gdb_address_symbol(GDB_ADDRESS addr, int onlyAddr, char *symbol, int maxLen);
    /*  Called to convert an address to s symbol.  The function returns the pointer to
        the symbol string possibly truncated to maxLen characters.  If onlyAddr is
        non-zero then addr is simply converted to a hex value ("0xXXXX....").  If
        onlyAddr is 0 then the symbol information associated with the addr is appended
        to the string.

 	NULL is never returned from this function.  At a minimum the hex address is
 	returned.  The symbol information is not appended if it cannot be determined.

        The full output formats are as follows:
 
          0xXXXX... <name[+offset] in filename>
          0xXXXX... <name[+offset] at filename:line> */

int gdb_target_arch(void);
    /* Return 8 for a 64-bit target architecture otherwise return 4.  This is used to
       know whether the inferior was compiled for a 64 ot 32 bit architecture. */
    
/*--------------------------------------------------------------------------------------*/
			   /*-------------------------------*
			    | Convenience Variable Routines |
			    *-------------------------------*/

void gdb_set_int(char *theVariable, int theValue);
    /* set $theVariable = (int)theValue */

void gdb_set_long(char *theVariable, long theValue);
    /* set $theVariable = (long)theValue */

void gdb_set_long_long(char *theVariable, long long theValue);
    /* set $theVariable = (long long)theValue */

void gdb_set_double(char *theVariable, double theValue);
    /* set $theVariable = (double)theValue */

void gdb_set_string(char *theVariable, char *theString);
    /* set $theVariable = (char *)theString.
    
       Note, unlike gdb_set_int() and gdb_set_double() strings are stored by gdb in
       the target's memory.  That means the target has to be running when this call
       is done. Sorry, but that's also the rule for using strings in the gdb command
       language as well. */

void gdb_set_address(char *theVariable, GDB_ADDRESS theValue);
    /* set $theVariable = (GDB_ADDRESS)theValue */

int gdb_get_int(char *expression);
    /*  Returns the int value of the specified expression. */

long gdb_get_long(char *expression);
    /*  Returns the long value of the specified expression. */

long long gdb_get_long_long(char *expression);
    /*  Returns the long long value of the specified expression. */

double gdb_get_double(char *expression);
    /*  Returns the double value of the specified floating point expression. */
    
char *gdb_get_string(char *theVariable, char *str, int maxlen);
    /* This is used to get the value of the specified convenience variable as a string.
       If the variable is undefined "" is returned.  Up to maxlen characters are copied
       to the specified string buffer (str). */

GDB_ADDRESS gdb_get_address(char *expression);
    /*  Returns the GDB_ADDRESS value of the specified expression. */

int gdb_is_var_defined(char *theVariable);
    /* Returns 1 if the specified convenience variable is defined and 0 if it is not. */       

/*--------------------------------------------------------------------------------------*/
		   /*----------------------------------------------*
		    | Direct Register and PC Manipulation Routines |
		    *----------------------------------------------*/

char *gdb_set_register(char *theRegister, void *value, int size);
    /* Set the value of theRegister (e.g., "$r0") from the size bytes in the specified
       value buffer.  The size must match the size of the register.  NULL is returned if
       the assignment is successful.  Otherwise the function returns a pointer to an
       appropriate error string.
 
       The following errors are possible:
    
	 no registers available at this time
	 no frame selected
	 bad register
	 invalid register length
	 left operand of assignment is not an lvalue
	 could not convert register to internal representation
    
       Note, that it is recommended that the more general gdb_set_int() be used for
       32-bit registers.  The gdb_set_register() routine is intended mainly for setting
       larger register data types like the AltiVec 16-byte registers. */

void *gdb_get_register(char *theRegister, void *value);
    /* Returns the value of theRegister (e.g., "$r0") in the provided value buffer.  The
       value pointer is returned as the function result and the value is copied to the
       specified buffer (assumed large enough to hold the value and at least a long long).
       If the register is invalid, or its value cannot be obtained, NULL is returned and
       the value buffer (treated as a long* pointer) is set with one of the following
       error codes:
											*/
       typedef enum {
	  Gdb_GetReg_NoRegs = 1,	/* no registers available at this time		*/
	  Gdb_GetReg_NoFrame,		/* no frame selected				*/
	  Gdb_GetReg_BadReg,		/* bad register (gdb doesn't know this register)*/
	  Gdb_GetReg_NoValue,		/* value not available				*/
       } Gdb_GetReg_Error;
       											/*
       Always use this function instead of, say, gdb_get_int(), to get register values
       because (a) it is more efficient (not expression evaluation is done) and (b) it
       is more accurate in that GDB might not be in the proper context to get the 
       current register frame value. */

GDB_ADDRESS gdb_get_sp(void);
    /* Returns the value of the stack pointer. */

int gdb_get_reg_size(int regnum);
    /* For 64-bit register value support; return the size of register regnum.  For
       generality we don't assume all the sizes are the same.  Hence the regnum
       argument. */

/*--------------------------------------------------------------------------------------*/
			     /*----------------------------*
			      | Target Memory Manipulation |
			      *----------------------------*/
    
GDB_ADDRESS gdb_read_memory(void *dst, char *src, int n);
    /* The n bytes in the target's memory represented by the src expression *string*
       are copied to the plugin memory specified by dst.  The target actual address is
       returned as the function result. */

GDB_ADDRESS gdb_read_memory_from_addr(void *dst, GDB_ADDRESS src, int n, int report_error);
    /* The n bytes in the target's memory at addr are copied to the plugin memory
       specified by dst.  The target address is returned as the function result.
       If an error is detected while reading NULL is returned if report_error is 0.
       Otherwise an error message is displayed.  */

void gdb_write_memory(char *dst, void *src, int n);
    /* The n bytes from the (plugin) src are written to the target memory address 
       represented by the dst expression *string*. *.

void gdb_write_memory_to_addr(GDB_ADDRESS dst, void *src, int n);
    /*  The n bytes from the (plugin) src are written to the target memory address 
        represented by the dst expression value. */

/*--------------------------------------------------------------------------------------*/
				   /*----------------*
				    | Print Routines |
				    *----------------*/
       
void gdb_printf(char *fmt, ...);
    /* Generate a printf to the current (redirected) stdout. */
 
void gdb_vfprintf(GDB_FILE *stream, char *fmt, va_list ap);
    /* Same as gdb_printf() except this one takes a va_list for the format values. */

void gdb_puts(char *s);
    /* Equivalent to gdb_printf("%s", s) */

void gdb_fprintf(GDB_FILE *stream, char *fmt, ...);
    /* Generate a fprintf to the specified stream.  The current (redirected) stream is
       not used for this call and should be used when the caller does have control over
       the output stream (as opposed to, say using gdb_execute_command() where gdb may do
       the output.
 
       Note that even if output is currently redirected with gdb_redirect_output() for
       gdb command output you can still use gdb_fprintf() for your own GDB_FILE streams
       that are not currently redirected and use their filters.  This allows, for
       instance, to chain output filters (this discussions assumes you read the comments
       on redirected output first).  For example, say you redirect output as follows
       (assume GDB_FILE "stream1" is already open and redirected to "filter1"):
       
	      GDB_FILE *previous_stream;
	      
	      stream2 = gdb_open_output(stdout, filter2, &previous_stream);
	      previous_stream = gdb_redirect_output(stream2);
	      
	      ...stuff that causes gdb to do prints...
	      
	      gdb_close_output(stream2);
	      
       In filter2 (a prototype of the form gdb_output_filter_ftype) you use it's data 
       parameter as a GDB_FILE stream then doing gdb_fprintf()'s to that stream will cause
       that output to go through filter1.  Further, when you gdb_close_output(stream2) 
       the output redirection will again revert back to using filter1 assuming it was the
       redirected stream in effect when you did the gdb_open_output().
       
       There's no limit on the cascading of the filters using this technique if you
       define the data to hold multiple GDB_FILE's that are passed among a set of
       cooperating output filters all expecting that same data. */
 
void gdb_vprintf(char *fmt, va_list ap);
    /* Same as gdb_fprintf() except this one takes a va_list for the format values. */
 
void gdb_vfprintf(GDB_FILE *stream, char *fmt, va_list ap);
    /* Same as gdb_fprintf() except this one takes a va_list for the format values. */

void gdb_fputs(char *s, GDB_FILE *stream);
    /* Equivalent to gdb_fprintf(stream, "%s", s) */
     
void gdb_print_address(char *address);
    /* Display the address and function corresponding to an address (expression or
       file/line specification).  If the address corresponds to a source line, the
       file/line information along with the source line are also displayed.  Otherwise
       the load segment information, which includes the segments type, encompassing
       address range, and, depending on section type, either the load address or
       pathname, are displayed.
 
       Note, if source lines are displayed they are shown exactly as if a LIST was done
       of it.  Thus it is listed in a context of N lines, where N is determined by the
       gdb SET listsize command.
 
       The SET listsize command set the gdb global lines_to_list which we access here. */

int gdb_query(char *fmt, ...);
    /* Display a query like gdb does it.  The arguments are exactly the same as for
       printf().  The function returns 1 if the answer is "yes".  The formatted message
       should end with a "?".  It should not say how to answer, because gdb does that. */
    
/*--------------------------------------------------------------------------------------*/
				  /*-----------------*
				   | Error Reporting |
				   *-----------------*/
    
void gdb_error(const char *fmt, ...);
    /* Report an error from a command.  Gdb reports the error and does not return to
       the caller. */

void gdb_verror(const char *fmt, va_list ap);
    /* Same as gdb_error() above except that this function accepts a va_list. */
    
void gdb_internal_error(char *msg);
    /* Report an internal error the same way gdb does.  This function does not
       return.  Note only a single string may be passed, not a format with values 
       like gdb_error(). */
       
/*--------------------------------------------------------------------------------------*/
				  /*-----------------*
				   | I/O Redirection |
				   *-----------------*/

typedef char *(*gdb_output_filter_ftype)(FILE *f, char *line, void *data);
    /* A function following this prototype is passed to gdb_open_output() to filter all
       redirected output written by gdb to stdout or stderr (indicated by f).  The
       function can return the (possibly same) line as it's function result or NULL.
       NULL implies that the line is not to be output or that the filter did the display.
       
       Note that all lines except possibly the last include the terminating '\n'.  The
       last may not have one if there is a partially built line at the time the stream 
       is flushed.
 
       If the line pointer is NULL, then that indicates to the filter that it's output is
       being flushed.  All filters MUST check for line == NULL.
 
       The data parameter passed to gdb_open_output() is passed to the filter as is. 
       This allows the caller to have a communication channel between the
       gdb_open_output() caller and it's filter. */
              
GDB_FILE *gdb_open_output(FILE *f, gdb_output_filter_ftype filter, void *data);
    /* Creates a new output stream pointer for stdout or stderr (specified by f).  When
       the output is redirected by calling gdb_redirect_output() all the stdout or stderr
       writes will be filtered by the specified filter with the following prototype:
  
	 char *filter(FILE *f, char *line, void *data);
  
       The filter can return NULL or the filtered line. NULL implies that the line is not
       to be output or that the filter did the output.
  
       Note that all lines except possibly the last include the terminating '\n'.  The
       last may not have one if there is a partially built line at the time the stream is
       flushed.
  
       If the line pointer is NULL, then that indicates to the filter that it's output is
       being flushed.
  
       The data parameter passed to gdb_open_output() is passed to the filter as is.  This
       allows the caller to have a communication channel between the gdb_open_output()
       caller and it's filter.
 
       The function returns a output stream (GDB_FILE) pointer for passing to
       gdb_redirect_output() and gdb_close_output().  Note that this is NOT a stdio FILE*
       stream pointer so it cannot be used in stdio.h routines.  Indeed it can ONLY be
       used as a parameter to the redirect and close routines just mentioned (see their
       comments for details).
  
       Two GDB_FILE are initially provided; gdb_default_stdout and gdb_default_stderr
       which can be passed to gdb_redirect_output() and gdb_close_output().  These
       represent the stdout and stderr streams the gdb normally uses.  YOU SHOULD NEVER
       STORE INTO THESE POINTERS.
  
       If no filter is passed (i.e., NULL is passed) to gdb_open_output() then
       gdb_default_stdout or gdb_default_stderr (depending on f) is returned as the
       function result.  In other words, while these streams are already open, no harm
       is done by attempting to explicitly reopen them.
       
       Caution - you must be cognizant of which output to redirect if you intend to use
       other gdb commands (e.g., x/i) whose output you are redirecting.  As a general
       rule stdout is usually the stream to be redirected.  Further, since the filter is
       called for entire lines (except possibly for the last) you must ensure all output
       always ends with a new line (\n). */

GDB_FILE *gdb_redirect_output(GDB_FILE *stream);
    /* Causes all future output from gdb to be filtered with the filter routine
       associated with the specified file (stream pointer) and sent to stdout or stderr
       also associated with the stream.  These both were specified when the stream pointer
       was created by gdb_open_output().  Redirection continues until another redirection
       is specified.
 
       The function returns the redirection stream that was in effect at the time of the
       call (i.e., the "old" or previous stream).  
   
       Conceptually, the initial "redirection" state is for gdb_default_stdout for stdout
       and gdb_default_stderr for stderr.  Passing these to gdb_redirect_output() causes
       gdb to use its standard default output machinery (i.e., as if no redirection was
       ever done).  Of course no filtering is done either.
  
       There is no explicit stream specification in gdb output.  The gdb echo and printf
       commands all output to stdout.  Indeed so do all other gdb output commands (e.g.,
       x).  Thus redirection done here affects the behavior of all gdb output commands.
       That is what dictated this form of design.
  
       If output is being redirected and thus filtered, there is nothing prohibiting that
       filter from temporarily redirecting its own output to another (possibly already
       opened) stream and thus using another filter to further filter the output.
  
       For example, output could be handled by a stream that does its own screen drawing.
       A stream could also be set up to filter gdb disassembly output to reformat it.
       The disassembly filter could then take each reformatted line and temporarily set
       up redirection to print to the screen stream for placement on the screen.
 
       Although the gdb command set does not provide a way to specify stdout or stderr
       the plugin ABI does.  The gdb_printf() and gdb_error() functions are provided to
       output to the current stdout and stderr redirections respectively.  Also provided
       is gdb_fflush() to force flushing of a specified stream. 
       
       Note: Due to the way gdb handles output internally, if both stdout and stderr are
       redirected then it is recommended that stderr be redirected before stdout.
       Almost all gdb output is to stdout streams.  Internally gdb has its own stream
       variables which are modified by this call.  There's only one instance of these
       streams.  Thus the preference should always be to associate the streams with 
       stdout.  Hence it should be the most recent steam that is redirected.
 
       See also comments for gdb_special_events() for the Gdb_Word_Completion_Cursor
       GdbEvent.  That event is for saving and restoring the word completion (i.e.,
       displaying list of alternative names for commands, filenames, etc. or displaying
       all alternatives) input cursor when output is redirected and needs some
       specialized input cursor handling.
       
       An example illustrating a use for redirection is discussed in the comments for
       gdb_fprintf(). */
 
void gdb_close_output(GDB_FILE *stream);
    /* This is used to close a stream previously opened by gdb_open_output().  Once
       closed the stream pointer must no longer be passed to gdb_redirect_output().
       The current redirection after closing is the redirection that was in effect when
       this stream was originally opened (note, opened, NOT redirected).
 
       Since gdb_redirect_output() returns the stream that was in effect at the time of
       its call, then if there are no other gdb_redirect_output() calls between
       gdb_open_output() and that gdb_redirect_output() call, then the stream redirected
       to after the close is the same stream returned by gdb_redirect_output().  For
       example,
       
	   new_stream = gdb_open_output(stdout, my_filter, NULL);
	   old_stream = gdb_redirect_output(new_stream);
	   - - -
	   gdb_close_output(new_stream);
	   
       The redirection at this point is to the old_stream since that was also in effect
       at the time the gdb_open_output() was done.  If there are any other redirections
       between opening new_stream and redirecting it then old_stream will not be the same
       as what's redirected to when the gdb_close_output(new_stream) is done. */
 
void gdb_fflush(GDB_FILE *stream);
    /* Makes sure all stream output is written.  The stream pointer should have been
       previously created by gdb_open_output(). */

void gdb_define_raw_input_handler(Gdb_Raw_Input_Handler theInputHandler);
    /* Gdb has a mode where it basically reads raw lines from the terminal (as
       opposed to command lines).  This occurs when a COMMANDS, DEFINE, DOCUMENT, IF,
       or WHILE command is entered from the terminal (as opposed to a script), i.e.,
       interactively.  It also reads control structures (i.e.., WHILE and IF) this way.  
       
       gdb_define_raw_input_handler() allows you to specify an handler to filter the raw
       data lines before gdb saves them as the lines making up the body of the control
       command.  The handler has the following prototype:
       
	 void raw_input_handler(char *theRawLine);
	 
       This allows for handler to look at the lines before gdb sees them and also to
       possibly echo the lines elsewhere in the display.  Whatever...
       
       Only one handler may exist at any point in time.  Specifying NULL as
       theInputHandler reverts back to gdb's original behavior.
       
       Note, that the prompt gdb uses is not the standard prompt.  By default it is a '>'
       appropriately indented to show control structure nesting depth.  This however
       may be changed by calling gdb_set_raw_input_prompt_handler() to define a handler
       which can return the desired prompt.
       
       If the specified stream is associated with a filter that is controlling the
       display wants the prompts in a position other than the normal gdb default then
       the SET prompt can be used to define the standard prompt with xterm terminal
       positioning controls (or whatever is appropriate for the terminal).  But since
       that cannot be done with the raw data line prompt gdb_define_raw_input_handler() 
       is provided to let you get control just before ANY prompt is displayed by gdb
       during the raw input.
	     
       Caution: It appears that gdb has a tendency to reset itself to its own handler
                under some conditions (one known is using CTL-C).  So you may need to
                recall gdb_define_raw_input_handler() to reestablish your raw input
                handler. */

void gdb_set_raw_input_prompt_handler(Gdb_Raw_Input_Set_Prompt thePromptHandler);
    /* This defines a handler that can be used to set the prompt used during raw input.
       The handler has the following prototype:
 
         void thePromptHandler(char *prompt);
 
       Specifying NULL for the thePromptHandler removes the handler and gdb reverts to
       it's standard '>' prompt.  The handler is expected to modify the 256-character
       prompt buffer with the desired prompt.
 
       Caution/Warning: The prompt gdb is using is for raw input is in a 256-character 
       local buffer in gdb,  This is on the call chain so it can be legally (!) accessed.
       But remember it has the 256 limit.  In order to get gdb to use the desired prompt
       that specific buffer must be modified. */
       
void gdb_control_prompt_position(Gdb_Prompt_Positioning positioningFunction);
    /* This routine allows you to specify a routine which will get control just before
       any prompt the gdb displays prior to reading from the stdin command line
       (excluding queries).  While you cannot change the prompt (other than gdb's
       standard prompt with a set prompt command) you can use the positioning function
       to do as that name implies, namely control the position of where the prompt will
       be displayed.  This is useful when using a redirected output where the associated
       filter want's to the prompt in some specific position.
 
       The positioningFunction should have the following prototype:
 
         void positioningFunction(int continued);
   
       The continued parameter is 0 unless the prompt is going to be for a continued
       line, i.e., the input line had a '\' at the end.  In other words, continued is
       the number of lines entered before the upcoming line that have been continued.
       The positioningFunction() might want to use this to adjust the position of the
       prompt.
       
       Specifying NULL for the positioningFunction removes the positioning control.
 
       Note, also see gdb_define_raw_input_handler().  It discusses why you would want
       to use gdb_control_prompt_position() to control the prompt positioning.
 
       It should be pointed out that while it is tempting to use this routine to always
       control the prompt position, it is not recommended for the standard gdb prompt
       which is controlled by a SET prompt command.  That's because apparently gdb uses
       the current standard prompt for its history display and scrolling through that
       display will not appear on the screen where your prompt is positioned if it's 
       positioned separately. */
 
typedef void (*gdb_stdin_preprocess_ftype)(char *commandLine, void *data);
    /* A function following this prototype is passed to gdb_open_stdin() to preprocess
       all gdb stdin command lines.  The preprocess function may look at or change the
       contents of the command line.  By the time the preprocessor sees the line all
       leading blanks and tabs have been removed.
       
       The data parameter is the same value originally passed to gdb_open_stdin().
       This allows the caller to pass additional information to the preprocess
       function. */

typedef void (*gdb_stdin_postprocess_ftype)(void *data);
    /* When a stdin preprocess function is passed to gdb_open_stdin() you may also pass
       a postprocess function which gets called after the stdin command has been
       completed by gdb. */

void gdb_redirect_stdin(gdb_stdin_preprocess_ftype stdin_filter,
		        gdb_stdin_postprocess_ftype stdin_postprocess, void *data);
    /* Causes all future stdin command line stream input be pre and post processed by
       the specified routines (either can be specified as NULL if one or the other is
       not needed).
 
       The preprocessing routine has the following prototype:
 
           void preprocess(char *commandLine, void *data);
     
       The preprocess function may look at or change the contents of the command line
       (you should assume that the command line was malloc'ed so you can change it's
       size and contents if you wish).  By the time the preprocessor sees the line all
       leading blanks and tabs have been removed. 
 
       The postprocessing routine has the following prototype:
 
           void postprocess(void *data);
     
       The data parameter is passed to these routines is the data parameter passed to 
       gdb_open_stdin().  It permits the caller to pass additional information among the
       pre and postprocessors and to communicate back to the gdb_open_stdin() caller.
 
       Only one redirection is active at any one time but nesting of redirections is
       allowed.  A redirection stays in effect until gdb_close_stdin() is called. */

void gdb_close_stdin(void);
    /* Causes stdin command line redirection to revert to the state it had prior to the
       most recent gdb_redirect_stdin() call. */

void gdb_set_previous_command(char *replacement_line);
    /* A null line is entered from a terminal command line tells gdb to repeat the
       previous command.  Calling gdb_set_previous_command() and specifying a command
       line of your own gives you control over what that command is to be.
 
       You generally would use this routine in a gdb_redirect_stdin() preprocess routine
       to control gdb's behavior of your own repeated plugin commands if the desired
       behavior is not the gdb default.  There's nothing stopping you from doing this
       from the preprocess routine directly on the (malloc'ed) command line passed to it.
       Gdb will obediently execute the command AND record it in its command history list.
       You might not want that to happen, i.e., recording your replacement every time the
       user types a return on the command line.
 
       What gdb does for a empty command line however is NOT record it in its command
       history.  Instead it simply uses the previous command line.  By calling
       gdb_set_previous_command() you can change what that previous line is. */
 
char *gdb_get_previous_command(void);
     /* This returns a copy of gdb's previous command line.  This is the line gdb would
        execute when a null line is entered as a command form the keyboard.  The function
        returns a (malloc'ed) pointer to a copy of the line or NULL if it doesn't
	exist. */

/*--------------------------------------------------------------------------------------*/
				 /*-------------------*
				  | Memory Management |
				  *-------------------*/

void *gdb_malloc(int amount);
    /*  Allocate gdb memory.  This function does not return if the memory cannot be
        allocated.  The error is reported and the command aborted. */

void *gdb_realloc(void *p, int amount);
    /* (Re)allocate gdb memory. */
    
void gdb_free(void *p);
    /* Free memory allocated by gdb_malloc(). */

/*--------------------------------------------------------------------------------------*/
			      /*-------------------------*
			       | Miscellaneous Utilities |
			       *-------------------------*/

int gdb_setup_argv(char *s, char *argv0, int *argc, char *argv[], int maxargs);
    /* This builds an argv[] vector from a command line of arguments.  Since each
       plugin command is passed all its arguments as a string, this routine may be
       convenient to break up the line into individual arguments using the standard
       argv/argc conventions.  It is assumed the argv[] array is large enough to hold
       maxargs+1 entries.  The argv[0] entry is set with the argv0 parameter which is
       assumed to be a command name.  However, if argv0 is NULL then argv[0] will
       contain the first word of s.  The argv[argc] entry is always NULL.  The function
       returns argc as its result.
       
       An argv argument is defined here as any sequence of non-blank characters where
       blanks may be contained in singly or doubly quoted strings or paired parenthesis
       or paired square brackets.  Thus (char *)a[i + 1] represents a single argv
       argument.  Also escaped characters (including hex and octal) are permitted.
       
       The argv[] pointers are pointers into the string s.  String s is MODIFIED to
       contain null characters at the end of each argument.  So the caller must assume
       s has the same lifetime as argv[] and is a modifiable string.
 
       Caution: If you pass the command line arg string passed to a plugin then you run
       the risk of modifying gdb's command line.  This is not a problem unless you intend
       to all the plugin command to be reexecuted when the user enters a null command 
       line (i.e., just a return) to reexecute the previous command.  Since null
       delimiters are placed between the args in the string the reexecuted string will
       be effectively truncated to the first argument. */
 
int gdb_strcmpl(char *s1, char *s2);
    /*  Compare two strings for equality (ignoring case).  The function returns 1 if
        the two strings are equal (independent of case) and returns 0 otherwise. */

int gdb_keyword(char *keyword, register char **table);
    /* Find the keyword in the table and return its index as the function result.  The
       table is a list of pointers to all the keywords with a NULL pointer (0) to mark
       the end of the list.  A value of -1 is returned if the keyword is not found in
       the table (case is ignored).  If the keyword is found, the function will return
       the array element index (relative to 0) associated with the found keyword. */

int gdb_is_string(char *expression);
    /* Returns 1 if the expression is a quoted "string" and 0 if it isn't. */

char *gdb_tilde_expand(char *pathname);
    /* Returns an malloc'ed string with is the result of expanding tilde's (~) in the
       specified pathname. */

int gdb_demangled_symbol(char *mangled, char *demangled, int maxLen, int params);
    /* If demangling is enabled in gdb (SET print demangle |asm-demangle) then try see
       if the mangled symbol can be demangled (including is parameters if params is
       non-zero).  The function sets the demangled name (possibly truncated to maxLen
       characters).
 
       If demangling is not enabled or cannot be done the original mangled name is
       copied to mangled (again possibly truncated to maxLen unless both demangled
       and mangled are pointers to the same string).
 
       In all cases the function returns the length of the mangled string.
 
       Note, a convention gdb uses for Mac OS X is to prefix stubs (trampolines) with
       the string "dyld_stub_".  If this is present in the mangled symbol it is
       stripped off and the remaining characters used for the mangled symbol.  If that
       can be demangled the demangled symbol is returned WITHOUT the "dyld_stub_"
       prefix. */

int gdb_show_objc_object(GDB_ADDRESS addr, char *objStr, int maxLen);
    /* Ask an object to display itself into the specified object string (up to maxLen
       characters).  If the string is truncated then '...' is appended to the end of
       the string.  The function returns the number of characters in the string.  0
       is returned if the string cannot be generated.
 
       Note, this function is basically a gdb PRINT-OBJECT (PO) command except that
       the output is returned in the string instead of being written to stdout. */

/*--------------------------------------------------------------------------------------*/
			    /*------------------------------*
			     | Low-level gdb event handling |
			     *------------------------------*/
    
/* The following are the kinds of low-level gdb events that allow callbacks to user-
   defined functions.  See gdb_special_events() comments for details. */

typedef enum {					/* gdb_special_events() events...	*/
    Gdb_Before_Command,				/* intercept gdb command execution	*/
    Gdb_After_SET_Command,			/* generic SET option handling		*/
    Gdb_Before_Query,				/* intercept queries			*/
    Gdb_After_Query,				/* intercept query result		*/
    Gdb_Before_Warning,				/* intercept warnings			*/
    Gdb_After_Creating_Breakpoint,		/* notify when a breakpoint is created	*/
    Gdb_Before_Deleting_Breakpoint,		/* notify when a breakpint is deleted	*/
    Gdb_After_Modified_Breakpoint,		/* notify when a breakpoint is modified	*/
    Gdb_After_Attach,				/* notify when a process is attached	*/
    Gdb_Before_Detach,				/* notify when a process is detached	*/
    Gdb_After_Register_Changed,			/* notify what target reg was changed	*/
    Gdb_After_Memory_Changed,			/* notify when target memory was changed*/
    Gdb_Context_Is_Changed,			/* notify of context change (new pid)	*/
    Gdb_Before_Error,				/* notify that error is being reported	*/
    Gdb_After_File_Changed,			/* notify that a FILE cmd was specified	*/
    Gdb_After_Attach_To_File,			/* notify after attaching to a file	*/
    Gdb_Before_Prompt,				/* notify that prompt is next display	*/
    Gdb_Begin_ReadRawLine,			/* notify of start of raw line read	*/
    Gdb_ReadRawLine,				/* read a raw line			*/
    Gdb_End_ReadRawLine,			/* notify of end of raw line read	*/
    Gdb_State_Changed,				/* notify of a gdb's state change	*/
    Gdb_Word_Completion_Cursor,			/* save/restore word completion cursor	*/
    Gdb_Word_Completion_Query,			/* intercept word completion query	*/
    Gdb_Word_Completion_Read,			/* intercept word completion query read	*/
    Gdb_History_Prompt,				/* intercept readline history prompts	*/
    Gdb_Interactive				/* called while in loops		*/
} GdbEvent;

/* The following define the kinds of state changes that are reported to a
   gdb_special_events() callback assocated with the Gdb_State_Changed GdbEvent. */

typedef enum {					/* State changes for Gdb_State_Changed	*/
  Gdb_Not_Active,				/* gdb is not active (it's exiting)	*/
  Gdb_Active,					/* gdb is becoming active		*/
  Gdb_Target_Loaded,				/* gdb just loaded target program	*/
  Gdb_Target_Exited,				/* target program has exited		*/
  Gdb_Target_Running,				/* target brogram is going to run	*/
  Gdb_Target_Stopped				/* target program has stopped		*/
} GdbState;
   
typedef void (*Gdb_Callback)();
    /* The gdb_special_events() accept a general callback function pointer whose actual
       prototype depends on the GdbEvent's defined above.  The individual prototypes are
       described in the gdb_special_events() comments below.  But in order to pass one
       of the even prototypes to gdb_special_events() you need to type cast it to the
       general form.  Thus Gdb_Callback is defined for that purpose. */

void gdb_special_events(GdbEvent theEvent, void (*callback)());
    /* This is a low-level interface to allow you to be notified when certain events
       occur within gdb.  The kinds of events supported are defined by GdbEvent.  For
       each of those events a specific callback function can be specified when that
       event occurs.  The prototypes for the callbacks are a function of the GdbEvent's
       and are summarized below along with additional comments about each event.  Passing
       NULL for an callback effectively removes that callback.

       Gdb_Before_Command - intercept gdb command execution
	   int callback(char *arg, int from_tty);
	   
	   The arguments are the same as for a gdb plugin (arg is the command line
	   arguments).  However this callback returns and int.  If it returns 0 then
	   the command is not executed by gdb.  Otherwise it is.

       Gdb_After_SET_Command - generic SET option handling
	   void callback(char *theSetting, Gdb_Set_Type type, void *value, int show);
       
	   This is identical to the routine specified to gdb_define_set_generic() and
	   the callback prototype is actually defined by Gdb_Set_Funct.  You should use
	   gdb_define_set_generic() and not this interface for defining the generic
	   SET handler.
	   
       Gdb_Before_Query - intercept queries
	   int callback(const char *prompt, int *result);

	   Called just before a prompt to a "y"/"n" query prompt.  If the callback
	   returns 0 then query will NOT be displayed and a 0 or 1 should be returned
	   by the callback in its result parameter.  If the callback returns 0 then
	   the result parameter is ignored and the prompt is displayed.
       
       Gdb_After_Query -intercept query result
       	   int callback(int result);
       	   
       	   Called just after a query response is read.  The response to the query is
       	   passed to the callback and the callback returns it's interpretation of
       	   that query (or the same value).  Note that this event is only handled if
       	   the Gdb_Before_Query event was specified.
      
       Gdb_Before_Warning - intercept warnings
	   int callback(const char *message);
	   
	   Called just before a warning message is displayed.  If the callback returns
	   0 the warning is not displayed.  Otherwise it is.
       
       Gdb_After_Creating_Breakpoint - notify when a breakpoint is created
	   void callback(GDB_ADDRESS address, int enabled);
	   
	   Called just after a new breakpoint, whatchpoint, or tracepoint is created.
	   If the breakpoint is currently enabled (it wont if it's for an outer scope),
	   the enabled is passed as 1.
       
       Gdb_Before_Deleting_Breakpoint - notify when a breakpint is deleted
	   void callback(GDB_ADDRESS address, int enabled);
	   
	   Same as Gdb_After_Creating_Breakpoint except the callback is notified when
	   the breakpoint, whatchpoint, or tracepoint is deleted.
       
       Gdb_After_Modified_Breakpoint
	   void callback(GDB_ADDRESS address, int enabled);
	   
	   Same as Gdb_After_Creating_Breakpoint except the callback is notified when
	   the breakpoint, whatchpoint, or tracepoint is modified.
       
       Gdb_After_Attach - notify when a process is attached
	   void callback(int pid);
	   
	   Called after a process is attached to gdb as the result if a ATTACH command.
	   
       Gdb_Before_Detach - notify when a process is detached
	   void callback(void);
	   
	   Called before a process is detached to gdb as the result if a DETACH command.
	   
       Gdb_After_Register_Changed - notify what target register was changed
	   void callback(void);
	   
	   When a target program's register is changed by gdb this callback is called.
       
       Gdb_After_Memory_Changed - notify when target memory was changed
	   void callback(GDB_ADDRESS address, int length);
	   
	   When the target program's memory is changed by gdb this callback is called.
	   The target address and the amount of memory changed is passed.
       
       Gdb_Context_Is_Changed - notify of context change (new pid)
	   void callback(int pid);
	   
	   Called when gdb switches to a new process (i.e., when it prints "Switching
	   to thread N", where N is the process id which is also passed to the callback.
       
       Gdb_Before_Error - notify that error is being reported
	   void callback(void);
       
	   Called just before an error message is about to be displayed.  
	   
	   Note that if stderr output is redirected then your output filter will get
	   the error output but gdb's internal error recovery will cause it to not
	   return to the plugin that caused the error (just like calling gdb_error()).
	   You can use the Gdb_Before_Error to detect the errors but you should always
	   return from the callback.  Don't try, for example, to use setjmp/longjmp
	   because you'll confuse gdb's error recover which will lead to a fatal error.
	   
	   If you must do something that wants to quietly detect that the operation will
	   result in an error then use gdb_execute_command_silent() or if applicable,
	   gdb_eval_silent() which know how to silently detect and recover from errors
	   as the result of executing some statement.
       
       Gdb_After_File_Changed - notify that a FILE command was specified
	   void callback(char *filename);
	   
	   Called after processing a FILE command.  The filename from the FILE command
	   is passed.
       
       Gdb_After_Attach_To_File - notify that a FILE (or ATTACH if it can figure out
                                  the file) command has occurred
	   void callback(char *filename);
	   
	   Called after processing a FILE command or (ATTACH if it can figure out
	   the file).  The filename or NULL is passed.  This is more general than
	   using Gdb_After_File_Changed.
       
       Gdb_Before_Prompt - notify that prompt is next display
	   void callback(void);
	   
	   Called just before any prompt is about to be displayed.
       
       Gdb_Begin_ReadRawLine - notify of start of raw line read
           void callback(char *prompt);
	   
	   Called at the start of reading "raw" input lines.  Such lines are the lines
	   contained in DEFINE and DOCUMENT and the outer-most WHILE and IF commands.
	   
       Gdb_ReadRawLine - read a raw line
           char *callback(char *prompt);
	   
	   Called to read raw data lines from the terminal.  The callback should either
	   return a line or NULL.  If NULL is returned gdb reads it normally would.
	   
	   Note that gdb_define_raw_input_handler() uses this same mechanism to define
	   a function to read raw lines and using that call is recommended. 
	   
       Gdb_End_ReadRawLine - notify of end of raw line read
	   void callback(void);
	   
	   Called at the end of reading "raw" input lines.
       
       Gdb_State_Changed - notify of a gdb's state change
	   void callback(GdbState newState);
	   
	   Called when gdb changes state.  The new state is as defined by GdbState.
	   Note that gdb_define_exit_handler() uses this mechanism to call its
	   specified exit handler when gdb quits (GdbState == Gdb_Not_Active).
       
       Gdb_Word_Completion_Cursor - save/restore word completion cursor
           void callback(int save_cursor);
	   
	   Gdb word completion (i.e., displaying list of alternative names for commands,
	   filenames, etc. or displaying all alternatives) is unique since it can cause
	   a output display and/or prompt when certain keys are typed during keyboard
	   input.  The prompt, if any, is only for a "y" or "n" answer and is not a
	   standard gdb query since typing the "y" or "n" does not require a return to
	   send it.  The prompt itself as well as the displays are output to the current
	   (possibly) redirected stdout stream.  However, because word completion occurs
	   during input, it needs to do make sure the cursor can be restored after the
	   response to the prompt just in case the output is being redirected in some
	   unexpected way.  By default it writes (to gdb_default_stderr) a standard xterm
	   ESC 7 to save the cursor and ESC 8 to restore it.  But a callback is provided
	   if this is not what's wanted or requires some additional processing.
		 
	   The callback is sent a 1 to indicate the cursor is to be saved and 0 if it is
	   to be restored.
       
       Gdb_Word_Completion_Query - intercept word completion query
       	   void callback(GDB_FILE *stream, char *query);
       
       	   As mentioned above for Gdb_Word_Completion_Cursor word completion may issue
	   a prompt, specifically "Display all N possibilities? (y or n) ", where N is
	   the number of possibilities.  It will issue this and put up a read to the
	   terminal awaiting a "y", "Y", or " " to indicate "yes" or a "n", "N", or
	   rubout to indicate a "no" response (or CTL-G to indicate abort).  By
	   specifying a Gdb_Word_Completion_Query callback you can do the prompt
	   yourself.
	   
	   The callback takes fprintf-like parameters for the GDB_FILE stream which
	   should receive the prompt.
	   
       Gdb_Word_Completion_Read - intercept word completion query read
	   int callback(void);
	   
	   The Gdb_Word_Completion_Query callback is, as described above, used to
	   intercept the query prompt for a "yes" or "no" answer.  The
	   Gdb_Word_Completion_Read callback allows you to intercept the read and do it
	   yourself.
	   
	   The callback should return 0 for "no" and non-zero for "yes".  The default
	   convention is to accept "y", "Y", or " " to indicate "yes" and "n", "N", or
	   rubout to indicate a "no" (and a CTL-G to indicate abort).  No return is
	   necessary.  

       Gdb_History_Prompt
	   char *callback(char *display_prompt);
	   
	   Gdb_History_Prompt callback intercepts the gdb prompt when it is trying to
	   display a history prompt, e.g., when CTRL-R is entered and the prompt to be
	   shown is "(reverse-i-search)".
	    
	   The callback is given the history prompt and should return a prompt.  This
	   can either be the ORIGINAL unmodified input prompt or another prompt in a
	   buffer controlled by the callback.  It should NOT modify the input prompt.
	   
	   Note that the callback is called for every character before it is echoed
	   to the display.  If the callback returns a modified prompt it should not 
	   assume the input prompt on the next call will be the same as the one
	   returned on the previous call.  Indeed, it will always be the one gdb
	   wants to display for the history prompt.
	   
	   Also note, gdb displays the prompt AFTER positioning the cursor to the
	   start of the line it is on.

       Gdb_Interactive
	   void callback(void);
       
	   Generally called when gdb is in compute bound tasks.  Could be used to
	   provide some kind of feedback that something is going on. */

/*--------------------------------------------------------------------------------------*/
			     /*--------------------------*
			      | Object Module Operations |
			      *--------------------------*/

const char *gdb_is_addr_in_section(GDB_ADDRESS addr, char *segname_sectname);
    /* This is a rather low-level function which is used to determine whether the
       specified addr is located within one of an object file's load sections
       (segname_sectname).  If it is, a (const) pointer to the segname_sectname
       pointer is returned.  Otherwise NULL is returned.
 
       If segname_sectname is passed as NULL then the function will return a pointer
       to the first segname_sectname found in ANY of the loaded sections which
       contains the addr.
 
       The segname_sectname is a string indicating the Mach-o load segname and sectname
       concatenated with a period (e.g., "__TEXT.__cstring", "__DATA.__cfstring", etc.).
       See 'struct section' definition in /usr/include/mach-o/loader.h for some details.
       Also see the Mach-o Runtime ABI documentation.
 
       For repeated tests for different sections for the SAME addr this function caches
       the gdb search information to avoid needless repeated object file searching.  To
       clear this cache pass addr with the value 0.  0 is returned for this case too. */

typedef struct Section_Range {
    GDB_ADDRESS          addr;			/* lowest address in section		*/
    GDB_ADDRESS          endaddr;		/* 1+highest address in section		*/
    struct Section_Range *next;			/* next on list				*/
} Section_Range;

int gdb_find_section(char *segname_sectname, Section_Range **ranges);
     /* This function searches all the sections in all the loaded object files for the
        specified segname_sectname.  If found a count is returned as the function result
        indicating the number of instances found with that section name.  If ranges is
        not NULL it will be returned as a pointer to a gdb_malloc'ed list (same number
        of entries as was found) of section address ranges.  Each list entry has the
        layout shown above.
        
        The segname_sectname is a string indicating the Mach-o load segname and sectname
        concatenated with a period (e.g., "__TEXT.__cstring", "__DATA.__cfstring", etc.).
        See 'struct section' definition in /usr/include/mach-o/loader.h for some details.
        Also see the Mach-o Runtime ABI documentation. */

/*--------------------------------------------------------------------------------------*/
			   /*-------------------------------*
			    | Implementation Considerations |
			    *-------------------------------*

Generally gdb is a self-contained program.  Over the years there have been various UI's
added to gdb but what they all had in common is that they were all build as part of gdb,
i.e., linked with it.  This plugin support library is different.  It is not linked with
gdb but thanks to those many UI's there are many "hooks" in gdb to allow this library
to do the things it does.  Also, both the pre-linked UI's and the plugin library can
change the values of gdb global data.

Just for clarity, a gdb "hook" is a function pointer, usually (but not always) preset
to NULL.  Depending on the hook, gdb tests it and calls it if not NULL to add to or
replace some piece of functionality.  Those various UI's depend upon these hooks to
get control at key places within gdb.  The plugin library does the same.

In the case of the older UI's built and linked with gdb there was only one UI used
whenever gdb is used.  So another difference with the plugin support library is that 
there may be multiple library instances accessing all talking to the same gdb.  This
means that multiple library instances are not totally independent of one another.  For
example, one instance may change a hook that another instance also changes.

This architecture is not perfect and the user of the plugin support library user needs to
be aware of the potential for "cross-talk" among multiple instances of the library.

The general rule for hooks followed by this plugin architecture is that all hooks are
chained.  In other words the previous value of the hook is called as part of the
new hook's execution if the previous hook's value is not NULL.  

For example, assume the initial value for hook "x" is NULL and the first library instance
wants to set hook "x".   It would save the previous value (NULL) and set "x" to point at
the first instance's replacement function (call it "hook1").  When a second library 
instance wants to set hook "x" it picks up the previous value, now "hook1" and sets "x"
to point it's own function (call it "hook2").  If the first instance is in control and
hook1 is called then it will see it's saved previous version is NULL and not do anything
other than what hook1 is supposed to do.  But if the second instance is in control and
hook2 is called it will do whatever hook2 needs to do and also call it's previous value
which would be hook1.  Thus both hook2 and hook1 are called if hook2 is called (assuming
hook2 follows the established rules).

Most of the hooks set by the library and all the "special events" defined for
gdb_special_events() (which are in reality mostly hooks) are for setting various gdb
hooks and follow the chaining convention "behind your back".  So you can see non-
cooperating plugin libraries are going to get into trouble with this scheme (we said it
wasn't perfect).

Besides hooks there is some critical data that the plugin libraries modify.  The top
of this list is the various pieces of data controlling I/O.  With only one gdb and
multiple library instances this is just asking for trouble!  There's no simple solution
to this.  The library could chain the data by saving the original value much as it
does hooks.  But that would mean, for example, one instance reads from the terminal
and all the other instances also think they are reading as well.  To avoid this the
normal chaining conventions for I/O were NOT followed.  Each is independent and each
instance, when it does I/O, is talking directly to its library instance than then to gdb,
and not each other.

Again all of this requires some sort of cooperation among the simultaneous plugin
library users.  So like the hook conventions this too leaves a lot to be desired.

To allow the various plugins to cooperate with each other one API routine has been
provided, gdb_private_data().  This allows library instances to "talk" to one another
through some globally allocated data.
*/

#ifdef __cplusplus
}
#endif

#endif
