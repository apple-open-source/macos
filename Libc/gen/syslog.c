/*
 * Copyright (c) 1999-2008 Apple Inc. All rights reserved.
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
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <netdb.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <notify.h>
#include <asl.h>
#include <asl_private.h>
#include <asl_ipc.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include <crt_externs.h>

#define	LOG_NO_NOTIFY	0x1000
#define	INTERNALLOG	LOG_ERR|LOG_CONS|LOG_PERROR|LOG_PID

#ifdef BUILDING_VARIANT
__private_extern__ int	_sl_LogStat;		/* status bits, set by openlog() */
__private_extern__ const char *_sl_LogTag;	/* string to tag the entry with */
__private_extern__ int	_sl_LogFacility;	/* default facility code */
__private_extern__ int	_sl_LogMask;		/* local mask of priorities to be logged */
__private_extern__ int	_sl_MasterLogMask;  /* master (remote control) mask of priorities to be logged */
__private_extern__ int	_sl_ProcLogMask;	/* process-specific (remote control) mask of priorities to be logged */
__private_extern__ int  _sl_RCToken;		/* for remote control change notification */
__private_extern__ int  _sl_NotifyToken;	/* for remote control of priority filter */
__private_extern__ int  _sl_NotifyMaster;	/* for remote control of priority filter */
__private_extern__ int  _sl_pid;			/* pid */
#else /* !BUILDING_VARIANT */
__private_extern__ int	_sl_LogStat = 0;			/* status bits, set by openlog() */
__private_extern__ const char *_sl_LogTag = NULL;	/* string to tag the entry with */
__private_extern__ int	_sl_LogFacility = LOG_USER;	/* default facility code */
__private_extern__ int	_sl_LogMask = 0xff;			/* mask of priorities to be logged */
__private_extern__ int	_sl_MasterLogMask = 0;		/* master mask of priorities to be logged */
__private_extern__ int	_sl_ProcLogMask = 0;		/* process-specific mask of priorities to be logged */
__private_extern__ int  _sl_RCToken = -1;			/* for remote control change notification */
__private_extern__ int  _sl_NotifyToken = -1;		/* for remote control of max logged priority */
__private_extern__ int  _sl_NotifyMaster = -1;		/* for remote control of max logged priority */
__private_extern__ int  _sl_pid = -1;				/* pid */
#endif /* BUILDING_VARIANT */

__private_extern__ void _sl_init_notify();

#define ASL_SERVICE_NAME "com.apple.system.logger"
static mach_port_t asl_server_port = MACH_PORT_NULL;

#define NOTIFY_SYSTEM_MASTER "com.apple.system.syslog.master"
#define NOTIFY_PREFIX_SYSTEM "com.apple.system.syslog"
#define NOTIFY_PREFIX_USER "user.syslog"
#define NOTIFY_STATE_OFFSET 1000

/* notify SPI */
uint32_t notify_register_plain(const char *name, int *out_token);
const char *asl_syslog_faciliy_num_to_name(int);

/*
 * syslog, vsyslog --
 *	print message on log file; output is intended for syslogd(8).
 */
void
#ifdef __STDC__
syslog(int pri, const char *fmt, ...)
#else
syslog(pri, fmt, va_alist)
	int pri;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vsyslog(pri, fmt, ap);
	va_end(ap);
}

