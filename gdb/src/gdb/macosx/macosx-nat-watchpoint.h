#ifndef __GDB_MACOSX_NAT_WATCHPOINT_H__
#define __GDB_MACOSX_NAT_WATCHPOINT_H__

#include "defs.h"

struct target_waitstatus;

void macosx_enable_page_protection_events (int pid);
void macosx_disable_page_protection_events (int pid);
int macosx_can_use_hw_watchpoint (int type, int cnt, int ot);
int macosx_region_ok_for_hw_watchpoint (CORE_ADDR start, LONGEST len);
int macosx_insert_watchpoint (CORE_ADDR addr, size_t len, int type);
int macosx_remove_watchpoint (CORE_ADDR addr, size_t len, int type);
int macosx_stopped_by_watchpoint (struct target_waitstatus *w, int, int);

#endif /* __GDB_MACOSX_NAT_WATCHPOINT_H__ */
