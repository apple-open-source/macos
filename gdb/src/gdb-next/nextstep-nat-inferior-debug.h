#ifndef _NEXTSTEP_NAT_INFERIOR_DEBUG_H_
#define _NEXTSTEP_NAT_INFERIOR_DEBUG_H_

#include <stdio.h>
#include <mach/mach.h>

#include "defs.h"

extern FILE *inferior_stderr;
extern int inferior_debug_flag;

struct next_exception_data;
struct next_inferior_status;

void inferior_debug PARAMS ((int level, const char *fmt, ...));
void next_debug_task_port_info PARAMS ((mach_port_t task));
void next_debug_inferior_status PARAMS ((struct next_inferior_status *s));
void next_debug_exception PARAMS ((struct next_exception_data *e));
void next_debug_message PARAMS ((msg_header_t *msg));
void next_debug_notification_message PARAMS ((struct next_inferior_status *inferior, msg_header_t *msg));

const char *unparse_exception_type PARAMS ((unsigned int i));
const char *unparse_protection PARAMS ((vm_prot_t p));
const char *unparse_inheritance PARAMS ((vm_inherit_t i));

#endif /* _NEXTSTEP_NAT_INFERIOR_DEBUG_H_ */
