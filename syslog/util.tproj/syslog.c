/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <netdb.h>
#include <notify.h>
#include <asl.h>
#include <asl_private.h>
#include <asl_ipc.h>
#include <asl_store.h>

#define MOD_CASE_FOLD 'C'
#define MOD_REGEX     'R'
#define MOD_SUBSTRING 'S'
#define MOD_PREFIX    'A'
#define MOD_SUFFIX    'Z'
#define MOD_NUMERIC   'N'

#define OP_EQ "eq"
#define OP_NE "ne"
#define OP_GT "gt"
#define OP_GE "ge"
#define OP_LT "lt"
#define OP_LE "le"

#define ASL_QUERY_OP_NOT	0x1000

#define FACILITY_CONSOLE "com.apple.console"

#define SEARCH_EOF -1
#define SEARCH_NULL 0
#define SEARCH_MATCH 1

#define PROC_NOT_FOUND -1
#define PROC_NOT_UNIQUE -2

#define RC_MASTER -1
#define RC_SYSLOGD -2

#define CHUNK 64
#define forever for(;;)

#define SEND_FORMAT_LEGACY 0
#define SEND_FORMAT_ASL 1

#define TIME_SEC		0x00000001
#define TIME_UTC		0x00000002
#define TIME_LCL		0x00000004

#define FORMAT_RAW		0x00000010
#define FORMAT_LEGACY	0x00000020
#define FORMAT_STD		0x00000040
#define FORMAT_XML		0x00000080

#define EXPORT			0x00000100

#define ASL_FILTER_MASK_PACEWNID 0xff
#define ASL_FILTER_MASK_PACEWNI  0x7f
#define ASL_FILTER_MASK_PACEWN   0x3f
#define ASL_FILTER_MASK_PACEW    0x1f
#define ASL_FILTER_MASK_PACE     0x0f
#define ASL_FILTER_MASK_PAC      0x07

#define FETCH_BATCH	256

#define _PATH_ASL_STORE "/var/log/asl.db"
static asl_store_t **dbstore = NULL;
static uint32_t store_count = 0;
static uint32_t store_raw = 1;

static asl_store_t *export = NULL;

/* notify SPI */
uint32_t notify_register_plain(const char *name, int *out_token);

extern char *asl_msg_to_string(aslmsg msg, uint32_t *len);
extern asl_msg_t *asl_msg_from_string(const char *buf);
extern char *asl_list_to_string(asl_search_result_t *list, uint32_t *outlen);
extern asl_search_result_t *asl_list_from_string(const char *buf);
extern int asl_msg_cmp(asl_msg_t *a, asl_msg_t *b);
extern time_t asl_parse_time(const char *in);
/* END PRIVATE API */

static const char *myname = "syslog";

#define ASL_SERVICE_NAME "com.apple.system.logger"
static mach_port_t asl_server_port = MACH_PORT_NULL;

