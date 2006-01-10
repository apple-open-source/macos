/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/ucred.h>
#include <sys/wait.h>

#include <sys/mount.h>
#include <sys/time.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

/* Global defs */
#ifdef DEBUG
int debug = 1;
#else
int debug = 0;
#endif
int *thread_status = NULL;
pthread_cond_t cond;
pthread_mutex_t mutex;

void nonfs __P((int));
void usage __P((void));
void *nfsiod_thread __P((void *));

/*
 * Nfsiod does asynchronous buffered I/O on behalf of the NFS client.
 * It does not have to be running for correct operation, but will
 * improve throughput.
 */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, num_servers;
	int i, rv, threadcnt;

#define	MAXNFSIODCNT      32
#define	DEFNFSIODCNT       1
	num_servers = DEFNFSIODCNT;
	while ((ch = getopt(argc, argv, "n:")) != EOF)
		switch (ch) {
		case 'n':
			num_servers = atoi(optarg);
			if (num_servers < 1 || num_servers > MAXNFSIODCNT) {
				warnx("nfsiod count %d; reset to %d",
				    num_servers, DEFNFSIODCNT);
				num_servers = DEFNFSIODCNT;
			}
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/*
	 * XXX
	 * Backward compatibility, trailing number is the count of daemons.
	 */
	if (argc > 1)
		usage();
	if (argc == 1) {
		num_servers = atoi(argv[0]);
		if (num_servers < 1 || num_servers > MAXNFSIODCNT) {
			warnx("nfsiod count %d; reset to %d",
				num_servers, DEFNFSIODCNT);
			num_servers = DEFNFSIODCNT;
		}
	}

	thread_status = malloc(sizeof(int) * num_servers);
	if (thread_status == NULL)
		errx(1, "unable to allocate memory");
	rv = pthread_cond_init(&cond, NULL);
	if (rv)
		errc(1, rv, "condition variable init failed");
	rv = pthread_mutex_init(&mutex, NULL);
	if (rv)
		errc(1, rv, "mutex init failed");

	if (debug == 0) {
		daemon(0, 0);
		(void)signal(SIGHUP, SIG_IGN);
		(void)signal(SIGINT, SIG_IGN);
		(void)signal(SIGQUIT, SIG_IGN);
		(void)signal(SIGSYS, nonfs);
	}

	openlog("nfsiod:", LOG_PID, LOG_DAEMON);

	threadcnt = 0;
	for (i=0; i < num_servers; i++) {
		pthread_t thd;
		thread_status[i] = 1;
		rv = pthread_create(&thd, NULL, nfsiod_thread, (void*)i);
		if (rv) {
			syslog(LOG_ERR, "thread_create: %s", strerror(rv));
			thread_status[i] = 0;
			continue;
		}
		threadcnt++;
	}
	/* if no threads started exit */
	if (!threadcnt)
		errx(1, "unable to start any threads");
	if (threadcnt != num_servers)
		syslog(LOG_ERR, "only able to create %d of %d threads",
			threadcnt, num_servers);

	/* wait for threads to complete */
	rv = pthread_mutex_lock(&mutex);
	if (rv)
		errc(1, rv, "mutex lock failed");
	while (threadcnt > 0) {
		rv = pthread_cond_wait(&cond, &mutex);
		if (rv)
			errc(1, rv, "nfsiod: cond wait failed");
		for (i=0; i < num_servers; i++) {
			if (!thread_status[i])
				continue;
			if (thread_status[i] == 1)
				continue;
			threadcnt--;
			thread_status[i] = 0;
			syslog(LOG_ERR, "lost nfsiod thread %d - "
				"%d of %d threads remain",
				i, threadcnt, num_servers);
		}
		rv = pthread_mutex_lock(&mutex);
		if (rv)
			errc(1, rv, "mutex lock failed");
	}

	exit (0);
}

void *
nfsiod_thread(void *arg)
{
	int rv, thread = (int)arg;
	if ((rv = nfssvc(NFSSVC_BIOD, NULL)) < 0) {
		thread_status[thread] = rv;
		syslog(LOG_ERR, "nfssvc: %s", strerror(rv));
		pthread_cond_signal(&cond);
		return NULL;
	}
	thread_status[thread] = 0;
	pthread_cond_signal(&cond);
	return NULL;
}

void
nonfs(signo)
	int signo;
{
	syslog(LOG_ERR, "missing system call: NFS not available.");
}

void
usage()
{
	(void)fprintf(stderr, "usage: nfsiod [-n num_servers]\n");
	exit(1);
}
