/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/signal.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <servers/bootstrap.h>
#include <asl.h>
#include <asl_private.h>
#include "daemon.h"
#include "file_watcher.h"
#include "service.h"
#include "notify_ipc.h"

#define CONFIG_FILE_PATH "/etc/notify.conf"

#define SERVER_STATUS_ERROR -1
#define SERVER_STATUS_INACTIVE 0
#define SERVER_STATUS_ACTIVE 1
#define SERVER_STATUS_ON_DEMAND 2

#define IndexNull -1

/* Compile flags */
#define RUN_TIME_CHECKS

#ifdef DEBUG
#define V_DEBUG 0
#define V_RESTART 1
#else
#define V_DEBUG 0
#define V_RESTART 1
#endif

#define STATUS_REQUEST_SHORT 1
#define STATUS_REQUEST_LONG 2
#define PRINT_STATUS_MSG_ID 0xfadefade

#define NOTIFYD_PROCESS_FLAG 0x00000001

mach_port_t server_port = MACH_PORT_NULL;
mach_port_t dead_session_port = MACH_PORT_NULL;
mach_port_t listen_set = MACH_PORT_NULL;
int kq = -1;
uint32_t debug = V_DEBUG;
uint32_t restart = V_RESTART;
FILE *debug_log_file = NULL;
int log_cutoff = ASL_LEVEL_NOTICE;
uint32_t shm_enabled = 1;
uint32_t nslots = 1024;
int32_t shmfd = -1;
uint32_t *shm_base = NULL;
uint32_t *shm_refcount = NULL;
uint32_t slot_id = (uint32_t)-1;
notify_state_t *ns = NULL;
uint32_t status_request = 0;
char *status_file = NULL;

static pthread_mutex_t daemon_lock = PTHREAD_MUTEX_INITIALIZER;

extern void cancel_session(task_t task);
extern int __notify_78945668_info__;

#define forever for(;;)

static const char *notify_type_name[] =
{
	"none", "plain", "memory", "port", "file", "signal"
};

extern boolean_t notify_ipc_server
(
	mach_msg_header_t *InHeadP,
	mach_msg_header_t *OutHeadP
);

typedef union
{
	mach_msg_header_t head;
	union __RequestUnion__notify_ipc_subsystem request;
} notify_request_msg;

typedef union
{
	mach_msg_header_t head;
	union __ReplyUnion__notify_ipc_subsystem reply;
} notify_reply_msg;

static int32_t
open_shared_memory()
{
	int32_t shmfd;
	uint32_t size;

	size = nslots * sizeof(uint32_t);

	shmfd = shm_open(SHM_ID, O_RDWR | O_CREAT, 0644);
	if (shmfd == -1) return -1;

	ftruncate(shmfd, size);
	shm_base = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
	close(shmfd);

	memset(shm_base, 0, size);
	shm_refcount = (uint32_t *)malloc(size);
	if (shm_refcount != NULL) memset(shm_refcount, 0, size);

	return 0;
}

static void
close_shared_memory(void)
{
	shm_unlink(SHM_ID);
}

static void
fprint_client(FILE *f, client_t *c)
{
	w_event_t *e;
	svc_info_t *s;

	if (c == NULL) 
	{
		fprintf(f, "NULL client\n");
		return;
	}

	fprintf(f, "client_id: %u\n", c->client_id);
	fprintf(f, "session: 0x%08x\n", c->info->session);
	fprintf(f, "pid: %d\n", c->info->pid);
	fprintf(f, "lastval: %u\n", c->info->lastval);
	fprintf(f, "notify: %s\n", notify_type_name[c->info->notify_type]);
	switch(c->info->notify_type)
	{
		case NOTIFY_TYPE_NONE:
			break;

		case NOTIFY_TYPE_PLAIN:
			break;

		case NOTIFY_TYPE_MEMORY:
			break;

		case NOTIFY_TYPE_PORT:
			fprintf(f, "mach port: 0x%08x\n", c->info->msg->header.msgh_remote_port);
			fprintf(f, "token: %u\n", c->info->token);
			break;

		case NOTIFY_TYPE_FD:
			fprintf(f, "fd: %d\n", c->info->fd);
			fprintf(f, "token: %u\n", c->info->token);
			break;

		case NOTIFY_TYPE_SIGNAL:
			fprintf(f, "signal: %d\n", c->info->sig);
			break;

		default: break;
	}

	if (c->info->private != NULL)
	{
		s = (svc_info_t *)c->info->private;
		for (e = s->private; e != NULL; e = e->next)
		{
			fprintf(f, "0x%lx %s %u %u (%u)\n", (unsigned long)e, e->name, e->type, e->flags, e->refcount);
		}
	}
}
	