void
usage()
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "%s -s [-r host] [-l level] message...\n", myname);
	fprintf(stderr, "   send a message\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "%s -s [-r host] -k key val [key val]...\n", myname);
	fprintf(stderr, "   send a message with the given keys and values\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "%s -c process [filter]\n", myname);
	fprintf(stderr, "   get (set if filter is specified) syslog filter for process (pid or name)\n");
	fprintf(stderr, "   level may be any combination of the characters \"p a c e w n i d\"\n");
	fprintf(stderr, "   p = Emergency (\"Panic\")\n");
	fprintf(stderr, "   a = Alert\n");
	fprintf(stderr, "   c = Critical\n");
	fprintf(stderr, "   e = Error\n");
	fprintf(stderr, "   w = Warning\n");
	fprintf(stderr, "   n = Notice\n");
	fprintf(stderr, "   i = Info\n");
	fprintf(stderr, "   d = Debug\n");
	fprintf(stderr, "   a minus sign preceeding a single letter means \"up to\" that level\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "%s -p [-db [file]...] [-k key [[op] val]]... [-o -k key [[op] val]] ...]...\n", myname);
	fprintf(stderr, "   -db   prune /var/log/asl.db or named file, rather than sending a prune command to syslogd.\n");
	fprintf(stderr, "   -p    prune datastore according to input expression (see below)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "%s [-db [file]...] [-x file] [-w [N]] [-F format] [-u] [-k key [[op] val]]... [-o -k key [[op] val]] ...]...\n", myname);
	fprintf(stderr, "   -db   read /var/log/asl.db or named file, rather than querying syslogd.\n");
	fprintf(stderr, "         use \"-\" to explicitly include a connection to syslogd.\n");
	fprintf(stderr, "   -x    export to named database, rather than printing\n");
	fprintf(stderr, "   -w    watch database (^C to quit)\n");
	fprintf(stderr, "         prints the last N matching lines (default 10) before waiting\n");
	fprintf(stderr, "         \"-w 0\" prints all matching lines before waiting\n");
	fprintf(stderr, "   -F    output format may be \"std\", \"raw\", \"bsd\", or \"xml\"\n");
	fprintf(stderr, "         format may also be a string containing variables of the form\n");
	fprintf(stderr, "         $Key or $(Key) - use the latter for non-whitespace delimited variables\n");
	fprintf(stderr, "   -T    timestamp format may be \"sec\" (seconds), \"utc\" (UTC), or \"local\" (local timezone)\n");
	fprintf(stderr, "   -u    print timestamps using UTC (equivalent to \"-T utc\")\n");
	fprintf(stderr, "   -k    key/value match\n");
	fprintf(stderr, "         if no operator or value is given, checks for the existance of the key\n");
	fprintf(stderr, "         if no operator is given, default is \"%s\"\n", OP_EQ);
	fprintf(stderr, "   -C    alias for \"-k Facility com.apple.console\"\n");
	fprintf(stderr, "   -o    begins a new query\n");
	fprintf(stderr, "         queries are \'OR\'ed together\n");
	fprintf(stderr, "operators are zero or more modifiers followed by a comparison\n");
	fprintf(stderr, "   %s   equal\n", OP_EQ);
	fprintf(stderr, "   %s   not equal\n", OP_NE);
	fprintf(stderr, "   %s   greater than\n", OP_GT);
	fprintf(stderr, "   %s   greater or equal\n", OP_GE);
	fprintf(stderr, "   %s   less than\n", OP_LT);
	fprintf(stderr, "   %s   less or equal\n", OP_LE);
	fprintf(stderr, "optional modifiers for operators\n");
	fprintf(stderr, "   %c    case-fold\n", MOD_CASE_FOLD);
	fprintf(stderr, "   %c    regular expression\n", MOD_REGEX);
	fprintf(stderr, "   %c    substring\n", MOD_SUBSTRING);
	fprintf(stderr, "   %c    prefix\n", MOD_PREFIX);
	fprintf(stderr, "   %c    suffix\n", MOD_SUFFIX);
	fprintf(stderr, "   %c    numeric comparison\n", MOD_NUMERIC);
}

const char *
notify_status_string(int status)
{
	if (status == NOTIFY_STATUS_OK) return "OK";
	if (status == NOTIFY_STATUS_INVALID_NAME) return "Process not registered";
	if (status == NOTIFY_STATUS_NOT_AUTHORIZED) return "Not authorized";
	return "Operation failed";
}

const char *
asl_level_string(int level)
{
	if (level == ASL_LEVEL_EMERG) return ASL_STRING_EMERG;
	if (level == ASL_LEVEL_ALERT) return ASL_STRING_ALERT;
	if (level == ASL_LEVEL_CRIT) return ASL_STRING_CRIT;
	if (level == ASL_LEVEL_ERR) return ASL_STRING_ERR;
	if (level == ASL_LEVEL_WARNING) return ASL_STRING_WARNING;
	if (level == ASL_LEVEL_NOTICE) return ASL_STRING_NOTICE;
	if (level == ASL_LEVEL_INFO) return ASL_STRING_INFO;
	if (level == ASL_LEVEL_DEBUG) return ASL_STRING_DEBUG;
	return "Unknown";
}

int
procinfo(char *pname, int *pid, int *uid)
{
	int mib[4];
	int i, status, nprocs;
	size_t miblen, size;
	struct kinfo_proc *procs, *newprocs;

	size = 0;
	procs = NULL;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_ALL;
	mib[3] = 0;
	miblen = 3;

	status = sysctl(mib, miblen, NULL, &size, NULL, 0);
	do
	{
		size += size / 10;
		newprocs = reallocf(procs, size);
		if (newprocs == NULL)
		{
			if (procs != NULL) free(procs);
			return PROC_NOT_FOUND;
		}

		procs = newprocs;
		status = sysctl(mib, miblen, procs, &size, NULL, 0);
	} while ((status == -1) && (errno == ENOMEM));

	if (status == -1)
	{
		if (procs != NULL) free(procs);
		return PROC_NOT_FOUND;
	}

	if (size % sizeof(struct kinfo_proc) != 0)
	{
		if (procs != NULL) free(procs);
		return PROC_NOT_FOUND;
	}

	if (procs == NULL) return PROC_NOT_FOUND;

	nprocs = size / sizeof(struct kinfo_proc);

	if (pname == NULL)
	{
		/* Search for a pid */
		for (i = 0; i < nprocs; i++) 
		{
			if (*pid == procs[i].kp_proc.p_pid)
			{
				*uid = procs[i].kp_eproc.e_ucred.cr_uid;
				return 0;
			}
		}

		return PROC_NOT_FOUND;
	}

	*pid = PROC_NOT_FOUND;

	for (i = 0; i < nprocs; i++) 
	{
		if (!strcmp(procs[i].kp_proc.p_comm, pname))
		{
			if (*pid != PROC_NOT_FOUND)
			{
				free(procs);
				return PROC_NOT_UNIQUE;
			}

			*pid = procs[i].kp_proc.p_pid;
			*uid = procs[i].kp_eproc.e_ucred.cr_uid;
		}
	}

	free(procs);
	if (*pid == PROC_NOT_FOUND) return PROC_NOT_FOUND;

	return 0;
}

int
rcontrol_get_string(const char *prefix, int pid, int *val)
{
	int t, status;
	char *name;
	uint64_t x;

	status = NOTIFY_STATUS_OK;

	if (pid == RC_SYSLOGD)
	{
		status = notify_register_plain(NOTIFY_SYSTEM_ASL_FILTER, &t);
	}
	else if (pid == RC_MASTER)
	{
		status = notify_register_plain(NOTIFY_SYSTEM_MASTER, &t);
	}
	else
	{
		name = NULL;
		asprintf(&name, "%s.%d", prefix, pid);
		if (name == NULL) return NOTIFY_STATUS_FAILED;

		status = notify_register_plain(name, &t);
		free(name);
	}

	if (status != NOTIFY_STATUS_OK) return status;

	x = 0;
	status = notify_get_state(t, &x);
	notify_cancel(t);

	*val = x;

	return status;
}

int
rcontrol_set_string(const char *prefix, int pid, int filter)
{
	int t, status;
	char *name;
	uint64_t x;

	status = NOTIFY_STATUS_OK;

	if (pid == RC_SYSLOGD)
	{
		status = notify_register_plain(NOTIFY_SYSTEM_ASL_FILTER, &t);
	}
	else if (pid == RC_MASTER)
	{
		status = notify_register_plain(NOTIFY_SYSTEM_MASTER, &t);
	}
	else
	{
		name = NULL;
		asprintf(&name, "%s.%d", prefix, pid);
		if (name == NULL) return NOTIFY_STATUS_FAILED;

		status = notify_register_plain(name, &t);
		free(name);
	}

	if (status != NOTIFY_STATUS_OK) return status;

	x = filter;
	status = notify_set_state(t, x);
	if ((pid == RC_SYSLOGD) && (status == NOTIFY_STATUS_OK)) status = notify_post(NOTIFY_SYSTEM_ASL_FILTER);
	notify_cancel(t);
	return status;
}

int
asl_string_to_filter(char *s)
{
	int f, i;

	if (s == NULL) return 0;
	if (s[0] == '\0') return 0;

	if ((s[0] >= '0') && (s[0] <= '9')) return ASL_FILTER_MASK(atoi(s));

	if (s[0] == '-')
	{
		if ((s[1] == 'P') || (s[1] == 'p')) i = ASL_LEVEL_EMERG;
		else if ((s[1] == 'A') || (s[1] == 'a')) i = ASL_LEVEL_ALERT;
		else if ((s[1] == 'C') || (s[1] == 'c')) i = ASL_LEVEL_CRIT;
		else if ((s[1] == 'E') || (s[1] == 'e')) i = ASL_LEVEL_ERR;
		else if ((s[1] == 'X') || (s[1] == 'x')) i = ASL_LEVEL_ERR;
		else if ((s[1] == 'W') || (s[1] == 'w')) i = ASL_LEVEL_WARNING;
		else if ((s[1] == 'N') || (s[1] == 'n')) i = ASL_LEVEL_NOTICE;
		else if ((s[1] == 'I') || (s[1] == 'i')) i = ASL_LEVEL_INFO;
		else if ((s[1] == 'D') || (s[1] == 'd')) i = ASL_LEVEL_DEBUG;
		else i = atoi(s + 1);
		f = ASL_FILTER_MASK_UPTO(i);
		return f;
	}

	f = 0;
	for (i = 0; s[i] != '\0'; i++)
	{
		if ((s[i] == 'P') || (s[i] == 'p')) f |= ASL_FILTER_MASK_EMERG;
		else if ((s[i] == 'A') || (s[i] == 'a')) f |= ASL_FILTER_MASK_ALERT;
		else if ((s[i] == 'C') || (s[i] == 'c')) f |= ASL_FILTER_MASK_CRIT;
		else if ((s[i] == 'E') || (s[i] == 'e')) f |= ASL_FILTER_MASK_ERR;
		else if ((s[i] == 'X') || (s[i] == 'x')) f |= ASL_FILTER_MASK_ERR;
		else if ((s[i] == 'W') || (s[i] == 'w')) f |= ASL_FILTER_MASK_WARNING;
		else if ((s[i] == 'N') || (s[i] == 'n')) f |= ASL_FILTER_MASK_NOTICE;
		else if ((s[i] == 'I') || (s[i] == 'i')) f |= ASL_FILTER_MASK_INFO;
		else if ((s[i] == 'D') || (s[i] == 'd')) f |= ASL_FILTER_MASK_DEBUG;
	}

	return f;
}

char *
asl_filter_string(int f)
{
	static char str[1024];
	int i;

	memset(str, 0, sizeof(str));
	i = 0;

	if ((f == ASL_FILTER_MASK_PACEWNID) != 0)
	{
		strcat(str, "Emergency - Debug");
		return str;
	}

	if ((f == ASL_FILTER_MASK_PACEWNI) != 0)
	{
		strcat(str, "Emergency - Info");
		return str;
	}

	if ((f == ASL_FILTER_MASK_PACEWN) != 0)
	{
		strcat(str, "Emergency - Notice");
		return str;
	}

	if ((f == ASL_FILTER_MASK_PACEW) != 0)
	{
		strcat(str, "Emergency - Warning");
		return str;
	}

	if ((f == ASL_FILTER_MASK_PACE) != 0)
	{
		strcat(str, "Emergency - Error");
		return str;
	}

	if ((f == ASL_FILTER_MASK_PAC) != 0)
	{
		strcat(str, "Emergency - Critical");
		return str;
	}

	if ((f & ASL_FILTER_MASK_EMERG) != 0)
	{
		strcat(str, "Emergency");
		i++;
	}

	if ((f & ASL_FILTER_MASK_ALERT) != 0)
	{
		if (i > 0) strcat(str, ", ");
		strcat(str, "Alert");
		i++;
	}

	if ((f & ASL_FILTER_MASK_CRIT) != 0)
	{
		if (i > 0) strcat(str, ", ");
		strcat(str, "Critical");
		i++;
	}

	if ((f & ASL_FILTER_MASK_ERR) != 0)
	{
		if (i > 0) strcat(str, ", ");
		strcat(str, "Error");
		i++;
	}

	if ((f & ASL_FILTER_MASK_WARNING) != 0)
	{
		if (i > 0) strcat(str, ", ");
		strcat(str, "Warning");
		i++;
	}

	if ((f & ASL_FILTER_MASK_NOTICE) != 0)
	{
		if (i > 0) strcat(str, ", ");
		strcat(str, "Notice");
		i++;
	}

	if ((f & ASL_FILTER_MASK_INFO) != 0)
	{
		if (i > 0) strcat(str, ", ");
		strcat(str, "Info");
		i++;
	}

	if ((f & ASL_FILTER_MASK_DEBUG) != 0)
	{
		if (i > 0) strcat(str, ", ");
		strcat(str, "Debug");
		i++;
	}

	if (i == 0) sprintf(str, "Off");

	return str;
}

int
rcontrol_get(const char *prefix, int pid)
{
	int filter, status;
	const char *name;

	filter = 0;

	if (pid < 0)
	{
		name = "Master";
		if (pid == RC_SYSLOGD) name = "ASL Data Store";

		status = rcontrol_get_string(NULL, pid, &filter);
		if (status == NOTIFY_STATUS_OK)
		{
			printf("%s filter mask: %s\n", name, asl_filter_string(filter));
			return 0;
		}

		printf("Unable to determine %s filter mask\n", name);
		return -1;
	}

	status = rcontrol_get_string(prefix, pid, &filter);
	if (status == NOTIFY_STATUS_OK)
	{
		printf("Process %d syslog filter mask: %s\n", pid, asl_filter_string(filter));
		return 0;
	}

	printf("Unable to determine syslog filter mask for pid %d\n", pid);
	return -1;
}

int
rcontrol_set(const char *prefix, int pid, int filter)
{
	int status;
	const char *name;

	if (pid < 0)
	{
		name = "Master";
		if (pid == RC_SYSLOGD) name = "ASL Data Store";
		status = rcontrol_set_string(NULL, pid, filter);

		if (status == NOTIFY_STATUS_OK)
		{
			printf("Set %s syslog filter mask: %s\n", name, asl_filter_string(filter));
			return 0;
		}

		printf("Unable to set %s syslog filter mask: %s\n", name, notify_status_string(status));
		return -1;
	}

	status = rcontrol_set_string(prefix, pid, filter);
	if (status == NOTIFY_STATUS_OK)
	{
		printf("Set process %d syslog filter mask set: %s\n", pid, asl_filter_string(filter));
		return 0;
	}

	printf("Unable to set syslog filter mask for pid %d: %s\n", pid, notify_status_string(status));
	return -1;
}

int
rsend(aslmsg msg, char *rhost)
{
	char *str, *out;
	uint32_t len, level;
	char *timestr;
	const char *val;
	time_t tick;
	struct tm gtime;
	int s;
	struct sockaddr_in dst;
	struct hostent *h;
	char myname[MAXHOSTNAMELEN + 1];

	if (msg == NULL) return 0;

	h = gethostbyname(rhost);
	if (h == NULL) return -1;

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s <= 0) return -1;

	memset(&dst, 0, sizeof(struct sockaddr_in));
	memcpy(&(dst.sin_addr.s_addr), h->h_addr_list[0], 4);
	dst.sin_family = AF_INET;
	dst.sin_port = 514;
	dst.sin_len = sizeof(struct sockaddr_in);

	level = ASL_LEVEL_DEBUG;

	val = asl_get(msg, ASL_KEY_LEVEL);
	if (val != NULL) level = atoi(val);

	memset(&gtime, 0, sizeof(struct tm));
	timestr = NULL;

	tick = time(NULL);
	gmtime_r(&tick, &gtime);

	/* Canonical form: YYYY.MM.DD hh:mm:ss UTC */
	asprintf(&timestr, "%d.%02d.%02d %02d:%02d:%02d UTC", gtime.tm_year + 1900, gtime.tm_mon + 1, gtime.tm_mday, gtime.tm_hour, gtime.tm_min, gtime.tm_sec);

	if (timestr != NULL)
	{
		asl_set(msg, ASL_KEY_TIME, timestr);
		free(timestr);
	}

	if (gethostname(myname, MAXHOSTNAMELEN) == 0) asl_set(msg, ASL_KEY_HOST, myname);

	len = 0;
	str = asl_msg_to_string(msg, &len);
	if (str == NULL) return -1;

	asprintf(&out, "%10u %s\n", len+1, str);
	free(str);
	if (out == NULL) return -1;

	sendto(s, out, len+12, 0, (const struct sockaddr *)&dst, sizeof(struct sockaddr_in));

	free(out);
	close(s);
	return 0;
}

