#include <NetInfo/system_log.h>
#include <NetInfo/syslock.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifndef streq
#define streq(A, B) (strcmp(A, B) == 0)
#endif

static syslock *log_lock = NULL;
static FILE *log_fp = NULL;
static char *log_title = NULL;
static int log_facility = -1;
static int log_max_priority = LOG_NOTICE;

int
log_name_to_facility(char *name)
{
	if (name == NULL) return LOG_DAEMON;

	else if (streq(name, "LOG_KERN")) return LOG_KERN;
	else if (streq(name, "KERN")) return LOG_KERN;
	else if (streq(name, "log_kern")) return LOG_KERN;
	else if (streq(name, "kern")) return LOG_KERN;

	else if (streq(name, "LOG_USER")) return LOG_USER;
	else if (streq(name, "USER")) return LOG_USER;
	else if (streq(name, "log_user")) return LOG_USER;
	else if (streq(name, "user")) return LOG_USER;

	else if (streq(name, "LOG_MAIL")) return LOG_MAIL;
	else if (streq(name, "MAIL")) return LOG_MAIL;
	else if (streq(name, "log_mail")) return LOG_MAIL;
	else if (streq(name, "mail")) return LOG_MAIL;

	else if (streq(name, "LOG_DAEMON")) return LOG_DAEMON;
	else if (streq(name, "DAEMON")) return LOG_DAEMON;
	else if (streq(name, "log_daemon")) return LOG_DAEMON;
	else if (streq(name, "daemon")) return LOG_DAEMON;

	else if (streq(name, "LOG_AUTH")) return LOG_AUTH;
	else if (streq(name, "AUTH")) return LOG_AUTH;
	else if (streq(name, "log_auth")) return LOG_AUTH;
	else if (streq(name, "auth")) return LOG_AUTH;

	else if (streq(name, "LOG_SYSLOG")) return LOG_SYSLOG;
	else if (streq(name, "SYSLOG")) return LOG_SYSLOG;
	else if (streq(name, "log_syslog")) return LOG_SYSLOG;
	else if (streq(name, "syslog")) return LOG_SYSLOG;

	else if (streq(name, "LOG_LPR")) return LOG_LPR;
	else if (streq(name, "LPR")) return LOG_LPR;
	else if (streq(name, "log_lpr")) return LOG_LPR;
	else if (streq(name, "lpr")) return LOG_LPR;

	else if (streq(name, "LOG_NETINFO")) return LOG_NETINFO;
	else if (streq(name, "NETINFO")) return LOG_NETINFO;
	else if (streq(name, "log_netinfo")) return LOG_NETINFO;
	else if (streq(name, "netinfo")) return LOG_NETINFO;

	else if (streq(name, "LOG_LOCAL0")) return LOG_LOCAL0;
	else if (streq(name, "LOCAL0")) return LOG_LOCAL0;
	else if (streq(name, "log_local0")) return LOG_LOCAL0;
	else if (streq(name, "local0")) return LOG_LOCAL0;

	else if (streq(name, "LOG_LOCAL1")) return LOG_LOCAL1;
	else if (streq(name, "LOCAL1")) return LOG_LOCAL1;
	else if (streq(name, "log_local1")) return LOG_LOCAL1;
	else if (streq(name, "local1")) return LOG_LOCAL1;

	else if (streq(name, "LOG_LOCAL2")) return LOG_LOCAL2;
	else if (streq(name, "LOCAL2")) return LOG_LOCAL2;
	else if (streq(name, "log_local2")) return LOG_LOCAL2;
	else if (streq(name, "local2")) return LOG_LOCAL2;

	else if (streq(name, "LOG_LOCAL3")) return LOG_LOCAL3;
	else if (streq(name, "LOCAL3")) return LOG_LOCAL3;
	else if (streq(name, "log_local3")) return LOG_LOCAL3;
	else if (streq(name, "local3")) return LOG_LOCAL3;

	else if (streq(name, "LOG_LOCAL4")) return LOG_LOCAL4;
	else if (streq(name, "LOCAL4")) return LOG_LOCAL4;
	else if (streq(name, "log_local4")) return LOG_LOCAL4;
	else if (streq(name, "local4")) return LOG_LOCAL4;

	else if (streq(name, "LOG_LOCAL5")) return LOG_LOCAL5;
	else if (streq(name, "LOCAL5")) return LOG_LOCAL5;
	else if (streq(name, "log_local5")) return LOG_LOCAL5;
	else if (streq(name, "local5")) return LOG_LOCAL5;

	else if (streq(name, "LOG_LOCAL6")) return LOG_LOCAL6;
	else if (streq(name, "LOCAL6")) return LOG_LOCAL6;
	else if (streq(name, "log_local6")) return LOG_LOCAL6;
	else if (streq(name, "local6")) return LOG_LOCAL6;

	else if (streq(name, "LOG_LOCAL7")) return LOG_LOCAL7;
	else if (streq(name, "LOCAL7")) return LOG_LOCAL7;
	else if (streq(name, "log_local7")) return LOG_LOCAL7;
	else if (streq(name, "local7")) return LOG_LOCAL7;

	return LOG_DAEMON;
}

void
system_log_open(char *title, int options, int facility, FILE *fp)
{
	if (log_lock == NULL) log_lock = syslock_new(0);
	syslock_lock(log_lock);

	if (log_title != NULL) free(log_title);
	log_title = NULL;

	if (title != NULL)
	{
		log_title = malloc(strlen(title) + 1);
		strcpy(log_title, title);
	}

	log_facility = facility;
	if (facility > 0) openlog(log_title, options, facility);

	log_fp = fp;

	syslock_unlock(log_lock);
}

void
system_log_set_logfile(FILE *fp)
{
	if (log_lock == NULL) log_lock = syslock_new(0);
	syslock_lock(log_lock);
	log_fp = fp;
	syslock_unlock(log_lock);
}

void
system_log_set_max_priority(int p)
{
	if (log_lock == NULL) log_lock = syslock_new(0);
	syslock_lock(log_lock);
	log_max_priority = p;
	syslock_unlock(log_lock);
}

int
system_log_max_priority(void)
{
	return log_max_priority;
}

void
system_log(int priority, char *str, ...)
{
	va_list ap;
	char now[32];
	time_t t;

	if (priority > log_max_priority) return;

	if (log_lock == NULL) log_lock = syslock_new(0);
	syslock_lock(log_lock);

	va_start(ap, str);

	if (log_facility > 0) vsyslog(priority, str, ap);

	if (log_fp != NULL)
	{
		memset(now, 0, 32);
		time(&t);
		strftime(now, 32, "%b %e %T", localtime(&t));
		if (log_title != NULL) fprintf(log_fp, "%s %s: ", now, log_title);
		vfprintf(log_fp, str, ap);
		fprintf(log_fp, "\n");
		fflush(log_fp);
	}

	va_end(ap);

	syslock_unlock(log_lock);
}

void
system_log_close()
{
	if (log_lock == NULL) log_lock = syslock_new(0);
	syslock_lock(log_lock);

	if (log_title != NULL) free(log_title);
	log_title = NULL;

	if (log_facility > 0) closelog();
	log_facility = -1;

	log_fp = NULL;

	syslock_unlock(log_lock);
}
