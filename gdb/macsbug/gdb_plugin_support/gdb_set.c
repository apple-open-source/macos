/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                      gdb_set.c                                       |
 |                                                                                      |
 |                              Gdb SET and SHOW Processing                             |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2006                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*
 
 This file contains the plugin interfaces extend gdb's SET and SHOW commands.  Here you
 can add to the SET/SHOW settings lists so that gdb sets values and displayes them just
 like it does its own.  User handlers can be specified to filter the operations as well.
*/

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "gdb_private_interfaces.h"

#include "command.h"
/* FIXME: we don't have interfaces to the parts of cmd_list_element
   that we need here yet.  They need to be added... */
#include "cli/cli-decode.h"
#include "gdbcmd.h"

/*--------------------------------------------------------------------------------------*/

static void (*gdb_set_hook)(struct cmd_list_element *c) = NULL; /* gdb's set_hook	*/
static Gdb_Set_Funct users_generic_sfunc = NULL;/* user's function for set_hook calls	*/
    
typedef struct Set_Info {			/* user defined set cmd list entries:	*/
    struct Set_Info 	    *next;		/*    ptr to next list entry		*/
    struct cmd_list_element *c_set;		/*    set command gdb list entry	*/
    struct cmd_list_element *c_show;		/*    show command gdb list entry	*/
    Gdb_Set_Type	    type;		/*    our version of the var_types	*/
    Gdb_Set_Funct	    sfunct;		/*    user's function to call		*/
    char   		    theSetting[1];	/*    start of the setting name		*/
} Set_Info;

static Set_Info *set_list = NULL;		/* list of user defined set commands	*/
   
static void define_set(char *theSetting, Gdb_Set_Funct sfunct, Gdb_Set_Type type,
		       void *value_ptr, int for_set_and_show, char *enumlist[],
		       char *helpInfo);
static void my_set(char *ignore, int from_tty, struct cmd_list_element *c);
static void my_set_hook(struct cmd_list_element *c);    
 
/*--------------------------------------------------------------------------------------*/