int
rlegacy(char *msg, int level, char *rhost)
{
	char *out;
	uint32_t len;
	time_t tick;
	char *ltime;
	int s;
	struct sockaddr_in dst;
	struct hostent *h;
	char myname[MAXHOSTNAMELEN + 1];

	if (msg == NULL) return 0;

	h = gethostbyname(rhost);
	if (h == NULL) return -1;

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s <= 0) return -1;

	memset(&dst, 0, sizeof(struct sockaddr_in));
	memcpy(&(dst.sin_addr.s_addr), h->h_addr_list[0], 4);
	dst.sin_family = AF_INET;
	dst.sin_port = 514;
	dst.sin_len = sizeof(struct sockaddr_in);

	tick = time(NULL);
	ltime = ctime(&tick);
	ltime[19] = '\0';

	gethostname(myname, MAXHOSTNAMELEN);

	asprintf(&out, "<%d>%s %s syslog[%d]: %s", level, ltime+4, myname, getpid(), msg);
	len = strlen(out);
	sendto(s, out, len, 0, (const struct sockaddr *)&dst, sizeof(struct sockaddr_in));

	free(out);
	close(s);
	return 0;
}

static int
_isanumber(char *s)
{
	int i;

	if (s == NULL) return 0;

	i = 0;
	if ((s[0] == '-') || (s[0] == '+')) i = 1;

	if (s[i] == '\0') return 0;

	for (; s[i] != '\0'; i++)
	{
		if (!isdigit(s[i])) return 0;
	}

	return 1;
}

