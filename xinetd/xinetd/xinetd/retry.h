#ifndef RETRY_H
#define RETRY_H

#include "config.h"
#include "defs.h"

status_e schedule_retry(struct server *serp);
void cancel_service_retries(struct service *sp);

#endif