static void
fprint_quick_client(FILE *f, client_t *c)
{
	int pid = 0;

	if (c == NULL) return;

	pid_for_task(c->info->session, &pid);
	fprintf(f, "  %u %s", pid, notify_type_name[c->info->notify_type]);
	if (c->info->notify_type == NOTIFY_TYPE_SIGNAL) fprintf(f, " %d", c->info->sig);
	fprintf(f, "\n");
}
	
static void
fprint_quick_name_info(FILE *f, name_info_t *n)
{
	list_t *c;

	if (n == NULL) return;

	fprintf(f, "\"%s\" uid=%u gid=%u %03x", n->name, n->uid, n->gid, n->access);
	if (n->slot != -1) 
	{
		fprintf(f, " slot %u", n->slot);
		if (shm_refcount[n->slot] != -1) fprintf(f, " = %u", shm_base[n->slot]);
	}
	fprintf(f, "\n");

	for (c = n->client_list; c != NULL; c = _nc_list_next(c))
	{
		fprint_quick_client(f, _nc_list_data(c));
	}
	fprintf(f, "\n");
}

static void
fprint_name_info(FILE *f, const char *name, name_info_t *n)
{
	list_t *c;

	if (n == NULL)
	{
		fprintf(f, "%s unknown\n", name);
		return;
	}

	fprintf(f, "name: %s\n", n->name);
	fprintf(f, "uid: %u\n", n->uid);
	fprintf(f, "gid: %u\n", n->gid);
	fprintf(f, "access: %03x\n", n->access);
	fprintf(f, "refcount: %u\n", n->refcount);
	if (n->slot == -1) fprintf(f, "slot: -unassigned-");
	else
	{
		fprintf(f, "slot: %u", n->slot);
		if (shm_refcount[n->slot] != -1)
			fprintf(f, " = %u (%u)", shm_base[n->slot], shm_refcount[n->slot]);
	}
	fprintf(f, "\n");
	fprintf(f, "val: %u\n", n->val);
	fprintf(f, "state: %llu\n", n->state);

	for (c = n->client_list; c != NULL; c = _nc_list_next(c))
	{
		fprintf(f, "\n");
		fprint_client(f, _nc_list_data(c));
	}
}

void
log_message(int priority, char *str, ...)
{
	va_list ap;
	char now[32];
	time_t t;

	if (priority > log_cutoff) return;
	
	va_start(ap, str);

	if (debug_log_file != NULL)
	{
		memset(now, 0, 32);
		time(&t);
		strftime(now, 32, "%b %e %T", localtime(&t));
		fprintf(debug_log_file, "notifyd %s: ", now);
		vfprintf(debug_log_file, str, ap);
		fflush(debug_log_file);
	}
	else
	{
		vfprintf(stderr, str, ap);
	}

	va_end(ap);
}

uint32_t
daemon_post(const char *name, uint32_t u, uint32_t g)
{
	name_info_t *n;
	
	if (name == NULL) return 0;

	n = (name_info_t *)_nc_table_find(ns->name_table, name);
	if (n == NULL) return 0;

	if (n->slot != (uint32_t)-1) shm_base[n->slot]++;

	return _notify_lib_post(ns, name, u, g);
}