int
asl_string_to_level(const char *s)
{
	if (s == NULL) return -1;

	if ((s[0] >= '0') && (s[0] <= '7') && (s[1] == '\0')) return atoi(s);

	if (!strncasecmp(s, "em", 2)) return ASL_LEVEL_EMERG;
	else if (!strncasecmp(s, "p",  1)) return ASL_LEVEL_EMERG;
	else if (!strncasecmp(s, "a",  1)) return ASL_LEVEL_ALERT;
	else if (!strncasecmp(s, "c",  1)) return ASL_LEVEL_CRIT;
	else if (!strncasecmp(s, "er", 2)) return ASL_LEVEL_ERR;
	else if (!strncasecmp(s, "x",  1)) return ASL_LEVEL_ERR;
	else if (!strncasecmp(s, "w",  1)) return ASL_LEVEL_WARNING;
	else if (!strncasecmp(s, "n",  1)) return ASL_LEVEL_NOTICE;
	else if (!strncasecmp(s, "i",  1)) return ASL_LEVEL_INFO;
	else if (!strncasecmp(s, "d",  1)) return ASL_LEVEL_DEBUG;

	return -1;
}

int
syslog_remote_control(int argc, char *argv[])
{
	int pid, uid, status, mask;
	const char *prefix;

	if ((argc < 3) || (argc > 4))
	{
		fprintf(stderr, "usage:\n");
		fprintf(stderr, "%s -c process [mask]\n", myname);
		fprintf(stderr, "   get (set if mask is specified) syslog filter mask for process (pid or name)\n");
		fprintf(stderr, "   process may be pid or process name\n");
		fprintf(stderr, "   use \"-c 0\" to get master syslog filter mask\n");
		fprintf(stderr, "   use \"-c 0 off\" to disable master syslog filter mask\n");
		fprintf(stderr, "\n");
		return -1;
	}

	pid = RC_MASTER;
	uid = -2;

	status = PROC_NOT_FOUND;

	if ((!strcmp(argv[2], "syslogd")) || (!strcmp(argv[2], "syslog")))
	{
		pid = RC_SYSLOGD;
		uid = 0;
		status = 0;
	}
	else if (_isanumber(argv[2]) != 0)
	{
		pid = atoi(argv[2]);
		status = procinfo(NULL, &pid, &uid);
	}
	else
	{
		status = procinfo(argv[2], &pid, &uid);
	}

	if (status == PROC_NOT_FOUND)
	{
		fprintf(stderr, "%s: process not found\n", argv[2]);
		return -1;
	}

	if (status == PROC_NOT_UNIQUE)
	{
		fprintf(stderr, "%s: multiple processes found\n", argv[2]);
		fprintf(stderr, "use pid to identify a process uniquely\n");
		return -1;
	}

	if (pid == 0) pid = RC_MASTER;

	prefix = NOTIFY_PREFIX_USER;
	if (uid == 0) prefix = NOTIFY_PREFIX_SYSTEM;

	if (argc == 4)
	{
		if ((pid == RC_MASTER) && (!strcasecmp(argv[3], "off"))) mask = 0;
		else if ((pid == RC_SYSLOGD) && (!strcasecmp(argv[3], "off"))) mask = 0;
		else
		{
			mask = asl_string_to_filter(argv[3]);
			if (mask < 0)
			{
				printf("unknown syslog mask: %s\n", argv[3]);
				return -1;
			}
		}

		rcontrol_set(prefix, pid, mask);
	}
	else
	{
		rcontrol_get(prefix, pid);
	}

	return 0;
}