void
vsyslog(int pri, const char *fmt, va_list ap)
{
	int status, i, saved_errno, filter, check, rc_filter;
	time_t tick;
	struct timeval tval;
	uint32_t elen, count, outlen;
	char *p, *str, *expanded, *err_str, hname[MAXHOSTNAMELEN+1];
	const char *val;
	uint64_t cval;
	int fd, mask, level, facility;
	aslmsg msg;
	kern_return_t kstatus;
	caddr_t out;

	saved_errno = errno;

	if (_sl_pid == -1) _sl_pid = getpid();

	/* Check for invalid bits. */
	if (pri & ~(LOG_PRIMASK | LOG_FACMASK))
	{
		syslog(INTERNALLOG, "syslog: unknown facility/priority: %x", pri);
		pri &= (LOG_PRIMASK | LOG_FACMASK);
	}

	level = LOG_PRI(pri);
	facility = pri & LOG_FACMASK;

	if (facility == 0) facility = _sl_LogFacility;

	_sl_init_notify();

	/* initialize or re-check process-specific and master filters  */
	if (_sl_RCToken >= 0) 
	{
		check = 0;
		status = notify_check(_sl_RCToken, &check);
		if ((status == NOTIFY_STATUS_OK) && (check != 0))
		{
			if (_sl_NotifyMaster >= 0)
			{
				cval = 0;
				if (notify_get_state(_sl_NotifyMaster, &cval) == NOTIFY_STATUS_OK) _sl_MasterLogMask = cval;
			}

			if (_sl_NotifyToken >= 0)
			{
				cval = 0;
				if (notify_get_state(_sl_NotifyToken, &cval) == NOTIFY_STATUS_OK) _sl_ProcLogMask = cval;
			}
		}
	}

	filter = _sl_LogMask;
	rc_filter = 0;

	/* master filter overrides local filter */
	if (_sl_MasterLogMask != 0)
	{
		filter = _sl_MasterLogMask;
		rc_filter = 1;
	}

	/* process-specific filter overrides local and master */
	if (_sl_ProcLogMask != 0)
	{
		filter = _sl_ProcLogMask;
		rc_filter = 1;
	}

	mask = LOG_MASK(level);
	if ((mask & filter) == 0) return;

	/* Build the message. */
	msg = asl_new(ASL_TYPE_MSG);

	if (_sl_LogTag == NULL) _sl_LogTag = *(*_NSGetArgv());
	if (_sl_LogTag != NULL) 
	{
		asl_set(msg, ASL_KEY_SENDER, _sl_LogTag);
	}

	str = (char *)asl_syslog_faciliy_num_to_name(facility);
	if (str != NULL) asl_set(msg, ASL_KEY_FACILITY, str);

	str = NULL;
	memset(&tval, 0, sizeof(struct timeval));

	status = gettimeofday(&tval, NULL);
	if (status == 0)
	{
		str = NULL;
		asprintf(&str, "%lu", tval.tv_sec);
		if (str != NULL)
		{
			asl_set(msg, ASL_KEY_TIME, str);
			free(str);
		}

		str = NULL;
		asprintf(&str, "%lu", tval.tv_usec * 1000);
		if (str != NULL)
		{
			asl_set(msg, ASL_KEY_TIME_NSEC, str);
			free(str);
		}
	}
	else
	{
		tick = time(NULL);
		str = NULL;
		asprintf(&str, "%lu", tick);
		if (str != NULL)
		{
			asl_set(msg, ASL_KEY_TIME, str);
			free(str);
		}
	}

	str = NULL;
	asprintf(&str, "%u", _sl_pid);
	if (str != NULL)
	{
		asl_set(msg, ASL_KEY_PID, str);
		free(str);
	}

	str = NULL;
	asprintf(&str, "%d", getuid());
	if (str != NULL)
	{
		asl_set(msg, ASL_KEY_UID, str);
		free(str);
	}

	str = NULL;
	asprintf(&str, "%u", getgid());
	if (str != NULL)
	{
		asl_set(msg, ASL_KEY_GID, str);
		free(str);
	}

	str = NULL;
	asprintf(&str, "%u", level);
	if (str != NULL)
	{
		asl_set(msg, ASL_KEY_LEVEL, str);
		free(str);
	}

	status = gethostname(hname, MAXHOSTNAMELEN);
	if (status < 0) asl_set(msg, ASL_KEY_HOST, "localhost");
	else asl_set(msg, ASL_KEY_HOST, hname);

	/* check for %m */
	count = 0;
	for (i = 0; fmt[i] != '\0'; i++)
	{
		if ((fmt[i] == '%') && (fmt[i+1] == 'm')) count++;
	}

	expanded = NULL;
	elen = 0;
	err_str = NULL;

	/* deal with malloc failures gracefully */
	if (count > 0)
	{
		err_str = strdup(strerror(saved_errno));
		if (err_str == NULL) count = 0;
		else
		{
			elen = strlen(err_str);
			expanded = malloc(i + (count * elen));
			if (expanded == NULL) count = 0;
		}
	}

	if (expanded == NULL) expanded = (char *)fmt;
	if (count > 0)
	{
		p = expanded;

		for (i = 0; fmt[i] != '\0'; i++)
		{
			if ((fmt[i] == '%') && (fmt[i+1] == 'm'))
			{
				memcpy(p, err_str, elen);
				p += elen;
				i++;
			}
			else
			{
				*p++ = fmt[i];
			}
		}

		*p = '\0';
	}

	if (err_str != NULL) free(err_str);

	vasprintf(&str, expanded, ap);
	if (count > 0) free(expanded);

	if (str != NULL)
	{
		asl_set(msg, ASL_KEY_MSG, str);

		/* Output to stderr if requested. */
		if (_sl_LogStat & LOG_PERROR)
		{
			p = NULL;
			if (_sl_LogStat & LOG_PID) asprintf(&p, "%s[%u]: %s", (_sl_LogTag == NULL) ? "???" : _sl_LogTag, _sl_pid, str);
			else asprintf(&p, "%s: %s", (_sl_LogTag == NULL) ? "???" : _sl_LogTag, str);

			if (p != NULL)
			{
				struct iovec iov[2];

				iov[0].iov_base = p;
				iov[0].iov_len = strlen(p);
				iov[1].iov_base = "\n";
				iov[1].iov_len = 1;
				writev(STDERR_FILENO, iov, 2);
				free(p);
			}
		}

		free(str);
	}

	/* Set "ASLOption store" if remote control is active */
	if (rc_filter != 0)
	{
		val = asl_get(msg, ASL_KEY_OPTION);
		if (val == NULL)
		{
			asl_set(msg, ASL_KEY_OPTION, ASL_OPT_STORE);
		}
		else
		{
			str = NULL;
			asprintf(&str, "%s %s", ASL_OPT_STORE, val);
			if (str != NULL)
			{
				asl_set(msg, ASL_KEY_OPTION, str);
				free(str);
				str = NULL;
			}
		}
	}

	/* send a mach message to syslogd */
	str = asl_format_message(msg, ASL_MSG_FMT_RAW, ASL_TIME_FMT_SEC, ASL_ENCODE_ASL, &count);
	if (str != NULL)
	{
		outlen = count + 11;
		kstatus = vm_allocate(mach_task_self(), (vm_address_t *)&out, outlen + 1, TRUE);
		if (kstatus == KERN_SUCCESS)
		{
			memset(out, 0, outlen + 1);
			snprintf((char *)out, outlen, "%10u %s", count, str);

			status = 0;
			if (asl_server_port == MACH_PORT_NULL) kstatus = bootstrap_look_up(bootstrap_port, ASL_SERVICE_NAME, &asl_server_port);

			if (kstatus == KERN_SUCCESS) kstatus = _asl_server_message(asl_server_port, (caddr_t)out, outlen + 1);
			else vm_deallocate(mach_task_self(), (vm_address_t)out, outlen + 1);

			if (kstatus == KERN_SUCCESS)
			{
				free(str);
				asl_free(msg);
				return;
			}
		}

		free(str);
	}

	/*
	 * Output the message to the console.
	 */
	if (_sl_LogStat & LOG_CONS && (fd = open(_PATH_CONSOLE, O_WRONLY | O_NOCTTY | O_NONBLOCK)) >= 0)
	{
		count = 0;

		p = asl_format_message(msg, ASL_MSG_FMT_STD, ASL_TIME_FMT_LCL, ASL_ENCODE_SAFE, &count);
		if (p != NULL)
		{
			struct iovec iov;

			/* count includes trailing nul */
			iov.iov_len = count - 1;
			iov.iov_base = p;
			writev(fd, &iov, 1);

			free(p);
		}

		close(fd);
	}

	asl_free(msg);
}