/*--------------------------------------------*
 | gdb_define_set - define a SET/SHOW handler |
 *--------------------------------------------*
 
 This provides a way to extend the gdb SET to include your own settings and to allow SHOW
 to display your current settings.  A gdb SET command has the general form,
 
   SET <theSetting> argument(s...)
 
 and SHOW,
 
   SHOW <theSetting>
   
 also HELP can display the SET's help info for <theSetting>,
 
   HELP SET <theSetting>
   
 Thus gdb_define_set() allows you to include your settings in these commands.
 
 The gdb_define_set() parameters are:
 
   theSetting	     The keyword associated with the desired setting.
   
   sfunct	     The function to handle the SET and/or SHOW operation in some 
		     specialized way not already provided by gdb. See description for type
		     below.  This function will be described after all the gdb_define_set()
		     parameters are described.  Specifying NULL for this parameter means
		     that only the setting is to be recorded by SET.
		     
   type		     A Gdb_Set_Type enum indicating what kind of argument to expect and
                     what the meaning of the value_ptr.  The expected form of the SET
		     arguments as a function Gdb_Set_Type is defined as follows:

		     Set_Boolean       "on", "1", "yes", "off", "0", "no"
		     Set_UInt0         any unsigned int, 0 yields UINT_MAX
		     Set_Int0	       any int, 0 yields INT_MAX
		     Set_Int	       any int, 0 is treated as value 0
		     Set_String	       sequence of chars, escapes removed
		     Set_String_NoEsc  sequence of chars, escapes retained
		     Set_Filename      a filename
		     Set_Enum	       one of a specified set of strings

   value_ptr	     This is a POINTER to a object that will receive the value of the SET
                     argument.  The object's type is a function of the Gdb_Set_Type.
		     
		     Gdb_Set_Type's Set_Boolean, Set_UInt0, Set_Int0, and Set_Int all
		     require the value_ptr to be a pointer to an (unsigned) int.  When
		     the SET command is executed the value is set according to the
		     argument and summarized in the comments above for each Gdb_Set_Type
		     and described some more as follows:
		     
		     Set_Boolean       Value is 0 or 1 depending on the setting.
		     
		     Set_UInt0	       Value is an unsigned int but if a value of 0 is
		                       entered for the setting, the value is set to
				       UINT_MAX to indicated "unlimited".
		    
		     Set_Int0	       Same as Set_UInt0 except the pointer is to a
				       signed int and a 0 setting causes the value to be
				       set to INT_MAX.
		     
		     Set_Int	       Value is an int, 0 has no special meaning and is
		                       treated like any other integer setting.
		     
		     Gdb_Set_Type's Set_String, Set_String_NoEsc, and Set_Filename all
		     require value_ptr to be a pointer to a char * (i.e., a char **). The
		     SET argument is a sequence of characters and that sequence is
		     gdb_malloc()'ed and the pointer to it stored where value_ptr points.
		     The SET arguments form depends on the Gdb_Set_Type:
		     
		     Set_String	       Any sequence of characters with escaped characters
		                       processed.
				       
		     Set_String_NoEsc  A sequence of characters stored verbatim, i.e.,
		                       escaped are not processed.
				       
		     Set_Filename      A pathname.
		     
		     Note that the pointer to the string pointer must initially be NULL
		     or to gdb_malloc()'s space since the old space will be gdb_free()'ed
		     when replaced with a new setting.
		     
		     Gdb_Set_Type Set_Enum is special in that it will only appear in
		     the sfunct when the sfunct is called as a result of
		     gdb_define_set_enum().  See it's comments for further details.
		     
   for_set_and_show  Normally the sfunct is only called when a SET command is done.  But
                     by setting for_set_and_show to a non-zero value it will also be
		     called for SHOW as well.  The sfunc has a parameter indicating why
		     it's being called (see below).
   
   helpInfo          This is a short help info to be used for HELP SHOW and when SHOW
                     displays the current setting.  THE HELP INFO STRING MUST BEGIN WITH
		     THE SEQUENCE OF CHARACTERS "Set " EXACTLY.  If helpInfo is passed as
		     NULL, then a generic help is created as a function of the type:
			   
		     Set_Boolean                 "Set <theSetting> on | off"
		     Set_UInt0/Set_Int0/Set_Int  "Set <theSetting> n"
		     Set_String/Set_String_NoEsc "Set <theSetting> string of chars..."
		     Set_Filename                "Set <theSetting> filename"
		     Set_Enum                    "Set <theSetting> enum1|enum2..."
		     
		     where <theSetting> is replaced with the passed setting name and
		     enumN is gdb_define_set_enum()'s enum list. 
			   
		     Note, keep the help show since it is used by gdb in the SHOW and
		     HELP SET (or HELP SHOW) commands.
		     
   The SET sfunct should have the following prototype:
   
     void sfunct(char *theSetting, Gdb_Set_Type type, void *value, int show, int confirm);
   
   where theSetting = the SET/SHOW setting being processed.
	 type       = the type of the value described above.
	 value      = pointer to the value whose form is a function of the type.
	 show       = 0 if called for SET and 1 for SHOW.
	 confirm    = 1 if SET/SHOW is entered from terminal and SET confirm on.
	 
   The value is usually for convenience and of course unnecessary if you associate unique
   sfunct's with unique settings.  If you use an sfunct for more than one setting then
   theSetting and type can be used to interpret the meaning of the value. 
	 
   Note that you do not have to specify a sfunct at all.  If you pass NULL then the data
   value points to will be set by SET and shown by SHOW.  In many cases this may be all
   that is necessary.
   
   Finally, one last point -- do not confuse these SET operations with the SET variable
   (e.g., SET $i = 1") operations.  They are NOT handled here.  If you need to intercept
   the SET variable command you still need to use gdb_replace_command().
*/

void gdb_define_set(char *theSetting, Gdb_Set_Funct sfunct, Gdb_Set_Type type,
		    void *value_ptr, int for_set_and_show, char *helpInfo)
{
    define_set(theSetting, sfunct, type, value_ptr, for_set_and_show, NULL, helpInfo);
}