int
syslog_send(int argc, char *argv[])
{
	int i, start, kv, len, rfmt, rlevel;
	aslclient asl;
	aslmsg m;
	char tmp[64], *str, *rhost;

	kv = 0;
	rhost = NULL;
	rfmt = SEND_FORMAT_LEGACY;
	start = 1;
	rlevel = 7;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-s")) start = i+1;
		else if (!strcmp(argv[i], "-k"))
		{
			kv = 1;
			rfmt = SEND_FORMAT_ASL;
		}
		else if (!strcmp(argv[i], "-r"))
		{
			rhost = argv[++i];
			start = i+1;
		}
		else if (!strcmp(argv[i], "-l"))
		{
			rlevel = asl_string_to_level(argv[++i]);
			if (rlevel < 0)
			{
				fprintf(stderr, "Unknown level: %s\n", argv[i]);
				return(-1);
			}
			start = i+1;
		}
	}

	asl = asl_open(myname, "syslog", 0);
	asl_set_filter(asl, ASL_FILTER_MASK_UPTO(ASL_LEVEL_DEBUG));

	m = asl_new(ASL_TYPE_MSG);
	asl_set(m, ASL_KEY_SENDER, myname);

	sprintf(tmp, "%d", rlevel);
	asl_set(m, ASL_KEY_LEVEL, tmp);

	str = NULL;

	if (kv == 0)
	{
		len = 0;
		for (i = start; i < argc; i++) len += (strlen(argv[i]) + 1);
		str = calloc(len + 1, 1);
		if (str == NULL) return -1;

		for (i = start; i < argc; i++)
		{
			strcat(str, argv[i]);
			if ((i+1) < argc) strcat(str, " ");
		}
		asl_set(m, ASL_KEY_MSG, str);
	}
	else
	{
		for (i = start + 1; i < argc; i += 2) asl_set(m, argv[i], argv[i + 1]);
	}

	if (rhost == NULL)
	{
		asl_send(asl, m);
	}
	else if (rfmt == SEND_FORMAT_ASL)
	{
		rsend(m, rhost);
	}
	else if ((rfmt == SEND_FORMAT_LEGACY) && (str != NULL))
	{
		rlegacy(str, rlevel, rhost);
	}

	asl_free(m);

	if (str != NULL) free(str);

	asl_close(asl);

	return 0;
}

static void
print_xml_header(FILE *f)
{
	if (f == NULL) return;

	fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(f, "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n");
	fprintf(f, "<plist version=\"1.0\">\n");
	fprintf(f, "<array>\n");
}

static void
print_xml_trailer(FILE *f)
{
	if (f == NULL) return;

	fprintf(f, "</array>\n");
	fprintf(f, "</plist>\n");
}

static void
print_xml_str(FILE *f, const char *str)
{
	uint32_t i;

	if (f == NULL) return;
	if (str == NULL) return;

	for (i = 0; str[i] != '\0'; i++)
	{
		if (str[i] == '&') fprintf(f, "&amp;");
		else if (str[i] == '<') fprintf(f, "&lt;");
		else if (str[i] == '>') fprintf(f, "&gt;");
		else if (str[i] == '"') fprintf(f, "&quot;");
		else if (str[i] == '\'') fprintf(f, "&apos;");
		else fprintf(f, "%c", str[i]);
	}
}

static void
printsafe(FILE *f, const char *str)
{
	uint8_t c;
	uint32_t i;

	if (f == NULL) return;
	if (str == NULL) return;

	for (i = 0; str[i] != '\0'; i++)
	{
		c = str[i];

		if (isascii(c) && iscntrl(c))
		{
			if (c == '\n') printf("\\n");
			else if (c == '\t') printf("\t");
			else printf("^%c", c ^ 0100);
		}
		else printf("%c", c);
	}
}

static void
printmsg(FILE *f, asl_msg_t *msg, char *fmt, int pflags)
{
	char *str;
	const char *mf, *tf;
	uint32_t len, status;
	uint64_t msgid;

	if (f == NULL)
	{
		if (export != NULL)
		{
			status = asl_store_save(export, msg, -1, -1, &msgid);
			if (status != ASL_STATUS_OK)
			{
				fprintf(stderr, "export database write failed: %s\n", asl_store_error(status));
				asl_store_close(export);
				export = NULL;
			}
		}

		return;
	}

	mf = ASL_MSG_FMT_RAW;
	if (fmt != NULL) mf = (const char *)fmt;
	else if (pflags & FORMAT_STD) mf = ASL_MSG_FMT_STD;
	else if (pflags & FORMAT_LEGACY) mf = ASL_MSG_FMT_BSD;
	else if (pflags & FORMAT_XML) mf = ASL_MSG_FMT_XML;

	tf = ASL_TIME_FMT_SEC;
	if (pflags & TIME_UTC) tf = ASL_TIME_FMT_UTC;
	if (pflags & TIME_LCL) tf = ASL_TIME_FMT_LCL;

	len = 0;
	str = asl_format_message(msg, mf, tf, &len);
	if (str != NULL)
	{
		fprintf(f, "%s", str);
		free(str);
	}
}

uint32_t
send_prune(asl_search_result_t *pl)
{
	char *str;
	caddr_t vmstr;
	uint32_t len, status;
	kern_return_t kstatus;
	security_token_t sec;

	if (asl_server_port == MACH_PORT_NULL) return 1;

	len = 0;
	str = asl_list_to_string(pl, &len);

	kstatus = vm_allocate(mach_task_self(), (vm_address_t *)&vmstr, len, TRUE);
	if (kstatus != KERN_SUCCESS)
	{
		free(str);
		return 1;
	}

	memmove(vmstr, str, len);
	free(str);

	sec.val[0] = -1;
	sec.val[1] = -1;
	status = 0;

	kstatus = _asl_server_prune(asl_server_port, (caddr_t)vmstr, len, (int *)&status, &sec);
	if (kstatus != KERN_SUCCESS) status = 1;

	return status;
}

asl_search_result_t *
send_query(asl_search_result_t *q, uint64_t start, int count, int dir, uint64_t *last)
{
	char *str, *res;
	caddr_t vmstr;
	uint32_t len, reslen, status;
	kern_return_t kstatus;
	security_token_t sec;
	asl_search_result_t *l;

	if (asl_server_port == MACH_PORT_NULL) return NULL;

	len = 0;
	str = asl_list_to_string(q, &len);

	kstatus = vm_allocate(mach_task_self(), (vm_address_t *)&vmstr, len, TRUE);
	if (kstatus != KERN_SUCCESS)
	{
		free(str);
		return NULL;
	}

	memmove(vmstr, str, len);
	free(str);

	res = NULL;
	reslen = 0;
	sec.val[0] = -1;
	sec.val[1] = -1;
	status = 0;

	kstatus = _asl_server_query(asl_server_port, (caddr_t)vmstr, len, start, count, dir, (caddr_t *)&res, &reslen, last, (int *)&status, &sec);

	if (res == NULL) return NULL;
	l = asl_list_from_string(res);
	vm_deallocate(mach_task_self(), (vm_address_t)res, reslen);
	return l;
}

asl_search_result_t *
db_query(asl_store_t *s, asl_search_result_t *q, uint64_t qmin, uint64_t *cmax)
{
	uint32_t status;
	asl_search_result_t *res;

	res = NULL;
	status = asl_store_match(s, q, &res, cmax, qmin, 0, 1, 0, 0);
	if (status != 0) return NULL;

	return res;
}