void
daemon_set_state(const char *name, uint64_t val)
{
	name_info_t *n;

	if (name == NULL) return;
	
	n = (name_info_t *)_nc_table_find(ns->name_table, name);
	if (n == NULL) return;

	n->state = val;
}

void
fprint_quick_status(FILE *f)
{
	void *tt;
	name_info_t *n;
	
	tt = _nc_table_traverse_start(ns->name_table);
	
	while (tt != NULL)
	{
		n = _nc_table_traverse(ns->name_table, tt);
		if (n == NULL) break;
		fprint_quick_name_info(f, n);
	}
	
	_nc_table_traverse_end(ns->name_table, tt);
	fprintf(f, "\n");
}

void
fprint_status(FILE *f)
{
	void *tt;
	name_info_t *n;
	int32_t i;
	list_t *l;
	client_t *c;
	watcher_t *w;
	
	tt = _nc_table_traverse_start(ns->name_table);
	
	while (tt != NULL)
	{
		n = _nc_table_traverse(ns->name_table, tt);
		if (n == NULL) break;
		fprintf(f, "--- NAME %s ---\n", n->name);
		fprint_name_info(f, n->name, n);
		fprintf(f, "\n");
	}
	
	_nc_table_traverse_end(ns->name_table, tt);
	fprintf(f, "\n");
	
	fprintf(f, "--- CONTROLLED NAMES ---\n");
	for (i = 0; i < ns->controlled_name_count; i++)
	{
		fprintf(f, "%s %u %u 0x%3x\n", ns->controlled_name[i]->name, ns->controlled_name[i]->uid, ns->controlled_name[i]->gid, ns->controlled_name[i]->access);
	}
	fprintf(f, "\n");
	
	fprintf(f, "--- CLIENT FREELIST ---\n");
	l = ns->free_client_list;
	if (l == NULL) fprintf(f, "NULL\n");
	while (l != NULL)
	{
		c = _nc_list_data(l);
		fprintf(f, "%u", c->client_id);
		l = _nc_list_next(l);
		if (l != NULL) fprintf(f, " ");
	}
	fprintf(f, "\n");
	
	fprintf(f, "--- WATCH LIST ---\n");
	l = _nc_list_tail(watch_list);
	if (l == NULL) fprintf(f, "NULL\n");
	while (l != NULL)
	{
		w = _nc_list_data(l);
		watcher_printf(w, f);
		fprintf(f, "\n");
		l = _nc_list_prev(l);
	}
	fprintf(f, "\n");
}

void
print_status()
{
	FILE *f;
	uint32_t req;
	
	if (status_request == 0) return;
	
	req = status_request;
	status_request = 0;
	
	if (status_file == NULL)
	{
		asprintf(&status_file, "/var/run/notifyd_%u.status", getpid());
		if (status_file == NULL) return;
	}
	
	unlink(status_file);
	f = fopen(status_file, "w");
	if (f == NULL) return;
	
	if (req == STATUS_REQUEST_SHORT) fprint_quick_status(f);
	else if (req == STATUS_REQUEST_LONG) fprint_status(f);
	
	fclose(f);
}

static void
server_run_loop()
{
	int len;
	struct kevent event;
	unsigned long x;
	uint32_t kid;

	forever
	{
		/*
		 * Get events.
		 */
		memset(&event, 0, sizeof(struct kevent));
		len = kevent(kq, NULL, 0, &event, 1, NULL);
		if (len == 0) continue;

		x = (unsigned long)event.udata;
		kid = x;

#ifdef DEBUG
		log_message(ASL_LEVEL_ERR, "kevent %u fflags 0x%08x\n", kid, event.fflags);
#endif
		pthread_mutex_lock(&daemon_lock);
		watcher_trigger(kid, event.fflags, 0);
		if (status_request != 0) print_status();
		pthread_mutex_unlock(&daemon_lock);
	}
}

