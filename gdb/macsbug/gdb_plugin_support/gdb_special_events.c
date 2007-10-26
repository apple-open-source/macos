/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                 gdb_special_events.c                                 |
 |                                                                                      |
 |                           Gdb Interfaces to Internal Hooks                           |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2006                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*
 
 This file has only one plugin user visible routine, gdb_special_events().  It allows a
 plugin to use many of the gdb internally provided "hooks" while at the same time 
 hiding the gdb internals from the user.  We map the hook calls on to user provided hook
 routines which are independent of gdb's data definitions.
*/

#include "gdb_private_interfaces.h"

#include <stdio.h>
#include <stdarg.h>

#include "command.h"
/* FIXME: we don't have interfaces to the parts of cmd_list_element
   that we need here yet.  They need to be added... */
#include "cli/cli-decode.h"
#include "breakpoint.h"
#include "gdbcore.h" // file_changed_hook
#include "inferior.h"
//#include "target.h"

/*--------------------------------------------------------------------------------------*/

static void (*saved_call_command_hook)(struct cmd_list_element *c, char *arg, int from_tty);
static int (*users_call_command)(char *arg, int from_tty);
static int call_command_defined = 0;

static void (*saved_set_hook)(struct cmd_list_element *c);
static Gdb_Set_Funct users_set_hook;
static int set_hook_defined = 0;

static int (*saved_query_hook)(const char *format, va_list ap);
static int (*users_query_hook)(const char *message, int *result);
static int query_hook_defined = 0;
static int (*saved__default_gdb_query_hook)(char *, va_list);

static int (*users_query_after_hook)(int result);
static int query_after_hook_defined = 0;

static void (*saved_warning_hook)(const char *format, va_list);
static int (*users_warning_hook)(const char *message);
static int warning_hook_defined = 0;

static void (*saved_create_bkpt)(struct breakpoint *b);
static void (*users_create_bkpt)(GDB_ADDRESS addr, int enabled);
static int create_bkpt_defined = 0;

static void (*saved_delete_bkpt)(struct breakpoint *b);
static void (*users_delete_bkpt)(GDB_ADDRESS addr, int enabled);
static int delete_bkpt_defined = 0;

static void (*saved_modify_bkpt)(struct breakpoint *b);
static void (*users_modify_bkpt)(GDB_ADDRESS addr, int enabled);
static int modify_bkpt_defined = 0;

static void (*saved_attach_hook)(void);
static void (*users_attach_hook)(int pid);
static int attach_hook_defined = 0;

static void (*saved_detach_hook)(void);
static void (*users_detach_hook)(void);
static int detach_hook_defined = 0;

static void (*saved_register_changed_hook)(int regno);
static void (*users_register_changed_hook)(void);
static int register_changed_hook_defined = 0;

static void (*saved_memory_changed_hook)(CORE_ADDR addr, int len);
static void (*users_memory_changed_hook)(GDB_ADDRESS addr, int len);
static int memory_changed_hook_defined = 0;

static void (*saved_context_hook)(int pid);
static void (*users_context_hook)(int pid);
static int context_hook_defined = 0;

static void (*saved_error_begin_hook)(void);
static void (*users_error_begin_hook)(void);
static int error_begin_hook_defined = 0;

static void (*saved_file_changed_hook)(char *filename);
static void (*users_file_changed_hook)(char *filename);
static int file_changed_hook_defined = 0;

static void (*saved_exec_file_display_hook)(char *filename);
static void (*users_exec_file_display_hook)(char *filename);
static int exec_file_display_hook_defined = 0;

static void (*saved_rl_startup_hook)(void);
static void (*users_rl_startup_hook)(void);
static int rl_startup_hook_defined = 0;

static void (*saved_readline_begin_hook)(char *format, ...);
static void (*users_readline_begin_hook)(char *prompt);
static int readline_begin_hook_defined = 0;

static char *(*saved_command_line_input_hook)(char *prompt);
static char *(*users_command_line_input_hook)(char *prompt);
static int command_line_input_hook_defined = 0;

static void (*saved_readline_end_hook)(void);
static void (*users_readline_end_hook)(void);
static int readline_end_hook_defined = 0;

static void (*saved_state_change_hook)(Debugger_state new_state);
static void (*users_state_change_hook)(GdbState new_state);
static int state_change_hook_defined = 0;

static void (*saved__word__completion_hook)(int save_cursor);
static void (*users__word__completion_hook)(int save_cursor);
static int __word__completion_hook_defined = 0;

static void (*saved__word__completion_query_hook)(GDB_FILE *stream, char *format, ...);
static void (*users__word__completion_query_hook)(GDB_FILE *stream, char *prompt);
static int __word__completion_query_hook_defined = 0;

static void (*saved__word__completion_read_hook)(void);
static void (*users__word__completion_read_hook)(void);
static int __word__completion_read_hook_defined = 0;