void
search_once(FILE *f, char *pfmt, int pflags, asl_search_result_t *ql, uint64_t qmin, uint64_t *cmax, uint32_t count, uint32_t tail)
{
	asl_search_result_t *res;
	int i, j;

	if (pflags & FORMAT_XML) print_xml_header(f);

	i = 0;
	while (i < store_count)
	{
		res = NULL;
		if ((dbstore[i] == NULL) && (store_raw == 0))
		{
			if (count == 0)
			{
				res = send_query(ql, qmin, 0, 0, cmax);
				i++;
			}
			else
			{
				res = send_query(ql, qmin, count, 0, cmax);
				if (*cmax > qmin) qmin = *cmax;
				if (res == NULL) i++;
				else if (res->count < count) i++;
			}
		}
		else
		{
			res = db_query(dbstore[i], ql, qmin, cmax);
			i++;
		}

		if (res != NULL)
		{
			j = 0;
			if (tail != 0)
			{
				j = res->count - tail;
				tail = 0;
				if (j < 0) j = 0;
			}

			for (; j < res->count; j++) printmsg(f, res->msg[j], pfmt, pflags);

			aslresponse_free((aslresponse)res);
		}
	}

	if (pflags & FORMAT_XML) print_xml_trailer(f);
}

uint32_t
optype(char *o)
{
	uint32_t op, i;

	op = ASL_QUERY_OP_NULL;

	if (o == NULL) return op;

	for (i = 0; o[i] != '\0'; i++)
	{
		if (o[i] == MOD_CASE_FOLD) op |= ASL_QUERY_OP_CASEFOLD;
		else if (o[i] == MOD_REGEX) op |= ASL_QUERY_OP_REGEX;
		else if (o[i] == MOD_NUMERIC) op |= ASL_QUERY_OP_NUMERIC;
		else if (o[i] == MOD_SUBSTRING) op |= ASL_QUERY_OP_SUBSTRING;
		else if (o[i] == MOD_PREFIX) op |= ASL_QUERY_OP_PREFIX;
		else if (o[i] == MOD_SUFFIX) op |= ASL_QUERY_OP_SUFFIX;

		else if (!strncasecmp(o+i, OP_EQ, sizeof(OP_EQ)))
		{
			op |= ASL_QUERY_OP_EQUAL;
			i += (sizeof(OP_EQ) - 2);
		}
		else if (!strncasecmp(o+i, OP_NE, sizeof(OP_NE)))
		{
			op |= ASL_QUERY_OP_NOT_EQUAL;
			i += (sizeof(OP_NE) - 2);
		}
		else if (!strncasecmp(o+i, OP_GT, sizeof(OP_GT)))
		{
			op |= ASL_QUERY_OP_GREATER;
			i += (sizeof(OP_GT) - 2);
		}
		else if (!strncasecmp(o+i, OP_GE, sizeof(OP_GE)))
		{
			op |= ASL_QUERY_OP_GREATER_EQUAL;
			i += (sizeof(OP_GE) - 2);
		}
		else if (!strncasecmp(o+i, OP_LT, sizeof(OP_LT)))
		{
			op |= ASL_QUERY_OP_LESS;
			i += (sizeof(OP_LT) - 2);
		}
		else if (!strncasecmp(o+i, OP_LE, sizeof(OP_LE)))
		{
			op |= ASL_QUERY_OP_LESS_EQUAL;
			i += (sizeof(OP_LE) - 2);
		}
		else
		{
			fprintf(stderr, "invalid option: %s\n", o);
			return 0;
		}
	}

	/* sanity check */
	if (op & ASL_QUERY_OP_NUMERIC)
	{
		if (op & ASL_QUERY_OP_CASEFOLD)
		{
			fprintf(stderr, "warning: case fold modifier has no effect with numeric comparisons\n");
			op &= ~ASL_QUERY_OP_CASEFOLD;
		}

		if (op & ASL_QUERY_OP_REGEX)
		{
			fprintf(stderr, "warning: regex modifier has no effect with numeric comparisons\n");
			op &= ~ASL_QUERY_OP_REGEX;
		}

		if (op & ASL_QUERY_OP_SUBSTRING)
		{
			fprintf(stderr, "warning: substring modifier has no effect with numeric comparisons\n");
			op &= ~ASL_QUERY_OP_SUBSTRING;
		}

		if (op & ASL_QUERY_OP_PREFIX)
		{
			fprintf(stderr, "warning: prefix modifier has no effect with numeric comparisons\n");
			op &= ~ASL_QUERY_OP_PREFIX;
		}

		if (op & ASL_QUERY_OP_SUFFIX)
		{
			fprintf(stderr, "warning: suffix modifier has no effect with numeric comparisons\n");
			op &= ~ASL_QUERY_OP_SUFFIX;
		}
	}

	if (op & ASL_QUERY_OP_REGEX)
	{
		if (op & ASL_QUERY_OP_SUBSTRING)
		{
			fprintf(stderr, "warning: substring modifier has no effect with regular expression comparisons\n");
			op &= ~ASL_QUERY_OP_SUBSTRING;
		}

		if (op & ASL_QUERY_OP_PREFIX)
		{
			fprintf(stderr, "warning: prefix modifier has no effect with regular expression comparisons\n");
			op &= ~ASL_QUERY_OP_PREFIX;
		}

		if (op & ASL_QUERY_OP_SUFFIX)
		{
			fprintf(stderr, "warning: suffix modifier has no effect with regular expression comparisons\n");
			op &= ~ASL_QUERY_OP_SUFFIX;
		}
	}

	return op;
}

int
add_op(asl_msg_t *q, char *key, char *op, char *val, uint32_t flags)
{
	uint32_t o;

	if (key == NULL) return -1;
	if (q == NULL) return -1;

	o = ASL_QUERY_OP_NULL;
	if (op != NULL)
	{
		o = optype(op);
		if (o == ASL_QUERY_OP_NULL) return -1;
		if (val == NULL)
		{
			fprintf(stderr, "no value supplied for operator %s %s\n", key, op);
			return -1;
		}

		if ((o & ASL_QUERY_OP_NUMERIC) && (strcmp(key, ASL_KEY_TIME) != 0) && (_isanumber(val) == 0))
		{
			fprintf(stderr, "non-numeric value supplied for numeric operator %s %s %s\n", key, op, val);
			return -1;
		}

	}

	o |= flags;
	asl_set_query(q, key, val, o);

	return 0;
}

