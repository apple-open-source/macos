#include "nextstep-nat-cfmthread.h"

#include "defs.h"
#include "gdbcmd.h"
#include "breakpoint.h"

static CORE_ADDR lookup_address (const char *s)
{
  struct minimal_symbol *msym;
  msym = lookup_minimal_symbol (s, NULL, NULL);
  if (msym == NULL) {
    error ("unable to locate symbol \"%s\"", s);
  }
  return SYMBOL_VALUE_ADDRESS (msym);
}

void next_cfm_thread_init (next_cfm_thread_status *s)
{
  s->notify_debugger = 0;
  s->info_api_cookie = 0;
}

void next_cfm_thread_create (next_cfm_thread_status *s, task_t task)
{
  struct breakpoint *b;
  struct symtab_and_line sal;
  char buf[64];

  snprintf (buf, 64, "PrepareClosure + %u", s->breakpoint_offset);
  s->notify_debugger = lookup_address ("PrepareClosure") + s->breakpoint_offset;

  INIT_SAL (&sal);
  sal.pc = s->notify_debugger;
  b = set_momentary_breakpoint (sal, NULL, bp_shlib_event);
  b->disposition = donttouch;
  b->thread = -1;
  b->addr_string = strsave (buf);

  breakpoints_changed ();
}

void next_cfm_thread_destroy (next_cfm_thread_status *s)
{
  s->notify_debugger = 0;
  s->info_api_cookie = 0;
}

void
_initialize_nextstep_nat_cfmthread ()
{
}
