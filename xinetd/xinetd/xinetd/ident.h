#ifndef IDENT_H
#define IDENT_H

#include "defs.h"

idresult_e log_remote_user(const struct server *serp,unsigned timeout);
const char *idresult_explain(idresult_e result);

#endif

