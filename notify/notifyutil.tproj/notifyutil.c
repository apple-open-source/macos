/*
 * Copyright (c) 2006-2010 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <mach/mach.h>
#include <notify.h>
#include <notify_private.h>

#define forever for(;;)
#define IndexNull ((uint32_t)-1)

#define PRINT_QUIET 0x00000000
#define PRINT_KEY   0x00000001
#define PRINT_STATE 0x00000002
#define PRINT_TIME  0x00000004
#define PRINT_UTIME 0x00000008

#ifndef USEC_PER_SEC
#define USEC_PER_SEC 1000000
#endif

extern uint32_t notify_register_plain(const char *name, int *out_token);

typedef struct
{
	uint32_t token;
	uint32_t count;
	char *name;
} tnmap_entry_t;

static tnmap_entry_t *tnmap;
static uint32_t tnmap_count = 0;

void
usage(const char *name)
{
	fprintf(stderr, "usage: %s [-q] [-m] [-f] [-v] [-t] [-T] [action]...\n", name);
	fprintf(stderr, "    -q             quiet mode\n");
	fprintf(stderr, "    -m             use mach ports for notifications [default]\n");
	fprintf(stderr, "    -f             use file descriptors for notifications\n");
	fprintf(stderr, "    -v             verbose mode - prints key and state value for notifications\n");
	fprintf(stderr, "    -t             print timestamp when reporting notifications\n");
	fprintf(stderr, "    -T             print timestamp with microseconds when sending and receiving notifications\n");
	fprintf(stderr, "actions:\n");
	fprintf(stderr, "    -p key         post a notifcation for key\n");
	fprintf(stderr, "    -w key         register for key and report notifications\n");
	fprintf(stderr, "    -# key         (# is an integer value, eg \"-1\") register for key and report # notifications\n");
	fprintf(stderr, "    -g key         get state value for key\n");
	fprintf(stderr, "    -s key val     set state value for key\n");
}

const char *
notify_status_strerror(int status)
{
	switch (status)
	{
		case NOTIFY_STATUS_OK: return("OK");
		case NOTIFY_STATUS_INVALID_NAME: return "Invalid Name";
		case NOTIFY_STATUS_INVALID_TOKEN: return "Invalid Token";
		case NOTIFY_STATUS_INVALID_PORT: return "Invalid Port";
		case NOTIFY_STATUS_INVALID_FILE: return "Invalid File";
		case NOTIFY_STATUS_INVALID_SIGNAL: return "Invalid Signal";
		case NOTIFY_STATUS_INVALID_REQUEST: return "Invalid Request";
		case NOTIFY_STATUS_NOT_AUTHORIZED: return "Not Authorized";
		default: return "Failed";
	}
}

void
tnmap_add(uint32_t t, uint32_t count, const char *n)
{
	if (n == NULL) return;

	if (tnmap_count == 0) tnmap = (tnmap_entry_t *)malloc(sizeof(tnmap_entry_t));
	else tnmap = (tnmap_entry_t *)reallocf(tnmap, (tnmap_count + 1) * sizeof(tnmap_entry_t));

	if (tnmap == NULL)
	{
		fprintf(stderr, "Can't allocate memory!\n");
		tnmap_count = 0;
		return;
	}

	tnmap[tnmap_count].token = t;
	tnmap[tnmap_count].count = count;
	tnmap[tnmap_count].name = strdup(n);
	if (tnmap[tnmap_count].name == NULL)
	{
		fprintf(stderr, "Can't allocate memory!\n");
		tnmap = NULL;
		tnmap_count = 0;
		return;
	}

	tnmap_count++;
}

void
tnmap_delete(uint32_t index)
{
	uint32_t i;

	if (index == IndexNull) return;
	if (index >= tnmap_count) return;

	free(tnmap[index].name);

	for (i = index + 1; i < tnmap_count; i++) tnmap[i - 1] = tnmap[i];
	tnmap_count--;

	if (tnmap_count == 0)
	{
		free(tnmap);
		tnmap = NULL;
	}
	else
	{
		tnmap = (tnmap_entry_t *)reallocf(tnmap, tnmap_count * sizeof(tnmap_entry_t));
		if (tnmap == NULL)
		{
			fprintf(stderr, "Can't allocate memory!\n");
			tnmap_count = 0;
		}
	}
}

uint32_t
tnmap_find_name(const char *n)
{
	uint32_t i;

	for (i = 0; i < tnmap_count; i++) if (!strcmp(tnmap[i].name, n)) return i;
	return IndexNull;
}

uint32_t
tnmap_find_token(uint32_t t)
{
	uint32_t i;

	for (i = 0; i < tnmap_count; i++) if (t == tnmap[i].token) return i;
	return IndexNull;
}

void 
file_watcher(int fd, int printopt)
{
	struct timeval now;
	char tstr[32];
	int status, i, tid, index;
	uint64_t state;

	forever
	{
		memset(&now, 0, sizeof(now));
		i = read(fd, &tid, 4);
		if (i < 0) return;
		gettimeofday(&now, NULL);

		tid = ntohl(tid);
		index = tnmap_find_token(tid);
		if (index == IndexNull) continue;

		if (printopt & PRINT_TIME)
		{
			memcpy(tstr, ctime(&(now.tv_sec)), 24);
			tstr[24] = '\0';
			printf("%s", tstr);
			if (printopt & (PRINT_KEY | PRINT_STATE)) printf(" ");
		}

		if (printopt & PRINT_UTIME)
		{
			snprintf(tstr, sizeof(tstr), "%llu", now.tv_usec + USEC_PER_SEC);
			printf("%d.%s", (int)now.tv_sec, tstr+1);
			if (printopt & (PRINT_KEY | PRINT_STATE)) printf(" ");
		}

		if (printopt & PRINT_KEY)
		{
			printf("%s", tnmap[index].name);
			if (printopt & PRINT_STATE) printf(" ");
		}

		if (printopt & PRINT_STATE)
		{
			state = 0;
			status = notify_get_state(tid, &state);
			if (status == NOTIFY_STATUS_OK) printf("%llu",(unsigned long long)state);
			else printf(": %s", notify_status_strerror(status));
		}

		if (printopt != PRINT_QUIET) printf("\n");

		if ((tnmap[index].count != IndexNull) && (tnmap[index].count != 0)) tnmap[index].count--;
		if (tnmap[index].count == 0)
		{
			status = notify_cancel(tid);
			tnmap_delete(index);
		}

		if (tnmap_count == 0) return;
	}
}

void 
port_watcher(mach_port_t port, int printopt)
{
	struct timeval now;
	char tstr[32];
	int tid, index;
	uint64_t state;
	mach_msg_empty_rcv_t msg;
	kern_return_t status;

	if (port == MACH_PORT_NULL) return;

	forever
	{
		memset(&now, 0, sizeof(now));
		memset(&msg, 0, sizeof(msg));
		status = mach_msg(&msg.header, MACH_RCV_MSG, 0, sizeof(msg), port, 0, MACH_PORT_NULL);
		if (status != KERN_SUCCESS) return;
		gettimeofday(&now, NULL);

		tid = msg.header.msgh_id;
		index = tnmap_find_token(tid);
		if (index == IndexNull) continue;

		if (printopt & PRINT_TIME)
		{
			memcpy(tstr, ctime(&(now.tv_sec)), 24);
			tstr[24] = '\0';
			printf("%s", tstr);
			if (printopt & (PRINT_KEY | PRINT_STATE)) printf(" ");
		}

		if (printopt & PRINT_UTIME)
		{
			snprintf(tstr, sizeof(tstr), "%llu", now.tv_usec + USEC_PER_SEC);
			printf("%d.%s", (int)now.tv_sec, tstr+1);
			if (printopt & (PRINT_KEY | PRINT_STATE)) printf(" ");
		}

		if (printopt & PRINT_KEY)
		{
			printf("%s", tnmap[index].name);
			if (printopt & PRINT_STATE) printf(" ");
		}

		if (printopt & PRINT_STATE)
		{
			state = 0;
			status = notify_get_state(tid, &state);
			if (status == NOTIFY_STATUS_OK) printf("%llu",(unsigned long long)state);
			else printf(": %s", notify_status_strerror(status));
		}

		if (printopt != PRINT_QUIET) printf("\n");

		if ((tnmap[index].count != IndexNull) && (tnmap[index].count != 0)) tnmap[index].count--;
		if (tnmap[index].count == 0)
		{
			status = notify_cancel(tid);
			tnmap_delete(index);
		}

		if (tnmap_count == 0) return;
	}
}

int
main(int argc, const char *argv[])
{
	const char *name;
	char tstr[32];
	uint32_t i, n, index, flag, use_mach, status, simple;
	int watch_fd, printopt, tid;
	mach_port_t watch_port;
	struct timeval now, then;
	uint64_t state;

	use_mach = 1;
	watch_fd = -1;
	watch_port = MACH_PORT_NULL;
	printopt = PRINT_KEY;
	flag = 0;

	name = strrchr(argv[0], '/');
	if (name == NULL) name = argv[0];
	else name++;

	for (i = 1; i < argc; i++)
	{
		if ((!strcmp(argv[i], "-help")) || (!strcmp(argv[i], "-h")))
		{
			usage(name);
			exit(0);
		}
		else if (!strcmp(argv[i], "-m"))
		{
			use_mach = 1;
		}
		else if (!strcmp(argv[i], "-f"))
		{
			use_mach = 0;
		}
		else if (!strcmp(argv[i], "-q"))
		{
			printopt = PRINT_QUIET;
		}
		else if (!strcmp(argv[i], "-v"))
		{
			printopt |= PRINT_STATE;
		}
		else if (!strcmp(argv[i], "-t"))
		{
			printopt |= PRINT_TIME;
		}
		else if (!strcmp(argv[i], "-T"))
		{
			printopt |= PRINT_UTIME;
		}
	}

	if (printopt & PRINT_UTIME)
	{
		if (printopt & PRINT_TIME)
		{
			printf("Warning: \"-T\" overrides \"-t\" option\n");
			printopt &= ~PRINT_TIME;
		}
	}

	for (i = 1; i < argc; i++)
	{
		if ((!strcmp(argv[i], "-p")) || (!strcmp(argv[i], "-P")))
		{
			simple = 0;
			if (argv[i][1] == 'P') simple = 1;

			if ((i + 1) >= argc)
			{
				usage(name);
				exit(1);
			}

			i++;
			memset(&now, 0, sizeof(now));
			memset(&then, 0, sizeof(then));
			gettimeofday(&now, NULL);

			status = NOTIFY_STATUS_OK;
			if (simple == 0) status = notify_post(argv[i]);
			else status = notify_simple_post(argv[i]);

			gettimeofday(&then, NULL);
			if (status != NOTIFY_STATUS_OK) printf("%s: %s\n", argv[i], notify_status_strerror(status));
			if (printopt & PRINT_UTIME)
			{
				snprintf(tstr, sizeof(tstr), "%llu", now.tv_usec + USEC_PER_SEC);
				printf("notify_post start %d.%s", (int)now.tv_sec, tstr+1);
				snprintf(tstr, sizeof(tstr), "%llu", then.tv_usec + USEC_PER_SEC);
				printf(" end %d.%s\n", (int)then.tv_sec, tstr+1);
			}
		}
		else if ((argv[i][0] == '-') && ((argv[i][1] == 'w') || ((argv[i][1] >= '0') && (argv[i][1] <= '9'))))
		{
			if ((i + 1) >= argc)
			{
				usage(name);
				exit(1);
			}

			n = IndexNull;
			if (argv[i][1] != 'w') n = atoi(argv[i] + 1);

			i++;
			tid = IndexNull;

			index = tnmap_find_name(argv[i]);
			if (index != IndexNull)
			{
				fprintf(stderr, "Already watching for %s\n", argv[i]);
				continue;
			}

			if (use_mach == 1) status = notify_register_mach_port(argv[i], &watch_port, flag, &tid);
			else status = notify_register_file_descriptor(argv[i], &watch_fd, flag, &tid);
			flag = NOTIFY_REUSE;
			if (status == NOTIFY_STATUS_OK) tnmap_add(tid, n, argv[i]);
			else printf("%s: %s\n", argv[i], notify_status_strerror(status));
		}
		else if (!strcmp(argv[i], "-g"))
		{
			if ((i + 1) >= argc)
			{
				usage(name);
				exit(1);
			}

			i++;
			state = 0;
			tid = IndexNull;

			status = notify_register_plain(argv[i], &tid);
			if (status == NOTIFY_STATUS_OK)
			{
				status = notify_get_state(tid, &state);
				notify_cancel(tid);
			}

			if (status == NOTIFY_STATUS_OK) printf("%s %llu\n", argv[i], (unsigned long long)state);
			else printf("%s: %s\n", argv[i], notify_status_strerror(status));
		}
		else if (!strcmp(argv[i], "-s"))
		{
			if ((i + 2) >= argc)
			{
				usage(name);
				exit(1);
			}

			i++;
			tid = IndexNull;
			status = notify_register_plain(argv[i], &tid);
			if (status == NOTIFY_STATUS_OK)
			{
				state = atoll(argv[i + 1]);
				status = notify_set_state(tid, state);
				notify_cancel(tid);
			}

			if (status != NOTIFY_STATUS_OK)  printf("%s: %s\n", argv[i], notify_status_strerror(status));
			i++;
		}
	}

	if (tnmap_count > 0) 
	{
		if (use_mach == 1) port_watcher(watch_port, printopt);
		else file_watcher(watch_fd, printopt);
	}

	exit(0);
}