#if 0
static int (*saved_rl_getc_function)(FILE *);
static int (*users_rl_getc_function)(int);
static int rl_getc_function_defined = 0;
#endif

static void (*saved_rl_redisplay_function)(void);
static char *(*users_rl_redisplay_function)(char *);
static int rl_redisplay_function_defined = 0;

static void (*saved_interactive_hook)(void);
static void (*users_interactive_hook)(void);
static int interactive_hook_defined = 0;

/*--------------------------------------------------------------------------------------*/

/*----------------------*
 | my_call_command_hook |
 *----------------------*/
 
static void my_call_command_hook(struct cmd_list_element *c, char *arg, int from_tty)
{
    if (users_call_command(arg, from_tty)) {
	if (saved_call_command_hook)
	    saved_call_command_hook(c, arg, from_tty);
    	(*c->function.cfunc)(arg, from_tty);
    }
}


/*-------------*
 | my_set_hook |
 *-------------*/

static void my_set_hook(struct cmd_list_element *c)
{
    void 	 *value;
    Gdb_Set_Type type;
    
    __my_set_hook_guts(c, &type, &value);
    users_set_hook(c->name, type, value, 0, input_from_terminal_p());
    
    if (saved_set_hook)
    	saved_set_hook(c);
}


/*---------------*
 | my_query_hook |
 *---------------*/

static int my_query_hook(const char *format, va_list ap)
{
    int  result = 0;
    char msg[1024];
    
    vsprintf(msg, format, ap);
    
    if (users_query_hook(msg, &result)) {
        deprecated_query_hook = NULL;
        result = query("%s", msg);
	deprecated_query_hook = my_query_hook;
	if (users_query_after_hook)
	    result = users_query_after_hook(result);
    }
    
    return (result);
}


/*-----------------*
 | my_warning_hook |
 *-----------------*/

static void my_warning_hook(const char *format, va_list ap)
{
    char msg[1024];
    int result = 0;
    
    vsprintf(msg, format, ap);
    
    if (users_warning_hook(msg)) {
    	deprecated_warning_hook = NULL;
    	warning("%s", msg);
	deprecated_warning_hook = my_warning_hook;
    }
}


/*---------------------------*
 | my_create_breakpoint_hook |
 *---------------------------*/

static void my_create_breakpoint_hook(struct breakpoint *b)
{
    users_create_bkpt((unsigned long)b->loc->address, b->enable_state == bp_enabled);
    if (saved_create_bkpt)
    	saved_create_bkpt(b);
}


/*---------------------------*
 | my_delete_breakpoint_hook |
 *---------------------------*/

static void my_delete_breakpoint_hook(struct breakpoint *b)
{
    users_delete_bkpt((unsigned long)b->loc->address, b->enable_state == bp_enabled);
    if (saved_delete_bkpt)
    	saved_delete_bkpt(b);
}


/*---------------------------*
 | my_modify_breakpoint_hook |
 *---------------------------*/

static void my_modify_breakpoint_hook(struct breakpoint *b)
{
    users_modify_bkpt((unsigned long)b->loc->address, b->enable_state == bp_enabled);
    if (saved_modify_bkpt)
    	saved_modify_bkpt(b);
}


/*----------------*
 | my_attach_hook |
 *----------------*/

static void my_attach_hook(void)
{
    users_attach_hook(PIDGET(inferior_ptid));
    if (saved_attach_hook)
    	saved_attach_hook();
}


/*----------------*
 | my_detach_hook |
 *----------------*/

static void my_detach_hook(void)
{
    users_detach_hook();
    if (saved_detach_hook)
    	saved_detach_hook();
}


/*--------------------------*
 | my_register_changed_hook |
 *--------------------------*/

static void my_register_changed_hook(int ignore)
{
    users_register_changed_hook();
    if (saved_register_changed_hook)
    	saved_register_changed_hook(ignore);
}


/*------------------------*
 | my_memory_changed_hook |
 *------------------------*/

static void my_memory_changed_hook(CORE_ADDR addr, int len)
{
    users_memory_changed_hook((GDB_ADDRESS)addr, len);
    if (saved_memory_changed_hook)
    	saved_memory_changed_hook(addr, len);
}


/*-----------------*
 | my_context_hook |
 *-----------------*/

static void my_context_hook(int pid)
{
    users_context_hook(pid);
    if (saved_context_hook)
    	saved_context_hook(pid);
}


/*---------------------*
 | my_error_begin_hook |
 *---------------------*/

static void my_error_begin_hook(void)
{
    users_error_begin_hook();
    if (saved_error_begin_hook)
    	saved_error_begin_hook();
}


/*----------------------*
 | my_file_changed_hook |
 *----------------------*/

static void my_file_changed_hook(char *filename)
{
    users_file_changed_hook(filename);
    if (saved_file_changed_hook)
    	saved_file_changed_hook(filename);
}


/*---------------------------*
 | my_exec_file_display_hook |
 *---------------------------*/

