/*
 * (c) Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms
 * and conditions for redistribution.
 */
#ifndef _X_SIGNALS
#define _X_SIGNALS

#include "defs.h"

#if defined(NO_POSIX_SIGS)
int sigprocmask(int how,sigset_t *set,sigset_t *oset);
int sigaction(int sig,struct sigaction *sap,struct sigaction *osap);
#endif
status_e signal_init(void);
char *sig_name(int sig);
void signal_default_state(void);
void check_pipe(void);

#endif