/*--------------------------------------------------------------------------------*
 | gdb_define_set_enum - define a set command (for an emumerated set of keywords) |
 *--------------------------------------------------------------------------------*
 
 This is almost identical to gdb_define_set() above except that in place of the type, a
 pointer to a NULL terminated list of acceptable string pointers is expected.  For
 example,
 
   gdb_define_set_enum("example", sfunct, valid_settings, &value, 0, "Set UP | DOWN");
 
 where, 
 
   char *valid_settings[] = {"UP", "DOWN", NULL};
   
 The list should contain all unique items and the matching of the SET argument is case
 sensitive.

 When sfunct is called it's type parameter will be set to Set_Enum to indicate that the
 value is a pointer to a pointer to one of the valid_settings strings.
*/

void gdb_define_set_enum(char *theSetting, Gdb_Set_Funct sfunct, char *enumlist[],
                         void *value_ptr, int for_set_and_show, char *helpInfo)
{
    define_set(theSetting, sfunct, Set_Enum, value_ptr, for_set_and_show, enumlist, helpInfo);
}


/*-------------------------------------------------------*
 | gdb_define_set_generic - define a generic set handler |
 *-------------------------------------------------------*
 
 You can specify a "generic" sfunct to catch ALL SET (not SHOW) operations after gdb
 processes it.  The sfunct is as described above but unlike the sfunct's associated with
 a specific setting, the generic function will filter all SET setting operations.  As
 usual though the sfunct's theSetting and type parameters can be used to interpret the
 value.  Since this function is only called for SET the sfunct show parameter will always
 be 0.
*/

void gdb_define_set_generic(Gdb_Set_Funct sfunct)
{
    #define SET_GENERIC ((Gdb_Set_Type)-1)

    if (sfunct)
    	define_set(NULL, sfunct, SET_GENERIC, NULL, 0, NULL, NULL);
    else
    	deprecated_set_hook = gdb_set_hook;
}


/*-----------------------------------------------------------------------------*
 | define_set - common internal routine for the gdb_define_set_xxxx() routines |
 *-----------------------------------------------------------------------------*
 
 This is called by gdb_define_set(), gdb_define_set_enum(), and gdb_define_set_generic()
 to define a SET handler or for the generic case, the set_hook.  There's only one
 set_hook so we can always associate the set_hook function with the user's specified
 function.  But for gdb_define_set() and gdb_define_set_enum() we need to keep a list of
 the information passed here so that our one common SET handler can figure out which
 user function it is to call for a particular setting.  We have to do it this way because
 we need to hide the internal gdb SET handler from the user and there is no place in the
 gdb struct cmd_list_element to save our data.
*/