static void
server_side_loop()
{
	kern_return_t status;
	notify_request_msg *request;
	notify_reply_msg *reply;
	uint32_t rqs, rps;
	uint32_t rbits, sbits;
	mach_dead_name_notification_t *deadname;
	
	rqs = sizeof(notify_request_msg) + MAX_TRAILER_SIZE;
	rps = sizeof(notify_reply_msg) + MAX_TRAILER_SIZE;
	reply = (notify_reply_msg *)calloc(1, rps);

	rbits = MACH_RCV_MSG | MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) | MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0);
	sbits = MACH_SEND_MSG | MACH_SEND_TIMEOUT;

	forever
	{
		request = (notify_request_msg *)calloc(1, rqs);
		request->head.msgh_local_port = listen_set;
		request->head.msgh_size = rqs;
		
		memset(reply, 0, rps);
		status = mach_msg(&(request->head), rbits, 0, rqs, listen_set, 0, MACH_PORT_NULL);
		pthread_mutex_lock(&daemon_lock);
		if (request->head.msgh_id == PRINT_STATUS_MSG_ID)
		{
			if (status_request != 0) print_status();
			free(request);
			pthread_mutex_unlock(&daemon_lock);
			continue;
		}

		if (request->head.msgh_id == MACH_NOTIFY_DEAD_NAME)
		{
			deadname = (mach_dead_name_notification_t *)request;
			cancel_session(deadname->not_port);
			free(request);
			pthread_mutex_unlock(&daemon_lock);
			continue;
		}

		status = notify_ipc_server(&(request->head), &(reply->head));
		status = mach_msg(&(reply->head), sbits, reply->head.msgh_size, 0, MACH_PORT_NULL, 10, MACH_PORT_NULL);
		if (status == MACH_SEND_INVALID_DEST)
		{
			mach_port_destroy(mach_task_self(), request->head.msgh_remote_port);
		}

		free(request);

		pthread_mutex_unlock(&daemon_lock);
	}
}

static kern_return_t
create_service(char *name, mach_port_t *p)
{
	kern_return_t status;
	mach_port_t priv_boot_port, send_port;

	status = bootstrap_check_in(bootstrap_port, name, p);
	if (status == KERN_SUCCESS)
	{
		return status;
	}
	
	if (status == BOOTSTRAP_UNKNOWN_SERVICE)
	{
		if (restart != 0)
		{
			status = bootstrap_create_server(bootstrap_port, "/usr/sbin/notifyd", 0, TRUE, &priv_boot_port);
			if (status != KERN_SUCCESS) return status;
		}
		else
		{
			priv_boot_port = bootstrap_port;
		}

		status = bootstrap_create_service(priv_boot_port, name, &send_port);
		if (status != KERN_SUCCESS) return status;

		status = bootstrap_check_in(priv_boot_port, name, p);
	}

	return status;
}

static void
string_list_free(char **l)
{
	int i;

	if (l == NULL) return;
	for (i = 0; l[i] != NULL; i++)
	{
		if (l[i] != NULL) free(l[i]);
		l[i] = NULL;
	}
	free(l);
}

static char **
string_insert(char *s, char **l, unsigned int x)
{
	int i, len;

	if (s == NULL) return l;
	if (l == NULL) 
	{
		l = (char **)malloc(2 * sizeof(char *));
		l[0] = strdup(s);
		l[1] = NULL;
		return l;
	}

	for (i = 0; l[i] != NULL; i++);
	len = i + 1; /* count the NULL on the end of the list too! */

	l = (char **)realloc(l, (len + 1) * sizeof(char *));

	if ((x >= (len - 1)) || (x == IndexNull))
	{
		l[len - 1] = strdup(s);
		l[len] = NULL;
		return l;
	}

	for (i = len; i > x; i--) l[i] = l[i - 1];
	l[x] = strdup(s);
	return l;
}

static char **
string_append(char *s, char **l)
{
	return string_insert(s, l, IndexNull);
}

