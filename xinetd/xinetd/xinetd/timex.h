/*
 * (c) Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms
 * and conditions for redistribution.
 */
#ifndef _X_TIME
#define _X_TIME

#include "pset.h"
#include "defs.h"

bool_int ti_current_time_check(const pset_h intervals);
status_e ti_add(pset_h iset,const char *interval_str);
void ti_dump(pset_h iset,int fd);
void ti_free(pset_h iset);

#endif