static void define_set(char *theSetting, Gdb_Set_Funct sfunct, Gdb_Set_Type type,
		       void *value_ptr, int for_set_and_show, char *enumlist[],
		       char *helpInfo)
{
    struct cmd_list_element *c_set, *c_show;
    var_types               var_type;
    cmd_sfunc_ftype         *set_func = NULL, *show_func = NULL;
    int			    i, found;
    Set_Info		    *si, *prev, *next;
    char		    **p, help[1024], *showInfo;
    
    /* Map our Gdb_Set_Type to gdb's var_types...					*/
    
    switch (type) {
    	case Set_Boolean:      var_type = var_boolean;	    	break;
    	case Set_UInt0:        var_type = var_uinteger;	    	break;
    	case Set_Int0:         var_type = var_integer;	    	break;
    	case Set_Int:          var_type = var_zinteger;	    	break;
    	case Set_String:       var_type = var_string;	    	break;
    	case Set_String_NoEsc: var_type = var_string_noescape; 	break;
    	case Set_Filename:     var_type = var_filename;	    	break;
    	case Set_Enum:         var_type = var_enum;		break;
	
	default:
	    if (theSetting == NULL && type == SET_GENERIC) {	/* handle set_hook...	*/
		deprecated_set_hook = my_set_hook;
		users_generic_sfunc = sfunct;
		return;
	    }
	    gdb_error("Invalid setting type passed to gdb_define_set()");
	    break;
    }
    
    if (theSetting == NULL) {			/* must have a setting name...		*/
	gdb_error("NULL setting name passed to gdb_define_set()");
	return;
    }
    
    if (!helpInfo || !*helpInfo) {		/* handle default help...		*/
	strcpy(help, "Set ");
	
	switch (type) {
	    case Set_Boolean:
	    	sprintf(help + 4, "%s on | off", theSetting);
	    	break;
	    
	    case Set_UInt0:
	    case Set_Int0:
	    case Set_Int:
	    	sprintf(help + 4, "%s n", theSetting);
	    	break;
		
	    case Set_String:
	    case Set_String_NoEsc:
	    	sprintf(help + 4, "%s string of characters...", theSetting);
	    	break;
	   
	   case Set_Filename:
	    	sprintf(help + 4, "%s filename", theSetting);
	    	break;
		
	    case Set_Enum:
	    	i = sprintf(help + 4, "%s ", theSetting);
		p = (char **)enumlist;
		while (*p) {
		    strcpy(&help[i], *p);
		    i += strlen(*p);
		    help[i++] = ' '; help[i++] = '|'; help[i++] = ' ';
		    ++p;
		}
		help[i - 3] = '\0';
	    	break;
	}
	
	helpInfo = gdb_malloc(strlen(help) + 1);
	strcpy(helpInfo, help);
    } else if (helpInfo && strncmp(helpInfo, "Set ", 4) != 0) {
	gdb_error("gdb_define_set() help info MUST begin with \"Set \" exactly");
	return;
    }
    
    /* Define the SET setting to gdb (enums handled differently)...			*/
    /* We have two different algorithms; one for pre version gdb 6.3 and one for gdb	*/
    /* 6.3 and all following (hopefully "they" won't change this stuff again).  The 	*/
    /* pre 6.3 stuff is kept for historical purposes and "just in case".		*/
    
    #if GDB_VERSION > 5
    
    /* Use the "Set..." string to constuct the "Show..." string...			*/
    
    showInfo = gdb_malloc(strlen(helpInfo) + 2);
    strcpy(showInfo, "Show ");
    strcpy(showInfo + 5, helpInfo + 4);
    
    /* If user specified a sfunc we will call it from our my-set() function.  If	*/
    /* for_set_and_show is set then we also use it for "show" commands...		*/
    
    if (sfunct) {
    	set_func = my_set;
    	if (for_set_and_show)
    	    show_func = my_set;
    }
    
    /* Call the appropriate gdb handler...						*/
    
    switch (var_type) {
	case var_boolean:
	case var_auto_boolean:
      	    add_setshow_boolean_cmd(theSetting, class_support, (char *)value_ptr, helpInfo,
                                    showInfo, NULL, set_func, show_func, &setlist, &showlist);
	    break;

	case var_uinteger:
	case var_integer:
	case var_zinteger:
	   add_setshow_uinteger_cmd(theSetting, class_support, (char *)value_ptr, helpInfo,
				    showInfo, NULL, set_func, show_func, &setlist, &showlist);
	    break;
	    
	case var_string:
	    add_setshow_string_cmd(theSetting, class_support, (char **)value_ptr, helpInfo,
	    			   showInfo, NULL, set_func, show_func, &setlist, &showlist);
	    break;
	
	case var_string_noescape:
	    add_setshow_string_noescape_cmd(theSetting, class_support, (char **)value_ptr,
	    				    helpInfo, showInfo, NULL, set_func, show_func,
	    				    &setlist, &showlist);
	    break;
	    
	case var_optional_filename:
	case var_filename:
	    add_setshow_filename_cmd(theSetting, class_support, (char **)value_ptr, helpInfo,
	    			     showInfo, NULL, set_func, show_func, &setlist, &showlist);
	    break;

	case var_enum:
	    add_setshow_enum_cmd(theSetting, class_support, enumlist, (char **)value_ptr,
				 helpInfo, showInfo, NULL, set_func, show_func, &setlist,
				 &showlist);
	    break;
	
	default:
	  gdb_error("internal error: bad var_type");
    }
    
    /* If we have a sfunct then my_set() needs to be able to find the user info 	*/
    /* associated with the struct cmd_list_element pointer gdb passes to my_set() so	*/
    /* it can call the user's function.  Therefore we need to search the setlist and	*/
    /* showlist to hunt down the command we just added (damn gdb interfaces don't 	*/
    /* return the pointer).  We'll record these pointers in our own list a little 	*/
    /* later.										*/
    
    if (sfunct) {
    	for (c_set = setlist, found = 0; c_set = c_set->next; c_set)
    	    if (strcmp(theSetting, c_set->name) == 0) {
    	    	found = 1;
    	    	break;
    	    }
    	if (!found)
	    gdb_error("internal error: cannot find added set command");
     	
     	if (for_set_and_show) {
	    for (c_show = showlist, found = 0; c_show = c_show->next; c_show)
		if (strcmp(theSetting, c_show->name) == 0) {
		    found = 1;
		    break;
		}
	    if (!found)
		gdb_error("internal error: cannot find added show command");
	}
    }
    
    #else /* GDB_VERSION <= 5 */
    
    if (type != Set_Enum)
    	c_set = add_set_cmd(theSetting, class_support, var_type, (char *)value_ptr, helpInfo,
				            &setlist);
    else
    	c_set = add_set_enum_cmd(theSetting, class_support, enumlist, (char *)value_ptr,
			     	  helpInfo, &setlist);
    
    /* If a function handler is defined before add_show_from_set() it will be called 	*/
    /* by both SET and SHOW.  If defined after add_show_from_set() it will be called 	*/
    /* only for SET.  If no sfunct is specified then only the value will be set by SET.	*/ 
    
    if (sfunct) {
	if (for_set_and_show) {
	    c_set->function.sfunc = my_set;
	    c_show = add_show_from_set(c_set, &showlist);
	} else {
	    c_show = add_show_from_set(c_set, &showlist);
	    c_set->function.sfunc = my_set;
	}
    } else 
        c_show = add_show_from_set(c_set, &showlist);
    
    #endif /* GDB_VERSION <= 5 */
    
    /* If theSetting was prviously specified only remember the most recent call...	*/
    
    for (si = set_list, prev = NULL; si; prev = si, si = si->next)
    	if (gdb_strcmpl(theSetting, si->theSetting)) {
	    if (prev)
	    	prev->next = si->next;
	    else
	    	set_list = si->next;
	    gdb_free(si);
	    break;
	}
    
    /* If we have a sfunct then my_set() needs to be able to find the user info 	*/
    /* associated with the struct cmd_list_element pointer gdb passes to my_set() so	*/
    /* it can call the user's function.  Obviously we don't need to record the info if	*/
    /* no user function is associated with this setting.				*/
    
    if (sfunct) {
	si = gdb_malloc(sizeof(Set_Info) + strlen(theSetting));
	
	si->c_set  = c_set;
	si->c_show = c_show;
	strcpy(si->theSetting, theSetting);
	si->type   = type;
	si->sfunct = sfunct;
	si->next   = set_list;
	set_list   = si;
    }
}


