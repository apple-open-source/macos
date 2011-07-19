/*
 * Copyright (c) 2007-2010 Apple Inc. All rights reserved.
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
#include <dirent.h>
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
#include <asl_msg.h>
#include <asl_private.h>
#include "asl_ipc.h"
#include <asl_core.h>
#include <asl_store.h>
#include <asl_file.h>

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

#define QUERY_FLAG_SEARCH_REVERSE 0x00000001

#define FACILITY_CONSOLE "com.apple.console"

/* Shared with Libc */
#define NOTIFY_RC "com.apple.asl.remote"

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

#define TIME_SEC		0x00000010
#define TIME_UTC		0x00000020
#define TIME_LCL		0x00000040

#define FORMAT_RAW		0x00000100
#define FORMAT_LEGACY	0x00000200
#define FORMAT_STD		0x00000400
#define FORMAT_XML		0x00000800
#define COMPRESS_DUPS   0x00010000

#define EXPORT			0x00000100

#define ASL_FILTER_MASK_PACEWNID 0xff
#define ASL_FILTER_MASK_PACEWNI  0x7f
#define ASL_FILTER_MASK_PACEWN   0x3f
#define ASL_FILTER_MASK_PACEW    0x1f
#define ASL_FILTER_MASK_PACE     0x0f
#define ASL_FILTER_MASK_PAC      0x07

#define FETCH_BATCH	1024
#define MAX_RANDOM 8192

#define DB_SELECT_ASL     0
#define DB_SELECT_STORE   1
#define DB_SELECT_FILES   2
#define DB_SELECT_SYSLOGD 3
#define DB_SELECT_LEGACY  4

/* STD and BSD format messages start with 'DAY MMM DD HH:MM:SS ' timestamp */
#define STD_BSD_DATE_LEN 20

/* Max message size for direct watch */
#define MAX_DIRECT_SIZE 16384

/* Buffer for direct watch data */
#define DIRECT_BUF_SIZE 1024

static asl_file_list_t *db_files = NULL;
static asl_store_t *store = NULL;
static asl_file_t *legacy = NULL;
static asl_file_t *export = NULL;
static const char *sort_key = NULL;
static const char *sort_key_2 = NULL;
static int sort_numeric = 0;
static char *last_printmsg_str = NULL;
static int last_printmsg_count = 0;

#ifdef CONFIG_IPHONE
static uint32_t dbselect = DB_SELECT_SYSLOGD;
#else
static uint32_t dbselect = DB_SELECT_ASL;
#endif

/* notify SPI */
uint32_t notify_register_plain(const char *name, int *out_token);

extern asl_msg_t *asl_msg_from_string(const char *buf);
extern char *asl_list_to_string(asl_search_result_t *list, uint32_t *outlen);
extern asl_search_result_t *asl_list_from_string(const char *buf);
extern int asl_msg_cmp(asl_msg_t *a, asl_msg_t *b);
extern time_t asl_parse_time(const char *in);
/* END PRIVATE API */

#define ASL_SERVICE_NAME "com.apple.system.logger"
static mach_port_t asl_server_port = MACH_PORT_NULL;

static const char *myname = "syslog";

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
	fprintf(stderr, "%s [-f file...] [-d path...] [-x file] [-w [N]] [-F format] [-nocompress] [-u] [-sort key1 [key2]] [-nsort key1 [key2]] [-k key [[op] val]]... [-o -k key [[op] val]] ...]...\n", myname);
	fprintf(stderr, "   -f     read named file[s], rather than standard log message store.\n");
	fprintf(stderr, "   -d     read all file in named directory path, rather than standard log message store.\n");
	fprintf(stderr, "   -x     export to named ASL format file, rather than printing\n");
	fprintf(stderr, "   -w     watch data store (^C to quit)\n");
	fprintf(stderr, "          prints the last N matching lines (default 10) before waiting\n");
	fprintf(stderr, "          \"-w all\" prints all matching lines before waiting\n");
	fprintf(stderr, "          \"-w boot\" prints all matching lines since last system boot before waiting\n");
	fprintf(stderr, "   -F     output format may be \"std\", \"raw\", \"bsd\", or \"xml\"\n");
	fprintf(stderr, "          format may also be a string containing variables of the form\n");
	fprintf(stderr, "          $Key or $(Key) - use the latter for non-whitespace delimited variables\n");
	fprintf(stderr, "   -T     timestamp format may be \"sec\" (seconds), \"utc\" (UTC), or \"local\" (local timezone)\n");
	fprintf(stderr, "   -E     text encoding may be \"vis\", \"safe\", or \"none\"\n");
	fprintf(stderr, "   -nodc  no duplicate message compression\n");
	fprintf(stderr, "   -u     print timestamps using UTC (equivalent to \"-T utc\")\n");
	fprintf(stderr, "   -sort  sort messages using value for specified key1 (secondary sort by key2 if provided)\n");
	fprintf(stderr, "   -nsort numeric sort messages using value for specified key1 (secondary sort by key2 if provided)\n");
	fprintf(stderr, "   -k     key/value match\n");
	fprintf(stderr, "          if no operator or value is given, checks for the existance of the key\n");
	fprintf(stderr, "          if no operator is given, default is \"%s\"\n", OP_EQ);
	fprintf(stderr, "   -B     only process log messages since last system boot\n");
	fprintf(stderr, "   -C     alias for \"-k Facility com.apple.console\"\n");
	fprintf(stderr, "   -o     begins a new query\n");
	fprintf(stderr, "          queries are \'OR\'ed together\n");
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
rcontrol_get_string(const char *name, int *val)
{
	int t, status;
	uint64_t x;

	status = notify_register_plain(name, &t);
	if (status != NOTIFY_STATUS_OK) return status;

	x = 0;
	status = notify_get_state(t, &x);
	notify_cancel(t);

	*val = x;

	return status;
}