static void my_exec_file_display_hook(char *filename)
{
    users_exec_file_display_hook(filename);
    if (saved_exec_file_display_hook)
    	saved_exec_file_display_hook(filename);
}


/*--------------------*
 | my_rl_startup_hook |
 *--------------------*/

static void my_rl_startup_hook(void)
{
    users_rl_startup_hook();
    if (saved_rl_startup_hook)
    	saved_rl_startup_hook();
}


/*------------------------*
 | my_readline_begin_hook |
 *------------------------*/

static void my_readline_begin_hook(char *format, ...)
{
    va_list ap;
    char    prompt[1024];
    
    va_start(ap, format);
    vsprintf(prompt, format, ap);
    va_end(ap);
    
    users_readline_begin_hook(prompt);
    if (saved_readline_begin_hook)
    	saved_readline_begin_hook("%s", prompt);
}


/*----------------------------*
 | my_command_line_input_hook |
 *----------------------------*/

static char *my_command_line_input_hook(char *prompt, int repeat, char *annotation_suffix)
{
    char *line = users_command_line_input_hook(prompt);
    if (!line && saved_command_line_input_hook)
    	line = saved_command_line_input_hook(prompt);
	
    if (!line) {
    	command_line_input_hook = NULL;
    	line = command_line_input(prompt, repeat, annotation_suffix);
        command_line_input_hook = my_command_line_input_hook;
    }
    
    return (line);
}


/*----------------------*
 | my_readline_end_hook |
 *----------------------*/

static void my_readline_end_hook(void)
{
    users_readline_end_hook();
    if (saved_readline_end_hook)
    	saved_readline_end_hook();
}


/*----------------------*
 | my_state_change_hook |
 *----------------------*/

static void my_state_change_hook(Debugger_state new_state)
{
    GdbState my_state;
    
    switch (new_state) {
	case STATE_NOT_ACTIVE:		  	my_state = Gdb_Not_Active;     break;
	case STATE_ACTIVE:			my_state = Gdb_Active; 	       break;
	case STATE_INFERIOR_LOADED:		my_state = Gdb_Target_Loaded;  break;
	case STATE_INFERIOR_EXITED:		my_state = Gdb_Target_Exited;  break;
	case STATE_INFERIOR_LOGICALLY_RUNNING: 	my_state = Gdb_Target_Running; break;
	case STATE_INFERIOR_STOPPED:		my_state = Gdb_Target_Stopped; break;
	default:			  	return;
    }
    
    users_state_change_hook(my_state);
    if (saved_state_change_hook)
    	saved_state_change_hook(new_state);
}


/*---------------------------*
 | my__word__completion_hook |
 *---------------------------*/

static void my__word__completion_hook(int save_cursor)
{
    users__word__completion_hook(save_cursor);
    if (saved__word__completion_hook)
    	saved__word__completion_hook(save_cursor);
}


/*---------------------------------*
 | my__word__completion_query_hook |
 *---------------------------------*/

static void my__word__completion_query_hook(GDB_FILE *stream, char *format, ...)
{
    va_list ap;
    char    prompt[1024];
    
    va_start(ap, format);
    vsprintf(prompt, format, ap);
    va_end(ap);
    
    users__word__completion_query_hook(stream, prompt);
    if (saved__word__completion_query_hook)
    	saved__word__completion_query_hook(stream, prompt);
}


/*--------------------------------*
 | my__word__completion_read_hook |
 *--------------------------------*/

static int my__word__completion_read_hook(void)
{
    users__word__completion_read_hook();
    if (saved__word__completion_read_hook)
    	saved__word__completion_read_hook();
}


#if 0
/*---------------------*
 | my_rl_getc_function |
 *---------------------*/

static int my_rl_getc_function(FILE *stream)
{
    int c = ((int (*)(FILE*))saved_rl_getc_function)(stream);
    return (users_rl_getc_function(c));
}
#endif


/*--------------------------*
 | my_rl_redisplay_function |
 *--------------------------*
 
 This intercepts ALL output to gdb readline's rl_redisplay() (in readline/display.c) but
 we only call the user's handler when the prompt buffer (rl_display_prompt) is not 
 the general gdb prompt buffer, rl_prompt.  When it isn't, then it is used for history
 display prompts like "(reverse-i-search)".  Those are the ones we want to let the user
 handle.
*/

static void my_rl_redisplay_function(void)
{
    if (rl_display_prompt && rl_display_prompt != rl_prompt)
    	rl_display_prompt = users_rl_redisplay_function(rl_display_prompt);
    if (saved_rl_redisplay_function)
    	saved_rl_redisplay_function();
}


/*---------------------*
 | my_interactive_hook |
 *---------------------*/

static void my_interactive_hook(void)
{
    users_interactive_hook();
    if (saved_interactive_hook)
    	saved_interactive_hook();
}

/*--------------------------------------------------------------------------------------*/