/*-----------------------------------------------------------*
 | my_set - glue routine between user's set function and gdb |
 *-----------------------------------------------------------*
 
 This is the common SET/SHOW handler we define to gdb for all user settings.  Here we
 map the SET/SHOW operation back to the associated user function and call it.  This is
 all done to hide the struct cmd_list_element * from the user function.
*/

static void my_set(char *ignore, int from_tty, struct cmd_list_element *c)
{
    Set_Info *si;
    void     *value;
    
    /* Reassociate the SET/SHOW with the user info...					*/
    
    if (c->type == show_cmd) {
	for (si = set_list; si; si = si->next)
	    if (c == si->c_show)
		break;
    } else {
	for (si = set_list; si; si = si->next)
	    if (c == si->c_set)
		break;
    }
    
    if (!si || strcmp(si->theSetting, c->name) != 0)
	gdb_internal_error("my_set() inconsistency in set command lookup");
    
    /* Get the pointer to the value that was SET or SHOWen...				*/
    
    switch (c->var_type) {
	case var_boolean:			/* *(int *)c->var			*/
	case var_auto_boolean:			/* *(int *)c->var			*/
	case var_uinteger:			/* *(unsigned int *)c->var		*/
	case var_integer:			/* *(int *)c->var			*/
	case var_zinteger:			/* *(unsigned int *)c->var		*/
	    value = (void *)(unsigned int *)c->var;
	    break;
	    
	case var_string:			/* *(unsigned char **)c->var		*/
	case var_string_noescape:
	case var_optional_filename:
	case var_filename:
	case var_enum:				/* *(char **)c->var			*/
	    value = (void *)(char **)c->var;
	    break;
	
	default:
	  gdb_error("internal error: bad var_type");
    }
    
    si->sfunct(c->name,si->type,value,(c->type==show_cmd),	/* call user sfunct	*/
    	       from_tty && input_from_terminal_p());
}


