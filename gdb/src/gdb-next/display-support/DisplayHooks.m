#include "defs.h"
#include "frame.h"
#include "symtab.h"
#include "breakpoint.h"

#include <Foundation/Foundation.h>

#include <fcntl.h>

#include "ViewDisplayProvider_Protocol.h"
#include "GuiDisplayProvider_Protocol.h"
#include "GdbManager.h"
#include "DisplayTypes.h"
#include "DisplayHooks.h"
#include "DisplayMisc.h"

#ifndef ROOTED_P
#define SLASH_P(X) ((X)=='\\' || (X) == '/')
#define ROOTED_P(X) ((SLASH_P((X)[0]))|| ((X)[1] ==':'))
#endif

/* 'localException' is defined by NS_HANDLER */

#define EXCEPTION_MSG(func) \
NSLog (@"Exception sending remote message \"%@\": name: \"%@\", reason: \"%@\"", \
       func, [localException name], [localException reason]);

void
tell_displayer_display_lines
(struct symtab *symtab, int first_line, int last_line)
{
    id <ViewDisplayProvider> displayProvider = nil;

    if (gdbManager == nil) { return; }

    if (symtab->fullname == NULL) {
      symtab_to_filename (symtab);
      if (symtab->fullname == NULL) {
	return;
      }
    }
    if (last_line < first_line) {
      int t = last_line;
      last_line = first_line;
      first_line = t;
    }
    if (last_line > first_line) { 
      last_line -= 1;	/* I think last_line means up to but not including */
    }
    if (first_line != last_line) {
      extern int lines_to_list; /* from source.c */
      first_line = first_line + (lines_to_list / 2);
      last_line = first_line;
    }

    NS_DURING {
      displayProvider = [gdbManager displayProviderForProtocol:@protocol(ViewDisplayProvider)];

      if (displayProvider != nil) {

	/* rooted path */
	NSString *fileString = [NSString stringWithCString: symtab->fullname];
	
	[displayProvider lineChangedForThread: -1
			 inFile: fileString
			 atStartLine: first_line
			 toEndLine: last_line];
      }
    }
    NS_HANDLER {
      EXCEPTION_MSG (@"display_lines");
      shut_down_display_system ();
    }
    NS_ENDHANDLER;
}

void displayer_command_loop ()
{
  if (gdbManager == nil) { return; }
  [gdbManager doCommandLoop];
}

int 
tell_displayer_do_query (char *query, va_list args)
{
  id <GuiDisplayProvider2> displayProvider = nil;
  char *buf;
  int result = -1;
    
  if (gdbManager == nil) { return; }

  vasprintf (&buf, query, args);
  if (buf == NULL) {
    return 0;
  }

  NS_DURING {
    displayProvider = [gdbManager displayProviderForProtocol: @protocol (GuiDisplayProvider2)];
    if (displayProvider != nil) {
      result = [displayProvider query: [NSString stringWithCString: buf]];
    } else {
      result = 0;
    }
  }
  NS_HANDLER {
    EXCEPTION_MSG (@"query");
    shut_down_display_system ();
    result = 0;
  }
  NS_ENDHANDLER;

  free (buf);
  return result;
}

void
tell_displayer_fputs_output (const char *linebuffer, FILE *stream)
{
    GdbOutputType   oType = GDB_OUTPUT_OTHER;
    NSString *outputString = [NSString stringWithCString: linebuffer];
    
    if (gdbManager == nil) { return; }

    if (stream == gdb_stdout) {
        oType = GDB_OUTPUT_STDOUT;
    } else if (stream == gdb_stderr) {
        oType = GDB_OUTPUT_STDERR;
    }

    [gdbManager processOutput: outputString
                outputType: oType];
}

void
tell_displayer_state_changed(Debugger_state newState)
{
    id <ViewDisplayProvider> displayProvider = nil;

    if (gdbManager == nil) { return; }

    NS_DURING {

      displayProvider = [gdbManager displayProviderForProtocol: @protocol (ViewDisplayProvider)];
      if (displayProvider != nil) {
	DebuggerState s;
	switch (newState) {
	case STATE_NOT_ACTIVE:
	  s = DBG_STATE_NOT_ACTIVE;
	  break;
	case STATE_ACTIVE:
	  s = DBG_STATE_ACTIVE;
	  break;
	case STATE_INFERIOR_LOADED:
	  s = DBG_STATE_INFERIOR_LOADED;
	  break;
	case STATE_INFERIOR_EXITED:
	  s = DBG_STATE_INFERIOR_EXITED;
	  break;
	case STATE_INFERIOR_LOGICALLY_RUNNING:
	  s = DBG_STATE_INFERIOR_LOGICALLY_RUNNING;
	  break;
	case STATE_INFERIOR_STOPPED:
	  s = DBG_STATE_INFERIOR_STOPPED;
	  break;
	}
	[displayProvider inferiorStateChanged: s];
      }
    }
    NS_HANDLER {
      EXCEPTION_MSG (@"state_changed");
      shut_down_display_system ();
    }
    NS_ENDHANDLER;
    return;
}

