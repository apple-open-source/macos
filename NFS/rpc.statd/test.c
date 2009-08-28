/*
 * Copyright (c) 2002-2008 Apple Inc.  All rights reserved.
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

#ifndef lint
static const char rcsid[] =
"$FreeBSD$";
#endif				/* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <rpc/rpc.h>
#include <rpcsvc/sm_inter.h>

char *progname;

void
usage(void)
{
	fprintf(stderr, "usage: %s <cmd> <cmdarg> [statdhost]\n", progname);
	fprintf(stderr, "%s stat host [statdhost]\n", progname);
	fprintf(stderr, "%s mon host [statdhost]\n", progname);
	fprintf(stderr, "%s unmon host [statdhost]\n", progname);
	fprintf(stderr, "%s unmonall [statdhost]\n", progname);
	fprintf(stderr, "%s crash [statdhost]\n", progname);
	fprintf(stderr, "%s notify host state [statdhost]\n", progname);
	fprintf(stderr, "(talks to statd at localhost if statdhost omitted)\n");
	exit(1);
}


/* Default timeout can be changed using clnt_control() */
static struct timeval TIMEOUT = {25, 0};

struct sm_stat_res *
sm_stat_1(struct sm_name *argp, CLIENT *clnt)
{
	static struct sm_stat_res res;
	char proc[] = "SM_STAT";

	bzero((char *) &res, sizeof(res));
	if (clnt_call(clnt, SM_STAT, (xdrproc_t)xdr_sm_name, argp, (xdrproc_t)xdr_sm_stat_res, &res, TIMEOUT) != RPC_SUCCESS) {
		clnt_perror(clnt, proc);
		return (NULL);
	}
	return (&res);
}


struct sm_stat_res *
sm_mon_1(struct mon *argp, CLIENT *clnt)
{
	static struct sm_stat_res res;
	char proc[] = "SM_MON";

	bzero((char *) &res, sizeof(res));
	if (clnt_call(clnt, SM_MON, (xdrproc_t)xdr_mon, argp, (xdrproc_t)xdr_sm_stat_res, &res, TIMEOUT) != RPC_SUCCESS) {
		clnt_perror(clnt, proc);
		return (NULL);
	}
	return (&res);
}


struct sm_stat *
sm_unmon_1(struct mon_id *argp, CLIENT *clnt)
{
	static struct sm_stat res;
	char proc[] = "SM_UNMON";

	bzero((char *) &res, sizeof(res));
	if (clnt_call(clnt, SM_UNMON, (xdrproc_t)xdr_mon_id, argp, (xdrproc_t)xdr_sm_stat, &res, TIMEOUT) != RPC_SUCCESS) {
		clnt_perror(clnt, proc);
		return (NULL);
	}
	return (&res);
}


struct sm_stat *
sm_unmon_all_1(struct my_id *argp, CLIENT *clnt)
{
	static struct sm_stat res;
	char proc[] = "SM_UNMON_ALL";

	bzero((char *) &res, sizeof(res));
	if (clnt_call(clnt, SM_UNMON_ALL, (xdrproc_t)xdr_my_id, argp, (xdrproc_t)xdr_sm_stat, &res, TIMEOUT) != RPC_SUCCESS) {
		clnt_perror(clnt, proc);
		return (NULL);
	}
	return (&res);
}


void *
sm_simu_crash_1(void *argp, CLIENT *clnt)
{
	static char res;
	char proc[] = "SM_SIMU_CRASH";

	bzero((char *) &res, sizeof(res));
	if (clnt_call(clnt, SM_SIMU_CRASH, (xdrproc_t)xdr_void, argp, (xdrproc_t)xdr_void, &res, TIMEOUT) != RPC_SUCCESS) {
		clnt_perror(clnt, proc);
		return (NULL);
	}
	return ((void *) &res);
}

void *
sm_notify_1(struct stat_chge *argp, CLIENT *clnt)
{
	static char res;
	char proc[] = "SM_NOTIFY";

	bzero((char *) &res, sizeof(res));
	if (clnt_call(clnt, SM_NOTIFY, (xdrproc_t)xdr_stat_chge, argp, (xdrproc_t)xdr_void, &res, TIMEOUT) != RPC_SUCCESS) {
		clnt_perror(clnt, proc);
		return (NULL);
	}
	return ((void *) &res);
}