static void
add_store(const char *name, uint32_t flags)
{
	asl_store_t *s;
	uint32_t status;

	s = NULL;

	if (name != NULL)
	{
		status = asl_store_open(name, flags, &s);
		if (status != ASL_STATUS_OK)
		{
			fprintf(stderr, "database %s open failed: %s \n", name, asl_store_error(status));
			exit(1);
		}

		if (s == NULL)
		{
			fprintf(stderr, "database %s open failed\n", name);
			exit(1);
		}
	}
	else
	{
		store_raw = 0;
	}

	if (store_count == 0) dbstore = (asl_store_t **)calloc(1, sizeof(asl_store_t *));
	else dbstore = (asl_store_t **)reallocf(dbstore, (store_count + 1) * sizeof(asl_store_t *));

	if (dbstore == NULL)
	{
		fprintf(stderr, "Can't allocate memory!\n");
		exit(1);
	}

	dbstore[store_count] = s;
	store_count++;
}

int
main(int argc, char *argv[])
{
	FILE *outfile;
	int i, j, n, watch, prune, status, pflags, tflags, sflags, iamroot, user_tflag;
	int notify_file, notify_token;
	asl_search_result_t *qlist, *lx, *res;
	asl_msg_t *cq;
	char *logname, *pfmt;
	const char *dbname, *exportname;
	uint32_t flags, tail_count, batch;
	uint64_t qmin, cmax;
	kern_return_t kstatus;

	watch = 0;
	prune = 0;
	iamroot = 0;
	user_tflag = 0;
	logname = NULL;
	pfmt = NULL;
	flags = 0;
	tail_count = 0;
	batch = FETCH_BATCH;
	sflags = ASL_STORE_FLAG_READ_ONLY;
	pflags = FORMAT_STD;
	tflags = TIME_LCL;
	cq = NULL;
	dbname = _PATH_ASL_STORE;
	exportname = NULL;

	if (getuid() == 0) iamroot = 1;

	for (i = 1; i < argc; i++)
	{
		if ((!strcmp(argv[i], "-help")) || (!strcmp(argv[i], "--help")))
		{
			usage();
			exit(0);
		}

		if (!strcmp(argv[i], "-s"))
		{
			syslog_send(argc, argv);
			exit(0);
		}

		if (!strcmp(argv[i], "-c"))
		{
			syslog_remote_control(argc, argv);
			exit(0);
		}

		if (!strcmp(argv[i], "-p"))
		{
			prune = 1;
			sflags = 0;
		}
	}

	qlist = (asl_search_result_t *)calloc(1, sizeof(asl_search_result_t));
	if (qlist == NULL) exit(1);

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-db"))
		{
			if ((i + 1) < argc)
			{
				for (j = i + 1; j < argc; j++)
				{
					if (!strcmp(argv[j], "-"))
					{
						/* -db - means add syslogd search (dbstore is NULL) */
						add_store(NULL, sflags);
					}
					else if (argv[j][0] == '-') 
					{
						if (j == (i + 1))
						{
							/* No databases: add /var/log/asl.db */
							add_store(_PATH_ASL_STORE, sflags);
							i = j - 1;
						}

						break;
					}
					else
					{
						add_store(argv[j], sflags);
					}
				}
			}
			else
			{
				/* No databases: add /var/log/asl.db */
				add_store(_PATH_ASL_STORE, sflags);
			}
		}
		else if (!strcmp(argv[i], "-w"))
		{
			watch = 1;
			tail_count = 10;
			if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
			{
				i++;
				tail_count = atoi(argv[i]);
			}
		}
		else if (!strcmp(argv[i], "-u"))
		{
			tflags = TIME_UTC;
			user_tflag = 1;
		}
		else if (!strcmp(argv[i], "-x"))
		{
			if ((i + 1) >= argc)
			{
				aslresponse_free(qlist);
				usage();
				exit(1);
			}

			exportname = argv[++i];
		}
		else if (!strcmp(argv[i], "-f"))
		{
			if ((i + 1) >= argc)
			{
				aslresponse_free(qlist);
				usage();
				exit(1);
			}

			logname = argv[++i];
		}
		else if (!strcmp(argv[i], "-F"))
		{
			if ((i + 1) >= argc)
			{
				aslresponse_free(qlist);
				usage();
				exit(1);
			}

			i++;

			if (!strcmp(argv[i], "raw"))
			{
				pflags = FORMAT_RAW;
				if (user_tflag == 0) tflags = TIME_SEC;
			}
			else if (!strcmp(argv[i], "std"))
			{
				pflags = FORMAT_STD;
			}
			else if (!strcmp(argv[i], "bsd"))
			{
				pflags = FORMAT_LEGACY;
			}
			else if (!strcmp(argv[i], "xml"))
			{
				pflags = FORMAT_XML;
			}
			else 
			{
				pflags = 0;
				pfmt = argv[i];
			}
		}
		else if (!strcmp(argv[i], "-T"))
		{
			if ((i + 1) >= argc)
			{
				aslresponse_free(qlist);
				usage();
				exit(1);
			}

			i++;
			user_tflag = 1;

			if (!strcmp(argv[i], "sec")) tflags = TIME_SEC;
			else if (!strcmp(argv[i], "utc")) tflags = TIME_UTC;
			else if (!strcmp(argv[i], "local")) tflags = TIME_LCL;
			else if (!strcmp(argv[i], "lcl")) tflags = TIME_LCL;
			else  tflags = TIME_LCL;
		}
		else if (!strcmp(argv[i], "-o"))
		{
			flags = 0;

			if (qlist->count == 0)
			{
				qlist->msg = (asl_msg_t **)calloc(1, sizeof(asl_msg_t *));
			}
			else 
			{
				qlist->msg = (asl_msg_t **)reallocf(qlist->msg, (qlist->count + 1) * sizeof(asl_msg_t *));
			}

			if (qlist->msg == NULL) exit(1);

			cq = asl_new(ASL_TYPE_QUERY);
			qlist->msg[qlist->count] = cq;
			qlist->count++;
		}
		else if (!strcmp(argv[i], "-n"))
		{
			flags = ASL_QUERY_OP_NOT;
		}
		else if (!strcmp(argv[i], "-C"))
		{
			if (qlist->count == 0)
			{
				qlist->msg = (asl_msg_t **)calloc(1, sizeof(asl_msg_t *));
				if (qlist->msg == NULL) exit(1);

				cq = asl_new(ASL_TYPE_QUERY);
				qlist->msg[qlist->count] = cq;
				qlist->count++;
			}

			status = add_op(cq, ASL_KEY_FACILITY, OP_EQ, FACILITY_CONSOLE, flags);

			flags = 0;
			if (status != 0)
			{
				aslresponse_free(qlist);
				exit(1);
			}
		}
		else if (!strcmp(argv[i], "-k"))
		{
			i++;
			for (n = i; n < argc; n++)
			{
				if (!strcmp(argv[n], "-o")) break;
				if (!strcmp(argv[n], "-n")) break;
				if (!strcmp(argv[n], "-k")) break;
				if ((n - i) > 2)
				{
					fprintf(stderr, "invalid sequence: -k");
					for (j = i; j <= n; j++) fprintf(stderr, " %s", argv[j]);
					fprintf(stderr, "\n");
					usage();
					exit(1);
				}
			}

			n -= i;
			if (n == 0)
			{
				i--;
				continue;
			}

			if (qlist->count == 0)
			{
				qlist->msg = (asl_msg_t **)calloc(1, sizeof(asl_msg_t *));
				if (qlist->msg == NULL) exit(1);

				cq = asl_new(ASL_TYPE_QUERY);
				qlist->msg[qlist->count] = cq;
				qlist->count++;
			}

			status = 0;
			if (n == 1) status = add_op(cq, argv[i], NULL, NULL, flags);
			else if (n == 2) status = add_op(cq, argv[i], OP_EQ, argv[i+1], flags);
			else status = add_op(cq, argv[i], argv[i+1], argv[i+2], flags);

			flags = 0;
			if (status != 0)
			{
				aslresponse_free(qlist);
				exit(1);
			}
		}
	}

	pflags |= tflags;

	if (store_count == 0) add_store(NULL, sflags);

	kstatus = bootstrap_look_up(bootstrap_port, ASL_SERVICE_NAME, &asl_server_port);
	if (kstatus != KERN_SUCCESS)
	{
		if (prune == 1)
		{
			fprintf(stderr, "prune operation failed: can't contact syslogd server\n");
			exit(1);
		}

		if (iamroot == 0)
		{
			fprintf(stderr, "operation failed: can't contact syslogd server\n");
			exit(1);
		}

		/* force raw access (for single-user mode when syslogd is not running) */
		if (store_raw == 0)
		{
			fprintf(stderr, "*** can't contact syslogd server - using read-only database access ***\n");
			add_store(_PATH_ASL_STORE, ASL_STORE_FLAG_READ_ONLY);
			store_raw = 1;
		}
	}

	if (prune == 1)
	{
		if (watch == 1)
		{
			fprintf(stderr, "Warning: -w flag has no effect when pruning\n");
			watch = 0;
		}

		if (qlist->count == 0)
		{
			fprintf(stderr, "no queries for pruning\n");

			aslresponse_free(qlist);
			for (j = 0; j < store_count; j++) asl_store_close(dbstore[j]);
			if (dbstore != NULL) free(dbstore);

			exit(0);
		}

		for (i = 0; i < store_count; i++)
		{
			status = ASL_STATUS_OK;

			if ((dbstore[i] == NULL) && (store_raw == 0))
			{
				if (iamroot == 0)
				{
					fprintf(stderr, "you must be root to prune the log database\n");

					aslresponse_free(qlist);
					for (j = 0; j < store_count; j++) asl_store_close(dbstore[j]);
					if (dbstore != NULL) free(dbstore);

					exit(1);
				}

				status = send_prune(qlist);
			}
			else
			{
				status = asl_store_prune(dbstore[i], qlist);
			}

			if (status != ASL_STATUS_OK) 
			{
				fprintf(stderr, "database prune failed: %s\n", asl_store_error(status));

				aslresponse_free(qlist);
				for (j = 0; j < store_count; j++) asl_store_close(dbstore[j]);
				if (dbstore != NULL) free(dbstore);

				exit(1);
			}
		}

		aslresponse_free(qlist);

		exit(0);
	}

	outfile = stdout;

	if (exportname != NULL)
	{
		if (watch == 1)
		{
			fprintf(stderr, "Warning: -w flag has no effect with -x export flag\n");
			watch = 0;
		}

		status = asl_store_open(exportname, 0, &export);
		if (status != ASL_STATUS_OK) 
		{
			aslresponse_free(qlist);
			fprintf(stderr, "export database open failed: %s\n", asl_store_error(status));
			exit(1);
		}

		outfile = NULL;
		pflags = EXPORT;
	}

	qmin = 0;
	cmax = 0;
	notify_file = -1;
	notify_token = -1;

	if (watch == 1)
	{
		if (store_raw == 1)
		{
			fprintf(stderr, "Warning: -w flag can only be used to watch syslogd's active database\n");
			watch = 0;
		}
		else if (store_count > 1)
		{
			fprintf(stderr, "Warning: -w flag has no effect with multiple databases\n");
			watch = 0;
		}
		else
		{
			status = notify_register_file_descriptor("com.apple.system.logger.message", &notify_file, 0, &notify_token);
			if (status != NOTIFY_STATUS_OK) notify_token = -1;
		}
	}

	if ((qlist->count == 0) && (watch == 1) && (store_raw == 0))
	{
		lx = (asl_search_result_t *)calloc(1, sizeof(asl_search_result_t));
		if (lx == NULL) exit(1);

		lx->count = 1;
		lx->msg = (asl_msg_t **)calloc(1, sizeof(asl_msg_t *));
		if (lx->msg == NULL)
		{
			aslresponse_free(lx);
			exit(1);
		}

		lx->msg[0] = asl_new(ASL_TYPE_QUERY);
		if (lx->msg[0] == NULL)
		{
			aslresponse_free(lx);
			exit(1);
		}

		asl_set_query(lx->msg[0], "Level", "0", ASL_QUERY_OP_NUMERIC | ASL_QUERY_OP_GREATER_EQUAL);
		qmin = -1;
		res = send_query(lx, qmin, 1, 1, &cmax);
		aslresponse_free(lx);
		aslresponse_free(res);
		qmin = cmax - tail_count;
		tail_count = 0;
	}

	if (qlist->count == 0)
	{
		qlist->msg = (asl_msg_t **)calloc(1, sizeof(asl_msg_t *));
		if (qlist->msg == NULL) exit(1);

		cq = asl_new(ASL_TYPE_QUERY);
		qlist->msg[qlist->count] = cq;
		qlist->count++;
	}

	search_once(outfile, pfmt, pflags, qlist, qmin, &cmax, batch, tail_count);

	if (watch == 1)
	{
		if (notify_token == -1)
		{
			forever
			{
				usleep(500000);
				if (cmax > qmin) qmin = cmax;
				search_once(outfile, pfmt, pflags, qlist, qmin, &cmax, 0, 0);
			}
		}
		else
		{
			while (read(notify_file, &i, 4) == 4)
			{
				if (cmax > qmin) qmin = cmax;
				search_once(outfile, pfmt, pflags, qlist, qmin, &cmax, 0, 0);
			}
		}
	}

	for (i = 0; i < store_count; i++) asl_store_close(dbstore[i]);
	if (dbstore != NULL) free(dbstore);
	if (export != NULL) asl_store_close(export);

	aslresponse_free(qlist);

	exit(0);
}