static void
get_full_path_name (char *filename, char **fullname)
{
  extern char *source_path;
  int fd;

  fd = openp (source_path, 0, filename, O_RDONLY, 0, fullname);
  if (fd > 0) { close (fd); }
}

void
displayer_create_breakpoint_hook (struct breakpoint *bp)
{
  tell_displayer_breakpoint_changed (bp, BP_STATE_NEW);
}

void
displayer_delete_breakpoint_hook (struct breakpoint *bp)
{
  tell_displayer_breakpoint_changed (bp, BP_STATE_DELETED);
}

void
displayer_modify_breakpoint_hook (struct breakpoint *bp)
{
  tell_displayer_breakpoint_changed (bp, BP_STATE_OTHER_INFO_CHANGED);
}

void
tell_displayer_breakpoint_changed (struct breakpoint *bp, BreakpointState newState)
{
  char *fp = NULL;
  char *to_free_fp = NULL;
  id <GuiDisplayProvider> displayProvider = nil;
  int lineNumber = -1;

  if (gdbManager == nil) { return; }

  /* for now, only handle real breakpoints */

  if (bp->type != bp_breakpoint) { return; }
  if (bp->number <= 0) { return; }

  /*
   * I do not know whether the filename pointer of the breakpoint struct
   * is normally a full path or not.  If it is not a full path, I do not
   * know what effect it might have, if we replaced it with a full path.
   * Since I don't know, for the time being I'm not going to do it.
   *
   * This means that we may be recomputing the full path over and over.
   * That needs to be addressed (FIXME).  Moreover, I don't think that
   * the way get_full_path_name works (ie. by calling open) is very
   * efficient.  MVS -- it is not but it works, and is correct (i.e. uses
   * the current directory path) -- rhagy.
   */

  fp = bp->source_file;
  if (fp == NULL) { return; }
  if (! ROOTED_P (fp)) { /* rooted path? */
    to_free_fp = NULL;
    get_full_path_name (fp, &to_free_fp);
    if ((to_free_fp == NULL) || (! ROOTED_P (to_free_fp)))
      return;
    fp = to_free_fp;
  }
  lineNumber = bp->line_number;

  NS_DURING {

    displayProvider = [gdbManager displayProviderForProtocol:
				    @protocol(GuiDisplayProvider)];
    if (displayProvider != nil) {
      NSString *fileString;
      if (fp) {
	/* full path */
	fileString = [NSString stringWithCString: fp];
      } else {
	fileString = nil;
      }
      [displayProvider breakpointChanged: bp->number
		       newState: newState
		       inFile: fileString
		       atLine: lineNumber];
    }
  }
  NS_HANDLER {
    EXCEPTION_MSG (@"breakpoint_changed");
    shut_down_display_system ();
  }
  NS_ENDHANDLER;

  if (to_free_fp) {
    free (to_free_fp);
  }

  return;
}

void
tell_displayer_frame_changed (int newFrame)
{
  id <GuiDisplayProvider> displayProvider = nil;

  if (gdbManager == nil) { return; }

  NS_DURING {
    displayProvider = [gdbManager displayProviderForProtocol:@protocol(GuiDisplayProvider)];
    if (displayProvider != nil) {
      [displayProvider frameChanged: newFrame];
    }
  }
  NS_HANDLER {
    EXCEPTION_MSG(@"frame_changed");
    shut_down_display_system();
  }
  NS_ENDHANDLER;
  return;
}

void tell_displayer_stack_changed ()
{
  id <GuiDisplayProvider> displayProvider = NULL;
  int numFrames = -1;
  struct frame_info *f = NULL;
    
  if (gdbManager == nil) { return; }

  numFrames = 0;
  f = get_current_frame ();
  while (f != NULL) {
    numFrames++;
    f = get_prev_frame (f);
  }
    
  NS_DURING {
    displayProvider = [gdbManager displayProviderForProtocol:@protocol(GuiDisplayProvider)];
    if (displayProvider != nil) {
      [displayProvider stackChanged: numFrames
		       limitReached: 0];
    }
  }
  NS_HANDLER {
    EXCEPTION_MSG (@"stack_changed");
    shut_down_display_system ();
  }
  NS_ENDHANDLER;
}

const char *
tell_displayer_get_input (char *prompt, int repeat, char *anno)
{
  if (gdbManager == nil) { return ""; }

  if (prompt != NULL) {
    [gdbManager processOutput: [NSString stringWithCString: prompt]
		outputType: GDB_OUTPUT_STDOUT];
  }

  return [gdbManager waitForLineOfInput];
}
