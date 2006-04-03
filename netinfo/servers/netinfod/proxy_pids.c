/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * We track the readall proxy pids, so we can signal them when we
 * shut down.  
 */

#include <NetInfo/config.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <NetInfo/system_log.h>
#include "ni_globals.h"

/* Nothing magic about these delay times; they just seem sorta right */
#define PROXY_TERM_SEC	2
#define PROXY_TERM_USEC	0
#define PROXY_KILL_SEC	0
#define PROXY_KILL_USEC	(5 * 100 * 1000)	/* 1/2 second in usec */

static unsigned long readall_proxy_pids[MAX_READALL_PROXIES][2];
static int nproxies;

extern int kill(int, int);

void
initialize_readall_proxies(unsigned max_pids)
{
	int i;

	for (i = 0; i < MAX_READALL_PROXIES; i++)
	{
		readall_proxy_pids[i][0] = 0;
		readall_proxy_pids[i][1] = 0;
	}

	nproxies = 0;
}

void add_proxy(const int pid, const unsigned long addr)
{
	if (nproxies == MAX_READALL_PROXIES)
	{
		system_log(LOG_ALERT, "Proxy table overflow (%u proxies)",
			nproxies);
		exit(1);
	}

	readall_proxy_pids[nproxies][0] = pid;
	readall_proxy_pids[nproxies][1] = addr;
	nproxies++;
}

void remove_proxy(const int pid)
{
	int i;

	for (i = 0; i < nproxies; i++)
		if (readall_proxy_pids[i][0] == pid) break;


	for (i++; i < nproxies; i++)
	{
		readall_proxy_pids[i-1][0] = readall_proxy_pids[i][0];
		readall_proxy_pids[i-1][1] = readall_proxy_pids[i][1];
	}

	nproxies--;

	readall_proxy_pids[nproxies][0] = 0;
	readall_proxy_pids[nproxies][1] = 0;

	return;
}

/*
 * Get information, like the IP address.
 */
unsigned long get_proxy(const int pid)
{
	int i;

	for (i = 0; i < nproxies; i++)
	{
		if (readall_proxy_pids[i][0] == pid)
			return readall_proxy_pids[i][1];
	}

	return 0;
}

bool_t kill_proxies(void)
{
	struct timeval proxy_termtime = {PROXY_TERM_SEC, PROXY_TERM_USEC};
	struct timeval proxy_killtime = {PROXY_KILL_SEC, PROXY_KILL_USEC};

	if (readall_proxies <= 0) return(TRUE);

	if (killpg(process_group, SIGUSR1) != 0)
	{
		switch(errno)
		{
			case ESRCH:
				/* Pgrp isn't there: proxies all gone? */
				system_log(LOG_INFO,
					"expected proxies not there for termination");
				return(TRUE);

			default:
				system_log(LOG_WARNING, "couldn't terminate proxies - %m");
			return(FALSE);
		}
	}
 
	select(0, NULL, NULL, NULL, &proxy_termtime);

	if (killpg(process_group, SIGKILL) != 0)
	{
		/* Ensure they're gone */
		switch(errno)
		{
			case ESRCH:
				/* proxies all terminated */
				return(TRUE);
			default:
				system_log(LOG_WARNING, "couldn't kill proxies - %m");
				return(FALSE);
		}
	}
 
	select(0, NULL, NULL, NULL, &proxy_killtime);

	/* If we still have children, complain */
	if (killpg(getpid(), SIGKILL) != 0)
	{
		switch(errno)
		{
			case ESRCH:
				/* proxies all terminated */
				return(TRUE);
			default:
				system_log(LOG_WARNING, "couldn't test-kill proxies - %m");
				return(FALSE);
		}
	}

	/* Something was still there... */
	return(FALSE);
}