int 
main(int argc, char **argv)
{
	CLIENT *cli;
	char dummy, *cmd, *arg;
	char *statdhost;
	char localhost[] = "localhost";
	char transp[] = "udp";
	int state = 0;

	cmd = arg = statdhost = NULL;

	progname = argv[0];
	argv++;
	argc--;

	if (argc < 1)
		usage();
	cmd = argv[0];
	argv++;
	argc--;

	if (!strcmp(cmd, "stat") || !strcmp(cmd, "mon") || !strcmp(cmd, "unmon") || !strcmp(cmd, "notify")) {
		if (argc < 1)
			usage();
		arg = argv[0];
		argv++;
		argc--;
	}
	if (!strcmp(cmd, "notify")) {
		if (argc < 1)
			usage();
		state = atoi(argv[0]);
		argv++;
		argc--;
	}

	if (argc >= 1) {
		statdhost = argv[0];
		argv++;
		argc--;
	}

	if (!statdhost)
		statdhost = localhost;

	printf("Creating client for %s\n", statdhost);
	cli = clnt_create(statdhost, SM_PROG, SM_VERS, transp);
	if (!cli) {
		printf("Failed to create client\n");
		clnt_pcreateerror(statdhost);
		exit(1);
	}

	if (!strcmp(cmd, "stat")) {
		struct sm_name smn;
		struct sm_stat_res *res;
		smn.mon_name = arg;
		if ((res = sm_stat_1(&smn, cli))) {
			printf("sm_stat result: status %d state %d\n", res->res_stat, res->state);
		} else {
			printf("sm_stat call failed!\n");
		}
	} else if (!strcmp(cmd, "mon")) {
		struct mon mon;
		struct sm_stat_res *res;
		mon.mon_id.mon_name = arg;
		mon.mon_id.my_id.my_name = localhost;
		mon.mon_id.my_id.my_prog = SM_PROG;
		mon.mon_id.my_id.my_vers = SM_VERS;
		mon.mon_id.my_id.my_proc = SM_STAT;
		bzero(mon.priv, sizeof(mon.priv));
		if ((res = sm_mon_1(&mon, cli))) {
			printf("sm_mon result: status %d state %d\n", res->res_stat, res->state);
		} else {
			printf("sm_mon call failed!\n");
		}
	} else if (!strcmp(cmd, "unmon")) {
		struct mon_id mon_id;
		struct sm_stat *res;
		mon_id.mon_name = arg;
		mon_id.my_id.my_name = localhost;
		mon_id.my_id.my_prog = SM_PROG;
		mon_id.my_id.my_vers = SM_VERS;
		mon_id.my_id.my_proc = SM_STAT;
		if ((res = sm_unmon_1(&mon_id, cli))) {
			printf("sm_unmon result: state %d\n", res->state);
		} else {
			printf("sm_unmon call failed!\n");
		}
	} else if (!strcmp(cmd, "unmonall")) {
		struct my_id myid;
		struct sm_stat *res;
		myid.my_name = localhost;
		myid.my_prog = SM_PROG;
		myid.my_vers = SM_VERS;
		myid.my_proc = SM_STAT;
		if ((res = sm_unmon_all_1(&myid, cli))) {
			printf("sm_unmon_all result: state %d\n", res->state);
		} else {
			printf("sm_unmon_all call failed!\n");
		}
	} else if (!strcmp(cmd, "crash")) {
		if (sm_simu_crash_1(&dummy, cli)) {
			printf("sm_simu_crash called\n");
		} else {
			printf("sm_simu_crash call failed!\n");
		}
	} else if (!strcmp(cmd, "notify")) {
		struct stat_chge schg;
		schg.mon_name = arg;
		schg.state = state;
		if (sm_notify_1(&schg, cli)) {
			printf("sm_notify called\n");
		} else {
			printf("sm_notify call failed!\n");
		}
	} else {
		usage();
	}

	return 0;
}

