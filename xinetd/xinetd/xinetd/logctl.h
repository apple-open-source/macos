#ifndef LOGCTL_H
#define LOGCTL_H

#include "defs.h"
#include "xlog.h"
#include "log.h"

status_e log_start(struct service *sp,xlog_h *xhp);
void log_end(struct log *lp,xlog_h xh);

#endif