static unsigned int
string_list_length(char **l)
{
	int i;

	if (l == NULL) return 0;
	for (i = 0; l[i] != NULL; i++);
	return i;
}

static unsigned int
string_index(char c, char *s)
{
	int i;
	char *p;

	if (s == NULL) return IndexNull;

	for (i = 0, p = s; p[0] != '\0'; p++, i++)
	{
		if (p[0] == c) return i;
	}

	return IndexNull;
}

static char **
explode(char *s, char *delim)
{
	char **l = NULL;
	char *p, *t;
	int i, n;

	if (s == NULL) return NULL;

	p = s;
	while (p[0] != '\0')
	{
		for (i = 0; ((p[i] != '\0') && (string_index(p[i], delim) == IndexNull)); i++);
		n = i;
		t = malloc(n + 1);
		for (i = 0; i < n; i++) t[i] = p[i];
		t[n] = '\0';
		l = string_append(t, l);
		free(t);
		t = NULL;
		if (p[i] == '\0') return l;
		if (p[i + 1] == '\0') l = string_append("", l);
		p = p + i + 1;
	}
	return l;
}

static uint32_t
atoaccess(char *s)
{
	uint32_t a;

	if (s == NULL) return 0;
	if (strlen(s) != 6) return 0;

	a = 0;
	if (s[0] == 'r') a |= (NOTIFY_ACCESS_READ << NOTIFY_ACCESS_USER_SHIFT);
	if (s[1] == 'w') a |= (NOTIFY_ACCESS_WRITE << NOTIFY_ACCESS_USER_SHIFT);

	if (s[2] == 'r') a |= (NOTIFY_ACCESS_READ << NOTIFY_ACCESS_GROUP_SHIFT);
	if (s[3] == 'w') a |= (NOTIFY_ACCESS_WRITE << NOTIFY_ACCESS_GROUP_SHIFT);

	if (s[4] == 'r') a |= (NOTIFY_ACCESS_READ << NOTIFY_ACCESS_OTHER_SHIFT);
	if (s[5] == 'w') a |= (NOTIFY_ACCESS_WRITE << NOTIFY_ACCESS_OTHER_SHIFT);

	return a;
}

static void
read_config()
{
	FILE *f;
	struct stat sb;
	char line[1024];
	char **args;
	uint32_t argslen;
	uint32_t uid, gid, access, cid;
	uint64_t val64;

	/* Check config file */
	if (stat(CONFIG_FILE_PATH, &sb) != 0) return;
	
	if (sb.st_uid != 0)
	{
		log_message(ASL_LEVEL_ERR, "config file %s not owned by root: ignored\n", CONFIG_FILE_PATH);
		return;
	}

	if (sb.st_mode & 02)
	{
		log_message(ASL_LEVEL_ERR, "config file %s is world-writable: ignored\n", CONFIG_FILE_PATH);
		return;
	}

	/* Read config file */
	f = fopen(CONFIG_FILE_PATH, "r");
	if (f == NULL) return;

	forever
	{
		if (fgets(line, 1024, f) == NULL) break;
		if (line[0] == '\0') continue;
		if (line[0] == '#') continue;

		line[strlen(line) - 1] = '\0';
		args = explode(line, "\t ");
		argslen = string_list_length(args);
		if (argslen == 0) continue;

		if (!strcasecmp(args[0], "monitor"))
		{
			if (argslen < 3)
			{
				string_list_free(args);
				continue;
			}
			_notify_lib_register_plain(ns, args[1], 0, -1, 0, 0, &cid);
			service_open_file(cid, args[1], args[2], 0, 0, 0);
		}
		
		else if (!strcasecmp(args[0], "set"))
		{
			if (argslen < 3)
			{
				string_list_free(args);
				continue;
			}

			val64 = atoll(args[2]);
		
			_notify_lib_register_plain(ns, args[1], 0, -1, 0, 0, &cid);
			_notify_lib_set_state(ns, cid, val64, 0, 0);
		}
		
		else if (!strcasecmp(args[0], "reserve"))
		{
			if (argslen == 1)
			{
				string_list_free(args);
				continue;
			}

			uid = 0;
			gid = 0;
			access = NOTIFY_ACCESS_DEFAULT;

			if (argslen > 2) uid = atoi(args[2]);
			if (argslen > 3) gid = atoi(args[3]);
			if (argslen > 4) access = atoaccess(args[4]);

			if ((uid != 0) || (gid != 0)) _notify_lib_set_owner(ns, args[1], uid, gid);
			if (access != NOTIFY_ACCESS_DEFAULT) _notify_lib_set_access(ns, args[1], access);
		}
		else if (!strcasecmp(args[0], "quit"))
		{
			string_list_free(args);
			break;
		}

		string_list_free(args);
	}

	fclose(f);
}

