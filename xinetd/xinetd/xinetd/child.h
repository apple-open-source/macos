/*
 * (c) Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms
 * and conditions for redistribution.
 */
#ifndef _X_CHILD
#define _X_CHILD

#include "defs.h"

void child_process(struct server *serp);
void child_exit(void);
void exec_server( const struct server *serp );

#endif