#ifndef BUILDING_VARIANT

__private_extern__ void
_syslog_fork_child()
{
	_sl_RCToken = -1;
	_sl_NotifyToken = -1;
	_sl_NotifyMaster = -1;

	asl_server_port = MACH_PORT_NULL;

	_sl_pid = getpid();
}

__private_extern__ void
_sl_init_notify()
{
	int status;
	char *notify_name;
	uint32_t euid;

	if (_sl_LogStat & LOG_NO_NOTIFY)
	{
		_sl_RCToken = -2;
		_sl_NotifyMaster = -2;
		_sl_NotifyToken = -2;
		return;
	}

	if (_sl_RCToken == -1)
	{
		status = notify_register_check(NOTIFY_RC, &_sl_RCToken);
		if (status != NOTIFY_STATUS_OK) _sl_RCToken = -2;
	}

	if (_sl_NotifyMaster == -1)
	{
		status = notify_register_plain(NOTIFY_SYSTEM_MASTER, &_sl_NotifyMaster);
		if (status != NOTIFY_STATUS_OK) _sl_NotifyMaster = -2;
	}

	if (_sl_NotifyToken == -1)
	{
		_sl_NotifyToken = -2;

		euid = geteuid();
		notify_name = NULL;
		if (euid == 0) asprintf(&notify_name, "%s.%d", NOTIFY_PREFIX_SYSTEM, getpid());
		else asprintf(&notify_name, "user.uid.%d.syslog.%d", euid, getpid());

		if (notify_name != NULL)
		{
			status = notify_register_plain(notify_name, &_sl_NotifyToken);
			free(notify_name);
			if (status != NOTIFY_STATUS_OK) _sl_NotifyToken = -2;
		}
	}
}

void
openlog(const char *ident, int logstat, int logfac)
{
	kern_return_t kstatus;

	if (ident != NULL) _sl_LogTag = ident;

	_sl_LogStat = logstat;

	if (logfac != 0 && (logfac &~ LOG_FACMASK) == 0) _sl_LogFacility = logfac;

	if (asl_server_port == MACH_PORT_NULL) 
	{
		kstatus = bootstrap_look_up(bootstrap_port, ASL_SERVICE_NAME, &asl_server_port);
	}

	_sl_pid = getpid();
	_sl_init_notify();
}

void
closelog()
{
	if (asl_server_port != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), asl_server_port);
	asl_server_port = MACH_PORT_NULL;

	if (_sl_NotifyToken != -1) notify_cancel(_sl_NotifyToken);
	_sl_NotifyToken = -1;

	if (_sl_NotifyMaster != -1) notify_cancel(_sl_NotifyMaster);
	_sl_NotifyMaster = -1;
}

/* setlogmask -- set the log mask level */
int
setlogmask(int pmask)
{
	int omask;

	omask = _sl_LogMask;
	if (pmask != 0) _sl_LogMask = pmask;
	return (omask);
}

#endif /* !BUILDING_VARIANT */