static void
daemon_startup()
{
	uint32_t a;

	/* create kqueue */
	kq = kqueue();

	/* reserve "self." (internal) */
	a = 0;
	a |= (NOTIFY_ACCESS_READ << NOTIFY_ACCESS_USER_SHIFT);
	a |= (NOTIFY_ACCESS_WRITE << NOTIFY_ACCESS_USER_SHIFT);
	_notify_lib_set_access(ns, "self.",  a);

	/* read configuration */
	read_config();
}

static void
daemon_shutdown()
{
	watcher_shutdown();

	if (shm_enabled != 0) close_shared_memory();
	_notify_lib_notify_state_free(ns);
	if (status_file != NULL) free(status_file);
}

static void
poke_run_loop()
{
	mach_msg_empty_send_t msg;
	kern_return_t kstatus;
	
	memset(&msg, 0, sizeof(mach_msg_empty_send_t));
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSGH_BITS_ZERO);
	msg.header.msgh_remote_port = server_port;
	msg.header.msgh_local_port = MACH_PORT_NULL;
	msg.header.msgh_size = sizeof(mach_msg_empty_send_t);
	msg.header.msgh_id = PRINT_STATUS_MSG_ID;

	kstatus = mach_msg(&(msg.header), MACH_SEND_MSG | MACH_SEND_TIMEOUT, msg.header.msgh_size, 0, MACH_PORT_NULL, 10, MACH_PORT_NULL);
}

static void
catch_usr1(int x)
{
	status_request = STATUS_REQUEST_SHORT;
	poke_run_loop();
}

static void
catch_usr2(int x)
{
	status_request = STATUS_REQUEST_LONG;
	poke_run_loop();
}

static void
catch_winch(int x)
{
	if (log_cutoff == ASL_LEVEL_DEBUG) log_cutoff = ASL_LEVEL_NOTICE;
	else log_cutoff = ASL_LEVEL_DEBUG;
}