int
rcontrol_set_string(const char *name, int filter)
{
	int t, status;
	uint64_t x;

	status = notify_register_plain(name, &t);
	if (status != NOTIFY_STATUS_OK) return status;

	x = filter;
	status = notify_set_state(t, x);
	notify_post(NOTIFY_RC);
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

const char *
rcontrol_name(pid_t pid, uid_t uid)
{
	static char str[1024];

	if (pid == RC_SYSLOGD) return NOTIFY_SYSTEM_ASL_FILTER;
	if (pid == RC_MASTER) return NOTIFY_SYSTEM_MASTER;

	memset(str, 0, sizeof(str));
	if (uid == 0) snprintf(str, sizeof(str) - 1, "%s.%d", NOTIFY_PREFIX_SYSTEM, pid);
	else snprintf(str, sizeof(str) - 1, "user.uid.%d.syslog.%d", uid, pid);
	return str;
}

int
rcontrol_get(pid_t pid, uid_t uid)
{
	int filter, status;
	const char *name;

	filter = 0;

	if (pid < 0)
	{
		name = "Master";
		if (pid == RC_SYSLOGD) name = "ASL Data Store";

		status = rcontrol_get_string(rcontrol_name(pid, uid), &filter);
		if (status == NOTIFY_STATUS_OK)
		{
			printf("%s filter mask: %s\n", name, asl_filter_string(filter));
			return 0;
		}

		printf("Unable to determine %s filter mask\n", name);
		return -1;
	}

	status = rcontrol_get_string(rcontrol_name(pid, uid), &filter);
	if (status == NOTIFY_STATUS_OK)
	{
		printf("Process %d syslog filter mask: %s\n", pid, asl_filter_string(filter));
		return 0;
	}

	printf("Unable to determine syslog filter mask for pid %d\n", pid);
	return -1;
}

int
rcontrol_set(pid_t pid, uid_t uid, int filter)
{
	int status;
	const char *name, *rcname;

	rcname = rcontrol_name(pid, uid);

	if (pid < 0)
	{
		name = "Master";
		if (pid == RC_SYSLOGD) name = "ASL Data Store";
		status = rcontrol_set_string(rcname, filter);

		if (status == NOTIFY_STATUS_OK)
		{
			if (pid == RC_MASTER) status = notify_post(NOTIFY_SYSTEM_MASTER);
			return 0;
		}

		printf("Unable to set %s syslog filter mask: %s\n", name, notify_status_string(status));
		return -1;
	}

	status = rcontrol_set_string(rcname, filter);
	if (status == NOTIFY_STATUS_OK)
	{
		status = notify_post(rcname);
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


	tick = time(NULL);
	timestr = NULL;
	asprintf(&timestr, "%lu", tick);
	if (timestr != NULL)
	{
		asl_set(msg, ASL_KEY_TIME, timestr);
		free(timestr);
	}

	if (gethostname(myname, MAXHOSTNAMELEN) == 0) asl_set(msg, ASL_KEY_HOST, myname);

	len = 0;
	str = asl_msg_to_string((asl_msg_t *)msg, &len);
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

const char *
asl_string_to_char_level(const char *s)
{
	if (s == NULL) return NULL;

	if ((s[0] >= '0') && (s[0] <= '7') && (s[1] == '\0')) return s;

	if (!strncasecmp(s, "em", 2)) return "0";
	else if (!strncasecmp(s, "p",  1)) return "0";
	else if (!strncasecmp(s, "a",  1)) return "1";
	else if (!strncasecmp(s, "c",  1)) return "2";
	else if (!strncasecmp(s, "er", 2)) return "3";
	else if (!strncasecmp(s, "x",  1)) return "3";
	else if (!strncasecmp(s, "w",  1)) return "4";
	else if (!strncasecmp(s, "n",  1)) return "5";
	else if (!strncasecmp(s, "i",  1)) return "6";
	else if (!strncasecmp(s, "d",  1)) return "7";

	return NULL;
}

int
syslog_remote_control(int argc, char *argv[])
{
	int pid, uid, status, mask;

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

		rcontrol_set(pid, uid, mask);
	}
	else
	{
		rcontrol_get(pid, uid);
	}

	return 0;
}

int
syslog_send(int argc, char *argv[])
{
	int i, start, kv, len, rfmt, rlevel, filter, status;
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
		for (i = start + 1; i < argc; i += 2)
		{
			if (!strcmp(argv[i], "-k")) i++;
			asl_set(m, argv[i], argv[i + 1]);
			if (!strcmp(argv[i], ASL_KEY_LEVEL)) rlevel = atoi(argv[i + 1]);
		}
	}

	if (rhost == NULL)
	{
		filter = 0;
		status = rcontrol_get_string(rcontrol_name(RC_SYSLOGD, 0), &filter);
		if (status != 0)
		{
			fprintf(stderr, "Warning: Can't get current syslogd ASL filter value\n");
		}
		else if ((ASL_FILTER_MASK(rlevel) & filter) == 0)
		{
			fprintf(stderr, "Warning: The current syslogd ASL filter value (%s)\n", asl_filter_string(filter));
			fprintf(stderr, "         will exclude this message from the ASL database\n");
		}

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
printmsg(FILE *f, aslmsg msg, char *fmt, int pflags)
{
	char *str;
	const char *mf, *tf;
	uint32_t encode, len, status;
	uint64_t xid;

	if (f == NULL)
	{
		if (export != NULL)
		{
			xid = 0;
			status = asl_file_save(export, msg, &xid);
			if (status != ASL_STATUS_OK)
			{
				fprintf(stderr, "export file write failed: %s\n", asl_core_error(status));
				asl_file_close(export);
				export = NULL;
			}
		}

		return;
	}

	encode = pflags & 0x0000000f;

	mf = ASL_MSG_FMT_RAW;
	if (fmt != NULL) mf = (const char *)fmt;
	else if (pflags & FORMAT_STD) mf = ASL_MSG_FMT_STD;
	else if (pflags & FORMAT_LEGACY) mf = ASL_MSG_FMT_BSD;
	else if (pflags & FORMAT_XML) mf = ASL_MSG_FMT_XML;

	tf = ASL_TIME_FMT_SEC;
	if (pflags & TIME_UTC) tf = ASL_TIME_FMT_UTC;
	if (pflags & TIME_LCL) tf = ASL_TIME_FMT_LCL;

	len = 0;
	str = asl_format_message((asl_msg_t *)msg, mf, tf, encode, &len);
	if (str == NULL) return;

	if (pflags & COMPRESS_DUPS)
	{
		if (last_printmsg_str != NULL) 
		{
			if (!strcmp(str + STD_BSD_DATE_LEN, last_printmsg_str + STD_BSD_DATE_LEN))
			{
				last_printmsg_count++;
				free(str);
			}
			else
			{
				if (last_printmsg_count > 0)
				{
					fprintf(f, "--- last message repeated %d time%s ---\n", last_printmsg_count, (last_printmsg_count == 1) ? "" : "s");
				}

				free(last_printmsg_str);
				last_printmsg_str = str;
				last_printmsg_count = 0;

				fprintf(f, "%s", str);
			}
		}
		else
		{
			last_printmsg_str = str;
			last_printmsg_count = 0;

			fprintf(f, "%s", str);
		}
	}
	else
	{
		fprintf(f, "%s", str);
		free(str);
	}
}

asl_search_result_t *
store_query(asl_search_result_t *q, uint64_t start, int count, int dir, uint64_t *last)
{
	uint32_t status;
	asl_search_result_t *res;

	if (store == NULL)
	{
		status = asl_store_open_read(NULL, &store);
		if (status != 0) return NULL;
	}

	res = NULL;
	status = asl_store_match(store, q, &res, last, start, count, dir);
	if (status != 0) return NULL;

	return res;
}

asl_search_result_t *
file_query(asl_search_result_t *q, uint64_t start, int count, int dir, uint64_t *last)
{
	uint32_t status;
	asl_search_result_t *res;

	res = NULL;
	status = asl_file_list_match(db_files, q, &res, last, start, count, dir);
	if (status != 0) return NULL;

	return res;
}

asl_search_result_t *
legacy_query(asl_search_result_t *q, uint64_t start, int count, int dir, uint64_t *last)
{
	uint32_t status;
	asl_search_result_t *res;

	res = NULL;
	status = asl_file_match(legacy, q, &res, last, start, count, dir);
	if (status != 0) return NULL;

	return res;
}

asl_search_result_t *
syslogd_query(asl_search_result_t *q, uint64_t start, int count, int dir, uint64_t *last)
{
	char *str, *res;
	caddr_t vmstr;
	uint32_t len, reslen, status;
	int flags;
	kern_return_t kstatus;
	security_token_t sec;
	asl_search_result_t *l;

	if (asl_server_port == MACH_PORT_NULL)
	{
		kstatus = bootstrap_look_up(bootstrap_port, ASL_SERVICE_NAME, &asl_server_port);
		if (kstatus != KERN_SUCCESS)
		{
			fprintf(stderr, "query failed: can't contact syslogd\n");
			return NULL;
		}
	}

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
	flags = 0;
	if (dir < 0) flags = QUERY_FLAG_SEARCH_REVERSE;

	kstatus = _asl_server_query(asl_server_port, (caddr_t)vmstr, len, start, count, flags, (caddr_t *)&res, &reslen, last, (int *)&status, &sec);

	if (res == NULL) return NULL;
	l = asl_list_from_string(res);
	vm_deallocate(mach_task_self(), (vm_address_t)res, reslen);
	return l;
}

void
filter_and_print(aslmsg msg, asl_search_result_t *ql, FILE *f, char *pfmt, int pflags)
{
	int i, do_match, did_match;

	if (msg == NULL) return;

	do_match = 1;
	if (ql == NULL) do_match = 0;
	else if (ql->count == 0) do_match = 0;

	did_match = 1;

	if (do_match != 0)
	{
		did_match = 0;

		for (i = 0; (i < ql->count) && (did_match == 0); i++)
		{
			did_match = asl_msg_cmp(ql->msg[i], (asl_msg_t *)msg);
		}
	}

	if (did_match != 0) printmsg(f, msg, pfmt, pflags);
}

void
syslogd_direct_watch(FILE *f, char *pfmt, int pflags, asl_search_result_t *ql)
{
	struct sockaddr_in address;
	int i, bytes, n, inlen, sock, stream, status;
	uint16_t port;
	socklen_t addresslength;
	char *str, buf[DIRECT_BUF_SIZE];
	aslmsg msg;

	if (asl_server_port == MACH_PORT_NULL)
	{
		status = bootstrap_look_up(bootstrap_port, ASL_SERVICE_NAME, &asl_server_port);
		if (status != KERN_SUCCESS)
		{
			fprintf(stderr, "query failed: can't contact syslogd\n");
			exit(1);
		}
	}

reset:

	addresslength = sizeof(address);
	sock = socket(AF_INET, SOCK_STREAM, 0);
	port = (arc4random() % (IPPORT_HILASTAUTO - IPPORT_HIFIRSTAUTO)) + IPPORT_HIFIRSTAUTO;

	address.sin_addr.s_addr = 0;
	address.sin_family = AF_INET;
	address.sin_port = htons(port);

	status = bind(sock, (struct sockaddr *)&address, sizeof(address));

	for (i = 0; (i < MAX_RANDOM) && (status < 0); i++)
	{
		port = (arc4random() % (IPPORT_HILASTAUTO - IPPORT_HIFIRSTAUTO)) + IPPORT_HIFIRSTAUTO;
		address.sin_port = htons(port);

		status = bind(sock, (struct sockaddr *)&address, sizeof(address));
	}

	if (status < 0)
	{
		fprintf(stderr, "query failed: can't find a port to connect to syslogd\n");
		exit(1);
	}

	bytes = 0;

	if (listen(sock, 1) == -1)
	{
		perror("listen");
		exit(1);
	}

	i = htons(port);
	_asl_server_register_direct_watch(asl_server_port, i);

	stream = accept(sock, (struct sockaddr*)&address, &addresslength);
	if (stream == -1)
	{
		perror("accept");
		exit(1);
	}

	forever
	{
		inlen = 0;
		bytes = recvfrom(stream, &n, sizeof(n), 0, NULL, NULL);
		if (bytes == 0) break;
		else inlen = ntohl(n);

		if (inlen == 0) continue;
		if (inlen > MAX_DIRECT_SIZE)
		{
			fprintf(f, "\n*** received invalid message data [%d] from syslogd - resetting connection ***\n\n", inlen);
			close(stream);
			close(sock);
			goto reset;
		}

		str = NULL;
		if (inlen <= DIRECT_BUF_SIZE)
		{
			str = buf;
		}
		else
		{
			str = calloc(1, inlen + 1);
			if (str == NULL)
			{
				fprintf(stderr, "\ncan't allocate memory - exiting\n");
				close(stream);
				close(sock);
				exit(1);
			}
		}

		n = 0;
		while (n < inlen)
		{
			bytes = recvfrom(stream, str + n, inlen - n, 0, NULL, NULL);
			if (bytes == 0) break;
			else n += bytes;
		}

		msg = (aslmsg)asl_msg_from_string(str);
		if (str != buf) free(str);
		filter_and_print(msg, ql, f, pfmt, pflags);
		asl_free(msg);
	}

	close(stream);
	close(sock);

	address.sin_addr.s_addr = 0;
}

int
sort_compare_key(const void *a, const void *b, const char *key)
{
	aslmsg *ma, *mb;
	const char *va, *vb;
	uint64_t na, nb;

	if (key == NULL) return 0;

	ma = (aslmsg *)a;
	mb = (aslmsg *)b;

	va = asl_get(*ma, key);
	vb = asl_get(*mb, key);

	if (va == NULL) return -1;
	if (vb == NULL) return 1;

	if (sort_numeric == 1)
	{
		na = atoll(va);
		nb = atoll(vb);
		if (na < nb) return -1;
		if (na > nb) return 1;
		return 0;
	}

	return strcmp(va, vb);
}

int
sort_compare(const void *a, const void *b)
{
	int cmp;

	if (sort_key == NULL) return 0;

	cmp = sort_compare_key(a, b, sort_key);
	if ((cmp == 0) && (sort_key_2 != NULL)) cmp = sort_compare_key(a, b, sort_key_2);

	return cmp;
}

void
search_once(FILE *f, char *pfmt, int pflags, asl_search_result_t *ql, uint64_t qmin, uint64_t *cmax, uint32_t count, uint32_t batch, int dir, uint32_t tail)
{
	asl_search_result_t *res;
	int i, more, total;

	if (pflags & FORMAT_XML) print_xml_header(f);

	res = NULL;
	more = 1;
	total = 0;

	while (more == 1)
	{
		if (batch == 0) more = 0;

		if ((dbselect == DB_SELECT_ASL) || (dbselect == DB_SELECT_STORE)) res = store_query(ql, qmin, batch, dir, cmax);
		else if (dbselect == DB_SELECT_FILES) res = file_query(ql, qmin, batch, dir, cmax);
		else if (dbselect == DB_SELECT_SYSLOGD) res = syslogd_query(ql, qmin, batch, dir, cmax);
		else if (dbselect == DB_SELECT_LEGACY) res = legacy_query(ql, qmin, batch, dir, cmax);

		if ((dir >= 0) && (*cmax > qmin)) qmin = *cmax + 1;
		else if ((dir < 0) && (*cmax < qmin)) qmin = *cmax - 1;

		if (res == NULL)
		{
			more = 0;
		}
		else
		{
			if ((batch > 0) && (res->count < batch)) more = 0;
			total += res->count;
			if ((count > 0) && (total >= count)) more = 0;

			i = 0;
			if (tail != 0)
			{
				i = res->count - tail;
				tail = 0;
				if (i < 0) i = 0;
			}

			if (sort_key != NULL)
			{
				qsort(res->msg, res->count, sizeof(asl_msg_t *), sort_compare);
			}

			if ((f != NULL) || (export != NULL))
			{
				for (; i < res->count; i++) printmsg(f, (aslmsg)(res->msg[i]), pfmt, pflags);
			}

			aslresponse_free((aslresponse)res);
		}
	}

	if ((pflags & COMPRESS_DUPS) && (last_printmsg_count > 0))
	{
		fprintf(f, "--- last message repeated %d time%s ---\n", last_printmsg_count, (last_printmsg_count == 1) ? "" : "s");
		free(last_printmsg_str);
		last_printmsg_str = NULL;
		last_printmsg_count = 0;
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
	const char *qval;

	if (key == NULL) return -1;
	if (q == NULL) return -1;

	qval = NULL;
	if (strcmp(key, ASL_KEY_TIME) == 0)
	{
		qval = (const char *)val;
	}
	else if ((strcmp(key, ASL_KEY_LEVEL) == 0) && (_isanumber(val) == 0))
	{
		/* Convert level strings to numeric values */
		qval = asl_string_to_char_level(val);
		if (qval == NULL)
		{
			fprintf(stderr, "invalid value for \"Level\"key: %s\n", val);
			return -1;
		}
	}

	o = ASL_QUERY_OP_NULL;
	if (val == NULL) o = ASL_QUERY_OP_TRUE;

	if (op != NULL)
	{
		o = optype(op);
		if (o == ASL_QUERY_OP_NULL) return -1;
		if (val == NULL)
		{
			fprintf(stderr, "no value supplied for operator %s %s\n", key, op);
			return -1;
		}

		if ((qval == NULL) && (o & ASL_QUERY_OP_NUMERIC) && (_isanumber(val) == 0))
		{
			fprintf(stderr, "non-numeric value supplied for numeric operator %s %s %s\n", key, op, val);
			return -1;
		}
	}

	o |= flags;
	if (qval != NULL) asl_msg_set_key_val_op(q, key, qval, o);
	else asl_msg_set_key_val_op(q, key, val, o);

	return 0;
}

static uint32_t
add_db_file(const char *name)
{
	asl_file_t *s;
	uint32_t status;

	if (dbselect == DB_SELECT_LEGACY)
	{
		fprintf(stderr, "syslog can only read one legacy format database\n");
		fprintf(stderr, "can't combine legacy and non-legacy databases in a single search\n");
		exit(1);
	}

	/* shouldn't happen */
	if (name == NULL) return DB_SELECT_ASL;

	s = NULL;
	status = asl_file_open_read(name, &s);
	if (status != ASL_STATUS_OK)
	{
		fprintf(stderr, "data store file %s open failed: %s \n", name, asl_core_error(status));
		exit(1);
	}

	if (s == NULL)
	{
		fprintf(stderr, "data store file %s open failed\n", name);
		exit(1);
	}

	if (s->flags & ASL_FILE_FLAG_LEGACY_STORE)
	{
		if (db_files != NULL)
		{
			fprintf(stderr, "syslog can only read a single legacy format database\n");
			fprintf(stderr, "can't combine legacy and non-legacy databases in a single search\n");
			exit(1);
		}

		legacy = s;
		return DB_SELECT_LEGACY;
	}

	db_files = asl_file_list_add(db_files, s);
	return DB_SELECT_FILES;
}

static uint32_t
add_db_dir(const char *name)
{
	DIR *dp;
	struct dirent *dent;
	uint32_t status;
	asl_file_t *s;
	char *path;

	/* 
	 * Try opening as a data store
	 */
	status = asl_store_open_read(name, &store);
	if (status == 0)
	{
		if (name == NULL) return DB_SELECT_ASL;
		if (!strcmp(name, PATH_ASL_STORE)) return DB_SELECT_ASL;
		return DB_SELECT_STORE;
	}

	/*
	 * Open all readable files
	 */
	dp = opendir(name);
	if (dp == NULL)
	{
		fprintf(stderr, "%s: %s\n", name, strerror(errno));
		exit(1);
	}

	while ((dent = readdir(dp)) != NULL)
	{
		if (dent->d_name[0] == '.') continue;

		path = NULL;
		asprintf(&path, "%s/%s", name, dent->d_name);

		/* 
		 * asl_file_open_read will fail if path is NULL,
		 * if the file is not an ASL store file,
		 * or if it isn't readable.
		 */
		s = NULL;
		status = asl_file_open_read(path, &s);
		if (path != NULL) free(path);
		if ((status != ASL_STATUS_OK) || (s == NULL)) continue;

		db_files = asl_file_list_add(db_files, s);
	}

	closedir(dp);

	return DB_SELECT_FILES;
}

int
main(int argc, char *argv[])
{
	FILE *outfile;
	int i, j, n, watch, status, pflags, tflags, iamroot, user_tflag, export_preserve_id, saw_dash_d, since_boot;
	int notify_file, notify_token;
	asl_search_result_t *qlist;
	asl_msg_t *cq;
	char *pfmt;
	const char *exportname;
	uint32_t flags, tail_count, batch, encode;
	uint64_t qmin, cmax;

	watch = 0;
	iamroot = 0;
	user_tflag = 0;
	pfmt = NULL;
	flags = 0;
	tail_count = 0;
	batch = FETCH_BATCH;
	pflags = FORMAT_STD | COMPRESS_DUPS;
	tflags = TIME_LCL;
	encode = ASL_ENCODE_SAFE;
	cq = NULL;
	exportname = NULL;
	export_preserve_id = 0;
	saw_dash_d = 0;
	since_boot = 0;

	i = asl_store_location();
	if (i == ASL_STORE_LOCATION_MEMORY) dbselect = DB_SELECT_SYSLOGD;

	if (getuid() == 0) iamroot = 1;

	for (i = 1; i < argc; i++)
	{
		if ((!strcmp(argv[i], "-help")) || (!strcmp(argv[i], "--help")))
		{
			usage();
			exit(0);
		}

		if (!strcmp(argv[i], "-time"))
		{
			qmin = time(NULL);
			printf("%llu\n", qmin);
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
	}

	qlist = (asl_search_result_t *)calloc(1, sizeof(asl_search_result_t));
	if (qlist == NULL) exit(1);

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-f"))
		{
			if ((i + 1) < argc)
			{
				for (j = i + 1; j < argc; j++)
				{
					if (!strcmp(argv[j], "-"))
					{
						dbselect = DB_SELECT_SYSLOGD;
						i++;
						break;
					}
					else if (argv[j][0] == '-')
					{
						break;
					}
					else
					{
						dbselect = add_db_file(argv[j]);
						i++;
					}
				}
			}
		}
		else if (!strcmp(argv[i], "-d"))
		{
			saw_dash_d = i + 1;

			if (saw_dash_d < argc)
			{
				for (j = saw_dash_d; j < argc; j++)
				{
					if (!strcmp(argv[j], "store"))
					{
						dbselect = add_db_dir(PATH_ASL_STORE);
						i++;
					}
					else if (!strcmp(argv[j], "archive"))
					{
						dbselect = add_db_dir(PATH_ASL_ARCHIVE);
						i++;
					}
					else if (argv[j][0] == '-')
					{
						break;
					}
					else
					{
						dbselect = add_db_dir(argv[j]);
						i++;
					}
				}
			}
			else
			{
				fprintf(stderr, "missing directory name following -d flag\n");
				return -1;
			}
		}
		else if (!strcmp(argv[i], "-b"))
		{
			batch = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "-B"))
		{
			since_boot = 1;
		}
		else if (!strcmp(argv[i], "-w"))
		{
			watch = 1;
			tail_count = 10;
			if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
			{
				i++;
				if (!strcmp(argv[i], "all"))
				{
					tail_count = (uint32_t)-1;
				}
				else if (!strcmp(argv[i], "boot"))
				{
					since_boot = 1;
				}
				else
				{
					tail_count = atoi(argv[i]);
				}
			}
		}
		else if (!strcmp(argv[i], "-sort"))
		{
			if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
			{
				i++;
				sort_key = argv[i];

				if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
				{
					i++;
					sort_key_2 = argv[i];
				}
			}
			else
			{
				sort_key = ASL_KEY_MSG_ID;
			}

			batch = 0;
		}
		else if (!strcmp(argv[i], "-nsort"))
		{
			if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
			{
				i++;
				sort_key = argv[i];

				if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
				{
					i++;
					sort_key_2 = argv[i];
				}
			}
			else
			{
				sort_key = ASL_KEY_MSG_ID;
			}

			sort_numeric = 1;
			batch = 0;
		}
		else if (!strcmp(argv[i], "-u"))
		{
			tflags = TIME_UTC;
			user_tflag = 1;
		}
		else if ((!strcmp(argv[i], "-x")) || (!strcmp(argv[i], "-X")))
		{
			if ((i + 1) >= argc)
			{
				aslresponse_free(qlist);
				usage();
				exit(1);
			}

			if (!strcmp(argv[i], "-x")) export_preserve_id = 1;

			exportname = argv[++i];
		}
		else if (!strcmp(argv[i], "-E"))
		{
			if ((i + 1) >= argc)
			{
				aslresponse_free(qlist);
				usage();
				exit(1);
			}

			i++;

			if (!strcmp(argv[i], "vis")) encode = ASL_ENCODE_ASL;
			else if (!strcmp(argv[i], "safe")) encode = ASL_ENCODE_SAFE;
			else if (!strcmp(argv[i], "none")) encode = ASL_ENCODE_NONE;
			else if ((argv[i][0] >= '0') && (argv[i][0] <= '9') && (argv[i][1] == '\0')) encode = atoi(argv[i]);
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
				pflags = FORMAT_STD | COMPRESS_DUPS;
			}
			else if (!strcmp(argv[i], "bsd"))
			{
				pflags = FORMAT_LEGACY | COMPRESS_DUPS;
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
		else if (!strcmp(argv[i], "-nodc"))
		{
			pflags = pflags & ~COMPRESS_DUPS;
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

			cq = asl_msg_new(ASL_TYPE_QUERY);
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

				cq = asl_msg_new(ASL_TYPE_QUERY);
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

				cq = asl_msg_new(ASL_TYPE_QUERY);
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

			i += (n - 1);
		}
		else
		{
			fprintf(stderr, "syslog: unknown option \"%s\"\n", argv[i]);
			fprintf(stderr, "run \"syslog -help\" for usage\n");
			exit(1);
		}
	}

	pflags |= tflags;
	pflags |= encode;

	outfile = stdout;

	/*
	 * Catch and report some cases where watch (-w) doesn't work
	 */
	if (watch == 1)
	{
		if (sort_key != NULL)
		{
			fprintf(stderr, "Warning: -w flag has no effect with -sort flag\n");
			watch = 0;
		}

		if (dbselect == DB_SELECT_FILES)
		{
			if (saw_dash_d == 0)
			{
				fprintf(stderr, "Warning: -w flag not supported for a set of one or more files\n");
			}
			else
			{
				fprintf(stderr, "Warning: directory \"%s\" is not an ASL data store\n", argv[saw_dash_d]);
				fprintf(stderr, "         -w flag not supported for a set of one or more files\n");
			}

			watch = 0;
		}
	}

	if (exportname != NULL)
	{
		if (watch == 1)
		{
			fprintf(stderr, "Warning: -w flag has no effect with -x export flag\n");
			watch = 0;
		}

		status = asl_file_open_write(exportname, 0644, -1, -1, &export);
		if (status != ASL_STATUS_OK) 
		{
			aslresponse_free(qlist);
			fprintf(stderr, "export file open failed: %s\n", asl_core_error(status));
			exit(1);
		}

		/*
		 * allow the string cache to be unlimited to maximize string dup compression
		 * preserve message IDs
		 */
		export->flags = ASL_FILE_FLAG_UNLIMITED_CACHE;
		if (export_preserve_id != 0) export->flags |= ASL_FILE_FLAG_PRESERVE_MSG_ID;

		outfile = NULL;
		pflags = EXPORT;
	}

	qmin = 0;
	cmax = 0;
	notify_file = -1;
	notify_token = -1;

	/* set starting point */
	if (since_boot == 1)
	{
		/* search back for last "BOOT_TIME (ut_type == 2) record */
		asl_search_result_t *bt;
		aslmsg bq;

		bt = (asl_search_result_t *)calloc(1, sizeof(asl_search_result_t));
		if (bt == NULL)
		{
			fprintf(stderr, "\ncan't allocate memory - exiting\n");
			exit(1);
		}

		bt->msg = (asl_msg_t **)calloc(1, sizeof(asl_msg_t *));
		if (bt->msg == NULL)
		{
			fprintf(stderr, "\ncan't allocate memory - exiting\n");
			exit(1);
		}

		bq = asl_new(ASL_TYPE_QUERY);
		if (bq == NULL)
		{
			fprintf(stderr, "\ncan't allocate memory - exiting\n");
			exit(1);
		}

		asl_set_query(bq, "ut_type", "2", ASL_QUERY_OP_EQUAL);
		bt->count = 1;
		bt->msg[0] = (asl_msg_t *)bq;
	
		/* XXX */
		search_once(NULL, NULL, 0, bt, -1, &qmin, 1, 1, -1, 0);
		asl_free(bq);
		free(bt->msg);
		free(bt);
	
		if (qmin > 0) qmin--;
		tail_count = 0;
	}
	else if (watch == 1)
	{
		/* go back tail_count records from last record */
		qmin = -1;
		search_once(NULL, NULL, 0, qlist, qmin, &cmax, 1, 1, -1, 0);

		if (cmax >= tail_count) qmin = cmax - tail_count;
		else qmin = 0;

		tail_count = 0;
	}

	if ((watch == 1) && (dbselect == DB_SELECT_ASL))
	{
		status = notify_register_file_descriptor("com.apple.system.logger.message", &notify_file, 0, &notify_token);
		if (status != NOTIFY_STATUS_OK) notify_token = -1;
	}

	/* output should be line buffered */
	if (outfile != NULL) setlinebuf(outfile);

	search_once(outfile, pfmt, pflags, qlist, qmin + 1, &cmax, 0, 0, 1, tail_count);

	if (watch == 1)
	{
		if (dbselect == DB_SELECT_SYSLOGD)
		{
			syslogd_direct_watch(outfile, pfmt, pflags, qlist);
		}
		else if (notify_token == -1)
		{
			forever
			{
				usleep(500000);
				if (cmax > qmin) qmin = cmax;
				search_once(outfile, pfmt, pflags, qlist, qmin + 1, &cmax, 0, batch, 1, 0);
			}
		}
		else
		{
			while (read(notify_file, &i, 4) == 4)
			{
				if (cmax > qmin) qmin = cmax;
				search_once(outfile, pfmt, pflags, qlist, qmin + 1, &cmax, 0, batch, 1, 0);
			}
		}
	}

	if (db_files != NULL) asl_file_list_close(db_files);
	if (store != NULL) asl_store_close(store);
	if (export != NULL) asl_file_close(export);

	aslresponse_free(qlist);

	exit(0);
}