/*------------------------------------*
 | my_set_hook_guts - set_hook "guts" |
 *------------------------------------*
 
 This is called from my_set_hook() below and from a set hook that could be installed
 by gdb_special_events().  Since we want to do the same thing for each of these the
 guts of the set hook are factored out to here.
*/

void __my_set_hook_guts(struct cmd_list_element *c, Gdb_Set_Type *type, void **value)
{
    switch (c->var_type) {
	case var_auto_boolean:			/* *(int *)c->var			*/
	case var_boolean:			/* *(int *)c->var			*/
	    *type  = Set_Boolean;
	    *value = (void *)(int *)c->var;
	    break;

	case var_uinteger:			/* *(unsigned int *)c->var		*/
	    *type  = Set_UInt0;
	    *value = (void *)(unsigned int *)c->var;
	    break;
	
	case var_integer:			/* *(int *)c->var			*/
	    *type  = Set_Int0;
	    *value = (void *)(int *)c->var;
	    break;
	
	case var_zinteger:			/* *(unsigned int *)c->var		*/
	    *type  = Set_Int;
	    *value = (void *)(unsigned int *)c->var;
	    break;
	    
	case var_string:			/* *(unsigned char **)c->var		*/
	    *type  = Set_String;
	    *value = (void *)(unsigned char **)c->var;
	    break;
	    
	case var_string_noescape:		/* *(unsigned char **)c->var		*/
	    *type  = Set_String_NoEsc;
	    *value = (void *)(unsigned char **)c->var;
	    break;
	    
	case var_optional_filename:		/* *(unsigned char **)c->var		*/
	case var_filename:			/* *(unsigned char **)c->var		*/
	    *type  = Set_Filename;
	    *value = (void *)(unsigned char **)c->var;
	    break;
	    
	case var_enum:				/* *(char **)c->var			*/
	    *type  = Set_Enum;
	    *value = (void *)(char **)c->var;
	    break;
	
	default:
	  gdb_error("internal error: bad var_type");
    }
}

    
/*-------------------------------------------------------------*
 | my_set_hook - set_hook function called for all set commands |
 *-------------------------------------------------------------*
 
 This is our installed set_hook routine, installed by gdb_define_set_generic().  This is
 similar to my_set() above except that we already know the user function to call.
*/

static void my_set_hook(struct cmd_list_element *c)
{
    void 	 *value;
    Gdb_Set_Type type;
     
    if (!users_generic_sfunc)
	return;
   
    __my_set_hook_guts(c, &type, &value);
    users_generic_sfunc(c->name, type, value, 0, input_from_terminal_p());
}

/*--------------------------------------------------------------------------------------*/

/*---------------------------------------*
 | __initialize_set - set initialization |
 *---------------------------------------*
 
 This is called as part of overall gdb support initialization to initialize the stuff
 that needs initializing in this file.
*/

void __initialize_set(void)
{
    static initialized = 0;
    
    if (!initialized) {
    	initialized = 1;
	//gdb_set_hook = set_hook;
	gdb_set_hook = INITIAL_GDB_VALUE(deprecated_set_hook, deprecated_set_hook);
    }
}

