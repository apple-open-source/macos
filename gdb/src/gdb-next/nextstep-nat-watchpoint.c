#include "nextstep-nat-dyld.h"
#include "nextstep-nat-inferior.h"
#include "nextstep-nat-mutils.h"
#include "nextstep-nat-sigthread.h"
#include "nextstep-nat-threads.h"
#include "nextstep-xdep.h"

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdbthread.h"

int next_mach_insert_watchpoint (CORE_ADDR addr, size_t len, int type)
{
  return -1;
}

int next_mach_remove_watchpoint (CORE_ADDR addr, size_t len, int type)
{
  return -1;
}

int next_mach_stopped_by_watchpoint ()
{
  return 0;
}

void state_change (unsigned long i)
{
}
