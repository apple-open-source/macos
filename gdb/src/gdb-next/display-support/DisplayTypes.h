#ifndef _GDB_DISPLAY_TYPES_H_
#define _GDB_DISPLAY_TYPES_H_

/* Used for View protocol */

/* State of the debugger apropos the inferior */

typedef enum {
  DBG_STATE_NOT_ACTIVE = 0,
  DBG_STATE_ACTIVE = 1,		  /* debugger running, no inferior */
  DBG_STATE_INFERIOR_LOADED = 2,  /* inferior loaded */
  DBG_STATE_INFERIOR_EXITED = 3,  /* exited either normally or due to crash */
  /* For GUI issues; when logically running, user has issued an
     explict run or continue command whereas the inferior will be
     running for a next or step command, it won't be logically
     running.  */
  DBG_STATE_INFERIOR_LOGICALLY_RUNNING = 4,
  DBG_STATE_INFERIOR_RUNNING = 5, /* running under control of the debugger */
  DBG_STATE_INFERIOR_STOPPED = 6  /* stopped at a breakpoint or due to attach */
} DebuggerState;

/* Used for the Gui protocol */

/* Breakpoint states */

typedef enum {
  BP_STATE_NEW = 0,
  BP_STATE_ENABLED = 1,
  BP_STATE_DISABLED = 2,
  BP_STATE_OTHER_INFO_CHANGED = 3,
  BP_STATE_DELETED = 4,
  BP_STATE_MOVED = 5
} BreakpointState;

/* Output from GDB types */

typedef enum {
  GDB_OUTPUT_STDOUT = 0,
  GDB_OUTPUT_STDERR = 1,
  GDB_OUTPUT_ANNOTATION = 2,
  GDB_OUTPUT_OTHER = 3
} GdbOutputType;

#endif /* _GDB_DISPLAY_TYPES_H_ */