/*-------------------------------------------------*
 | gdb_special_events - set user-defined gdb-hooks |
 *-------------------------------------------------*
 
 This is a low-level interface to allow you to be notified when certain events occur
 within gdb.  The kinds of events supported are defined by GdbEvent.  For each of those
 events a specific callback function can be specified when that event occurs.  The
 prototypes for the callbacks are a function of the GdbEvent's and are summarized in the
 code below along with additional comments about each event.
 
 Passing NULL for an callback effectively removes that callback.
*/

void gdb_special_events(GdbEvent theEvent, void (*callback)())
{
    switch (theEvent) {
    	case Gdb_Before_Command:
	    /* int callback(char *arg, int from_tty);					*/
	   
	    /* The arguments are the same as for a gdb plugin (arg is the command line	*/
	    /* arguments).  However this callback returns and int. If it returns 0 then	*/
	    /* the command is not executed by gdb.  Otherwise it is.			*/
	   
	    if (callback) {
		saved_call_command_hook      = deprecated_call_command_hook;
		users_call_command           = (int (*)(char *, int))callback;
		deprecated_call_command_hook = my_call_command_hook;
		call_command_defined         = 1;
	    } else if (call_command_defined) {
	    	deprecated_call_command_hook = saved_call_command_hook;
		call_command_defined         = 0;
	    }
	    break;
    	
	case Gdb_After_SET_Command:
	    /* void callback(char *theSetting, Gdb_Set_Type type, void *value,int show);*/
       
	    /* This is identical to the routine specified to gdb_define_set_generic() 	*/
	    /* and the callback prototype is actually defined by Gdb_Set_Funct.  You 	*/
	    /* should use gdb_define_set_generic() and not this interface for defining	*/
	    /* the generic SET handler.							*/
	   
	    if (callback) {
		saved_set_hook      = deprecated_set_hook;
		users_set_hook      = (Gdb_Set_Funct)callback;
		deprecated_set_hook = my_set_hook;
		set_hook_defined = 1;
	    } else if (set_hook_defined) {
	    	deprecated_set_hook = saved_set_hook;
		set_hook_defined    = 0;
	    }
	    break;
    	
	case Gdb_Before_Query:
	    /* int callback(const char *prompt, int *result);				*/

	    /* Called just before a prompt to a "y"/"n" query prompt.  If the callback	*/
	    /* returns 0 then query will NOT be displayed and a 0 or 1 should be 	*/
	    /* returned by the callback in its result parameter.  If the callback 	*/
	    /* returns 0 then the result parameter is ignored and the prompt is 	*/
	    /* displayed.								*/
	   
	    /* IMPLEMENTATION NOTE: __initialize_io(), which is called as part of the	*/
	    /* gdb support initialization sets __default_gdb_query_hook to query_hook	*/
	    /* at that time.  __default_gdb_query_hook is used by I/O redirection to	*/
	    /* call that hook as part of the query hook support associated with the	*/
	    /* redirection.  If the user sets a Gdb_Before_Query event then we must	*/
	    /* chain our new query hook here into the __default_gdb_query_hook so that	*/
	    /* the event query hook will get called by the redirection query hook.	*/
	    
	    if (callback) {
		saved_query_hook  	      = deprecated_query_hook;
		users_query_hook   	      = (int (*)(const char *, int *))callback;
		deprecated_query_hook         = my_query_hook;
		query_hook_defined	      = 1;
		saved__default_gdb_query_hook = __default_gdb_query_hook;
		__default_gdb_query_hook      = my_query_hook;
	    } else if (query_hook_defined) {
	    	deprecated_query_hook         = saved_query_hook;
		query_hook_defined	      = 0;
		__default_gdb_query_hook      = saved__default_gdb_query_hook;
	    }
	    break;
	
	case Gdb_After_Query:
	    /* int callback(int result);						*/
	    
       	    /* Called just after a query response is read. The response to the query is	*/
       	    /* passed to the callback and the callback returns it's interpretation of	*/
       	    /* that query (or the same value).	Note that this event is only handled if	*/
       	    /* the Gdb_Before_Query event was specified.				*/
       	    
       	    /* Note that there is no corresponding hook to this in gdb.  We support it 	*/
       	    /* uniquely here.								*/
       	    
	    if (callback) {
		users_query_after_hook   = (int (*)(int))callback;
		query_after_hook_defined = 1;
	    } else if (query_after_hook_defined)
		query_after_hook_defined = 0;
	    break;
       	    
    	
	case Gdb_Before_Warning:
	    /* int callback(const char *message);					*/
	   
	    /* Called just before a warning message is displayed.  If the callback 	*/
	    /* returns 0 the warning is not displayed.  Otherwise it is.		*/
	   
	    if (callback) {
		saved_warning_hook      = deprecated_warning_hook;
		users_warning_hook      = (int (*)(const char *))callback;
		deprecated_warning_hook = my_warning_hook;
		warning_hook_defined    = 1;
	    } else if (warning_hook_defined) {
	    	deprecated_warning_hook = saved_warning_hook;
		warning_hook_defined    = 0;
	    }
	    break;
	    
	case Gdb_After_Creating_Breakpoint:
	    /* void callback(GDB_ADDRESS address, int enabled);				*/
	   
	    /* Called just after a new breakpoint, whatchpoint, or tracepoint is 	*/
	    /* created.  If the breakpoint is currently enabled (it wont if it's for an	*/
	    /* outer  scope), the enabled is passed as 1.				*/
	   
	    if (callback) {
		saved_create_bkpt                 = deprecated_create_breakpoint_hook;
		users_create_bkpt                 = (void (*)(GDB_ADDRESS addr, int enabled))callback;
		deprecated_create_breakpoint_hook = my_create_breakpoint_hook;
		create_bkpt_defined               = 1;
	    } else if (create_bkpt_defined) {
	    	deprecated_create_breakpoint_hook = saved_create_bkpt;
		create_bkpt_defined               = 0;
	    }
	    break;
	    
	case Gdb_Before_Deleting_Breakpoint:
	    /* void callback(GDB_ADDRESS address, int enabled);				*/
	   
	    /* Same as Gdb_After_Creating_Breakpoint except the callback is notified	*/
	    /* when the breakpoint, whatchpoint, or tracepoint is deleted.		*/
	   
	    if (callback) {
		saved_delete_bkpt                 = deprecated_delete_breakpoint_hook;
		users_delete_bkpt                 = (void (*)(GDB_ADDRESS addr, int enabled))callback;
		deprecated_delete_breakpoint_hook = my_delete_breakpoint_hook ;
		delete_bkpt_defined               = 1;
	    } else if (delete_bkpt_defined) {
	    	deprecated_delete_breakpoint_hook = saved_delete_bkpt;
		delete_bkpt_defined               = 0;
	    }
	    break;
	    
	case Gdb_After_Modified_Breakpoint:
	    /* void callback(GDB_ADDRESS address, int enabled);				*/
	   
	    /* Same as Gdb_After_Creating_Breakpoint except the callback is notified	*/
	    /* when the breakpoint, whatchpoint, or tracepoint is modified.		*/
	   
	    if (callback) {
		saved_modify_bkpt                 = deprecated_modify_breakpoint_hook;
		users_modify_bkpt                 = (void (*)(GDB_ADDRESS addr, int enabled))callback;
		deprecated_modify_breakpoint_hook = my_modify_breakpoint_hook ;
		modify_bkpt_defined               = 1;
	    } else if (modify_bkpt_defined) {
	    	deprecated_modify_breakpoint_hook = saved_modify_bkpt;
		modify_bkpt_defined               = 0;
	    }
	    break;
	    
	case Gdb_After_Attach:
	    /* void callback(int pid);							*/
	   
	    /* Called after a process is attached to gdb as the result if a ATTACH 	*/
	    /* command.									*/
	   
	    if (callback) {
		saved_attach_hook      = deprecated_attach_hook;
		users_attach_hook      = (void (*)(void))callback;
		deprecated_attach_hook = my_attach_hook;
		attach_hook_defined = 1;
	    } else if (attach_hook_defined) {
	    	deprecated_attach_hook = saved_attach_hook;
		attach_hook_defined    = 0;
	    }
	    break;
	
	case Gdb_Before_Detach:
	    /* void callback(void);							*/
	   
	    /* Called before a process is detached to gdb as the result if a DETACH 	*/
	    /* command.									*/
	   
	    if (callback) {
		saved_detach_hook              = deprecated_detach_hook;
		users_detach_hook              = (void (*)(void))callback;
		deprecated_detach_hook         = my_detach_hook;
		detach_hook_defined            = 1;
	    } else if (detach_hook_defined) {
	    	deprecated_detach_hook         = saved_detach_hook;
		detach_hook_defined = 0;
	    }
	    break;
	    
	case Gdb_After_Register_Changed:
	    /* void callback(void);							*/
	   
	    /* When a target program's register is changed by gdb this callback is	*/
	    /* called.									*/
	    
	    if (callback) {
		saved_register_changed_hook      = deprecated_register_changed_hook;
		users_register_changed_hook      = (void (*)(void))callback;
		deprecated_register_changed_hook = my_register_changed_hook;
		register_changed_hook_defined    = 1;
	    } else if (register_changed_hook_defined) {
	    	deprecated_register_changed_hook = saved_register_changed_hook;
		register_changed_hook_defined    = 0;
	    }
	    break;
	
	case Gdb_After_Memory_Changed:
	    /* void callback(GDB_ADDRESS address, int length);				*/
	   
	    /* When the target program's memory is changed by gdb this callback is	*/
	    /* called.  The target address and the amount of memory changed is passed.	*/
	    
	    if (callback) {
		saved_memory_changed_hook      = deprecated_memory_changed_hook;
		users_memory_changed_hook      = (void (*)(GDB_ADDRESS, int))callback;
		deprecated_memory_changed_hook = my_memory_changed_hook;
		memory_changed_hook_defined    = 1;
	    } else if (memory_changed_hook_defined) {
	    	deprecated_memory_changed_hook = saved_memory_changed_hook;
		memory_changed_hook_defined    = 0;
	    }
	    break;
	    
	case Gdb_Context_Is_Changed:
	    /* void callback(int pid);
	   
	    /* Called when gdb switches to a new process (i.e., when it prints 		*/
	    /* "Switching to thread N", where N is the process id which is also passed	*/
	    /* to the callback.								*/
	    
	    if (callback) {
		saved_context_hook      = deprecated_context_hook;
		users_context_hook      = (void (*)(int))callback;
		deprecated_context_hook = my_context_hook;
		context_hook_defined    = 1;
	    } else if (context_hook_defined) {
	    	deprecated_context_hook = saved_context_hook;
		context_hook_defined    = 0;
	    }
	    break;
	    
	case Gdb_Before_Error:
	    /* void callback(void);							*/
       
	    /* Called just before an error message is about to be displayed.		*/
	    
	    /* Note that if stderr output is redirected then your output filter will get*/
	    /* the error output but gdb's internal error recovery will cause it to not	*/
	    /* return to the plugin that caused the error (just like calling 		*/
	    /* gdb_error()).  You can use the Gdb_Before_Error to detect the errors but */
	    /* you should always return from the callback.  Don't try, for example, to 	*/
	    /* use setjmp/longjmp because you'll confuse gdb's error recover which will	*/
	    /* lead to a fatal error.							*/
	    
	    /* If you must do something that wants to quietly detect that the operation	*/
	    /* will result in an error then use gdb_execute_command_silent() or if 	*/
	    /* applicable, gdb_eval_silent() which know how to silently detect and 	*/
	    /* recover from errors as the result of executing some statement.		*/
	    
	    if (callback) {
		saved_error_begin_hook      = deprecated_error_begin_hook;
		users_error_begin_hook      = (void (*)(void))callback;
		deprecated_error_begin_hook = my_error_begin_hook;
		error_begin_hook_defined    = 1;
	    } else if (error_begin_hook_defined) {
	    	deprecated_error_begin_hook = saved_error_begin_hook;
		error_begin_hook_defined    = 0;
	    }
	    break;
	    
	case Gdb_After_File_Changed:
	    /* void callback(char *filename);						*/
	   
	    /* Called after processing a FILE command.  The filename from the FILE	*/
	    /* command is passed.							*/
	    
	    if (callback) {
		saved_file_changed_hook      = deprecated_file_changed_hook;
		users_file_changed_hook      = (void (*)(char *))callback;
		deprecated_file_changed_hook = my_file_changed_hook;
		file_changed_hook_defined    = 1;
	    } else if (file_changed_hook_defined) {
	    	deprecated_file_changed_hook = saved_file_changed_hook;
		file_changed_hook_defined    = 0;
	    }
	    break;
	
	case Gdb_After_Attach_To_File:
	    /* void callback(char *filename);						*/
	   
	    /* Called after processing a FILE or (ATTACH (if it can figure out the 	*/
	    /* file) command.  The filename is the pathname of the inferior or NULL.	*/
	    
	    if (callback) {
		saved_exec_file_display_hook      = deprecated_exec_file_display_hook;
		users_exec_file_display_hook      = (void (*)(char *))callback;
		deprecated_exec_file_display_hook = my_exec_file_display_hook;
		exec_file_display_hook_defined    = 1;
	    } else if (exec_file_display_hook_defined) {
	    	deprecated_exec_file_display_hook = saved_exec_file_display_hook;
		exec_file_display_hook_defined    = 0;
	    }
	    break;
	
	case Gdb_Before_Prompt:
	    /* void callback(void);							*/
	   
	    /* Called just before any prompt is about to be displayed.			*/
	    
	    if (callback) {
		saved_rl_startup_hook   = rl_startup_hook;
		users_rl_startup_hook   = (void (*)(void))callback;
		rl_startup_hook         = my_rl_startup_hook;
		rl_startup_hook_defined = 1;
	    } else if (rl_startup_hook_defined) {
	    	rl_startup_hook         = saved_rl_startup_hook;
		rl_startup_hook_defined = 0;
	    }
	    break;
    
	case Gdb_Begin_ReadRawLine:
	    /* void callback(char *);							*/
	   
	    /* Called at the start of reading "raw" input lines.  Such lines are the	*/
	    /* lines contained in DEFINE and DOCUMENT and the outer-most WHILE and IF 	*/
	    /* commands.								*/
	    
	    if (callback) {
		saved_readline_begin_hook      = deprecated_readline_begin_hook;
		users_readline_begin_hook      = (void (*)(char *))callback;
		deprecated_readline_begin_hook = my_readline_begin_hook;
		readline_begin_hook_defined    = 1;
	    } else if (readline_begin_hook_defined) {
	    	deprecated_readline_begin_hook = saved_readline_begin_hook;
		readline_begin_hook_defined    = 0;
	    }
	    break;
	    
	case Gdb_ReadRawLine:
	    /* char *callback(char *);							*/
	   
	    /* Called to read raw data lines from the terminal.  The callback should	*/
	    /* either return a line or NULL. If NULL is returned gdb reads it normally	*/
	    /* would.									*/
	   
	    /* Note that gdb_define_raw_input_handler() uses this same mechanism to	*/
	    /* define a function to read raw lines and using that call is recommended.	*/ 
	    
	    if (callback) {
		saved_command_line_input_hook   = command_line_input_hook;
		users_command_line_input_hook   = (char *(*)(char *))callback;
		command_line_input_hook         = my_command_line_input_hook;
		command_line_input_hook_defined = 1;
	    } else if (command_line_input_hook_defined) {
	    	command_line_input_hook         = saved_command_line_input_hook;
		command_line_input_hook_defined = 0;
	    }
	    break;

	case Gdb_End_ReadRawLine:
	    /* void callback(void);							*/
	    
	    if (callback) {
		saved_readline_end_hook      = deprecated_readline_end_hook;
		users_readline_end_hook      = (void (*)(void))callback;
		deprecated_readline_end_hook = my_readline_end_hook;
		readline_end_hook_defined    = 1;
	    } else if (readline_end_hook_defined) {
	    	deprecated_readline_end_hook = saved_readline_end_hook;
		readline_end_hook_defined    = 0;
	    }
	    break;
 	
	case Gdb_State_Changed:
	    /* void callback(GdbState newState);					*/
	   
	    /* Called when gdb changes state.  The new state is as defined by GdbState.	*/
	    /* Note that gdb_define_exit_handler() uses this mechanism to call its	*/
	    /* specified exit handler when gdb quits (GdbState == Gdb_Not_Active).	*/
	    
	    if (callback) {
		saved_state_change_hook   = state_change_hook;
		users_state_change_hook   = (void (*)(GdbState))callback;
		state_change_hook         = my_state_change_hook;
		state_change_hook_defined = 1;
	    } else if (state_change_hook_defined) {
	    	state_change_hook         = saved_state_change_hook;
		state_change_hook_defined = 0;
	    }
	    break;
	    
	case Gdb_Word_Completion_Cursor:
	    /* void callback(int save_cursor);						*/
	   
	    /* Gdb word completion (i.e., displaying list of alternative names for 	*/
	    /* commands, filenames, etc. or displaying all alternatives) is unique since*/
	    /* it can cause a output display and/or prompt when certain keys are typed	*/
	    /* during keyboard input.  The prompt, if any, is only for a "y" or "n" 	*/
	    /* answer and is not a standard gdb query since typing the "y" or "n" does 	*/
	    /* not require a return to send it.  The prompt itself as well as the 	*/
	    /* displays are output to the current (possibly) redirected stdout stream. 	*/
	    /* However, because word completion occurs during input, it needs to do make*/
	    /* sure the cursor can be restored after the response to the prompt just in */
	    /* case the output is being redirected in some unexpected way.  By default	*/
	    /* it writes (to gdb_default_stderr) a standard xterm ESC 7 to save the 	*/
	    /* cursor and ESC 8 to restore it.  But a callback is provided if this is 	*/
	    /* not what's wanted or requires some additional processing.		*/
		   
	    /* The callback is sent a 1 to indicate the cursor is to be saved and 0 if	*/
	    /* it is to be restored.							*/
	  
	    if (callback) {
		saved__word__completion_hook    = __word__completion_hook;
		users__word__completion_hook    = (void (*)(int))callback;
		__word__completion_hook         = my__word__completion_hook;
		__word__completion_hook_defined = 1;
	    } else if (__word__completion_hook_defined) {
	    	__word__completion_hook         = saved__word__completion_hook;
		__word__completion_hook_defined = 0;
	    }
	    break;
	    
	case Gdb_Word_Completion_Query:
	    /* int callback(GDB_FILE *stream, char *format, ...);			*/
       
	    /* As mentioned above for Gdb_Word_Completion_Cursor word completion may	*/
	    /* issue a prompt, specifically "Display all N possibilities? (y or n) ",	*/
	    /* where N is the number of possibilities.  It will issue this and put up	*/
	    /* a read to the terminal awaiting a "y", "Y", or " " to indicate "yes" or	*/
	    /* a "n", "N", or rubout to indicate a "no" response (or CTL-G to indicate	*/
	    /* abort).  By specifying a Gdb_Word_Completion_Query callback you can do 	*/
	    /* the prompt and read yourself.						*/
	   
	    /* The callback takes fprintf-like parameters for a GDB_FILE stream and 	*/
	    /* should return 0 for "no" and non-zero for "yes".				*/
	    
	    if (callback) {
		saved__word__completion_query_hook    = __word__completion_query_hook;
		users__word__completion_query_hook    = (void (*)(GDB_FILE*, char*))callback;
		__word__completion_query_hook         = my__word__completion_query_hook;
		__word__completion_query_hook_defined = 1;
	    } else if (__word__completion_query_hook_defined) {
	    	__word__completion_query_hook         = saved__word__completion_query_hook;
		__word__completion_query_hook_defined = 0;
	    }
	    break;
	
	case Gdb_Word_Completion_Read:
	    /* int callback(void);							*/
	   
	    /* The Gdb_Word_Completion_Query callback is, as described above, used to	*/
	    /* intercept the query prompt for a "yes" or "no" answer.  The		*/
	    /* Gdb_Word_Completion_Read callback allows you to intercept the read and 	*/
	    /* do it yourself.								*/
	   
	    /* The callback should return 0 for "no" and non-zero for "yes".  The	*/
	    /* default convention is to accept "y", "Y", or " " to indicate "yes" and	*/
	    /* "n", "N", or rubout to indicate a "no" (and a CTL-G to indicate abort).	*/
	    /* No return is necessary.  						*/

	    if (callback) {
		saved__word__completion_read_hook    = __word__completion_read_hook;
		users__word__completion_read_hook    = (void (*)(void))callback;
		__word__completion_read_hook         = my__word__completion_read_hook;
		__word__completion_read_hook_defined = 1;
	    } else if (__word__completion_read_hook_defined) {
	    	__word__completion_read_hook         = saved__word__completion_read_hook;
		__word__completion_read_hook_defined = 0;
	    }
	    break;
	
	#if 0
	case Gdb_Raw_Terminal_Getc:
	    /* int callback(int c);							*/
	    
	    /* A Gdb_Raw_Terminal_Getc callback intercepts raw terminal character input	*/
	    /* before any normal readline processing, i.e., all escapes, backspaces,	*/
	    /* etc. pass through this callback.  					*/
	    
	    /* The most recent character typed to the keyboard is passed to the callback*/
	    /* and the callback should return it or some appropriate substitute.	*/
	    
	    if (callback) {
		saved_rl_getc_function   = rl_getc_function;;
		users_rl_getc_function   = (int (*)(int))callback;
		rl_getc_function         = my_rl_getc_function;
		rl_getc_function_defined = 1;
	    } else if (rl_getc_function_defined) {
	    	rl_getc_function         = saved_rl_getc_function;
		rl_getc_function_defined = 0;
	    }
	    break;
	#endif
	
	case Gdb_History_Prompt:
	    /* char *callback(char *display_prompt);					*/
	    
	    /* A Gdb_History_Prompt callback intercepts the gdb prompt when it is  	*/
	    /* trying to display a history prompt, e.g., when CTRL-R is entered and the	*/
	    /* prompt to be shown is "(reverse-i-search)".				*/
	    
	    /* The callback is given the history prompt and should return a prompt.	*/
	    /* This can either be the ORIGINAL unmodified input prompt or another prompt*/
	    /* in a buffer controlled by the callback.  It should NOT modify the input	*/
	    /* prompt.									*/
	   
	    /* Note that the callback is called for every character before it is echoed	*/
	    /* to the display.  If the callback returns a modified prompt it should not */
	    /* assume the input prompt on the next call will be the same as the one	*/
	    /* returned on the previous call.  Indeed, it will always be the one gdb	*/
	    /* wants to display for the history prompt.					*/
	   
	    /* Also note, gdb displays the prompt AFTER positioning the cursor to the	*/
	    /* start of the line it is on.						*/
	    	    
	    if (callback) {
		saved_rl_redisplay_function   = rl_redisplay_function;;
		users_rl_redisplay_function   = (char *(*)(char *))callback;
		rl_redisplay_function         = my_rl_redisplay_function;
		rl_redisplay_function_defined = 1;
	    } else if (rl_redisplay_function_defined) {
	    	rl_redisplay_function         = saved_rl_redisplay_function;
		rl_redisplay_function_defined = 0;
	    }
	    break;
	
	case Gdb_Interactive:
	    /* void callback(void);							*/
       
	    /* Generally called when gdb is in compute bound tasks.  Could be used to	*/
	    /* provide some kind of feedback that something is going on.		*/
	    
	    if (callback) {
		saved_interactive_hook      = deprecated_interactive_hook;
		users_interactive_hook      = (void (*)(void))callback;
		deprecated_interactive_hook = my_interactive_hook;
		interactive_hook_defined    = 1;
	    } else if (interactive_hook_defined) {
	    	deprecated_interactive_hook = saved_interactive_hook;
		interactive_hook_defined    = 0;
	    }
	    break;
	    
	default:
	    gdb_internal_error("gdb_special_events() with an invalid GdbEvent");
   }
}