int
main(int argc, char *argv[])
{
	pthread_attr_t attr;
	kern_return_t kstatus;
	pthread_t t;
	int32_t i, first_start, do_startup;
	int kid;
	char line[1024], *sname;
	watcher_t *w;
	list_t *n;

	__notify_78945668_info__ = NOTIFYD_PROCESS_FLAG;
	signal(SIGPIPE, SIG_IGN);

	nslots = getpagesize() / sizeof(uint32_t);
	first_start = 0;
	do_startup = 1;
	sname = NOTIFY_SERVICE_NAME;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-d"))
		{
			log_cutoff = ASL_LEVEL_DEBUG;
		}
		else if (!strcmp(argv[i], "-D"))
		{
			debug = 1;
			debug_log_file = stderr;
			log_cutoff = ASL_LEVEL_DEBUG;
			restart = 0;
			sname = argv[++i];
		}
		else if (!strcmp(argv[i], "-log_file"))
		{
			debug_log_file = fopen(argv[++i], "a");
		}
		else if (!strcmp(argv[i], "-log_cutoff"))
		{
			log_cutoff = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "-no_startup"))
		{
			do_startup = 0;
		}
		else if (!strcmp(argv[i], "-shm_pages"))
		{
			nslots = atoi(argv[++i]) * (getpagesize() / sizeof(uint32_t));
			if (nslots == 0) shm_enabled = 0;
		}
		else if (!strcmp(argv[i], "-no_restart"))
		{
			restart = 0;
		}
	}

	/* Create state table early */
	ns = _notify_lib_notify_state_new(0);

	if (debug == 0) log_message(ASL_LEVEL_DEBUG, "notifyd started\n");

	if (geteuid() != 0)
	{
		log_message(ASL_LEVEL_ERR, "not root: disabled shared memory\n");
		shm_enabled = 0;
	}

	if (shm_enabled != 0)
	{
		i = open_shared_memory();
		if (i == -1)
		{
			log_message(ASL_LEVEL_ERR, "open_shared_memory failed: %s\n", strerror(errno));
			exit(1);
		}
	}

	/* get server port from launchd */
	kstatus = create_service(sname, &server_port);
	if (kstatus != KERN_SUCCESS)
	{
		log_message(ASL_LEVEL_ERR, "create_service failed (%u)\n", kstatus);
		exit(1);
	}

	/* create a port for receiving MACH_NOTIFY_DEAD_NAME notifications */
	kstatus = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &dead_session_port);
	if (kstatus != KERN_SUCCESS)
	{
		log_message(ASL_LEVEL_ERR, "mach_port_allocate dead_session_port failed (%u)\n", kstatus);
		exit(1);
	}

	/* create a set for our receivers */
	kstatus = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &listen_set);
	if (kstatus != KERN_SUCCESS)
	{
		log_message(ASL_LEVEL_ERR, "mach_port_allocate listen_set failed (%u)\n", kstatus);
		exit(1);
	}

	kstatus = mach_port_move_member(mach_task_self(), server_port, listen_set);
	if (kstatus != KERN_SUCCESS)
	{
		log_message(ASL_LEVEL_ERR, "mach_port_move_member server_port failed (%u)\n", kstatus);
		exit(1);
	}
	
	kstatus = mach_port_move_member(mach_task_self(), dead_session_port, listen_set);
	if (kstatus != KERN_SUCCESS)
	{
		log_message(ASL_LEVEL_ERR, "mach_port_move_member dead_session_port failed (%u)\n", kstatus);
		exit(1);
	}
	
	signal(SIGUSR1, catch_usr1);
	signal(SIGUSR2, catch_usr2);
	signal(SIGWINCH, catch_winch);

	if (do_startup != 0)
	{
		daemon_startup();
	}

	/* detach a thread for listening */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&t, &attr, (void *(*)(void *))server_side_loop, NULL);
	pthread_attr_destroy(&attr);

	if (debug == 0)
	{
		server_run_loop();
	}
	else
	{
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		pthread_create(&t, &attr, (void *(*)(void *))server_run_loop, NULL);
		pthread_attr_destroy(&attr);
		while (debug)
		{
			printf("> ");
			fflush(stdout);
			if (fgets(line, 1024, stdin) == NULL)
			{
				printf("Goodbye\n");
				debug = 0;
				continue;
			}

			if (line[0] == 'q')
			{
				printf("Goodbye\n");
				debug = 0;
				continue;
			}

			else if (line[0] == '?')
			{
				fprint_status(stderr);
				continue;
			}

			else if (line[0] == 'k')
			{
				kid = atoi(line + 1);
				watcher_trigger(kid, 0, 0); 
			}

			else if (line[0] == 'w')
			{
				kid = atoi(line + 1);
				for (n = watch_list; n != NULL; n = _nc_list_next(n))
				{
					w = _nc_list_data(n);
					if (w->wid == kid) watcher_printf(w, stderr);
				}
			}
		}
	}

	daemon_shutdown();

	log_message(ASL_LEVEL_DEBUG, "notifyd exiting\n");
	if (debug_log_file != NULL) fclose(debug_log_file);

	exit(0);
}
